// src/codegen_verilog.cpp
#include "codegen_verilog.h"
#include "codegen_naming.h"
#include "ast/ast_nodes.h"
#include "lnodeimpl.h"
#include "memimpl.h"
#include "mem_port_impl.h"
#include "logger.h"
#include <algorithm>
#include <cassert>
#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ch {

// --- verilogwriter Class Implementation ---
void toVerilog(const std::string &filename, ch::core::context *ctx) {
    // Check if we're in static destruction phase
    // If so, skip code generation to avoid segfaults
    try {
        if (!ctx) {
            std::cerr << "[toVerilog] Error: Context is null!" << std::endl;
            return;
        }

        std::ofstream file(filename);
        if (!file.is_open()) {
            std::cerr << "[toVerilog] Failed to open " << filename << std::endl;
            return;
        }

        verilogwriter writer(ctx);
        writer.print(file);

        file.close();
        std::cout << "[toVerilog] Generated " << filename << std::endl;
    } catch (...) {
        // Silently ignore exceptions during static destruction
        // Don't even print error messages during static destruction
        // if (!ch::detail::in_static_destruction()) {
        //     std::cerr << "[toVerilog] Exception caught during code
        //     generation"
        //               << std::endl;
        // }
    }
}
ModuleEmitter::ModuleEmitter(ch::core::context *ctx, ModuleView view)
    : ctx_(ctx), view_(std::move(view)) {
    node_names_ = view_.names;
    nodes_ = view_.nodes;
}

/// 扁平路径的 `ModuleView`：整个 ctx ⇒ 一个 `module top`。
///
/// ⚠️ 端口选择 / 排序 / 过滤规则**逐字保留**拆分前 `print_header` 的实现
/// ⇒ ⇒ 扁平产物 MUST 逐字节不变（本 change 的唯一验收面）。
ModuleView make_flat_module_view(ch::core::context *ctx) {
    ModuleView view;
    view.module_name = ch::kFlatModuleName;
    view.clock_name = "default_clock";
    view.reset_name = "default_reset";

    // 名称表（与层次化分析层**共用**同一份实现）
    view.names = ch::build_verilog_name_table(ctx->get_eval_list());
    view.nodes = ctx->get_eval_list();

    // 端口：input / output / clock / reset，按 id 升序
    std::vector<ch::core::lnodeimpl *> ports;
    for (auto &node_ptr : ctx->get_nodes()) {
        auto *node = node_ptr.get();
        if (node->type() == ch::core::lnodetype::type_input ||
            node->type() == ch::core::lnodetype::type_output ||
            node->type() == ch::core::lnodetype::type_clock ||
            node->type() == ch::core::lnodetype::type_reset) {
            ports.push_back(node);
        }
    }
    std::sort(ports.begin(), ports.end(),
              [](ch::core::lnodeimpl *a, ch::core::lnodeimpl *b) {
                  return a->id() < b->id();
              });

    // ⚠️ 时钟 / 复位**先行**（跨设计位置稳定 ⇒ Verilator dlopen / SHA-1 缓存 key 稳定）
    auto push_of_type = [&view, &ports](ch::core::lnodetype ty) {
        for (auto *port_node : ports) {
            if (port_node->type() != ty) {
                continue;
            }
            if (ty == ch::core::lnodetype::type_input) {
                // 有内部 driver 的子模块 `ch_in`（`sub.io().x <<= ...`）MUST NOT
                // 成为顶层端口：它在 print_decl / print_logic 里是**内部 wire**
                // 并从 driver 赋值。否则 Verilator 会看到没人驱动的顶层输入
                // （= X，reset 脉冲后为 0）⇒ 静默废掉顶层以下的所有 `ch_in`。
                auto *in = static_cast<ch::core::inputimpl *>(port_node);
                if (in->driver() != nullptr) {
                    continue;
                }
            }
            view.ports.push_back(port_node);
        }
    };
    push_of_type(ch::core::lnodetype::type_clock);
    push_of_type(ch::core::lnodetype::type_reset);
    push_of_type(ch::core::lnodetype::type_input);
    push_of_type(ch::core::lnodetype::type_output);
    return view;
}

// 扁平发射入口（对外的 `verilogwriter` 只是把整颗 ctx 交给 `ModuleEmitter`）
verilogwriter::verilogwriter(ch::core::context *ctx)
    : emitter_(std::make_unique<ModuleEmitter>(ctx, make_flat_module_view(ctx))) {}

void verilogwriter::print(std::ostream &out) { emitter_->print(out); }

std::string verilogwriter::get_verilog_name(ch::core::lnodeimpl *node) const {
    return emitter_->get_verilog_name(node);
}

void ModuleEmitter::print(std::ostream &out) {
    try {
        // Check if we're in static destruction phase

        print_header(out);
        print_body(out);
        print_footer(out);
    } catch (...) {
        // Silently ignore exceptions
    }
}

std::string ModuleEmitter::get_verilog_name(ch::core::lnodeimpl *node) const {
    auto it = node_names_.find(node);
    return it == node_names_.end() ? std::string() : it->second;
}

std::string ModuleEmitter::sanitize_name(const std::string &name) const {
    // 唯一实现见 `include/codegen_naming.h`（层次化分析层用的是同一份 ⇒ 名字不会漂移）
    return ch::sanitize_verilog_name(name);
}

std::string ModuleEmitter::get_width_str(uint32_t size) const {
    try {
        if (size <= 1) {
            return ""; // No width specification needed for 1-bit signals
        } else {
            // Return width in Verilog format [MSB:LSB]
            return "[" + std::to_string(size - 1) + ":0]";
        }
    } catch (...) {
        // Silently ignore exceptions
        return "";
    }
}

std::string
ModuleEmitter::get_literal_str(const ch::core::sdata_type &val) const {
    try {
        uint64_t value = static_cast<uint64_t>(val);
        uint32_t width = val.bv_.size();

        // verilator-perf-three-way: bitvector stores `value` in `words_[0]`
        // (uint64) without masking, so `ch_uint<8>(999)` reports width=8 but
        // carries 999. SystemVerilog rejects `8'h3e7` as a hard syntax error
        // (`Too many digits for N bit number`); mask to width bits to emit
        // `8'he7` instead. ADR-035 caps width at 64, so width>=64 is unchanged.
        uint64_t masked = (width == 0)    ? uint64_t{0}
                        : (width >= 64)   ? value
                        : (value & ((uint64_t{1} << width) - 1));

        // Special handling for small values
        if (width == 1) {
            return (masked ? "1'b1" : "1'b0");
        }

        // For larger values or wider widths, use hex format
        std::stringstream ss;
        ss << width << "'h" << std::hex << masked;
        return ss.str();
    } catch (...) {
        // Silently ignore exceptions
        return "1'b0";
    }
}

