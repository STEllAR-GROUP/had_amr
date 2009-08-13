//  Copyright (c) 2007-2009 Hartmut Kaiser
// 
//  Distributed under the Boost Software License, Version 1.0. (See accompanying 
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#if !defined(HPX_COMPONENTS_AMR_SERVER_STENCIL_VALUE_OCT_17_2008_0848AM)
#define HPX_COMPONENTS_AMR_SERVER_STENCIL_VALUE_OCT_17_2008_0848AM

#include <hpx/hpx_fwd.hpp>
#include <hpx/runtime/applier/applier.hpp>
#include <hpx/runtime/threads/thread.hpp>
#include <hpx/runtime/components/component_type.hpp>
#include <hpx/runtime/components/server/managed_component_base.hpp>
#include <hpx/runtime/actions/component_action.hpp>
#include <hpx/lcos/counting_semaphore.hpp>
#include <hpx/lcos/mutex.hpp>

#include <boost/noncopyable.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/bind.hpp>

#include "stencil_value_in_adaptor.hpp"
#include "stencil_value_out_adaptor.hpp"

///////////////////////////////////////////////////////////////////////////////
namespace hpx { namespace components { namespace amr { namespace server 
{
    namespace detail
    {
        // same as counting semaphore, but initialized to 1
        struct initialized_semaphore : lcos::counting_semaphore
        {
            initialized_semaphore() : lcos::counting_semaphore(1) {}
        };
    }

    /// \class stencil_value stencil_value.hpp hpx/components/amr/server/stencil_value.hpp
    template <int N>
    class HPX_COMPONENT_EXPORT stencil_value 
      : public components::detail::managed_component_base<stencil_value<N> >
    {
    protected:
        // the in_adaptors_type is the concrete stencil_value_in_adaptor
        // of the proper type
        typedef amr::server::stencil_value_in_adaptor in_adaptor_type;

        // the out_adaptors_type is the concrete stencil_value_out_adaptor
        // of the proper type
        typedef 
            managed_component<amr::server::stencil_value_out_adaptor>
        out_adaptor_type;

    public:
        /// Construct a new stencil_value instance
        stencil_value();

        /// Destruct this stencil instance
        ~stencil_value();

        /// \brief finalize() will be called just before the instance gets 
        ///        destructed
        ///
        /// \param appl [in] The applier to be used for finalization of the 
        ///             component instance. 
        void finalize();

        /// The function get will be called by the out-ports whenever 
        /// the current value has been requested.
        naming::id_type get_value(int i);

        ///////////////////////////////////////////////////////////////////////
        // parcel action code: the action to be performed on the destination 
        // object (the accumulator)
        enum actions
        {
            stencil_value_call = 0,
            stencil_value_get_output_ports = 1,
            stencil_value_connect_input_ports = 2,
            stencil_value_set_functional_component = 3,
        };

        /// Main thread function looping through all timesteps
        threads::thread_state main();

        /// This is the main entry point of this component. Calling this 
        /// function (by applying the call_action) will trigger the repeated 
        /// execution of the whole time step evolution functionality.
        ///
        /// It invokes the time series evolution for this data point using the
        /// data referred to by the parameter \a initial. After finishing 
        /// execution it returns a reference to the result as its return value
        /// (parameter \a result)
        naming::id_type call(naming::id_type const& initial);

        /// Return the gid's of the output ports associated with this 
        /// \a stencil_value instance.
        std::vector<naming::id_type> get_output_ports();

        /// Connect the destinations given by the provided gid's with the 
        /// corresponding input ports associated with this \a stencil_value 
        /// instance.
        void connect_input_ports(std::vector<naming::id_type> const& gids);

        /// Set the gid of the component implementing the actual time evolution
        /// functionality
        void set_functional_component(naming::id_type const& gid, int row, 
            int column);

        // Each of the exposed functions needs to be encapsulated into an action
        // type, allowing to generate all required boilerplate code for threads,
        // serialization, etc.
        typedef hpx::actions::result_action1<
            stencil_value, naming::id_type, stencil_value_call, 
            naming::id_type const&, &stencil_value::call
        > call_action;

        typedef hpx::actions::result_action0<
            stencil_value, std::vector<naming::id_type>, 
            stencil_value_get_output_ports, &stencil_value::get_output_ports
        > get_output_ports_action;

        typedef hpx::actions::action1<
            stencil_value, stencil_value_connect_input_ports, 
            std::vector<naming::id_type> const&,
            &stencil_value::connect_input_ports
        > connect_input_ports_action;

        typedef hpx::actions::action3<
            stencil_value, stencil_value_set_functional_component, 
            naming::id_type const&, int, int, 
            &stencil_value::set_functional_component
        > set_functional_component_action;

    private:
        bool is_called_;                              // is one of the 'main' stencils
        threads::thread_id_type driver_thread_;

        detail::initialized_semaphore sem_in_[N];
        lcos::counting_semaphore sem_out_[N];
        lcos::counting_semaphore sem_result_;

        boost::scoped_ptr<in_adaptor_type> in_[N];    // adaptors used to gather input
        boost::scoped_ptr<out_adaptor_type> out_[N];  // adaptors used to provide result

        naming::id_type value_gids_[2];               // reference to previous values
        naming::id_type functional_gid_;              // reference to functional code

        int row_;             // position of this stencil in whole graph
        int column_;

        typedef lcos::mutex mutex_type;
        mutex_type mtx_;
    };

}}}}

#endif

