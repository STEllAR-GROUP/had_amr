//  Copyright (c) 2007-2012 Hartmut Kaiser
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#if !defined(AMR_C_FUNCTIONS_FEB_16_2009_0141PM)
#define AMR_C_FUNCTIONS_FEB_16_2009_0141PM

#include <hpx/config/export_definitions.hpp>
#include "../parameter.h"
#include "../had_config.hpp"

#if HAD_AMR_C_TEST_EXPORTS
#define HAD_AMR_C_TEST_EXPORT HPX_SYMBOL_EXPORT
#else
#define HAD_AMR_C_TEST_EXPORT HPX_SYMBOL_IMPORT
#endif

///////////////////////////////////////////////////////////////////////////////
/// The function \a generate_initial_data will be called to initialize the
/// given instance of 'stencil_data'
HAD_AMR_C_TEST_EXPORT int generate_initial_data(
    stencil_data* data, std::size_t item, std::size_t maxitems, std::size_t row,
            Par const& par);

/// The function \a evaluate_timestep will be called to compute the result data
/// for the given timestep
HAD_AMR_C_TEST_EXPORT int rkupdate(std::vector< nodedata* > const& val,
    stencil_data* result, std::vector< had_double_type* > const& vecx, int size,
    bool boundary, int *bbox, int compute_index,
    had_double_type const&, had_double_type const&, had_double_type const&,
    int level, Par const& par);

#endif
