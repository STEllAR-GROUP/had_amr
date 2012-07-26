//  Copyright (c) 2007-2011 Hartmut Kaiser
//                          Matt Anderson 
//  Distributed under the Boost Software License, Version 1.0. (See accompanying 
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#if !defined(HPX_COMPONENTS_HAD_CONFIG_FEB_08_2010_0226PM)
#define HPX_COMPONENTS_HAD_CONFIG_FEB_08_2010_0226PM

#ifdef MPFR_FOUND
#   ifdef HAD_AMR_USE_MPET
#       include <mp/mp.hpp>
#       include <mp/mpfr.hpp>

typedef mp::mp_<mp::mpfr> had_double_type;
#   else
#   include "amr_c_test/mpreal.h"
#   include "amr_c_test/serialize_mpreal.hpp"

typedef mpfr::mpreal had_double_type;
#   endif
#else
typedef double had_double_type;
#endif
const int num_eqns = 3;
const int maxlevels = 20;

#endif