std::string ModuleEmitter::get_op_str(ch::core::ch_op op) const {
    try {
        switch (op) {
        case ch::core::ch_op::add:
            return "+";
        case ch::core::ch_op::sub:
            return "-";
        case ch::core::ch_op::mul:
            return "*";
        case ch::core::ch_op::div:
            return "/";
        case ch::core::ch_op::mod:
            return "%";
        case ch::core::ch_op::and_:
            return "&";
        case ch::core::ch_op::or_:
            return "|";
        case ch::core::ch_op::xor_:
            return "^";
        case ch::core::ch_op::not_:
            return "~"; // Bitwise NOT
        case ch::core::ch_op::eq:
            return "==";
        case ch::core::ch_op::ne:
            return "!=";
        case ch::core::ch_op::lt:
            return "<";
        case ch::core::ch_op::le:
            return "<=";
        case ch::core::ch_op::gt:
            return ">";
        case ch::core::ch_op::ge:
            return ">=";
        case ch::core::ch_op::shl:
            return "<<";
        case ch::core::ch_op::shr:
            return ">>";
        case ch::core::ch_op::sshr:
            return ">>>";
        case ch::core::ch_op::neg:
            return "-";
        case ch::core::ch_op::and_reduce:
            return "&";
        case ch::core::ch_op::or_reduce:
            return "|";
        case ch::core::ch_op::xor_reduce:
            return "^";
        case ch::core::ch_op::popcount:
            return "$countones";
        case ch::core::ch_op::assign:
            return "=";
        case ch::core::ch_op::bit_sel:
            return "<bit_sel>";
        case ch::core::ch_op::bits_extract:
            return "<bits_ex>";
        case ch::core::ch_op::sext:
            return "<sext>";
        case ch::core::ch_op::zext:
            return "<zext>";
        case ch::core::ch_op::rotate_l:
            return "<rotl>";
        case ch::core::ch_op::rotate_r:
            return "<rotr>";
        // Unreachable placeholders (handled outside get_op_str). Required for
        // -Wswitch-enum coverage of the 33-value ch_op enum.
        case ch::core::ch_op::concat:
            return "<concat>";
        case ch::core::ch_op::mux:
            return "<mux>";
        case ch::core::ch_op::bits_update:
            return "<bits_update>";
        }
        // No default: -Wswitch-enum enforces full enum coverage at compile time.
        return "<OP_NOT_IMPLEMENTED>";
    } catch (...) {
        // Silently ignore exceptions
        return "<OP_NOT_IMPLEMENTED>";
    }
}

void ModuleEmitter::emit_signal_decl(std::ostream &out,
                                     ch::core::lnodeimpl *node) {
    try {
        if (!node || !node_names_.count(node)) {
            return;
        }
        out << "    logic " << get_width_str(node->size())
            << " " << node_names_[node] << ";\n";
    } catch (...) {
        // Silently ignore exceptions (codebase convention for print_* methods)
    }
}

void ModuleEmitter::emit_always_ff(std::ostream &out,
                                   const std::string &reg_name,
                                   const std::string &next_name,
                                   const std::string &reset_value) {
    try {
        if (reg_name.empty() || next_name.empty()) {
            return;
        }
        out << "    always_ff @(posedge " << view_.clock_name << " or posedge "
            << view_.reset_name << ") begin // "
            << "Register update for " << reg_name << "\n";
        out << "        if (!" << view_.reset_name << ")\n";
        out << "            " << reg_name << " <= " << next_name << ";\n";
        out << "        else\n";
        out << "            " << reg_name << " <= " << reset_value << ";\n";
        out << "    end\n";
    } catch (...) {
        // Silently ignore exceptions (codebase convention for print_* methods)
    }
}

void ModuleEmitter::print_header(std::ostream &out) {
    try {
        // Verilator lint pragmas (must be outside module declaration):
        //   WIDTHEXPAND/WIDTHTRUNC arise from CH_MEM select tree mux widths
        //   (e.g. 5-bit shift amount mux_select_71 << 5'h1f producing 32-bit
        //   result assigned to 31-bit target). These are functionally correct
        //   (select tree outputs are zero-extended) but trigger Verilator
        //   warnings. Disable globally for the generated module.
        out << "/* verilator lint_off WIDTHEXPAND */\n";
        out << "/* verilator lint_off WIDTHTRUNC */\n";
        out << "module " << view_.module_name << " (\n";

        // 端口清单由 `ModuleView` 提供（**已排序 / 已过滤**）
        // ⇒ ⇒ 发射器不再自己扫一遍 ctx（那是单体 writer 的写法）
        const auto &ports = view_.ports;
        for (size_t i = 0; i < ports.size(); ++i) {
            auto *port_node = ports[i];
            // ⚠️ **端口旁注释**（冒泡端口 · ADR-060 §2.2）：单独一行输出
            //    （行尾 `//` 会吞掉后面的逗号 ⇒ 端口表语法会坏）
            if (!view_.port_notes.empty()) {
                const auto name_it = node_names_.find(port_node);
                if (name_it != node_names_.end()) {
                    const auto note_it = view_.port_notes.find(name_it->second);
                    if (note_it != view_.port_notes.end()) {
                        out << "    // " << note_it->second << "\n";
                    }
                }
            }
            switch (port_node->type()) {
            case ch::core::lnodetype::type_input:
                print_input(out,
                            static_cast<ch::core::inputimpl *>(port_node));
                break;
            case ch::core::lnodetype::type_output:
                print_output(out,
                             static_cast<ch::core::outputimpl *>(port_node));
                break;
            case ch::core::lnodetype::type_clock:
            case ch::core::lnodetype::type_reset:
                out << "    input " << node_names_[port_node];
                break;
            default:
                break;
            }
            if (i < ports.size() - 1) {
                out << ",\n";
            }
        }

        // ⚠️ **合成端口**（隐式端口 · ADR-060 §2.1）⇒ ⇒ 追加在节点端口之后
        //    （⚠️ 合成端口**不是 IR 节点** ⇒ ⇒ 不能走上面的 `print_input`/`print_output`）
        //    ⚠️ 扁平路径**恒空** ⇒ ⇒ 产物逐字节不变。
        for (std::size_t i = 0; i < view_.synthesized_ports.size(); ++i) {
            const auto &sp = view_.synthesized_ports[i];
            if (!ports.empty() || i > 0) {
                out << ",\n";
            }
            // ⚠️ **产物注释**（ADR-060 §2.4.1.1 ③）：**端口表内、该条目正上方单独成行**
            //   ⇒ ⇒ **禁**行尾注释 ⇒ ⇒ **每个合成物恰好一处**（④）+ 只含名字（④）
            if (view_.emit_synth_comments) {
                out << "    // CppHDL-synth: implicit port " << sp.name
                    << " (ADR-060 2.4.1)\n";
            }
            out << "    " << (sp.is_input ? "input" : "output") << " "
                << get_width_str(sp.width) << " " << sp.name;
        }

        out << "\n);\n\n";
    } catch (...) {
        // Silently ignore exceptions
    }
}

void ModuleEmitter::print_body(std::ostream &out) {
    try {
        print_decl(out);
        out << "\n";
        print_logic(out);
        out << "\n";
        // 层次化发射的实例化语句（扁平路径的 `body_epilogue` 恒为空）
        for (const auto &line : view_.body_epilogue) {
            out << line << "\n";
        }
        if (!view_.body_epilogue.empty()) {
            out << "\n";
        }
    } catch (...) {
        // Silently ignore exceptions
    }
}

