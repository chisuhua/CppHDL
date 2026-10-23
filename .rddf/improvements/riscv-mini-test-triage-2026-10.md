---
name: riscv-mini-test-triage-2026-10
priority: P0
source: objective-riscv-mini-convergence §10（2026-10-23 roadmap 重梳理）
phase: phase-5
category: testing
type: feature
created_at: 2026-10-23
---

# Improvement: riscv-mini 19 个「编译但从不执行」target 清零 + 7 orphan 测试定性

- **Date drafted**: 2026-10-23
- **Drafted by**: build-agent (planner role)
- **Status**: 📋 Handoff document — 5-segment draft，等待 rdd-builder 启动
- **Objective**: `objective-riscv-mini-convergence`（C1，无依赖，可立即启动）

## 1. Why (背景与动机)

`examples/riscv-mini/CMakeLists.txt` 注册了 19 个 Catch2 target，但 `grep -rn "add_test" examples/`
**恒为 0**（仅注释命中）⇒ 它们每次 CI 都编译、却从未被执行（已登记在 `examples/disabled_examples.txt`）。
2026-10-23 手工运行 22 个二进制实测：

```
exit=139 (SIGSEGV): test_pipeline_minimal / test_pipeline_debug / test_pipeline_debug4 /
                    test_pipeline_debug5 / test_rv32i_pipeline_device / test_address_decoder
exit=44 (44/49 断言失败): test_riscv_tests
exit=0: 其余 15 个
```

**Why now**：
1. 这是 `objective-riscv-mini-convergence` 的第一块（D4 覆盖假象）——「消除技术债」无法绕过。
2. **它是 C2（收敛）的回归护栏前置** —— C2 会大改 include 路径，若没有这批测试先变绿并注册进 CTest，
   C2 期间「我改坏了」与「本来就红」无法归因。

**根因（预诊断，实施时验证）**：
- 6 个 SIGSEGV 中至少 `test_pipeline_minimal.cpp:9-13` 的 `ch::ch_module<Rv32iPipeline>` 直接放 TEST_CASE
  顶层 ⇒ 命中根 `AGENTS.md §COMPONENT LIFECYCLE PATTERNS` 的反模式（`ch_module` 必须置于 `describe()` 内，
  否则 silent SIGSEGV）。修法 = 改 `ch::ch_device<T>`。
- `test_riscv_tests` 44/49 失败是**真功能失败**（riscv-mini 参考解释器 vs RTL 分歧），需独立裁决。

## 2. What Changes (变更范围)

### C1-a 绿的 15 个注册进 CTest
- 为 15 个 exit-0 的 target 补 `add_test`（或按 `add_catch_test` 风格接进根 `tests/CMakeLists.txt`）。
- `examples/disabled_examples.txt` 对应 19 行移除（归位）。

### C1-b 红的 7 个修或删（带理由登记）
| target | 现状 | 裁决方向（实施时可调整，须写理由） |
|---|---|---|
| test_pipeline_minimal / debug / debug4 / debug5 | `ch_module` 放 TEST_CASE 顶层 SIGSEGV | 修：`ch_module`→`ch_device`（若仍红则删并登记） |
| test_rv32i_pipeline_device | exit 139（`ch_device<Rv32iPipeline>` construct-only） | 修或删（同型探针，若 C2 收敛后要重写，可删） |
| test_address_decoder | exit 139 | 修或删 |
| test_riscv_tests | 44/49 断言失败 | **独立裁决**：真 RTL 分歧，建议单独立项或整体删（登记根因） |

### C1-c 7 个 orphan 测试源文件定性
未注册到任何 target 的 `examples/riscv-mini/tests/*.cpp`：
`test_pipeline_debug2` / `test_hazard_minimal` / `test_interrupt_flow`（**从未编译**）、
`test_csr_unit`（已注册为根 `tests/test_csr_unit`）/ `test_riscv_tests_soc`（已注册为 `test_address_decoder`）/
`test_cache_smoke`（已注册为 `riscv_mini_cache_smoke`）/ `test_rv32i_pipeline`（example 版 203 行是根
`tests/test_rv32i_pipeline.cpp` 545 行的**过期副本**，md5 不同）。
→ 前 3 个「修进 CTest 或删」；`test_rv32i_pipeline`（example 副本）**删**（有权威根副本）。

### C1-d 新护栏
- 新增 `scripts/verify_example_test_wiring.sh`（命名对齐 `verify_rst_n_wiring.sh` 范式）：
  每个 `examples/*/tests/test_*.cpp` 要么被注册进 CTest、要么登记在 `examples/disabled_examples.txt`
  （带归位条件），否则 exit 1；挂进 CI 对应 job。

## 3. Acceptance (验收标准)

### 3.1 编译 + 测试 (PHASE GATES 🟢)
- [ ] `cmake --build build -j$(nproc)` 0 error 0 warning
- [ ] 19 个原「不执行」target 全部进 CTest，且 exit=0 的保持绿
- [ ] 修好的红 target 断言真实通过（不是 `REQUIRE(true)` 伪装）

### 3.2 覆盖守恒（护栏自身的盲区）
- [ ] `bash scripts/verify_example_test_wiring.sh` 绿（含对已删 orphan 的确认）
- [ ] `ctest -N | tail -1` 与 root `AGENTS.md` `CTEST_TOTAL` 一致（新增的注册数 MUST 同步 NOTES）

### 3.3 文档同步
- [ ] `examples/disabled_examples.txt` 移除已归位行；新删的文件登记带理由
- [ ] root `AGENTS.md` NOTES §Test counts 同步

## 4. Capabilities (涉及的能力维度)

| Capability | 影响 | 文档 |
|------------|------|------|
| `example-runtime-coverage` | 运行护栏扩展到 riscv-mini 测试 target | `openspec/specs/example-runtime-coverage/spec.md` MODIFIED |
| （新）`riscv-mini-test-wiring` | 19 target 注册 + orphan 定性 | 若 archive 时合并进 `example-runtime-coverage` 则在 delta 标注 |

## 5. Impact (影响面分析)

### 5.1 风险

| 风险 | 等级 | 缓解 |
|------|------|------|
| 修红 target 时牵出框架层 bug（如 `ch_device` 仍 SIGSEGV） | 🟡 中 | 修不动的单个 target 先删 + 登记（C1 是「清零」不是「全修」）；框架 bug 单独立项 |
| `test_riscv_tests` 的 RTL 分歧是真 bug | 🟡 中 | 不在 C1 修；登记为 C3 的前置发现或独立 change |
| 新增 CTest 注册数改变 CTEST_TOTAL | 🟢 低 | 同步 NOTES + `verify_test_counts.sh` |
| 与 C2 并行冲突（都改 examples/riscv-mini/CMakeLists.txt） | 🟡 中 | **串行：C1 先 archive，再启 C2** |

### 5.2 明确不做（Non-Goals）
- 不改任何 RV32I 实现语义（hazard 前推等留给 C2 裁决）
- 不收敛 include 路径（C2 的事）
- 不做 e2e parity（C3 的事）
