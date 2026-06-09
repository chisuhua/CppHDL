set(BENCHMARK_SOURCES
    perf_main.cpp
    perf_timer.h
    stats_collector.h
    report_generator.h
)

add_executable(perf_tests ${BENCHMARK_SOURCES})

target_include_directories(perf_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/include
)

if(TARGET cpphdl)
    target_link_libraries(perf_tests cpphdl)
endif()

# W9 (perf-report-followup.md): standalone perf_regression executable that
# compares perf_results.json against tests/benchmark/perf_baseline.json
# and exits non-zero on >1.2x regression. Wired into ctest in the root
# tests/CMakeLists.txt.
add_executable(perf_regression perf_regression.cpp)
target_include_directories(perf_regression PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/include
)

# Copy Verilator harness source files into the build directory so that
# verilator_runner can read them via relative paths at runtime. Each harness
# has its own main() and is compiled standalone by `verilator --build --exe`,
# so we deliberately do NOT add them to BENCHMARK_SOURCES (would cause
# multiple-main() link errors). Guarded with if(EXISTS ...) because the
# harness files are created by parallel tasks (Tasks 3/4) that may not have
# landed yet.
set(_harness_tc07_src ${CMAKE_CURRENT_SOURCE_DIR}/verilator_harness_tc07.cpp)
set(_harness_tc07_dst ${CMAKE_CURRENT_BINARY_DIR}/verilator_harness_tc07.cpp)
if(EXISTS ${_harness_tc07_src})
    configure_file(${_harness_tc07_src} ${_harness_tc07_dst} COPYONLY)
endif()

set(_harness_tc08_src ${CMAKE_CURRENT_SOURCE_DIR}/verilator_harness_tc08.cpp)
set(_harness_tc08_dst ${CMAKE_CURRENT_BINARY_DIR}/verilator_harness_tc08.cpp)
if(EXISTS ${_harness_tc08_src})
    configure_file(${_harness_tc08_src} ${_harness_tc08_dst} COPYONLY)
endif()

add_test(NAME perf_tests COMMAND perf_tests --all)
# TIMEOUT raised from 120 to 300 to cover Verilator's first-build overhead
# (~30s for C++ compile + Verilator codegen) on cold caches.
set_tests_properties(perf_tests PROPERTIES
    LABELS "perf"
    TIMEOUT 300
)

target_compile_features(perf_tests PRIVATE cxx_std_20)
