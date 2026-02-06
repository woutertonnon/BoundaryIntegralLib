cmake_minimum_required (VERSION 2.9)
project(MEHCscheme)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS_RELEASE "-O3 -fopenmp -march=native -mtune=native -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG   "-O0 -g")
set(MFEM_USE_SUITESPARSE YES CACHE BOOL "" FORCE)
set(MFEM_USE_LAPACK YES CACHE BOOL "" FORCE)

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})
#find_package(Git QUIET)
#FIND_PACKAGE(Boost COMPONENTS program_options filesystem REQUIRED)
#INCLUDE_DIRECTORIES(${Boost_INCLUDE_DIRS})
#find_package(MFEM REQUIRED)
# Use the bundled extern/mfem headers/build instead of an external ../mfem/build
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/extern/mfem" "${PROJECT_SOURCE_DIR}/include" "/usr/include/suitesparse")
link_directories("${CMAKE_BINARY_DIR}/src")


# Force MFEM to build with MUMPS (and MPI) when configuring the bundled extern/mfem
# These are set as CACHE variables with FORCE so they propagate into the subproject.
#set(MFEM_USE_MPI ON CACHE BOOL "Force MFEM to enable MPI (required for MUMPS)" FORCE)
#set(MFEM_USE_MUMPS ON CACHE BOOL "Force MFEM to enable MUMPS" FORCE)

enable_testing()
add_subdirectory(extern)
add_subdirectory(src)
add_subdirectory(tests)

