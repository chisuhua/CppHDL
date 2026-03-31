# AXI4-Lite SoC Integration Example CMakeLists.txt

add_executable(axi_soc_example axi_soc_example.cpp)

target_link_libraries(axi_soc_example PRIVATE cpphdl)
target_include_directories(axi_soc_example PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../..)

set_target_properties(axi_soc_example PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

install(TARGETS axi_soc_example
    RUNTIME DESTINATION bin/axi4
)
