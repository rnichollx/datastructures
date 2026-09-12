cmake_minimum_required(VERSION 3.21)

project(RPNXDataStructures LANGUAGES CXX)


include(GNUInstallDirs)
set(BUILD_TESTING ON CACHE BOOL "Build tests for RPNXDataStructures")
include(CTest)



add_library(rpnx_datastructures INTERFACE
        include/rpnx/hadix_map.hpp
        include/rpnx/geometric_array.hpp
        include/rpnx/segmented_dynar.hpp
        include/rpnx/sharded_unordered_map.hpp
        include/rpnx/iterator.hpp
        include/rpnx/variant.hpp
        include/rpnx/functional.hpp
        include/rpnx/memory.hpp
        include/rpnx/dyn_iterator.hpp
        include/rpnx/annex.hpp
        include/rpnx/set.hpp
        include/rpnx/map.hpp
        include/rpnx/dynar.hpp
)


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


if (BUILD_TESTING)
    find_package(GTest REQUIRED)
    add_executable(rpnx_datastructures_tests
        tests/hadix_map_tests.cpp
        tests/geometric_array_tests.cpp
        tests/segmented_dynar_tests.cpp
            tests/sharded_map_tests.cpp
            tests/iterator_tests.cpp
            tests/variant_tests.cpp
            tests/function_tests.cpp
            tests/result_tests.cpp
            tests/annex_tests.cpp
            tests/set_tests.cpp
            tests/map_tests.cpp
            tests/dynar_tests.cpp
            tests/uint64_base_tests.cpp
            tests/test_utils_tests.cpp
            tests/test_utils.hpp
    )
    target_link_libraries(rpnx_datastructures_tests PRIVATE rpnx_datastructures GTest::gtest_main GTest::gtest)
    gtest_discover_tests(rpnx_datastructures_tests)
    find_package(benchmark REQUIRED)
    add_executable(rpnx_datastructures_benchmarks
        benchmarks/function.cpp
        benchmarks/hadix_map.cpp
        benchmarks/hadix_map.hpp
        benchmarks/geometric_array.cpp
        benchmarks/geometric_array.hpp
    )
    target_link_libraries(rpnx_datastructures_benchmarks PRIVATE rpnx_datastructures benchmark::benchmark benchmark::benchmark_main)
endif()
