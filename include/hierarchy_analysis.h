// include/hierarchy_analysis.h
//
// 层次化 RTL 输出的**分析层**（ADR-058 §2.1 · ADR-060 · ADR-061 · ADR-062）。
//
// 职责：把「组件树 + ctx 里的节点图」算成一份**结构化的模块视图**
// （模块清单 / 端口表 / 连接表）⇒ ⇒ 供发射器（T3-④）消费，且**可逐字节比对**。
//
// ⚠️ 本层 **MUST NOT** 改 IR（ADR-060 §2.1「图重写 MUST NOT 真的改 IR」）：
//    只读 `lnodeimpl` 的 `parent()` / `type()` / `num_srcs()` / `src()`
//    ⇒ ⇒ 否则 `toVerilog()` 的既有路径与仿真行为都会变。
//
// ⚠️ **本阶段（T3-②）的范围**：模块清单 + 节点折叠 + `self` / `clock_reset` /
//    `inline_folded` / `bubbled` 四类端口 + 连接表 + 三条前置校验。
//    **隐式端口合成**（跨模块边上「子侧端点不是端口」/ 兄弟边）是 **T3-④** 的义务
//    ⇒ ⇒ 本层遇到它 MUST **响亮拒绝**（MUST NOT 静默产出不完整的端口表
//       —— 那会让 golden 变成「把 bug 合法化」）。

#pragma once

#include "component.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace ch {

namespace core {
class lnodeimpl;
}   // namespace core

/// 端口的**来源分类**（写进 golden ⇒ 可审计）。
enum class HierarchyPortOrigin {
    Self,          ///< 该 module 自己的 `ch_in` / `ch_out`
    ClockReset,    ///< context 级 `default_clock` / `default_reset`（`owner == nullptr`）
    InlineFolded,  ///< 被 inline 的后代的 `ch_out` ⇒ 落到最近的成层祖先
    Bubbled,       ///< 无 driver 的子 `ch_in` ⇒ 逐级向上冒泡成 input（ADR-060 §2.2）
};

struct HierarchyPort {
    /// Verilog 名（**与扁平产物同一张全局表** ⇒ ADR-061 决策④；
    /// 全局去重 ⇒ 同一 module 内唯一，可直接用作端口名）
    std::string name;
    bool is_input = false;
    std::uint32_t width = 0;
    HierarchyPortOrigin origin = HierarchyPortOrigin::Self;
    const ch::core::lnodeimpl* node = nullptr;
};

/// 一条实例端口映射。
struct HierarchyConnection {
    std::string child_port;    ///< 子 module 的端口名
    std::string parent_side;   ///< 父 module 内的节点名
    /// `true`：子 `input` ← 父节点（父驱动子）；
    /// `false`：子 `output` → 父节点（子驱动父）
    bool into_child = true;
};

struct HierarchyInstance {
    std::string instance_name;   ///< 父 module 内的实例名（**确定性**派生）
    std::string module_key;      ///< 被实例化模块的 key（= 成层组件的全路径）
    std::vector<HierarchyConnection> connections;
};

/// 体内节点（不是端口 ⇒ 该 module 内的 wire / 寄存器 / 组合逻辑）。
struct HierarchyBodyNode {
    const ch::core::lnodeimpl* node = nullptr;
    std::string name;      ///< Verilog 名（与扁平产物同一张全局表）
};

struct HierarchyModule {
    std::string key;                  ///< 组件全路径（唯一；root 即组件名）
    std::string module_type_name;     ///< `<module_type_name>`（sanitize 归发射侧）
    bool is_root = false;
    std::vector<HierarchyPort> ports;
    /// 体内节点（按节点 id 升序 ⇒ 确定性）
    std::vector<HierarchyBodyNode> body;
    std::vector<HierarchyInstance> instances;
};

/// 分析层对「**需要隐式端口合成**」的跨 module 边的**响应方式**。
///
/// ⚠️ **默认 `Reject`** ⇒ ⇒ 「分析层 fail-closed」这条既有契约**一字不改**仍然成立
/// （T3-② 的判据继续有效）⇒ ⇒ 发射器改用 `Record`（把这类边**记录**下来，
/// 由发射器按 `PortPolicy` 决定「合成 + 告警 / 失败 / 首错即返」）。
///
/// ⚠️ **同一分类 · 两种响应**（不是两套判定标准）：边**怎么分类**始终一样，
/// 变的只是「遇到它怎么办」。
enum class ImplicitPortHandling {
    Reject,   ///< 响亮拒绝（**默认** · 分析层阶段就抛）
    Record,   ///< 记录到 `pending_implicit_edges`（⇒ 交给发射器处置）
};

