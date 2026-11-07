# Design: 层次化等价性 harness 公共化（P8）

> 本文是 **builder 域**的定稿设计（`design.md` 无 skill owns）。推导与证据在
> `proposal.md` 附录 A；可勾选判据在 `tasks.md`。
> ⚠️ 本 change **新增公开 API 面**（`include/hierarchy_equivalence.h`）⇒ ⇒ 归档前须过
> **critic**（触及公开 API ⇒ 2 轮预算）。

## 1. 公共 API 形态

### 1.1 头文件与命名空间

- 路径：`include/hierarchy_equivalence.h`
- 命名空间：`ch::hier_eq`
- ⚠️ **头名 ≠ 命名空间**（`hierarchy_equivalence` vs `hier_eq`）⇒ ⇒ 文档 MUST 两处都写
  ⇒ ⇒ 用户 `grep` 头名时找不到符号名。

### 1.2 公共面（**闭合集合** · 逐符号登记）

```cpp
namespace ch::hier_eq {

/// 一个**受驱动的顶层输入**。
/// ⚠️ `set` 回调 MUST 走 `Simulator::set_input_value(port, <纯整数>)`
///    （`<<=` 会把顶层端口变成「有内部驱动」⇒ 两侧观测面不一致 ⇒ **假分歧**）。
struct DrivenInput {
    std::string name;
    std::function<void(ch::Simulator &, std::uint64_t)> set;
};

/// 一个**被观测**的信号。
/// ⚠️ `node_id` MUST 由 `make_observation(...)` 填（⇒ 用户**不**手写 `impl()->id()`）。
struct Observation {
    std::string name;
    std::function<std::uint64_t(ch::Simulator &)> read;
    std::uint32_t node_id = 0;
};

/// 一次「产物生成」的结果。
struct ProductSet {
    bool ok = false;
    std::string error;
    std::string hier_path;
    std::vector<ch::VerilatorPortSpec> ports;       ///< **由产物派生**
    std::vector<ch::SignalMapEntry> signal_map;     ///< **由产物派生**
};

/// 一次「逐拍比对」的结果。
struct Result {
    bool ok = false;             ///< 运行本身成功
    bool equal = false;          ///< 逐拍全等
    std::string error;           ///< 运行失败原因（**稳定前缀** + 可行动的下一步）
    std::string first_mismatch;  ///< 空 = 全等
    std::size_t mismatch_count = 0;
    /// ⚠️ **扁平侧**逐拍观测值（★ **不是**「两侧轨迹」⇒ ⇒ 层次化侧的值**不**保留
    ///    ⇒ ⇒ 名字 MUST 体现这一点 ⇒ ⇒ 故叫 `flat_trace` 而**不是** `trace`）。
    std::vector<std::vector<std::uint64_t>> flat_trace;
    std::string flat_product;
    std::string hier_product;
};

/// ① 生成层次化产物 + **由产物派生**端口集合 + 映射表。
ProductSet emit(ch::Component &root, const std::string &work_dir,
                 ch::Granularity granularity = ch::Granularity::RespectDeclarations);

/// ② 逐拍比对「扁平产物」与「层次化产物」。
Result compare(ch::core::context *ctx, const std::string &work_dir,
               const std::string &hier_path,
               const std::vector<ch::VerilatorPortSpec> &ports,
               const std::vector<DrivenInput> &inputs,
               const std::vector<Observation> &observations,
               const std::vector<std::vector<std::uint64_t>> &stimulus);

/// 观测信号的**工厂**（★ 用户 MUST NOT 手填 `node_id` ⇒ ⇒ 公共文档里不出现 `impl()`）。
Observation make_observation(std::string name, std::uint32_t node_id,
                              std::function<std::uint64_t(ch::Simulator &)> read);

}  // namespace ch::hier_eq
```

⇒ ⇒ **公共符号 = 7 个**（6 个类型/入口 + 1 个工厂）⇒ ⇒ **MUST NOT** 按「总数」当质量指标
⇒ ⇒ A2 用**双向核对**（多一个 ⇒ 红 · 少一个 ⇒ 红）+ **A2' 能力判据**（逐项有可运行调用点
⇒ ⇒ 少任一项 ⇒ **红**）⇒ ⇒ 两者**独立**（★ 少了 A2' ⇒ 「砍功能凑数」恒绿）。

