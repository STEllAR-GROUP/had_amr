# Copyright (c) 2007-2009 Hartmut Kaiser
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

set(sources
    amr_c.cpp
    logging.cpp
    stencil.cpp)

set(headers
    logging.hpp
    stencil.hpp
    stencil_data.hpp
    stencil_data_locking.hpp
    stencil_functions.hpp)

set(ini_files
    had_amr_c.ini)

set(dependencies had_amr_c_test_component)

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
    set(headers ${headers} ../mpreal.h ../serialize_mpreal.hpp)
    set(sources ${sources} ../mpreal.cpp ../serialize_mpreal.cpp)
  endif()
endif()

add_definitions(-DHPX_ACTION_ARGUMENT_LIMIT=6)

add_hpx_component(had_amr_c
  SOURCES ${sources}
  HEADERS ${headers}
  FOLDER "Had_Amr"
  INI ${ini_files}
  DEPENDENCIES ${dependencies})

