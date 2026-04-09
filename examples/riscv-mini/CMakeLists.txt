# RV32I Mini CMakeLists.txt
# 注意：以下示例可能有编译错误（旧代码），仅用于参考

# # RV32I Minimal System - disabled due to API changes
# add_executable(rv32i_minimal rv32i_minimal.cpp)
# target_link_libraries(rv32i_minimal PRIVATE cpphdl)
# target_include_directories(rv32i_minimal PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
# set_target_properties(rv32i_minimal PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)

# # Phase 3B Minimal Test - disabled due to API changes
# add_executable(phase3b_minimal phase3b_minimal.cpp)
# target_link_libraries(phase3b_minimal PRIVATE cpphdl)
# target_include_directories(phase3b_minimal PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
# set_target_properties(phase3b_minimal PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)

# RV32I Phase 1 Test (RegFile + ALU + Decoder)
add_executable(rv32i_phase1_test rv32i_phase1_test.cpp)
target_link_libraries(rv32i_phase1_test PRIVATE cpphdl)
target_include_directories(rv32i_phase1_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
set_target_properties(rv32i_phase1_test PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)

# RV32I Phase 2 Test (Branch & Jump Instructions)
add_executable(rv32i_phase2_test rv32i_phase2_test.cpp)
target_link_libraries(rv32i_phase2_test PRIVATE cpphdl)
target_include_directories(rv32i_phase2_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
set_target_properties(rv32i_phase2_test PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)

# RV32I Phase 3 Test (Load/Store Instructions)
add_executable(rv32i_phase3_test rv32i_phase3_test.cpp)
target_link_libraries(rv32i_phase3_test PRIVATE cpphdl)
target_include_directories(rv32i_phase3_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
set_target_properties(rv32i_phase3_test PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)

install(TARGETS rv32i_phase1_test rv32i_phase2_test rv32i_phase3_test RUNTIME DESTINATION bin/riscv-mini)