void ModuleEmitter::print_footer(std::ostream &out) {
    try {
        out << "endmodule\n";
    } catch (...) {
        // Silently ignore exceptions
    }
}

void ModuleEmitter::print_decl(std::ostream &out) {
    try {
        // Check if we're in static destruction phase

        // Declare inputs, outputs, wires, regs
        // Iterate through the sorted list, but handle types differently for
        // declaration
        std::vector<ch::core::lnodeimpl *> inputs, outputs, wires, regs;
        std::vector<ch::core::memimpl *> mems;

        for (auto *node : nodes_) {
            if (declared_nodes_.count(node))
                continue; // Skip if already processed
            // ⚠️ **module 端口节点 MUST NOT 再声明成内部 wire**（否则同名重复声明）
            //    （扁平路径：端口 = 无 driver 的 input + output + clock/reset ⇒ 本分支不触发）
            bool is_module_port = false;
            for (auto *port_node : view_.ports) {
                if (port_node == node) {
                    is_module_port = true;
                    break;
                }
            }
            if (is_module_port) {
                continue;
            }

            switch (node->type()) {
            case ch::core::lnodetype::type_input: {
                // verilator-hier-input-flattening-fix: input nodes with
                // an internal driver (submodule `ch_in <<= ...`) are not
                // top-level ports (filtered out in print_header), but
                // they still need a `logic` declaration so the
                // downstream assigns can read them. Driver-less inputs
                // keep their existing semantics (top-level `input`
                // port, no internal declaration needed).
                auto *in = static_cast<ch::core::inputimpl *>(node);
                if (in->driver() != nullptr) {
                    wires.push_back(node);
                } else {
                    inputs.push_back(node);
                }
                declared_nodes_.insert(node);
                break;
            }
            case ch::core::lnodetype::type_output:
                // ⚠️ **角色③**（ADR-061 决策② 下格 · F3）：`type_output` 若是**体内**
                //    普通信号（inline 后代的 `ch_out` 落在**非顶层**成层 module）
                //    ⇒ ⇒ 它**不在** `view_.ports` 里 ⇒ ⇒ MUST 发 `logic` 声明
                //    （否则它是 `assign` 的目标却**无声明** ⇒ ⇒ Verilator 报 undeclared
                //      identifier ⇒ ⇒ lint 失败）
                //    ⚠️ 在端口表里的 ⇒ 走**端口声明**路径（`print_output`）⇒ 不变。
                if (std::find(view_.ports.begin(), view_.ports.end(), node) ==
                    view_.ports.end()) {
                    wires.push_back(node);
                } else {
                    outputs.push_back(node);
                }
                declared_nodes_.insert(node);
                break;
            case ch::core::lnodetype::type_reg:
                regs.push_back(node);
                declared_nodes_.insert(node);
                break;
            case ch::core::lnodetype::type_mem:
                mems.push_back(static_cast<ch::core::memimpl *>(node));
                declared_nodes_.insert(node);
                break;
            case ch::core::lnodetype::type_op:
            case ch::core::lnodetype::type_proxy: // Proxy nodes often require
                                                  // wires for intermediate
                                                  // connections
            case ch::core::lnodetype::type_lit: // Literals might need wires if
                                                // used multiple times or by
                                                // complex nodes
                // For simplicity, declare a wire for ops and proxies.
                // A more advanced check would see if the node is used inline or
                // requires a dedicated wire. For now, assume they need a wire
                // if they are sources to non-literal/proxy nodes. For now,
                // assume they need a wire if they are sources to
                // non-literal/proxy nodes.
                wires.push_back(node);
                declared_nodes_.insert(node);
                break;
            case ch::core::lnodetype::type_mux: // Mux nodes are assigned in
                                                // print_mux and need wires
            case ch::core::lnodetype::type_bitsupdate:
                wires.push_back(node);
                declared_nodes_.insert(node);
                break;
            default:
                // Other types like cd, etc., might not need declarations here.
                //
                // NOTE: `type_mem_read_port` deliberately has NO declaration
                // case. A mem read port is a fictional node — it has no
                // Verilog signal. Its data proxy is driven directly by
                // `print_mem_read_port` (`assign proxy = mem[addr];` /
                // `always_ff`), so declaring the read port would only create
                // a declared-but-undriven wire.
                break;
            }
        }

        // Print wires only. Regs are emitted by print_logic (in
        // topological order, paired with their always blocks). Inputs
        // and outputs are already in the port list (ADR-035).
        for (auto *node : wires) {
            if (node_names_.count(node)) {
                emit_signal_decl(out, node);
            }
        }
        // Memory arrays (unpacked SystemVerilog arrays).
        for (auto *mem : mems) {
            if (node_names_.count(mem)) {
                print_mem_decl(out, mem);
            }
        }
        // ⚠️ **合成连接网**（兄弟边的中转线 · 层次化专用）：它们**不是 IR 节点**
        //    ⇒ ⇒ 与 `synthesized_ports` 同族 ⇒ ⇒ 在这里发声明（必须先于
        //    `body_epilogue` 的实例化语句使用它）。
        //    ⚠️ 扁平路径**恒空** ⇒ ⇒ 产物逐字节不变。
        for (const auto &sw : view_.synthesized_wires) {
            const std::string w = get_width_str(sw.width);
            // ⚠️ **产物注释**（ADR-060 §2.4.1.1 ③）：**`logic` 声明行正上方单独成行**
            if (view_.emit_synth_comments) {
                out << "    // CppHDL-synth: net " << sw.name << " (ADR-060 2.4.1)\n";
            }
            out << "    logic " << (w.empty() ? "" : w + " ") << sw.name << ";\n";
        }
    } catch (...) {
        // Silently ignore exceptions
    }
}

