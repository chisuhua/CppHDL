// tests/hier_corpus_fixture.h
//
// **层次化路径的语料级回归 · 共享 fixture**（残留 1b · capability
// `hierarchical-corpus-regression`）。
//
// ⚠️ **为什么是「fixture」而不是「每个 DUT 一份实现」**：本 change 的判据 MUST 作用在
//   **既有语料的真实 DUT** 上 ⇒ ⇒ 而既有 DUT 的类**大多在各自测试文件的匿名命名空间
//   内** ⇒ ⇒ **无法**从外部枚举/引用 ⇒ ⇒ ⇒ 所以「用哪些 DUT」这个事实**只能**留在
//   那些文件里 ⇒ ⇒ ⇒ 而「怎么查」是**通用**的 ⇒ ⇒ ⇒ 抽到这里。
//
// ⚠️ **本 fixture 用的是 P8 的公共 API**（`ch::hier_eq`）⇒ ⇒ ⇒ 它**同时**是
//   「公共面真的够用」的**第二个**消费方（第一个是 `tests/test_public_equivalence_api.cpp`
//   ⇒ ⇒ 第三个是本 change 的 T1 用例 ⇒ ⇒ 三个消费方在**三个不同的文件**里 ⇒ ⇒
//   ⇒ ⇒ 这本身就是「公共面够用」的**三方**证据）
//   ⇒ ⇒ ⇒ **零**重复实现 ⇒ ⇒ ⇒ 与 emitter 漂移的风险为 0。
//
// ⚠️ **产物级检查（T0）** 的 6 项：
//   ① 发射成功（`ok` 且 `error` 为空）
//   ② **`ForceLayer` 下产物含 ≥ 2 个 module**（★ module 清单取自**发射器自己的**
//      `signal_map` ⇒ ⇒ **MUST NOT** 自研文本解析器）
//   ③ 派生端口集**自洽**（每个 input 端口对应 ctx 里的一个 `type_input` 节点；
//      clock / reset 齐备）
//   ④ 确定性（两次发射 ⇒ 产物**逐字节相同**）
//   ⑤ 层次化产物 **≠** 扁平产物（⇒ ⇒ 证明「分层真的改变了产物」）
//   ⑥ **lint** 门（`verilator --lint-only --top-module top` ⇒ ⇒ **无** `-Wall`、
//      **无** `-Wno-fatal` ⇒ ⇒ 与 `scripts/verify_hierarchical_rtl.sh` 的 R1 同款）
//      ⇒ ⇒ verilator 不可用 ⇒ ⇒ **显式 skip**（★ **MUST NOT** 静默通过）
//
// ⚠️ **组合逻辑 DUT 的处理**：本 fixture **不**假设 DUT 有寄存器 ⇒ ⇒ 组合逻辑 DUT
//   （子组件 0 reg）同样适用 ⇒ ⇒ 但它们的「时序判别力」为 0 ⇒ ⇒ **必须登记**
//   （见 `tests/hier_corpus_registry.md`）。

#pragma once

#include "ch.hpp"
#include "codegen_verilog.h"
#include "external_tool_helpers.h"
#include "hierarchy_equivalence.h"
#include "hierarchy_verilog.h"

#include <cstdio>
#include <fstream>
#include <set>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace ch_test::hier_corpus {

/// 一次「语料级层次化回归 · 产物级」的结论。
struct ProductLevel {
    bool ok = false;                       ///< 全部**非 lint** 项都通过
    std::string error;                     ///< 失败原因（点名）
    std::size_t module_count = 0;          ///< 产物里的 module 数
    std::size_t input_port_count = 0;      ///< 派生端口集里的 input 数
    std::size_t output_port_count = 0;     ///< 派生端口集里的 output 数
    bool deterministic = false;            ///< 两次发射逐字节相同
    bool differs_from_flat = false;        ///< 层次化产物 ≠ 扁平产物
    bool lint_ok = false;                  ///< lint 门通过
    bool lint_skipped = false;             ///< verilator 不可用 ⇒ 显式跳过
    std::string hier_path;                 ///< 层次化产物路径
};

