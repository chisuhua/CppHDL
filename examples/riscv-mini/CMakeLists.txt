# RV32I Mini CMakeLists.txt
# ISA 组件验证：RegFile + ALU + Decoder + Branch + Load/Store

add_executable(rv32i_phase1_test rv32i_phase1_test.cpp)
target_link_libraries(rv32i_phase1_test PRIVATE cpphdl)
target_include_directories(rv32i_phase1_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
set_target_properties(rv32i_phase1_test PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)

add_executable(rv32i_phase2_test rv32i_phase2_test.cpp)
target_link_libraries(rv32i_phase2_test PRIVATE cpphdl)
target_include_directories(rv32i_phase2_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
set_target_properties(rv32i_phase2_test PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)

add_executable(rv32i_phase3_test rv32i_phase3_test.cpp)
target_link_libraries(rv32i_phase3_test PRIVATE cpphdl)
target_include_directories(rv32i_phase3_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
set_target_properties(rv32i_phase3_test PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)

# TODO Phase 2: Pipeline 测试需要 API 修复
# ForwardingUnit IO 端口名已变更 (id_ex_rs1_addr → id_rs1_addr 等)
# HazardDetectionUnit 需要 include 路径修复
# add_executable(rv32i_pipeline_test tests/test_rv32i_pipeline.cpp)

install(TARGETS rv32i_phase1_test rv32i_phase2_test rv32i_phase3_test RUNTIME DESTINATION bin/riscv-mini)
