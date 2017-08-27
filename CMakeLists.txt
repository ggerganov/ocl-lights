cmake_minimum_required (VERSION 2.8)
project (ocl-lights)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RELEASE)
endif()

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -W -Wall -Wno-long-long -pedantic")

#
## Dependencies
find_package(Threads REQUIRED)
find_package(OpenCL REQUIRED)
include_directories(SYSTEM ${OPENCL_INCLUDE_DIRS})

#
## Logging parameters
if (CG_LOGGING) # Verbosity
    add_definitions(-DCG_LOGGING=${CG_LOGGING})
else (CG_LOGGING)
    add_definitions(-DCG_LOGGING=10)
endif (CG_LOGGING)

option(CG_OUTPUT "Use cg-output" ON)
if (CG_OUTPUT)
    add_definitions(-DCG_OUTPUT -DCOLOR_OUTPUT)
endif()

add_definitions(-DCG_COLOR_OUTPUT) # Colored log messages

## Libs
set(CG_CORE_LIB cg_core)
set(CG_OPENCL_LIB cg_opencl)
set(CG_IMGUI_LIB ImGui)

include_directories(src)
include_directories(external)

add_subdirectory(src)
add_subdirectory(external)
add_subdirectory(main)
