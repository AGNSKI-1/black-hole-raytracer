cmake_minimum_required(VERSION 3.16)

project(relray VERSION 0.1.0 LANGUAGES CXX)

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

option(RELRAY_BUILD_EXAMPLES "Build example programs" ON)
option(RELRAY_BUILD_TESTS "Build validation tests" ON)

add_library(relray INTERFACE)
add_library(relray::relray ALIAS relray)

target_include_directories(relray INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_compile_features(relray INTERFACE cxx_std_17)

if(MSVC)
    target_compile_options(relray INTERFACE /W4)
else()
    target_compile_options(relray INTERFACE -Wall -Wextra -Wpedantic)
endif()

if(RELRAY_BUILD_EXAMPLES)
    add_executable(relray_render_example examples/render_scene.cpp)
    target_link_libraries(relray_render_example PRIVATE relray::relray)
endif()

if(RELRAY_BUILD_TESTS)
    enable_testing()
    add_executable(relray_tests tests/validation.cpp)
    target_link_libraries(relray_tests PRIVATE relray::relray)
    add_test(NAME relray_validation COMMAND relray_tests)
endif()

install(TARGETS relray EXPORT relrayTargets)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/relrayConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/relrayConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/relray
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/relrayConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(EXPORT relrayTargets
    FILE relrayTargets.cmake
    NAMESPACE relray::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/relray
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/relrayConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/relrayConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/relray
)

export(EXPORT relrayTargets FILE ${CMAKE_CURRENT_BINARY_DIR}/relrayTargets.cmake NAMESPACE relray::)