void ModuleEmitter::print_logic(std::ostream &out) {
    try {
        // Check if we're in static destruction phase

        // Iterate through the sorted list and print logic for each node type
        // This assumes the sorted list correctly orders combinational logic
        // before sequential.
        for (auto *node : nodes_) {
            if (printed_logic_nodes_.count(node))
                continue; // Skip if already processed

            switch (node->type()) {
            case ch::core::lnodetype::type_op:
                print_op(out, static_cast<ch::core::opimpl *>(node));
                printed_logic_nodes_.insert(node);
                break;
            case ch::core::lnodetype::type_proxy:
                print_proxy(out, static_cast<ch::core::proxyimpl *>(node));
                printed_logic_nodes_.insert(node);
                break;
            case ch::core::lnodetype::type_reg:
                print_reg(out, static_cast<ch::core::regimpl *>(node));
                printed_logic_nodes_.insert(node);
                break;
            case ch::core::lnodetype::type_mux:
                print_mux(out, static_cast<ch::core::muximpl *>(node));
                printed_logic_nodes_.insert(node);
                break;
            case ch::core::lnodetype::type_bitsupdate:
                print_bitsupdate(out,
                                 static_cast<ch::core::bitsupdateimpl *>(node));
                printed_logic_nodes_.insert(node);
                break;
            case ch::core::lnodetype::type_mem:
                print_mem_init(out, static_cast<ch::core::memimpl *>(node));
                printed_logic_nodes_.insert(node);
                break;
            case ch::core::lnodetype::type_mem_read_port:
                print_mem_read_port(
                    out, static_cast<ch::core::mem_read_port_impl *>(node));
                printed_logic_nodes_.insert(node);
                break;
            case ch::core::lnodetype::type_mem_write_port:
                print_mem_write_port(
                    out, static_cast<ch::core::mem_write_port_impl *>(node));
                printed_logic_nodes_.insert(node);
                break;
            case ch::core::lnodetype::type_lit: {
                auto *lit_node = static_cast<ch::core::litimpl *>(node);
                if (node_names_.count(node)) {
                    out << "    assign " << node_names_[node] << " = "
                        << get_literal_str(lit_node->value()) << ";\n";
                }
                printed_logic_nodes_.insert(node);
                break;
            }
            case ch::core::lnodetype::type_output: // Handle output assignments
                // ADR-035 / Phase 1.3: walk the proxy chain to find the
                // actual driver. The output's src(0) is a proxy like
                // "_io"; the proxy's src(0) is the real driver (e.g., a
                // reg). Emitting "assign io = <real_driver>" replaces
                // the old "find first type_reg" hack (which was both
                // hard-coded to "io" and non-deterministic).
                {
                    auto *output_node =
                        static_cast<ch::core::outputimpl *>(node);
                    // ⚠️ 层次化：驱动在**别的 module**（e.g. 子 module 的输出）
                    //    ⇒ 本 module 不发这条 assign ⇒ 值由**实例端口映射**提供
                    if (view_.suppress_driver_assign.count(node) != 0) {
                        break;
                    }
                    if (output_node->num_srcs() > 0) {
                        ch::core::lnodeimpl *driver = output_node->src(0);
                        while (driver &&
                               driver->type() ==
                                   ch::core::lnodetype::type_proxy &&
                               driver->num_srcs() > 0) {
                            // ⚠️ **驱动重定向**（层次化 · proxy 子形态 · ADR-060 v1.9）：
                            //    该 proxy 改由**合成 input 端口**驱动 ⇒ ⇒ 它是**本 module
                            //    内的本地 wire**（`assign <proxy> = __if_<n>;` 由
                            //    `print_proxy` 发）⇒ ⇒ 穿透 MUST **停在它自己**。
                            //    ⇒ ⇒ 否则会取到它的 `src(0)` —— 而那已被**父层**驱动覆盖
                            //    ⇒ ⇒ 引用**父层信号** ⇒ ⇒ 跨 module 引用（且该信号在
                            //    本 module 内未声明 ⇒ 非法 Verilog）。
                            if (view_.port_drives_node.count(driver) != 0) {
                                break;
                            }
                            // A mem read port's data proxy is the real Verilog
                            // driver (`assign proxy = mem[addr];`), while the
                            // read port itself is a fictional node with no
                            // Verilog signal. Stop the walk at the proxy
                            // instead of advancing to the read port, which
                            // would emit `assign out = <read_port>;` — a
                            // reference to an undeclared signal.
                            ch::core::lnodeimpl *next = driver->src(0);
                            if (next &&
                                next->type() ==
                                    ch::core::lnodetype::type_mem_read_port) {
                                break;
                            }
                            driver = next;
                        }
                        if (driver && node_names_.count(driver)) {
                            std::string driver_name = node_names_[driver];
                            if (driver_name[0] != '_') {
                                std::string output_name =
                                    node_names_[node];
                                out << "    assign " << output_name
                                    << " = " << driver_name << ";\n";
                            }
                        }
                    }
                }
                printed_logic_nodes_.insert(node);
                break;
            case ch::core::lnodetype::type_input:
                // verilator-hier-input-flattening-fix: emit
                // `assign <input> = <driver>;` for driven submodule
                // inputs. Mirrors type_output logic (walk unnamed
                // proxy chain to reach the real driver). Driver-less
                // inputs (top-level ch_in with no `<<=` wiring) get
                // filtered out here because there is nothing to
                // assign — their values come from the testbench via
                // Vtop field writes (sync_inputs_to_vtop).
                {
                    auto *input_node =
                        static_cast<ch::core::inputimpl *>(node);
                    ch::core::lnodeimpl *driver = input_node->driver();
                    // ⚠️ 层次化：驱动在别的 module ⇒ 由实例端口映射驱动本输入
                    if (view_.suppress_driver_assign.count(node) != 0) {
                        break;
                    }
                    if (driver != nullptr) {
                        // Walk only UNNAMED proxies; named wires
                        // already have their own declaration +
                        // assign, so we want to read the value at
                        // that wire's name.
                        while (driver &&
                               driver->type() ==
                                   ch::core::lnodetype::type_proxy &&
                               !node_names_.count(driver) &&
                               driver->num_srcs() > 0) {
                            // Same guard as in `print_output`: never walk
                            // past a mem read port — it is a fictional node
                            // with no Verilog signal. (A mem data proxy is
                            // always *named*, so the loop normally stops
                            // before this; the guard keeps that from being
                            // an accident.)
                            ch::core::lnodeimpl *next = driver->src(0);
                            if (next &&
                                next->type() ==
                                    ch::core::lnodetype::type_mem_read_port) {
                                break;
                            }
                            driver = next;
                        }
                        if (driver && node_names_.count(driver) &&
                            node_names_.count(node)) {
                            out << "    assign "
                                << node_names_[node] << " = "
                                << node_names_[driver] << ";\n";
                        }
                    }
                }
                printed_logic_nodes_.insert(node);
                break;
            }
        }
    } catch (...) {
        // Silently ignore exceptions
    }
}

void ModuleEmitter::print_input(std::ostream &out, ch::core::inputimpl *node) {
    try {
        out << "    input " << get_width_str(node->size()) << " "
            << node_names_[node];
    } catch (...) {
        // Silently ignore exceptions
    }
}

void ModuleEmitter::print_output(std::ostream &out,
                                 ch::core::outputimpl *node) {
    try {
        out << "    output " << get_width_str(node->size()) << " "
            << node_names_[node];
    } catch (...) {
        // Silently ignore exceptions
    }
}

