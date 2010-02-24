//  Copyright (c) 2007-2010 Hartmut Kaiser
// 
//  Distributed under the Boost Software License, Version 1.0. (See accompanying 
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <hpx/hpx.hpp>
#include <hpx/lcos/future_wait.hpp>

#include <boost/foreach.hpp>

#include <math.h>

#include "../amr/amr_mesh.hpp"
#include "../amr/amr_mesh_tapered.hpp"
#include "../amr/amr_mesh_left.hpp"
#include "../amr/rk_left.hpp"

#include "stencil.hpp"
#include "logging.hpp"
#include "stencil_data.hpp"
#include "stencil_functions.hpp"

///////////////////////////////////////////////////////////////////////////////
namespace hpx { namespace components { namespace amr 
{
    ///////////////////////////////////////////////////////////////////////////
    stencil::stencil()
      : numsteps_(0)
    {
    }

    int stencil::floatcmp(double x1,double x2) {
      // compare to floating point numbers
      double epsilon = 1.e-8;
      if ( x1 + epsilon >= x2 && x1 - epsilon <= x2 ) {
        // the numbers are close enough for coordinate comparison
        return 1;
      } else {
        return 0;
      }
    }
        
    ///////////////////////////////////////////////////////////////////////////
    // Implement actual functionality of this stencil
    // Compute the result value for the current time step
    int stencil::eval(naming::id_type const& result, 
        std::vector<naming::id_type> const& gids, int row, int column,
        Parameter const& par)
    {
        //BOOST_ASSERT(gids.size() <= 5);

        // make sure all the gids are looking valid
        if (result == naming::invalid_id)
        {
            HPX_THROW_EXCEPTION(bad_parameter,
                "stencil::eval", "result gid is invalid");
            return -1;
        }

        // this should occur only after result has been delivered already
        BOOST_FOREACH(naming::id_type gid, gids)
        {
            if (gid == naming::invalid_id)
                return -1;
        }

        // start asynchronous get operations

        // get all input memory_block_data instances
        access_memory_block<stencil_data> resultval;
        std::vector<access_memory_block<stencil_data> > val;

        int i;
        std::vector< stencil_data * > vecval;
        resultval = get_memory_block_async(val,gids,result);
        for (i=0;i<val.size();i++) vecval.push_back(val[i].get_ptr());

        if ( val.size()%2 == 0 ) {
          if ( val.size() == 2 ) {
            if ( column == 0 ) resultval->x_ = val[0]->x_;
            else resultval->x_ = val[1]->x_; 
          }
        } else {
          if ( gids.size() == 1 ) { 
            resultval.get() = val[0].get();
            return -1;
          }

          // update x position
          resultval->x_ = val[(val.size()-1)/2]->x_;
        }

        // initialize result 
        resultval->overwrite_alloc_ = 0;
        resultval->right_alloc_ = 0;
        resultval->left_alloc_ = 0;

        // the first two input values should have the same timestep
        BOOST_ASSERT(val[0]->timestep_== val[1]->timestep_);

        if (val[0]->level_ == 0 && val[0]->timestep_ < numsteps_ || val[0]->level_ > 0) {

            // call rk update 
            int gft = rkupdate(&*vecval.begin(),resultval.get_ptr(),vecval.size(),
                     numsteps_,par,gids.size(),column);
            BOOST_ASSERT(gft);
            // refine only after rk subcycles are finished (we don't refine in the midst of rk subcycles)
            if ( resultval->iter_ == 0 ) resultval->refine_ = refinement(&resultval->value_,resultval->level_);
            else resultval->refine_ = false;

            std::size_t allowedl = par.allowedl;

            // eliminate unrefinable cases
            if ( gids.size() != 5 && par.stencilsize == 3 && par.integrator == 0 ) resultval->refine_ = false;
            if ( gids.size() != 9 && par.stencilsize == 3 && par.integrator == 1 ) resultval->refine_ = false;

            if ( resultval->refine_ && resultval->level_ < allowedl 
                 && val[0]->timestep_ >= 1.e-6  ) {
              finer_mesh_tapered(result, gids, row, column, par);
            } else {
              resultval->overwrite_alloc_ = 0;
            } 

            // One special case: refining at time = 0
            if ( resultval->refine_ && 
                 val[0]->timestep_ < 1.e-6 && resultval->level_ < allowedl ) {
              finer_mesh_initial(result, gids, resultval->level_+1, resultval->x_, row, column, par);
            }

            if (log_ && fmod(resultval->timestep_,par.output) < 1.e-6) 
                stubs::logging::logentry(log_, resultval.get(), row,0, par);
        }
        else {
            // the last time step has been reached, just copy over the data
            if ((gids.size())%2 == 1) {
              resultval.get() = val[(gids.size()-1)/2].get();
            } else if (gids.size() == 2) {
              // bdry computation
              if ( column == 0 ) {
                resultval.get() = val[0].get();
              } else {
                resultval.get() = val[1].get();
              }
            } else {
              BOOST_ASSERT(false);
            }
        }
 
        // set return value difference between actual and required number of
        // timesteps (>0: still to go, 0: last step, <0: overdone)
        if ( val[0]->level_ > 0 ) {
          if ( row == 1 || row == 2 ) return 0;
          else {
            return 1;
          }
        } else {
          //int t = (int) (resultval->timestep_ + 0.5);
          int t = resultval->cycle_;
          int r = numsteps_ - t;
          return r;
        }
    }

