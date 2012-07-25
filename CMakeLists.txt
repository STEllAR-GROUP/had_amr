# Copyright (c) 2007-2012 Hartmut Kaiser
# Copyright (c) 2009-2010 Matt Anderson
# Copyright (c) 2011      Bryce Lelbach
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

project(had_amr CXX)

cmake_minimum_required(VERSION 2.8)

if(NOT HPX_ROOT)
    if(NOT $ENV{HPX_ROOT} STREQUAL "")
        set(HPX_ROOT $ENV{HPX_ROOT})
    else()
        hpx_error("HPX_ROOT not set, unable to find hpx!")
    endif()
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${HPX_ROOT}/share/cmake-2.8/Modules)

if(MSVC)
  set_property(GLOBAL PROPERTY USE_FOLDERS ON)
endif()

###############################################################################
find_package(HPX)

if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    set(HPX_POSTFIX ${HPX_DEBUG_POSTFIX})
endif()

message(${HPX_BUILD_TYPE})

hpx_include(SetOutputPaths)

if(NOT MSVC)
  set(HPX_RPATH ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}${HPX_RPATH})
endif()

include_directories(${HPX_INCLUDE_DIR})
link_directories(${HPX_LIBRARY_DIR})

add_definitions(-DHPX_ACTION_ARGUMENT_LIMIT=6)

###############################################################################
# MPFR
find_package(HPX_Mpfr)

# MPFR depends on GMP, so no point looking for it if we don't have GMP
find_package(HPX_GMP)
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

###############################################################################
# just recurse into all components subdirectories and execute the scripts there
set(subdirs
    amr
    amr_c
    amr_c_test)

find_package(HPX_RNPL)

if(RNPL_FOUND)
  # FIXME: HPX_* identifiers please
  add_definitions(-DRNPL_FOUND)
  include_directories(${RNPL_INCLUDE_DIR})
endif()

###############################################################################
foreach(subdir ${subdirs})
  add_subdirectory(${subdir})
endforeach()

# define build target for this directory
set(sources
    amr_client.cpp)

# define basic dependencies
set(dependencies
    had_amr_c_component
    had_amr_component
    had_amr_c_test_lib)

# this is a hack which needs to be cleaned up (see ticket #468)
if(MSVC)
  set(dependencies
    ${dependencies}
    distributing_factory${HPX_POSTFIX})
else()
  set(dependencies
    ${dependencies}
    hpx_component_distributing_factory${HPX_POSTFIX})
endif()

if(RNPL_FOUND)
  # add RNPL as a dependency
  set(dependencies
      ${dependencies}
      ${RNPL_LIBRARY})
endif()

source_group("Source Files" FILES ${sources})

add_hpx_executable(had_amr_client
  MODULE had_amr
  SOURCES ${sources}
  DEPENDENCIES ${dependencies})

# add a dependency to the pseudo-target
#add_hpx_pseudo_dependencies(examples.had_amr had_amr_client_exe)