/// 建目录（单层；**已存在 ⇒ 无害**）。
///
/// ⚠️ **为什么 fixture 必须自己建**：`ch::hier_eq::emit` 在 `work_dir` **之下**建
///   `flat/` `hier/`，但**不建** `work_dir` 本身 ⇒ ⇒ 调用方 MUST 先建它
///   ⇒ ⇒ （公共 API 的这条义务已在 `docs/usage_guide/05-component-design.md` §5.4-ter
///   写明 ⇒ ⇒ **不是**遗漏）。
inline bool ensure_dir(const std::string &path) {
    if (::mkdir(path.c_str(), 0755) == 0) {
        return true;
    }
    return ::access(path.c_str(), F_OK) == 0;
}

/// 读整个文件（不存在 ⇒ 空串）。
inline std::string read_whole_file(const std::string &path) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        return {};
    }
    return std::string((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
}

/// 产物的 module 清单（★ **发射器自己的**真相 ⇒ ⇒ `signal_map[].hier[].module`
///    ⇒ ⇒ **MUST NOT** 自研文本解析器（★ 仓内既有教训：自研解析器「不可能完全闭合」）。
inline std::set<std::string> modules_of(const std::vector<ch::SignalMapEntry> &map) {
    std::set<std::string> mods;
    for (const auto &e : map) {
        for (const auto &loc : e.hier) {
            mods.insert(loc.module);
        }
    }
    return mods;
}

/// 派生端口集的**自洽性**：每个 input 端口 MUST 对应 ctx 里的一个 `type_input` 节点
/// （⇒ ⇒ 「端口集与 ctx 不自洽」会被抓住）⇒ ⇒ 且 clock / reset MUST 齐备。
inline bool ports_self_consistent(ch::core::context *ctx,
                                  const std::vector<ch::VerilatorPortSpec> &ports,
                                  std::string &detail) {
    if (ctx == nullptr) {
        detail = "null context";
        return false;
    }
    bool has_clock = false;
    bool has_reset = false;
    for (const auto &port : ports) {
        if (port.is_clock) {
            has_clock = true;
        }
        if (port.is_reset) {
            has_reset = true;
        }
        if (!port.is_input) {
            continue;
        }
        const ch::core::lnodeimpl *node = nullptr;
        for (const auto &owned : ctx->get_nodes()) {
            if (owned->id() == port.node_id) {
                node = owned.get();
                break;
            }
        }
        if (node == nullptr) {
            detail = "derived input port '" + port.port_name + "' has no ctx node";
            return false;
        }
        // ⚠️ 框架的 clock / reset 是**独立**的节点类型（★ **不是** `type_input`）
        //    ⇒ ⇒ 它们**必须**被接受 ⇒ ⇒ 否则本检查会**假红**（实测：`default_clock`
        //    被判「does not correspond to a ctx input node」）。
        const bool is_framework_infra = (node->type() == ch::core::lnodetype::type_clock) ||
                                        (node->type() == ch::core::lnodetype::type_reset);
        if (node->type() != ch::core::lnodetype::type_input && !is_framework_infra) {
            detail = "derived input port '" + port.port_name +
                     "' does not correspond to a ctx input node";
            return false;
        }
    }
    if (!has_clock || !has_reset) {
        detail = "derived port set lacks the framework clock/reset ports";
        return false;
    }
    return true;
}

/// 一次「产物级」检查（★ 模板 ⇒ ⇒ DUT 类型在**调用点**可见 ⇒ ⇒ 复用**真实**语料 DUT）。
///
/// @param tag       该 DUT 的标识（用于产物文件名 ⇒ ⇒ 互不覆盖）
/// @param work_root 工作目录的父路径（调用方给）
template <typename DutType>
ProductLevel check_product_level(const std::string &tag, const std::string &work_root) {
    ProductLevel r;
    const std::string dir_a = work_root + "/" + tag + "_a";
    const std::string dir_b = work_root + "/" + tag + "_b";
    // ⚠️ `emit` **不建** `work_dir` ⇒ ⇒ MUST 由调用方建（见 `ensure_dir` 的注释）
    if (!ensure_dir(dir_a) || !ensure_dir(dir_b)) {
        r.error = "cannot create work dirs under " + work_root;
        return r;
    }

    // ⚠️ **MUST NOT** 在这里套一层 `GranularityScope`：它是**静态**开关 ⇒ ⇒ 外层 scope
    //    会在 DUT 的**构造/describe 期**就生效 ⇒ ⇒ 与既有 T3-005 的用法（`emit` 内部
    //    自带 scope）**不一致** ⇒ ⇒ ⇒ 粒度**只**在发射期生效（ADR-061 决策⑤）。
    ch::ch_device<DutType> dev;

    // ① 发射（★ 两次 ⇒ 顺便验 ④ 确定性）
    const auto a = ch::hier_eq::emit(dev.instance(), dir_a, ch::Granularity::ForceLayer);
    if (!a.ok) {
        r.error = "emit failed: " + a.error;
        return r;
    }
    const auto b = ch::hier_eq::emit(dev.instance(), dir_b, ch::Granularity::ForceLayer);
    if (!b.ok) {
        r.error = "second emit failed: " + b.error;
        return r;
    }
    r.hier_path = a.hier_path;

    // ② module 数（★ 取自**发射器自己的** `signal_map`）
    const auto mods = modules_of(a.signal_map);
    r.module_count = mods.size();
    if (r.module_count < 2) {
        r.error = "ForceLayer produced fewer than 2 modules (" +
                  std::to_string(r.module_count) + ")";
        return r;
    }

    // ③ 派生端口集自洽
    std::string detail;
    if (!ports_self_consistent(dev.context(), a.ports, detail)) {
        r.error = "derived port set is not self-consistent: " + detail;
        return r;
    }
    for (const auto &port : a.ports) {
        if (port.is_input) {
            ++r.input_port_count;
        } else {
            ++r.output_port_count;
        }
    }
    if (r.output_port_count == 0) {
        r.error = "derived port set has no top-level output";
        return r;
    }

    // ④ 确定性（两次发射的产物**逐字节**相同）
    {
        const std::string pa = read_whole_file(a.hier_path);
        const std::string pb = read_whole_file(b.hier_path);
        if (pa.empty() || pb.empty()) {
            r.error = "emitted product is empty";
            return r;
        }
        r.deterministic = (pa == pb);
        if (!r.deterministic) {
            r.error = "two emissions of the same design differ byte-wise";
            return r;
        }
    }

    // ⑤ 层次化产物 ≠ 扁平产物（⇒ ⇒ 证明「分层真的改变了产物」）
    {
        const std::string flat_path = work_root + "/" + tag + "_flat.v";
        ch::toVerilog(flat_path, dev.context());
        const std::string flat = read_whole_file(flat_path);
        const std::string hier = read_whole_file(a.hier_path);
        r.differs_from_flat = (!flat.empty() && !hier.empty() && flat != hier);
        if (!r.differs_from_flat) {
            r.error = "hierarchical product is byte-identical to the flat product";
            return r;
        }
    }

    // ⑥ lint 门（★ 与 `scripts/verify_hierarchical_rtl.sh` 的 R1 **同款**命令：
    //    **无** `-Wall`、**无** `-Wno-fatal` ⇒ ⇒ warning 不得被消音）
    // ⚠️⚠️ **这里是 R1 的「复制」不是「共用」**（★ critic SHOULD_FIX-3）：
    //    ⇒ ⇒ R1 侧改 top 名 / 加 flag 时，**本处会无声过时** ⇒ ⇒ ⇒
    //    ⇒ ⇒ ★ 权威 = `scripts/verify_hierarchical_rtl.sh` 的 R1（ADR-060 §2.5）
    //    ⇒ ⇒ ⇒ 改本处 MUST 同步改那里（★ 抽共享 helper 是后续项）
    if (!ch_test::tool_available("verilator")) {
        r.lint_skipped = true;
    } else {
        const std::string cmd = "verilator --lint-only --top-module top '" + a.hier_path + "'";
        const auto res = ch_test::run_tool(cmd);
        const bool no_error = res.stderr_out.find("%Error") == std::string::npos;
        r.lint_ok = (res.exit_code == 0) && no_error;
        if (!r.lint_ok) {
            r.error = "lint gate failed (exit=" + std::to_string(res.exit_code) + "): " +
                      res.stderr_out;
            return r;
        }
    }

    r.ok = true;
    return r;
}

}  // namespace ch_test::hier_corpus
