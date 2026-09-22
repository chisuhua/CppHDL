# Issue #25 — VerilatorBackend eval/sync 路径修复

## Why

**Issue #25 的真实范围被低估**。原报告称"always_ff reg 不传播到 data_map_",但 Oracle 审计(C12)+ 本会话实证显示:**整个 `sync_outputs_from_vtop` 路径都有问题**,不只 sequential reg 更新:

| 路径 | 现状 | 期望(per codegen 语义) | 实测 |
|------|------|---------------------|------|
| `assign top_slave_unnamed_output = top_slave_mux_select_1;` (组合 wire) | sync_outputs_from_vtop 读回 → data_map_ | `awready == 1` 当 awvalid=1 且 busy=0 | **`awready == 0`** ❌ |
| `always_ff @(posedge clk) top_slave_reg <= ...` (sequential reg) | 同上 | reg 更新值 | **`top_slave_reg == 0`**(issue #25 原文) ❌ |

**根因假设**(`src/core/verilator_backend.cpp:696-711` 的 `eval_sequential()` 注释描述但未实现的 3 步时钟翻转):`eval_sequential()` 当前实现只调用 1 次 `eval_fn_()`,缺少 Verilator 所需的完整时钟序列 `clk=0→eval→clk=1→eval(posedge 触发)→clk=0→eval`。同时 `sync_inputs_to_vtop()` / `sync_outputs_from_vtop()` 可能每次都强制覆盖 Vtop 字段,绕过 Verilator 用于边沿检测的 `VlClockSig`/`VlClockReceiver` 基础设施。

**实证证据**(Oracle 审计 + 本会话验证):

1. `tests/test_verilator_e2e_harness.cpp:152-167`:`CounterFixture<32>` 跑 50 cycle,`actual == 50` 被降级为 `CHECK`(非 REQUIRE)
2. `tests/test_verilator_e2e_harness.cpp:170-208`:`CounterFixture<4>` 跑 50 cycle,`actual == 50 % 16` 被降级为 `CHECK`
3. `tests/test_axi_lite_verilator_e2e.cpp:212-250`:`AxiLiteTop` 写 0xDEADBEEF 到 reg0 然后读回:
   - `awready=0`(期望 1 — 组合)
   - `wready=0`(期望 1 — 组合)
   - `bvalid=0`(期望 1 — 组合)
   - `rdata=0x0`(期望 0xDEADBEEF — sequential reg)

**为什么必须现在修**(Oracle 审计 + 当前 perf 数据):
- `perf_three_way` ctest(TIMEOUT 360)虽然 PASS,但 TC-07 depth=1000 verilator 行的 `34.54 ticks/sec` 是**实测取得的**,说明 fix 路径是部分可工作的;若 issue #25 真不工作,verilator 编译/运行会失败。
- ADR-035 §M5(verilator 真实 e2e)的所有 REQUIRE 断言都被这条 message 降级,无法验证 Verilator 与 interpreter 语义等价。
- ChipForge Phase 6d.5 E8(verilator 跑 vendored RISC-V ELF)需要顺序逻辑真仿真才能跑通。

**与其他已完成变更的关系**:
- `verilator-perf-three-way`(2026-09-22,archived):修了 codegen mask,但没碰 runtime eval/sync
- `adr-035-phase-3-complete`(2026-09-21,archived):声称 Phase 3.4 (clock model) ✅ 但 Oracle R2 实证显示是 stub

## What Changes

### 实现层(`src/core/verilator_backend.cpp`)

| 位置 | 改动 |
|------|------|
| `eval_sequential()` (L696-711) | 1. 直接设置 Vtop 时钟字段为 0 + 一次 eval;2. 设置时钟为 1 + 一次 eval(posedge 触发 always_ff);3. 设置时钟为 0 + 一次 eval(清理)。共 3 步直接 Vtop 字段操作 |
| `sync_inputs_to_vtop()` (L349-361) | 在 eval_sequential 前不覆盖 Vtop 时钟字段;只同步 user inputs(awvalid/wdata 等) |
| `sync_outputs_from_vtop()` (L363-372) | 在每步 eval 后强制刷新 Vtop 输出字段到 data_map_;不能延迟到 eval_sequential 返回后才一次同步 |
| `build_port_access_table()` (L318-347) | 检查 `node->size()` 时钟/复位 type 与用户 IO 一视同仁;不剔除 `type_clock`/`type_reset` 的访问表条目(目前剔除可能是元凶之一) |

### 验证层(测试增强)

| 测试 | 改动 |
|------|------|
| `tests/test_verilator_e2e_harness.cpp:152-167` | `CHECK` → `REQUIRE(actual == 50)` |
| `tests/test_verilator_e2e_harness.cpp:170-208` | `CHECK` → `REQUIRE(actual == 50 % 16)` |
| `tests/test_axi_lite_verilator_e2e.cpp:212-250` | `INFO` → `REQUIRE(awready == 1)` / `REQUIRE(wready == 1)` / `REQUIRE(bvalid == 1)` / `REQUIRE(rdata == 0xDEADBEEF)` |

### Spec 增量(`openspec/specs/verilator-backend-e2e/spec.md`)

新增 1 个 SHALL:
> **VerilatorBackend MUST 正确传播 Vtop 组合 wire 输出与 sequential reg 更新到 Simulator 的 data_map_**。

## Acceptance Criteria

- [ ] **AC1**: `tests/test_verilator_e2e_harness.cpp` 全部 REQUIRE(actual == expected)成功(counter 50 ticks, counter<4> mod 16)
- [ ] **AC2**: `tests/test_axi_lite_verilator_e2e.cpp` 全部 REQUIRE 成功:AWREADY/WREADY/BVALID/RVALID 在握手窗口为 1;RDATA == 0xDEADBEEF 写后读回
- [ ] **AC3**: `ctest -L base` 100% 通过(无 regression)
- [ ] **AC4**: `./run_all_ported_tests.sh` 28/28 PASS
- [ ] **AC5**: `openspec validate --strict` PASS
- [ ] **AC6**: TC-07 depth=1000 verilator 行 reports `> 30 ticks/sec`(当前 34.54 是 baseline,不允许 regress)
- [ ] **AC7**: 文档同步 — root `AGENTS.md` + `docs/CHANGELOG.md` + `docs/developer_guide/verilator-integration.md` 记录 fix

## Out of Scope (本 change 不做)

- 64-bit port cap 提升(ADR-035):TC-11 仍然不可用
- K1 ORC JIT cross-DUT 状态污染根因定位:`fix-jit-orc-state-leak` 已标 SUPERSEDED,变通方案有效
- TC-09 / TC-11 verilator harness:本 change 不实施

## 依赖关系

```
verilator-perf-three-way  (done)
       ↓
verilator-issue-25-fix    (this change — option A from Oracle audit)
       ↓ (unlocks)
ChipForge Phase 6d.5 E8   (vendored RISC-V ELF in verilator)
```

无独立依赖。可单独 archive。

## 估时

- T0 调查 eval/sync 实际行为:0.5 day
- T1 实现 3 步时钟翻转 + 修正 sync 路径:0.5-1 day
- T2 强化测试 RED→GREEN:0.5 day
- T3 docs/spec/archive:0.25 day

**总计:1.5-2.5 day**。