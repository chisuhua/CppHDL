# Change: fix-deprecate-logical-operators-residual — 闭合 deprecate-hdl-logical-operators-redo 后 silent drift

## Why

**现状**（oracle audit 2026-10-20）：

`openspec/changes/archive/2026-10-15-2026-10-16-deprecate-hdl-logical-operators-redo/` archive proposal §B 表（line 99）声称 "examples/riscv-mini/src/{rv32i_regfile,rv32i_alu,rv32i_tcm}.h" 列入 migration scope，但 §B 表也提到 "stages/{ex,id}_stage.h"。

实测 audit 发现 **`examples/riscv-mini/src/stages/{ex,id,if}_stage.h` 仍有 3 处 `ch_bool &&/||` 残留**：

| File:line | 形式 |
|----------|------|
| `examples/riscv-mini/src/stages/ex_stage.h:126` | `ch_bool should_fill = sign_bit && (shift_amt > ch_uint<5>(i));` |
| `examples/riscv-mini/src/stages/id_stage.h:174` | `ch_bool jump_flag = is_jal \|\| is_jalr;` |
| `examples/riscv-mini/src/stages/id_stage.h:177` | `ch_bool alu_flag = is_op \|\| is_opimm \|\| is_lui \|\| is_auipc;` |

**根因**：

- `ba04fdb refactor(riscv-mini)` (2026-06-15) 把 stages/ 移入 `examples/riscv-mini/src/`
- `deprecate-redo` (2026-10-16) 落地的 migration 35 文件清单**漏掉**这 3 个 stages
- archived tasks.md §0.1 audit 列表（line 30-32）也未含 — proposal §B 与 tasks 内部不一致

**风险评估**（per `include/core/operators_logic.h:160-198` [[deprecated]] overload）：

| 风险 | 严重度 | 实测 |
|------|-------|------|
| 当前编译触发 deprecation warning | 🟢 无 | `examples/riscv-mini/src/stages/` 被 `include/cpu/pipeline/stages/` shadow（`examples/riscv-mini/CMakeLists.txt:22-31` 注释明确），shadow dead code 不被编译 |
| 未来 re-enable RV32I SOC 触发 warning | 🟡 中 | 一旦 `examples/riscv-mini/src/rv32i_pipeline.h` 优先级反转或移除 `include/cpu/pipeline/` shadow，立即 ~3 个 warning |
| 触发硬编译错误 | 🟢 无 | `ch_bool && ch_bool` / `ch_bool \|\| ch_bool` 是 deprecated 但**仍可编译**；与 `rv32i_system.h:182` 的 `ch_bool && port` 不同（后者 overload 不存在） |
| 项目契约违反 | 🟡 中 | AGENTS.md §已消除段声称"先 migration 35 个文件、~233 行完成"，实际 3 处遗漏 → archive 声明与代码 drift**

**oracle SWOT（2026-10-20）**：
- S: 修复极便宜（~3 处全 ch_bool，sed-safe）
- W: 无 -Werror、shadow dead code、无 spec 违规
- O: 顺手闭合 archive drift，扫清 redo tasks.md §5.1 未来 CI guard 障碍
- T: 无 - 现 build 已 0 warning，re-enable 时仅 warning 不致命

**预期收益**：

1. **闭合 archive 声明与代码 drift**：3 处 sed-rewrite 让"35 文件 233 行"声明与实际一致
2. **ZERO-DEBT POLICY 合规**：ROOT AGENTS.md "No dead code" + "No undocumented changes" — 当前是隐性 drift
3. **未来 RV32I re-enable 时** 0 deprecation warning 增量

**预期成本**：

- 一次性 CI 墙钟增量 ~0 sec（shadow file，不进 build）
- 改造工作量 ~30 min（3 处 sed + 3 处复算 verify）
- 1 commit

**Out of Scope**（明确划线）：

