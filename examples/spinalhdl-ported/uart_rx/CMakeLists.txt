# UART RX Example CMakeLists.txt

add_executable(uart_rx_example uart_rx.cpp)

target_link_libraries(uart_rx_example PRIVATE cpphdl)

set_target_properties(uart_rx_example PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

install(TARGETS uart_rx_example
    RUNTIME DESTINATION bin/examples/spinalhdl-ported
)