### 1.3 `make_observation` 的形状

⚠️ **问题**：用户要观测一个 `ch_out`，需要它的 `node_id` ⇒ ⇒ 而 `node_id` 的来源是
`port.impl()->id()` ⇒ ⇒ `impl()` 是**内部访问器** ⇒ ⇒ 让用户写它 = 把内部细节泄漏到公共面。

⇒ ⇒ **解法**：工厂函数 `make_observation(name, node_id, read)` ⇒ ⇒ **但 node_id 仍要用户给**！

⇒ ⇒ **真正的解法**：工厂**接收端口对象**并**自己取** `node_id`：

```cpp
/// ⚠️ **模板** ⇒ 用户只传端口 ⇒ ⇒ `node_id` 由 helper 填 ⇒ ⇒ 用户永不接触 `impl()`。
template <typename PortT>
Observation make_observation(std::string name, const PortT &port,
                              std::function<std::uint64_t(ch::Simulator &)> read);
```

⇒ ⇒ **代价**：模板 ⇒ ⇒ 需要 `port.impl()` 可用 ⇒ ⇒ `ch_out<T>` / `ch_in<T>` 的
`impl()` 是**公开**的（`include/core/io.h:243` / `:282`）⇒ ⇒ 不泄漏内部细节（用户**不**调用它
⇒ ⇒ 只是 helper 内部用）⇒ ⇒ ⇒ 公共头**必须** include `core/io.h`。

⚠️ **可移植性**：`core/io.h` 在 macOS/Windows 上可 include（✅ 无 `#error`）⇒ ⇒ OK。

⚠️ **命名**：`make_observation` 是**模板** ⇒ ⇒ 它不进「非模板符号」清单 ⇒ ⇒ 护栏 R2'
（按**声明 `namespace ch::hier_eq`** 计数）**仍**能抓到它（它声明在该 namespace 内）
⇒ ⇒ 双向核对的实现 MUST 覆盖模板。

### 1.4 `Observation::node_id` 的**用途**（保留理由）

`node_id` 的**唯一**用途 = 「被观测信号 MUST 出现在映射表里 ∧ `hier_top_port_name` **非空**」
的对账（capability `rtl-hierarchical-output` 的 Requirement ④/⑤ ⇒ ⇒ 那是**规范契约**
⇒ ⇒ 必须有机械载体）⇒ ⇒ ⇒ **保留** ⇒ ⇒ 且该对账 MUST 移进 **helper**（⇒ 用户拿不到
`signal_map` 之外的对账能力 ⇒ ⇒ 契约失去载体 ⇒ ⇒ 用户必须自己再实现一遍 ⇒ ⇒ 必然漂移）。

⇒ ⇒ ⇒ `compare` MUST 内部做「被观测侧」的对账（`node_id` ∈ `signal_map` ∧
`hier_top_port_name` 非空）⇒ ⇒ 不满足 ⇒ `ok=false` + 稳定错误码。

⚠️ **「盲区侧」的对账**（不在观测面内的 `ch_out` MUST 出现在映射表里 ∧
`hier_top_port_name` 为空）**留在仓内测试**（★ 需要枚举 DUT 的**全部** `ch_out` ⇒ ⇒ 那是
**DUT 的知识** ⇒ ⇒ helper 拿不到 ⇒ ⇒ 用户也拿不到 ⇒ ⇒ 仓内判据承担）。

### 1.5 端口面的**精确口径**（★ 落进公共头注释）

| 端口 | 扁平侧 | 层次化侧（`ForceLayer`） | 规则 |
|---|---|---|---|
| 根 `io()` 的 `ch_in` | 顶层端口 | **恒为**顶层端口（`include/hierarchy_verilog.h:136-139`） | ⇒ `DrivenInput` **只能**取根 `ch_in` |
| 根 `io()` 的 `ch_out` | 顶层端口 | **恒为**顶层端口（同上） | ⇒ `Observation` **只能**取根 `ch_out` |
| 子组件的 `ch_out` | **多出**为顶层端口（`src/codegen_verilog.cpp:71-110`） | **不在**顶层（移进子 module ⇒ ADR-058 §4 风险 6 盲区） | ⇒ **MUST NOT** 观测 |
| 未驱动的子 `ch_in` | 顶层输入（默认 0） | 冒泡成根 module 输入（**无 C++ 句柄**） | ⇒ ⇒ MUST 登记 |

