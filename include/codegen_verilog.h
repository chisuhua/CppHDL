// include/codegen_verilog.h
#pragma once

#include "core/context.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
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

/// 扁平产物的 module 名（**唯一真源**）。
///
/// ⚠️ 扁平发射（`make_flat_module_view`）与层次化发射的**映射表**（「扁平位置」那一列）
///    MUST 共用这一份 ⇒ ⇒ 否则「映射表说的扁平位置」与「扁平产物真实的 module 名」会漂移。
inline constexpr const char *kFlatModuleName = "top";

void toVerilog(const std::string &filename, ch::core::context *ctx);

/// 一个 module 的**发射输入**（层次化 T3-③）。
///
/// ⚠️ 这个结构就是「模块边界」的全部信息 —— 发射器 MUST NOT 自己再去看
/// `ctx_->get_nodes()` 猜边界（那正是单体 writer 的写法）。扁平路径由
/// `make_flat_module_view(ctx)` 算出一份 ⇒ ⇒ 产物 MUST 与拆分前逐字节相同。
/// 一个**合成端口**（隐式端口 · ADR-060 §2.1）。
///
/// ⚠️ 合成端口**不是 IR 节点** ⇒ ⇒ 没有 `lnodeimpl` ⇒ ⇒ 只带「名字 / 方向 / 宽度」。
struct SynthesizedPort {
    std::string name;
    bool is_input = false;
    std::uint32_t width = 0;
};

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
    /// body 尾部追加的原文（层次化发射用它写**实例化语句**与实例连接网）。
    /// ⚠️ 扁平路径恒为空 ⇒ ⇒ 产物逐字节不变。
    std::vector<std::string> body_epilogue;
    /// **驱动重定向表**（发射期虚拟重定向 · ADR-060 §2.1）⇒ ⇒
    /// 「本 module 内这个节点**改由**哪个**合成端口**驱动」。
    ///
    /// ⚠️ **MUST NOT** 写回 `lnodeimpl`（`set_src` 会污染 `users_` ⇒ ⇒ 既有
    /// `toVerilog()` 产物 / 解释器行为 / 两次生成的确定性**全部会变**）⇒ ⇒
    /// 只在**发射期**生效 ⇒ ⇒ 扁平路径**恒空** ⇒ ⇒ 产物逐字节不变。
    std::unordered_map<const ch::core::lnodeimpl *, std::string> port_drives_node;
    /// **合成端口**（隐式端口 · ADR-060 §2.1）⇒ ⇒ **没有对应的 IR 节点**
    /// （隐式端口是**发射期**造出来的 ⇒ ⇒ 不是 `lnodeimpl`）⇒ ⇒ 故与 `ports` 分开。
    /// ⚠️ 扁平路径**恒空** ⇒ ⇒ 产物逐字节不变。
    std::vector<SynthesizedPort> synthesized_ports;
    /// **合成连接网**（层次化专用 · 兄弟边在共同祖先 module 内的中转线 ·
    /// ADR-060 §2.1 / design §7.2.3）。
    ///
    /// 与 `synthesized_ports` 同族：**不是 IR 节点** ⇒ ⇒ 只带名字与宽度 ⇒ ⇒
    /// 由 `print_decl` 发 `logic [W] name;`。
    /// ⚠️ 扁平路径**恒空** ⇒ ⇒ 产物逐字节不变。
    std::vector<SynthesizedPort> synthesized_wires;
    /// **本 module 内 MUST NOT 发驱动赋值的节点**。
    ///
    /// ⚠️ 层次化：一个节点的驱动若在**别的 module** ⇒ 那个信号在本 module 里
    ///    **不存在** ⇒ 发 `assign <node> = <别 module 的信号>` = 引用未声明标识符
    ///    ⇒ ⇒ 该值 MUST 由**实例端口映射**（`.port(net)`）提供 ⇒ 本 module 不发这条 assign。
    /// ⚠️ 扁平路径恒为空 ⇒ ⇒ 产物逐字节不变。
    std::unordered_set<const ch::core::lnodeimpl *> suppress_driver_assign;
    /// **端口旁注释**（端口名 → 注释文本）：在端口声明**之前**单独一行输出
    /// （`// <note>`）⇒ Verilog 合法，且**不用行尾注释**（避免吞掉逗号）。
    ///
    /// ⚠️ 只用于「冒泡端口」这类**来源不可见**的端口（ADR-060 §2.2）。
    /// ⚠️ 扁平路径恒为空 ⇒ ⇒ 产物逐字节不变。
    std::map<std::string, std::string> port_notes;
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

    /// **mem 端口的 `en` 槽解析**（ADR-066 决策 ⑧ ⑥ · **读 / 写两侧共用** ⇒ ⇒ 免漂移）。
    ///
    /// ⚠️ **两种情形 MUST 分开**（否则「禁止回退」会把**合法缺省**也判红）：
    ///   · `enable == nullptr` ⇒ ⇒ `1'b1`（**合法缺省** · 语义 = 恒使能）
    ///   · `enable != nullptr` **且取不到名字** ⇒ ⇒ **fail-closed**（诊断点名）
    ///     ⇒ ⇒ **MUST NOT** 回退 `1'b1`（⇒ 读侧恒使能 / 写侧恒写 ⇒ ⇒ **值错且 lint 绿**）
    ///
    /// @return `true` = 解析成功（`out_enable` 被填）· `false` = fail-closed（**已诊断**）
    bool resolve_mem_enable(const ch::core::lnodeimpl *enable,
                            const std::string &mem_name, std::string &out_enable);

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
