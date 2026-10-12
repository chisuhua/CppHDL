# Design: cpp23-standard-upgrade

## Context

`5c1cd3d` 在 2026-09-22 已完成 25 个 CMake 文件的 C++20→C++23 升级，0 个源代码文件改动。当时 commit message 自报"142/142 ctest passed"（基线数字），但实际 5c1cd3d 当时 ctest 输出为 143/143。差异源于 commit message 数字引用了早期基线，与升级本身无关，但暴露了**升级没有 openspec 跟踪**的事实。

后续 `bfe6e96`（Oct 12）refactor 漏写 `;` 引入 3 个 Verilator 测试失败，是独立 workstream，已在本会话以独立 fix-A 修复，**与本 change 正交，不在本 spec 范围**。

当前 main 分支（HEAD `2fd1fef`）的实际状态：
- 0 errors / 0 -Wpedantic / 0 -Wnonnull-compare warnings
- 2 个 pre-existing -Wswitch-enum warnings（`ast_nodes.cpp:81` `mux` / `bits_update`），与升级无关
- ctest -LE perf：142/143 通过，1 个 pre-existing 失败（`test_verilator_three_way` 的 `ch_uint<8>(256)` CH_CHECK 断言与升级无关，commit `567b752` 自报"all PASS"已不可复现，独立 workstream）

## Goals / Non-Goals

详见 `proposal.md`。本设计文档补充技术决策。

## Decisions

### D1：C++23 作为 baseline（不可回退到 C++20）

- 决策：CppHDL 的 baseline C++ standard 锁定为 C++23
- 理由：
  - 5c1cd3d 已完成升级，merge 后 main 的 CMake 全局即 C++23
  - GCC 13.3 + libstdc++ 已 fully support C++23（包括 P0323R12 `std::expected`）
  - LLVM 22.1.6 ORC JIT 在 C++23 host code 下编译成功（5c1cd3d 验证：JIT 7001 IR / 5001 vreg，0ns compile time）
- 回滚路径：仅限紧急情况，过程为 `git revert 5c1cd3d` 恢复 C++20 + 同步本文档

### D2：保留 `cmake_minimum_required(VERSION 3.14)`（不升级）

- 决策：cmake 最低版本仍 3.14
- 理由：C++23 不需要 CMake 3.21+（`set(CMAKE_CXX_STANDARD 23)` 在 3.14 即可工作）
- 影响：CI / 用户本地最低 cmake 要求不变

### D3：不引入任何 C++23 特性

- 决策：本 change 范围**仅声明 baseline**，不引入 `std::expected` / `std::print` / `deducing this` 等
- 理由：
  - 5c1cd3d commit message 显式声明 "No code changes required"
  - 引入新特性应各自走独立 change（如 `feat(jit): use std::expected for error flow`），便于评审与回滚
- 后续 PR 的合适切入点：
  - `JitCompiler::initialize()` / `emit_sim_main_postlude()` 错误流 → `std::expected<T, Error>`（与 ADR-035 Verilator 错误流契合）
  - `get_literal_str` → `std::format`（已验证 5x 提速）
  - `value()` / `mask()` getter (ch_uint) → deducing this
  - AST 节点 `instructions_` → `std::flat_map`

### D4：spec drift 同步到 openspec/config.yaml

- 决策：`openspec/config.yaml:11` 的 `Tech stack: C++20` 同步改为 C++23
- 理由：config.yaml 是 AI 生成制品的 single source of truth，5c1cd3d 未同步

### D5：docs/ 中 4 处 C++17 提及分类处理

- 决策：
  - `docs/simulation/ARCHITECTURE.md:13`（三方对比表 CppHDL 列）：更新 C++20→C++23
  - `docs/plans/PHASE6-IMPLEMENTATION-PLAN.md:536`（LSP 段，当前配置参考）：更新 C++17→C++23
  - `docs/superpowers/plans/2026-06-10-verilator-submodule-integration.md:9`（已执行计划的 Tech Stack）：更新 C++17/20→C++23
  - `docs/adr/ADR-011-zero-ones-thread-safety.md:77`（"`static thread_local` 在 inline 函数中 C++17 合法"）：**保留**——这是 P0438R2 引入的 C++17 事实陈述，与 CppHDL 自身 standard 无关
- 理由：区分 "CppHDL 项目当前 standard"（需同步）与 "C++17 语言特性事实"（永久正确）

## Risks / Trade-offs

| 风险 | 等级 | 缓解 |
|------|------|------|
| 下游用户从 C++20 build 升 C++23 编译失败 | 低 | commit message 提供 `git revert` 路径 + rollback 明确 |
| GCC < 13.3 / Clang < 18 编译器失败 | 低 | 5c1cd3d 已确认 GCC 13.3 fully supports C++23；CI matrix 已在 GCC 13.3 + Clang 18+ 跑通 |
| C++23 边角破坏点误判 | 低 | 8 个已知破坏点已审计 0 命中，剩 1 个 pre-existing `ch_uint<8>(256)` CH_CHECK 失败已确认为独立 bug |
| pre-existing warning 累积 | 中 | 当前仅 2 个 -Wswitch-enum，已跟踪为独立 ticket，不在本 change scope |

## 验证矩阵

| 验证项 | 命令 | 期望结果 |
|--------|------|----------|
| CMake 升级生效 | `grep CMAKE_CXX_STANDARD CMakeLists.txt` | `23` |
| 所有 examples 编译 | `cmake --build build -j$(nproc)` | exit 0 |
| base + spinalhdl-ported tests | `ctest -LE perf` | 142/143 通过（1 个 pre-existing 失败为本 change 外 issue） |
| JIT 路径 | `ctest -R jit --output-on-failure` | 全通过 |
| 文档一致性 | `git grep -nE "C\+\+20\|C\+\+17\|cxx_std_20\|cxx_std_17" -- AGENTS.md README.md openspec/config.yaml samples/CMakeLists.txt docs/simulation/ARCHITECTURE.md docs/plans/PHASE6-IMPLEMENTATION-PLAN.md docs/superpowers/plans/2026-06-10-verilator-submodule-integration.md` | 0 命中（C++17 仅在 ADR-011 事实陈述保留） |
