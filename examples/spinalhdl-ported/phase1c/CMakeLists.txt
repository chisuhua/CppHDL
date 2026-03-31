# Phase 1C Features Test CMakeLists.txt

add_executable(phase1c_test phase1c_test.cpp)

target_link_libraries(phase1c_test PRIVATE cpphdl)

set_target_properties(phase1c_test PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

install(TARGETS phase1c_test
    RUNTIME DESTINATION bin/examples/spinalhdl-ported
)