⇒ ⇒ **观测集的可见性**由 `compare` 的对账**机械保证**（`hier_top_port_name` 非空 ⇒ 层次化侧
真的能读到）⇒ ⇒ ⇒ **观测一个不可见信号 ⇒ 直接 `ok=false`** ⇒ ⇒ 用户**不需要**自己查盲区。

## 2. 实现的**物理归属**

### 2.1 `src/hierarchy_equivalence_impl.h`（**内部头** · 不进 `include/`）

★ 仓内同款先例：`src/hierarchy_chain.h` · `src/hierarchy_port_check.h` ·
`src/hierarchy_port_set_check.h` · `src/hierarchy_signal_map_check.h`
⇒ ⇒ ⇒ **公共 API 面零额外扩张**（本 change **新增 1 个**公共头，那是用户要的）。

内容：现有 `tests/hier_equivalence.h` 的 `ch_test::hier::impl` **整体迁移**（产物生成 ·
backend 装配 · 逐拍驱动 · 目录创建 · 文件读取）+ **新增**：
- `check_observation_surface`（被观测侧对账）⇒ ⇒ 从测试体的**文件局部**代码上提
- **fail-closed** 校验（空输入 / 无随激励变化的信号 / >64bit 端口 / 无顶层输出）

### 2.2 `src/hierarchy_equivalence.cpp`（实现 · 登记进 `CMakeLists.txt`）

⚠️ **为什么实现 MUST 进库**（而不留在 `tests/`）：
- 用户要 **链接** `libcpphdl` 使用 ⇒ ⇒ 实现 MUST 是库的一部分
- ⇒ ⇒ 公共头**只**声明 ⇒ ⇒ **`include/` 不拖进** `core/verilator_backend.h`
  （★ 它在 `__cpp_lib_expected` 缺失时**显式 `#error`** ⇒ ⇒ 公共头拖进它 ⇒ ⇒
  macOS/Windows 的 `#include` 契约**破**）⇒ ⇒ ⇒ **实现里用它，公共头不用**

⇒ ⇒ **公共头的 include 面**（全部**无** `#error`）：
`component.h`（`Component` / `Granularity`）· `core/io.h`（`ch_out` / 工厂函数）·
`core/verilator_port_set.h`（`VerilatorPortSpec`）· `hierarchy_verilog.h`（`SignalMapEntry`）·
`simulator.h`（`Simulator`）· `<cstdint>` · `<functional>` · `<string>` · `<vector>`

### 2.3 `tests/hier_equivalence.h` 改为「include 公共头 + alias」

```cpp
#pragma once
#include "hierarchy_equivalence.h"     // ★ 公共面
namespace ch_test::hier {
using Observation = ch::hier_eq::Observation;
using DrivenInput = ch::hier_eq::DrivenInput;
using ProductSet  = ch::hier_eq::ProductSet;
using Result      = ch::hier_eq::Result;
using emit        = ch::hier_eq::emit;
using compare     = ch::hier_eq::compare;
using make_observation = ch::hier_eq::make_observation;
}
```

⇒ ⇒ 既有 `tests/test_hierarchy_equivalence.cpp`（9 用例 / 414 断言）**断言集合与期望值不变**
⇒ ⇒ 仅 `trace` → `flat_trace` 的**机械改名**（B1 放宽后的口径）。

## 3. fail-closed 契约（★ metis #5 / #6 ⇒ **新增能力**，不是既有行为）

⚠️ **实测**：现有 `compare_products`（`tests/hier_equivalence.h:132-209`）**不校验观测集**
⇒ ⇒ 空 / 恒定观测集 ⇒ `first_mismatch` 为空 ⇒ `equal=true, ok=true` ⇒ ⇒ **恒绿**
⇒ ⇒ 「至少一个随激励变化」目前**只**存在于**测试体**（`tests/test_hierarchy_equivalence.cpp:486-500`）
⇒ ⇒ ⇒ **本 change MUST 把这些校验搬进 helper**（⇒ ⇒ 用户不再可能踩坑）。

