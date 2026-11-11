// src/rtl_emit/manifest.cpp
//
// `rtl-emit-manifest-v1` 的**参考实现**（见 `include/rtl_emit/manifest.h` 的口径说明）。

#include "rtl_emit/manifest.h"

#include "ast/ast_nodes.h"
#include "codegen_naming.h"
#include "component.h"

#include <algorithm>
#include <deque>
#include <ostream>
#include <sstream>
#include <unordered_set>

namespace ch {
namespace rtl_emit {
namespace {

// ── JSON 字符串转义（最小实现 · 够用且正确）─────────────────────────
void json_escape(const std::string &in, std::ostream &out) {
    out << '"';
    for (const char c : in) {
        switch (c) {
        case '"':  out << "\\\""; break;
        case '\\': out << "\\\\"; break;
        case '\n': out << "\\n";  break;
        case '\r': out << "\\r";  break;
        case '\t': out << "\\t";  break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                char buf[8];
                std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                out << buf;
            } else {
                out << c;
            }
        }
    }
    out << '"';
}

void json_bool(bool v, std::ostream &out) { out << (v ? "true" : "false"); }

/// 「功能级」判据：从 `start` **前向可达**到某个 `type_output` 节点。
///
/// ⚠️ 用**显式队列 + visited 集**（图里可能有环 ⇒ ⇒ 递归会爆栈）。
bool reaches_observable_output(const core::lnodeimpl *start) {
    if (start == nullptr) {
        return false;
    }
    std::deque<const core::lnodeimpl *> queue{start};
    std::unordered_set<const core::lnodeimpl *> visited{start};
    while (!queue.empty()) {
        const core::lnodeimpl *node = queue.front();
        queue.pop_front();
        for (const core::lnodeimpl *user : node->get_users()) {
            if (user == nullptr) {
                continue;
            }
            if (user->type() == core::lnodetype::type_output) {
                return true;
            }
            if (visited.insert(user).second) {
                queue.push_back(user);
            }
        }
    }
    return false;
}

/// 该节点**是否被驱动**（节点级）。
///
/// ⚠️ **判据 = 「有驱动源」**，**不是**「有使用者」—— 实测踩坑：
///   每个 `ch_in` 节点都**恒有**一个自己的 **proxy** 用户
///   （`top.prog_we` ⇒ 用户 = `top._prog_we`）⇒ ⇒ 用 `get_users()` 判「被消费」
///   ⇒ ⇒ **恒为 true** ⇒ ⇒ 该字段**零信息量**。
///   ⇒ ⇒ 正确口径（与 schema 的 `ext_clk` 样例一致）：
///   · `ext_clk` **节点级确有消费者**（依据 `rv32i_pipeline_top.h:137` **有连线**）
///     ⇒ ⇒ 「有连线」= **有驱动源**；
///   · 而「功能级是死路」= 驱动的值**到不了**任何 `ch_out` ⇒ ⇒ 那才是 `effective`。
///
/// ⚠️ `input` 与 `output` 的「驱动源」取法**不同**：
///   · `input` ⇒ `inputimpl::driver()`（框架里 `<<=` 写入的就是它）；
///   · `output` ⇒ `outputimpl::src_driver()`（= `src(0)`）。
bool has_driver_source(const core::lnodeimpl *node) {
    if (node == nullptr) {
        return false;
    }
    switch (node->type()) {
    case core::lnodetype::type_input: {
        const auto *in = static_cast<const core::inputimpl *>(node);
        return in->driver() != nullptr;
    }
    case core::lnodetype::type_output: {
        const auto *out = static_cast<const core::outputimpl *>(node);
        return out->src_driver() != nullptr;
    }
    default:
        return false;
    }
}

}   // namespace

RtlManifest build_rtl_manifest(Component &root, core::context *ctx) {
    RtlManifest manifest;
    manifest.root = root.name();
    if (ctx == nullptr) {
        return manifest;
    }

    // ⚠️ **端口名 = 扁平产物的 Verilog 名**（与层次化产物**逐字相同**）
    //   ⇒ ⇒ 复用**同一个**命名表构造器 ⇒ ⇒ 两侧**不会**漂移。
    const auto names = build_verilog_name_table(ctx->get_eval_list());

    for (core::lnodeimpl *node : ctx->get_eval_list()) {
        if (node == nullptr) {
            continue;
        }
        const auto ty = node->type();
        const bool is_input = (ty == core::lnodetype::type_input);
        const bool is_output = (ty == core::lnodetype::type_output);
        // ⚠️ **clock / reset 也纳入清单** —— 它们是**产物顶层端口表**的成员
        //   （契约 `rtl-hierarchical-output`：「顶层端口表 MUST 是 clock → reset →
        //   无 driver 的 input → 全部 `ch_out`」）⇒ ⇒ 不纳入 ⇒ ⇒ 清单**不完整**
        //   ⇒ ⇒ 消费方（TB 生成器）**无法**据此复原端口面 ⇒ ⇒ 且 `role` 字段的
        //   `clock`/`reset` 两个取值**将无处可用** ⇒ ⇒ 矛盾。
        const bool is_clock_reset = (ty == core::lnodetype::type_clock ||
                                     ty == core::lnodetype::type_reset);
        if (!is_input && !is_output && !is_clock_reset) {
            continue;
        }

        RtlPortEntry entry;
        const auto it = names.find(node);
        entry.name = (it != names.end()) ? it->second : node->name();
        // ⚠️ **局部副本**：`entry` 稍后会被 `std::move` 进容器 ⇒ ⇒ 此后不可再读它
        const std::string port_name = entry.name;
        entry.dir = is_input ? "input" : (is_output ? "output" : "input");
        entry.width = node->size();
        entry.role = "data";
        if (ty == core::lnodetype::type_clock) {
            entry.role = "clock";
        } else if (ty == core::lnodetype::type_reset) {
            entry.role = "reset";
        }

        // ── owner：节点的**归属组件** ─────────────────────────────────
        Component *owner = node->get_parent();
        entry.owner = (owner != nullptr) ? owner->name() : std::string();
        const std::string owner_path = entry.owner;   // ← 局部副本（见上）

        // ── is_top_io：**根组件自有** IO（**不是** inline 后代输出）─────
        entry.is_top_io = (owner == &root);

        // ── cpp_member：节点名**去掉 owner 路径前缀**后的部分 ─────────
        // ⚠️ 框架**无反射** ⇒ ⇒ 未具名端口拿不到真实成员名（见头文件说明）
        {
            const std::string &node_name = node->name();
            const std::string &owner_path = entry.owner;
            if (!owner_path.empty() && node_name.rfind(owner_path + ".", 0) == 0) {
                entry.cpp_member = node_name.substr(owner_path.size() + 1);
            } else {
                entry.cpp_member = node_name;
            }
        }

        // ── driven（节点级 = 有驱动源）/ effective（功能级 = 可观测）────
        if (is_clock_reset) {
            // ⚠️ clock / reset 由**框架**驱动（不是设计里的信号）⇒ ⇒ 恒为「已驱动」
            //   且**驱动整个设计** ⇒ ⇒ 恒「有效」
            entry.driven = true;
            entry.effective = true;
        } else {
            entry.driven = has_driver_source(node);
            // ⚠️ `effective` = 该端口的值**能到达**某个 `ch_out`；它**本身就是** `ch_out`
            //   时**恒为 true**（它就是可观测面本身）
            entry.effective = is_output ? true : reaches_observable_output(node);
        }

        // ── semantic：v1 **无生产者** ⇒ ⇒ 恒 nullopt ─────────────────
        entry.semantic = std::nullopt;

        manifest.ports.push_back(std::move(entry));

        // ── C3 悬空输入诊断（**只诊断 · 不动端口面**）────────────────
        // ⚠️ 判据：归属组件**不是根**且**无 driver** ⇒ ⇒ 该 `ch_in` 是「悬空」的
        //   ⇒ ⇒ 记入 diagnostics[]（**不**改端口面 · **不**改顶层端口表）。
        if (is_input && owner != nullptr && owner != &root) {
            // ⚠️ **「有 driver」的判据 MUST 用 `inputimpl::driver()`**（**不是** `num_srcs()`）
            //   —— 实测：`ch_in` 节点**自身**的 `num_srcs()` 恒为 0（驱动落在
            //   **proxy** 上：`top.leaf._data_in`）⇒ ⇒ 用 `num_srcs()` 会把
            //   **每一个**被正常驱动的子组件端口都误报成「悬空」。
            //   ⇒ ⇒ `driver()` 才是 codegen 用的那条判据（`src/codegen_verilog.cpp`
            //   的 `in->driver() != nullptr`）⇒ ⇒ **同一套语义**。
            const auto *in_node = static_cast<const core::inputimpl *>(node);
            if (in_node->driver() == nullptr) {
                // ⚠️ ⚠️ **use-after-move 防护**：`entry` 上面已被 `std::move` 进容器
                //   ⇒ ⇒ 此处**不可**再读 `entry.name` / `entry.owner`（已被搬空）
                //   ⇒ ⇒ 必须用**局部副本**。
                RtlDiagnostic diag;
                diag.kind = "dangling_input";
                diag.port = port_name;      // ← 局部副本
                diag.owner = owner_path;     // ← 局部副本
                diag.detail = "该 ch_in 无 driver（未被任何表达式驱动）";
                manifest.diagnostics.push_back(std::move(diag));
            }
        }
    }

    // ⚠️ **确定性**：按名字排序 ⇒ ⇒ 同一设计两次生成**逐字节相同**（C5 的 F2 判据）
    std::sort(manifest.ports.begin(), manifest.ports.end(),
              [](const RtlPortEntry &a, const RtlPortEntry &b) { return a.name < b.name; });
    std::sort(manifest.diagnostics.begin(), manifest.diagnostics.end(),
              [](const RtlDiagnostic &a, const RtlDiagnostic &b) {
                  if (a.kind != b.kind) return a.kind < b.kind;
                  if (a.port != b.port) return a.port < b.port;
                  return a.owner < b.owner;
              });
    return manifest;
}

void write_rtl_manifest_json(const RtlManifest &manifest, std::ostream &out) {
    out << "{\n";
    out << "  \"schema\": ";
    json_escape(manifest.schema, out);
    out << ",\n";
    out << "  \"root\": ";
    json_escape(manifest.root, out);
    out << ",\n";

    out << "  \"ports\": [\n";
    for (std::size_t i = 0; i < manifest.ports.size(); ++i) {
        const RtlPortEntry &p = manifest.ports[i];
        out << "    {";
        out << "\"name\": ";    json_escape(p.name, out);
        out << ", \"dir\": ";   json_escape(p.dir, out);
        out << ", \"width\": " << p.width;
        out << ", \"role\": ";  json_escape(p.role, out);
        out << ", \"driven\": ";      json_bool(p.driven, out);
        out << ", \"effective\": ";   json_bool(p.effective, out);
        out << ", \"owner\": ";       json_escape(p.owner, out);
        out << ", \"is_top_io\": ";   json_bool(p.is_top_io, out);
        out << ", \"cpp_member\": ";  json_escape(p.cpp_member, out);
        out << ", \"semantic\": ";
        if (p.semantic.has_value()) {
            json_escape(*p.semantic, out);
        } else {
            out << "null";
        }
        out << "}";
        if (i + 1 < manifest.ports.size()) {
            out << ",";
        }
        out << "\n";
    }
    out << "  ],\n";

    out << "  \"diagnostics\": [\n";
    for (std::size_t i = 0; i < manifest.diagnostics.size(); ++i) {
        const RtlDiagnostic &d = manifest.diagnostics[i];
        out << "    {";
        out << "\"kind\": ";  json_escape(d.kind, out);
        out << ", \"port\": ";   json_escape(d.port, out);
        out << ", \"owner\": ";  json_escape(d.owner, out);
        out << ", \"detail\": "; json_escape(d.detail, out);
        out << "}";
        if (i + 1 < manifest.diagnostics.size()) {
            out << ",";
        }
        out << "\n";
    }
    out << "  ]\n";
    out << "}\n";
}

}   // namespace rtl_emit
}   // namespace ch
