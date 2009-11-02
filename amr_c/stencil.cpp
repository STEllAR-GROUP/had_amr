//  Copyright (c) 2007-2009 Hartmut Kaiser
// 
//  Distributed under the Boost Software License, Version 1.0. (See accompanying 
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <hpx/hpx.hpp>
#include <hpx/lcos/future_wait.hpp>

#include <boost/foreach.hpp>

#include "../amr/amr_mesh.hpp"
#include "../amr/amr_mesh_tapered.hpp"

#include "stencil.hpp"
#include "logging.hpp"
#include "stencil_data.hpp"
#include "stencil_functions.hpp"

///////////////////////////////////////////////////////////////////////////////
namespace hpx { namespace components { namespace amr 
{
    ///////////////////////////////////////////////////////////////////////////
    namespace detail
    {
        // helper functions to get several memory pointers asynchronously
        inline boost::tuple<
            access_memory_block<stencil_data>, access_memory_block<stencil_data>
          , access_memory_block<stencil_data> >
        get_async(naming::id_type const& g1, naming::id_type const& g2
          , naming::id_type const& g3)
        {
            return wait(components::stubs::memory_block::get_async(g1)
              , components::stubs::memory_block::get_async(g2)
              , components::stubs::memory_block::get_async(g3));
        }

        inline boost::tuple<
            access_memory_block<stencil_data>, access_memory_block<stencil_data>
          , access_memory_block<stencil_data>, access_memory_block<stencil_data> >
        get_async(naming::id_type const& g1, naming::id_type const& g2
          , naming::id_type const& g3, naming::id_type const& g4)
        {
            return wait(components::stubs::memory_block::get_async(g1)
              , components::stubs::memory_block::get_async(g2)
              , components::stubs::memory_block::get_async(g3)
              , components::stubs::memory_block::get_async(g4));
        }

        inline boost::tuple<
            access_memory_block<stencil_data>, access_memory_block<stencil_data>
          , access_memory_block<stencil_data>, access_memory_block<stencil_data>
          , access_memory_block<stencil_data> >
        get_async(naming::id_type const& g1, naming::id_type const& g2
          , naming::id_type const& g3, naming::id_type const& g4
          , naming::id_type const& g5)
        {
            return wait(components::stubs::memory_block::get_async(g1)
              , components::stubs::memory_block::get_async(g2)
              , components::stubs::memory_block::get_async(g3)
              , components::stubs::memory_block::get_async(g4)
              , components::stubs::memory_block::get_async(g5));
        }
        inline boost::tuple<
            access_memory_block<stencil_data>, access_memory_block<stencil_data>
          , access_memory_block<stencil_data>, access_memory_block<stencil_data>
          , access_memory_block<stencil_data>, access_memory_block<stencil_data> >
        get_async(naming::id_type const& g1, naming::id_type const& g2
          , naming::id_type const& g3, naming::id_type const& g4
          , naming::id_type const& g5, naming::id_type const& g6)
        {
            return wait(components::stubs::memory_block::get_async(g1)
              , components::stubs::memory_block::get_async(g2)
              , components::stubs::memory_block::get_async(g3)
              , components::stubs::memory_block::get_async(g4)
              , components::stubs::memory_block::get_async(g5)
              , components::stubs::memory_block::get_async(g6));
        }
    }

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
                detail::get_async(gids[0], gids[1], gids[2], result);
        } 
        else if (gids.size() == 2) {
            boost::tie(val1, val2, resultval) = 
                detail::get_async(gids[0], gids[1], result);
        } 
        else if (gids.size() == 5) {
            boost::tie(val1, val2, val3, val4, val5, resultval) = 
                detail::get_async(gids[0], gids[1], gids[2], gids[3], gids[4], result);
        } 
        else {
            BOOST_ASSERT(false);    // should not happen
        }

        // make sure all input data items agree on the time step number
       // BOOST_ASSERT(val1->timestep_ == val2->timestep_);
       // if ( gids.size() == 3 ) {
       //   BOOST_ASSERT(val1->timestep_ == val3->timestep_);
       // }

        // the predecessor
        std::size_t middle_timestep;
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

        if (middle_timestep < numsteps_) {

            if (gids.size() == 3) {
              // this is the actual calculation, call provided (external) function
              evaluate_timestep(val1.get_ptr(), val2.get_ptr(), val3.get_ptr(), 
                  resultval.get_ptr(), numsteps_,par);

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
                  resultval.get_ptr(), numsteps_,par);

              // copy over the coordinate value to the result
              resultval->x_ = val3->x_;
            }

            std::size_t allowedl = par.allowedl;
            if ( val2->refine_ && gids.size() == 5 && val2->level_ < allowedl ) {
              finer_mesh(result, gids,par);
            }

