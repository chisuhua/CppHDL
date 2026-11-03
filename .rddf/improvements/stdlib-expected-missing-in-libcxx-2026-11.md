---
name: stdlib-expected-missing-in-libcxx-2026-11
priority: P1
source: 头文件自包含性护栏（scripts/verify_header_self_contained.sh）实测挖出
phase: phase-5
category: framework
type: bugfix
created_at: 2026-11-03
---

# Improvement: `std::expected` 在 **libc++（macOS / AppleClang）不存在** ⇒ macOS 上开 verilator 会构建失败

- **Date drafted**: 2026-11-03
- **Drafted by**: build-agent (builder role)
- **Status**: 📋 **待实施**（`header-self-containment-fix-2026-11` 的 change **只按能力绕开**、**不**修框架 ⇒ 见 §5）
- **来源**：`header-self-containment-fix-2026-11` 的**新护栏**
  （`scripts/verify_header_self_contained.sh`）实测挖出 —— 该护栏用**两个**编译器
  各跑一遍（g++ / clang++）⇒ ⇒ **只有 clang++ 那半边红** ⇒ ⇒ 定位到
  **标准库可用性**而非「缺 include」。

## 1. Why (背景与动机)

### 1.1 现象（实测，可复现）

```bash
printf '#include "core/verilator_backend.h"\nint main(){return 0;}\n' > /tmp/t.cpp
clang++ -std=c++23 -fsyntax-only -Iinclude -Iinclude/core -Iinclude/ast \
        -Iinclude/abstract -Iinclude/utils -Iexternal/inipp /tmp/t.cpp
# ⇒ include/core/verilator_backend.h:57:30: error:
#      no template named 'expected' in namespace 'std'
```

同一命令用 `g++` ⇒ ✅ 通过。

### 1.2 根因

`include/core/verilator_backend.h:17` 有 `#include <expected>`，
`:57` 有 `using VerilatorResult = std::expected<void, VerilatorError>;`。

⇒ ⇒ **`std::expected` 是 C++23 的标准库设施** ⇒ ⇒
**libstdc++（GCC 12+）提供** ⇒ ⇒ **libc++ 直到 LLVM 19 才提供**
⇒ ⇒ **AppleClang 15（macOS 14）= LLVM 16 ⇒ 无** ⇒ ⇒
**macOS 14 / 15 上 `#include "core/verilator_backend.h"` 编译失败**。

### 1.3 为什么 CI 一直是绿的（**长期不可见**的机制）

`core/verilator_backend.h` **不在 `ch.hpp` 的 include 闭包里** ⇒ ⇒
它只被 `src/core/verilator_backend.cpp` 与 `tests/test_verilator_backend.cpp`
include ⇒ ⇒ 而这两者**只在 `BUILD_VERILATOR=ON` 时才编译** ⇒ ⇒

| CI job | verilator | 编译 `verilator_backend.h`？ |
|---|---|---|
| `build-and-test`（ubuntu + **macos-14**） | ❌ 不装 | **否** ⇒ ⇒ 缺陷不可见 |
| `pr-verilator-fast` | ✅ 装 | 是 —— 但 **只在 Linux**（runner 镜像）⇒ ⇒ 用的 libstdc++ ⇒ ⇒ 通过 |
| `nightly-verilator` | ✅ 装 | 同上（Linux）⇒ ⇒ 通过 |

⇒ ⇒ **没有任何一个 CI job 用 libc++ 编译这个头** ⇒ ⇒ 缺陷**从未被 CI 暴露**。

⚠️ **实际影响**：**macOS 用户开启 `BUILD_VERILATOR=ON` ⇒ ⇒ 构建失败** ⇒ ⇒
macOS 被事实上排除在 verilator 后端之外（而 `feat-platform-matrix` 的目标
恰恰是扩平台 ⇒ ⇒ 这条缺陷**与该目标直接冲突**）。

### 1.4 为什么本 change 不顺手修

修法要么换掉 `std::expected`（**公开 API 变更** ⇒ 需 ADR），要么条件编译
（**大面积** `#if __cpp_lib_expected` ⇒ ⇒ 两种错误路径都要维护）⇒ ⇒
属**另一个决策** ⇒ ⇒ 本 change **只按能力绕开**（护栏的 `stdlib:<能力>` 豁免）
⇒ ⇒ 缺陷**独立立项**（本文件）。