/// 一条「需要隐式端口合成」的跨 module 边（ADR-060 §2.1）。
struct PendingImplicitEdge {
    std::string from_node;     ///< 驱动端节点名
    std::string from_module;   ///< 驱动端所属 module key
    std::string to_node;       ///< 被驱动端节点名
    std::string to_module;     ///< 被驱动端所属 module key
    /// 人类可读的成因（`sibling / non-adjacent modules` / `child side is not a port` / …）
    std::string reason;
    // ⚠️ 以下四个字段是 **B2b-1 合成**所需（发射器要拿到**节点**与**实例**）
    /// 驱动端节点（⇒ ⇒ 合成端口时要用它的**名字**做 `assign` 的 RHS）
    const ch::core::lnodeimpl *from_ptr = nullptr;
    /// 被驱动端节点（⇒ ⇒ 父侧实例映射的实参）
    const ch::core::lnodeimpl *to_ptr = nullptr;
    /// `true` = **子 → 父**（ADR-060 §2.1 行 2：父层读子内部节点 ⇒ 合成**子侧 output** 端口）
    /// `false` = **父 → 子**（行 1：父层驱动子内部节点 ⇒ 合成**子侧 input** 端口）
    bool child_drives_parent = false;
    /// 子实例在父 module 内的名字（**仅直接父子关系**下有意义 ⇒ ⇒ 非相邻边留空）
    std::string child_instance_name;
};

/// 一条「需要跨 module 边界的**冒泡**」（ADR-060 v1.13 §2.2 G3）。
///
/// ⚠️ 用途只有两个：`UndrivenPolicy::Reject` 的诊断、`Bubble` 的 `CHWARN` / 产物注释
/// ⇒ ⇒ **MUST NOT 进 `to_text()`**（T3-② 的 4 份 golden 不受影响）。
struct BubbledInput {
    const ch::core::lnodeimpl* node = nullptr;
    /// **inline 折叠后**的 owner module key（`== root` ⇒ 不跨 module 边界 ⇒ 不属「隐式悬空」）
    std::string module;
    /// 冒泡层级链（owner module → … → root），用于诊断点名「冒泡到哪几层」
    std::vector<std::string> chain;
};

/// 连接表的一行（**只记录跨 module 的边**）。
struct HierarchyEdge {
    std::string from_node;     ///< 驱动端（源）节点名
    std::string from_module;   ///< 驱动端所属 module key
    std::string to_node;       ///< 被驱动端（目的）节点名
    std::string to_module;     ///< 被驱动端所属 module key
};

struct HierarchyAnalysis {
    /// **拓扑序**：叶子在前、根在最后（⇒ 发射器按此顺序产出 module）
    std::vector<HierarchyModule> modules;
    std::vector<HierarchyEdge> edges;
    /// 分析期告警（**可审计**：既打 `CHWARN` 也留在这里 ⇒ 判据可断言）
    std::vector<std::string> warnings;
    /// 节点 → 折叠后的 module key（`owner == nullptr` ⇒ 根 module 的 key）
    /// ⇒ ⇒ 发射器据此判断「某条驱动边是否跨 module」（跨了就 MUST 由实例端口映射接管）
    std::unordered_map<const ch::core::lnodeimpl *, std::string> module_of_node;
    /// 需要**隐式端口合成**的跨 module 边（ADR-060 §2.1）⇒ ⇒ **仅在
    /// `ImplicitPortHandling::Record` 下填充**。
    /// ⚠️ **MUST NOT 进 `to_text()`** ⇒ ⇒ T3-② 的 4 份 golden 逐字节不变。
    std::vector<PendingImplicitEdge> pending_implicit_edges;
    /// **需要冒泡**的悬空 `ch_in`（ADR-060 v1.13 G3）—— 端口与**逐跳连接**都在本层生成。
    /// ⚠️ **MUST NOT 进 `to_text()`**（同上）。
    std::vector<BubbledInput> bubbled_inputs;

