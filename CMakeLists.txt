# Copyright (c) 2007-2012 Hartmut Kaiser
# Copyright (c) 2012      Thomas Heller
# Copyright (c) 2009-2010 Matt Anderson
# Copyright (c) 2011      Bryce Lelbach
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

project(had_amr CXX)

cmake_minimum_required(VERSION 2.8)

###############################################################################
# enable solution folders for MSVC
if(MSVC)
  set_property(GLOBAL PROPERTY USE_FOLDERS ON)
endif()

###############################################################################
# Find and include HPX settings
if(NOT HPX_ROOT)
    if(NOT $ENV{HPX_ROOT} STREQUAL "")
        set(HPX_ROOT $ENV{HPX_ROOT})
    endif()
endif()
if(NOT HPX_ROOT)
    hpx_error("HPX_ROOT not set, unable to find hpx!")
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${HPX_ROOT}/share/cmake-2.8/Modules)

find_package(HPX)

if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    set(HPX_POSTFIX ${HPX_DEBUG_POSTFIX})
endif()

include_directories(${HPX_INCLUDE_DIR})
link_directories(${HPX_LIBRARY_DIR})

###############################################################################
if(NOT MSVC)
  set(HPX_RPATH ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}${HPX_RPATH})
endif()

###############################################################################
# Handle MPFR, GMP, et.al.
set(HAD_AMR_USE_MPET ON CACHE BOOL "Using MP Expression template library")
set(HAD_AMR_USE_MPFR ON CACHE BOOL "Using MPFR library")

if(HAD_AMR_USE_MPFR)
    # MPFR depends on GMP, so no point looking for it if we don't have GMP
    find_package(HPX_GMP)
    if(GMP_FOUND)
        # add GMP as a dependency
        set(dependencies
            ${dependencies}
            ${GMP_LIBRARY})
        include_directories(${GMP_INCLUDE_DIR})

        find_package(HPX_MPFR)
        if(MPFR_FOUND)
            if(HAD_AMR_USE_MPET)
                find_package(HPX_MP)

                if(MP_FOUND)
                    # add MP as a dependency
                    set(dependencies
                        ${dependencies}
                        ${MP_LIBRARY})
                    include_directories(${MP_INCLUDE_DIR})
                    add_definitions(-DHAD_AMR_USE_MPET=1)
                endif()
            endif()

            # add MPFR as a dependency
            set(dependencies
                ${dependencies}
                ${MPFR_LIBRARY})
            include_directories(${MPFR_INCLUDE_DIR})

            # FIXME: HPX_* identifiers please
            add_definitions(-DMPFR_FOUND=1)
            #add_definitions(-DMPFR_USE_NED_ALLOCATOR=1)

            # allow the mpreal class to take advantage of move semantics
            # FIXME: HPX_* identifiers please
            add_definitions(-DMPFR_USE_BOOST_MOVE=1)
        endif()
    endif()
endif()

###############################################################################
# Handle RNPL library
find_package(HPX_RNPL)
if(RNPL_FOUND)
    # add RNPL as a dependency
    set(dependencies
        ${dependencies}
        ${RNPL_LIBRARY})
    include_directories(${RNPL_INCLUDE_DIR})

    # FIXME: HPX_* identifiers please
    add_definitions(-DRNPL_FOUND)
endif()

###############################################################################
# HAD_AMR uses more than 4 arguments in actions
add_definitions(-DHPX_ACTION_ARGUMENT_LIMIT=7)
add_definitions(-DHPX_FUNCTION_LIMIT=10)

###############################################################################
# just recurse into all components subdirectories and execute the scripts there
set(subdirs
    amr
    amr_c
    amr_c_test)

foreach(subdir ${subdirs})
    add_subdirectory(${subdir})
endforeach()

###############################################################################
# define build target for this directory
set(sources
    amr_client.cpp)

# define basic dependencies
set(dependencies
    ${dependencies}
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

source_group("Source Files" FILES ${sources})

add_hpx_executable(had_amr_client
    MODULE had_amr
    SOURCES ${sources}
    DEPENDENCIES ${dependencies}
    FOLDER "Had_Amr")
