# Make an automatic library - will be static or dynamic based on user setting
add_library(boundaryoperatorslib BoundaryOperators.cpp "../include/BoundaryOperators.hpp")
add_library(incidence incidence.cpp)
add_library(stokesoperator StokesOperator.cpp)
add_library(stokesdgs StokesDGS.cpp)

# We need this directory, and users of our library will need it too
target_include_directories(boundaryoperatorslib PUBLIC ../include ${CMAKE_CURRENT_SOURCE_DIR}/extern/mfem)
target_link_directories(boundaryoperatorslib PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/extern/mfem)

target_link_libraries(incidence PRIVATE mfem)
target_link_libraries(stokesoperator PRIVATE mfem incidence boundaryoperatorslib)
target_link_libraries(stokesdgs PRIVATE mfem stokesoperator)