| # | 触发 | 行为 | 稳定错误码前缀 |
|---|---|---|---|
| F1 | `observations` 为空 | `ok=false` | `empty-observations` |
| F2 | `stimulus` 为空 | `ok=false` | `empty-stimulus` |
| F3 | 观测集非空但**无任何信号随激励变化** | `ok=false` | `no-varying-signal` |
| F4 | 产物生成失败 | `ok=false` | `emit-failed` |
| F5 | 派生端口集为空 | `ok=false` | `port-set-empty` |
| F6 | **>64bit 端口** | `ok=false` + **点名端口 + 位宽** | `port-width-64-exceeded` |
| F7 | **DUT 无顶层 `ch_out`** | `ok=false` + **点名成因** | `no-top-level-output` |
| F8 | 扁平 backend 初始化失败 | `ok=false` | `backend-init-failed` |
| F9 | 层次化 backend 初始化失败 | `ok=false` | `backend-init-failed` |
| F10 | verilator **不可用** | `ok=false` + **正确构建命令** | `verilator-unavailable` |
| F11 | 观测信号在层次化顶层**不可见** | `ok=false` + 点名信号 | `observation-not-visible` |

⇒ ⇒ **F3 的实现**：`compare` MUST 用**两组**激励跑 ⇒ ⇒ 判「某观测信号的取值集合在两组下不同」
⇒ ⇒ ⇒ **代价**：跑两遍 ⇒ ⇒ ⚠️ 已在草稿里登记为**新增工作量**（metis #6）。

⚠️ **F10 的正确构建命令**：`cmake --build build --target verilator`
⇒ ⇒ **MUST NOT** 建议 `apt install verilator`（★ 仓内已知陷阱：那条路**是错的**）。

## 4. 护栏改口径（`scripts/verify_hierarchical_p8.sh`）

| # | 现状（禁入） | **改后（受限入）** | 强度对比 |
|---|---|---|---|
| **R1** | `include/` 下**不得**出现 harness 头（文件名含 `equivalence`） | 「`include/` 下**文件名**匹配 `*equivalence*` 或 `*hier_eq*` 的文件 ⇒ MUST **逐字等于** `include/hierarchy_equivalence.h`」（**全称否定 + 单一字面豁免**） | v1 的「存在性」严格变弱 ⇒ ⇒ **改回全称** ⇒ ⇒ 新 ⊇ 旧 |
| **R2** | `include/` 下**不得**出现 harness 符号 | 同左 **+ 双向核对**（每个 deny 符号 MUST **仍存在于代码库某处** ⇒ ⇒ 否则改名后判据静默失效） | **增强** |
| **R2'** | —（无） | **新增**：「**声明** `namespace ch::hier_eq` 的文件 ⇒ MUST **恰好是** `include/hierarchy_equivalence.h`」 | **新增**（补 R1 的能力缺口） |
| **R3** | harness MUST 存在于 `tests/` | 「公共头 MUST **确实存在**」（**断言存在** ⇒ ⇒ MUST NOT 沿用「不存在 ⇒ 说明没有 ⇒ 绿」） | 语义**明确化** |
| **R4** | 公开 API 面 ⊆ **4** 个已裁决公共头（base/head 钉死） | **拆两条**：① **历史窗口**（原 R4，**逐字保留**）② **活账**：P8 之后新增的公开头 MUST 登记在 `.rddf/checks/public-api-surface.md` ⇒ ⇒ **双向核对** | **新增活账**（★ 「4→5 扩容」是**空操作**） |

⚠️ **`--self-test` MUST 同步改**（5 条投毒，**名字写死**）：

| # | 投毒 | 期望 |
|---|---|---|
| ① | 内部实现符号泄漏到公共面 | 红 |
| ② | 公共头**改名**（不再匹配豁免） | 红 |
| ③ | 公共头**缺失** | 红（**非零退出**） |
| ④ | **多出第 2 个公共头**，且名字**不**命中 `*equivalence*` / `*hier_eq*`（如 `include/hierarchy_compare_public.h`） | 红（★ 否则红的是**旧**判据 ⇒ 证明不了新机制） |
| ⑤ | 公共头 `#include` 了内部实现头 / `src/` 里的头 | 红（★ **R2 的盲区** ⇒ 符号名检查看不见 `#include` 图） |