- ❌ **`examples/riscv-mini/src/rv32i_system.h` 不动**——archived redo proposal §C.1 (line 106-109) 显式标 "Truly dead code ... do not migrate — separate fix bit-rot change"。oracle 验证：当前 0 references，且 `docs/freertos/02-architecture.md:159` 与 phase-5 roadmap 引用，删除属架构决策
- ❌ **`include/cpu/pipeline/stages/` 不动**——active 路径且 0 deprecate drift（实测）
- ❌ **`examples/riscv-mini/src/stages/{ex,id,if}_stage.h` 之外**——其他文件 audit 0 命中
- ❌ **`include/cpu/cache/{i,d}_cache_stub.h` 不动**——ADR-037 cache-stub-physical-removal 已立项 follow-up（2027-04-17 deadline，本季度 oracle 优先级 C 推荐提前启动，独立 change）
- ❌ **改 main spec `dual-backend-test-coverage/spec.md` 或 `cpu-cache-subsystem/spec.md`**——本 change 是 no-spec-changes（archived redo 同类）

## What Changes

### 1. sed-rewrite 3 处 ch_bool && / || → & / |

| File:line | Before | After |
|----------|--------|-------|
| `examples/riscv-mini/src/stages/ex_stage.h:126` | `ch_bool should_fill = sign_bit && (shift_amt > ch_uint<5>(i));` | `ch_bool should_fill = sign_bit & (shift_amt > ch_uint<5>(i));` |
| `examples/riscv-mini/src/stages/id_stage.h:174` | `ch_bool jump_flag = is_jal \|\| is_jalr;` | `ch_bool jump_flag = is_jal \| is_jalr;` |
| `examples/riscv-mini/src/stages/id_stage.h:177` | `ch_bool alu_flag = is_op \|\| is_opimm \|\| is_lui \|\| is_auipc;` | `ch_bool alu_flag = is_op \| is_opimm \| is_lui \| is_auipc;` |

**注**：ch_bool `operator&` / `operator|` 是 bitwise，对 1-bit 值等价于 logical AND/OR（reference: `include/core/operators_logic.h` 已有 `operator&(ch_bool, ch_bool)`/`operator|(ch_bool, ch_bool)` 实现）。语义不变。

### 2. 不改 spec（no-spec-changes）

本 change 不触动 `openspec/specs/dual-backend-test-coverage/spec.md` 或其他 spec——archived `deprecate-hdl-logical-operators-redo` 同类处理（用 `--skip-specs` archive）。

## Acceptance

#### Scenario: 当前 build 仍 0 deprecation warning

- **WHEN** `cd build && cmake --build . -j$(nproc) 2>&1 | grep -c "deprecated-declarations"`
- **THEN** 输出 0（shadow file 改不改都不影响 build path）

#### Scenario: file-level grep 0 残留

- **WHEN** `grep -rnE "(ch_bool|ch_uint)\b.*(\|\||\&\&)" examples/riscv-mini/src/stages/*.h`
- **THEN** 输出 0 行（原 3 行被 sed-rewrite 消除）

#### Scenario: 编译无错

- **WHEN** `cmake --build build -j$(nproc) 2>&1 | grep -E "error|Error"`
- **THEN** 0 行错误

#### Scenario: ctest 全量不退化

- **WHEN** `ctest -LE perf --output-on-failure`
- **THEN** 156/156 PASS（同 baseline）

- **WHEN** `ctest -L verilator_e2e -LE perf --output-on-failure`
- **THEN** 21/21 PASS（同 baseline）

#### Scenario: 不破反约束

- **WHEN** reviewer 检查本 change 的 diff
- **THEN** MUST NOT 改 `include/cpu/pipeline/stages/*.h`（active 路径 0 drift）
- **AND** MUST NOT 改 `examples/riscv-mini/src/rv32i_system.h`（archived redo §C.1 显式排除）
- **AND** MUST NOT 改 `examples/riscv-mini/src/cache/*.h`（ADR-037 独立 follow-up）
- **AND** MUST NOT 改任何 spec 文件（no-spec-changes change）

## Capabilities

本 change 是 no-spec-changes，**不新增 capability requirement**。已归档的 `dual-backend-test-coverage/spec.md` R6 排除清单 + `cpu-cache-subsystem/spec.md` R-cache-stub-removal 不变。