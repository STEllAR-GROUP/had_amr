//  Copyright (c) 2007-2010 Hartmut Kaiser
//  Matt Anderson
// 
//  Distributed under the Boost Software License, Version 1.0. (See accompanying 
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#if !defined(HPX_COMPONENTS_AMR_SERVER_RK_MESH_FEB_25_2010_0153AM)
#define HPX_COMPONENTS_AMR_SERVER_RK_MESH_FEB_25_2010_0153AM

#include <hpx/hpx_fwd.hpp>
#include <hpx/runtime/applier/applier.hpp>
#include <hpx/runtime/threads/thread.hpp>
#include <hpx/runtime/components/component_type.hpp>
#include <hpx/runtime/components/server/simple_component_base.hpp>
#include <hpx/components/distributing_factory/distributing_factory.hpp>

#include "../../parameter.hpp"

///////////////////////////////////////////////////////////////////////////////
namespace hpx { namespace components { namespace amr { namespace server 
{
    ///////////////////////////////////////////////////////////////////////////
    class HPX_COMPONENT_EXPORT uni_amr
      : public simple_component_base<uni_amr>
    {
    private:
        typedef simple_component_base<uni_amr> base_type;

    public:
        uni_amr();

        // components must contain a typedef for wrapping_type defining the
        // component type used to encapsulate instances of this component
        typedef amr::server::uni_amr wrapping_type;

        ///////////////////////////////////////////////////////////////////////
        // parcel action code: the action to be performed on the destination 
        // object (the accumulator)
        enum actions
        {
            uni_amr_init_execute = 0,
            uni_amr_execute = 1
        };

        /// This is the main entry point of this component. 
        std::vector<naming::id_type> init_execute(
            components::component_type function_type,
            components::component_type logging_type, 
            std::size_t level,
            had_double_type x,
            Parameter const& par);

        std::vector<naming::id_type> execute(
            std::vector<naming::id_type> const& initialdata,
            components::component_type function_type, std::size_t numvalues, 
            std::size_t numsteps,
            components::component_type logging_type, Parameter const& par);

        ///////////////////////////////////////////////////////////////////////
        // Each of the exposed functions needs to be encapsulated into an action
        // type, allowing to generate all required boilerplate code for threads,
        // serialization, etc.
        typedef hpx::actions::result_action5<
            uni_amr, std::vector<naming::id_type>, uni_amr_init_execute, 
            components::component_type,
            components::component_type,
            std::size_t, had_double_type,
            Parameter const&, &uni_amr::init_execute
        > init_execute_action;

        typedef hpx::actions::result_action6<
            uni_amr, std::vector<naming::id_type>, uni_amr_execute, 
            std::vector<naming::id_type> const&,
            components::component_type, std::size_t, std::size_t,
            components::component_type, Parameter const&, &uni_amr::execute
        > execute_action;

    protected:
        typedef 
            components::distributing_factory::iterator_range_type
        distributed_iterator_range_type;

        static void init(distributed_iterator_range_type const& functions,
            distributed_iterator_range_type const& logging,
            std::size_t numsteps);

        void prepare_initial_data(
            distributed_iterator_range_type const& functions, 
            std::vector<naming::id_type>& initial_data,
            std::size_t level, had_double_type xmin,
            Parameter const& par);

        static void init_stencils(
            distributed_iterator_range_type const& stencils,
            distributed_iterator_range_type const& functions, int static_step, 
            int numvalues,Parameter const& par);

        static void get_output_ports(
            distributed_iterator_range_type const& stencils,
            std::vector<std::vector<naming::id_type> >& outputs);

        static void connect_input_ports(
            components::distributing_factory::result_type const* stencils,
            std::vector<std::vector<std::vector<naming::id_type> > > const& outputs,
            Parameter const& par);

        static void execute(distributed_iterator_range_type const& stencils, 
            std::vector<naming::id_type> const& initial_data, 
            std::vector<naming::id_type>& result_data);

        static void start_row(distributed_iterator_range_type const& stencils);

        static void prep_ports(Array3D &dst_port,Array3D &dst_src,
                                    Array3D &dst_step,Array3D &dst_size,
                                    Array3D &src_size,int numvalues,Parameter const& par);

    private:
        std::size_t numvalues_;
    };

}}}}

#endif