    /// 结构化文本快照（golden 形态 ⇒ 可逐字节比对）。
    /// ⚠️ 确定性：模块按拓扑序；端口按节点 id 升序；体内节点按 id 升序；
    ///    连接按 (from id, to id) 升序。
    std::string to_text() const;
};

/// 分析 `root` 所在的组件树 + 其 context。
///
/// 前置校验（**在分析之前**跑 ⇒ 诊断指向根因）三条：
///   ① 节点归属的组件**不在** `root` 的子树里 ⇒ 抛（给 `add_child` 修法）；
///   ② 组件已登记进树但**从未 `build()`** ⇒ 抛（它有节点、无端口）；
///   ③ **成层**组件自己驱动自己的 `ch_in` ⇒ 抛（子 module 内对 `input` 赋值非法）。
///
/// 另外两条**响亮拒绝**：
///   ④ 跨 module 的 `ch_mem` ⇒ **永远**抛（ADR-060：mem MUST 与使用者同层；该裁决是
///      「拒绝」⇒ ⇒ **不参与** `PortPolicy` 的 collect 语义）；
///   ⑤ 需要**隐式端口合成**的跨 module 边 ⇒ 由 `handling` 决定：**抛**（默认）或**记录**
///      （发射器按策略处置）。
///
/// ⚠️ **默认参数 = `Reject`** ⇒ ⇒ 「分析层 fail-closed」这条既有契约不变。
HierarchyAnalysis analyze_hierarchy(Component& root,
                                     ImplicitPortHandling handling = ImplicitPortHandling::Reject);

// ═══════════════════════════════════════════════════════════════════════════
// 层次友好度**诊断**（★ 只读 · capability `hierarchy-friendliness-diagnosis`）
// ═══════════════════════════════════════════════════════════════════════════

/// 层次友好度的**三档**结论。
///
/// ⚠️ **为什么是 3 档而不是 5 档**：报告的 5 大项里有 **4 项是正交事实**
/// （隐式边 / 悬空输入 / 未消费子输出 / 跨模块 mem）⇒ ⇒ 谁更糟**没有客观依据**
/// （默认 `PortPolicy::SynthesizeWarn` + `UndrivenPolicy::Bubble` ⇒ 隐式边与
/// 悬空输入**都是可发射的告警**）⇒ ⇒ 5 档是**伪全序** ⇒ ⇒ 收敛为 3 档，
/// 严重度信息放在**明细**里。
enum class HierarchyFriendliness {
    kClean = 0,          ///< 无隐式边 / 无悬空输入 / 无未消费子输出 / 无跨模块 mem
    kNeedsWork = 1,      ///< 至少一项「有代价的发现」（⇒ 详见 `details`）
    kNotAnalyzable = 2,  ///< 分析层拒绝 ⇒ ⇒ 报告**仍然返回**（★ **不抛异常**）
};

/// 一层 module 的规模（★ 三个数**分开给** ⇒ pass-through 层 `body=0` 但端口很多
/// ⇒ ⇒ 合并成单一 "scale" **必误导**）。
struct HierarchyModuleSize {
    std::string module;              ///< module key
    std::size_t port_count = 0;      ///< 该 module 的端口数
    std::size_t instance_count = 0;  ///< 该 module 内的子实例数
    std::size_t body_count = 0;      ///< 该 module 体内的节点数
};

/// 一条诊断明细（★ **value-only** ⇒ 不带 `lnodeimpl*` ⇒ 报告不耦合 IR 生命周期）。
struct HierarchyDiagnosisDetail {
    /// 明细类别：`"implicit_edge"` / `"dangling_input"` / `"unconsumed_child_output"`
    /// / `"cross_module_mem"` / `"not_analyzable"`
    std::string category;
    std::string module;   ///< 所属 module（★ 空 = 根 / 全局）
    std::string node;     ///< ★ **Verilog 名**（★ **NOT** `node->name()` ⇒ 那个对所有
                          ///< `ch_in` 恒为 `"unnamed_input"` ⇒ 明细会是一串同名条目）
    std::string reason;   ///< 人类可读成因（★ 各隐式边的成因不同 ⇒ 用户才知道该
                          ///< 「加一个端口」还是「加一条 N 跳链」）
};

