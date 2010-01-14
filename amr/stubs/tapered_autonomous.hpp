//  Copyright (c) 2007-2010 Hartmut Kaiser
// 
//  Distributed under the Boost Software License, Version 1.0. (See accompanying 
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#if !defined(HPX_COMPONENTS_AMR_STUBS_AMR_TAPERED_AUTONOMOUS_JAN_11_2010_0117PM)
#define HPX_COMPONENTS_AMR_STUBS_AMR_TAPERED_AUTONOMOUS_JAN_11_2010_0117PM

#include <hpx/hpx_fwd.hpp>
#include <hpx/runtime/applier/applier.hpp>
#include <hpx/runtime/threads/thread.hpp>
#include <hpx/runtime/components/component_type.hpp>
#include <hpx/runtime/components/stubs/stub_base.hpp>

#include "../server/tapered_autonomous.hpp"

///////////////////////////////////////////////////////////////////////////////
namespace hpx { namespace components { namespace amr { namespace stubs 
{
    ///////////////////////////////////////////////////////////////////////////
    struct tapered_autonomous
      : components::stubs::stub_base<amr::server::tapered_autonomous>
    {
        ///////////////////////////////////////////////////////////////////////
        // exposed functionality of this component

        ///////////////////////////////////////////////////////////////////////
        static lcos::future_value<std::vector<naming::id_type> > 
        init_execute_async(naming::id_type const& gid, 
            components::component_type function_type, 
            components::component_type logging_type,
            std::size_t level,double x,Parameter const& par)
        {
            // Create an eager_future, execute the required action,
            // we simply return the initialized future_value, the caller needs
            // to call get() on the return value to obtain the result
            typedef amr::server::tapered_autonomous::init_execute_action action_type;
            return lcos::eager_future<action_type>(gid, function_type,
                logging_type,level,x, par);
        }

        static std::vector<naming::id_type> init_execute(naming::id_type const& gid, 
            components::component_type function_type,
            components::component_type logging_type,
            std::size_t level, double x,Parameter const& par)
        {
            return init_execute_async(gid, function_type, logging_type,level,x,par).get();
        }

        ///////////////////////////////////////////////////////////////////////
        static lcos::future_value<std::vector<naming::id_type> > 
        execute_async(naming::id_type const& gid, 
            std::vector<double> const& initial_data,
            components::component_type function_type, 
            components::component_type logging_type,
            Parameter const& par)
        {
            // Create an eager_future, execute the required action,
            // we simply return the initialized future_value, the caller needs
            // to call get() on the return value to obtain the result
            typedef amr::server::tapered_autonomous::execute_action action_type;
            return lcos::eager_future<action_type>(gid, initial_data, 
                function_type, logging_type, par);
        }

        static std::vector<naming::id_type> execute(naming::id_type const& gid, 
            std::vector<double> const& initial_data,
            components::component_type function_type, 
            components::component_type logging_type,
            Parameter const& par)
        {
            return execute_async(gid, initial_data, function_type,
                logging_type,par).get();
        }
    };

}}}}

#endif