    ///////////////////////////////////////////////////////////////////////////
    // Implement a finer mesh via interpolation of inter-mesh points
    // Compute the result value for the current time step
    int stencil::finer_mesh_tapered(naming::id_type const& result, 
        std::vector<naming::id_type> const& gids, int row,int column, Parameter const& par) 
    {

      int i;
      naming::id_type gval[9];
      access_memory_block<stencil_data> mval[9];

      bool left = left_tapered_mesh(gids,row,column,par);

      if ( left ) {
        // -------------------- Left (unbiased) Tapered Mesh --------------------------
        std::vector<naming::id_type> initial_data;
        left_tapered_prep_initial_data(initial_data,gids,row,column,par);

        // mesh object setup
        naming::id_type here = applier::get_applier().get_runtime_support_gid();
        components::component_type logging_type =
                  components::get_component_type<components::amr::server::logging>();
        components::component_type function_type =
                  components::get_component_type<components::amr::stencil>();
        // create the mesh only if you need to, otherwise reuse (reduce overhead)
        if ( par.integrator == 0 ) {
          if ( !child_left_mesh[row].get_gid() ) {
              child_left_mesh[row].create(here, 1, true);
          }
        } else if ( par.integrator == 1 ) {
          if ( !rk_left_mesh[row].get_gid() ) {
              rk_left_mesh[row].create(here, 1, true);
          }
        } else {
          BOOST_ASSERT(false);
        }

        bool do_logging = false;
        if ( par.loglevel > 0 ) {
          do_logging = true;
        }

        std::vector<naming::id_type> result_data;
        if ( par.integrator == 0 ) {
          result_data = child_left_mesh[row].execute(initial_data, function_type,
                do_logging ? logging_type : components::component_invalid,par);
        } else if ( par.integrator == 1 ) {
          result_data =  rk_left_mesh[row].execute(initial_data, function_type,
                do_logging ? logging_type : components::component_invalid,par);
        } else {
          BOOST_ASSERT(false);
        }
  
        // -------------------------------------------------------------------
        // You get 3 values out: left, center, and right -- that's it.  overwrite the coarse grid point and
        // tell the neighbors to remember the left and right values.
        access_memory_block<stencil_data> overwrite, resultval;
        int mid; 
        if ( (result_data.size())%2 == 1 ) {
          mid = (result_data.size()-1)/2;
        } else {
          BOOST_ASSERT(false);
        }
        boost::tie(overwrite, resultval) = 
            get_memory_block_async<stencil_data>(result_data[mid], result);

        // overwrite the coarse point computation
        resultval->value_ = overwrite->value_;

        resultval->overwrite_alloc_ = 1;
        resultval->overwrite_ = result_data[mid];

        // remember neighbor value
        overwrite->right_alloc_ = 1;
        overwrite->right_ = result_data[result_data.size()-1];

        overwrite->left_alloc_ = 1;
        overwrite->left_ = result_data[0];

        // DEBUG -- log the right/left points computed
        access_memory_block<stencil_data> amb1 = 
                       hpx::components::stubs::memory_block::get(result_data[0]);
        stubs::logging::logentry(log_, amb1.get(), row,1, par);

        access_memory_block<stencil_data> amb2 = 
                       hpx::components::stubs::memory_block::get(result_data[result_data.size()-1]);
        stubs::logging::logentry(log_, amb2.get(), row,1, par);

        // release result data
        for (std::size_t i = 1; i < result_data.size()-1; ++i) { 
          if ( i != mid ) components::stubs::memory_block::free(result_data[i]);
        }
      } else {
        // -------------------- Right (biased) Tapered Mesh --------------------------
        std::vector<naming::id_type> initial_data;
        right_tapered_prep_initial_data(initial_data,gids,row,column,par);

        // mesh object setup
        naming::id_type here = applier::get_applier().get_runtime_support_gid();
        components::component_type logging_type =
                  components::get_component_type<components::amr::server::logging>();
        components::component_type function_type =
                  components::get_component_type<components::amr::stencil>();
        // create the mesh only if you need to, otherwise reuse (reduce overhead)
        if ( !child_mesh[row].get_gid() ) {
            child_mesh[row].create(here,1, true);
        }

        bool do_logging = false;
        if ( par.loglevel > 0 ) {
          do_logging = true;
        }
        std::vector<naming::id_type> result_data(
            child_mesh[row].execute(initial_data, function_type,
              do_logging ? logging_type : components::component_invalid,par));
  
        // -------------------------------------------------------------------
        // You get 2 values out: center, and right -- that's it.  overwrite the coarse grid point and
        // tell the neighbor to remember the right value.
        access_memory_block<stencil_data> overwrite,resultval;
        boost::tie(overwrite, resultval) = 
            get_memory_block_async<stencil_data>(result_data[0], result);

        // overwrite the coarse point computation
        resultval->value_ = overwrite->value_;

        // remember the overwrite point and the neighbor
        resultval->overwrite_alloc_ = 1;
        resultval->overwrite_ = result_data[0];

        overwrite->right_alloc_ = 1;
        overwrite->right_ = result_data[result_data.size()-1];

        overwrite->left_alloc_ = 0;

        // DEBUG -- log the right points computed if no interp was involved
        access_memory_block<stencil_data> amb = 
                         hpx::components::stubs::memory_block::get(result_data[result_data.size()-1]);
        stubs::logging::logentry(log_, amb.get(), row,1, par);

        // release result data
        for (std::size_t i = 1; i < result_data.size()-1; ++i) 
            components::stubs::memory_block::free(result_data[i]);
      }

      return 0;
    }

