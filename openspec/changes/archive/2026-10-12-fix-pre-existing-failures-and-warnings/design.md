# Design: fix-pre-existing-failures-and-warnings

## Context

本会话之前的基线核查（PR-1 + PR-2 已合并至 2fd1fef + 3 commits）暴露 2 个 pre-existing 失败：

- **F1** `test_verilator_three_way` 6/143 失败（实际 7 个 case 失败，第一个 SIGABRT 中止整个 ctest 子进程，外部观察为 1 个）
- **F2** 2 个 -Wswitch-enum 警告

两者均与 5c1cd3d / bfe6e96 正交。本 change 仅做最小化修复（删除 7 个 test case + 加 2 个 switch case），不改任何库语义。

## Goals / Non-Goals

详见 `proposal.md`。本设计文档补充技术决策。

## Decisions

### D1：删除 7 个 failing test case，不重写为有效值版本

- 决策：直接从 `test_verilator_three_way.cpp` 删除 7 个使用 overflow 值的 test case
- 理由：
  - 测试设计的"codegen mask overflow"路径不可达（ch_uint 严格 ctor 在前）
  - `bitvector<word_t>` 无友元暴露 `words_[0]`，无法绕过 ctor 构造 overflow 值
  - `ch_uint` 公共 API 唯一不触发 CH_CHECK 的非 bool 整数路径是 `bitvector(uint32_t size)` 默认 ctor（value=0），无法验证 mask
  - 重写为有效值（如 0, 1, 255）的版本**已存在**（line 108, 126, 142, 316）—— 5 个 working case 已覆盖正常 codegen 路径
- 删除的 7 个 case（line 号）：
  - L80 `get_literal_str masks ch_uint<8>(256) to 8'h0 (not 8'h100)`
  - L94 `get_literal_str masks ch_uint<8>(999) to 8'he7 (not 8'h3e7)`
  - L166 `get_literal_str masks ch_uint<16>(65537) to 16'h1 (not 16'h10001)`
  - L182 `get_literal_str masks ch_uint<32>(4294967296) to 32'h0`
  - L201 `print_concat masks ch_uint<4>(999) literal in concat RHS`
  - L224 `ch_uint<8>(256) zero-extended to 32-bit produces no 32'h100/8'h100`
  - L244 `get_literal_str fuzz: random (width, value) produces no overflowing hex`
- 保留的 5 个 working case：L108, L126, L142, L316, L353

### D2：`opimpl::create_instruction` switch 加 unreachable case 而非降级 `-Wswitch-enum` → `-Wswitch`

- 决策：加 2 个 case，`return nullptr;`，附注释
- 理由：
  - ROOT AGENTS.md "Treat warnings as errors" —— 应修复而非降级
  - `ch_op::mux` / `ch_op::bits_update` 在 enum 中（`include/core/lnodeimpl.h:25,28`）但 switch 不显式 case 即触发 warning
  - 2 个 case 在 `opimpl` 路径**永远 unreachable**（`muximpl` / `bitsupdateimpl` 是独立 lnodeimpl 子类，codegen 走 `src/codegen_verilog.cpp:244,246` 的独立 case 分支，JIT 走 `JitOp::SELECT` / `JitOp::BITS_UPDATE` per `include/jit/AGENTS.md:42-43`）
  - 加 explicit case + `return nullptr;` 是最薄的兼容方案，与项目惯例一致（其他 unused enum 值的 case 暂未发现，但 switch 有 `default: return nullptr;` 兜底）

### D3：不改 ch_uint 严格 ctor 行为

- 决策：`include/bv/bv_assign.h:63` 的 `CH_CHECK` 保留
- 理由：
  - 库语义稳定 —— "overflow → assert"是 design contract，修改会破坏下游用户的 fail-fast 假设
  - 与 SpinalHDL `UInt(width).raw(value)` 静默 narrowing 语义**有意不同**（CppHDL 偏好显式错误）
  - 即使去掉 CH_CHECK，fix D1 删除 7 个 test 仍必要（codegen mask 仅 defensive 用途，正常路径不触发）

### D4：openspec archive 用 `--skip-specs`

- 决策：`openspec archive fix-pre-existing-failures-and-warnings --skip-specs -y`
- 理由：
  - 符合 `fix-ns-pollution` 模式（AGENTS.md 显式允许 + 本 change `proposal.md` 已声明 "no spec changes"）
  - 本 change 无新 capability、无新 requirement，archive 只保留历史 commit 记录

## Risks / Trade-offs

| 风险 | 等级 | 缓解 |
|------|------|------|
| 删除 test case 失去 codegen mask 的回归保护 | 低 | `get_literal_str:158-160` mask 代码在 commit `96ca969` 引入，code review 充分；无新代码改动，仅删除 7 个不可达 case |
| `case ch_op::mux: return nullptr;` 静默吞掉未来 opimpl 派发 bug | 极低 | muximpl 是独立类，opimpl 不会持有 `op_ = ch_op::mux`；如未来重构改变这点，`return nullptr;` 会立即在 ctest 中暴露（不会 SIGABRT 但会返回 null 触发上游逻辑） |
| pre-existing 失败被错误归因到本 change | 低 | commit message 明确"pre-existing, not introduced by 5c1cd3d/bfe6e96" |
| 用户期望 ch_uint 支持静默 narrowing | 低 | proposal NG1 显式排除；CH_CHECK 保留 = 与 SpinalHDL 区分的 design decision |

## 验证矩阵

| 验证项 | 命令 | 期望 |
|--------|------|------|
| 编译 | `cmake --build build -j$(nproc)` | exit 0 |
| 警告 | `cmake --build build --target cpphdl -j1 2>&1 \| grep -E "warning:"` | 仅 0 个 -Wswitch-enum（其他 pre-existing 警告若有，记录但不改） |
| 测试 | `ctest -LE perf` | 143/143 通过（基线 142/143 + 修 1） |
| openspec spec 校验 | `openspec validate --specs --strict` | 7/7（不变） |
| openspec change 校验 | `openspec validate fix-pre-existing-failures-and-warnings --strict` | valid |
| 残留 | `git grep "ch_uint<8>(256)\|ch_uint<8>(999)\|ch_uint<16>(65537)\|ch_uint<32>(4294967296)\|ch_uint<4>(999)"` | 仅 `git log` 历史保留，源码 0 命中 |
