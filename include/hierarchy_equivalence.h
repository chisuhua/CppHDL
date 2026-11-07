// include/hierarchy_equivalence.h
//
// **层次化 ≡ 扁平 逐拍等价性 helper**（P8 · 公共 API）。
//
// ⚠️ **规范所有者**：本头管的是「**怎么用**这个 helper」的用户面契约
//   （入口形状 · 公共面闭合集合 · fail-closed 语义 · 调用方义务 · 判别力边界声明）
//   ⇒ ⇒ **产物契约**（层次化产物的结构 / 端口命名 / 等价性判据**本体**）归
//   capability `rtl-hierarchical-output` ⇒ ⇒ 本头 **MUST NOT** 复述它。
//
// ─────────────────────────────────────────────────────────────────────────────
// ⚠️ **为什么需要它**
// ─────────────────────────────────────────────────────────────────────────────
// 层次化产物的等价性 harness 此前只在 `tests/` 内部 ⇒ ⇒ **用户自己的设计拿不到**
// 「层次化后行为不变」的逐拍反馈（台账 §7 残留 1）。
// 而手工复现要重写四步，每一步都有**静默失效**的陷阱：
//   ① 生成层次化产物 —— `GranularityScope` 是**静态**开关 ⇒ MUST 只包住发射
//   ② 派生端口集合 —— MUST 由 emitter 产物派生（`backend_port_set`）⇒ MUST NOT 手写
//   ③ 装配两个 backend —— 扁平侧 MUST 用「backend 自己生成的那份 `top.v`」
//   ④ 施加激励 —— MUST 走 `set_input_value(port, <纯整数>)`
//      （`<<=` 会把顶层端口变成「有内部驱动」⇒ 两侧观测面不一致 ⇒ **假分歧**）
// ⇒ ⇒ **门槛不在「写多少行」，在「知道每个陷阱在哪」** ⇒ ⇒ 这正是本 helper 的价值。
//
// ─────────────────────────────────────────────────────────────────────────────
// ⚠️ **公共面 = 一个闭合集合**（★ **MUST NOT** 以「符号数量少」当质量指标）
// ─────────────────────────────────────────────────────────────────────────────
// `emit` / `compare` / `ProductSet` / `Result` / `DrivenInput` / `Observation`
// + 工厂 `make_observation`。
// ⇒ ⇒ 第 5/6 项（`DrivenInput` / `Observation`）是「**调用所必需**」⇒ ⇒ **不是**
// 「新增承诺」⇒ ⇒ ⇒ 文档 MUST 分开列。
//
// ─────────────────────────────────────────────────────────────────────────────
// ⚠️ **判别力边界**（★ 必读 —— 否则会误用）
// ─────────────────────────────────────────────────────────────────────────────
// 本 helper 比较的是**两份 Verilog 产物**（扁平 vs 层次化）⇒ ⇒ 它**只**判别
// 「**产物变换**是否引入行为分叉」⇒ ⇒ ⇒ 它**不**判别「产物本身的语义正确性」
// （★ 两份产物**同源** ⇒ ⇒ 同源缺陷**一起错** ⇒ ⇒ 结构性不可判别）。
//
// ⚠️ 它与「同一设计的两种执行路径之比较」（既有双后端 parity）是**互补**关系
// ⇒ ⇒ ⇒ **MUST NOT** 被读成「做了它就够了」：
//   · 本 helper：**C++ 同源** ⇒ 抓「层次化**变换**算错」⇒ 抓不到「共享发射底座算错」
//   · 双后端 parity：**异源** ⇒ 抓「**两套执行后端**不一致」⇒ 抓不到「变换算错」
//
// ⚠️ **观测盲区**：成层子树的 `ch_out` 在层次化顶层**不可观测**
//   （ADR-058 §4 风险 6）⇒ ⇒ ⇒ 本 helper **MUST NOT** 让用户观测它们
//   ⇒ ⇒ ⇒ 观测了 ⇒ ⇒ `compare` 会**机械拒绝**（前缀 `observation-not-visible`）。
//
// ⚠️ **观测面 MUST 手写**（★ 本 helper **不**自动推断）：
//   · 自动推断 = 回到「全部输出」的空洞判据 ⇒ ⇒ 且会踩上面的盲区
//   · 观测集合 MUST **非空** 且 MUST 含**至少一个随激励变化**的信号
//     ⇒ ⇒ 否则「逐拍全等」**恒绿** ⇒ ⇒ ⇒ 本 helper 会**拒绝**（前缀
//     `no-varying-signal` / `empty-observations` / `empty-stimulus`）。
//
// ⚠️ **端口口径**（★ 根端口恒在顶层 ⇒ ⇒ 子端口不在）：
//   · 根 `io()` 的 `ch_in` / `ch_out` ⇒ 两种粒度下**恒为顶层端口**
//     （`backend_port_set` 只取根 module 端口且 MUST 含 clock/reset）
//   · **子组件**的 `ch_out` ⇒ 扁平侧**多出**为顶层端口，层次化侧**不在**顶层
//     ⇒ ⇒ ⇒ `Observation` **只能**取**根** `ch_out`（否则 ⇒ ⇒ 被机械拒绝）
//
// ⚠️ **平台**：Verilator backend 是**显式 Linux-only**（`-DBUILD_VERILATOR=ON` 在
//   macOS/Windows 上于 configure 期 `FATAL_ERROR`）⇒ ⇒ 本 helper 在那两类平台上
//   **可编译**但**不可用**（⇒ ⇒ 运行时返回前缀 `verilator-unavailable`）。

