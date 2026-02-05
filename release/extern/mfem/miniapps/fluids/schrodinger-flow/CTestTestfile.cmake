# CMake generated Testfile for 
# Source directory: /home/me/Documents/BoundaryIntegralLib/extern/mfem/miniapps/fluids/schrodinger-flow
# Build directory: /home/me/Documents/BoundaryIntegralLib/release/extern/mfem/miniapps/fluids/schrodinger-flow
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[schrodinger_flow_ser]=] "/home/me/Documents/BoundaryIntegralLib/release/schrodinger_flow" "-lf" "-nx" "8" "-ny" "8" "-ms" "32" "-no-vis")
set_tests_properties([=[schrodinger_flow_ser]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/me/Documents/BoundaryIntegralLib/extern/mfem/miniapps/fluids/schrodinger-flow/CMakeLists.txt;34;add_test;/home/me/Documents/BoundaryIntegralLib/extern/mfem/miniapps/fluids/schrodinger-flow/CMakeLists.txt;0;")