⚠️ **CI 接线**：★ **实测** `verify_hierarchical_p8.sh` **当前不在 `.github/workflows/ci.yml`**
⇒ ⇒ MUST 接线（⇒ ⇒ 否则「强度不减」只是一次性本地证据）。

## 5. 确定性（`to_text` 不适用 ⇒ 本 helper **无** `to_text`）

⚠️ 与 P6 的 `HierarchyDiagnosis::to_text()` **不同**：本 helper 的产物是**两份 Verilog 文本**
⇒ ⇒ 「快照」的确定性由**产物字节**承担 ⇒ ⇒ ⇒ `emit` **两次** ⇒ 产物**逐字节相同**
（⇒ ⇒ 那是既有 T3-005 的 `[determinism]` 用例 ⇒ ⇒ 公共化后**沿用**）⇒ ⇒ ⇒
**MUST NOT** 另造一个 `to_text()`（⇒ ⇒ 无内容可「文本化」⇒ ⇒ 纯冗余）。

## 6. 逐文件改动清单

| 文件 | 改动 | 性质 |
|---|---|---|
| `include/hierarchy_equivalence.h`（**新**） | 公共 API：6 类型/入口 + 1 工厂 | **公开 API 面新增** |
| `src/hierarchy_equivalence_impl.h`（**新**） | 内部实现（从 `tests/hier_equivalence.h` 迁移 + 新增对账 + fail-closed） | **内部头**（不进 `include/`） |
| `src/hierarchy_equivalence.cpp`（**新**） | 实现 + 公共符号定义 | 实现（登记进 `CMakeLists.txt`） |
| `tests/hier_equivalence.h`（**改**） | 改为「include 公共头 + alias」 | 测试侧薄包装 |
| `tests/test_hierarchy_equivalence.cpp`（**改**） | `trace` → `flat_trace` 的**机械改名** | **零断言改动** |
| `tests/CMakeLists.txt`（**改**） | 登记 `src/hierarchy_equivalence.cpp`? ⇒ ⇒ **不**（那是主库的 `CMakeLists.txt`）⇒ ⇒ ⇒ 主 `CMakeLists.txt` 的 `CH_SOURCES` +1 | 登记 |
| `tests/test_public_equivalence_api.cpp`（**新**） | **out-of-tree** 子工程的**真实消费方**（只 `-I include` + 链接 `libcpphdl`）⇒ ⇒ 证明公共面**真的够用** | 测试 |
| `scripts/verify_hierarchical_p8.sh`（**改**） | 护栏改口径（R1 全称化 + R2 双向 + **R2'** + R3 明确化 + **活账**）+ `--self-test` 5 条投毒 | 护栏 |
| `.rddf/checks/public-api-surface.md`（**新**） | 公开 API 面**活账**（登记 + 双向核对的数据源） | 登记 |
| `.github/workflows/ci.yml`（**改**） | 接入 `verify_hierarchical_p8.sh` | CI 接线 |
| `docs/usage_guide/05-component-design.md`（**改**） | 新增用户面小节（示例 **MUST 由真实测试源文件承载**） | 文档 |
| `AGENTS.md`（**改**） | 计数锚点（`CTEST_TOTAL` +1 · non-perf +1 · `VERILATOR_E2E_TOTAL` +1） | 锚点 |
| `openspec/changes/p8-.../**`（**新**） | `proposal.md` / `design.md` / `tasks.md` / delta / `.openspec.yaml` | 工件 |

⚠️ **`.rddf/checks/` 目录**：本仓既有 `.rddf/checks/`（⇒ 已有 `input-port-required-registry.md`
先例）⇒ ⇒ 活账放这里与既有惯例一致。

## 7. 明确不做

- ✗ 不改发射器 / 分析层 / backend（**零** `src/hierarchy_*.cpp` 行为改动）
- ✗ 不改 `ch::analyze_hierarchy` / `ch::diagnose_hierarchy`（P6 的产物）
- ✗ 不把「A vs B」做成公共 API（走传递性）
- ✗ 不做「自动推断观测面」
- ✗ 不新增 ADR（§11.1 A4）
- ✗ **不**改 `src/hierarchy_verilog.cpp` 的**行为**（只允许「调用共享原语」式的**逐字等价**改动
  ⇒ ⇒ 若必须改 ⇒ ⇒ 判据 = 既有 3 个产物**逐字节不变**）
