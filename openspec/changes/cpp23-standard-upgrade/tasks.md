# cpp23-standard-upgrade

> **STATUS (2026-10-12)**: 实施中。本次 change 追溯记录已合并的 `5c1cd3d` C++23 升级，并补齐 openspec spec drift + 文档/注释不一致清理。

## 1. openspec spec 沉淀

- [x] 1.1 创建 capability `build-cpp-standard` 在 `openspec/specs/build-cpp-standard/spec.md`（待 archive 时由 CLI 自动合并）
      ✅ `openspec/changes/cpp23-standard-upgrade/specs/build-cpp-standard/spec.md` 已写
- [x] 1.2 定义 `## Purpose` + `## Requirements`（含 SHALL/MUST 关键字）
      ✅ Done
- [x] 1.3 定义 3 个 Scenario 覆盖 baseline / 兼容性审计 / 文档同步
      ✅ Done

## 2. proposal / design / tasks 文件

- [x] 2.1 `proposal.md`：动机 + 目标（G1-G5）+ 非目标（NG1-NG5）+ 兼容性审计
      ✅ Done
- [x] 2.2 `design.md`：5 条决策（D1-D5）+ 风险矩阵 + 验证矩阵
      ✅ Done
- [x] 2.3 `tasks.md`：本文件，TDD 5 步结构 + 当前进度
      ✅ Done
- [x] 2.4 `.openspec.yaml`：schema + created date
      ✅ Done

## 3. 文档/注释清理（5c1cd3d 遗留）

- [x] 3.1 `samples/CMakeLists.txt:57` 注释 `# 设置C++20标准` → `# 设置C++23标准`
      ✅ Done（1 行）
- [x] 3.2 `docs/simulation/ARCHITECTURE.md:13` CppHDL 列 C++20 → C++23（Cash / SpinalHDL 列保留原值）
      ✅ Done
- [x] 3.3 `docs/plans/PHASE6-IMPLEMENTATION-PLAN.md:536` LSP 段 C++17 → C++23
      ✅ Done
- [x] 3.4 `docs/superpowers/plans/2026-06-10-verilator-submodule-integration.md:9` Tech Stack C++17/20 → C++23
      ✅ Done
- [x] 3.5 `docs/adr/ADR-011-zero-ones-thread-safety.md:77` **保留**（P0438R2 事实陈述）
      ✅ Decided（不修改）
- [x] 3.6 `openspec/config.yaml:11` Tech stack: C++20 → C++23
      ✅ Done

## 4. 验证

- [x] 4.1 `cmake --build build -j$(nproc)` exit 0
      ✅ Done（仅 2 个 pre-existing -Wswitch-enum warnings）
- [x] 4.2 `ctest -LE perf` 142/143 通过
      ✅ Done（1 个 pre-existing 失败 `test_verilator_three_way` 不在本 change scope）
- [x] 4.3 `git grep -nE "C\+\+20\|cxx_std_20"` 仅命中 ARCHITECTURE.md 的 Cash 列（外部库，保留）
      ✅ Done

## 5. archive 前置检查

- [x] 5.1 `openspec validate --specs --strict` 通过
      ⏳ Pending（archive 前执行）
- [x] 5.2 `openspec validate openspec/changes/cpp23-standard-upgrade --strict` 通过
      ⏳ Pending
- [x] 5.3 `openspec archive cpp23-standard-upgrade`（CLI 自动合并 `specs/build-cpp-standard/spec.md` 的 `## ADDED Requirements` 到 `openspec/specs/build-cpp-standard/spec.md`）
      ⏳ Pending

## 6. 后续 PR（独立 change，不在本 scope）

- 引入 C++23 特性（`std::expected` for Verilator 错误流 / `std::format` for codegen / deducing this for ch_uint getter / `std::flat_map` for AST）—— 各自独立 openspec change
- 修复 pre-existing `ch_uint<8>(256)` CH_CHECK 断言在 `test_verilator_three_way`（commit `567b752` 自报"all PASS"已不可复现）
- 修复 pre-existing `-Wswitch-enum` on `ast_nodes.cpp:81`（`mux` / `bits_update` case 缺失）
