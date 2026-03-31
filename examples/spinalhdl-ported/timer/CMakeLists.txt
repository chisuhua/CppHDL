# Timer Example CMakeLists.txt

add_executable(timer_example timer.cpp)

target_link_libraries(timer_example PRIVATE cpphdl)

set_target_properties(timer_example PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

install(TARGETS timer_example
    RUNTIME DESTINATION bin/examples/spinalhdl-ported
)
