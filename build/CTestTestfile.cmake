# CMake generated Testfile for 
# Source directory: C:/Users/khali/Code/C-AIR
# Build directory: C:/Users/khali/Code/C-AIR/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(unit_project_tree "C:/Users/khali/Code/C-AIR/build/cair_unit_tests.exe")
set_tests_properties(unit_project_tree PROPERTIES  _CMAKE_TEST_BUILD_DEPENDS "C:/Users/khali/Code/C-AIR/build/cair_unit_tests.exe" _BACKTRACE_TRIPLES "C:/Users/khali/Code/C-AIR/CMakeLists.txt;54;add_test;C:/Users/khali/Code/C-AIR/CMakeLists.txt;0;")
add_test(unit_find_replace "C:/Users/khali/Code/C-AIR/build/cair_search_tests.exe")
set_tests_properties(unit_find_replace PROPERTIES  _CMAKE_TEST_BUILD_DEPENDS "C:/Users/khali/Code/C-AIR/build/cair_search_tests.exe" _BACKTRACE_TRIPLES "C:/Users/khali/Code/C-AIR/CMakeLists.txt;58;add_test;C:/Users/khali/Code/C-AIR/CMakeLists.txt;0;")
add_test(integration_widgets "C:/Users/khali/Code/C-AIR/build/cair_widget_tests.exe")
set_tests_properties(integration_widgets PROPERTIES  _CMAKE_TEST_BUILD_DEPENDS "C:/Users/khali/Code/C-AIR/build/cair_widget_tests.exe" _BACKTRACE_TRIPLES "C:/Users/khali/Code/C-AIR/CMakeLists.txt;63;add_test;C:/Users/khali/Code/C-AIR/CMakeLists.txt;0;")
add_test(e2e_app_startup "C:/Users/khali/Code/C-AIR/build/cair_smoke.exe" "--self-test")
set_tests_properties(e2e_app_startup PROPERTIES  _CMAKE_TEST_BUILD_DEPENDS "C:/Users/khali/Code/C-AIR/build/cair_smoke.exe" _BACKTRACE_TRIPLES "C:/Users/khali/Code/C-AIR/CMakeLists.txt;68;add_test;C:/Users/khali/Code/C-AIR/CMakeLists.txt;0;")
