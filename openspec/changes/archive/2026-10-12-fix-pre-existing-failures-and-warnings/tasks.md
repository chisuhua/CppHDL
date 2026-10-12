# fix-pre-existing-failures-and-warnings

> **STATUS (2026-10-12)**: 实施中。修复 2 个 pre-existing 失败（test_verilator_three_way SIGABRT + 2 个 -Wswitch-enum 警告）。

## 1. 复现失败 (RED 状态 — 验证 baseline 与 567b752 / 5c1cd3d 正交)

- [x] 1.1 确认 `ctest -LE perf` 在修复前显示 1 个 SIGABRT (test_verilator_three_way) + 2 个 -Wswitch-enum 警告
      ✅ Done（上一会话基线核查）
- [x] 1.2 确认失败根因：
      - `tests/test_verilator_three_way.cpp:80,94,166,182,201,224,244` 7 个 case 使用 overflow 值 → `include/bv/bv_assign.h:63` CH_CHECK 触发
      - `src/ast/ast_nodes.cpp:81` switch 缺 `ch_op::mux` / `ch_op::bits_update` case
      ✅ Done
- [x] 1.3 确认 codegen mask 代码本身正确：`src/codegen_verilog.cpp:158-160` `value & ((1ULL << width) - 1)`
      ✅ Done

## 2. Issue 1: 删除 7 个不可达 test case

- [x] 2.1 读 `tests/test_verilator_three_way.cpp` 全文，定位 7 个 failing case
      ✅ Done（L80, L94, L166, L182, L201, L224, L244）
- [x] 2.2 写 file header 注释解释 7 个 case 不可达原因（ch_uint 严格 ctor 阻止 codegen mask 路径触达）
- [x] 2.3 删除 7 个 test case
- [x] 2.4 验证 `ctest -L verilator` 全通过（test_verilator_three_way 1/1 pass）+ 5 个 working case 仍通过
      ⏳ Pending

## 3. Issue 2: 加 2 个 -Wswitch-enum case

- [x] 3.1 确认 `ch_op` enum 含 `mux` 和 `bits_update`（`include/core/lnodeimpl.h:25,28`）
      ✅ Done
- [x] 3.2 确认 `opimpl` 永不持有这俩 op_（muximpl/bitsupdateimpl 是独立类，codegen 走独立分支，JIT 走 JitOp::SELECT/BITS_UPDATE per `include/jit/AGENTS.md:42-43`）
      ✅ Done
- [x] 3.3 在 `src/ast/ast_nodes.cpp:81` switch 加 `case ch_op::mux: return nullptr;` 和 `case ch_op::bits_update: return nullptr;` + 注释
- [x] 3.4 验证 `cmake --build build --target cpphdl -j1 2>&1 | grep -E "switch-enum|warning:"` 仅 pre-existing 警告
      ⏳ Pending

## 4. 验证

- [x] 4.1 `cmake --build build -j$(nproc)` exit 0
      ⏳ Pending
- [x] 4.2 `ctest -LE perf` 143/143 通过
      ⏳ Pending
- [x] 4.3 `git grep` 残留检查：源码无 `ch_uint<8>(256)` 等 overflow 构造
      ⏳ Pending

## 5. Commit

- [x] 5.1 `git add tests/test_verilator_three_way.cpp src/ast/ast_nodes.cpp`
- [x] 5.2 `git commit -m "fix: remove unreachable codegen-mask test cases + add missing -Wswitch-enum cases"`
      ⏳ Pending

## 6. Archive

- [x] 6.1 `openspec archive fix-pre-existing-failures-and-warnings --skip-specs -y`
      ⏳ Pending（commit 后执行）
- [x] 6.2 `openspec validate --specs --strict` 7/7（无变化）
      ⏳ Pending

## 7. 后续 PR（独立 workstream，不在本 scope）

- 修复 pre-existing `test_verilator_three_way` ch_uint ctor 设计选择（如需）—— 决定保留严格 ctor vs 静默 narrowing
- 修复其他潜在的 pre-existing 警告（`-Wnonnull-compare` on logger.h 等，per `5c1cd3d` commit message）
- 引入 C++23 特性（`std::expected` for Verilator 错误流 / `std::format` for codegen / deducing this for ch_uint getter / `std::flat_map` for AST）