void ModuleEmitter::print_reg(std::ostream &out, ch::core::regimpl *node) {
    try {
        // Declare the register
        emit_signal_decl(out, node);

        // Find the 'next' source node for this register
        auto *next_node = node->get_next();

        // The ch_reg init value becomes the synchronous reset value so the
        // Verilator model starts from the same state as the CppHDL Simulator
        // when no reset pulse is applied (e.g. the CPU PC starts at
        // 0x80000000, matching the vendored ELF harness which skips reset).
        //
        // verilator-reset-initial-value-mismatch fix: normalize the
        // init literal to the register's width before emitting the
        // Verilog hex literal. Previously the literal kept its native
        // width (`ch_reg<ch_uint<8>>(10_d)` → `reg <= 4'ha;` since
        // 10_d's intrinsic width is 4 bits), which produced unstable
        // Verilog output across `_d`/`_h`/`_b` literal suffix choices.
        // The reset branch must read `<reg_width>'h<value>` so the
        // width is always 1:1 with the reg declaration.
        std::string reset_value = "1'b0";
        auto *init_node = node->get_init_val();
        if (init_node && init_node->type() ==
                             ch::core::lnodetype::type_lit) {
            auto *lit = static_cast<ch::core::litimpl *>(init_node);
            ch::core::sdata_type widened(
                static_cast<uint64_t>(lit->value()),
                node->size());
            reset_value = get_literal_str(widened);
        } else if (init_node) {
            // Non-literal init (e.g. ch_bool(true)): simulator side
            // may use a special init path; we conservatively fall back
            // to 1'b0 in the reset branch. Emit a warning so the
            // mismatch is visible during the codegen stage rather than
            // surfacing as a silent trace-parity bug downstream.
            CHWARN("[print_reg] ch_reg init_node is not type_lit "
                   "(type=%d, size=%u, name=%s); reset branch falls back to 1'b0",
                   static_cast<int>(init_node->type()),
                   init_node->size(),
                   node->name().c_str());
        }

        // ⚠️ **驱动重定向**（发射期虚拟重定向 · ADR-060 §2.1 行 1）
        //    「父层驱动子层寄存器」⇒ ⇒ 子侧端点**不是**子端口 ⇒ ⇒ 在子侧合成
        //    **input** 端口 ⇒ ⇒ 本寄存器的 next **改由该端口驱动**
        //    ⇒ ⇒ ⚠️ 原 next 节点**在父 module 里** ⇒ ⇒ 子 module **引用不到**
        //    ⇒ ⇒ 不重定向 ⇒ ⇒ 该寄存器会退化成「保持」（**静默丢驱动**）。
        //    ⚠️ 重定向只改**发射期**的取值 ⇒ ⇒ **MUST NOT** 写回 `lnodeimpl`。
        const auto redirected = view_.port_drives_node.find(node);
        if (redirected != view_.port_drives_node.end()) {
            emit_always_ff(out, node_names_[node], redirected->second, reset_value);
            return;
        }

        if (next_node && node_names_.count(next_node)) {
            // For register updates, assign to the register itself
            std::string reg_name = node_names_[node];

            // Bug 3: the next-value expression may be wider than the reg
            // (e.g. counter + ch_uint<8>(1) widens 8->9 bits, and a mux
            // over it inherits 9 bits). Verilator rejects a wide RHS into
            // a narrow reg with %Warning-WIDTHTRUNC, so slice it down to
            // the reg's width.
            std::string next_name = node_names_[next_node];
            if (next_node->size() > node->size()) {
                next_name = next_name + "[" +
                            std::to_string(node->size() - 1) + ":0]";
            }

            emit_always_ff(out, reg_name, next_name, reset_value);
        } else {
            // If next is not found or not named, emit a hold-state
            // always_ff so the register's init value is still applied
            // at the reset pulse (otherwise uart_tx etc. never see
            // their declared init value).
            std::string reg_name = node_names_[node];
            out << "    always_ff @(posedge " << view_.clock_name << " or posedge "
            << view_.reset_name << ") begin // "
                << "Register update (no next) for " << reg_name << "\n";
            out << "        if (!" << view_.reset_name << ")\n";
            out << "            " << reg_name << " <= " << reg_name << ";\n";
            out << "        else\n";
            out << "            " << reg_name << " <= " << reset_value << ";\n";
            out << "    end\n";
        }
    } catch (...) {
        // Silently ignore exceptions
    }
}

void ModuleEmitter::print_op(std::ostream &out, ch::core::opimpl *node) {
    try {
        if (!node) {
            return;
        }
        switch (node->op()) {
        case ch::core::ch_op::not_:
        case ch::core::ch_op::neg:
        case ch::core::ch_op::and_reduce:
        case ch::core::ch_op::or_reduce:
        case ch::core::ch_op::xor_reduce:
        case ch::core::ch_op::popcount:
        case ch::core::ch_op::sext:
        case ch::core::ch_op::zext:
        case ch::core::ch_op::assign:
            return print_unary_op(out, node);

        case ch::core::ch_op::add:
        case ch::core::ch_op::sub:
        case ch::core::ch_op::mul:
        case ch::core::ch_op::div:
        case ch::core::ch_op::mod:
        case ch::core::ch_op::and_:
        case ch::core::ch_op::or_:
        case ch::core::ch_op::xor_:
        case ch::core::ch_op::eq:
        case ch::core::ch_op::ne:
        case ch::core::ch_op::lt:
        case ch::core::ch_op::le:
        case ch::core::ch_op::gt:
        case ch::core::ch_op::ge:
        case ch::core::ch_op::shl:
        case ch::core::ch_op::shr:
        case ch::core::ch_op::sshr:
            return print_binary_op(out, node);

        case ch::core::ch_op::bit_sel:
            return print_bit_select(out, node);
        case ch::core::ch_op::bits_extract:
            return print_bits_extract(out, node);
        case ch::core::ch_op::rotate_l:
            return print_rotate_l(out, node);
        case ch::core::ch_op::rotate_r:
            return print_rotate_r(out, node);

        case ch::core::ch_op::concat:
            return print_concat(out, node);

        // mux -> print_mux (lnodetype), bits_update -> print_bitsupdate
        // (lnodetype; opimpl path is dead).
        case ch::core::ch_op::mux:
        case ch::core::ch_op::bits_update:
            return;
        }
        // No default: -Wswitch-enum catches missing cases at compile time.
    } catch (...) {
        // Silently ignore exceptions
    }
}

void ModuleEmitter::print_unary_op(std::ostream &out, ch::core::opimpl *node) {
    try {
        if (node->num_srcs() < 1 || !node_names_.count(node)) {
            return;
        }
        auto *src = node->src(0);
        if (!src || !node_names_.count(src)) {
            return;
        }
        out << "    assign " << node_names_[node] << " = ";

        if (node->op() == ch::core::ch_op::sext) {
            uint32_t ext = node->size() - src->size();
            if (ext == 0) {
                out << node_names_[src];
            } else {
                out << "{{" << ext << "{" << node_names_[src] << "["
                    << (src->size() - 1) << "]}}, " << node_names_[src] << "}";
            }
        } else if (node->op() == ch::core::ch_op::zext) {
            uint32_t ext = node->size() - src->size();
            if (ext == 0) {
                out << node_names_[src];
            } else {
                out << "{{" << ext << "{1'b0}}, " << node_names_[src] << "}";
            }
        } else if (node->op() == ch::core::ch_op::popcount) {
            out << "$countones(" << node_names_[src] << ")";
        } else if (node->op() == ch::core::ch_op::assign) {
            out << node_names_[src];
        } else {
            // if/else (not switch) avoids -Wswitch-enum on a partial subset.
            auto op = node->op();
            if (op == ch::core::ch_op::not_) {
                out << "~";
            } else if (op == ch::core::ch_op::neg) {
                out << "-";
            } else if (op == ch::core::ch_op::and_reduce) {
                out << "&";
            } else if (op == ch::core::ch_op::or_reduce) {
                out << "|";
            } else if (op == ch::core::ch_op::xor_reduce) {
                out << "^";
            }
            out << node_names_[src];
        }
        out << ";\n";
    } catch (...) {
        // Silently ignore exceptions
    }
}

