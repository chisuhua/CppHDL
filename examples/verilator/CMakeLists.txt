# examples/verilator/CMakeLists.txt
# Verilator 仿真后端端到端示例
# ADR-035 Phase 4.4 配套代码

add_executable(verilator_01_minimal 01_minimal_counter.cpp)
target_link_libraries(verilator_01_minimal PRIVATE cpphdl)
set_target_properties(verilator_01_minimal PROPERTIES
    CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
install(TARGETS verilator_01_minimal RUNTIME DESTINATION bin/examples/verilator)

add_executable(verilator_02_counter 02_counter_with_input.cpp)
target_link_libraries(verilator_02_counter PRIVATE cpphdl)
set_target_properties(verilator_02_counter PROPERTIES
    CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
install(TARGETS verilator_02_counter RUNTIME DESTINATION bin/examples/verilator)

add_executable(verilator_03_component 03_component_style.cpp)
target_link_libraries(verilator_03_component PRIVATE cpphdl)
set_target_properties(verilator_03_component PROPERTIES
    CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
install(TARGETS verilator_03_component RUNTIME DESTINATION bin/examples/verilator)
