# I2C Controller Example CMakeLists.txt

add_executable(i2c_controller_example i2c_controller.cpp)

target_link_libraries(i2c_controller_example PRIVATE cpphdl)

set_target_properties(i2c_controller_example PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

install(TARGETS i2c_controller_example
    RUNTIME DESTINATION bin/examples/spinalhdl-ported
)