    ///////////////////////////////////////////////////////////////////////////
    // Decide whether to use a left or right biased tapered mesh
    bool stencil::left_tapered_mesh(std::vector<naming::id_type> const& gids, int row,int column, Parameter const& par) 
    {
      if ( par.integrator == 0 ) {
        access_memory_block<stencil_data> edge1,edge2;
        boost::tie(edge1,edge2) = get_memory_block_async<stencil_data>(gids[0],gids[1]);
        if ( !edge1->refine_ || !edge2->refine_ || (row == 1 && column == 1) ) return true;
        else return false;
      } else if (par.integrator == 1) {
        // not implemented yet
        //BOOST_ASSERT(false);
        return true;
      }
    }

    ///////////////////////////////////////////////////////////////////////////
    // Prep initial data for left (unbiased) tapered mesh
    int stencil::left_tapered_prep_initial_data(std::vector<naming::id_type> & initial_data, 
        std::vector<naming::id_type> const& gids, int row,int column, Parameter const& par) 
    {
      int i;
      if ( par.integrator == 0 ) {
        // Euler {{{
        BOOST_ASSERT(gids.size() == 5);
        naming::id_type gval[9];
        access_memory_block<stencil_data> mval[9];

        boost::tie(gval[0], gval[2], gval[4], gval[6], gval[8]) = 
                        components::wait(components::stubs::memory_block::clone_async(gids[0]), 
                             components::stubs::memory_block::clone_async(gids[1]),
                             components::stubs::memory_block::clone_async(gids[2]),
                             components::stubs::memory_block::clone_async(gids[3]),
                             components::stubs::memory_block::clone_async(gids[4]));
        boost::tie(gval[1], gval[3], gval[5],gval[7]) = 
                    components::wait(components::stubs::memory_block::clone_async(gids[2]), 
                    components::stubs::memory_block::clone_async(gids[2]),
                    components::stubs::memory_block::clone_async(gids[2]),
                    components::stubs::memory_block::clone_async(gids[2]));
        boost::tie(mval[0], mval[2], mval[4], mval[6], mval[8]) = 
          get_memory_block_async<stencil_data>(gval[0], gval[2], gval[4], gval[6], gval[8]);

        // the edge of the AMR mesh has been reached.  
        // Use the left mesh class instead of standard tapered
        boost::tie(mval[1], mval[3], mval[5],mval[7]) = 
            get_memory_block_async<stencil_data>(gval[1], gval[3], gval[5],gval[7]);

        // increase the level by one
        for (i=0;i<9;i++) {
          ++mval[i]->level_;
          mval[i]->index_ = i;
        }

        // this updates the coordinate position
        for (i=1;i<9;i=i+2) {
          mval[i]->x_ = 0.5*(mval[i-1]->x_+mval[i+1]->x_);
        }

        // unset alloc on these gids
        for (i=1;i<9;i=i+2) {
          mval[i]->left_alloc_ = 0;
          mval[i]->right_alloc_ = 0;
          mval[i]->overwrite_alloc_ = 0;
        }

        // avoid interpolation if possible
        int s;
        s = 0;
        for (i=1;i<9;i=i+2) {
          s = findpoint(mval[i-1],mval[i+1],mval[i]);
          if ( s == 0 ) { 
            interpolation(&(mval[i]->value_),&(mval[i-1]->value_),&(mval[i+1]->value_));
            // DEBUG
            stubs::logging::logentry(log_, mval[i].get(), row,2, par);
          }
          mval[i]->refine_ = refinement(&(mval[i]->value_),mval[i]->level_);

          // eliminate unrefinable cases
          if ( gids.size() != 5 && par.stencilsize == 3 && par.integrator == 0 ) mval[i]->refine_ = false;
          if ( gids.size() != 9 && par.stencilsize == 3 && par.integrator == 1 ) mval[i]->refine_ = false;
        }

        for (i=0;i<9;i++) {
          initial_data.push_back(gval[i]);
        }
        // }}}
      } else if (par.integrator == 1) {
        // rk3 {{{
        BOOST_ASSERT(gids.size() == 9);
        naming::id_type gval[17];
        access_memory_block<stencil_data> mval[17];

        boost::tie(gval[0], gval[2], gval[4], gval[6], gval[8]) = 
                        components::wait(components::stubs::memory_block::clone_async(gids[0]), 
                             components::stubs::memory_block::clone_async(gids[1]),
                             components::stubs::memory_block::clone_async(gids[2]),
                             components::stubs::memory_block::clone_async(gids[3]),
                             components::stubs::memory_block::clone_async(gids[4]));
        boost::tie(gval[10], gval[12], gval[14], gval[16]) = 
                        components::wait(components::stubs::memory_block::clone_async(gids[5]), 
                             components::stubs::memory_block::clone_async(gids[6]),
                             components::stubs::memory_block::clone_async(gids[7]),
                             components::stubs::memory_block::clone_async(gids[8]));
        boost::tie(gval[1], gval[3], gval[5],gval[7]) = 
                    components::wait(components::stubs::memory_block::clone_async(gids[4]), 
                    components::stubs::memory_block::clone_async(gids[4]),
                    components::stubs::memory_block::clone_async(gids[4]),
                    components::stubs::memory_block::clone_async(gids[4]));
        boost::tie(gval[9], gval[11], gval[13],gval[15]) = 
                    components::wait(components::stubs::memory_block::clone_async(gids[4]), 
                    components::stubs::memory_block::clone_async(gids[4]),
                    components::stubs::memory_block::clone_async(gids[4]),
                    components::stubs::memory_block::clone_async(gids[4]));
        boost::tie(mval[0], mval[2], mval[4], mval[6], mval[8]) = 
          get_memory_block_async<stencil_data>(gval[0], gval[2], gval[4], gval[6], gval[8]);
        boost::tie(mval[10], mval[12], mval[14], mval[16]) = 
          get_memory_block_async<stencil_data>(gval[10], gval[12], gval[14], gval[16]);

        // the edge of the AMR mesh has been reached.  
        // Use the left mesh class instead of standard tapered
        boost::tie(mval[1], mval[3], mval[5],mval[7]) = 
            get_memory_block_async<stencil_data>(gval[1], gval[3], gval[5],gval[7]);
        boost::tie(mval[9], mval[11], mval[13],mval[15]) = 
            get_memory_block_async<stencil_data>(gval[9], gval[11], gval[13],gval[15]);

        // increase the level by one
        for (i=0;i<17;i++) {
          ++mval[i]->level_;
          mval[i]->index_ = i;
        }

        // this updates the coordinate position
        for (i=1;i<17;i=i+2) {
          mval[i]->x_ = 0.5*(mval[i-1]->x_+mval[i+1]->x_);
        }

        // unset alloc on these gids
        for (i=1;i<17;i=i+2) {
          mval[i]->left_alloc_ = 0;
          mval[i]->right_alloc_ = 0;
          mval[i]->overwrite_alloc_ = 0;
        }

        // avoid interpolation if possible
        int s;
        s = 0;
        for (i=1;i<17;i=i+2) {
          s = findpoint(mval[i-1],mval[i+1],mval[i]);
          if ( s == 0 ) { 
            interpolation(&(mval[i]->value_),&(mval[i-1]->value_),&(mval[i+1]->value_));
            // DEBUG
            stubs::logging::logentry(log_, mval[i].get(), row,2, par);
          }
          mval[i]->refine_ = refinement(&(mval[i]->value_),mval[i]->level_);
        }

        for (i=0;i<17;i++) {
          initial_data.push_back(gval[i]);
        }
        // }}}
      }

      return 0;
    }
    
