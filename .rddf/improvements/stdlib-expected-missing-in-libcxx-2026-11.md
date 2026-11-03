---
name: stdlib-expected-missing-in-libcxx-2026-11
priority: P1
source: 头文件自包含性护栏（scripts/verify_header_self_contained.sh）实测挖出
phase: phase-5
category: framework
type: bugfix
created_at: 2026-11-03
---

# Improvement: `verilator_backend.h` 依赖的 `std::expected` 在部分工具链组合下**不存在**
（⇒ 裁决 = **Verilator 后端显式 Linux-only**）

- **Date drafted**: 2026-11-03
- **Drafted by**: build-agent (builder role)
- **Status**: ✅ **已裁决并落地 = 选项 D**（2026-11-03，用户裁决：「**显式排除macOS/Windows**」）⇒ ⇒ **不**去修 `std::expected`，而是把「Verilator 后端 = Linux-only」变成**显式契约**：CMake 在 `APPLE`/`WIN32` + `BUILD_VERILATOR=ON` 时 `FATAL_ERROR`；头文件在能力缺失时给**显式 `#error`**；文档 + 根 `AGENTS.md` 同步。⇒ 详见 §7。
  ⚠️ **立项时的两个事实判断已被实测/推理推翻**（见 §7.1）：① 「libc++ 直到 LLVM 19 才提供 `std::expected`」**不成立**；② 「macOS 上 `BUILD_VERILATOR=ON` 会构建失败」 也**不成立**。真实的缺陷面是「**clang + libstdc++ 13 这一组合**」。
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

⇒ ⇒ **`std::expected` 是 C++23 的标准库设施** ⇒ ⇒ 但「**声称 C++23**」**不等于**提供它。

🔴 **2026-11-03 订正（立项时的推断是错的）**：

| 立项时的说法 | 实测 / 推理 | 结论 |
|---|---|---|
| 「libc++ 直到 LLVM 19 才提供 `std::expected`」 | libc++ 在 **LLVM 16** 就已实现 `<expected>` | ❌ **错** |
| 「macOS 14/15 上 `BUILD_VERILATOR=ON` 构建失败」 | macOS CI **一直绿**，而 `tests/dual_backend_fixture.h` **无条件** include 本头 ⇒ ⇒ macOS 上这个头**本来就编译得过** | ❌ **错** |
| 「clang++ 编译不过」 | ✅ 复现，但**真因不同**（见下） | ✅ 但归因错 |

**真正测到的机制**（本机 `clang++ 18.1.3` + `libstdc++ 13`）：

```bash
$ clang++ -std=c++23 -dM -E -x c++ - | grep __cpp_concepts
#define __cpp_concepts 201907L          # ← clang 18 报 201907
$ g++ -std=c++23 -dM -E -x c++ - | grep __cpp_concepts
#define __cpp_concepts 202002L          # ← g++ 报 202002
$ sed -n '34p' /usr/include/c++/13/expected
#if __cplusplus > 202002L && __cpp_concepts >= 202002L   # ← libstdc++ 的大门
```

⇒ ⇒ libstdc++ 13 的 `<expected>` **整个文件被预处理掉** ⇒ ⇒ TU 里连 `std`
都未声明 ⇒ ⇒ 报错是 `use of undeclared identifier 'std'`（**与真因相距极远**）。

⇒ ⇒ 结论：这是 **「clang + libstdc++ 13」这一组合**的问题，**不是** macOS/libc++ 的问题。

### 1.3 为什么 CI 一直是绿的（**长期不可见**的机制）

🔴 **订正**：立项时写「该头只在 `BUILD_VERILATOR=ON` 时才编译」是**错的** ——
`tests/dual_backend_fixture.h:68` **无条件** include 它，而双后端测试**在所有平台
都编译**（工具缺失时走 WARN + 早退）⇒ ⇒ 该头**每个平台都会被编译**。

| CI job | 平台 | 编译器 | 编译该头？ | 结果 |
|---|---|---|---|---|
| `build-and-test` | ubuntu-22.04 | **g++**（libstdc++） | **是**（dual 测试） | ✅ |
| `build-and-test` | **macos-14** | AppleClang（**libc++**） | **是**（dual 测试） | ✅ ⇒ ⇒ **macOS 其实是好的** |
| `pr-verilator-fast` / `nightly-verilator` | Linux | g++ | 是 | ✅ |

