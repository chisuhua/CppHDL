// include/codegen_verilog.h
#pragma once

#include "core/context.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Forward declarations for node types used by the writer
namespace ch {
namespace core {
class lnodeimpl;
class litimpl;
class inputimpl;
class outputimpl;
class regimpl;
class opimpl;
class proxyimpl;
class memimpl;
class mem_read_port_impl;
class mem_write_port_impl;
enum class ch_op;
} // namespace core
} // namespace ch

namespace ch {

void toVerilog(const std::string &filename, ch::core::context *ctx);

/// 一个 module 的**发射输入**（层次化 T3-③）。
///
/// ⚠️ 这个结构就是「模块边界」的全部信息 —— 发射器 MUST NOT 自己再去看
/// `ctx_->get_nodes()` 猜边界（那正是单体 writer 的写法）。扁平路径由
/// `make_flat_module_view(ctx)` 算出一份 ⇒ ⇒ 产物 MUST 与拆分前逐字节相同。
struct ModuleView {
    /// module 名（扁平 = `top`；层次化 = 该成层 module 的名字）
    std::string module_name = "top";
    /// 时钟 / 复位信号名（**参数化**：子 module 的 `always_ff` 不能引用
    /// RTL 顶层的 `default_clock` / `default_reset` —— 那是本模块里不存在的信号）
    std::string clock_name = "default_clock";
    std::string reset_name = "default_reset";
    /// 节点遍历顺序（**拓扑序**：组合在前、时序在后）
    std::vector<ch::core::lnodeimpl *> nodes;
    /// 模块端口（**已按发射顺序排好、已过滤** —— 端口选哪些是本结构的责任，不是发射器的）
    std::vector<ch::core::lnodeimpl *> ports;
    /// 节点的 Verilog 名（**全局**唯一表 ⇒ 端口名与扁平产物逐字相同）
    std::unordered_map<const ch::core::lnodeimpl *, std::string> names;
};

/// 扁平产物的 `ModuleView`（= 拆分前的行为：整个 ctx 一个 `module top`）。
ModuleView make_flat_module_view(ch::core::context *ctx);

// Emits **one** module from a `ModuleView`.
class ModuleEmitter {
public:
    ModuleEmitter(ch::core::context *ctx, ModuleView view);

    // Generates the complete Verilog module and writes it to the given stream.
    void print(std::ostream &out);

    // Verilog port name for `node` (with uniqueness _N suffix). Empty if
    // the node was not assigned a name by the codegen.
    std::string get_verilog_name(ch::core::lnodeimpl *node) const;

private:
    // --- Helper functions for name generation and validation ---
    std::string sanitize_name(const std::string &name) const;
    std::string get_width_str(uint32_t size) const;
    std::string get_literal_str(const ch::core::sdata_type &val) const;

    // --- Helper function to print operators ---
    std::string get_op_str(ch::core::ch_op op) const;

    // --- SV helper methods (emit_signal_decl / emit_always_ff) ---
    void emit_signal_decl(std::ostream &out, ch::core::lnodeimpl *node);
    void emit_always_ff(std::ostream &out,
                        const std::string &reg_name,
                        const std::string &next_name,
                        const std::string &reset_value = "1'b0");

    // --- Core printing methods ---
    void print_header(std::ostream &out);
    void print_body(std::ostream &out);
    void print_footer(std::ostream &out);

    // --- Declaration printing (wires, regs, assigns for literals) ---
    void print_decl(std::ostream &out);

    // --- Logic printing (assign statements, always blocks) ---
    void print_logic(std::ostream &out);

    // --- Specific node type printers ---
    void print_input(std::ostream &out, ch::core::inputimpl *node);
    void print_output(std::ostream &out, ch::core::outputimpl *node);
    void print_reg(std::ostream &out, ch::core::regimpl *node);
    void print_op(std::ostream &out, ch::core::opimpl *node);
    void print_proxy(std::ostream &out, ch::core::proxyimpl *node);
    void print_mux(std::ostream &out, ch::core::muximpl *node);
    void print_bitsupdate(std::ostream &out, ch::core::bitsupdateimpl *node);

    // --- Memory emission (ADR-035 R8: memory backdoor, ChipForge 6d.5 E8) ---
    void print_mem_decl(std::ostream &out, ch::core::memimpl *node);
    void print_mem_init(std::ostream &out, ch::core::memimpl *node);
    void print_mem_read_port(std::ostream &out, ch::core::mem_read_port_impl *node);
    void print_mem_write_port(std::ostream &out, ch::core::mem_write_port_impl *node);

    // --- Specialized op printers (Wave 2) ---
    void print_unary_op(std::ostream &out, ch::core::opimpl *node);
    void print_binary_op(std::ostream &out, ch::core::opimpl *node);
    void print_bit_select(std::ostream &out, ch::core::opimpl *node);
    void print_bits_extract(std::ostream &out, ch::core::opimpl *node);
    void print_rotate_l(std::ostream &out, ch::core::opimpl *node);
    void print_rotate_r(std::ostream &out, ch::core::opimpl *node);
    void print_concat(std::ostream &out, ch::core::opimpl *node);

    // --- Data members ---
    ch::core::context *ctx_;
    ModuleView view_;
    // Map to store the generated Verilog name for each node
    std::unordered_map<const ch::core::lnodeimpl *, std::string> node_names_;
    // Set to track nodes already declared/printed to avoid duplicates
    std::unordered_set<ch::core::lnodeimpl *> declared_nodes_;
    // Set to track nodes already printed in the logic section to avoid
    // duplicates
    std::unordered_set<ch::core::lnodeimpl *> printed_logic_nodes_;
    // 节点遍历顺序（拓扑序）
    std::vector<ch::core::lnodeimpl *> nodes_;
};

// 扁平发射入口（对外 API 不变）：整个 ctx ⇒ 一个 `module top`。
// ⚠️ 层次化发射（T3-④）直接用 `ModuleEmitter` + 自算的 `ModuleView`；
//    本类只是**扁平路径**的薄适配器 ⇒ ⇒ 「拆分前后逐字节相同」是可判定的。
class verilogwriter {
public:
    explicit verilogwriter(ch::core::context *ctx);

    void print(std::ostream &out);

    std::string get_verilog_name(ch::core::lnodeimpl *node) const;

private:
    std::unique_ptr<ModuleEmitter> emitter_;
};

} // namespace ch
