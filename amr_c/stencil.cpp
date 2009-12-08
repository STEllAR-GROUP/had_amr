//  Copyright (c) 2007-2009 Hartmut Kaiser
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
    {}

    ///////////////////////////////////////////////////////////////////////////
    // Implement actual functionality of this stencil
    // Compute the result value for the current time step
    int stencil::eval(naming::id_type const& result, 
        std::vector<naming::id_type> const& gids, int row, int column,
        Parameter const& par)
    {
        BOOST_ASSERT(gids.size() <= 5);

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
        access_memory_block<stencil_data> val1, val2, val3, val4, val5, resultval;
        if (gids.size() == 3) { 
            boost::tie(val1, val2, val3, resultval) = 
                get_memory_block_async<stencil_data>(gids[0], gids[1], gids[2], result);
        } 
        else if (gids.size() == 2) {
            boost::tie(val1, val2, resultval) = 
                get_memory_block_async<stencil_data>(gids[0], gids[1], result);
        } 
        else if (gids.size() == 5) {
            boost::tie(val1, val2, val3, val4, val5, resultval) = 
                get_memory_block_async<stencil_data>(gids[0], gids[1], gids[2], gids[3], gids[4], result);
        } 
        else {
          boost::tie(val1, resultval) = 
                get_memory_block_async<stencil_data>(gids[0], result);
          resultval.get() = val1.get();
          return -1;
        }

        // the predecessor
        double middle_timestep;
        if (gids.size() == 3) 
          middle_timestep = val2->timestep_;
        else if (gids.size() == 2 && column == 0) 
          middle_timestep = val1->timestep_;      // left boundary point
        else if (gids.size() == 2 && column != 0) {
          middle_timestep = val2->timestep_;      // right boundary point
        } else if ( gids.size() == 3 ) {
          middle_timestep = val2->timestep_;
        } else if ( gids.size() == 5 ) {
          middle_timestep = val3->timestep_;
        }

        if (val1->level_ == 0 && middle_timestep < numsteps_ || val1->level_ > 0) {

            if (gids.size() == 3) {
              // this is the actual calculation, call provided (external) function
              evaluate_timestep(val1.get_ptr(), val2.get_ptr(), val3.get_ptr(), 
                  resultval.get_ptr(), numsteps_,par,gids.size());

              // copy over the coordinate value to the result
              resultval->x_ = val2->x_;
            } else if (gids.size() == 2) {
              // bdry computation
              if ( column == 0 ) {
                evaluate_left_bdry_timestep(val1.get_ptr(), val2.get_ptr(),
                  resultval.get_ptr(), numsteps_,par);

                // copy over the coordinate value to the result
                resultval->x_ = val1->x_;
              } else {
                evaluate_right_bdry_timestep(val1.get_ptr(), val2.get_ptr(),
                  resultval.get_ptr(), numsteps_,par);

                // copy over the coordinate value to the result
                resultval->x_ = val2->x_;
              }
            } else if (gids.size() == 5) {
              // this is the actual calculation, call provided (external) function
              evaluate_timestep(val2.get_ptr(), val3.get_ptr(), val4.get_ptr(), 
                  resultval.get_ptr(), numsteps_,par,gids.size());

              // copy over the coordinate value to the result
              resultval->x_ = val3->x_;
            }

            std::size_t allowedl = par.allowedl;
            if ( val2->refine_ && gids.size() == 5 && val2->level_ < allowedl ) {
              finer_mesh(result, gids,par);
            }

            // One special case: refining at time = 0
            if ( resultval->refine_ && gids.size() == 5 && 
                 val1->timestep_ < 1.e-6 && resultval->level_ < allowedl ) {
              finer_mesh_initial(result, gids, resultval->level_+1, resultval->x_, par);
            }

            if (log_ && fmod(resultval->timestep_,par.output) < 1.e-6)  
                stubs::logging::logentry(log_, resultval.get(), row, par);
        }
        else {
            // the last time step has been reached, just copy over the data
            if (gids.size() == 3) {
              resultval.get() = val2.get();
            } else if (gids.size() == 2) {
              // bdry computation
              if ( column == 0 ) {
                resultval.get() = val1.get();
              } else {
                resultval.get() = val2.get();
              }
            } else if (gids.size() == 5) {
              resultval.get() = val3.get();
            } else {
              BOOST_ASSERT(false);
            }
        }
 
        // set return value difference between actual and required number of
        // timesteps (>0: still to go, 0: last step, <0: overdone)
        if ( val1->level_ > 0 ) {
          if ( row == 1 || row == 2 ) return 0;
          else {
            return 1;
          }
        } else {
          int t = (int) (resultval->timestep_ + 0.5);
          int r = numsteps_ - t;
          return r;
        }
    }

    ///////////////////////////////////////////////////////////////////////////
    // Implement a finer mesh via interpolation of inter-mesh points
    // Compute the result value for the current time step
    int stencil::finer_mesh(naming::id_type const& result, 
        std::vector<naming::id_type> const& gids, Parameter const& par) 
    {

      int i;
      naming::id_type gval[9];
      boost::tie(gval[0], gval[1], gval[2], gval[3], gval[4]) = 
                        components::wait(components::stubs::memory_block::clone_async(gids[0]), 
                             components::stubs::memory_block::clone_async(gids[1]),
                             components::stubs::memory_block::clone_async(gids[2]),
                             components::stubs::memory_block::clone_async(gids[3]),
                             components::stubs::memory_block::clone_async(gids[4]));

      access_memory_block<stencil_data> mval[9];
      boost::tie(mval[0], mval[1], mval[2], mval[3], mval[4]) = 
          get_memory_block_async<stencil_data>(gval[0], gval[1], gval[2], gval[3], gval[4]);

      // temporarily store the anchor values before overwriting them
      double t1,t2,t3,t4,t5;
      double x1,x2,x3,x4,x5;
      t1 = mval[0]->value_;
      t2 = mval[1]->value_;
      t3 = mval[2]->value_;
      t4 = mval[3]->value_;
      t5 = mval[4]->value_;

      x1 = mval[0]->x_;
      x2 = mval[1]->x_;
      x3 = mval[2]->x_;
      x4 = mval[3]->x_;
      x5 = mval[4]->x_;

      if ( !mval[1]->refine_ || !mval[0]->refine_) {
        // the edge of the AMR mesh has been reached.  
        // Use the left mesh class instead of standard tapered
        boost::tie(gval[5], gval[6], gval[7],gval[8]) = 
                      components::wait(components::stubs::memory_block::clone_async(gids[2]), 
                      components::stubs::memory_block::clone_async(gids[2]),
                      components::stubs::memory_block::clone_async(gids[2]),
                      components::stubs::memory_block::clone_async(gids[2]));
        boost::tie(mval[5], mval[6], mval[7],mval[8]) = 
            get_memory_block_async<stencil_data>(gval[5], gval[6], gval[7],gval[8]);

        // increase the level by one
        for (i=0;i<9;i++) {
          ++mval[i]->level_;
          mval[i]->index_ = i;
        }

        // this updates the coordinate position
        mval[0]->x_ = x1;
        mval[1]->x_ = 0.5*(x1+x2);
        mval[2]->x_ = x2;
        mval[3]->x_ = 0.5*(x2+x3);
        mval[4]->x_ = x3;
        mval[5]->x_ = 0.5*(x3+x4);
        mval[6]->x_ = x4;
        mval[7]->x_ = 0.5*(x4+x5);
        mval[8]->x_ = x5;
      
        // coarse node duplicates
        mval[0]->value_ = t1;
        mval[2]->value_ = t2;
        mval[4]->value_ = t3;
        mval[6]->value_ = t4;
        mval[8]->value_ = t5;

        if ( par.linearbounds == 1 ) {
          // linear interpolation
          if ( mval[0]->right_alloc_ == 1 && mval[0]->right_level_ == mval[1]->level_ ) {
            mval[1]->value_ = mval[0]->right_value_;
          } else if ( mval[1]->left_alloc_ == 1 && mval[1]->left_level_ == mval[1]->level_ ) {
            mval[1]->value_ = mval[1]->left_value_;
          } else {
            mval[1]->value_ = 0.5*(t1 + t2);
           // std::cout << "interpolating " << " x value : " << mval[1]->x_ << std::endl;
          }
          if ( mval[1]->right_alloc_ == 1 && mval[1]->right_level_ == mval[3]->level_ ) {
            mval[3]->value_ = mval[1]->right_value_;
          } else if ( mval[2]->left_alloc_ == 1 && mval[2]->left_level_ == mval[3]->level_ ) {
            mval[3]->value_ = mval[2]->left_value_;
          } else {
            mval[3]->value_ = 0.5*(t2 + t3);
           // std::cout << "interpolating " << " x value : " << mval[3]->x_ << std::endl;
          }
          if ( mval[2]->right_alloc_ == 1 && mval[2]->right_level_ == mval[5]->level_ ) {
            mval[5]->value_ = mval[2]->right_value_;
          } else if ( mval[3]->left_alloc_ == 1 && mval[3]->left_level_ == mval[5]->level_ ) {
            mval[5]->value_ = mval[3]->left_value_;
          } else {
            mval[5]->value_ = 0.5*(t3 + t4);
           // std::cout << "interpolating " << " x value : " << mval[5]->x_ << std::endl;
          }
          if ( mval[3]->right_alloc_ == 1 && mval[3]->right_level_ == mval[7]->level_ ) {
            mval[7]->value_ = mval[3]->right_value_;
          } else if ( mval[4]->left_alloc_ == 1 && mval[4]->left_level_ == mval[7]->level_ ) {
            mval[7]->value_ = mval[4]->left_value_;
          } else {
            mval[7]->value_ = 0.5*(t4 + t5);
           // std::cout << "interpolating " << " x value : " << mval[7]->x_ << std::endl;
          }
        } else {
          // other user defined options not implemented yet
          interpolation();
          BOOST_ASSERT(false);
        }

        // the initial data for the child mesh comes from the parent mesh
        naming::id_type here = applier::get_applier().get_runtime_support_gid();
        components::component_type logging_type =
                  components::get_component_type<components::amr::server::logging>();
        components::component_type function_type =
                  components::get_component_type<components::amr::stencil>();
        components::amr::amr_mesh_left child_mesh (
                  components::amr::amr_mesh_left::create(here, 1, true));

        std::vector<naming::id_type> initial_data;
        for (i=0;i<9;i++) {
          initial_data.push_back(gval[i]);
        }

        bool do_logging = false;
        if ( par.loglevel > 0 ) {
          do_logging = true;
        }
        std::vector<naming::id_type> result_data(
            child_mesh.execute(initial_data, function_type,
              do_logging ? logging_type : components::component_invalid,par));
  
        access_memory_block<stencil_data> r_val1, r_val2, r_val3, resultval;
        boost::tie(r_val1, r_val2, r_val3, resultval) = 
            get_memory_block_async<stencil_data>(result_data[2], result_data[4], result_data[6], result);

        // overwrite the coarse point computation
        resultval->value_ = r_val2->value_;
  
        // remember neighbor value
        resultval->right_alloc_ = 1;
        resultval->right_value_ = r_val3->value_;
        resultval->right_level_ = r_val3->level_;

        resultval->left_alloc_ = 1;
        resultval->left_value_ = r_val1->value_;
        resultval->left_level_ = r_val1->level_;

        // release result data
        for (std::size_t i = 0; i < result_data.size(); ++i) 
            components::stubs::memory_block::free(result_data[i]);

      } else {
        boost::tie(gval[5], gval[6], gval[7]) = 
                      components::wait(components::stubs::memory_block::clone_async(gids[2]), 
                      components::stubs::memory_block::clone_async(gids[2]),
                      components::stubs::memory_block::clone_async(gids[2]));
        boost::tie(mval[5], mval[6], mval[7]) = 
            get_memory_block_async<stencil_data>(gval[5], gval[6], gval[7]);

        // increase the level by one
        for (i=0;i<8;i++) {
          ++mval[i]->level_;
          mval[i]->index_ = i;
        }

        // this updates the coordinate position
        mval[0]->x_ = 0.5*(x1+x2);
        mval[1]->x_ = x2;
        mval[2]->x_ = 0.5*(x2+x3);
        mval[3]->x_ = x3;
        mval[4]->x_ = 0.5*(x3+x4);
        mval[5]->x_ = x4;
        mval[6]->x_ = 0.5*(x4+x5);
        mval[7]->x_ = x5;
      
        // coarse node duplicates
        mval[1]->value_ = t2;
        mval[3]->value_ = t3;
        mval[5]->value_ = t4;
        mval[7]->value_ = t5;

        if ( par.linearbounds == 1 ) {
          // linear interpolation
          if ( mval[0]->right_alloc_ == 1 && mval[0]->right_level_ == mval[0]->level_ ) {
          //  std::cout << "A: Using right value : " << mval[0]->right_level_ << " x value : " << mval[0]->x_ << " right value : " << mval[0]->right_value_ << std::endl;
            mval[0]->value_ = mval[0]->right_value_;
          } else if ( mval[1]->left_alloc_ == 1 && mval[1]->left_level_ == mval[0]->level_ ) {
            mval[0]->value_ = mval[1]->left_value_;
          } else {
            mval[0]->value_ = 0.5*(t1 + t2);
          //  std::cout << "A: interpolating " << " x value : " << mval[0]->x_ << " right level : " << mval[0]->right_level_ << " right alloc: " << mval[0]->right_alloc_ << " level: " << mval[0]->level_ << std::endl;
          }
          if ( mval[1]->right_alloc_ == 1 && mval[1]->right_level_ == mval[2]->level_ ) {
         //   std::cout << "B: Using right value : " << mval[1]->right_level_ << " x value : " << mval[2]->x_ << " right value : " << mval[1]->right_value_ << std::endl;
            mval[2]->value_ = mval[1]->right_value_;
          } else if ( mval[2]->left_alloc_ == 1 && mval[2]->left_level_ == mval[2]->level_ ) {
            mval[2]->value_ = mval[1]->left_value_;
          } else {
            mval[2]->value_ = 0.5*(t2 + t3);
          //  std::cout << "B: interpolating " << " x value : " << mval[2]->x_ << " right level : " << mval[0]->right_level_ << " right alloc: " << mval[0]->right_alloc_ << " level: " << mval[0]->level_  << std::endl;
          }
          if ( mval[2]->right_alloc_ == 1 && mval[2]->right_level_ == mval[4]->level_ ) {
        //    std::cout << "C: Using right value : " << mval[2]->right_level_ << " x value : " << mval[4]->x_ << " right value : " << mval[2]->right_value_ << std::endl;
            mval[4]->value_ = mval[2]->right_value_;
          } else if ( mval[3]->left_alloc_ == 1 && mval[3]->left_level_ == mval[4]->level_ ) {
            mval[4]->value_ = mval[3]->left_value_;
          } else {
            mval[4]->value_ = 0.5*(t3 + t4);
          //  std::cout << "C: interpolating " <<  " x value : " << mval[4]->x_ << " right level : " << mval[0]->right_level_ << " right alloc: " << mval[0]->right_alloc_ << " level: " << mval[0]->level_  << std::endl;
          }
          if ( mval[3]->right_alloc_ == 1 && mval[3]->right_level_ == mval[6]->level_ ) {
         //   std::cout << "D: Using right value : " << mval[3]->right_level_ << " x value : " << mval[6]->x_ << " right value : " << mval[3]->right_value_ <<  std::endl;
            mval[6]->value_ = mval[3]->right_value_;
          } else if ( mval[4]->left_alloc_ == 1 && mval[4]->left_level_ == mval[6]->level_ ) {
            mval[6]->value_ = mval[4]->left_value_;
          } else {
            mval[6]->value_ = 0.5*(t4 + t5);
          //  std::cout << "D: interpolating " << " x value : " << mval[6]->x_ << " right level : " << mval[0]->right_level_ << " right alloc: " << mval[0]->right_alloc_ << " level: " << mval[0]->level_  << std::endl;
          }
        } else {
          // other user defined options not implemented yet
          interpolation();
          BOOST_ASSERT(false);
        }

        // the initial data for the child mesh comes from the parent mesh
        naming::id_type here = applier::get_applier().get_runtime_support_gid();
        components::component_type logging_type =
                  components::get_component_type<components::amr::server::logging>();
        components::component_type function_type =
                  components::get_component_type<components::amr::stencil>();
        components::amr::amr_mesh_tapered child_mesh (
                  components::amr::amr_mesh_tapered::create(here, 1, true));

        std::vector<naming::id_type> initial_data;
        for (i=0;i<8;i++) {
          initial_data.push_back(gval[i]);
        }

        bool do_logging = false;
        if ( par.loglevel > 0 ) {
          do_logging = true;
        }
        std::vector<naming::id_type> result_data(
            child_mesh.execute(initial_data, function_type,
              do_logging ? logging_type : components::component_invalid,par));
  
        access_memory_block<stencil_data> r_val1, r_val2, resultval;
        boost::tie(r_val1, r_val2, resultval) = 
            get_memory_block_async<stencil_data>(result_data[3], result_data[4], result);

        // overwrite the coarse point computation
        resultval->value_ = r_val1->value_;
  
        // remember right neighbor value
        resultval->right_alloc_ = 1;
        resultval->right_value_ = r_val2->value_;
        resultval->right_level_ = r_val2->level_;
      //  std::cout << "result x value : " << resultval->x_ << " result right level : " << resultval->right_level_ << " result right alloc: " << resultval->right_alloc_ << " result level: " << resultval->level_ << " result right value : " << resultval->right_value_ << " result value " << resultval->value_ << std::endl;
        
        // release result data
        for (std::size_t i = 0; i < result_data.size(); ++i) 
            components::stubs::memory_block::free(result_data[i]);
      }

      return 0;
    }
    
    ///////////////////////////////////////////////////////////////////////////
    // Implement a finer mesh via interpolation of inter-mesh points
    // Compute the result value for the current time step
    int stencil::finer_mesh_initial(naming::id_type const& result, 
        std::vector<naming::id_type> const& gids, std::size_t level, double x, Parameter const& par) 
    {

      // the initial data for the child mesh comes from the parent mesh
      naming::id_type here = applier::get_applier().get_runtime_support_gid();
      components::component_type logging_type =
                components::get_component_type<components::amr::server::logging>();
      components::component_type function_type =
                components::get_component_type<components::amr::stencil>();
      components::amr::amr_mesh_left child_mesh (
                components::amr::amr_mesh_left::create(here, 1, true));

      bool do_logging = false;
      if ( par.loglevel > 0 ) {
        do_logging = true;
      }
      std::vector<naming::id_type> result_data(
          child_mesh.init_execute(function_type,
            do_logging ? logging_type : components::component_invalid,
            level, x, par));

     //  if using mesh_left
//#if 0
      access_memory_block<stencil_data> r_val1, r_val2, r_val3, resultval;
      boost::tie(r_val1, r_val2, r_val3, resultval) = 
          get_memory_block_async<stencil_data>(result_data[2], result_data[4], result_data[6], result);
      //overwrite the coarse point computation
      resultval->value_ = r_val2->value_;
  
      // remember neighbor value
      resultval->right_alloc_ = 1;
      resultval->right_value_ = r_val3->value_;
      resultval->right_level_ = r_val3->level_;

      resultval->left_alloc_ = 1;
      resultval->left_value_ = r_val1->value_;
      resultval->left_level_ = r_val1->level_;
//#endif

     // if using mesh_tapered
#if 0
     access_memory_block<stencil_data> r_val1, r_val2, resultval;
     boost::tie(r_val1, r_val2, resultval) = 
     get_memory_block_async<stencil_data>(result_data[3], result_data[4], result);

      // overwrite the coarse point computation
      resultval->value_ = r_val1->value_;
  
      // remember neighbor value
      resultval->right_alloc_ = 1;
      resultval->right_value_ = r_val2->value_;
      resultval->right_level_ = r_val2->level_;
#endif
      
      // release result data
      for (std::size_t i = 0; i < result_data.size(); ++i) 
          components::stubs::memory_block::free(result_data[i]);

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
                stubs::logging::logentry(log_, val.get(), row, par);
        }
        return result;
    }

    void stencil::init(std::size_t numsteps, naming::id_type const& logging)
    {
        numsteps_ = numsteps;
        log_ = logging;
    }

}}}