⇒ ⇒ **CI 里没有任何一处用 `clang++ + libstdc++ 13`** ⇒ ⇒ 真正的缺陷面
（clang/libstdc++ 组合）**从未被 CI 暴露** —— 它是被
`scripts/verify_header_self_contained.sh`（**用两个编译器各跑一遍**）**首次**抓到的。

⚠️ **实际影响**：**macOS 用户开启 `BUILD_VERILATOR=ON` ⇒ ⇒ 构建失败** ⇒ ⇒
macOS 被事实上排除在 verilator 后端之外（而 `feat-platform-matrix` 的目标
恰恰是扩平台 ⇒ ⇒ 这条缺陷**与该目标直接冲突**）。

### 1.4 为什么本 change 不顺手修

修法要么换掉 `std::expected`（**公开 API 变更** ⇒ 需 ADR），要么条件编译
（**大面积** `#if __cpp_lib_expected` ⇒ ⇒ 两种错误路径都要维护）⇒ ⇒
属**另一个决策** ⇒ ⇒ 本 change **只按能力绕开**（护栏的 `stdlib:<能力>` 豁免）
⇒ ⇒ 缺陷**独立立项**（本文件）。

## 2. 候选修法（A/B/C/D）—— **已于 2026-11-03 裁决为 D**（见 §7）

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

🔴 **最终裁决（2026-11-03，用户）= 选项 D「显式排除 macOS/Windows」** ——
即：**不修** `std::expected`，把「Verilator 后端 = Linux-only」变成**显式契约**
（理由见 §7.2）⇒ ⇒ 下面这段「建议选项 A」是**立项时的建议**，已被裁决**取代**，
保留作决策史。

⚠️ **立项时的建议**：**选项 A**（自研最小类型）—— 理由：① `VerilatorResult` 的**消费者只有
3 处**（`src/core/verilator_backend.cpp` 的 7 个返回点 + `tests/dual_backend_fixture.h`
的 1 个 `.has_value()` + `tests/test_verilator_error_flow.cpp` 的 1 条
`STATIC_REQUIRE`）⇒ ⇒ 爆炸半径**很小**；② 一次解决，**不再有两条路径**；
③ 不引入新依赖。

## 3. Acceptance (验收判据 —— **按选项 D 重写**)

- [x] **CMake**：`BUILD_VERILATOR=ON` + (`APPLE` OR `WIN32`) ⇒ **configure 期
      `FATAL_ERROR`**，消息含「Linux-only」+ 替代命令（`-DBUILD_VERILATOR=OFF`）
      + 文档指针
- [x] **头文件**：能力缺失时 **显式 `#error`**（消息指明「clang + libstdc++ 13」这一
      组合与可行方向），而不是 `use of undeclared identifier 'std'`
- [x] ⚠️ **反向判据（MUST NOT）**：该头 **MUST NOT** 在 `__APPLE__` / `_WIN32` 上
      `#error` —— `tests/dual_backend_fixture.h:68` **无条件** include 它，加平台
      `#error` 会打断 macOS/Windows 的**整个测试构建**（⇒ 「显式排除」只落在 CMake 层）
- [x] **文档**：`docs/developer_guide/verilator-integration.md` §Supported platforms
      + macOS 小节 + `docs/usage_guide/10-verilator-backend.md` 快速开始 +
      根 `AGENTS.md` KNOWN LIMITATIONS
- [x] **Linux 上零回归**：`g++` 构建 0 error；`ctest -LE perf` 零回归
- [ ] ~~`clang++` 下该头可编译~~ ⇒ **不再作为验收项**（选项 D 明确不修）；它仍是
      `scripts/lib/header_self_contained_exempt.txt` 的 `stdlib:expected` 豁免项
      ⇒ ⇒ **将来真修好时该豁免会自动过期转红**（那条机制保留着）

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

## 7. ✅ 裁决与落地（2026-11-03 · 选项 D「显式排除 macOS/Windows」）

### 7.1 立项时的两个事实判断被推翻（**先订正事实，再谈方案**）