    ///////////////////////////////////////////////////////////////////////////
    // Prep initial data for right biased tapered mesh
    int stencil::right_tapered_prep_initial_data(std::vector<naming::id_type> & initial_data, 
        std::vector<naming::id_type> const& gids, int row,int column, Parameter const& par) 
    {
      int i;
      if ( par.integrator == 0 ) {
        // Euler {{{
        naming::id_type gval[9];
        access_memory_block<stencil_data> mval[9];

        boost::tie(gval[8], gval[1], gval[3], gval[5], gval[7]) = 
                        components::wait(components::stubs::memory_block::clone_async(gids[0]), 
                             components::stubs::memory_block::clone_async(gids[1]),
                             components::stubs::memory_block::clone_async(gids[2]),
                             components::stubs::memory_block::clone_async(gids[3]),
                             components::stubs::memory_block::clone_async(gids[4]));
        boost::tie(gval[0], gval[2], gval[4],gval[6]) = 
                    components::wait(components::stubs::memory_block::clone_async(gids[2]), 
                    components::stubs::memory_block::clone_async(gids[2]),
                    components::stubs::memory_block::clone_async(gids[2]),
                    components::stubs::memory_block::clone_async(gids[2]));
        boost::tie(mval[8], mval[1], mval[3], mval[5], mval[7]) = 
          get_memory_block_async<stencil_data>(gval[8], gval[1], gval[3], gval[5], gval[7]);

        boost::tie(mval[0], mval[2], mval[4],mval[6]) = 
            get_memory_block_async<stencil_data>(gval[0], gval[2], gval[4],gval[6]);

        // temporarily store the anchor values before overwriting them
        nodedata tm1,t1,t3,t5,t7;
        tm1 = mval[8]->value_;
        t1 = mval[1]->value_;
        t3 = mval[3]->value_;
        t5 = mval[5]->value_;
        t7 = mval[7]->value_;

        // increase the level by one
        for (i=0;i<9;i++) {
          ++mval[i]->level_;
          mval[i]->index_ = i;
        }

        // this updates the coordinate position
        mval[0]->x_ = 0.5*(mval[8]->x_+mval[1]->x_);
        mval[2]->x_ = 0.5*(mval[1]->x_+mval[3]->x_);
        mval[4]->x_ = 0.5*(mval[3]->x_+mval[5]->x_);
        mval[6]->x_ = 0.5*(mval[5]->x_+mval[7]->x_);

        // reset alloc on these gids
        for (i=0;i<8;i=i+2) {
          mval[i]->left_alloc_ = 0;
          mval[i]->right_alloc_ = 0;
          mval[i]->overwrite_alloc_ = 0;
        }

        // avoid interpolation if possible
        int s0,s2,s4,s6;
        s0 = 0; s2 = 0; s4 = 0; s6 = 0;

        s0 = findpoint(mval[8],mval[1],mval[0]);
        s2 = findpoint(mval[1],mval[3],mval[2]);
        s4 = findpoint(mval[3],mval[5],mval[4]);
        s6 = findpoint(mval[5],mval[7],mval[6]);

        if (s0 == 0) interpolation(&(mval[0]->value_),&tm1,&t1);
        if (s2 == 0) interpolation(&(mval[2]->value_),&t1,&t3);
        if (s4 == 0) interpolation(&(mval[4]->value_),&t3,&t5);
        if (s6 == 0) interpolation(&(mval[6]->value_),&t5,&t7);

        // DEBUG
        if ( s0 == 0 ) stubs::logging::logentry(log_, mval[0].get(), row,2, par);
        if ( s2 == 0 ) stubs::logging::logentry(log_, mval[2].get(), row,2, par);
        if ( s4 == 0 ) stubs::logging::logentry(log_, mval[4].get(), row,2, par);
        if ( s6 == 0 ) stubs::logging::logentry(log_, mval[6].get(), row,2, par);

        // apply refinement criteria test to interpolated/found values
        mval[0]->refine_ = refinement(&(mval[0]->value_),mval[0]->level_);
        mval[2]->refine_ = refinement(&(mval[2]->value_),mval[2]->level_);
        mval[4]->refine_ = refinement(&(mval[4]->value_),mval[4]->level_);
        mval[6]->refine_ = refinement(&(mval[6]->value_),mval[6]->level_);

        for (i=0;i<8;i++) {
          initial_data.push_back(gval[i]);
        }

        // this cloned gid is not needed anymore
        components::stubs::memory_block::free(gval[8]);
        // }}}
      } else if (par.integrator == 1) {
        // not implemented yet
        BOOST_ASSERT(false);
      }

      return 0;
    }

