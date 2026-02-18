find_package(GTest QUIET)
if (NOT GTEST_FOUND)
    message(WARNING "GoogleTest not found. Install libgtest-dev (and build the libs) to enable the test target.")
    return()
endif()

include_directories(${GTEST_INCLUDE_DIRS})

add_executable(incidence_tests
    IncidenceTest.cpp
)
add_executable(bd_separator_tests
    BoundarySeparatorTest.cpp
)
add_executable(spectraop_tests
    SpectraOpTest.cpp
)
add_executable(boundaryoperators_tests
    BoundaryOperatorsTests.cpp
)
add_executable(stokesoperator_tests
    StokesOperatorTest.cpp
)
add_executable(stokesdgs_tests
    StokesDGSTest.cpp
)
add_executable(stokesmg_tests
    StokesMGTest.cpp
)


target_link_libraries(incidence_tests
    PRIVATE
        incidence
        mfem
        # Prefer imported targets when available (CMake >= 3.5)
        $<$<TARGET_EXISTS:GTest::gtest>:GTest::gtest>
        $<$<TARGET_EXISTS:GTest::gtest_main>:GTest::gtest_main>
        # Fallback for older FindGTest modules
        ${GTEST_LIBRARIES}
        ${GTEST_MAIN_LIBRARIES}
        pthread
)

target_link_libraries(bd_separator_tests
    PRIVATE
        bd_separator
        mfem
        # Prefer imported targets when available (CMake >= 3.5)
        $<$<TARGET_EXISTS:GTest::gtest>:GTest::gtest>
        $<$<TARGET_EXISTS:GTest::gtest_main>:GTest::gtest_main>
        # Fallback for older FindGTest modules
        ${GTEST_LIBRARIES}
        ${GTEST_MAIN_LIBRARIES}
        pthread
)

target_link_libraries(spectraop_tests
    PRIVATE
        spectra_mfem
        mfem
        Eigen3::Eigen
        # Prefer imported targets when available (CMake >= 3.5)
        $<$<TARGET_EXISTS:GTest::gtest>:GTest::gtest>
        $<$<TARGET_EXISTS:GTest::gtest_main>:GTest::gtest_main>
        # Fallback for older FindGTest modules
        ${GTEST_LIBRARIES}
        ${GTEST_MAIN_LIBRARIES}
        pthread
)

target_link_libraries(boundaryoperators_tests
    PRIVATE
        boundaryoperatorslib
        mfem
        # Prefer imported targets when available (CMake >= 3.5)
        $<$<TARGET_EXISTS:GTest::gtest>:GTest::gtest>
        $<$<TARGET_EXISTS:GTest::gtest_main>:GTest::gtest_main>
        # Fallback for older FindGTest modules
        ${GTEST_LIBRARIES}
        ${GTEST_MAIN_LIBRARIES}
        pthread
)

target_link_libraries(stokesoperator_tests
    PRIVATE
        boundaryoperatorslib
        mfem
        incidence
        stokesoperator
        # Prefer imported targets when available (CMake >= 3.5)
        $<$<TARGET_EXISTS:GTest::gtest>:GTest::gtest>
        $<$<TARGET_EXISTS:GTest::gtest_main>:GTest::gtest_main>
        # Fallback for older FindGTest modules
        ${GTEST_LIBRARIES}
        ${GTEST_MAIN_LIBRARIES}
        pthread
)

target_link_libraries(stokesdgs_tests
    PRIVATE
        mfem
        stokesdgs
        # Prefer imported targets when available (CMake >= 3.5)
        $<$<TARGET_EXISTS:GTest::gtest>:GTest::gtest>
        $<$<TARGET_EXISTS:GTest::gtest_main>:GTest::gtest_main>
        # Fallback for older FindGTest modules
        ${GTEST_LIBRARIES}
        ${GTEST_MAIN_LIBRARIES}
        pthread
)

target_link_libraries(stokesmg_tests
    PRIVATE
        stokesoperator
        stokesdgs
        stokesmg
        mfem
        # Prefer imported targets when available (CMake >= 3.5)
        $<$<TARGET_EXISTS:GTest::gtest>:GTest::gtest>
        $<$<TARGET_EXISTS:GTest::gtest_main>:GTest::gtest_main>
        # Fallback for older FindGTest modules
        ${GTEST_LIBRARIES}
        ${GTEST_MAIN_LIBRARIES}
        pthread
)

add_test(NAME incidence_tests COMMAND incidence_tests)
add_test(NAME boundaryoperators_tests COMMAND boundaryoperators_tests)
add_test(NAME spectraop_tests COMMAND spectraop_tests)
add_test(NAME stokesoperator_tests COMMAND stokesoperator_tests)
add_test(NAME stokesdgs_tests COMMAND stokesdgs_tests)
add_test(NAME stokesmg_tests COMMAND stokesmg_tests)