/// 层次友好度诊断报告（★ **value-only**）。
///
/// ⚠️ **5 大项全部是 `std::optional`** ⇒ ⇒ 「**未测得**」与「**实测为 0**」
/// **结构上可区分** ⇒ ⇒ 不可分析时 MUST NOT 交出一份「全是 0」的误导性报告。
struct HierarchyDiagnosis {
    HierarchyFriendliness verdict = HierarchyFriendliness::kClean;

    /// 隐式**边**数（★ 单位 = 边，**不是**端口 ⇒ 见 `synthesized_port_count`）
    std::optional<std::size_t> implicit_edge_count;
    /// 需合成的**端口**数（★ ≈ 代价 ⇒ 同类隐式边的代价可能是 1 个端口也可能是
    /// N 跳端口链 ⇒ ⇒ 与发射器的隐式合成端口数**相等** ⇒ ★ **不含**向上
    /// pass-through（★ 它的数量 = 链长 ⇒ 不可由边数推导））
    std::optional<std::size_t> synthesized_port_count;
    /// **跨层**悬空输入数（★ 根自己的 / inline 折叠进根的**不计** ⇒ 与发射器同款口径）
    std::optional<std::size_t> dangling_input_count;
    /// 未被子层消费的输出数
    std::optional<std::size_t> unconsumed_child_output_count;
    /// 跨模块 `ch_mem` 访问数（★ 正常路径必为 0 —— 分析层对它是**永远抛**；
    /// ★ 不可分析路径由**一次独立预扫描**报出计数 ⇒ ⇒ 「有跨模块 mem」不会被
    /// 静默表达成「0」）
    ///
    /// ⚠️ **该数是「尽力而为」的**（★ **不是**精确值）：预扫描容错 —— 节点归属的
    /// 组件**不在**组件树内（前置校验①那类用户错误）时那些边会被**跳过**
    /// ⇒ ⇒ 计数可能**小于**真实值 ⇒ ⇒ 调用方 MUST NOT 把它当精确值。
    std::optional<std::size_t> cross_module_mem_count;

    std::size_t module_count = 0;
    std::vector<HierarchyModuleSize> module_sizes;
    std::vector<HierarchyDiagnosisDetail> details;

    /// **确定性快照**（★ 排序固定 ⇒ 同一设计连续两次诊断**逐字节相同**
    /// ⇒ ⇒ 「字段只增不改」这类兼容承诺**可机械核验**）
    std::string to_text() const;
};

/// **只读**诊断入口（★ **单一判定实现**）。
///
/// ⚠️ 与 `analyze_hierarchy` 的关系：
/// · 内部 MUST 用 `ImplicitPortHandling::Record`（★ 默认 `Reject` ⇒ 遇到第一条
///   隐式边就抛 ⇒ 诊断会退化成「不可分析」⇒ 与「一次报全」的目的相反）；
/// · 分析层**拒绝**时 ⇒ 返回 `kNotAnalyzable` 的报告（★ **不抛异常**）⇒ ⇒
///   用户拿到的是**报告**而不是异常；
/// · ⚠️ **只捕 `std::runtime_error`**（★ 分析层的 15 个抛点全是它）⇒ ⇒
///   `std::out_of_range` / `std::bad_alloc` 会**冒泡** ⇒ ⇒ 框架 bug **不会**被
///   伪装成「你的设计不可分析」。
///
/// ⚠️ **两个副作用**（由 `CHFATAL_EXCEPTION` 的定义决定 ⇒ 用户须知）：
/// · 诊断路径**必然**在 stderr 出现 `FATAL:`（★ 该宏**先 log 再 throw**）；
/// · **静态析构期**该宏**一律不抛** ⇒ ⇒ 该期可能得到**部分结果**。
///
/// ⚠️ **结果依赖当前 `GranularityScope`**（★ inline 折叠会改变 module 归属 ⇒
/// ⇒ 同一设计在不同粒度下的诊断结果**可能不同** ⇒ ⇒ 例如跨模块 `ch_mem` 在
/// `ForceInline` 下折进同层 ⇒ 0）。
HierarchyDiagnosis diagnose_hierarchy(Component& root);

}   // namespace ch
