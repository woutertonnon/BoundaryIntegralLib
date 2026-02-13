# Make an automatic library - will be static or dynamic based on user setting
add_library(incidence incidence.cpp)
add_library(bd_separator BoundarySeparator.cpp)
add_library(boundaryoperatorslib BoundaryOperators.cpp "../include/BoundaryOperators.hpp")
add_library(stokesoperator StokesOperator.cpp)
add_library(stokesdgs StokesDGS.cpp)
add_library(stokesmg StokesMG.cpp)

# We need this directory, and users of our library will need it too
target_include_directories(boundaryoperatorslib PUBLIC ../include ${CMAKE_BINARY_DIR}/extern/mfem)
target_link_directories(boundaryoperatorslib PRIVATE ${CMAKE_BINARY_DIR}/extern/mfem)

target_link_libraries(incidence PRIVATE mfem)
target_link_libraries(bd_separator PRIVATE mfem)
target_link_libraries(stokesoperator PRIVATE incidence boundaryoperatorslib mfem)
target_link_libraries(stokesdgs PRIVATE stokesoperator mfem)
target_link_libraries(stokesmg PRIVATE stokesoperator stokesdgs mfem)
