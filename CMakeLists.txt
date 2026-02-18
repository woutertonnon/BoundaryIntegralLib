cmake_minimum_required (VERSION 3.20) # Bumped to support C++23
project(MEHCscheme)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS_RELEASE "-O3 -fopenmp -march=native -mtune=native -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG   "-O0 -g")

set(MFEM_USE_SUITESPARSE YES CACHE BOOL "" FORCE)
set(MFEM_USE_LAPACK YES CACHE BOOL "" FORCE)

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})

# --- Dependencies ---

find_package(Eigen3 3.4 REQUIRED)

include_directories(
    "${CMAKE_BINARY_DIR}/extern/mfem"
    "${PROJECT_SOURCE_DIR}/include"
    "/usr/include/suitesparse"
)

# Spectra is header-only, so we just point to the include directory in the submodule
include_directories("${CMAKE_SOURCE_DIR}/extern/spectra/include")
include_directories(${EIGEN3_INCLUDE_DIR})

link_directories("${CMAKE_BINARY_DIR}/src")

enable_testing()

# LTO

include(CheckIPOSupported)
check_ipo_supported(RESULT result OUTPUT output)

if(result)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
else()
    message(STATUS "LTO not supported: ${output}")
endif()

# Note: We do NOT need add_subdirectory(extern/spectra) because it's header-only
# and we don't want to build its tests/examples.
add_subdirectory(extern)
add_subdirectory(src)
add_subdirectory(tests)
