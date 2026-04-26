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

add_test(NAME perf_tests COMMAND perf_tests --all)
set_tests_properties(perf_tests PROPERTIES
    LABELS "perf"
    TIMEOUT 120
)

target_compile_features(perf_tests PRIVATE cxx_std_20)