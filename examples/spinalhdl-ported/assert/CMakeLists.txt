# Assert Example CMakeLists.txt

add_executable(assert_example assert_example.cpp)

target_link_libraries(assert_example PRIVATE cpphdl)

set_target_properties(assert_example PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

install(TARGETS assert_example
    RUNTIME DESTINATION bin/examples/spinalhdl-ported
)
