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
    class HPX_COMPONENT_EXPORT rk_mesh
      : public simple_component_base<rk_mesh>
    {
    private:
        typedef simple_component_base<rk_mesh> base_type;

    public:
        rk_mesh();

        // components must contain a typedef for wrapping_type defining the
        // component type used to encapsulate instances of this component
        typedef amr::server::rk_mesh wrapping_type;

        ///////////////////////////////////////////////////////////////////////
        // parcel action code: the action to be performed on the destination 
        // object (the accumulator)
        enum actions
        {
            rk_mesh_init_execute = 0,
            rk_mesh_execute = 1
        };

        /// This is the main entry point of this component. 
        std::vector<naming::id_type> init_execute(
            components::component_type function_type, std::size_t numvalues, 
            std::size_t numsteps,
            components::component_type logging_type, Parameter const& par);

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
            rk_mesh, std::vector<naming::id_type>, rk_mesh_init_execute, 
            components::component_type, std::size_t, std::size_t,
            components::component_type, Parameter const&, &rk_mesh::init_execute
        > init_execute_action;

        typedef hpx::actions::result_action6<
            rk_mesh, std::vector<naming::id_type>, rk_mesh_execute, 
            std::vector<naming::id_type> const&,
            components::component_type, std::size_t, std::size_t,
            components::component_type, Parameter const&, &rk_mesh::execute
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

        static void prep_ports_nine(Array3D &dst_port,Array3D &dst_src,
                                    Array3D &dst_step,Array3D &dst_size,
                                    Array3D &src_size,int numvalues,Parameter const& par);

    private:
        std::size_t numvalues_;
    };

}}}}

#endif