void ModuleEmitter::print_binary_op(std::ostream &out, ch::core::opimpl *node) {
    try {
        if (node->num_srcs() < 2 || !node_names_.count(node)) {
            return;
        }
        auto *lhs_node = node->lhs();
        auto *rhs_node = node->rhs();
        if (!lhs_node || !rhs_node || !node_names_.count(lhs_node) ||
            !node_names_.count(rhs_node)) {
            return;
        }
        std::string lhs_name = node_names_[lhs_node];
        std::string rhs_name = node_names_[rhs_node];
        std::string op_str = get_op_str(node->op());

        if (rhs_node->type() == ch::core::lnodetype::type_lit) {
            auto *lit_node = static_cast<ch::core::litimpl *>(rhs_node);
            uint64_t value = static_cast<uint64_t>(lit_node->value());
            uint32_t width = lit_node->value().bv_.size();
            if (value == 1 && width == 1) {
                rhs_name = "1'b1";
            } else {
                rhs_name = get_literal_str(lit_node->value());
            }
        }

        out << "    assign " << node_names_[node] << " = " << lhs_name << " "
            << op_str << " " << rhs_name << ";\n";
    } catch (...) {
        // Silently ignore exceptions
    }
}

void ModuleEmitter::print_bit_select(std::ostream &out, ch::core::opimpl *node) {
    try {
        if (node->num_srcs() < 2 || !node_names_.count(node)) {
            return;
        }
        auto *lhs = node->src(0);
        auto *rhs = node->src(1);
        if (!lhs || !rhs || !node_names_.count(lhs) || !node_names_.count(rhs)) {
            return;
        }
        out << "    assign " << node_names_[node] << " = " << node_names_[lhs]
            << "[";
        if (rhs->type() == ch::core::lnodetype::type_lit) {
            out << static_cast<uint64_t>(static_cast<ch::core::litimpl *>(rhs)->value());
        } else {
            out << node_names_[rhs];
        }
        out << "];\n";
    } catch (...) {
        // Silently ignore exceptions
    }
}

void ModuleEmitter::print_bits_extract(std::ostream &out,
                                       ch::core::opimpl *node) {
    try {
        if (node->num_srcs() < 2 || !node_names_.count(node)) {
            return;
        }
        auto *lhs = node->src(0);
        auto *rhs = node->src(1);
        if (!lhs || !rhs || !node_names_.count(lhs)) {
            return;
        }
        if (rhs->type() != ch::core::lnodetype::type_lit) {
            return; // Requires literal range.
        }
        auto range_val = static_cast<uint64_t>(
            static_cast<ch::core::litimpl *>(rhs)->value());
        uint32_t msb = static_cast<uint32_t>(range_val >> 32);
        uint32_t lsb = static_cast<uint32_t>(range_val & 0xFFFFFFFFULL);
        out << "    assign " << node_names_[node] << " = " << node_names_[lhs]
            << "[" << msb << ":" << lsb << "];\n";
    } catch (...) {
        // Silently ignore exceptions
    }
}

void ModuleEmitter::print_rotate_l(std::ostream &out, ch::core::opimpl *node) {
    try {
        if (node->num_srcs() < 2 || !node_names_.count(node)) {
            return;
        }
        auto *lhs = node->src(0);
        auto *rhs = node->src(1);
        if (!lhs || !node_names_.count(lhs)) {
            return;
        }
        uint32_t N = lhs->size();
        out << "    assign " << node_names_[node] << " = ";
        if (rhs && rhs->type() == ch::core::lnodetype::type_lit) {
            uint32_t amt = static_cast<uint32_t>(
                static_cast<uint64_t>(static_cast<ch::core::litimpl *>(rhs)->value()));
            if (amt == 0 || amt >= N) {
                out << node_names_[lhs] << ";\n"; // Identity rotation.
            } else {
                out << "{" << node_names_[lhs] << "[" << (N - 1) << ":"
                    << (N - amt) << "], " << node_names_[lhs] << "["
                    << (N - amt - 1) << ":0]};\n";
            }
        } else {
            out << node_names_[lhs]
                << "; // rotate_l with variable amount not supported\n";
        }
    } catch (...) {
        // Silently ignore exceptions
    }
}

void ModuleEmitter::print_rotate_r(std::ostream &out, ch::core::opimpl *node) {
    try {
        if (node->num_srcs() < 2 || !node_names_.count(node)) {
            return;
        }
        auto *lhs = node->src(0);
        auto *rhs = node->src(1);
        if (!lhs || !node_names_.count(lhs)) {
            return;
        }
        uint32_t N = lhs->size();
        out << "    assign " << node_names_[node] << " = ";
        if (rhs && rhs->type() == ch::core::lnodetype::type_lit) {
            uint32_t amt = static_cast<uint32_t>(
                static_cast<uint64_t>(static_cast<ch::core::litimpl *>(rhs)->value()));
            if (amt == 0 || amt >= N) {
                out << node_names_[lhs] << ";\n"; // Identity rotation.
            } else {
                out << "{" << node_names_[lhs] << "[" << (amt - 1) << ":0], "
                    << node_names_[lhs] << "[" << (N - 1) << ":" << amt
                    << "]};\n";
            }
        } else {
            out << node_names_[lhs]
                << "; // rotate_r with variable amount not supported\n";
        }
    } catch (...) {
        // Silently ignore exceptions
    }
}

void ModuleEmitter::print_concat(std::ostream &out, ch::core::opimpl *node) {
    try {
        if (node->num_srcs() < 2 || !node_names_.count(node)) {
            return;
        }
        auto *lhs_node = node->lhs();
        auto *rhs_node = node->rhs();
        if (!lhs_node || !rhs_node || !node_names_.count(lhs_node) ||
            !node_names_.count(rhs_node)) {
            return;
        }
        std::string lhs_name = node_names_[lhs_node];
        std::string rhs_name = node_names_[rhs_node];
        if (rhs_node->type() == ch::core::lnodetype::type_lit) {
            auto *lit_node = static_cast<ch::core::litimpl *>(rhs_node);
            rhs_name = get_literal_str(lit_node->value());
        }
        if (lhs_node->type() == ch::core::lnodetype::type_lit) {
            auto *lit_node = static_cast<ch::core::litimpl *>(lhs_node);
            lhs_name = get_literal_str(lit_node->value());
        }
        out << "    assign " << node_names_[node] << " = {" << lhs_name << ", "
            << rhs_name << "};\n";
    } catch (...) {
        // Silently ignore exceptions
    }
}