## 2. What Changes (变更范围 —— **待 architect 裁**)

- **定位**：`include/core/verilator_backend.h`（`VerilatorResult` 别名）
  + `src/core/verilator_backend.cpp`（7 处返回类型）
  + `tests/test_verilator_error_flow.cpp`（`STATIC_REQUIRE` 断言类型**就是**
  `std::expected<void, VerilatorError>` ⇒ ⇒ 改类型必然要改这条断言）。

- **候选修法**（**MUST 由 architect 裁决**）：

| # | 选项 | 代价 |
|---|---|---|
| A | **自研一个最小的 `Expected`-like 类型**（仓内 `ch::core::` 命名空间，~40 行） | 公开 API 变更（`VerilatorResult` 的类型变了）⇒ ⇒ 需 ADR；但**一次性**解决，且**零外部依赖** |
| B | **条件编译**：`#if __cpp_lib_expected` ⇒ 用 `std::expected`；`#else` ⇒ 用自研类型 | 两条路径都要测 ⇒ ⇒ 成本翻倍；且「两条路径语义一致」需要判据 |
| C | **降级为 `bool` + 出参**（错误码走出参） | API 变更最大 ⇒ ⇒ 但**最简单**、最可移植；错误信息仍可用出参携带 |
| D | **把 verilator 后端整体排除出非 libstdc++ 平台**（构建期检测 ⇒ 明确报错） | 成本最低，但**不解决问题**，只是把问题变成显式报错 ⇒ ⇒ 需在文档里写明「macOS 不支持 verilator 后端」 |

⚠️ **建议**：**选项 A**（自研最小类型）—— 理由：① `VerilatorResult` 的**消费者只有
3 处**（`src/core/verilator_backend.cpp` 的 7 个返回点 + `tests/dual_backend_fixture.h`
的 1 个 `.has_value()` + `tests/test_verilator_error_flow.cpp` 的 1 条
`STATIC_REQUIRE`）⇒ ⇒ 爆炸半径**很小**；② 一次解决，**不再有两条路径**；
③ 不引入新依赖。

## 3. Acceptance (验收判据)

- [ ] `clang++ -std=c++23 -fsyntax-only` 下 `core/verilator_backend.h` **可编译**
      （⇒ ⇒ **CI 必须有一处用 libc++ 编译这个头** ⇒ 见下）
- [ ] `g++` 下**同样**可编译 ⇒ ⇒ **两套标准库都通过**
- [ ] `tests/test_verilator_error_flow.cpp` 的类型契约断言**更新**（且仍然有效）
- [ ] `ctest -L verilator_e2e` 零回归（verilator 路径的**行为**不变）
- [ ] ⚠️ **CI 覆盖**：护栏 `verify_header_self_contained.sh` **在 macOS runner 上
      也跑**（本 change 已挂 `build-and-test` 的 macos-14 矩阵 ⇒ ⇒ 修好后
      `stdlib:expected` 豁免会**自动失效** ⇒ ⇒ 那时 R2a 会要求把它从豁免名单
      移除 ⇒ ⇒ **这就是「修好了」的机械信号**）⇒ ⇒ **修好 MUST 同时删掉那条豁免**

## 4. Impact

- **爆炸半径**：`std::expected` 的消费者 **3 个文件 / 9 处**（已实测枚举）。
- **风险**：选项 A/B 改的是**公开类型** ⇒ ⇒ 仓外消费者会受影响（无证据）。
- **不做**：全仓普查「其它 C++23 标准库设施在 libc++ 上的可用性」
  （⇒ ⇒ 护栏的 `stdlib:<能力>` 机制已能**逐步**发现，不做一次性普查）。

## 5. 备注（为何本 change 没有顺手修）

本 change（`header-self-containment-fix-2026-11`）的主题是「**头文件自包含性**」⇒ ⇒
而这条是「**标准库可用性**」⇒ ⇒ **两个不同的缺陷类** ⇒ ⇒ 混在一起会让 change 的
主题失焦 ⇒ ⇒ ⇒ 且修它需要**改公开 API** ⇒ ⇒ 按本仓纪律「行为/API 变更需 ADR」
⇒ ⇒ ⇒ 本 change **只按能力绕开**（护栏的 `stdlib:expected` 豁免），
缺陷**独立立项**（本文件）。