    ///////////////////////////////////////////////////////////////////////////
    // Implement a finer mesh via interpolation of inter-mesh points
    // Compute the result value for the current time step
    int stencil::finer_mesh_initial(naming::id_type const& result, 
        std::vector<naming::id_type> const& gids, std::size_t level, double x, 
        int row, int column, Parameter const& par) 
    {

      // the initial data for the child mesh comes from the parent mesh
      naming::id_type here = applier::get_applier().get_runtime_support_gid();
      components::component_type logging_type =
                components::get_component_type<components::amr::server::logging>();
      components::component_type function_type =
                components::get_component_type<components::amr::stencil>();

      if ( par.integrator == 0 ) {
        if ( !child_left_mesh[row].get_gid() ) {
            child_left_mesh[row].create(here, 1, true);
        }
      } else if ( par.integrator == 1 ) {
        if ( !rk_left_mesh[row].get_gid() ) {
            rk_left_mesh[row].create(here, 1, true);
        }
      } else {
        BOOST_ASSERT(false);
      }

      bool do_logging = false;
      if ( par.loglevel > 0 ) {
        do_logging = true;
      }

      std::vector<naming::id_type> result_data;
      if ( par.integrator == 0 ) {
        result_data = child_left_mesh[row].init_execute(function_type,
              do_logging ? logging_type : components::component_invalid,
              level, x, par);
      } else if ( par.integrator == 1 ) {
        result_data = rk_left_mesh[row].init_execute(function_type,
              do_logging ? logging_type : components::component_invalid,
              level, x, par);
      } else {
        BOOST_ASSERT(false);
      }


      //  using mesh_left
      access_memory_block<stencil_data> overwrite, resultval;
      int mid; 
      if ( (result_data.size())%2 == 1 ) {
        mid = (result_data.size()-1)/2;
      } else {
        BOOST_ASSERT(false);
      }

      boost::tie(overwrite, resultval) = 
          get_memory_block_async<stencil_data>(result_data[mid], result);

 
      // overwrite the coarse point computation
      resultval->value_ = overwrite->value_;
 
      resultval->overwrite_alloc_ = 1;
      resultval->overwrite_ = result_data[mid];
   
      // remember neighbor value
      overwrite->right_alloc_ = 1;
      overwrite->right_ = result_data[0];

      overwrite->left_alloc_ = 1;
      overwrite->left_ = result_data[result_data.size()-1];

      resultval->right_alloc_ = 0;
      resultval->left_alloc_ = 0;

      // DEBUG -- log the right/left points computed
      access_memory_block<stencil_data> amb1 = 
                         hpx::components::stubs::memory_block::get(result_data[0]);
      access_memory_block<stencil_data> amb2 = 
                         hpx::components::stubs::memory_block::get(result_data[result_data.size()-1]);
      stubs::logging::logentry(log_, amb1.get(), row,1, par);
      stubs::logging::logentry(log_, amb2.get(), row,1, par);

      for (std::size_t i = 1; i < result_data.size()-1; ++i) {
        // free all but the overwrite and end value
        if ( i != mid )  components::stubs::memory_block::free(result_data[i]);
      }

      // release result data
      //for (std::size_t i = 0; i < result_data.size(); ++i) 
      //    components::stubs::memory_block::free(result_data[i]);

      return 0;
    }