void ModuleEmitter::print_proxy(std::ostream &out, ch::core::proxyimpl *node) {
    try {
        // ⚠️ **驱动重定向**（发射期虚拟重定向 · ADR-060 v1.9 §2.1 行 1 · proxy 子形态）
        //    「父层驱动子模块的 bundle 字段」⇒ ⇒ 子侧端点**不是**子端口 ⇒ ⇒ 在子侧合成
        //    **input** 端口 ⇒ ⇒ 本 proxy 的 driver **改由该端口提供**。
        //    ⚠️ MUST 在 `suppress_driver_assign` **之前**拦截 —— 否则这里会 early-return
        //    （父层那条 assign 被抑制），而合成端口**没有**任何 `assign` 目标
        //    ⇒ ⇒ proxy 变成无驱动 `logic` ⇒ ⇒ **静默读 X**：默认 warning 集**不查**
        //    `UNDRIVEN` ⇒ lint 零告警通过（⇒ 只有值判据能抓，见测试）。
        //    ⚠️ 重定向只改**发射期**取值 ⇒ ⇒ **MUST NOT** 写回 `lnodeimpl`。
        {
            const auto redirected = view_.port_drives_node.find(node);
            if (redirected != view_.port_drives_node.end() &&
                node_names_.count(node)) {
                out << "    assign " << node_names_[node] << " = "
                    << redirected->second << ";\n";
                return;
            }
        }

        // Proxy nodes act as intermediates, usually just assign their source to
        // themselves
        if (node->num_srcs() > 0) {
            auto *src_node = node->src(0);
            // A mem read port's data proxy is driven by `print_mem_read_port`
            // (`assign proxy = mem[addr];` for async, `always_ff` for sync).
            // The read port itself is a *fictional* node — it has no Verilog
            // signal — so emitting `assign proxy = <read_port>` here would
            // double-drive the wire AND reference an undeclared signal.
            // Skip those proxies; `print_mem_read_port` owns them.
            if (src_node &&
                src_node->type() ==
                    ch::core::lnodetype::type_mem_read_port) {
                return;
            }
            // ⚠️ 层次化：驱动在**别的 module** ⇒ 该值由实例端口映射提供
            //    ⇒ 本 module MUST NOT 发这条 assign（否则引用未声明信号）
            if (view_.suppress_driver_assign.count(node) != 0) {
                return;
            }
            if (src_node && node_names_.count(src_node) &&
                node_names_.count(node)) {
                std::string src_name = node_names_[src_node];
                std::string proxy_name = node_names_[node];
                out << "    assign " << proxy_name << " = " << src_name
                    << ";\n";
            }
        }
    } catch (...) {
        // Silently ignore exceptions
    }
}

void ModuleEmitter::print_mux(std::ostream &out, ch::core::muximpl *node) {
    try {
        if (node->num_srcs() >= 3 && node_names_.count(node)) {
            auto *cond_node = node->condition();
            auto *true_node = node->true_value();
            auto *false_node = node->false_value();
            if (cond_node && true_node && false_node &&
                node_names_.count(cond_node) &&
                node_names_.count(true_node) &&
                node_names_.count(false_node)) {
                std::string cond_name = node_names_[cond_node];
                std::string true_name = node_names_[true_node];
                std::string false_name = node_names_[false_node];
                // Bug 4: a select() with mismatched operand widths (e.g.
                // counter + ch_uint<8>(1) widens 8->9 bits muxed with the
                // 8-bit counter) makes the mux result wider than the narrow
                // arm. Verilator rejects the bare narrow arm in a wider
                // conditional context with %Warning-WIDTHEXPAND, so
                // zero-extend each arm below the mux result width.
                uint32_t result_width = node->size();
                if (true_node->size() < result_width) {
                    uint32_t ext = result_width - true_node->size();
                    true_name = "{{" + std::to_string(ext) + "{1'b0}}, " +
                                true_name + "}";
                }
                if (false_node->size() < result_width) {
                    uint32_t ext = result_width - false_node->size();
                    false_name = "{{" + std::to_string(ext) + "{1'b0}}, " +
                                 false_name + "}";
                }
                out << "    assign " << node_names_[node] << " = " << cond_name
                    << " ? " << true_name << " : " << false_name << ";\n";
            }
        }
    } catch (...) {
        // Silently ignore exceptions
    }
}

void ModuleEmitter::print_bitsupdate(std::ostream &out,
                                     ch::core::bitsupdateimpl *node) {
    try {
        // Emit {target[N-1:msb+1], source, target[lsb-1:0]}; omit slices that
        // would be zero-width (msb==N-1) or negative (lsb==0).
        auto *tgt = node->target();
        auto *src = node->source();
        auto *rng = node->range();
        if (!tgt || !src || !rng || !node_names_.count(node) ||
            !node_names_.count(tgt) || !node_names_.count(src)) {
            return;
        }
        auto range_val = static_cast<uint64_t>(
            static_cast<ch::core::litimpl *>(rng)->value());
        uint32_t msb =
            static_cast<uint32_t>(range_val >> 32);
        uint32_t lsb = static_cast<uint32_t>(range_val & 0xFFFFFFFFULL);
        uint32_t N = tgt->size();
        out << "    assign " << node_names_[node] << " = {";
        if (msb < N - 1)
            out << node_names_[tgt] << "[" << (N - 1) << ":" << (msb + 1)
                << "], ";
        out << node_names_[src];
        if (lsb > 0)
            out << ", " << node_names_[tgt] << "[" << (lsb - 1) << ":0]";
        out << "};\n";
    } catch (...) {
        // Silently ignore exceptions
    }
}

void ModuleEmitter::print_mem_decl(std::ostream &out,
                                   ch::core::memimpl *node) {
    try {
        if (!node || !node_names_.count(node)) {
            return;
        }
        out << "    logic " << get_width_str(node->data_width())
            << " " << node_names_[node] << " [0:" << (node->depth() - 1)
            << "];\n";
    } catch (...) {
        // Silently ignore exceptions (codebase convention for print_* methods)
    }
}

void ModuleEmitter::print_mem_init(std::ostream &out,
                                   ch::core::memimpl *node) {
    try {
        if (!node || !node_names_.count(node)) {
            return;
        }
        const auto &init_data = node->init_data();
        std::string mem_name = node_names_[node];
        out << "    initial begin\n";
        if (!init_data.empty()) {
            out << "        for (int __i = 0; __i < "
                << static_cast<int>(node->depth()) << "; __i = __i + 1)\n";
            out << "            " << mem_name << "[__i] = 1'b0;\n";
        }
        for (std::size_t i = 0; i < init_data.size(); ++i) {
            const auto &w = init_data[i];
            if (w.is_zero()) {
                continue;
            }
            out << "        " << mem_name << "[" << i << "] = "
                << get_literal_str(w) << ";\n";
        }
        out << "    end\n";
    } catch (...) {
        // Silently ignore exceptions (codebase convention for print_* methods)
    }
}

