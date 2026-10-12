# Proposal: cpp23-standard-upgrade

> 追溯记录：将 CppHDL 项目从 C++20 升级到 C++23，并补齐 5c1cd3d 升级时缺失的 openspec 跟踪与文档清理。

## Why

`5c1cd3d feat(build): upgrade C++ standard from C++20 to C++23` (merged 2026-09-22) bumped the project's 25 CMake files from C++20 to C++23 with zero source-code changes, but merged without an accompanying openspec change. This creates spec drift: the build configuration is C++23 but `openspec/specs/` has no `build-cpp-standard` capability, and 4 documentation files + `openspec/config.yaml` still reference C++17/C++20. Future contributors cannot find the C++23 rationale from the spec layer.

## What Changes

- **Add** `build-cpp-standard` capability under `openspec/specs/build-cpp-standard/spec.md` defining C++23 as the project's baseline C++ standard, with 3 requirements (baseline, compatibility audit, doc consistency) covering 11 SHALL/MUST scenarios
- **Modify** 5 files to align documentation with the C++23 baseline: `samples/CMakeLists.txt:57`, `docs/simulation/ARCHITECTURE.md:13`, `docs/plans/PHASE6-IMPLEMENTATION-PLAN.md:536`, `docs/superpowers/plans/2026-06-10-verilator-submodule-integration.md:9`, `openspec/config.yaml:11`
- **Preserve** `docs/adr/ADR-011-zero-ones-thread-safety.md:77` (a P0438R2 fact about C++17, not a project-standard claim)
- **Document** the 8 known C++23 breakage points and their 0-hit audit results in `design.md`
- **Out of scope**: C++23 feature adoption (std::expected / std::format / etc.), pre-existing test_verilator_three_way failure, pre-existing -Wswitch-enum warning

## 动机

commit `5c1cd3d feat(build): upgrade C++ standard from C++20 to C++23`（2026-09-22）已经把项目的 25 个 CMake 文件从 C++20 升到 C++23，但本次升级**缺少对应的 openspec change**：

1. `openspec/specs/` 没有 `build-cpp-standard` capability
2. 升级的兼容性审计（8 个 C++23 已知破坏点）只散落在 commit message，未沉淀为 spec
3. 升级后 3 个遗留的文档不一致未清理：
   - `samples/CMakeLists.txt:57` 注释仍写 `# 设置C++20标准`（代码已 23）
   - `docs/simulation/ARCHITECTURE.md:13` CppHDL 列仍写 C++20
   - `docs/plans/PHASE6-IMPLEMENTATION-PLAN.md:536` LSP 段仍写 C++17
   - `docs/superpowers/plans/2026-06-10-verilator-submodule-integration.md:9` Tech Stack 仍写 C++17/20
   - `openspec/config.yaml:11` Tech stack 字段仍写 C++20
4. `5c1cd3d` commit message 自报的 "142/142 passed" 与提交时实际 ctest 输出不符（实际当时 143/143 通过，commit message 数字引用了 5c1cd3d 之前的 142 基线），存在 spec drift
5. 后续 `bfe6e96` refactor（Oct 12）漏写 `;` 引入 3 个 Verilator 测试回归（已在本会话以独立 fix-A 修复），但与 C++23 升级正交

## 目标

- **G1**：定义 `build-cpp-standard` capability spec，固化 C++23 作为项目 baseline C++ standard
- **G2**：沉淀 5c1cd3d 的兼容性审计结果（8 个 C++23 已知破坏点全 0 命中）为 spec
- **G3**：清理 5c1cd3d 留下的 4 处文档/注释不一致
- **G4**：把 `openspec/config.yaml:11` 的 Tech stack 字段同步到 C++23
- **G5**：建立 "C++ standard 变更" 的 spec-driven 流程基线（下次升 C++26 走同样模板）

## 非目标

- **NG1**：不引入任何 C++23 特性（`std::expected` / `std::print` / `deducing this` / `std::flat_map` / `std::span` 等）。当前 codebase 对这些特性的采用应作为独立 change
- **NG2**：不修复 `bfe6e96` 引入的 verilator 回归（已在本会话以独立 fix-A 修复，commit 单独跟踪）
- **NG3**：不修复 pre-existing 的 `-Wswitch-enum` warning（`ast_nodes.cpp:81` 的 `mux` / `bits_update` case 缺失），这与 C++23 升级正交
- **NG4**：不升级第三方子模块（`third_party/verilator` / `external/nameof`）的 C++ standard
- **NG5**：不修改 `cmake_minimum_required`（仍 3.14）

## 关联变更

- **基线 commit**：`5c1cd3d feat(build): upgrade C++ standard from C++20 to C++23`（已合并）
- **本次清理**：`samples/CMakeLists.txt:57` 注释同步（1 行）
- **本次文档清理**：4 个 docs 文件的 C++17/C++20 提及更新到 C++23
- **本次 config 同步**：`openspec/config.yaml:11`
- **正交修复（本会话独立 fix-A）**：`src/core/verilator_backend.cpp:278` 补 `;`（与本 change 无 spec 关联）

## 兼容性审计（来自 5c1cd3d commit message，已验证）

8 个 C++23 已知破坏点全部 0 命中：

| 破坏点 | 影响 | 项目状态 |
|--------|------|----------|
| `u8` 字符串字面量变为 `const char8_t*`（P0482R6） | C++17 隐式转换代码 break | 未使用 |
| `auto{x}` brace-init narrowing 禁止 | C++17 行为变 error | 未触发 |
| `if consteval` (P1938R3) 替代 `if constexpr` consteval 分支 | consteval 路径语义变 | 未使用 consteval |
| `#pragma warning` 替代 `#pragma GCC/Clang diagnostic` | pragma 跨编译器 | 全部使用 GCC/Clang 风格 pragma |
| `char8_t` 类型与 `std::u8string` 整合 | UTF-8 字符串 API | 未使用 |
| `consteval` / `is_constant_evaluated` 语义收紧 | 编译期求值路径 | 未使用 consteval |
| `0zu` 整数字面量后缀移除 | 格式化字符串 | 未使用 |
| lambda 静态变量在模板实例化中的行为 | 模板元编程边角 | 未触发 |
