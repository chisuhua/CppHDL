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
# ⚠️ 2026-10-24 (change `rv32i-canonical-convergence-2026-10`, ADR-046 D7):
# 此前此处有一段「include 顺序敏感 — include/cpu/pipeline 必须在 src 之前」的注释,
# 因为 include/cpu/ 与 src/ 存在 **10 组同 basename 头文件**, 消歧纯靠 -I 顺序
# (无任何护栏 ⇒ 调序即静默换实现)。
# ⇒ 收敛后 example/src 只剩 SoC 顶层 (rv32i_core_top.h), **同名头不再存在**
#   ⇒ -I 顺序**不再**决定实现 ⇒ 该约束**作废**。
# ⇒ -I 列表里保留 src/ 与 include/cpu/pipeline/ 是为了 (a) SoC 顶层可被 include,
#   (b) include/cpu/pipeline/stages/*.h 内部的同目录 include ("rv32i_decoder.h")
#   仍按「includer 自身目录优先」解析。
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

# ⚠️ 2026-10-30（ADR-046 D3「库 → example 反向耦合」的**第三种形态**收口）：
#   `test_csr_unit` 此前由**库**的 `tests/CMakeLists.txt` 注册（编译本目录的
#   `tests/test_csr_unit.cpp`）⇒ ⇒ 库测试 target 依赖 example 目录 ⇒ ⇒
#   「库 → example 反向依赖」。
#   ⇒ ⇒ 归属改到**本目录**（源文件在这里 ⇒ 依赖方向自然正确）。
#   ⇒ ⇒ **CTest 条目名不变**（`test_csr_unit`）⇒ ⇒ 计数锚点不变。
#   ⇒ ⇒ 本 helper 比库侧的 `add_catch_test` 多一条 `TIMEOUT 300`
#     （⇒ 防止 CI 挂死 ⇒ 是**改善**）。
riscv_mini_catch_test(test_csr_unit tests/test_csr_unit.cpp)
riscv_mini_catch_test(test_address_decoder tests/test_riscv_tests_soc.cpp)

# === 核侧 AXI4-Lite 主设备（S4a · change `rv32i-soc-mmio-channel-2026-11` · P2）===
# DUT = `src/axil_mmio_master.h`（核内 MMIO 通道的一端 · 纯 RTL 组件）
# 判据 = 「一次事务恰好一拍 `ready`」+ AW/W 相位分离 + VALID 期间数据稳定 ⇒
# 逐拍手推表（见源文件头）· 标签 `base` ⇒ `CTEST_TOTAL` +1（锚点同批同步）。
riscv_mini_catch_test(test_axil_mmio_master tests/test_axil_mmio_master.cpp)

# === 核级（IP 级）MMIO 通道判据（change `rv32i-ip-freeze-2026-11` · T1）===
# DUT = `src/rv32i_core_top.h`（核 IP 顶层）· 外接**响应式 AXI4-Lite 从设备 stub**
# ⇒ ⇒ 补上「IP 自己的接口被单独验证过」这一格（此前 24 条核级判据全程 tie 0 ⇒ 零 MMIO 覆盖）。
# ⚠️ **不新增端口**（读值用既有 `obs_data_write_data` 观测 ⇒ 端口面零变化）。
# ⚠️ 标签 `base` ⇒ `CTEST_TOTAL` +1（锚点同批同步）。
riscv_mini_catch_test(test_rv32i_core_mmio tests/test_rv32i_core_mmio.cpp)

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

# === SoC 级双后端 parity（change rv32i-pipeline-commit-path-2026-10）===
# DUT = `src/rv32i_core_top.h`（SoC 顶层，在 example 侧 ⇒ 测试也必须在
# example 侧：根 `tests/**` 不许 include example 路径，护栏 C3 会拦）。
# 本测试用 verilator 后端 ⇒ 需要 `verilator_e2e` 标签 + `verilator_backend`
# 资源锁 + verilator 安装目录的 PATH 注入（与根 `tests/CMakeLists.txt` 的
# `add_dual_backend_test` 同款；这里不能直接用那个函数，因为它在根 tests/ 作用域）。
riscv_mini_catch_test(test_rv32i_soc_dual_parity tests/test_rv32i_soc_dual_parity.cpp)
if(CPPHDL_VERILATOR_WRAPPER)
    get_filename_component(_soc_vl_bin_dir "${CPPHDL_VERILATOR_WRAPPER}" DIRECTORY)
    set(_soc_vl_path_env "PATH=$ENV{PATH}:${_soc_vl_bin_dir}")
else()
    set(_soc_vl_path_env "")
endif()
set_tests_properties(test_rv32i_soc_dual_parity PROPERTIES
    LABELS "base;verilator_e2e"
    RESOURCE_LOCK "verilator_backend"
    ENVIRONMENT "${_soc_vl_path_env}"
    TIMEOUT 600
)

