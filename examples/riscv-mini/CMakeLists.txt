# RV32I Mini CMakeLists.txt
# ISA 组件验证:RegFile + ALU + Decoder + Branch + Load/Store
#
# 两个 helper 函数消除 14 个 Catch2 + 1 个 noctest + 3 个 unit 目标的样板代码,
# 模式对齐 tests/CMakeLists.txt 的 add_catch_test()。

# ISA 单元测试(自带 main(),无 Catch2)
function(riscv_mini_unit_test TEST_NAME TEST_SOURCE)
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    target_link_libraries(${TEST_NAME} PRIVATE cpphdl)
    target_include_directories(${TEST_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include/cpu/riscv)
    set_target_properties(${TEST_NAME} PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON)
endfunction()

# Catch2 集成测试(链接 catch_amalgamated)
# 重要: include 顺序敏感 — include/cpu/pipeline 必须在 src 之前,
# 否则 #include "rv32i_decoder.h" 优先匹配到 examples/riscv-mini/src/rv32i_decoder.h,
# 之后 include/cpu/pipeline/rv32i_decoder.h 被 include 时触发 struct 重定义。
# include/cpu/pipeline 对 4 个非-pipeline 测试是无害的多余路径。
# 实现细节：真正的 target 构造在 `_riscv_mini_catch_test_impl` 里，
# 由两个 wrapper 调用：
#   riscv_mini_catch_test       —— 进 CTest（add_test）
#   riscv_mini_catch_test_noctest —— **不进** CTest（仅构建）
# 拆分的原因见 `riscv_mini_catch_test_noctest` 的定义处。
function(_riscv_mini_catch_test_impl TEST_NAME TEST_SOURCE DO_ADD_TEST)
    add_executable(${TEST_NAME} ${TEST_SOURCE}
        ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/catch_amalgamated.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../../tests/catch_amalgamated.hpp)
    target_link_libraries(${TEST_NAME} PRIVATE cpphdl)
    target_include_directories(${TEST_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include/cpu/riscv
        ${CMAKE_CURRENT_SOURCE_DIR}/../../tests
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include/cpu/pipeline
        ${CMAKE_CURRENT_SOURCE_DIR}/src)
    set_target_properties(${TEST_NAME} PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON)
    # ⚠️ 2026-10-23 (change `riscv-mini-test-triage-2026-10`): 本函数注册的
    # Catch2 target 此前**从未被 ctest 执行过** (`grep -rn "add_test" examples/`
    # 恒为 0) => 它们每次 CI 都在编译、却零运行覆盖。
    # => 在 helper 内统一补 `add_test`（一次覆盖全部 target，不逐个重复写）。
    # 护栏: `scripts/verify_example_test_wiring.sh`（C1 归属守恒 + C2 数量守恒）。
    if (DO_ADD_TEST)
        add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
        set_tests_properties(${TEST_NAME} PROPERTIES LABELS "base" TIMEOUT 300)
    endif()
endfunction()

# 进 CTest 的 Catch2 测试 target。
function(riscv_mini_catch_test TEST_NAME TEST_SOURCE)
    _riscv_mini_catch_test_impl(${TEST_NAME} ${TEST_SOURCE} TRUE)
endfunction()

# **不进** CTest 的 Catch2 测试 target（只构建）。
#
# ⚠️ 2026-10-23（change `riscv-mini-test-triage-2026-10`）：当一个 Catch2
# 二进制的**全部**用例都因「仓外资源缺失」而被 SKIP 时，Catch2 v3 的退出码是
# **4**（= 没有真正跑起来的用例）⇒ **CTest 会把它判为 FAILED**。
# `test_riscv_tests_pipeline` 的 46 个用例全部依赖仓外 riscv-tests 的预编译
# 二进制 ⇒ 在 CI（无该 checkout）里恒为 exit 4 ⇒ 不能进 CTest。
# ⇒ 它只构建、不注册；归位条件见 `examples/disabled_examples.txt`。
function(riscv_mini_catch_test_noctest TEST_NAME TEST_SOURCE)
    _riscv_mini_catch_test_impl(${TEST_NAME} ${TEST_SOURCE} FALSE)
endfunction()

# === ISA 组件验证 (Phase 1-3) ===
riscv_mini_unit_test(rv32i_phase1_test rv32i_phase1_test.cpp)
riscv_mini_unit_test(rv32i_phase2_test rv32i_phase2_test.cpp)
riscv_mini_unit_test(rv32i_phase3_test rv32i_phase3_test.cpp)

# === Pipeline 集成测试 (Catch2) ===
riscv_mini_catch_test(test_pipeline_exec tests/test_pipeline_exec.cpp)
riscv_mini_catch_test(test_soc_hello_world tests/test_soc_hello_world.cpp)
riscv_mini_catch_test(test_riscv_tests tests/test_riscv_tests.cpp)
riscv_mini_catch_test(test_address_decoder tests/test_riscv_tests_soc.cpp)

# === Pipeline 单元 ===
# 异常: test_riscv_tests_pipeline 额外需要 examples/riscv-mini/tests/ 在 include 路径
# ⚠️ 不进 CTest：46 个用例全部依赖仓外 riscv-tests 二进制 ⇒ CI 中恒为
#    Catch2 exit 4（全部 SKIP）⇒ CTest 会判红。归位条件见 disabled 清单。
riscv_mini_catch_test_noctest(test_riscv_tests_pipeline tests/test_riscv_tests_pipeline.cpp)
target_include_directories(test_riscv_tests_pipeline PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)

riscv_mini_catch_test(test_pipeline_minimal tests/test_pipeline_minimal.cpp)
riscv_mini_catch_test(test_pipeline_debug tests/test_pipeline_debug.cpp)
riscv_mini_catch_test(test_pipeline_wrapper tests/test_pipeline_wrapper.cpp)
riscv_mini_catch_test(test_wrapper_simple tests/test_wrapper_simple.cpp)
riscv_mini_catch_test(test_wrapper_ifstage tests/test_wrapper_ifstage.cpp)
riscv_mini_catch_test(test_wrapper_pipeline tests/test_wrapper_pipeline.cpp)
riscv_mini_catch_test(test_ifstage_device tests/test_ifstage_device.cpp)
riscv_mini_catch_test(test_rv32i_pipeline_example tests/test_rv32i_pipeline.cpp)
riscv_mini_catch_test(test_interrupt_flow tests/test_interrupt_flow.cpp)

# cache-complete-rewrite §3.4 smoke: ICachePrefetch + DCacheWriteBuffer wrapper build probe
riscv_mini_catch_test(riscv_mini_cache_smoke tests/test_cache_smoke.cpp)
# include path 需覆盖 include/cpu/cache (wrapper 用 #include "i_cache_complete.h")
target_include_directories(riscv_mini_cache_smoke PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include/cpu/cache)

# === 安装 (Phase 1-3 + 5 个核心 Pipeline 测试) ===
install(TARGETS rv32i_phase1_test rv32i_phase2_test rv32i_phase3_test
              test_pipeline_exec test_soc_hello_world test_riscv_tests
              test_address_decoder test_riscv_tests_pipeline
        RUNTIME DESTINATION bin/riscv-mini)
