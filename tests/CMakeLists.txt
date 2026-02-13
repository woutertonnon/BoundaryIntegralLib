if(NOT BUILD_TESTING)
  return()
endif()

include(CTest)

# GoogleTest
find_package(GTest QUIET)
if(NOT GTest_FOUND)
  message(WARNING "GoogleTest not found. Tests will be skipped. "
                  "Install GTest (e.g. libgtest-dev) to enable tests.")
  return()
endif()

find_package(Threads REQUIRED)

add_executable(boundaryoperators_tests
  BoundaryOperatorsTests.cpp
)

# If boundaryoperatorslib links MFEM PUBLIC, you don't need to link mfem here.
target_link_libraries(boundaryoperators_tests
  PRIVATE
    boundaryoperatorslib
    Threads::Threads
)

target_link_libraries(boundaryoperators_tests PRIVATE GTest::gtest GTest::gtest_main)


add_test(NAME boundaryoperators_tests COMMAND boundaryoperators_tests)
