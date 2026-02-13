cmake_minimum_required(VERSION 3.20)

project(boundaryoperatorslib VERSION 0.1.0 LANGUAGES C CXX)

# Standard testing option (BUILD_TESTING) + enable_testing() when ON
include(CTest)

# Put executables in the build dir root (matches your prior intent)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

add_library(boundaryoperators_project_options INTERFACE)

# Your public headers
target_include_directories(boundaryoperators_project_options
  INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_compile_features(boundaryoperators_project_options INTERFACE cxx_std_17) # Same as MFEM
target_link_libraries(boundaryoperators_project_options )

add_subdirectory(extern)
add_subdirectory(src)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