    ///////////////////////////////////////////////////////////////////////////
    naming::id_type stencil::alloc_data(int item, int maxitems, int row,
        std::size_t level, double x, Parameter const& par)
    {
        naming::id_type result = components::stubs::memory_block::create(
            applier::get_applier().get_runtime_support_gid(), sizeof(stencil_data));

        if (-1 != item) {
            // provide initial data for the given data value 
            access_memory_block<stencil_data> val(
                components::stubs::memory_block::checkout(result));

            // call provided (external) function
            generate_initial_data(val.get_ptr(), item, maxitems, row, level, x, par);

            if (par.loglevel > 1)         // send initial value to logging instance
                stubs::logging::logentry(log_, val.get(), row,0, par);
        }
        return result;
    }

    int stencil::findpoint(access_memory_block<stencil_data> const& anchor_to_the_left,
                           access_memory_block<stencil_data> const& anchor_to_the_right, 
                           access_memory_block<stencil_data> & resultval) 
    {
      // the pinball machine
      int s = 0;
      access_memory_block<stencil_data> amb0;
      amb0 = anchor_to_the_left;
      if (s == 0 && amb0->overwrite_alloc_ == 1) {
        access_memory_block<stencil_data> amb1 = hpx::components::stubs::memory_block::get(amb0->overwrite_);

        // look around
        if ( amb1->right_alloc_ == 1 ) {
          access_memory_block<stencil_data> amb2 = hpx::components::stubs::memory_block::get(amb1->right_);
          if ( floatcmp(amb2->x_,resultval->x_) ) {
            resultval->value_ = amb2->value_;
            // transfer overwrite information as well
            if ( amb2->overwrite_alloc_ == 1 ) {
              resultval->overwrite_alloc_ = 1;
              resultval->overwrite_ = amb2->overwrite_;
            }

            s = 1;
            return s;
          } else {
            if ( amb2->x_ > resultval->x_ ) {
              s = findpoint(amb1,amb2,resultval);
            } else {
              s = findpoint(amb2,amb1,resultval);
            }
          }
        }

        if ( s == 0 && amb1->left_alloc_ == 1 ) {
          access_memory_block<stencil_data> amb2 = hpx::components::stubs::memory_block::get(amb1->left_);
          if ( floatcmp(amb2->x_,resultval->x_) ) {
            resultval->value_ = amb2->value_;
            // transfer overwrite information as well
            if ( amb2->overwrite_alloc_ == 1 ) {
              resultval->overwrite_alloc_ = 1;
              resultval->overwrite_ = amb2->overwrite_;
            }
            s = 1;
            return s;
          } else {
            if ( amb2->x_ > resultval->x_ ) {
              s = findpoint(amb1,amb2,resultval);
            } else {
              s = findpoint(amb2,amb1,resultval);
            }
          }
        }

      }


      amb0 = anchor_to_the_right;
      if (s == 0 && amb0->overwrite_alloc_ == 1) {
        access_memory_block<stencil_data> amb1 = hpx::components::stubs::memory_block::get(amb0->overwrite_);

        // look around
        if ( amb1->right_alloc_ == 1 ) {
          access_memory_block<stencil_data> amb2 = hpx::components::stubs::memory_block::get(amb1->right_);
          if ( floatcmp(amb2->x_,resultval->x_) ) {
            resultval->value_ = amb2->value_;
            // transfer overwrite information as well
            if ( amb2->overwrite_alloc_ == 1 ) {
              resultval->overwrite_alloc_ = 1;
              resultval->overwrite_ = amb2->overwrite_;
            }
            s = 1;
            return s;
          } else {
            if ( amb2->x_ > resultval->x_ ) {
              s = findpoint(amb1,amb2,resultval);
            } else {
              s = findpoint(amb2,amb1,resultval);
            }
          }
        }

        if (s == 0 && amb1->left_alloc_ == 1 ) {
          access_memory_block<stencil_data> amb2 = hpx::components::stubs::memory_block::get(amb1->left_);
          if ( floatcmp(amb2->x_,resultval->x_) ) {
            resultval->value_ = amb2->value_;
            // transfer overwrite information as well
            if ( amb2->overwrite_alloc_ == 1 ) {
              resultval->overwrite_alloc_ = 1;
              resultval->overwrite_ = amb2->overwrite_;
            }
            s = 1;
            return s;
          } else {
            if ( amb2->x_ > resultval->x_ ) {
              s = findpoint(amb1,amb2,resultval);
            } else {
              s = findpoint(amb2,amb1,resultval);
            }
          }
        }
      }

      return s;
    }