            if (log_)     // send result to logging instance
                stubs::logging::logentry(log_, resultval.get(), row);
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
            }
            ++resultval->timestep_;
        }
 
        // set return value difference between actual and required number of
        // timesteps (>0: still to go, 0: last step, <0: overdone)
        return numsteps_ - resultval->timestep_;
    }

    ///////////////////////////////////////////////////////////////////////////
    // Implement a finer mesh via interpolation of inter-mesh points
    // Compute the result value for the current time step
    int stencil::finer_mesh(naming::id_type const& result, 
        std::vector<naming::id_type> const& gids, Parameter const& par) 
    {

      naming::id_type gval1, gval2, gval3, gval4, gval5, gval6;
      boost::tie(gval1, gval2, gval3, gval4, gval5, gval6) = 
                        components::wait(components::stubs::memory_block::clone_async(gids[0]), 
                             components::stubs::memory_block::clone_async(gids[1]),
                             components::stubs::memory_block::clone_async(gids[2]),
                             components::stubs::memory_block::clone_async(gids[3]),
                             components::stubs::memory_block::clone_async(gids[4]),
                             components::stubs::memory_block::clone_async(gids[4]));

      access_memory_block<stencil_data> mval1, mval2, mval3, mval4, mval5, mval6;
      boost::tie(mval1, mval2, mval3, mval4, mval5, mval6) = 
          detail::get_async(gval1, gval2, gval3, gval4, gval5, gval6);

      // increase the level by one
      ++mval1->level_;
      ++mval2->level_;
      ++mval3->level_;
      ++mval4->level_;
      ++mval5->level_;
      ++mval6->level_;

      // initialize timestep for the fine mesh
      mval1->timestep_ = 0;
      mval2->timestep_ = 0;
      mval3->timestep_ = 0;
      mval4->timestep_ = 0;
      mval5->timestep_ = 0;
      mval6->timestep_ = 0;

      // initialize the index
      mval1->index_ = 0;
      mval2->index_ = 1;
      mval3->index_ = 2;
      mval4->index_ = 3;
      mval5->index_ = 4;
      mval6->index_ = 5;

      // temporarily store the values before overwriting them
      double t1,t2,t3,t4,t5;
      double x1,x2,x3,x4,x5;
      t1 = mval1->value_;
      t2 = mval2->value_;
      t3 = mval3->value_;
      t4 = mval4->value_;
      t5 = mval5->value_;

      x1 = mval1->x_;
      x2 = mval2->x_;
      x3 = mval3->x_;
      x4 = mval4->x_;
      x5 = mval5->x_;

      // this updates the coordinate position
      mval1->x_ = x2;
      mval2->x_ = 0.5*(x2+x3);
      mval3->x_ = x3;
      mval4->x_ = 0.5*(x3+x4);
      mval5->x_ = x4;
      mval6->x_ = 0.5*(x4+x5);
      
      // ------------------------------
      // bias the stencil to the right
      mval1->value_ = t2;
      
      if ( par.linearbounds == 1 ) {
        // linear interpolation
        mval2->value_ = 0.5*(t2 + t3);
      } else {
        // other user defined options not implemented yet
        interpolation();
        BOOST_ASSERT(false);
      }

      mval3->value_ = t3;

      if ( par.linearbounds == 1 ) {
        // linear interpolation
        mval4->value_ = 0.5*(t3 + t4);
      } else {
        // other user defined options not implemented yet
        interpolation();
        BOOST_ASSERT(false);
      }

      mval5->value_ = t4;

      if ( par.linearbounds == 1 ) {
        // linear interpolation
        mval6->value_ = 0.5*(t4 + t5);
      } else {
        // other user defined options not implemented yet
        interpolation();
        BOOST_ASSERT(false);
      }
      
      // end bias the stencil to the right
      // ------------------------------

      // the initial data for the child mesh comes from the parent mesh
      naming::id_type here = applier::get_applier().get_runtime_support_gid();
      components::component_type logging_type =
                components::get_component_type<components::amr::server::logging>();
      components::component_type function_type =
                components::get_component_type<components::amr::stencil>();
      components::amr::amr_mesh_tapered child_mesh (
                components::amr::amr_mesh_tapered::create(here, 1, true));

      std::vector<naming::id_type> initial_data;
      initial_data.push_back(gval1);
      initial_data.push_back(gval2);
      initial_data.push_back(gval3);
      initial_data.push_back(gval4);
      initial_data.push_back(gval5);
      initial_data.push_back(gval6);

      std::size_t numvalues = 6;
      std::size_t numsteps = 2;

      bool do_logging = false;
      if ( par.loglevel > 0 ) {
        do_logging = true;
      }
      std::vector<naming::id_type> result_data(
          child_mesh.execute(initial_data, function_type, numvalues, numsteps, 
            do_logging ? logging_type : components::component_invalid,par));

      access_memory_block<stencil_data> r_val1, r_val2, resultval;
      boost::tie(r_val1, r_val2, resultval) = 
          detail::get_async(result_data[2], result_data[3], result);

      // overwrite the coarse point computation
      resultval->value_ = r_val1->value_;

      // remember right neighbor value
      resultval->right_alloc_ = 1;
      resultval->right_value_ = r_val2->value_;
      resultval->right_level_ = r_val2->level_;

      // release result data
      for (std::size_t i = 0; i < result_data.size(); ++i) 
          components::stubs::memory_block::free(result_data[i]);

      for (std::size_t i = 0; i < initial_data.size(); ++i) 
          components::stubs::memory_block::free(initial_data[i]);

      return 0;
    }

    ///////////////////////////////////////////////////////////////////////////
    naming::id_type stencil::alloc_data(int item, int maxitems, int row,
        Parameter const& par)
    {
        naming::id_type result = components::stubs::memory_block::create(
            applier::get_applier().get_runtime_support_gid(), sizeof(stencil_data));

        if (-1 != item) {
            // provide initial data for the given data value 
            access_memory_block<stencil_data> val(
                components::stubs::memory_block::checkout(result));

            // call provided (external) function
            generate_initial_data(val.get_ptr(), item, maxitems, row, par);

            if (log_)         // send initial value to logging instance
                stubs::logging::logentry(log_, val.get(), row);
        }
        return result;
    }

    void stencil::init(std::size_t numsteps, naming::id_type const& logging)
    {
        numsteps_ = numsteps;
        log_ = logging;
    }

}}}

