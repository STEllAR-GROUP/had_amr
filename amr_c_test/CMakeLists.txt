# Copyright (c) 2007-2012 Hartmut Kaiser
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

set(sources
    had_amr_c_test.cpp
    rand.cpp)

set(headers
    rand.hpp)

if(GMP_FOUND)
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
    include_directories(${MPFR_INCLUDE_DIR})

    # add the mpfr wrapper class sources
    set(headers ${headers} ../mpreal.h)
    set(sources ${sources} ../mpreal.cpp)
  endif()
endif()

add_hpx_component(had_amr_c_test
  SOURCES ${sources}
  HEADERS ${headers}
  FOLDER "Had_Amr"
  DEPENDENCIES ${dependencies})

