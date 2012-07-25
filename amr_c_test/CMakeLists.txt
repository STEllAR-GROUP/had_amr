# Copyright (c) 2007-2012 Hartmut Kaiser
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

set(sources
    had_amr_c_test.cpp
    rand.cpp)

set(headers
    rand.hpp stencil_functions.hpp)

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

    # add the mpfr wrapper class sources
    set(headers ${headers} mpreal.h serialize_mpreal.hpp)
    set(sources ${sources} mpreal.cpp serialize_mpreal.cpp init_mpfr.hpp)
  endif()
endif()

add_definitions(-DHAD_AMR_C_TEST_EXPORTS)

add_hpx_library(had_amr_c_test
  MODULE had_amr_c_test
  SOURCES ${sources}
  HEADERS ${headers}
  FOLDER "Had_Amr"
  DEPENDENCIES ${dependencies})

