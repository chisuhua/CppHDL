# SPI Controller Example CMakeLists.txt

add_executable(spi_controller_example spi_controller.cpp)

target_link_libraries(spi_controller_example PRIVATE cpphdl)

set_target_properties(spi_controller_example PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

install(TARGETS spi_controller_example
    RUNTIME DESTINATION bin/examples/spinalhdl-ported
)
