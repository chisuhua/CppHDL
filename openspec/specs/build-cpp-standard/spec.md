# build-cpp-standard Specification

## Purpose

定义 CppHDL 的 **C++ 语言基线**契约 —— 整个仓库（库 / 示例 / 测试 / benchmark）MUST 以 **C++23** 为唯一的 C++ 标准基线，且**不得**由任何子目录单独降级或改写。

- **唯一权威**：根 `CMakeLists.txt` 的 `CMAKE_CXX_STANDARD` / `CMAKE_CXX_STANDARD_REQUIRED` / `CMAKE_CXX_EXTENSIONS` 三元组；子目录 CMakeLists **MUST NOT** 覆写这三项（⇒ ⇒ 覆写即视为违反本 capability）。
- **为什么存在**：C++ 标准差异会改变**语言语义**（`char8_t` / `auto{x}` / `consteval` 收紧）⇒ ⇒ 同一份源码在不同子目录用不同标准编译 ⇒ ⇒ 「能编过」与「行为一致」不再等价 ⇒ ⇒ 标准基线必须有单点权威。
- **边界**：本 capability **不**管辖具体特性的可用性（`std::expected` 的平台限制 ⇒ 归 `verilator-error-flow`）⇒ **不**管辖第三方代码（`third_party/**` / `external/**` 保留其自带标准）⇒ **不**管辖「升级到新标准时的破坏点审计」——那是**一次性动作**，其结论保留在归档 change 中。

## Requirements
### Requirement: 项目 baseline C++ standard 为 C++23
CppHDL 项目 **MUST** 使用 C++23 作为 baseline C++ standard，所有 CMake target、示例、测试、benchmarks 的 `CXX_STANDARD` 属性 **SHALL** 一致设置为 23，`CMAKE_CXX_STANDARD` 全局变量 **SHALL** 为 23，`CXX_STANDARD_REQUIRED` **SHALL** 为 `ON`，`CXX_EXTENSIONS` **SHALL** 为 `OFF`。

#### Scenario: 顶层 CMake 全局标准
- **WHEN** CMake 配置根项目 (`CMakeLists.txt:6` 附近)
- **THEN** `set(CMAKE_CXX_STANDARD 23)` 且 `CMAKE_CXX_STANDARD_REQUIRED ON` 且 `CMAKE_CXX_EXTENSIONS OFF`

#### Scenario: cpphdl 库 target 标准
- **WHEN** `add_library(cpphdl ...)` 之后
- **THEN** `set_target_properties(cpphdl PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)`

#### Scenario: 25 个二级 CMakeLists 同步
- **WHEN** 配置 `tests/benchmark/CMakeLists.txt`、`samples/CMakeLists.txt`、`examples/**/CMakeLists.txt`
- **THEN** 全部出现 `CXX_STANDARD 23` 或 `cxx_std_23`（无 C++20 残留）

### Requirement: 兼容性审计覆盖 8 个 C++23 已知破坏点
项目 **MUST** 在升级 C++ standard 时，对 8 个 C++23 已知破坏点进行审计并记录结论：u8 char8_t 字面量变更（P0482R6）、`auto{x}` brace-init narrowing 禁止、`if consteval`（P1938R3）、`#pragma warning`（P2552R2）、`char8_t` 与 `std::u8string` 整合、`consteval`/`is_constant_evaluated` 语义收紧、`0zu` 整数字面量后缀移除、lambda 静态变量在模板实例化中的行为。审计结论 **SHALL** 记录在 commit message 或对应 openspec change 的 `design.md` 中。

#### Scenario: 升级 commit 含审计结论
- **WHEN** 阅读升级 C++ standard 的 commit message 或 design.md
- **THEN** 8 个破坏点全部有 "未使用 / 未触发" 的明确结论

#### Scenario: 0 命中情况下不需 code change
- **WHEN** 8 个破坏点审计结果全部为 0 命中
- **THEN** 升级 commit 允许 0 源代码文件改动（仅 CMake + 文档）

### Requirement: 文档与代码在 C++ standard 引用上一致
项目所有文档（`AGENTS.md`、`README.md`、`openspec/config.yaml`、capability folder `openspec/specs/<capability>/spec.md`、`docs/simulation/ARCHITECTURE.md`、历史 plan 的 Tech Stack 段）**MUST** 反映项目当前 C++ standard。事实陈述（如 "P0438R2 引入 C++17 的 `static thread_local` in inline functions 合法性"）**SHALL** 不被错误替换。

#### Scenario: 单一 source of truth
- **WHEN** 用户查询项目 C++ standard
- **THEN** `AGENTS.md` 顶部 Stack 段、`README.md` Key Technologies 段、`openspec/config.yaml` context 段、`CMakeLists.txt` `CMAKE_CXX_STANDARD` 四处全部一致为 C++23

#### Scenario: 历史 plan 的 Tech Stack 同步
- **WHEN** 已完成 phase plan（如 `docs/plans/PHASE6-IMPLEMENTATION-PLAN.md` LSP 段）或已执行 submodule integration plan（如 `docs/superpowers/plans/2026-06-10-verilator-submodule-integration.md` Tech Stack 段）提及 C++ standard
- **THEN** 该提及更新为当前 baseline（保留事实陈述类提及）

#### Scenario: 三方对比表区分对待
- **WHEN** 文档含三方对比表（如 `docs/simulation/ARCHITECTURE.md` 对比 CppHDL / Cash / SpinalHDL）
- **THEN** 只更新 CppHDL 列；外部库列（Cash 的 C++17、SpinalHDL 的 Scala JVM）保留原值

#### Scenario: 事实陈述保留
- **WHEN** 文档中提及某 C++17/20 特性属于语言事实（如 "P0438R2 在 C++17 引入 X 合法性"）
- **THEN** 该提及 **MUST NOT** 被替换为 C++23（事实陈述永久正确）