// ── **mem 端口的 `en` 槽解析**（ADR-066 决策 ⑧ ⑥）────────────────────────
// ⚠️ **读 / 写两侧共用**（⇒ ⇒ 一处改动 ⇒ ⇒ 两侧同时生效 ⇒ ⇒ 免漂移）。
// ⚠️ **两种情形 MUST 分开**（⇒ ⇒ 否则「禁止回退」会把**合法缺省**也判红）：
//   · `enable == nullptr` ⇒ ⇒ `1'b1`（合法缺省 · 语义 = 恒使能）
//   · `enable != nullptr` 且**取不到名字** ⇒ ⇒ **fail-closed**（诊断点名）
//     ⇒ ⇒ MUST NOT 回退 `1'b1`（⇒ 读侧恒使能 / 写侧恒写 ⇒ ⇒ **值错且 lint 绿**）
bool ModuleEmitter::resolve_mem_enable(const ch::core::lnodeimpl *enable,
                                        const std::string &mem_name,
                                        std::string &out_enable) {
    if (enable == nullptr) {
        out_enable = "1'b1";       // ⚠️ **合法缺省**（`ch_mem::aread` 恒传 nullptr）
        return true;
    }
    const auto it = node_names_.find(enable);
    if (it == node_names_.end()) {
        // ⚠️ **fail-closed**（ADR-066 决策 ⑧ ⑥）⇒ ⇒ 诊断点名（可行动）
        CHERROR("hierarchical emission: mem port '%s' has an enable operand that has "
                "no name in this module's scope. Refusing to fall back to '1'b1' "
                "(that would silently make the port always enabled and produce a "
                "wrong value). ADR-066 decision 8 clause 6.",
                mem_name.c_str());
        return false;
    }
    out_enable = it->second;
    return true;
}

void ModuleEmitter::print_mem_read_port(
    std::ostream &out, ch::core::mem_read_port_impl *node) {
    try {
        if (!node || !node_names_.count(node)) {
            return;
        }
        auto *parent = node->parent();
        auto *addr = node->addr();
        auto *enable = node->enable();
        auto *data_proxy = node->data_output();
        if (!parent || !addr || !data_proxy ||
            !node_names_.count(parent) || !node_names_.count(addr) ||
            !node_names_.count(data_proxy)) {
            return;
        }
        std::string mem_name = node_names_[parent];
        std::string addr_name = node_names_[addr];
        std::string proxy_name = node_names_[data_proxy];

        // B' (ChipForge Phase 6d.7+): per-port opt-out of 1-cycle latency.
        // aread (async_read) emits a continuous `assign` (combinational read,
        // 0-cycle latency in Verilog). The Simulator classifies async_read
        // ports in `combinational_instr_list_` (`src/simulator_init.cpp`),
        // so they are evaluated in `eval_combinational()` — the same 0-cycle
        // semantics as the generated Verilog.
        //
        // The read port is a *fictional* node: it has no Verilog signal of its
        // own. The data proxy is the signal, and it is driven here. The AST
        // carries a `read_port → data_proxy` edge (established in
        // `mem_read_port_impl`'s constructor) so that the topological sort
        // evaluates the read port *before* the proxy — hence `print_proxy`
        // must skip such proxies, and `print_output` must stop its
        // proxy-chain walk at the read port (see the comments there).
        //
        // For sync_read (sread) we keep the historical 1-cycle always_ff,
        // which is what the CPU's pc_lag / FLUSH mechanism depends on for
        // imem. DO NOT use aread for imem — that would misalign fetch with
        // pc_lag.
        // ⚠️ **fail-closed**（ADR-066 决策 ⑧ ⑥）⇒ ⇒ 缺名 ⇒ ⇒ **不**回退 `1'b1`
        std::string en_str;
        if (!resolve_mem_enable(enable, mem_name, en_str)) {
            return;
        }

        if (node->port_type() == ch::core::mem_port_type::async_read) {
            // aread: combinational read in generated Verilog (0-cycle
            // latency). `ch_mem::aread()` (include/core/mem.h) always passes
            // `nullptr` for the enable argument, so this branch has no
            // enable-gated path (a previous dead enable-branch was removed).
            // The Simulator classifies async_read ports in
            // `combinational_instr_list_` → 0-cycle there too, matching this
            // `assign` exactly.
            out << "    assign " << proxy_name << " = " << mem_name
                << "[" << addr_name << "]; // Comb read " << proxy_name << "\n";
            return;
        }

        // Sync read (existing 1-cycle path, unchanged behavior).
        // All sync mem read ports emit always_ff, giving 1-cycle read
        // latency. The CPU's pc_lag / FLUSH mechanism depends on this for
        // imem — keep as-is.
        //
        // ⚠️ Emitted shape (mind the dangling-else!):
        //
        //     if (!default_reset)
        //         if (en) proxy <= mem[addr];
        //     else
        //         proxy <= 1'b0;
        //
        // Per Verilog's dangling-else rule the `else` binds to the INNER
        // `if (en)` — so this is **`enable == 0` clears the proxy**. It is
        // NOT a reset branch: when `default_reset` is high the whole outer
        // body is skipped and the proxy holds.
        // (An earlier comment here said "Reset branch clears the read proxy"
        // — that wording was backwards. Corrected by change
        // `sread-disable-cycle-divergence-2026-11`.)
        //
        // The Simulator matches this via
        // `instr_mem_sync_read_port::eval` (`src/ast/instr_mem.cpp`), which
        // writes zero on the disabled cycle.
        out << "    always_ff @(posedge " << view_.clock_name << " or posedge "
            << view_.reset_name << ") begin // "
            << "Read port " << proxy_name << "\n";
        out << "        if (!" << view_.reset_name << ")\n";
        out << "            if (" << en_str << ")\n";
        out << "                " << proxy_name << " <= " << mem_name << "["
            << addr_name << "];\n";
        out << "        else\n";
        out << "            " << proxy_name << " <= 1'b0;\n";
        out << "    end\n";
    } catch (...) {
        // Silently ignore exceptions (codebase convention for print_* methods)
    }
}

void ModuleEmitter::print_mem_write_port(
    std::ostream &out, ch::core::mem_write_port_impl *node) {
    try {
        if (!node || !node_names_.count(node)) {
            return;
        }
        auto *parent = node->parent();
        auto *addr = node->addr();
        auto *wdata = node->wdata();
        auto *enable = node->enable();
        if (!parent || !addr || !wdata ||
            !node_names_.count(parent) || !node_names_.count(addr) ||
            !node_names_.count(wdata)) {
            return;
        }
        std::string mem_name = node_names_[parent];
        std::string addr_name = node_names_[addr];
        std::string wdata_name = node_names_[wdata];
        // ⚠️ **fail-closed**（ADR-066 决策 ⑧ ⑥）⇒ ⇒ 缺名 ⇒ ⇒ **不**回退 `1'b1`
        std::string en_str;
        if (!resolve_mem_enable(enable, mem_name, en_str)) {
            return;
        }
        // No reset branch: simulator reset() never clears memory contents
        // (preloaded ELF must survive; only ch_reg cells are reset).
        out << "    always_ff @(posedge " << view_.clock_name << ") begin // "
            << "Write port for " << mem_name << "\n";
        out << "        if (" << en_str << ")\n";
        out << "            " << mem_name << "[" << addr_name
            << "] <= " << wdata_name << ";\n";
        out << "    end\n";
    } catch (...) {
        // Silently ignore exceptions (codebase convention for print_* methods)
    }
}

} // namespace ch