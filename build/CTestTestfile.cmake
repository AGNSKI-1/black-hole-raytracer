# CMake generated Testfile for 
# Source directory: C:/Users/adamg/OneDrive/Desktop/U of A/569 - HPC/black-hole-raytracer
# Build directory: C:/Users/adamg/OneDrive/Desktop/U of A/569 - HPC/black-hole-raytracer/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
if(CTEST_CONFIGURATION_TYPE MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
  add_test(relray_validation "C:/Users/adamg/OneDrive/Desktop/U of A/569 - HPC/black-hole-raytracer/build/Debug/relray_tests.exe")
  set_tests_properties(relray_validation PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/adamg/OneDrive/Desktop/U of A/569 - HPC/black-hole-raytracer/CMakeLists.txt;36;add_test;C:/Users/adamg/OneDrive/Desktop/U of A/569 - HPC/black-hole-raytracer/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
  add_test(relray_validation "C:/Users/adamg/OneDrive/Desktop/U of A/569 - HPC/black-hole-raytracer/build/Release/relray_tests.exe")
  set_tests_properties(relray_validation PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/adamg/OneDrive/Desktop/U of A/569 - HPC/black-hole-raytracer/CMakeLists.txt;36;add_test;C:/Users/adamg/OneDrive/Desktop/U of A/569 - HPC/black-hole-raytracer/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
  add_test(relray_validation "C:/Users/adamg/OneDrive/Desktop/U of A/569 - HPC/black-hole-raytracer/build/MinSizeRel/relray_tests.exe")
  set_tests_properties(relray_validation PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/adamg/OneDrive/Desktop/U of A/569 - HPC/black-hole-raytracer/CMakeLists.txt;36;add_test;C:/Users/adamg/OneDrive/Desktop/U of A/569 - HPC/black-hole-raytracer/CMakeLists.txt;0;")
elseif(CTEST_CONFIGURATION_TYPE MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
  add_test(relray_validation "C:/Users/adamg/OneDrive/Desktop/U of A/569 - HPC/black-hole-raytracer/build/RelWithDebInfo/relray_tests.exe")
  set_tests_properties(relray_validation PROPERTIES  _BACKTRACE_TRIPLES "C:/Users/adamg/OneDrive/Desktop/U of A/569 - HPC/black-hole-raytracer/CMakeLists.txt;36;add_test;C:/Users/adamg/OneDrive/Desktop/U of A/569 - HPC/black-hole-raytracer/CMakeLists.txt;0;")
else()
  add_test(relray_validation NOT_AVAILABLE)
endif()
