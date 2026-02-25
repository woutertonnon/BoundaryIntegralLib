add_executable(mgconvergence
    MGConvergence.cpp
)
add_executable(femconvergence
    FEMConvergence.cpp
)

target_link_libraries(mgconvergence
    stokesoperator
    stokesdgs
    stokesmg
    Eigen3::Eigen
    mfem
    Boost::program_options
    spectra_mfem
)

target_link_libraries(femconvergence
    stokesoperator
    stokesdgs
    stokesmg
    mfem
    Boost::program_options
)
