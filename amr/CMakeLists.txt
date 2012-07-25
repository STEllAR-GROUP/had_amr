# Copyright (c) 2007-2012 Hartmut Kaiser
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

set(sources
    server/dynamic_stencil_value.cpp
    server/functional_component.cpp
    server/stencil_value_out_adaptor.cpp
    server/unigrid_mesh.cpp
    amr.cpp
    parameter.cpp)

set(headers
    server/dynamic_stencil_value.hpp
    server/dynamic_stencil_value_impl.hpp
    server/functional_component.hpp
    server/stencil_value_out_adaptor.hpp
    server/unigrid_mesh.hpp
    stubs/dynamic_stencil_value.hpp
    stubs/functional_component.hpp
    stubs/unigrid_mesh.hpp
    dynamic_stencil_value.hpp
    functional_component.hpp
    unigrid_mesh.hpp)

set(ini_files
    had_amr.ini)

set(dependencies
    hpx_component_distributing_factory${HPX_POSTFIX}
    had_amr_c_component)

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

add_hpx_component(had_amr
  SOURCES ${sources}
  HEADERS ${headers}
  FOLDER "Had_Amr"
  FOLDER "Had_Amr"
  INI ${ini_files}
  DEPENDENCIES ${dependencies})

