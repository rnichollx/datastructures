cmake_minimum_required(VERSION 3.21)

project(RPNXDataStructures LANGUAGES CXX)

# Option: enable testing for this project (defaults to ON when top-level, OFF when used as a subproject)
if (PROJECT_IS_TOP_LEVEL)
  option(RPNX_DS_BUILD_TESTS "Enable testing for RPNX::DataStructures" ON)
else()
  option(RPNX_DS_BUILD_TESTS "Enable testing for RPNX::DataStructures" OFF)
endif()


include(GNUInstallDirs)

add_library(rpnx_datastructures INTERFACE
        include/rpnx/segmented_dynar.hpp
)

add_library(RPNX::datastructures ALIAS rpnx_datastructures)

target_include_directories(rpnx_datastructures
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

set_target_properties(rpnx_datastructures PROPERTIES EXPORT_NAME datastructures)

install(
    DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
    TARGETS rpnx_datastructures
    EXPORT rpnx_datastructures_exports
)

export(
    EXPORT rpnx_datastructures_exports
    FILE RPNXDataStructuresConfig.cmake
    NAMESPACE RPNX::
)

install(
    EXPORT rpnx_datastructures_exports
    FILE RPNXDataStructuresConfig.cmake
    NAMESPACE RPNX::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/RPNXDataStructures
)


if (RPNX_DS_BUILD_TESTS)
    find_package(GTest REQUIRED)
    enable_testing()
    add_executable(rpnx_datastructures_tests
        tests/segmented_dynar_tests.cpp
    )
    target_link_libraries(rpnx_datastructures_tests PRIVATE RPNX::datastructures GTest::gtest_main GTest::gtest)
    gtest_discover_tests(rpnx_datastructures_tests)
endif ()