| 立项时的说法 | 实测 / 推理 | 结论 |
|---|---|---|
| 「libc++ 直到 LLVM 19 才提供 `std::expected`」 | libc++ 在 **LLVM 16** 就已实现 | ❌ 错 |
| 「macOS 上 `BUILD_VERILATOR=ON` ⇒ 构建失败」 | macOS CI 一直绿，且 `tests/dual_backend_fixture.h` **无条件** include 本头 ⇒ 该头在 macOS 上**本来就编译得过** | ❌ 错 |
| 「clang++ 编译不过」 | 可复现，但**真因**是 `clang 18` 的 `__cpp_concepts = 201907L` < libstdc++ 13 的 `<expected>` 门槛 `>= 202002L`（⇒ 整个头被预处理掉）| ✅ 现象对、归因错 |

⇒ ⇒ 真实的缺陷面 = **「clang + libstdc++ 13」这一组合**（Linux 上很常见），
**不是** macOS/libc++。

### 7.2 为什么选 D（而不是修 `std::expected`）

1. **后端本来就有 POSIX 假设**：`dlopen` / `libVtop.so` / fork-exec ⇒ 「换掉
   `std::expected`」只解决了**编译期**那一半 ⇒ macOS / Windows 上仍然**跑不起来**
   ⇒ ⇒ 修了也**不产生**「macOS 支持 verilator 后端」这个能力。
2. **改公开类型 = 需 ADR**（选项 A/B/C 都会改 `VerilatorResult` 的类型）⇒ ⇒ 而它
   换来的能力（能在 macOS 上**编译**这个头）**当前没有任何消费者**（后端在
   macOS 上不受支持）。
3. **显式 > 隐式**：D 把「不支持」变成 **configure 期的可行动报错** + 文档 + 头里的
   明确 `#error` ⇒ ⇒ 比「改类型换一个假的可用性」更诚实。
4. **可逆**：将来真要做 macOS 支持 ⇒ 本文件的 §2 选项 A/B/C 仍是完整的决策材料
   ⇒ ⇒ 且 `stdlib:expected` 豁免的「自动过期转红」机制会**提醒**有人来关它。

### 7.3 落地清单

| 文件 | 改动 |
|---|---|
| `CMakeLists.txt` | `BUILD_VERILATOR` 块开头：`if(APPLE OR WIN32)` ⇒ `message(FATAL_ERROR ...)`（含替代命令与文档指针）|
| `include/core/verilator_backend.h` | `#if !defined(__cpp_lib_expected) #error ...`（指明 clang/libstdc++ 组合 + 可行方向）+ 注释写明**为什么不能**在平台宏上 `#error` |
| `docs/developer_guide/verilator-integration.md` | 新增 **§Supported platforms** 表（Linux ✅ / macOS·Windows ❌ 显式排除）+ macOS 小节改写 |
| `docs/usage_guide/10-verilator-backend.md` | 快速开始加「平台支持」callout |
| 根 `AGENTS.md` | KNOWN LIMITATIONS 顶部条目（含「该头 MUST 仍可被 macOS/Windows include」这条反向判据）|

### 7.4 判据（可复跑）

```bash
# ① CMake 守卫存在（Linux 上不触发）
grep -n "BUILD_VERILATOR=ON is not supported" CMakeLists.txt

# ② 头的能力 `#error` 存在
grep -n "__cpp_lib_expected" include/core/verilator_backend.h

# ③ 反向判据：头里 MUST NOT 有平台 `#error`（否则 macOS/Windows 的测试构建全断）
grep -nE "#\s*error.*(__APPLE__|_WIN32)" include/core/verilator_backend.h   # 期望：无输出

# ④ Linux 零回归
cmake --build build -j$(nproc) && (cd build && ctest -LE perf -j4)
```

⚠️ **判别力的边界（如实登记）**：CMake 守卫的**行为**只能在 macOS/Windows 上验证
（本机无法跑）⇒ ⇒ 本文件只提供**静态**判据（①）+ **反向**判据（③）+ Linux 零回归（④）。
⇒ ⇒ 该政策**没有**常驻护栏脚本（与 `feat-platform-matrix` 同级别：政策靠文档 +
评审，而不是靠脚本）；若将来要机械化，最小可行方案 = 一个 grep 级 R1 + 反向 R2。
