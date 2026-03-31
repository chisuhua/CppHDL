# AXI4-Lite Examples CMakeLists.txt

# AXI4-Lite Example (时序验证)
add_executable(axi4_lite_example axi4_lite_example.cpp)
target_link_libraries(axi4_lite_example PRIVATE cpphdl)
target_include_directories(axi4_lite_example PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set_target_properties(axi4_lite_example PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)

# AXI4-Lite SoC Integration Example
add_executable(axi_soc_example axi_soc_example.cpp)
target_link_libraries(axi_soc_example PRIVATE cpphdl)
target_include_directories(axi_soc_example PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set_target_properties(axi_soc_example PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)

install(TARGETS axi4_lite_example axi_soc_example RUNTIME DESTINATION bin/axi4)