# === RV32I **层次化产物交付物**（诉求 ④ · ADR-070 D1/D2 · R21-C）===
# DUT = `src/rv32i_core_top.h`（example 侧 ⇒ 测试也必须在 example 侧）。
# ⚠️ 重量级：层次化 + 扁平两份 Verilator 模型（分钟级）⇒ ⇒ `verilator_e2e` 标签 +
#    `verilator_backend` 资源锁 + `TIMEOUT 900`。
# ⚠️ `CTEST_TOTAL` +1 · `VERILATOR_E2E_TOTAL` +1 ⇒ 锚点必须同批同步。
riscv_mini_catch_test(test_rv32i_hier_rtl tests/test_rv32i_hier_rtl.cpp)
set_tests_properties(test_rv32i_hier_rtl PROPERTIES
    LABELS "base;verilator_e2e"
    RESOURCE_LOCK "verilator_backend"
    ENVIRONMENT "${_soc_vl_path_env}"
    TIMEOUT 900
)

# cache-complete-rewrite §3.4 smoke: ICachePrefetch + DCacheWriteBuffer wrapper build probe
# ⚠️ 2026-10-28（change `riscv-mini-cache-wrapper-live-logic-2026-10`）：本测试现在
# **双后端**跑（interpreter + verilator）⇒ 需要 `verilator_e2e` 标签 +
# `verilator_backend` 资源锁 + verilator 安装目录的 PATH 注入
# （与上面 `test_rv32i_soc_dual_parity` 同款；`CTEST_TOTAL` **不变** ——
# 该 target **已**注册进 CTest，只是加标签 ⇒ 只有 `verilator_e2e` 计数 +1）。
riscv_mini_catch_test(riscv_mini_cache_smoke tests/test_cache_smoke.cpp)
# include path 需覆盖 include/cpu/cache (wrapper 用 #include "i_cache_complete.h")
target_include_directories(riscv_mini_cache_smoke PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include/cpu/cache)
if(CPPHDL_VERILATOR_WRAPPER)
    get_filename_component(_cache_vl_bin_dir "${CPPHDL_VERILATOR_WRAPPER}" DIRECTORY)
    set(_cache_vl_path_env "PATH=$ENV{PATH}:${_cache_vl_bin_dir}")
else()
    set(_cache_vl_path_env "")
endif()
set_tests_properties(riscv_mini_cache_smoke PROPERTIES
    LABELS "base;verilator_e2e"
    RESOURCE_LOCK "verilator_backend"
    ENVIRONMENT "${_cache_vl_path_env}"
    TIMEOUT 600
)

# ── RV32I SoC 的**激励驱动**（三角色分离 · `example-file-boundary-2026-11`）──
# ⚠️ **发射入口 = TB 自身**（`--emit-rtl` 模式）⇒ ⇒ 原独立的三个发射薄壳
#   （`emit_rtl.cpp` / `emit_rtl_fw_init.cpp` / `emit_rtl_hier.cpp`）**已删**
#   ⇒ ⇒ 与 `examples/axi4` / `examples/stream` 的收敛**同型**
#   （change `emit-cli-into-example` + 蓝图 §4.3.3「职责分离裁决」）。
# ⚠️ 默认模式 = **验证模式**（解释器路径 ⇒ `RESULT: checks=24 fails=0`）。
# ⚠️ 三种发射形态**共用这一个二进制**（模式标记 ⇒ 库层 CLI 的既有选项）：
#   · `--emit-rtl`                ⇒ 扁平产物
#   · `--emit-rtl --hierarchical` ⇒ 层次化产物（诉求 ⑤ · 走 `toVerilogHierarchical` ·
#                                    ADR-058 §2.3 · 产物 = 单文件 8 module）
#   · `--emit-rtl --firmware-init` ⇒ 带固件初值的扁平产物（`firmware-load-chain` L2 路线 A）
#                                    ⇒ 与基线**共用全部组件**，唯一差别 = TCM 的 `init_data`
#                                    ⇒ 产物里多出 `initial begin mem[i] = … end` ⇒ **零端口变更**
# ⇒ ⇒ 三种产物的**顶层端口表同源**（同一个 `make_flat_module_view`）
#   ⇒ ⇒ **同一个独立 TB 可吃全部产物**；各自用独立的 `--out` 目录 ⇒ 互不影响。
add_executable(tb_rv32i_soc tb_rv32i_soc.cpp)
target_link_libraries(tb_rv32i_soc PRIVATE cpphdl)
target_include_directories(tb_rv32i_soc PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
set_target_properties(tb_rv32i_soc PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)

# === 安装 (Phase 1-3 + 5 个核心 Pipeline 测试) ===
install(TARGETS rv32i_phase1_test rv32i_phase2_test rv32i_phase3_test
              test_pipeline_exec test_soc_hello_world test_riscv_tests
              test_address_decoder test_riscv_tests_pipeline
        RUNTIME DESTINATION bin/riscv-mini)