#pragma once

#include "component.h"
#include "core/io.h"
#include "core/verilator_port_set.h"
#include "hierarchy_verilog.h"
#include "simulator.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace ch::hier_eq {

/// 一个**受驱动的顶层输入**。
///
/// ⚠️ `set` 回调 MUST 走 `Simulator::set_input_value(port, <纯整数>)`。
/// ⚠️ **MUST NOT** 用 `<<=` —— 它会把顶层端口变成「有内部驱动」⇒ ⇒ 该端口不再是
/// 顶层端口 ⇒ ⇒ 两侧观测面不一致 ⇒ ⇒ **假分歧**。
struct DrivenInput {
    std::string name;
    std::function<void(ch::Simulator &, std::uint64_t)> set;
};

/// 一个**被观测**的信号（观测集合 `O` 的元素）。
///
/// ⚠️ `node_id` MUST 由 `make_observation(...)` 填 ⇒ ⇒ **MUST NOT** 手写
/// （⇒ ⇒ 用户**不**接触 `impl()` 这个内部访问器）。
///
/// ⚠️ `node_id` 的用途 = 让 `compare` 能做「被观测信号 MUST 出现在映射表里 ∧
/// `hier_top_port_name` **非空**」的对账 ⇒ ⇒ ⇒ 观测了一个**层次化顶层不可见**的
/// 信号 ⇒ ⇒ `compare` 会**机械拒绝**（前缀 `observation-not-visible`）。
struct Observation {
    std::string name;
    std::function<std::uint64_t(ch::Simulator &)> read;
    std::uint32_t node_id = 0;
};

/// 一次「产物生成」的结果。
struct ProductSet {
    bool ok = false;                                  ///< 生成是否成功
    std::string error;                                ///< 失败原因（★ **稳定前缀**）
    std::string hier_path;                            ///< 层次化产物路径
    std::vector<ch::VerilatorPortSpec> ports;          ///< **由产物派生**（★ 禁手写）
    std::vector<ch::SignalMapEntry> signal_map;        ///< **由产物派生**
};

