# --- Base Libraries ---
add_library(incidence incidence.cpp)
target_link_libraries(incidence PUBLIC mfem)

add_library(bd_separator BoundarySeparator.cpp)
target_link_libraries(bd_separator PUBLIC mfem)

add_library(spectra_mfem SpectraErrorOp.cpp)
target_link_libraries(spectra_mfem PUBLIC mfem Eigen3::Eigen)

add_library(boundaryoperatorslib BoundaryOperators.cpp)
target_link_libraries(boundaryoperatorslib PUBLIC mfem)

add_library(stokesoperator StokesOperator.cpp)
target_link_libraries(stokesoperator PUBLIC boundaryoperatorslib incidence mfem)

add_library(stokesdgs StokesDGS.cpp)
target_link_libraries(stokesdgs PUBLIC stokesoperator mfem)

add_library(stokesmg StokesMG.cpp)
target_link_libraries(stokesmg PUBLIC stokesdgs mfem)