    void stencil::init(std::size_t numsteps, naming::id_type const& logging)
    {
        numsteps_ = numsteps;
        log_ = logging;
    }

    // This routine is for debugging
    int stencil::testpoint(access_memory_block<stencil_data> const& val,
                            naming::id_type const& gid)
    {
       if ( floatcmp(val->x_,3.3333333333333333) == 1 ) {
           printf(" TEST overwrite %d timestep: %g index %d id %d level %d x %g right_alloc %d left_alloc %d refine %d\n",
               val->overwrite_alloc_,val->timestep_,
               val->index_,gid.id_lsb_,val->level_,
               val->x_,val->right_alloc_,val->left_alloc_,val->refine_);
           //if ( gid.id_lsb_ == 549233 ) {
           //  return 1;
           //}
       }
       return 0;
    }

    // This routine is for debugging
    void stencil::checkpoint(std::vector<naming::id_type> const& gids)
    {
      int i;
      for (i=0;i<gids.size();i++) {
        access_memory_block<stencil_data> amb = hpx::components::stubs::memory_block::get(gids[i]);
        printf(" gid: %d location: %g overwrite: %d\n",gids[i].id_lsb_,amb->x_,amb->overwrite_alloc_);
        if ( amb->overwrite_alloc_ == 1 ) {
          access_memory_block<stencil_data> amb2 = hpx::components::stubs::memory_block::get(amb->overwrite_);
          printf(" overwrite      location: %g overwrite: %d : %d %d : level %d\n",amb2->x_,amb2->overwrite_alloc_,amb2->left_alloc_,amb2->right_alloc_,amb2->level_);
          if ( amb2->overwrite_alloc_ == 1 ) {
            access_memory_block<stencil_data> amb3 = hpx::components::stubs::memory_block::get(amb2->overwrite_);
          printf("  overwrite overwrite    location: %g overwrite: %d : %d %d : level %d\n",amb3->x_,amb3->overwrite_alloc_,amb3->left_alloc_,amb3->right_alloc_,amb3->level_);
          }
          if ( amb2->right_alloc_ == 1 ) {
            access_memory_block<stencil_data> amb3 = hpx::components::stubs::memory_block::get(amb2->right_);
          printf(" overwrite right       location: %g overwrite: %d : %d %d : level %d\n",amb3->x_,amb3->overwrite_alloc_,amb3->left_alloc_,amb3->right_alloc_,amb3->level_);
          }
          if ( amb2->left_alloc_ == 1 ) {
            access_memory_block<stencil_data> amb3 = hpx::components::stubs::memory_block::get(amb2->left_);
          printf(" overwrite left       location: %g overwrite: %d : %d %d : level %d\n",amb3->x_,amb3->overwrite_alloc_,amb3->left_alloc_,amb3->right_alloc_,amb3->level_);
          }
        }
      }

    }

