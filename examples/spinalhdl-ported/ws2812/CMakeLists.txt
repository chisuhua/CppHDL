# WS2812 Controller Example CMakeLists.txt

add_executable(ws2812_controller_example ws2812_controller.cpp)

target_link_libraries(ws2812_controller_example PRIVATE cpphdl)

set_target_properties(ws2812_controller_example PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

install(TARGETS ws2812_controller_example
    RUNTIME DESTINATION bin/examples/spinalhdl-ported
)
