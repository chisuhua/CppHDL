# tests/CMakeLists.txt
set(CATCH_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

function(add_catch_test TEST_NAME)
    set(_srcs ${ARGN})
    list(APPEND _srcs ${CATCH_INCLUDE_DIR}/catch_amalgamated.cpp)
    add_executable(${TEST_NAME} ${_srcs})
    target_include_directories(${TEST_NAME} PRIVATE
        ${CATCH_INCLUDE_DIR}
        ${PROJECT_SOURCE_DIR}/include
    )
    if(TARGET cpphdl)
        target_link_libraries(${TEST_NAME} cpphdl)
    endif()
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
    set_tests_properties(${TEST_NAME} PROPERTIES LABELS "base")
endfunction()

# dual-backend-test-coverage (Phase 1) — wraps add_catch_test with the
# `verilator_e2e` LABELS + `RESOURCE_LOCK "verilator_backend"` +
# `_vl_path_env` ENVIRONMENT trio, mirroring the existing
# `test_verilator_*` tests (test_verilator_trace_parity, etc.). Accepts
# an optional `TIMEOUT <N>` kwarg (default 60s).
function(add_dual_backend_test TEST_NAME)
    set(_opts TIMEOUT)
    set(_multival_args SOURCES)
    set(_oneval_args ${_opts})
    cmake_parse_arguments(DBT "" "${_oneval_args}" "${_multival_args}" ${ARGN})

    if(NOT DBT_SOURCES)
        message(FATAL_ERROR "add_dual_backend_test(${TEST_NAME}): SOURCES missing")
    endif()
    if(NOT DBT_TIMEOUT)
        # ⚠️ **默认超时 180 s**（2026-11-03 实测订正，原为 60 s）：
        #   dual-backend 测试在**冷 verilator 缓存**下要为每个 DUT 现场编译
        #   一份 `libVtop.so` ⇒ 单测试冷启动实测 **21–130 s**
        #   （`test_cache_sync` 隔离复测 = **96 s**；warm = 1 s）
        #   ⇒ ⇒ 60 s 在冷缓存下**必然**超时失败（CI 的
        #   `cpphdl-verilator-${os}-${sha}` 缓存一旦未命中就会翻红）。
        #   ⇒ 180 s ≈ 实测最慢值（96 s）的 **1.9×** 余量。
        #   ⚠️ 代价：「挂死」的检测变慢 3×（判据强度不降 —— 仍是超时即失败）。
        #   ⚠️ 更重的测试显式给更大的值（`test_axi_interconnect_4x4` = 600 s，
        #   cold 实测 453 s）。数据与复现命令见
        #   `.rddf/checks/ci-wallclock-cold-vs-warm-2026-11.md`。
        set(DBT_TIMEOUT 180)
    endif()

    add_catch_test(${TEST_NAME} ${DBT_SOURCES})
    set_tests_properties(${TEST_NAME} PROPERTIES
        LABELS "base;verilator_e2e"
        RESOURCE_LOCK "verilator_backend"
        ENVIRONMENT "${_vl_path_env}"
        TIMEOUT ${DBT_TIMEOUT}
    )
endfunction()

enable_testing()

# ============================================================================
# RISC-V 相关测试 (使用 chlib/ 路径 - 已删除冗余的 cpu/ 和 riscv-integration/)
# ============================================================================

add_catch_test(test_forwarding chlib/test_forwarding.cpp)
add_catch_test(test_hazard chlib/test_hazard_complete.cpp)


# RV32I Pipeline integration test (from examples/riscv-mini/tests/)
# Includes IF/ID/EX/MEM/WB stage registers, forwarding, hazard, branch predictor
# 已迁移到根目录测试: tests/test_rv32i_pipeline.cpp (12 个 TEST_CASEs)
# 原 examples/ 版本 5 个测试已通过根目录版本覆盖

# ⚠️ 2026-10-30（ADR-046 D3「库 → example 反向耦合」的**第三种形态**收口）：
#   `test_csr_unit` 此前在此注册（`add_catch_test(test_csr_unit
#   ${PROJECT_SOURCE_DIR}/examples/riscv-mini/tests/test_csr_unit.cpp)`）
#   ⇒ ⇒ **库**的测试 target 编译**example** 目录里的源文件 ⇒ ⇒ 「库 → example」
#   的**反向依赖**。
#   ⇒ ⇒ 前两种形态（反向 `-I` 与反向 `#include`）已被 ADR-046 的护栏 C2/C3 覆盖，
#     但**这一种**（库 target 编 example 源文件）**未被覆盖** ⇒ ⇒ 本次收口。
#   ⇒ ⇒ 归属改到 example 侧：见 `examples/riscv-mini/CMakeLists.txt` 的
#     `riscv_mini_catch_test(test_csr_unit tests/test_csr_unit.cpp)`。
#   ⇒ ⇒ **CTest 条目名不变**（`test_csr_unit`）⇒ ⇒ 计数锚点不变。

# ============================================================================
# chlib 库测试 (26 个测试文件)
# ============================================================================

# chipforge 5-stage Simulator SEGV repro (Phase 6c M6 investigation)
# Independent repro of ch_bool(false) && (rd_addr != addr_t(0)) -> null impl
# Standalone CppHDL test (no chipforge dependency) to isolate the bug.
add_catch_test(test_chipforge_mux_segv_repro test_chipforge_mux_segv_repro.cpp)

# Bundle literal fix regression (fix-bundle-base-null-ptr-hijack)
# Verifies SFINAE-restricted integer ctor on bundle_base prevents
# null pointer constant hijack for ch_stream / ch_flow / AXI channels.
add_catch_test(test_bundle_literal test_bundle_literal.cpp)

# verilator-perf-three-way: codegen 字面量 mask 修复测试 (不依赖 verilator 二进制)
# 验证 ch_uint<N>(literal) 生成的 Verilog 字面量值落在 N 位内,避免触发
# Verilator "Too many digits for N bit number" 语法错误 (TC-07 depth=1000 关键)
#
# strengthen-verilator-e2e-coverage D5: file renamed
# test_verilator_three_way.cpp → test_codegen_literal_mask.cpp (诚信修复
# 误导命名,源文件 header 注释第 17 行自承 "不依赖 verilator 二进制")。
# Old ctest name retained as alias below for backwards compatibility.
add_catch_test(test_codegen_literal_mask test_codegen_literal_mask.cpp)
add_test(NAME test_verilator_three_way
         COMMAND test_codegen_literal_mask)
set_tests_properties(test_verilator_three_way PROPERTIES LABELS "base")

# Gap F2 closure: AXI4-Lite e2e test through VerilatorBackend (real verilator
# compile + dlopen + multi-cycle write/read). Skipped when verilator not on
# PATH (PR matrix). See test_axi_lite_verilator_e2e.cpp header for context.
#
# ADR-035 verilator-PATH fix (2026-10-13): the three VerilatorBackend tests
# (`test_axi_lite_verilator_e2e`, `test_verilator_backend`,
# `test_verilator_e2e_harness`) all SKIP every TEST_CASE when `verilator`
# is not on PATH. Catch2 then returns exit code 4 ("all tests skipped"),
# which ctest treats as a failure — so a missing `$PATH` for verilator
# silently looks like 3 flaky tests. We inject the install prefix's bin
# directory (where `${CPPHDL_VERILATOR_WRAPPER}` lives) into the child
# process PATH via the ctest `ENVIRONMENT` property, so `which verilator`
# and `popen("verilator ...")>` succeed and the tests actually execute
# their ASSERTIONS. Falls back to "verilator on PATH" behavior when
# CPPHDL_VERILATOR_WRAPPER is empty (BUILD_VERILATOR=OFF local-dev mode).
if(CPPHDL_VERILATOR_WRAPPER)
    get_filename_component(_vl_bin_dir "${CPPHDL_VERILATOR_WRAPPER}" DIRECTORY)
    set(_vl_path_env "PATH=$ENV{PATH}:${_vl_bin_dir}")
else()
    set(_vl_path_env "")
endif()
add_catch_test(test_axi_lite_verilator_e2e test_axi_lite_verilator_e2e.cpp)
set_tests_properties(test_axi_lite_verilator_e2e PROPERTIES
    LABELS "base;verilator_e2e"
    RESOURCE_LOCK "verilator_backend"
    ENVIRONMENT "${_vl_path_env}"
)

# dual-backend-test-coverage (Phase 1): DUAL_BACKEND_TEST_PAIR macro
# self-validation. Tagged [dual][template][counter]; registered via the
# new add_dual_backend_test() helper so it inherits the verilator_e2e
# label + resource lock + PATH env injection. The verilator half
# generates real Verilog → verilator compile → dlopen → counter
# check; the interpreter half is the source of truth.
add_dual_backend_test(test_dual_backend_template
    SOURCES test_dual_backend_template.cpp
    TIMEOUT 300
)

# 64-bit port boundary fixture (spec R10 §"64-bit cap"). Tagged
# [dual][width-cap]. Registered as `verilator_e2e` so it picks up
# the resource lock + PATH env, but the test itself is structured
# to PASS even when verilator rejects >64-bit designs (CounterWide96
# case). TIMEOUT 300 to accommodate cold verilator compile.
add_dual_backend_test(test_dual_backend_64_bit
    SOURCES test_dual_backend_64_bit.cpp
    TIMEOUT 300
)

# 2-step clock toggle fixture (spec R10 §"2-step clock toggle").
# Tagged [dual][clock]. Verifies `eval_sequential` 2-step toggle
# fires `always_ff @(posedge clk)` and counter increments correctly.
add_dual_backend_test(test_dual_backend_clock
    SOURCES test_dual_backend_clock.cpp
    TIMEOUT 300
)

# default_reset pulse fixture (spec R10 §"default_reset pulse").
# Tagged [dual][reset-init]. Verifies init values are loaded by
# `try_initialize()`'s reset pulse.
add_dual_backend_test(test_dual_backend_reset
    SOURCES test_dual_backend_reset.cpp
    TIMEOUT 300
)

# 基础功能测试
add_catch_test(test_arithmetic chlib/test_arithmetic.cpp)
add_catch_test(test_arithmetic_advance chlib/test_arithmetic_advance.cpp)
add_catch_test(test_assert chlib/test_assert.cpp)

# ⚠️⚠️ **A5 批次 B1**（2026-11-01 · change `dual-backend-parity-batch-b-2026-11`）
#    ⇒ ⇒ 双后端 parity 的**第 2 个新批次**（`chlib::AssertChecker` · **纯组合**）
#    ⇒ ⇒ **新增 `_behavior` 文件** ⇒ ⇒ 原 `test_assert` MUST NOT 修改
#    ⇒ ⇒ ⚠️ 只转 4 个 `AssertChecker` 用例 ⇒ ⇒ 原文件的 2 个
#    ⇒ ⇒ **`std::cout` 重定向**用例 MUST NOT 转（断言的是 C++ 侧打印
#    ⇒ ⇒ ⇒ 不是硬件信号 ⇒ ⇒ 双后端无意义 ⇒ ⇒ 且 stdout 是全局状态）
add_dual_backend_test(test_assert_behavior
    SOURCES chlib/test_assert_behavior.cpp
)
add_catch_test(test_bitwise chlib/test_bitwise.cpp)
add_catch_test(test_cache_simple chlib/test_cache_simple.cpp)
add_catch_test(test_cache_complete chlib/test_cache_complete.cpp)
add_catch_test(test_cam chlib/test_cam.cpp)
add_catch_test(test_combinational chlib/test_combinational.cpp)
add_catch_test(test_converter chlib/test_converter.cpp)
add_catch_test(test_fifo chlib/test_fifo.cpp)
add_catch_test(test_guard_contribution chlib/test_guard_contribution.cpp)
add_catch_test(test_multi_if_acceptance chlib/test_multi_if_acceptance.cpp)
add_catch_test(test_lnode_entry_set test_lnode_entry_set.cpp)
add_catch_test(test_if_stmt chlib/test_if_stmt.cpp)
add_catch_test(test_logic chlib/test_logic.cpp)
add_catch_test(test_memory chlib/test_memory.cpp)
add_catch_test(test_onehot chlib/test_onehot.cpp)
add_catch_test(test_pipeline_lib chlib/test_pipeline.cpp)
add_catch_test(test_selector_arbiter chlib/test_selector_arbiter.cpp)
add_catch_test(test_selector_priority chlib/test_selector_priority.cpp)
add_catch_test(test_selector_round_robin chlib/test_selector_round_robin.cpp)
add_catch_test(test_sequential chlib/test_sequential.cpp)
add_catch_test(test_simulator_trace chlib/test_simulator_trace.cpp)
add_catch_test(test_stream chlib/test_stream.cpp)
add_catch_test(test_switch chlib/test_switch.cpp)

# Cache 相关测试
add_catch_test(test_cache_pipeline chlib/test_cache_pipeline_integration.cpp)

# dual-backend-cache-tests: ICacheSync / DCacheSync runtime test
# (ADR-041 Sync variant, 1-cycle sread) — new file, dual verified via
# DUAL_BACKEND_TEST_PAIR macro. See openspec/changes/dual-backend-cache-tests/.
add_dual_backend_test(test_cache_sync
    SOURCES chlib/test_cache_sync.cpp
    # ⚠️ 显式 210（原 60）：冷 verilator 缓存下本测试实测 **96 s**
    #    （隔离复测，直接跑二进制，无并发负载）⇒ ⇒ 60 s 必然超时失败。
    #    210 = 2× 冷实测（30 s 的倍数）。
    #    依据：`.rddf/checks/ci-wallclock-cold-vs-warm-2026-11.md`
    TIMEOUT 210
)

# bundle master/slave connection regression fence (capability
# `bundle-master-slave-io`). Guards the cross-boundary value propagation that
# `samples/fifo_bundle.cpp` silently lost: the samples are NOT in ctest / CI,
# so without this Catch2 test a broken bundle connection is invisible.
add_catch_test(test_bundle_master_slave test_bundle_master_slave.cpp)
add_catch_test(test_bundle_ecosystem test_bundle_ecosystem.cpp)

# mem-read-timing: cross-backend parity for the memory read port timing
# contract. The Simulator's `ch_mem::aread` is 0-cycle, matching the
# generated Verilog `assign proxy = mem[addr];`, so interpreter and verilator
# MUST observe the same value for a register-driven read address.
# Weread-based designs were excluded from the dual-backend matrix before
# `sim-mem-read-timing-align-2026-10` eliminated the 1-cycle skew.
add_dual_backend_test(test_mem_read_timing_dual
    SOURCES test_mem_read_timing_dual.cpp
    TIMEOUT 300
)

# bundle-ecosystem-hardening (D6): the FREE `operator<<=` connection path
# for ch_stream. tests/test_ch_stream.cpp's 2 dual pairs cover bare-port
# dataflow, NOT the stream-to-stream connection (the only place in the
# repo that goes through the free operator) => that path had no parity
# coverage at all. The connection was converted from node-rebinding to
# data-flow edges (ADR-042 semantics).
add_dual_backend_test(test_stream_connect_dual
    SOURCES test_stream_connect_dual.cpp
    TIMEOUT 300
)

# chlib-stream-semantics-2026-10: `chlib::stream_fifo` / `chlib::sync_fifo`
# 的 FWFT（0-cycle）语义的双后端 parity 覆盖。
# ⚠️ 动机：0-cycle 的 `aread` 语义是历史上漂移过的地方
#    （`sim-mem-read-timing-align-2026-10` 才发现 interpreter 与 Verilog
#    差 1 拍）⇒ 只跑 interpreter 的断言 MUST NOT 被当作契约已被守住的证据。
# ⚠️ 本文件同时是仓库第一例「同一个 ch_mem 同时挂 aread + sread 两个读端口」
#    的设计 ⇒ 顺带覆盖 codegen 的多读端口路径。
add_dual_backend_test(test_stream_fifo_dual
    SOURCES test_stream_fifo_dual.cpp
    TIMEOUT 300
)

# stream-builder-and-pipe-ready-ownership-2026-11 (B1 · ADR-055):
# `StreamBuilder` 链的**上游反压**（链内闭包不变式）。此前 builder 链里
# **没人驱动** `source.ready` ⇒ ⇒ 它恒 0 ⇒ ⇒ 上游遵守 ready 协议 ⇒ ⇒ 永远推不动。
# ⚠️ 判别性：本文件在**空链**时断言 `in_ready == 1`（修复前恒 0 ⇒ 必红）——
#    「满时为 0」是**不可区分**的（修复前也是 0）。
add_dual_backend_test(test_stream_builder_backpressure_dual
    SOURCES test_stream_builder_backpressure_dual.cpp
    TIMEOUT 300
)

# ADR-056 / ADR-057（2026-11-02）：`stream_demux` 的 ready 方向 +
# `stream_fork` 的 AND 同步语义。
# ⚠️ 两个组件此前**零 dual 覆盖**，且在护栏 R8 里是**豁免**状态
#    （「方向修正是行为变更」/「同步语义从未工作过」）⇒ 不在任何 ready 契约内。
# ⚠️ 判别性 = **默认（未接反压）时 in_ready == 1**：修复前该节点是**孤儿**
#    ⇒ 恒 0，而「被挡住」时也是 0 ⇒ 只断「为 0」不可区分（假绿）。
add_dual_backend_test(test_stream_demux_fork_dual
    SOURCES test_stream_demux_fork_dual.cpp
    TIMEOUT 300
)

# B7 · capability `core-node-driver-visibility`：**未驱动字段的只读诊断**
# （`ch::core::diagnose_drive` / `drive_state`）的可执行判据。
# ⚠️ 判别性 = **五种状态逐一钉住**：少一格 ⇒ 「诊断说未驱动、实际已驱动」
#    这类误判就可能溜过去。
# ⚠️ **零回归**：本 API 是**只读**的 ⇒ 不改任何读取语义。
add_catch_test(test_node_driver_diagnosis test_node_driver_diagnosis.cpp)

# A5 批次 B（2026-11-02）：把既有**连接语义**用例转成双后端 parity。
# ⚠️ 形态 = 新增 `_behavior` 文件 ⇒ **原文件 MUST NOT 修改**
#    （保留 interpreter-only baseline ⇒ 零回归风险）。
# ⚠️ 判据（N2′）= 断言数(with verilator) == 2 × 断言数(interpreter-only)
#    ⇒ 证明 verilator 半边**真的跑了**。
add_dual_backend_test(test_bool_connection_behavior
    SOURCES test_bool_connection_behavior.cpp
    TIMEOUT 300
)
add_dual_backend_test(test_operator_connection_behavior
    SOURCES test_operator_connection_behavior.cpp
    TIMEOUT 300
)
add_dual_backend_test(test_wires_connection_behavior
    SOURCES test_wires_connection_behavior.cpp
    TIMEOUT 300
)
add_dual_backend_test(test_multi_if_acceptance_behavior
    SOURCES chlib/test_multi_if_acceptance_behavior.cpp
    TIMEOUT 300
)

# ⚠️⚠️ **路线 B（A5 的 H 批）第 1 批**（2026-11-02 · change
#    `dual-backend-parity-route-b-fsm-2026-11`）
#    ⇒ `ch_state_machine` 的**运行期转移**此前在 codegen 路径上**零验证**
#    ⇒ 同一缺陷类已在 uart_tx（状态寄存器 `q <= q` 自环）与 SPI fixture
#    （状态卡死在 TRANSFER）**两次真实发生**，且两次都被弱断言掩盖。
#    ⇒ ⇒ **新增**文件（**不是**转换 `tests/test_state_machine.cpp`：
#    ⇒ 那个文件 6/8 用例是编译期/构图期判据 ⇒ dual 化零增量价值）。
add_dual_backend_test(test_state_machine_dual
    SOURCES test_state_machine_dual.cpp
    TIMEOUT 300
)

# ⚠️⚠️ **路线 B（A5 的 H 批）第 2 批**（2026-11-02 · change
#    `dual-backend-parity-route-b-conditional-2026-11`）
#    ⇒ `chlib::conditional_block`（ADR-051 的 `_if` 语句块 DSL）的**运行期
#    语义**此前在 codegen 路径上**零验证**。
#    ⇒ 该 DSL 正是本仓「假绿」病的**原型病例**（ADR-051 记录的实现缺陷曾被
#    既有测试**钉住** ⇒ 绿灯承载的是缺陷）⇒ ⇒ 「条件真的参与了求值」的
#    判据是该病种的疫苗。
add_dual_backend_test(test_if_stmt_dual
    SOURCES chlib/test_if_stmt_dual.cpp
    TIMEOUT 300
)

# ⚠️⚠️ **路线 B（A5 的 H 批）第 2 批**（同上一个 change）
#    ⇒ `chlib::compose_guard_chain` 是仓内**唯一**一处「多条 (守卫, 值) 合成
#    一条 select 链」的代码 ⇒ ⇒ 它是「优先级方向（先匹配者胜）」的**唯一定义点**。
#    ⇒ ⚠️ 而那条契约在 ADR-050 的状态守卫侧是**运行期不可观测**的（`is_in(S)`
#    两两互斥）⇒ ⇒ 只能靠**结构级**断言钉住 ⇒ ⇒ **结构断言验的是图，不是行为**
#    ⇒ ⇒ 本文件用**非互斥**守卫让链序**运行期可观测** ⇒ ⇒ 「方向」第一次有了
#    **运行期**判据。
add_dual_backend_test(test_guard_contribution_dual
    SOURCES chlib/test_guard_contribution_dual.cpp
    TIMEOUT 300
)

# ⚠️⚠️ **路线 B（A5 的 H 批）第 3 批**（2026-11-02 · change
#    `dual-backend-parity-route-b-fifo-2026-11`）
#    ⇒ `sync_fifo` 是**所有** FIFO 型组件的本体，而它的**空/满门控（自保护）**
#    在 codegen 路径上**零验证** —— 而该缺陷类在仓内**真实发生过**（ADR-045：
#    满写覆写队首 / 空读让读指针自由运行 ⇒ 之后写入的元素永远读不出来）。
#    ⇒ 既有的 `test_stream_fifo_dual` 覆盖的是 `stream_fifo` **wrapper**
#    ⇒ ⇒ **不是**本体。
#    ⇒ 既有 `test_fifo.cpp` 是**裸 context + 直接调 `sync_fifo()`**（无
#    `ch_device` / 无端口）⇒ macro 不适用 ⇒ 必须新建带端口的 DUT。
add_dual_backend_test(test_sync_fifo_dual
    SOURCES chlib/test_sync_fifo_dual.cpp
    TIMEOUT 300
)

# ⚠️⚠️ **路线 B（A5 的 H 批）第 4 批**（2026-11-02 · change
#    `dual-backend-parity-route-b-arithmetic-encoding-2026-11`）
#    ⇒ `chlib` 的**编码器/译码器/BCD/格雷码**在 41 个 dual 文件里**零覆盖**
#    ⇒ ⇒ 它们的 Verilog codegen 路径从未被验证。
#    ⇒ 而它们全由 **`select` 链**实现 ⇒ ⇒ **链的方向**决定语义 ⇒ ⇒ 而方向在
#    interpreter 侧【几乎不可观测】（既有测试只覆盖单 bit ⇒ 分不出方向）
#    ⇒ ⇒ 「多 bit 同时置位选哪个」这条真正的语义，两条后端都没被钉过。
add_dual_backend_test(test_arithmetic_encoding_dual
    SOURCES chlib/test_arithmetic_encoding_dual.cpp
    TIMEOUT 300
)

# ⚠️⚠️ **路线 B（A5 的 H 批）第 5 批**（2026-11-02 · change
#    `dual-backend-parity-route-b-arithmetic-2026-11`）
#    ⇒ `chlib::arithmetic` 的**带进位/带借位**算术 + `chlib::logic`/`chlib::sequential`
#    的**门/寄存器/计数器/边沿检测** ⇒ ⇒ 盘点实测这些在 42 个 dual 文件里
#    **零覆盖** ⇒ ⇒ 它们的 Verilog codegen 路径**从未被验证**。
#    ⇒ 危险点：前者的「**扩位 + 截断**」与后者的「**寄存器 + 条件组合**（保持语义）」
#    ⇒ ⇒ 都是两后端最容易分叉的地方。
add_dual_backend_test(test_arithmetic_carry_dual
    SOURCES chlib/test_arithmetic_carry_dual.cpp
    TIMEOUT 300
)
add_dual_backend_test(test_sequential_counters_dual
    SOURCES chlib/test_sequential_counters_dual.cpp
    TIMEOUT 300
)

# ⚠️⚠️ **路线 B（A5 的 H 批）第 6 批**（2026-11-02 · change
#    `dual-backend-parity-route-b-pipeline-2026-11`）
#    ⇒ `chlib::pipeline`（`PipelineStage` / `PipelineStageDual` / `PipelineChain`
#    + 函数式 `pipeline_reg`）是仓内**唯一的流水线寄存器组件族** ⇒ ⇒ 盘点实测
#    dual 覆盖率 = **0** ⇒ ⇒ Verilog codegen 路径从未被验证。
#    ⇒ 危险点：**rst / stall / flush 三条规则的相对优先级**（codegen 侧要写成
#    `q <= rst ? 0 : (stall|flush) ? q : d`）⇒ ⇒ 而既有测试**全是裸 context**
#    ⇒ ⇒ 连 interpreter 侧的「端口驱动」形态都没有覆盖。
add_dual_backend_test(test_pipeline_dual
    SOURCES chlib/test_pipeline_dual.cpp
    TIMEOUT 300
)

# ⚠️⚠️ **`sread` 禁读拍语义的双后端判据**（change
#    `sread-disable-cycle-divergence-2026-11`）
#    ⇒ 禁读拍（`enable == 0`）曾经两后端分歧：Simulator **保持上一拍值**、
#    生成的 Verilog **清零** ⇒ ⇒ 既有测试零覆盖（唯一用可变 enable 的
#    `test_mem_connection.cpp:43` 恒为 1）⇒ ⇒ 这条路径从未被验证。
add_dual_backend_test(test_mem_sread_disable_dual
    SOURCES test_mem_sread_disable_dual.cpp
    TIMEOUT 300
)

# ⚠️⚠️ **`chlib::memory` 的 RAM 族**（同一 change
#    `sread-disable-cycle-divergence-2026-11`）
#    ⇒ `single_port_ram` / `dual_port_ram` 用 `mem.sread(addr, !we)` ⇒
#    **写的那一拍就是禁读拍** ⇒ 是唯一会踩中该语义的生产组件。
#    ⇒ 写拍的输出 = `0`（清零，非保持）⇒ 同时是本分歧的**回归探针**。
add_dual_backend_test(test_memory_dual
    SOURCES chlib/test_memory_dual.cpp
    TIMEOUT 300
)

# ⚠️⚠️ **路线 A（D 批）第 1 个文件**（2026-11-02）
#    ⇒ `tests/test_mem_connection.cpp` 的 4 种 `ch_mem` 读端口**连接形态**全部是
#    裸 `ch_device` 单后端 ⇒ ⇒ 在 verilator codegen 上从未被验证。
#    ⇒ 危险点：读端口是**虚构节点**，其数据靠 `data_proxy` 承载 ⇒ codegen 必须
#    ① 跳过「src(0) 是 mem read port」的 proxy 赋值 · ② 代理链遍历不得越过
#    read port（否则 `assign out = <read_port>` 引用未声明信号）· ③ 不把
#    `type_mem_read_port` 放进 wire 声明 ⇒ 三处任一错都**只有双后端能抓**。
#    ⇒ 与 `test_mem_sread_disable_dual`（禁读拍语义）**正交**：本文件 enable 恒 1。
add_dual_backend_test(test_mem_connection_dual
    SOURCES test_mem_connection_dual.cpp
    TIMEOUT 300
)

# ⚠️⚠️ **路线 A（D 批）第 2 个文件**（2026-11-02）
#    ⇒ `tests/test_mem_comb_read.cpp` 的 8 个用例逐个核对**双后端增量**后，只有
#    **2 个**需要新增文件（跨拍写后读 · 同拍写+aread 的写后值）：
#    · `:289` / `:323`（aread 0-cycle / sread 1-cycle）**已被**
#      `test_mem_read_timing_dual.cpp` 以**同一判别形态**（寄存器驱动地址）覆盖
#      ⇒ 重复转换 = 零增量；
#    · `:203` / `:247`（输入地址 + tick 后读）**对 0/1-cycle 零判别力**
#      （T2，见文件头）且数据通路已被 `test_mem_connection_dual` 覆盖；
#    · `:368` / `:439` 是 **codegen 文本断言**，与后端无关。
#    ⇒ 真实空缺 = 裸 `ch_mem` 的**写口 × aread 交互**（实测：三个 mem dual 文件的
#    `.write(` 计数全为 0；`chlib/test_memory_dual` 的写口属 RAM 族）。
#    ⇒ 危险点：写口的 `if (we)` 与读侧的 `assign proxy = mem[addr]` 的**相对时序**
#    由 codegen + VerilatorBackend 决定，interpreter 另有一套分类 ⇒ 只有双后端能对齐。
#    ⇒ 与原文件**正交**：`test_mem_comb_read.cpp` MUST NOT 修改。
add_dual_backend_test(test_mem_comb_read_dual
    SOURCES test_mem_comb_read_dual.cpp
    TIMEOUT 300
)

# ⚠️⚠️ **路线 B（新文件）—— `chlib/bitwise.h` 首次进双后端 parity**（2026-11-02）
#    ⇒ 实测「含 `DUAL_BACKEND_TEST_PAIR(` 调用」的源文件全集里**无任何一个**
#      include `chlib/bitwise.h` ⇒ ⇒ 该组件**零双后端覆盖**。
#    ⇒ ⚠️ **不是转换，是新写**：既有单后端测试 `tests/chlib/test_bitwise.cpp`
#      的 9 个用例**全是裸 context**（无 ch_device / 无顶层端口）⇒ ⇒ 按
#      `dual-backend-test-coverage` 的 R6 排除清单**结构上不可转换**
#      （Verilator 后端只能观测顶层端口）。
#    ⇒ ⇒ 真正的增量：原测试把输入写成**编译期常量** ⇒ 那条 select 链在
#      **建图期**就定型 ⇒ ⇒ **运行期求值路径从未被验证**；本文件把输入改成
#      **端口** ⇒ ⇒ select 链在运行期求值（interpreter 的逐节点求值 vs
#      Verilog 的嵌套三元 = 真正可能分叉的地方）。
#    ⇒ 危险点：6 个检测器都是 **select 链**（方向敏感 + 运行期反馈）·
#      `bit_reversal` 的移位/截断 · `bit_swap` 的 `~mask & zext << |` 组合。
add_dual_backend_test(test_bitwise_dual
    SOURCES chlib/test_bitwise_dual.cpp
    TIMEOUT 300
)

# ⚠️⚠️ **路线 B（新文件）—— `chlib/onehot.h` 首次进双后端 parity**（2026-11-02）
#    ⇒ 实测「含 `DUAL_BACKEND_TEST_PAIR(` 调用」的源文件全集里**无任何一个**
#      include `chlib/onehot.h` ⇒ ⇒ **零覆盖**。
#    ⇒ ⚠️ **不是转换**：`tests/chlib/test_onehot.cpp` 的 7 个用例用了 **21 个
#      Catch2 SECTION** ⇒ 命中 R6 排除项 "SECTION re-entry" ⇒ 不可转换。
#    ⇒ 与 `test_arithmetic_encoding_dual.cpp`（`chlib/converter.h` 的 binary_*）
#      **正交** —— 那是另一个组件。
#    ⇒ 危险点：`onehot_dec` 的 select 链从 i=0 迭代 ⇒ **最高**置位胜出，
#      而 `bitwise.h` 的 `first_set_bit_detector` ⇒ **最低**置位胜出 ⇒
#      **两个组件的"谁胜出"约定相反** ⇒ 链方向 = codegen 最易分叉处。
#    ⇒ ⚠️ **只断言合法单置位输入**（文档：多位置位/全 0 ⇒ 输出**未定义**
#      ⇒ 不把未定义行为冻结成断言）。
add_dual_backend_test(test_onehot_dual
    SOURCES chlib/test_onehot_dual.cpp
    TIMEOUT 300
)

# ⚠️⚠️ **路线 B（新文件）—— `chlib::ForwardingMux` 的组件级语义进双后端 parity**
#    ⇒ 组件级 dual 覆盖 = **0**（`chlib/forwarding.h` 只在 SoC 级组合 DUT 里出现）。
#    ⇒ ⚠️ **不是转换**：`tests/chlib/test_forwarding.cpp` 的 8 个用例中 **6 个**用
#      **函数式**接口（裸 context、无顶层端口）⇒ 命中 R6 排除项 ⇒ 不可转换。
#    ⇒ **零 DUT 复制**：直接用库组件 `chlib::ForwardingMux<32>` 当 DUT（先例：
#      `tests/test_blt_debug_behavior.cpp`）⇒ 测的就是**生产代码本身**。
#    ⇒ 危险点：档位 1 的**跳转返回地址**判定**故意**放在组件内部（ADR-047 D4）——
#      头注释明写：内联到流水线里 ⇒ **测试台无法驱动** ⇒ 只能测**副本** ⇒ 假绿。
add_dual_backend_test(test_forwarding_dual
    SOURCES chlib/test_forwarding_dual.cpp
    TIMEOUT 300
)

# ⚠️⚠️ **路线 A（D 批）第 3 个文件**（2026-11-02）—— `tests/test_mem_timing.cpp`
#    的 7 个用例逐条过「可转性 4 问」后**只剩 2 个形态**有增量：
#    · `:535`（sread 禁读拍清零）⇒ **已被** `test_mem_sread_disable_dual` 覆盖
#    · `:389` / `:435`（写后读 / 顺序写读）⇒ **已被** `test_mem_comb_read_dual` 覆盖
#    · `:306`（aread + init）⇒ **已被** `test_mem_connection_dual` 的 pair 2 覆盖
#    ⇒ ⇒ 剩下的新形态 = ① **`:182` 的 `select()` 里嵌读端口**（mux 的操作数是读端口的
#      data proxy ⇒ codegen 必须引用**已声明**的 proxy，不能引用那个**虚构**的
#      read_port 节点 ⇒ 否则生成**未声明信号**）
#      ② **`:358` 的同一 mem 上两个读端口**（既有 dual 文件每个 DUT 只有一个读端口）
#    ⇒ ⚠️ **这两个 DUT 对 0/1-cycle 零判别力**（T2：地址来自 TB 驱动的输入端口）
#      ⇒ ⇒ 时序维度由 `test_mem_read_timing_dual`（寄存器驱动形态）负责 ⇒ 不重复。
add_dual_backend_test(test_mem_multiplex_dual
    SOURCES test_mem_multiplex_dual.cpp
    TIMEOUT 300
)

# ⚠️⚠️ **路线 B（新文件）—— `chlib/arithmetic_advance.h` 首次进双后端 parity**（2026-11-02）
#    ⇒ 实测「含 `DUAL_BACKEND_TEST_PAIR(` 调用」的源文件全集里**无任何一个**
#      include 它 ⇒ ⇒ **零双后端覆盖**（CLA / CSA / Wallace / Booth / 非恢复除法 / 定点四则
#      —— 全是**进位/移位链**，正是 interpreter 与 Verilog 最易分叉处）。
#    ⇒ ⚠️ **不是转换**：`tests/chlib/test_arithmetic_advance.cpp` 用**裸 context**
#      （ctx_swap、零 ch_device、无顶层端口）⇒ 命中 R6 排除项 ⇒ 结构上不可转换。
#    ⇒ ⚠️ **且不在任何既有批次清单里**（计划漏项）⇒ 「新写」路线不受批次表约束。
#    ⇒ 🔴 读实现时发现**三处既有缺陷 / 名不副实**（本批**不修**，独立立项）：
#      ① `wallace_tree_multiplier` 与 `booth_multiplier` **函数体逐字相同**
#         （都是 `a*b`）⇒ 既不是 Wallace 树也不是 Booth 编码 ⇒ 与 `fwft_fifo` /
#         `stream_arbiter_sequence` 同型的「名字 over-promise」；
#      ② `carry_save_adder` 的 `carry` 声明 N 位而内部 `temp` 是 N+1 位
#         ⇒ `temp << 1` **最高位（真正的 carry-out）被静默截断**；
#      ③ `fixed_point_divider` 的 `if (b == 0_d)` 是 **C++ 控制流作用于硬件值**
#         ⇒ `ch_bool::operator uint64_t()` 对非 const 节点恒返 0 ⇒ 建图期恒假
#         ⇒ **除零保护是死代码**（故本文件**不喂** b==0 —— interpreter 侧会 SIGFPE）。
add_dual_backend_test(test_arithmetic_advance_dual
    SOURCES chlib/test_arithmetic_advance_dual.cpp
    TIMEOUT 300
)

# ⚠️⚠️ **路线 A（E 批）—— `ICacheAsync` / `DCacheAsync` 的 0-cycle 语义进双后端 parity**
#    （2026-11-02）⇒ 本文件**同时是契约矛盾的裁决装置**：
#    capability `dual-backend-test-coverage` 里一个 Requirement 写「Async 禁令已解除、
#    MUST 逐周期一致」，另一个 Requirement 的 Scenario 仍写 `ICacheAsync` **MUST NOT**
#    用 macro ⇒ ⇒ **本文件真绿 ⇒ 矛盾按事实消解；真红 ⇒ 禁令保留 + 发现真分歧**。
#    ⇒ ⚠️ **只取 Async 3 个用例**：Sync 6 个已被 `tests/chlib/test_cache_sync.cpp`
#      （10 pair）覆盖 ⇒ 转 Sync = 重复计数（4 问 ①）。
#    ⇒ 激励与原用例（`test_cache_complete.cpp` 的 `:303` / `:332` / `:611`）**逐位相同**，
#      但期望值来源升级为**手推**：把中间拍的 `axi_valid` / `axi_addr` / `axi_wdata` /
#      `axi_we` 全部钉住，并新增 `cyc4 的 rdata == 0xCAFEBABE`（写直达真的落进 cache）
#      —— ⚠️ 原用例**从未**断言写入的数据 ⇒ 若 `do_write & hit_way0` 被写成
#      `& hit_way1`（way0 永不写），原用例**仍然全绿**。
add_dual_backend_test(test_cache_async_dual
    SOURCES chlib/test_cache_async_dual.cpp
    TIMEOUT 300
)

# ⚠️ **路线 A（F 批）—— `riscv::HazardUnitComplete` 进双后端 parity**（2026-11-02）
#    ⇒ **零 DUT 复制**：该组件**本身**就是带顶层端口的 `Component`
#      （18 个 `ch_in` + 7 个 `ch_out`，全部 ≤32 bit）⇒ 直接用库组件当 DUT
#      （先例：`tests/chlib/test_forwarding_dual.cpp` / `test_blt_debug_behavior.cpp`）
#      ⇒ **测的就是生产代码本身**。
#    ⇒ 4 问：① 无 dual 兄弟（`tests/chlib/test_hazard_complete.cpp` 是 interpreter-only，
#      且只断 1–2 个输出）；② 判别力见 5 条变异；③ 真跑仿真读顶层端口；④ 纯组合
#      （**无 `ch_reg`**）⇒ 1 拍即可。
#    ⇒ ⚠️ **生产消费者 = 0**（真实流水线用的是 `riscv::HazardUnit`）⇒ 本文件的价值在
#      「前推档位 × 优先级 × 停顿 × 冲刷」这套**组合语义契约**，**不是**关键路径回归
#      —— 如实登记，不包装。
#    ⇒ ⚠️ 读实现时发现 `id_is_load` / `id_is_branch` 两个**死端口**（`describe()` 从不读）
#      ⇒ 本批**不修**（删端口 = 改公开 API ⇒ 独立立项），但本文件**仍然驱动**它们。
add_dual_backend_test(test_hazard_complete_dual
    SOURCES chlib/test_hazard_complete_dual.cpp
    TIMEOUT 300
)

# ⚠️ **路线 B（G 批）—— `chlib` 三个 stream pipe 进双后端 parity**（2026-11-03）
#    ⇒ **价值最高的理由**：`stream_pipeline.h` 的三个 pipe 在 ADR-055（2026-11-01）
#      **刚被重写**，且重写**修掉了三个真缺陷**（m2s 丢数据 · s2m 方向反了的
#      自环 · half 的 toggle 死锁）⇒ ⇒ **重写后的语义从未被双后端验证过**。
#    ⇒ 含 `ch_reg` ⇒ 走 `always_ff` codegen 路径 ⇒ 两后端最易分叉处。
#    ⇒ 有生产消费者（`include/bundle/stream_bundle_member_inlines.h` 的
#      `m2sPipe()` / `s2mPipe()` / `halfPipe()`）。
#    ⇒ 4 问 ①：全仓 54 个 dual 文件里三个符号各出现 **0** 次 ⇒ 无重复。
#    ⇒ ⚠️ 既有 `tests/test_stream_pipeline.cpp`（12 用例 / 68 断言）**全是裸 context**
#      （`ch_device=0`、无顶层端口）⇒ 命中 **R6 排除项** ⇒ **不可转换** ⇒ 新写。
#      原文件 **MUST NOT 修改**。
#    ⇒ DUT 是**端口 wrapper**（三个 pipe 是函数式组合子，不是 Component）
#      ⇒ 语义全在库组件里 ⇒ **不是「DUT 复制」**（先例：
#      `tests/chlib/test_stream_builder_backpressure_dual.cpp`）。
add_dual_backend_test(test_stream_pipeline_dual
    SOURCES chlib/test_stream_pipeline_dual.cpp
    TIMEOUT 300
)

# ⚠️ **路线 B（H 批）—— `priority_selector` / `round_robin_selector` 进双后端 parity**
#    （2026-11-03）
#    ⇒ ⚠️ **刻意只取同文件里两个「干净」的组合子**：`round_robin_arbiter` 的
#      `UpdatePriority` 用 `if (result.valid)` 做硬件判断 ⇒ **建图期求值** ⇒ 恒假 ⇒
#      **`next_ptr` 恒 0 ⇒ 不轮转**（实测）⇒ **带真缺陷** ⇒ ⇒ dual parity 会给缺陷
#      **盖章**（两后端一致地错 ⇒ parity 看不见）⇒ ⇒ **先修缺陷再覆盖**
#      （独立立项 `.rddf/improvements/selector-round-robin-not-rotating-2026-11.md`）。
#    ⇒ `round_robin_selector` **有生产消费者**（`include/chlib/stream.h` 的
#      `stream_arbiter_round_robin` 调它）。
#    ⇒ 4 问 ①：两个符号在 54 个 dual 文件里各出现 **0** 次 ⇒ 无重复。
#    ⇒ ⚠️ 既有 `tests/chlib/test_selector_priority.cpp` 与
#      `tests/chlib/test_selector_round_robin.cpp` **全是裸 context**（`ch_device=0`）
#      ⇒ 命中 **R6 排除项** ⇒ **不可转换** ⇒ 新写；两个原文件 **MUST NOT 修改**。
add_dual_backend_test(test_selector_arbiter_dual
    SOURCES chlib/test_selector_arbiter_dual.cpp
    TIMEOUT 300
)

# ⚠️ **路线 B（I 批）—— `stream_arbiter_priority` / `stream_arbiter_round_robin`
#    进双后端 parity**（2026-11-03）
#    ⇒ `stream_arbiter_round_robin` 含 `ch_reg`（`last_grant_reg`）⇒ 走 `always_ff`
#      codegen 路径 ⇒ 两后端最易分叉处；且它的**轮转反馈**是 ADR-055（2026-11-01）
#      **刚修好的**（修前恒传 `last_grant = 0_d` ⇒ 扫描起点恒为 1 ⇒ 低位通道饿死）
#      ⇒ ⇒ **修复后的语义从未被双后端验证过**。
#    ⇒ 🔴 钉住两条**反直觉**语义：① 优先级方向 = **高位优先**
#      （`priority_encoder` 的注释「优先处理低位」与实现相反 ⇒ 已独立立项
#      `priority-encoder-doc-contradicts-implementation-2026-11.md`）；
#      ② 两个仲裁器的 ready 门控**不同**：priority **不被** valid 门控（无请求时通道 0
#      仍拿到 ready）而 round_robin **被**门控（无请求时全部 ready=0）。
#    ⇒ ⚠️ **刻意避开** `stream_arbiter_lock`（C++ `if` 建图期求值 ⇒ 恒选通道 0 ⇒
#      **输出别的通道的数据** ⇒ 静默数据损坏 ⇒ 已独立立项
#      `stream-arbiter-lock-selects-channel-zero-2026-11.md` ⇒ ⇒ 先修缺陷再覆盖）。
add_dual_backend_test(test_stream_arbiter_dual
    SOURCES chlib/test_stream_arbiter_dual.cpp
    TIMEOUT 300
)

# ⚠️ **ADR-064 的行为判据**：chlib stream 宽度适配器（修前三个 API 一仿真就 SIGSEGV）
#    ⇒ 既有 `test_stream_width_adapter.cpp` 的 8 个用例**全裸 context、零 Simulator**
#      ⇒ 图从不被实例化 ⇒ 缺陷完全不可见（保留它们锁宽度/类型，本文件补真跑仿真）。
add_dual_backend_test(test_stream_width_adapter_dual
    SOURCES chlib/test_stream_width_adapter_dual.cpp
    TIMEOUT 300
)

# ⚠️ **ADR-065 的行为判据**：chlib 仲裁器族的两处「建图期求值」缺陷
#    （`stream_arbiter_lock` 恒选通道 0 且输出错数据 · `round_robin_arbiter` 不轮转）
add_dual_backend_test(test_stream_arbiter_lock_dual
    SOURCES chlib/test_stream_arbiter_lock_dual.cpp
    TIMEOUT 300
)

# ⚠️ **路线 C 第 3 批**：RV32I 的三个「含时序状态」组件进双后端 parity
#    （`riscv::Rv32iPc` · `ch::chlib::Clint<1>` · `riscv::CsrBank`）
#    ⇒ 三者 dual 覆盖均为 0 ⇒ 且都走 `always_ff` codegen 路径（两后端最易分叉处）。
add_dual_backend_test(test_rv32i_arch_dual
    SOURCES test_rv32i_arch_dual.cpp
    TIMEOUT 300
)

# ⚠️ **B2 根因判据**（2026-11-03）：`mux` 的槽位宽度 MUST NOT 被 `eval` 改变。
#    `instr_mux::eval` 曾用 `*dst_ = *分支`（拷贝赋值 ⇒ **RESIZE**）⇒ ⇒
#    「较窄的分支被选中」时整个 mux 槽位被压窄 ⇒ ⇒ 下游按声明宽度取位**越界**。
#    ⇒ ⇒ 只在 interpreter 侧（Verilog 的 assign 不会改宽度）⇒ ⇒ 两后端分歧。
add_dual_backend_test(test_mux_width_dual
    SOURCES test_mux_width_dual.cpp
    TIMEOUT 180
)

# ⚠️⚠️ **M1（把 `assign_truncate` 改回 `*dst_ = *分支`）在【值层面不可判别】** ——
#    已实测：压窄只丢掉**已经为 0** 的高位 ⇒ ⇒ 下游读到的值**逐位等价**
#    ⇒ ⇒ `REQUIRE` 抓不到。⇒ ⇒ 它**唯一**的可观测量 = `bit_select` 的**越界错误日志**。
#    ⇒ ⇒ 用 ctest 的输出判据直接把「有这条日志」判成红。
#    ⚠️ 这条 `FAIL_REGULAR_EXPRESSION` 是**必需的**，不是装饰：删掉它 ⇒ ⇒
#    M1 这一类回归就**完全没有护栏**（本仓「护栏自身有盲区」病症的一格）。
set_tests_properties(test_mux_width_dual PROPERTIES
    FAIL_REGULAR_EXPRESSION "Bit index out of range")

# ⚠️ **B1 根因判据**（2026-11-03）：**多 word 左移 MUST 与 Verilog 的 `<<` 一致**。
#    `include/bv/bv_shift.h` 的 `bv_shl_vector` 曾把跨 word 进位写成
#    `in[..] << (64 - d)`（方向反了）且漏了 `in[i-s] << d` ⇒ ⇒
#    ① 产生**虚假高位** · ② **丢失跨 word 进位** · ③ 还可能**越界读** `in[]`。
#    ⇒ ⇒ 只在 interpreter 侧（Verilog 的 `<<` 天然正确）⇒ ⇒ 两后端分歧；
#    最初被误登记为「`eq` 在宽操作数下判错」（`eq` 本身是对的）。
add_dual_backend_test(test_wide_shl_dual
    SOURCES test_wide_shl_dual.cpp
    TIMEOUT 300
)

# ⚠️ **路线 C 第一批 —— RV32I 微架构组件进双后端 parity**（2026-11-03）
#    ⇒ **零 DUT 复制**：`MemStage` / `ExStage` / `Rv32iDecoder` **本身带顶层端口**
#      ⇒ 直接当 DUT（与 `test_hazard_complete_dual.cpp` 同型）⇒ 成本最低。
#    ⇒ 三个组件**纯组合**（`ch_reg` 计数 = 0）⇒ 不发射 `always_ff`
#      ⇒ **陷阱 T5 不适用**（无「codegen 有时序逻辑但时序表为空」的漂移）。
#    ⇒ 4 问 ①：三个符号在 58 个 dual 文件里各出现 **0** 次 ⇒ 无重复；
#      唯一既有覆盖是 **SoC 级** hand-crafted dual ⇒ **判别形态不同** ⇒ 逐案裁决通过。
#    ⇒ 价值：这些组件是 RV32I 微架构的**单一权威**（ADR-046），最近 3 个真缺陷
#      （MemStage 的 LB 掩码 · IdStage 的 U 型立即数 · DataTCM 的 strbe）全在这一层。
#    ⇒ 🔴 本批**第二个产出**：挖出 `Rv32iDecoder` 的 **B 型立即数 bit 11 被放在 bit 0**
#      （同一笔误也在生产路径 `id_stage.h:140-145`）⇒ |偏移| >= 2048 的条件分支跳错地址
#      ⇒ 独立立项 `.rddf/improvements/rv32i-b-type-immediate-bit11-misplaced-2026-11.md`
#      ⇒ ⇒ 本批**刻意不修**（行为变更 ⇒ 需 ADR）⇒ 钉住**当前行为**（修复时转红 = 有意的变更信号）。
add_dual_backend_test(test_rv32i_stage_dual
    SOURCES test_rv32i_stage_dual.cpp
    TIMEOUT 300
)

# ⚠️ ADR-063 D4 的等价性判据在 `test_rv32i_stage_dual.cpp` 里新增了一个 wrapper
#    （并排实例化 `Rv32iDecoder` + `IdStage`）⇒ 需要 include `stages/id_stage.h`
#    ⇒ 该头**非自包含**（`#include "rv32i_decoder.h"` 在上一级目录）⇒ 补 include 路径。
target_include_directories(test_rv32i_stage_dual PRIVATE
    ${PROJECT_SOURCE_DIR}/include/cpu/pipeline
)

# ⚠️ **路线 C 第 2 批 —— RV32I 的「时序侧」组件进双后端 parity**（2026-11-03）
#    ⇒ `IdStage`（**16** 个 ch_reg = ID/EX 流水线寄存器）· `Rv32iRegFile`（**32** 个
#      ch_reg = 仓内最大的 always_ff 面）· `DataTCM<8,32>`（ch_mem **aread** 0-cycle
#      + 读-改-写的字节合并）⇒ ⇒ 三个组件**都走 always_ff / mem 写口**
#      ⇒ ⇒ 正是两后端最易分叉的路径（与第 1 批的纯组合组件互补）。
#    ⇒ **零 DUT 复制**：三个组件本身带顶层端口 ⇒ 直接当 DUT。
#    ⇒ ⚠️ **刻意不做 `InstrTCM`**：它的 `sread` 时序在「地址是顶层输入端口」时
#      **不可判别**（陷阱 T2 ⇒ 0-cycle 与 1-cycle 观测值相同）⇒ ⇒ 唯一可判别的形态
#      需要「地址由同一条沿提交的 ch_reg 驱动」⇒ ⇒ 需要一层 wrapper ⇒ 不是零 DUT 复制。
#    ⇒ 4 问 ①：三个符号在 60 个 dual 文件里各出现 **0** 次 ⇒ 无重复。
add_dual_backend_test(test_rv32i_timing_dual
    SOURCES test_rv32i_timing_dual.cpp
    TIMEOUT 300
)

# ⚠️ `id_stage.h` **非自包含**（`#include "rv32i_decoder.h"` 在**上一级**目录
#    ⇒ 单独 include 会失败）⇒ ⇒ 需要 `include/cpu/pipeline` 在 include 路径上
#    ⇒ ⇒ 与既有 `test_rv32i_pipeline_behavior` **同款**处理
#    ⇒ ⇒ 该头文件的自包含缺陷已独立登记（不在本批修）。
target_include_directories(test_rv32i_timing_dual PRIVATE
    ${PROJECT_SOURCE_DIR}/include/cpu/pipeline
)

# axi-spi-behavior-tests: AxiLiteSpi behavior parity runtime test
# (Phase 3 partial of dual-backend-test-coverage). New file using DUAL_BACKEND_TEST_PAIR
# macro. Original tests/test_axi_spi.cpp (interface-structure) untouched.
add_dual_backend_test(test_axi_spi_behavior
    SOURCES test_axi_spi_behavior.cpp
    TIMEOUT 300
)

# dual-backend-axi4-peripherals: 2 个 AXI4-Lite peripheral (Pwm + I2c) behavior
# runtime dual 化 (per Phase 3 partial 续, _behavior suffix 区别原结构测)
add_dual_backend_test(test_axi_pwm_behavior
    SOURCES test_axi_pwm_behavior.cpp
    TIMEOUT 300
)
add_dual_backend_test(test_axi_i2c_behavior
    SOURCES test_axi_i2c_behavior.cpp
    TIMEOUT 300
)

# dual-backend-axi4-peripherals-extended-2026-10: Phase 3 收尾 —
# Timer / Gpio / Uart behavior parity runtime tests (6/6 AXI4-Lite slaves).
add_dual_backend_test(test_axi_timer_behavior
    SOURCES test_axi_timer_behavior.cpp
    TIMEOUT 300
)
add_dual_backend_test(test_axi_gpio_behavior
    SOURCES test_axi_gpio_behavior.cpp
    TIMEOUT 300
)
add_dual_backend_test(test_axi_uart_behavior
    SOURCES test_axi_uart_behavior.cpp
    TIMEOUT 300
)

# RISC-V Mini 组件测试
add_catch_test(test_branch_predictor_v2 chlib/test_branch_predictor_v2.cpp)
# dual-backend-test-coverage (Phase 1): 3 BranchPredictorV2 timing
# tests migrated to DUAL_BACKEND_TEST_PAIR.
set_tests_properties(test_branch_predictor_v2 PROPERTIES
    LABELS "chlib;verilator_e2e"
    RESOURCE_LOCK "verilator_backend"
    ENVIRONMENT "${_vl_path_env}"
    TIMEOUT 300
)

# W1: TC-08 JIT register path perf regression (perf-report-followup.md Task 1)
add_catch_test(perf_jit_reg benchmark/test_jit_reg_perf.cpp)
set_tests_properties(perf_jit_reg PROPERTIES LABELS "perf" TIMEOUT 120)
# W2: TC-07 JIT small-graph threshold (perf-report-followup.md Task 2)
add_catch_test(perf_jit_smallgraph benchmark/test_jit_smallgraph.cpp)
set_tests_properties(perf_jit_smallgraph PROPERTIES LABELS "perf" TIMEOUT 60)
# W3: build_us instrumentation (perf-report-followup.md Task 3)
add_catch_test(perf_build_us benchmark/test_build_us_present.cpp)
set_tests_properties(perf_build_us PROPERTIES LABELS "perf" TIMEOUT 30
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    FIXTURES_REQUIRED "perf_results_clean"
    DEPENDS "perf_tests;perf_three_way"
    RESOURCE_LOCK "perf_results_file")
# W7: md/csv/json byte-equality (perf-report-followup.md Task 7)
# CATCH_AMALGAMATED_CUSTOM_MAIN + CATCH_CONFIG_RUNNER disable the
# default main() in catch_amalgamated.cpp so the test can provide its
# own main() that returns 0 (success) when perf_results.{json,csv,md}
# are not present (i.e. perf_tests has not been run yet). Otherwise
# Catch2 exits with 4 (all tests skipped), which ctest reports as
# failure. Pattern matches perf_verilator_integration (commit 1524fd0)
# and test_verilog_external (ADR-035 / Phase 1.6).
add_catch_test(perf_report_consistency benchmark/test_report_consistency.cpp)
target_compile_definitions(perf_report_consistency PRIVATE
    CATCH_AMALGAMATED_CUSTOM_MAIN
)
set_tests_properties(perf_report_consistency PROPERTIES LABELS "perf" TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    DEPENDS "perf_tests;perf_subprocess_isolation;perf_three_way"
    FIXTURES_REQUIRED "perf_results_clean"
    RESOURCE_LOCK "perf_results_file")
# F2 (fix-perf-subprocess-isolation): regression test for K1 ORC JIT
# cross-DUT state pollution mitigation. Verifies that perf_tests --all
# TC-08 JIT is within 1.5x of standalone TC-08 JIT. Pattern matches
# perf_report_consistency: CATCH_AMALGAMATED_CUSTOM_MAIN + WORKING_DIRECTORY
# at project root so subprocess can locate build/tests/benchmark/perf_tests.
#
# Runtime note (measured 2026-06-19 on developer workstation): the test
# invokes `perf_tests --all` which spawns 9 child processes (TC-07/08/09 ×
# {10,100,1000}) plus runs TC-01/02/04/06/10/11 in-process. Total wall-clock
# is ~15 minutes, dominated by TC-09 (ch_uint<32> arith chain, depth=1000)
# and TC-11 (ch_uint<256> wide reg chain, regs=1000). TIMEOUT 1800 (30 min)
# provides ~2× safety margin over the measured 930 s; reduce to 1200 only on
# machines confirmed to run TC-09/TC-11 in <8 min combined.
add_catch_test(perf_subprocess_isolation
    benchmark/test_perf_subprocess_isolation.cpp)
target_compile_definitions(perf_subprocess_isolation PRIVATE
    CATCH_AMALGAMATED_CUSTOM_MAIN
)
set_tests_properties(perf_subprocess_isolation PROPERTIES LABELS "perf" TIMEOUT 1800
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    RESOURCE_LOCK "perf_results_file")
# W12 (perf-report-followup.md): structural JIT-vs-interpreter ratio gate.
# The absolute 50ms gate (perf_jit_smallgraph) and the 1.2x baseline gate
# (perf_regression) cannot catch the case where JIT silently degrades to
# interpreter-class speed. This test asserts same-DUT jit_us < interp_us
# on a combinational XOR chain, which native code emission must win.
# Two TEST_CASEs scale depth (500 strict / 1000 ratio>=2.0x) to confirm
# JIT scalability grows with design size.
add_catch_test(perf_jit_vs_interp_ratio benchmark/test_jit_vs_interp_ratio.cpp)
set_tests_properties(perf_jit_vs_interp_ratio PROPERTIES LABELS "perf" TIMEOUT 120)

# W9: perf regression gate (perf-report-followup.md Task 9)
# Runs perf_regression against the checked-in perf_baseline.json.
# Exits non-zero on >1.2x regression. Tagged 'perf' so CI can opt-in via
# ctest -L perf. The test uses the source-tree absolute paths so the
# baseline is found regardless of CWD; the current perf_results.json is
# expected to be in the worktree root (as produced by perf_main.cpp).
add_test(NAME perf_regression
    COMMAND perf_regression
    ${CMAKE_SOURCE_DIR}/tests/benchmark/perf_baseline.json
    ${CMAKE_SOURCE_DIR}/perf_results.json
)
set_tests_properties(perf_regression PROPERTIES
    LABELS "perf" TIMEOUT 60
    DEPENDS "perf_tests;perf_subprocess_isolation;perf_three_way"
    FIXTURES_REQUIRED "perf_results_clean"
    RESOURCE_LOCK "perf_results_file"
)

# W11: Verilator integration test (perf-report-followup.md followup).
# Tags: [perf][verilator][integration]. Skips when BUILD_VERILATOR=OFF
# or the Verilator build has not been run yet.
add_catch_test(perf_verilator_integration benchmark/test_verilator_integration.cpp)
target_compile_definitions(perf_verilator_integration PRIVATE
    CPPHDL_VERILATOR_WRAPPER="${CPPHDL_VERILATOR_WRAPPER}"
    CPPHDL_VERILATOR_BIN="${CPPHDL_VERILATOR_BIN}"
    CPPHDL_VERILATOR_DIR="${CPPHDL_VERILATOR_DIR}"
    CATCH_AMALGAMATED_CUSTOM_MAIN
)
set_tests_properties(perf_verilator_integration PROPERTIES
    LABELS "perf" TIMEOUT 60
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    RESOURCE_LOCK "perf_results_file"
)

# Stream 组件独立测试
add_catch_test(test_stream_mux_demux chlib/test_stream_mux_demux.cpp)

# Phase 4 集成测试 (chlib 版本)
add_catch_test(test_phase4_chlib chlib/test_phase4_integration.cpp)

# ============================================================================
# 根目录测试 (之前未注册)
# ============================================================================

# 基础功能测试
add_catch_test(test_basic test_basic.cpp)
add_catch_test(test_literal test_literal.cpp)
add_catch_test(test_literal_debug test_literal_debug.cpp)
add_catch_test(test_literal_left_shift test_literal_left_shift.cpp)
add_catch_test(test_literal_width test_literal_width.cpp)
add_catch_test(test_reg test_reg.cpp)
add_catch_test(test_reg_linking test_reg_linking.cpp)
add_catch_test(test_reg_timing test_reg_timing.cpp)
# dual-backend-test-coverage (Phase 1): reg_timing timing tests migrated
# to DUAL_BACKEND_TEST_PAIR. Each TEST_CASE produces a [verilator]
# half that runs real verilator compile + dlopen.
set_tests_properties(test_reg_timing PROPERTIES
    LABELS "base;verilator_e2e"
    RESOURCE_LOCK "verilator_backend"
    ENVIRONMENT "${_vl_path_env}"
    TIMEOUT 300
)
add_catch_test(test_operator test_operator.cpp)
add_catch_test(test_operator_advanced test_operator_advanced.cpp)
add_catch_test(test_operator_connection test_operator_connection.cpp)
add_catch_test(test_operator_results test_operator_results.cpp)

# IO 测试
add_catch_test(test_simple_io test_simple_io.cpp)
add_catch_test(test_io_operations test_io_operations.cpp)
add_catch_test(test_io_extended_ops test_io_extended_ops.cpp)
add_catch_test(test_source_info test_source_info.cpp)

# Bundle 测试
add_catch_test(test_bundle test_bundle.cpp)
add_catch_test(test_bundle_large test_bundle_large.cpp)
add_catch_test(test_bundle_advanced test_bundle_advanced.cpp)
add_catch_test(test_bundle_advanced_features test_bundle_advanced_features.cpp)
add_catch_test(test_bundle_bitstream test_bundle_bitstream.cpp)
add_catch_test(test_bundle_connection test_bundle_connection.cpp)
add_catch_test(test_bundle_node_management test_bundle_node_management.cpp)
add_catch_test(test_bundle_serialization test_bundle_serialization.cpp)
add_catch_test(test_nested_bundle test_nested_bundle.cpp)
add_catch_test(test_pod_bundle_conversions test_pod_bundle_conversions.cpp)
add_catch_test(test_simulator_bundle test_simulator_bundle.cpp)
add_catch_test(test_tlm_bundle_phase1 test_tlm_bundle_phase1.cpp)

# Stream 测试
add_catch_test(test_ch_stream test_ch_stream.cpp)
# dual-backend-test-coverage (Phase 1): 2 ch_stream timing tests
# (SimulationDataFlow + SimulationDataConsumption) migrated to
# DUAL_BACKEND_TEST_PAIR.
set_tests_properties(test_ch_stream PROPERTIES
    LABELS "base;verilator_e2e"
    RESOURCE_LOCK "verilator_backend"
    ENVIRONMENT "${_vl_path_env}"
    TIMEOUT 300
)
add_catch_test(test_stream_arbiter test_stream_arbiter.cpp)
add_catch_test(test_stream_builder test_stream_builder.cpp)
add_catch_test(test_stream_operators test_stream_operators.cpp)
add_catch_test(test_stream_pipeline test_stream_pipeline.cpp)

# Memory 测试（核心功能已由 test_mem/test_mem_connection/test_mem_timing 覆盖）
add_catch_test(test_mem test_mem.cpp)
add_catch_test(test_mem_connection test_mem_connection.cpp)
add_catch_test(test_mem_timing test_mem_timing.cpp)
add_catch_test(test_mem_comb_read test_mem_comb_read.cpp)
# 已删除：以下测试使用过时 API（ChComponent/TestComponent），且测试内部实现细节
# - test_memory_leak.cpp
# - test_memory_optimization.cpp
# - test_memory_performance.cpp

# 组件测试
add_catch_test(test_state_machine test_state_machine.cpp)
# test_pipeline_regs.cpp deleted in cleanup-orphan-code (commit B)
# The PipelineReg Component coverage lives in:
#   - tests/chlib/test_forwarding.cpp  (chlib::ForwardingMux + IfIdRegs struct field access)
#   - cpu/pipeline/rv32i_pipeline.h  (full 5-stage integration via ch::ch_device<Rv32iSoc>)
# The deleted file was a 2-TEST_CASE compile-only smoke that hit 0 assertions
# (only `(void)reg;` discard). See openspec/changes/archive/2026-10-15-cleanup-orphan-code/proposal.md.
add_catch_test(test_fragment test_fragment.cpp)

# AXI 测试
add_catch_test(test_axi_dma test_axi_dma.cpp)
# 字节道（WSTRB）写语义 —— axi4-peripheral-deferred-2026-10 D1
# 双后端：窄寄存器截断（ch_uint<8>/<16>）在 Verilog codegen 路径上与
# interpreter 不同，必须跑 verilator 半边。
add_dual_backend_test(test_axi_byte_lane
    SOURCES test_axi_byte_lane.cpp
    TIMEOUT 300
)
# 通用 AXI slave 的字节道写语义（AxiLiteSlave / Axi4Slave）—— axi4-slave-byte-lane-2026-10
# 双后端：byte_lane_mask<DATA_WIDTH> 的泛化掩码展开 + 64 bit 总线宽度
# 在 Verilog codegen 路径上与 interpreter 差异最大。
add_dual_backend_test(test_axi_slave_byte_lane
    SOURCES test_axi_slave_byte_lane.cpp
    TIMEOUT 300
)
# 通用 AXI4-Lite slave 的协议规范形态回归（rst_n / AW→W 相位分离 / 地址锁存 /
# 响应保持 / 未映射 DECERR / 别名禁止）——双后端。
add_dual_backend_test(test_axi_lite_slave_conformance
    SOURCES test_axi_lite_slave_conformance.cpp
    # ⚠️ 450（原 300）：冷缓存实测 **224.4 s** ⇒ 300 只剩 1.34× 余量
    #    ⇒ 按「TIMEOUT ≥ 2× 冷实测」定档（30 s 的倍数）。
    TIMEOUT 450
)
# 通用 AXI4 Full slave 的多 beat burst 语义回归（INCR / FIXED / WRAP→SLVERR /
# 逐 beat wstrb / 越界 DECERR / 超 MAX_BURST_LEN→SLVERR / rst_n 复位）——双后端。
add_dual_backend_test(test_axi4_full_burst
    SOURCES test_axi4_full_burst.cpp
    TIMEOUT 300
)
# 「未接线 rst_n = 永久复位」的可执行护栏（零编译错误的静默破坏）——双后端。
add_dual_backend_test(test_axi_rst_n_guard
    SOURCES test_axi_rst_n_guard.cpp
    TIMEOUT 300
)
# 4x4 AXI 互连的 16 对全路由 / 路由锁存 / 独立仲裁 / DECERR error responder / rst_n
# ——双后端。TB 同时扮演 master 与 slave（MUST NOT 实例化 Axi4Slave 当 fixture）。
add_dual_backend_test(test_axi_interconnect_4x4
    SOURCES test_axi_interconnect_4x4.cpp
    # ⚠️ 930（原 600）：冷 verilator 缓存下实测 **453.1 s**（warm 281–284 s）
    #    ⇒ 600 只剩 1.3× 余量，而 CI runner（2 核）比本机（4 核）慢
    #    ⇒ 按「TIMEOUT ≥ 2× 冷实测」定档（30 s 的倍数）。
    #    数据：.rddf/checks/ci-wallclock-cold-vs-warm-2026-11.md
    TIMEOUT 930
)
# 2x2 AXI-Lite 矩阵的活地址解码修正（读侧 ar_route 锁存 / s*_rready 用读路由 /
# awready 在途门控 / 独立仲裁 / rst_n / DECERR）——双后端。
add_dual_backend_test(test_axi_lite_matrix_2x2
    SOURCES test_axi_lite_matrix_2x2.cpp
    TIMEOUT 600
)
add_catch_test(test_axi_gpio test_axi_gpio.cpp)
add_catch_test(test_axi_i2c test_axi_i2c.cpp)
add_catch_test(test_axi_lite_bundle test_axi_lite_bundle.cpp)
add_catch_test(test_axi_pwm test_axi_pwm.cpp)
add_catch_test(test_axi_pwm_simple test_axi_pwm_simple.cpp)
add_catch_test(test_axi_spi test_axi_spi.cpp)
add_catch_test(test_axi_timer test_axi_timer.cpp)
add_catch_test(test_axi_uart test_axi_uart.cpp)

# 运算测试
add_catch_test(test_bits_update test_bits_update.cpp)
add_catch_test(test_bitvector test_bitvector.cpp)
add_catch_test(test_bool_connection test_bool_connection.cpp)
add_catch_test(test_comparison_ops test_comparison_ops.cpp)
add_catch_test(test_uint_compare test_uint_compare.cpp)
# dual-backend-test-coverage (Phase 1): 3 ch_uint compare tests migrated
# to DUAL_BACKEND_TEST_PAIR.
set_tests_properties(test_uint_compare PROPERTIES
    LABELS "base;verilator_e2e"
    RESOURCE_LOCK "verilator_backend"
    ENVIRONMENT "${_vl_path_env}"
    TIMEOUT 300
)
add_catch_test(test_mod_width test_mod_width.cpp)
add_catch_test(test_popcount test_popcount.cpp)
add_catch_test(test_precise_width test_precise_width.cpp)
add_catch_test(test_shift_boundary test_shift_boundary.cpp)
add_catch_test(test_sdata_compare test_sdata_compare.cpp)

# FIFO 示例
add_catch_test(test_fifo_example test_fifo_example.cpp)

# Flow / Stream Bundle 测试
add_catch_test(test_ch_flow test_ch_flow.cpp)
add_catch_test(test_stream_width_adapter test_stream_width_adapter.cpp)

# 其它测试
add_catch_test(test_blt_debug test_blt_debug.cpp)

# ⚠️⚠️ **A5 试点**（2026-11-01 · change `dual-backend-parity-batch-2026-11`）
#    ⇒ ⇒ 双后端 parity 的**第 1 个新批次**（组合逻辑 ⇒ 无 clk 风险）
#    ⇒ ⇒ **新增 `_behavior` 文件** ⇒ ⇒ 原 `test_blt_debug` MUST NOT 修改
#    ⇒ ⇒ （仓内既有 pattern：`_behavior` 后缀）
add_dual_backend_test(test_blt_debug_behavior
    SOURCES test_blt_debug_behavior.cpp
)
add_catch_test(test_bv_cmp test_bv_cmp.cpp)
add_catch_test(test_bv_truncate test_bv_truncate.cpp)
add_catch_test(test_ch_reg_bitwise test_ch_reg_bitwise.cpp)
# component naming (hierarchical RTL T3-①a): path uniqueness + two naming dimensions
add_catch_test(test_component_naming test_component_naming.cpp)

# component ownership (hierarchical RTL T3-①b): construction scope stack, member
# ch_module parent/ctx, cross-ctx guard, deferred child build, io.h direction guards
add_catch_test(test_component_ownership test_component_ownership.cpp)

# component granularity (hierarchical RTL T3-①c): class-level CH_HIERARCHY_LAYER vs
# instance-level ch_layer / ch_inline, four-tier module type name, assembly without an
# active parent (declaration decoupled from assembly point)
add_catch_test(test_component_granularity test_component_granularity.cpp)

# public equivalence helper (p8-hierarchical-equivalence-public-helper-2026-11):
# out-of-tree consumer of the PUBLIC header only (proves the public surface is
# self-sufficient: compiles + links + runs, not just -fsyntax-only).
# ⚠️ 它跑**真 verilator**（编译扁平产物 + 层次化产物两份）⇒ ⇒ MUST 带
#    `verilator_e2e` 标签 + `RESOURCE_LOCK` ⇒ ⇒ **MUST NOT** 用 `add_catch_test`
#    （那样它会落进「非 verilator」分片 ⇒ ⇒ CI 的 verilator 腿跑不到它）。
add_dual_backend_test(test_public_equivalence_api
    SOURCES test_public_equivalence_api.cpp
    TIMEOUT 600
)

# hierarchy friendliness diagnosis (p6-hierarchy-friendliness-diagnosis-2026-11):
# read-only diagnosis entry point; 5 machine-decidable report items; "unmeasured" vs
# "measured zero" must be structurally distinguishable; synthesized-port count must
# equal the emitter's implicit-synthesized port count; deterministic snapshot.
add_catch_test(test_hierarchy_friendliness_diagnosis
    test_hierarchy_friendliness_diagnosis.cpp)

# io() const + opt-in node-name uniqueness (component-io-const-and-node-name-policy-2026-11):
# const contexts can read impl()/name()/is_valid(); ctx node-name policy defaults to
# kAllowDuplicates (zero behaviour change) and kRejectDuplicates fails fast at the
# creation point (with a dedicated duplicate-name DUT, plus the static-destruction
# degradation path).
add_catch_test(test_component_io_const_and_node_name_policy
    test_component_io_const_and_node_name_policy.cpp)

# hierarchy analysis (hierarchical RTL T3-②): module list / port table / connection
# table + three pre-checks. Golden files live in tests/golden/hierarchy/ and are
# compared byte-for-byte (set UPDATE_GOLDEN=1 to regenerate, then verify by hand).
add_catch_test(test_hierarchy_analysis test_hierarchy_analysis.cpp)
target_compile_definitions(test_hierarchy_analysis PRIVATE
    CPPHDL_GOLDEN_DIR="${PROJECT_SOURCE_DIR}/tests/golden/hierarchy")
# module emitter split (hierarchical RTL T3-③): verilogwriter delegates to
# ModuleEmitter + make_flat_module_view (byte-identical), and clock/reset names
# are parameterized so sub-modules never reference the RTL-top default clocks.
add_catch_test(test_verilog_module_emitter test_verilog_module_emitter.cpp)
# hierarchical emission (hierarchical RTL T3-④): 3 modules + 2 instances, top ports
# == flat ports, per-module clock/reset decision, determinism, error model.
add_catch_test(test_hierarchy_verilog test_hierarchy_verilog.cpp)

# 层次化 ≡ 扁平 的**逐拍等价** harness（T3-⑤ · ADR-058 §2.2）
# ⚠️ 依赖真 verilator ⇒ 与既有 e2e 套件同构：BUILD_VERILATOR=OFF 时随标签排除。
# ⚠️ harness 本体在 tests/hier_equivalence.h（**不暴露公共 API** · P8 独立立项）。
add_catch_test(test_hierarchy_equivalence test_hierarchy_equivalence.cpp)
set_tests_properties(test_hierarchy_equivalence PROPERTIES
    LABELS "base;verilator_e2e"
    RESOURCE_LOCK "verilator_backend"
    ENVIRONMENT "${_vl_path_env}"
)
# 探针矩阵（B4）的「每格都有断言」判据需要**本文件自身**的源码来做 tag 索引校验
target_compile_definitions(test_hierarchy_verilog PRIVATE
    HIER_TEST_SOURCE="${CMAKE_CURRENT_SOURCE_DIR}/test_hierarchy_verilog.cpp")

# 层次化 RTL 产物生成器 —— **不是 ctest 用例**（`CTEST_TOTAL` 锚点 MUST NOT 变）。
# 供 `scripts/verify_hierarchical_rtl.sh` 在真实进程里生成产物后交给 verilator 判。
add_executable(hier_rtl_products hier_rtl_products.cpp)
target_include_directories(hier_rtl_products PRIVATE ${PROJECT_SOURCE_DIR}/include)
if(TARGET cpphdl)
    target_link_libraries(hier_rtl_products cpphdl)
endif()

add_catch_test(test_module test_module.cpp)
# dual-backend-test-coverage (Phase 1): 3 CH_MODULE timing tests
# migrated to DUAL_BACKEND_TEST_PAIR.
set_tests_properties(test_module PROPERTIES
    LABELS "base;verilator_e2e"
    RESOURCE_LOCK "verilator_backend"
    ENVIRONMENT "${_vl_path_env}"
    TIMEOUT 300
)
add_catch_test(test_module_advanced test_module_advanced.cpp)
add_catch_test(test_multithread test_multithread.cpp)
add_catch_test(test_operation_results test_operation_results.cpp)
add_catch_test(test_operation_widths test_operation_widths.cpp)
add_catch_test(test_rv32i_pipeline test_rv32i_pipeline.cpp)
# dual-backend-rv32i-pipeline-behavior-tests: RV32I microarchitecture 子组件
# dual 行为 parity. _behavior suffix 区别原 test_rv32i_pipeline.cpp (per spec R6 #5
# SECTION 排除 + R7 "原文件不动"). 原文件 MUST NOT 改动.
add_dual_backend_test(test_rv32i_pipeline_behavior
    SOURCES test_rv32i_pipeline_behavior.cpp
    # ⚠️ 420（原 300）：冷缓存隔离复测 **204 s**（warm 远小于此）
    #    ⇒ 300 只剩 1.47× 余量 ⇒ 按「TIMEOUT ≥ 2× 冷实测」定档。
    TIMEOUT 420
)
# `stages/if_stage.h`（Batch 4 的 DUT）用**相对**路径 include 兄弟头
# (`#include "rv32i_tcm.h"`) ⇒ 该测试必须把 include/cpu/pipeline/ 放进
# 搜索路径（同 `test_verilator_complex_e2e` 的处理）。
target_include_directories(test_rv32i_pipeline_behavior PRIVATE
    ${PROJECT_SOURCE_DIR}/include/cpu/pipeline
)
# test_simplified_creation 已删除 — easy_nodes.h 不再存在
add_catch_test(test_trace test_trace.cpp)
add_catch_test(test_user_tracking test_user_tracking.cpp)
add_catch_test(test_verilog_arith test_verilog_arith.cpp)
add_catch_test(test_verilog_compare test_verilog_compare.cpp)
add_catch_test(test_verilog_gen test_verilog_gen.cpp)
# Inject verilator bin into PATH so detect_verilator() in test_verilog_gen.cpp
# resolves via `which verilator` instead of falling back to the relative
# "build/verilator-install/bin/verilator" candidates (which silently fail
# under ctest's WORKING_DIRECTORY=project root when invoked from a different
# cwd, e.g. a developer running the binary by hand from build/tests/).
# Same ENV injection pattern as the other verilator_* test targets above.
set_tests_properties(test_verilog_gen PROPERTIES
    ENVIRONMENT "${_vl_path_env}"
)
add_catch_test(test_verilog_shift test_verilog_shift.cpp)
# ADR-035 / Phase 1.6: end-to-end validation of generated Verilog
# via external tools (iverilog + verilator --lint-only). Tests skip
# cleanly if either tool is missing on the host.
# CATCH_AMALGAMATED_CUSTOM_MAIN + CATCH_CONFIG_RUNNER disable the
# default main() in catch_amalgamated.cpp so the test can provide its
# own main() that returns 0 (success) when neither iverilog nor
# verilator is installed. Otherwise Catch2 exits with 4 (all tests
# skipped), which ctest reports as failure.
add_catch_test(test_verilog_external test_verilog_external.cpp)
target_compile_definitions(test_verilog_external PRIVATE
    CATCH_AMALGAMATED_CUSTOM_MAIN
)
# ADR-035 / Phase 4.1: VerilatorBackend scaffolding tests
# (SHA-1 cache key, IEvalBackend interface, Verilog generation,
# verilator invocation, resource cleanup).
add_catch_test(test_verilator_backend test_verilator_backend.cpp)
set_tests_properties(test_verilator_backend PROPERTIES
    LABELS "base;verilator_e2e"
    RESOURCE_LOCK "verilator_backend"
    ENVIRONMENT "${_vl_path_env}"
)
# ADR-035 issue #25 / Phase 4.1+: e2e harness — real Verilator compile
# + dlopen + counter verification (closes R5/R7 in tasks v2).
add_catch_test(test_verilator_e2e_harness test_verilator_e2e_harness.cpp)
set_tests_properties(test_verilator_e2e_harness PROPERTIES
    LABELS "base;verilator_e2e"
    RESOURCE_LOCK "verilator_backend"
    ENVIRONMENT "${_vl_path_env}"
)
# strengthen-verilator-e2e-coverage phase 1: trace parity single-test for
# 3 fixtures (counter / AXI4-Lite / bundle multi-field). Skips cleanly
# when verilator is not on PATH; CATCH_AMALGAMATED_CUSTOM_MAIN keeps exit
# code 0 when ALL tests SKIP (otherwise Catch2 exits with 4 = failure).
add_catch_test(test_verilator_trace_parity test_verilator_trace_parity.cpp)
target_compile_definitions(test_verilator_trace_parity PRIVATE
    CATCH_AMALGAMATED_CUSTOM_MAIN
)
set_tests_properties(test_verilator_trace_parity PROPERTIES
    LABELS "base;verilator_e2e"
    RESOURCE_LOCK "verilator_backend"
    ENVIRONMENT "${_vl_path_env}"
)
# strengthen-verilator-e2e-coverage phase 2: bundle e2e fixtures
# (single-layer / nested / array / chain × 4 scenarios).
add_catch_test(test_verilator_bundle_e2e test_verilator_bundle_e2e.cpp)
target_compile_definitions(test_verilator_bundle_e2e PRIVATE
    CATCH_AMALGAMATED_CUSTOM_MAIN
)
set_tests_properties(test_verilator_bundle_e2e PROPERTIES
    LABELS "base;verilator_e2e"
    RESOURCE_LOCK "verilator_backend"
    ENVIRONMENT "${_vl_path_env}"
)
# strengthen-verilator-e2e-coverage phase 3: stream/fifo e2e fixtures
# (handshake single + backpressure multi × 2 scenarios).
add_catch_test(test_verilator_stream_e2e test_verilator_stream_e2e.cpp)
target_compile_definitions(test_verilator_stream_e2e PRIVATE
    CATCH_AMALGAMATED_CUSTOM_MAIN
)
set_tests_properties(test_verilator_stream_e2e PROPERTIES
    LABELS "base;verilator_e2e"
    RESOURCE_LOCK "verilator_backend"
    ENVIRONMENT "${_vl_path_env}"
)
# strengthen-verilator-e2e-coverage phase 6: memory e2e fixtures
# (sync_fifo + axi4_full burst × 2 scenarios). AxiFull is a heavier
# design so we set an explicit TIMEOUT 300s (add_catch_test does not
# support TIMEOUT; must use set_tests_properties).
add_catch_test(test_verilator_memory_e2e test_verilator_memory_e2e.cpp)
target_compile_definitions(test_verilator_memory_e2e PRIVATE
    CATCH_AMALGAMATED_CUSTOM_MAIN
)
set_tests_properties(test_verilator_memory_e2e PROPERTIES
    LABELS "base;verilator_e2e"
    RESOURCE_LOCK "verilator_backend"
    ENVIRONMENT "${_vl_path_env}"
    TIMEOUT 300
)
# strengthen-verilator-e2e-coverage phase 7: complex e2e fixtures
# (rv32i_pipeline + spi_fsm × 2 scenarios). TIMEOUT 600s covers cold
# verilator compile of complex designs (rough memory max cold ~3-5 min).
add_catch_test(test_verilator_complex_e2e test_verilator_complex_e2e.cpp)
target_compile_definitions(test_verilator_complex_e2e PRIVATE
    CATCH_AMALGAMATED_CUSTOM_MAIN
)
set_tests_properties(test_verilator_complex_e2e PROPERTIES
    LABELS "base;verilator_e2e"
    RESOURCE_LOCK "verilator_backend"
    ENVIRONMENT "${_vl_path_env}"
    TIMEOUT 600
)
# RV32I pipeline headers (`stages/if_stage.h` etc.) use relative
# includes for sibling files (`rv32i_tcm.h`); the test must have
# include/cpu/pipeline/ on its path so `#include "stages/if_stage.h"`
# resolves and the stage's `#include "rv32i_tcm.h"` lands.
target_include_directories(test_verilator_complex_e2e PRIVATE
    ${PROJECT_SOURCE_DIR}/include/cpu/pipeline
)
# verilator-std-expected: VerilatorError type contract + bool wrapper
# backward compat + source_location capture. No verilator toolchain
# required — exercises the type API and the try_initialize() rich-error
# return path against VerilogGeneration (unwritable work_dir).
add_catch_test(test_verilator_error_flow test_verilator_error_flow.cpp)
set_tests_properties(test_verilator_error_flow PROPERTIES
    LABELS "base;verilator_e2e"
    RESOURCE_LOCK "verilator_backend"
    ENVIRONMENT "${_vl_path_env}"
)
add_catch_test(test_wires_connection test_wires_connection.cpp)
# JIT compiler tests
add_catch_test(test_jit_compiler test_jit_compiler.cpp)
add_catch_test(test_jit_concat test_jit_concat.cpp)
# Phase 0 of c-class-major-refactor: golden output regression safety net
# (locks interpreter vs JIT byte-identical behavior before JIT split in
# Phase 1). Tagged [jit][golden]. See .omo/plans/c-class-major-refactor.md
# Task 1.
add_catch_test(test_jit_golden jit/test_jit_golden.cpp)
# Regression: interpreter and JIT must produce byte-identical results for
# ADD/SUB/MUL/SHL/NEG width overflow cases (locks interpreter-arith-bug-fix).
add_catch_test(test_interpreter_jit_compat test_interpreter_jit_compat.cpp)
# Per openspec/changes/2026-09-20-upgrade-llvm-22-jit-compat:
# LLVM version compatibility red-line tests. These assert that
# JitCompiler::is_available() returns true on detected LLVM 17-22 and
# gracefully SUCCEED-skip otherwise. The `#if defined(CH_JIT_ENABLED)`
# guard inside the .cpp files handles the CH_JIT_ENABLE=OFF PR-feedback
# matrix (no link error when JitCompiler is not built).
add_catch_test(test_jit_llvm_version jit/test_jit_llvm_version.cpp)
add_catch_test(test_jit_init_succeeds jit/test_jit_init_succeeds.cpp)

# ============================================================================
# SpinalHDL 移植示例 CTest 注册
# 所有 17 个移植示例均编译通过，通过运行可执行文件验证功能
# ============================================================================

function(add_spinalhdl_ported_test TEST_NAME TARGET_NAME)
    add_test(NAME spinalhdl_${TEST_NAME} COMMAND $<TARGET_FILE:${TARGET_NAME}>)
    set_tests_properties(spinalhdl_${TEST_NAME} PROPERTIES LABELS "spinalhdl-ported" TIMEOUT 30)
endfunction()

# Phase 1: 基础移植验证
add_spinalhdl_ported_test(counter counter_example)
add_spinalhdl_ported_test(counter_simple counter_simple_example)
add_spinalhdl_ported_test(fifo fifo_example)
add_spinalhdl_ported_test(uart_tx uart_tx_example)
add_spinalhdl_ported_test(uart_rx uart_rx_example)
add_spinalhdl_ported_test(pwm pwm_example)
add_spinalhdl_ported_test(gpio gpio_example)
add_spinalhdl_ported_test(spi spi_controller_example)
add_spinalhdl_ported_test(i2c i2c_controller_example)
add_spinalhdl_ported_test(timer timer_example)
add_spinalhdl_ported_test(vga vga_controller_example)
add_spinalhdl_ported_test(ws2812 ws2812_controller_example)
# ws2812 takes ~20s in default parallel but exceeds 30s under heavy -j$(nproc) load.
# Increase to 120s to prevent spurious "Subprocess aborted" in CI heavy-parallel runs.
set_tests_properties(spinalhdl_ws2812 PROPERTIES TIMEOUT 120)
add_spinalhdl_ported_test(quadrature_encoder quadrature_encoder_example)
add_spinalhdl_ported_test(sigma_delta_dac sigma_delta_dac_example)
add_spinalhdl_ported_test(width_adapter width_adapter_example)
add_spinalhdl_ported_test(assert assert_example)
add_spinalhdl_ported_test(phase1c phase1c_test)

# ============================================================================
# AXI4 总线示例 CTest 注册
# 4 个示例：AXI4-Lite, AXI4 Full, AXI SoC, Phase 3A 集成
# ============================================================================

add_spinalhdl_ported_test(axi4_lite axi4_lite_example)
add_spinalhdl_ported_test(axi4_full axi4_full_example)
add_spinalhdl_ported_test(axi4_soc axi_soc_example)
add_spinalhdl_ported_test(axi4_phase3a phase3a_complete)

# ============================================================================
# RISC-V Mini 示例 CTest 注册
# 3 个活跃测试（Phase 1/2/3）通过 100%，2 个文件因 API 变更被禁用
# ============================================================================

add_spinalhdl_ported_test(rv32i_phase1 rv32i_phase1_test)
add_spinalhdl_ported_test(rv32i_phase2 rv32i_phase2_test)
add_spinalhdl_ported_test(rv32i_phase3 rv32i_phase3_test)
add_spinalhdl_ported_test(branch_predictor_v2 test_branch_predictor_v2)

# ============================================================================
# Performance Benchmarks
# ============================================================================
add_subdirectory(benchmark)



