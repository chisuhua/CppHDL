// include/rtl_emit/manifest.h
//
// **端口清单（manifest）** 的构建与序列化 —— capability `rtl-emit-portability`
// 的 schema `rtl-emit-manifest-v1` 的**参考实现**。
//
// ⚠️ **本文件是「库」侧**：用户在自己的可执行文件里**登记**自己的设计 ⇒ ⇒ 调用
//   `build_rtl_manifest()` 拿清单 ⇒ ⇒ 写盘 / 喂 TB 生成器。
//   ⇒ ⇒ 仓内的 CLI（`tools/cpphdl-emit-rtl`）只是**薄壳**（演示用法 + 自测）。
//
// ⚠️ **schema 的规范所有者** = capability `rtl-emit-portability`
//   （`openspec/specs/rtl-emit-portability/spec.md`）⇒ ⇒ 本文件**实现**它，
//   **不另立**字段口径。
//
// ⚠️ **端口面归 `rtl-hierarchical-output`** ⇒ ⇒ 本文件**不改**任何端口、
//   **不选**哪些端口上顶层 ⇒ ⇒ 只**记录**事实 ⇒ ⇒ `is_top_io` 是**描述**，
//   不是**过滤器**（⚠️ 筛选是**消费方**的选择）。

#pragma once

#include "core/context.h"
#include "core/lnodeimpl.h"

#include <cstdint>
#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

namespace ch {
namespace rtl_emit {

// ── 一个端口条目（schema 的 10 个字段）────────────────────────────────
struct RtlPortEntry {
    /// RTL 全名（产物里的名字）—— 与 `rtl-hierarchical-output` 的端口表**逐字相同**
    std::string name;
    /// `input` / `output`
    std::string dir;
    /// 位宽（bit）
    std::uint32_t width = 0;
    /// `clock` / `reset` / `data`
    std::string role;
    /// **节点级**：该端口**是否被消费**（有下游使用者）
    bool driven = false;
    /// **功能级**：驱动它**是否产生可观测行为**（前向可达到某个 `ch_out`）
    bool effective = false;
    /// **所属组件路径**（组件实例全路径）
    std::string owner;
    /// 是否**根组件自有 IO**（**不是** inline 后代输出）
    bool is_top_io = false;
    /// **C++ 成员路径**（组件内的成员名 / 成员访问路径）
    std::string cpp_member;
    /// **可选** —— v1 **无生产者** ⇒ ⇒ 恒为 `nullopt`
    std::optional<std::string> semantic;
};

// ── 一条诊断（**只诊断 · 不动端口面**）───────────────────────────────
struct RtlDiagnostic {
    /// 诊断种类（当前只有 `dangling_input`）
    std::string kind;
    /// 相关端口名
    std::string port;
    /// 相关组件路径
    std::string owner;
    /// 人类可读的细节
    std::string detail;
};

// ── 端口清单 ─────────────────────────────────────────────────────────
struct RtlManifest {
    /// schema 标识（**固定** ⇒ ⇒ 消费方可据此判版本）
    std::string schema = "rtl-emit-manifest-v1";
    /// 根组件路径
    std::string root;
    /// 端口条目（**按名字排序** ⇒ ⇒ 清单**确定性**）
    std::vector<RtlPortEntry> ports;
    /// 诊断（**不阻断** ⇒ ⇒ 只记录）
    std::vector<RtlDiagnostic> diagnostics;
};

/// 构建端口清单。
///
/// @param root  根组件（其 `name()` 是清单里 `root` 字段的值）
/// @param ctx   该组件所属的 context（**必须**是 `root` 所在的那个）
///
/// ⚠️ **口径**：
/// · **端口集合** = `ctx->get_eval_list()` 里的 `type_input` / `type_output` 节点
///   ⇒ ⇒ 与层次化分析层（`src/hierarchy_analysis.cpp`）**同一来源** ⇒ ⇒ 不会漂移；
/// · **`name`** = 该节点的 **Verilog 名**（与扁平产物 / 层次化产物**逐字相同**）；
/// · **`owner`** = 该节点的**归属组件**的路径（`owner == nullptr` ⇒ ctx 级
///   clock/reset ⇒ ⇒ 归属记为**空串**）；
/// · **`cpp_member`** = 节点名**去掉 owner 路径前缀**后的部分
///   ⇒ ⇒ 对**显式命名**的端口，它**恰好**是 C++ 成员名
///   （N2 护栏 P1 强制「标识符 == 名字串」）⇒ ⇒ 对**未具名**端口它是 `unnamed_input`
///   ⇒ ⇒ ⚠️ **不是**真实的成员名（框架**无反射** ⇒ ⇒ 拿不到）⇒ ⇒ 见下 `member_known`；
/// · **`driven`** = 该端口**有下游使用者**（`get_users()` 非空）
///   ⇒ ⇒ 复用 `diagnose_drive()` 的**同一套**节点状态语义（不另造判定）；
/// · **`effective`** = 从该端口**前向可达**到**至少一个** `type_output` 节点
///   ⇒ ⇒ 「功能级」的可观测行为判据。
///
/// ⚠️ **`member_known` 的诚实边界**：框架**无反射** ⇒ ⇒ 未具名端口的 C++ 成员名
///   **拿不到** ⇒ ⇒ 本实现把 `cpp_member` 记为 `unnamed_input`（**可识别**的占位）
///   ⇒ ⇒ **消费方 MUST 把「`cpp_member == "unnamed_input"`」理解为「未知」**
///   ⇒ ⇒ （要真正拿到成员名 ⇒ ⇒ 端口**必须具名** ⇒ ⇒ N2 护栏就是为此存在）。
RtlManifest build_rtl_manifest(Component &root, core::context *ctx);

/// 把清单写成 **JSON**（`rtl-emit-manifest-v1`）。
///
/// ⚠️ **确定性**：端口按名字排序 ⇒ ⇒ 同一设计**两次生成逐字节相同**
///   （C5 护栏的 F2 判据）。
void write_rtl_manifest_json(const RtlManifest &manifest, std::ostream &out);

}   // namespace rtl_emit
}   // namespace ch
