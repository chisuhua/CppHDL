# VGA Controller Example CMakeLists.txt

add_executable(vga_controller_example vga_controller.cpp)

target_link_libraries(vga_controller_example PRIVATE cpphdl)

set_target_properties(vga_controller_example PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

install(TARGETS vga_controller_example
    RUNTIME DESTINATION bin/examples/spinalhdl-ported
)
