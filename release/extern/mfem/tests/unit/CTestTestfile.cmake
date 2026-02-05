# CMake generated Testfile for 
# Source directory: /home/me/Documents/BoundaryIntegralLib/extern/mfem/tests/unit
# Build directory: /home/me/Documents/BoundaryIntegralLib/release/extern/mfem/tests/unit
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[unit_tests]=] "/home/me/Documents/BoundaryIntegralLib/release/unit_tests")
set_tests_properties([=[unit_tests]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/me/Documents/BoundaryIntegralLib/extern/mfem/tests/unit/CMakeLists.txt;206;add_test;/home/me/Documents/BoundaryIntegralLib/extern/mfem/tests/unit/CMakeLists.txt;0;")
add_test([=[sedov_tests_cpu]=] "/home/me/Documents/BoundaryIntegralLib/release/sedov_tests_cpu")
set_tests_properties([=[sedov_tests_cpu]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/me/Documents/BoundaryIntegralLib/extern/mfem/tests/unit/CMakeLists.txt;266;add_test;/home/me/Documents/BoundaryIntegralLib/extern/mfem/tests/unit/CMakeLists.txt;306;add_serial_miniapp_test;/home/me/Documents/BoundaryIntegralLib/extern/mfem/tests/unit/CMakeLists.txt;0;")
add_test([=[sedov_tests_debug]=] "/home/me/Documents/BoundaryIntegralLib/release/sedov_tests_debug")
set_tests_properties([=[sedov_tests_debug]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/me/Documents/BoundaryIntegralLib/extern/mfem/tests/unit/CMakeLists.txt;276;add_test;/home/me/Documents/BoundaryIntegralLib/extern/mfem/tests/unit/CMakeLists.txt;306;add_serial_miniapp_test;/home/me/Documents/BoundaryIntegralLib/extern/mfem/tests/unit/CMakeLists.txt;0;")
add_test([=[tmop_pa_tests_cpu]=] "/home/me/Documents/BoundaryIntegralLib/release/tmop_pa_tests_cpu")
set_tests_properties([=[tmop_pa_tests_cpu]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/me/Documents/BoundaryIntegralLib/extern/mfem/tests/unit/CMakeLists.txt;266;add_test;/home/me/Documents/BoundaryIntegralLib/extern/mfem/tests/unit/CMakeLists.txt;307;add_serial_miniapp_test;/home/me/Documents/BoundaryIntegralLib/extern/mfem/tests/unit/CMakeLists.txt;0;")
add_test([=[tmop_pa_tests_debug]=] "/home/me/Documents/BoundaryIntegralLib/release/tmop_pa_tests_debug")
set_tests_properties([=[tmop_pa_tests_debug]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/me/Documents/BoundaryIntegralLib/extern/mfem/tests/unit/CMakeLists.txt;276;add_test;/home/me/Documents/BoundaryIntegralLib/extern/mfem/tests/unit/CMakeLists.txt;307;add_serial_miniapp_test;/home/me/Documents/BoundaryIntegralLib/extern/mfem/tests/unit/CMakeLists.txt;0;")
add_test([=[debug_device_tests]=] "/home/me/Documents/BoundaryIntegralLib/release/debug_device_tests")
set_tests_properties([=[debug_device_tests]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/me/Documents/BoundaryIntegralLib/extern/mfem/tests/unit/CMakeLists.txt;480;add_test;/home/me/Documents/BoundaryIntegralLib/extern/mfem/tests/unit/CMakeLists.txt;0;")