/// 一次「逐拍比对」的结果。
struct Result {
    bool ok = false;                                   ///< 运行本身成功
    bool equal = false;                                ///< 逐拍全等
    std::string error;                                 ///< 失败原因（★ **稳定前缀**）
    std::string first_mismatch;                        ///< 空 = 全等
    std::size_t mismatch_count = 0;
    /// ⚠️ **扁平侧**逐拍观测值（★ **不是**「两侧轨迹」⇒ ⇒ 层次化侧的值**不**保留
    ///    ⇒ ⇒ 名字 MUST 体现这一点）。
    std::vector<std::vector<std::uint64_t>> flat_trace;
    std::string flat_product;                          ///< 扁平产物文本
    std::string hier_product;                          ///< 被仿真的层次化产物文本
};

/// ① 生成层次化产物 + **由产物派生**端口集合 + 映射表。
///
/// ⚠️ `Granularity` 是**发射期**生效的（`GranularityScope` 是**静态**开关 ⇒ ⇒
/// MUST 只包住发射 ⇒ ⇒ 不得泄漏到别的阶段）。
///
/// @param granularity 层级粒度（`ch::Granularity` · ADR-061 决策⑤）
ProductSet emit(ch::Component &root, const std::string &work_dir,
                 ch::Granularity granularity = ch::Granularity::RespectDeclarations);

/// ② 逐拍比对「扁平产物」与「层次化产物」。
///
/// ⚠️ **A 侧固定是「扁平产物」** ⇒ ⇒ 「两个层次化粒度互比」**不**直接支持 ⇒ ⇒
/// 走**传递性**（A ≡ 扁平 ∧ B ≡ 扁平 ⇒ A ≡ B）。
///
/// ⚠️ `ports` MUST 是 `emit` 派生出来的（★ 禁手写 ⇒ ⇒ 否则与 emitter 真相脱钩）。
///
/// @param hier_path 被仿真的层次化产物（★ **可**指向被篡改的副本 ⇒ 判别力自证用）
/// @param ports     由**原始**产物派生的端口集合
/// @param inputs    受驱动的顶层输入
/// @param observations 被观测的顶层输出（★ MUST 非空且含至少一个随激励变化的信号）
/// @param stimulus  逐拍激励（★ 每行对应一拍 ⇒ ⇒ MUST 非空）
///
/// ⚠️ **失败前缀清单**（★ 稳定 ⇒ ⇒ 可机械分类）：
///   `empty-observations` · `empty-stimulus` · `no-varying-signal` · `emit-failed` ·
///   `port-set-empty` · `port-width-64-exceeded` · `no-top-level-output` ·
///   `backend-init-failed` · `verilator-unavailable` · `observation-not-visible`
///
/// ⚠️ **`no-varying-signal` 只在激励 ≥ 2 拍时判**（★ 单拍**无法**判断「观测是否
///    随激励变化」⇒ ⇒ ⇒ 单拍激励**豁免**本判据）⇒ ⇒ 但**单拍比较的判别力天然很弱**
///    ⇒ ⇒ 想真正验证行为等价 ⇒ ⇒ MUST 给**多拍且激励值不同**的激励。
Result compare(ch::core::context *ctx, const std::string &work_dir,
               const std::string &hier_path,
               const std::vector<ch::VerilatorPortSpec> &ports,
               const std::vector<DrivenInput> &inputs,
               const std::vector<Observation> &observations,
               const std::vector<std::vector<std::uint64_t>> &stimulus);

/// 观测信号的**工厂**（★ 用户 MUST NOT 手填 `node_id` ⇒ ⇒ 公共文档里不出现
/// `impl()` 这个内部访问器）。
///
/// @param port 顶层 `ch_out`（★ **只能**取**根**的 ⇒ ⇒ 子组件的 `ch_out` 在层次化
///        顶层不可观测 ⇒ ⇒ 观测它 ⇒ ⇒ `compare` 会机械拒绝）
template <typename PortT>
Observation make_observation(std::string name, const PortT &port,
                              std::function<std::uint64_t(ch::Simulator &)> read) {
    Observation o;
    o.name = std::move(name);
    o.read = std::move(read);
    const auto *impl = port.impl();
    o.node_id = (impl != nullptr) ? impl->id() : 0u;
    return o;
}

}  // namespace ch::hier_eq