    // This routine is for debugging -- pass in any gid
    // and it traverses the entire grid available at that moment
    void stencil::traverse_grid(naming::id_type const& start,int firstcall)
    {
      int i;
      int found;
#if 0
      if (firstcall == 1) lsb_count = 0;

      access_memory_block<stencil_data> amb = hpx::components::stubs::memory_block::get(start);
      printf("stencil::traverse_grid x: %g lsb: %d\n",amb->x_,start.id_lsb_);
      if ( amb->overwrite_alloc_ == 1 ) {
        // check if the lsb has already been recorded
        found = 0;
        for (i=0;i<lsb_count;i++) {
          if (amb->overwrite_.id_lsb_ == unique_lsb[i]) {
            found = 1;
            break;
          }
        }
        if ( found == 0 ) {
          unique_lsb[lsb_count] = amb->overwrite_.id_lsb_;
          lsb_count++;
          printf("stencil::traverse_grid overwrite\n");
          traverse_grid(amb->overwrite_,0);
        }
      }

      if ( amb->right_alloc_ == 1 ) {
        // check if the lsb has already been recorded
        found = 0;
        for (i=0;i<lsb_count;i++) {
          if (amb->right_.id_lsb_ == unique_lsb[i]) {
            found = 1;
            break;
          }
        }
        if ( found == 0 ) {
          unique_lsb[lsb_count] = amb->right_.id_lsb_;
          lsb_count++;
          printf("stencil::traverse_grid right\n");
          traverse_grid(amb->right_,0);
        }
      }

      if ( amb->left_alloc_ == 1 ) {
        // check if the lsb has already been recorded
        found = 0;
        for (i=0;i<lsb_count;i++) {
          if (amb->left_.id_lsb_ == unique_lsb[i]) {
            found = 1;
            break;
          }
        }
        if ( found == 0 ) {
          unique_lsb[lsb_count] = amb->left_.id_lsb_;
          lsb_count++;
          printf("stencil::traverse_grid left\n");
          traverse_grid(amb->left_,0);
        }
      }
#endif
    }


}}}

