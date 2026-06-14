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
# Link against cpphdl (matching perf_tests above) so the target inherits
# the project's CMAKE_CXX_STANDARD + libc++/libstdc++ link flags. Without
# this, macOS linker fails with:
#   "std::length_error::length_error(char const*)", referenced from
#   perf_regression.cpp.o
# even when <stdexcept> is included (compile succeeds, link fails).
if(TARGET cpphdl)
    target_link_libraries(perf_regression cpphdl)
endif()

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

# When BUILD_VERILATOR=ON, CPPHDL_VERILATOR_WRAPPER is the install-tree
# path. Inject it as --verilator= so perf_main.cpp's VerilatorRunner finds
# the real binary instead of looking it up on PATH (which would fail).
# Also inject VERILATOR_ROOT explicitly as belt-and-suspenders: the
# upstream Perl wrapper sets it in its own %ENV before system() on
# verilator_bin, but an explicit injection survives any future
# refactor that bypasses the wrapper.
#
# When BUILD_VERILATOR=OFF, CPPHDL_VERILATOR_WRAPPER is empty and we
# fall back to the original behavior (perf_main.cpp's default of
# 'verilator' on PATH, which will correctly produce UNSUPPORTED rows).
#
# The perf_tests binary runs multiple internal benchmarks, some of
# which exercise the JIT path. The JIT is enabled by default in
# CMakeLists.txt, so the test always runs in CI. The 300s timeout
# guards against hangs on malformed DUTs.
if(CPPHDL_VERILATOR_WRAPPER)
    add_test(NAME perf_tests COMMAND perf_tests --all
        --verilator=${CPPHDL_VERILATOR_WRAPPER}
        --report=json --report=csv --report=md)
    set_tests_properties(perf_tests PROPERTIES
        LABELS "perf"
        TIMEOUT 900
        ENVIRONMENT "VERILATOR_ROOT=${CPPHDL_VERILATOR_DIR}"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
else()
    add_test(NAME perf_tests COMMAND perf_tests --all
        --report=json --report=csv --report=md)
    set_tests_properties(perf_tests PROPERTIES
        LABELS "perf"
        TIMEOUT 900
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
endif()

target_compile_features(perf_tests PRIVATE cxx_std_20)
