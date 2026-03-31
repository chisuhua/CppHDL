# GPIO Example CMakeLists.txt

add_executable(gpio_example gpio.cpp)

target_link_libraries(gpio_example PRIVATE cpphdl)

set_target_properties(gpio_example PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

install(TARGETS gpio_example
    RUNTIME DESTINATION bin/examples/spinalhdl-ported
)
