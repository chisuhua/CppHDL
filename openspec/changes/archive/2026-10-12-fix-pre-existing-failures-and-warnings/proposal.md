# Proposal: fix-pre-existing-failures-and-warnings

> 修复 2 个 pre-existing 失败（不在 5c1cd3d 升级 scope 内），让 main 达到 zero-debt 状态。

## Why

本会话基线核查（`ctest -LE perf`）发现 2 个 pre-existing 失败（commit `2fd1fef` 之前已存在）：

1. **`test_verilator_three_way` (6/143 failure)**：commit `567b752` (2026-10-12 之前) 引入的 7 个 test case 使用 `ch_uint<8>(256)` 等 overflow 值，被 `include/bv/bv_assign.h:63` 的 `CH_CHECK(log2ceil(value+1) <= size, "value out of range")` 拦截，**SIGABRT 在 codegen 触达之前**。`567b752` commit message 自报"all PASS"不实（与 `bfe6e96` 自报"test_mem_comb_read passes"模式相同 —— 仅验证单 test 而非全 ctest 套件）。`src/codegen_verilog.cpp:158-160` 的 `get_literal_str` mask 逻辑本身正确，但 ch_uint 严格 ctor 阻止 codegen 触达。
2. **-Wswitch-enum 警告**：`src/ast/ast_nodes.cpp:81` 的 `opimpl::create_instruction` switch 缺 `case ch_op::mux:` 和 `case ch_op::bits_update:`，触发 2 个 -Wswitch-enum 警告。`ch_op` enum 在 `include/core/lnodeimpl.h:41-69` 含 31 个值，switch 仅显式处理 28 个。`ch_op::mux` / `ch_op::bits_update` 在 codegen (`src/codegen_verilog.cpp:244,246,688,689`) 和 JIT (`include/jit/AGENTS.md:42-43`) 路径有使用，但 `opimpl` 实例永不持有这俩 op_（muximpl/bitsupdateimpl 是独立类），所以 switch 缺 case 是 unreachable 但 warning 仍发。

两者均与 `5c1cd3d` C++23 升级正交，也与 `bfe6e96` verilator refactor 正交，属于独立 workstream。

## What Changes

- **Modify** `tests/test_verilator_three_way.cpp`：删除 7 个使用 overflow 值的 test case（line 80, 94, 166, 182, 201, 224, 244），添加 file header 注释说明 ch_uint 严格 ctor 阻止 codegen mask 路径触达
- **Modify** `src/ast/ast_nodes.cpp`：在 `opimpl::create_instruction` switch (line 81-173) 添加 `case ch_op::mux: return nullptr;` 和 `case ch_op::bits_update: return nullptr;`，附注释说明 muximpl/bitsupdateimpl 是独立类不通过 opimpl 派发
- **No spec changes**：本 change 仅修复既有行为，不引入新 API/语义。`openspec archive --skip-specs` 在 archive 时使用，符合 `fix-ns-pollution` 模式
- **No CHANGELOG / AGENTS.md** 变更：纯代码 bug fix

## Goals

- **G1**：`ctest -LE perf` 143/143 通过（消除 1 个 SIGABRT）
- **G2**：`cmake --build` exit 0，0 -Wswitch-enum 警告
- **G3**：codegen mask 防御代码（`get_literal_str:158-160`）的契约通过注释和 code review 保留
- **G4**：归档后 main 达 zero-debt 状态

## Non-Goals

- **NG1**：不修改 ch_uint 严格 ctor 行为（保留"overflow → assert"语义，与 SpinalHDL 静默 narrowing 不同）
- **NG2**：不引入 bitvector 友元访问（避免暴露内部 `words_` 数组）
- **NG3**：不重写 ch_uint ctor 路径以支持静默 narrowing（与 `fix-include-aggregation` (NG1) 一致 —— 库语义稳定优先）
- **NG4**：不修复其他 pre-existing 失败（若有，由独立 change 跟踪）

## 关联变更

- 5c1cd3d (C++23 升级) + bfe6e96 (verilator refactor 漏 `;`) —— 已在 2fd1fef 之前的本会话 fix
- 2fd1fef (HEAD) —— 当前 main，本 change 修复其遗留失败
- 567b752 (literal mask tests) —— 引入失败 test 的 commit，本 change 撤销其中 7 个不可达 case
- 96ca969 (literal mask fix in `get_literal_str`) —— codegen 修复本身正确，本 change 保留

## 兼容性审计

无 API 变化，无 C++ standard 切换，无 JIT 路径变更。
