# Copyright (c) 2007-2009 Hartmut Kaiser
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

set(ini_files
    had_amr_c.ini)

# this is a hack which needs to be cleaned up (see ticket #468)
if(MSVC)
  set(dependencies
    distributing_factory${HPX_POSTFIX}
    had_amr_component
    had_amr_c_test_lib)
else()
  set(dependencies
    hpx_component_distributing_factory${HPX_POSTFIX}
    had_amr_component
    had_amr_c_test_lib)
endif()

if(FOUND_GMP)
  # add GMP as a dependency
  set(dependencies
      ${dependencies}
      ${GMP_LIBRARY})
  include_directories(${GMP_INCLUDE_DIR})

  if(MPFR_FOUND)
    # add MPFR as a dependency
    set(dependencies
        ${dependencies}
        ${MPFR_LIBRARY})

    # FIXME: HPX_* identifiers please
    add_definitions(-DMPFR_FOUND=1)
    #add_definitions(-DMPFR_USE_NED_ALLOCATOR=1)

    include_directories(${MPFR_INCLUDE_DIR})

    # allow the mpreal class to take advantage of move semantics
    # FIXME: HPX_* identifiers please
    add_definitions(-DMPFR_USE_BOOST_MOVE=1)
  endif()
endif()

add_hpx_component(had_amr_c
  AUTOGLOB
  FOLDER "Had_Amr"
  INI ${ini_files}
  DEPENDENCIES ${dependencies})

