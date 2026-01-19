// include/core/node_builder.h
#ifndef CH_CORE_NODE_BUILDER_H
#define CH_CORE_NODE_BUILDER_H

#include "core/context.h"
#include "core/literal.h"
#include "core/lnode.h"
#include "core/lnodeimpl.h"
#include "types.h"
#include <cstdint>
#include <memory>
#include <source_location>

// 直接包含非核心定义
#include "../lnode/node_builder_ext.h"

namespace ch {
namespace core {

// 节点构建器核心接口
class node_builder {
public:
    // 单例访问点
    static node_builder &instance() {
        static node_builder instance;
        return instance;
    }

    // 配置和统计接口
    void set_debug_mode(bool debug) { debug_mode_ = debug; }
    void set_optimization_level(optimization_level level) {
        opt_level_ = level;
    }
    void enable_caching(bool enable) { caching_enabled_ = enable; }
    void set_name_prefix(const std::string &prefix) { name_prefix_ = prefix; }
    void enable_statistics(bool enable) { statistics_enabled_ = enable; }
    const build_statistics &get_statistics() const { return *statistics_; }
    void reset_statistics() { statistics_->reset(); }

    // 核心构建接口
    template <typename T>
    lnodeimpl *build_literal(
        T value, const std::string &name = "literal",
        const std::source_location &sloc = std::source_location::current()) {
        CHDBG_FUNC();
        if (debug_mode_) {
            CHINFO("[node_builder] Building literal with value %s",
                   name.c_str());
        }

        auto *ctx = ctx_curr_;
        if (!ctx) {
            CHERROR("[node_builder] No active context for literal creation");
            return nullptr;
        }

        if constexpr (std::is_arithmetic_v<T>) {
            // 处理算术字面值
            uint32_t width = get_literal_width(value);
            sdata_type sval(static_cast<uint64_t>(value), width);

            if (statistics_enabled_) {
                ++statistics_->literals_built;
                ++statistics_->total_nodes_built;
            }

            CHDBG("[node_builder] Building literal with value %llu, width %u",
                  static_cast<unsigned long long>(value), width);
            return ctx->create_literal(
                sval, prefixed_name_helper(name, name_prefix_), sloc);
        } else if constexpr (is_ch_literal_v<T>) {
            sdata_type sdata(value.value(), value.actual_width); // 直接构造

            if (statistics_enabled_) {
                ++statistics_->literals_built;
                ++statistics_->total_nodes_built;
            }

            return ctx->create_literal(
                sdata, prefixed_name_helper(name, name_prefix_), sloc);
        } else {
            // 处理 sdata_type
            if (statistics_enabled_) {
                ++statistics_->literals_built;
                ++statistics_->total_nodes_built;
            }

            CHDBG("[node_builder] Building literal from sdata_type");
            return ctx->create_literal(
                value, prefixed_name_helper(name, name_prefix_), sloc);
        }
    }

    template <typename T>
    lnodeimpl *build_input(
        const std::string &name = "input",
        const std::source_location &sloc = std::source_location::current()) {
        CHDBG_FUNC();
        if (debug_mode_) {
            CHINFO("[node_builder] Building input '%s'", name.c_str());
        }

        auto *ctx = ctx_curr_;
        if (!ctx) {
            CHERROR("[node_builder] No active context for input creation");
            return nullptr;
        }

        uint32_t size = ch_width_v<T>;
        if (statistics_enabled_) {
            ++statistics_->inputs_built;
            ++statistics_->total_nodes_built;
        }

        CHDBG("[node_builder] Building input with size %u, name '%s'", size,
              name.c_str());
        return ctx->create_input(size, prefixed_name_helper(name, name_prefix_),
                                 sloc);
    }

    template <typename T>
    lnodeimpl *build_output(
        const std::string &name = "output",
        const std::source_location &sloc = std::source_location::current()) {
        CHDBG_FUNC();
        if (debug_mode_) {
            CHINFO("[node_builder] Building output '%s'", name.c_str());
        }

        auto *ctx = ctx_curr_;
        if (!ctx) {
            CHERROR("[node_builder] No active context for output creation");
            return nullptr;
        }

        uint32_t size = ch_width_v<T>;
        if (statistics_enabled_) {
            ++statistics_->outputs_built;
            ++statistics_->total_nodes_built;
        }

        CHDBG("[node_builder] Building output with size %u, name '%s'", size,
              name.c_str());
        return ctx->create_output(
            size, prefixed_name_helper(name, name_prefix_), sloc);
    }

    template <typename T>
    std::pair<regimpl *, proxyimpl *> build_register(
        lnodeimpl *init_val = nullptr, lnodeimpl *next_val = nullptr,
        const std::string &name = "reg",
        const std::source_location &sloc = std::source_location::current()) {

        CHDBG_FUNC();
        auto *ctx = ctx_curr_;
        if (!ctx) {
            CHERROR("[node_builder] No active context for register creation");
            return {nullptr, nullptr};
        }

        uint32_t size = ch_width_v<T>;
        if (statistics_enabled_) {
            ++statistics_->registers_built;
            ++statistics_->total_nodes_built;
        }

        CHDBG("[node_builder] Building register with size %u, name '%s'", size,
              name.c_str());

        // Check if we have a default clock and reset
        ch::core::clockimpl *default_clk = ctx->get_default_clock();
        ch::core::resetimpl *default_rst = ctx->get_default_reset();

        // 构建寄存器节点
        regimpl *reg_node = ctx->create_node<regimpl>(
            size, default_clk->id(), default_rst, nullptr, nullptr, next_val,
            init_val, prefixed_name_helper(name, name_prefix_), sloc);

        // 构建代理节点
        proxyimpl *proxy_node = ctx->create_node<proxyimpl>(
            reg_node, prefixed_name_helper("_" + name, name_prefix_), sloc);

        // Explicitly link register and proxy nodes
        if (reg_node && proxy_node) {
            reg_node->set_proxy(proxy_node);
        }

        // 设置 next 值
        if (next_val && reg_node) {
            reg_node->set_next(next_val);
            CHDBG("[node_builder] Set next value for register");
        }

        return {reg_node, proxy_node};
    }

    template <typename Cond, typename TrueVal, typename FalseVal>
    lnodeimpl *build_mux(
        const lnode<Cond> &cond, const lnode<TrueVal> &true_val,
        const lnode<FalseVal> &false_val, const std::string &name = "mux",
        const std::source_location &sloc = std::source_location::current()) {

        constexpr unsigned result_width =
            std::max(ch_width_v<TrueVal>, ch_width_v<FalseVal>);

        auto *mux_node = ctx_curr_->create_node<muximpl>(
            result_width, cond.impl(), true_val.impl(), false_val.impl(), name,
            sloc);

        return mux_node;
    }

    template <typename T>
    lnodeimpl *build_bit_select(
        const lnode<T> &operand, uint32_t index,
        const std::string &name = "bit_select",
        const std::source_location &sloc = std::source_location::current()) {

        auto *ctx = ctx_curr_;
        if (!ctx) {
            CHERROR("[node_builder] No active context for bit select operation "
                    "creation");
            return nullptr;
        }

        if (!operand.impl()) {
            CHERROR("[node_builder] Invalid operand for bit select operation");
            return nullptr;
        }

        if (statistics_enabled_) {
            ++statistics_->operations_built;
            ++statistics_->total_nodes_built;
        }

        // Create operation node specifically for bit selection
        opimpl *op_node = ctx->create_node<opimpl>(
            1, // bit_select always returns 1 bit
            ch_op::bit_sel, false, operand.impl(),
            ctx->create_literal(sdata_type(index, 32), name + "_idx", sloc),
            prefixed_name_helper(name, name_prefix_), sloc);

        // Create proxy node with correct size
        // proxyimpl *proxy_node = ctx->create_node<proxyimpl>(
        //     op_node, prefixed_name_helper("_" + name, name_prefix_),
        //     sloc);

        // return proxy_node;
        return op_node;
    }

    template <typename T, typename Index>
    lnodeimpl *build_bit_select(
        const lnode<T> &operand, const lnode<Index> &index,
        const std::string &name = "bit_select",
        const std::source_location &sloc = std::source_location::current()) {

        auto *ctx = ctx_curr_;
        if (!ctx) {
            CHERROR("[node_builder] No active context for bit select operation "
                    "creation");
            return nullptr;
        }

        if (!operand.impl() || !index.impl()) {
            CHERROR("[node_builder] Invalid operand or index for bit select "
                    "operation");
            return nullptr;
        }

        if (statistics_enabled_) {
            ++statistics_->operations_built;
            ++statistics_->total_nodes_built;
        }

        // Create operation node specifically for bit selection
        opimpl *op_node = ctx->create_node<opimpl>(
            1, // bit_select always returns 1 bit
            ch_op::bit_sel, false, operand.impl(), index.impl(),
            prefixed_name_helper(name, name_prefix_), sloc);

        // Create proxy node with correct size
        // proxyimpl *proxy_node = ctx->create_node<proxyimpl>(
        //     op_node, prefixed_name_helper("_" + name, name_prefix_), sloc);

        // return proxy_node;
        return op_node;
    }

    template <typename T>
    lnodeimpl *build_bits(
        const lnode<T> &operand, uint32_t msb, uint32_t lsb,
        const std::string &name = "bits",
        const std::source_location &sloc = std::source_location::current()) {
        CHDBG_FUNC();
        auto *ctx = ctx_curr_;
        if (!ctx) {
            CHERROR(
                "[node_builder] No active context for bits operation creation");
            return nullptr;
        }

        if (!operand.impl()) {
            CHERROR("[node_builder] Invalid operand for bits operation");
            return nullptr;
        }

        if (msb < lsb) {
            CHWARN("[node_builder] MSB (%u) < LSB (%u) in build_bits, swapping",
                   msb, lsb);
            std::swap(msb, lsb);
        }

        uint32_t width = msb - lsb + 1;
        if (statistics_enabled_) {
            ++statistics_->operations_built;
            ++statistics_->total_nodes_built;
        }

        // Create the bits extract operation node - use a literal to encode the
        // range The range info will be encoded in the second operand (src1)
        uint64_t range_encoding = (static_cast<uint64_t>(msb) << 32) | lsb;

        opimpl *op_node = ctx->create_node<opimpl>(
            width, ch_op::bits_extract, false, operand.impl(),
            ctx->create_literal(sdata_type(range_encoding, 64), name + "_range",
                                sloc),
            prefixed_name_helper(name, name_prefix_), sloc);

        // Create proxy node
        // proxyimpl *proxy_node = ctx->create_node<proxyimpl>(
        //     op_node, prefixed_name_helper("_" + name, name_prefix_), sloc);

        // return proxy_node;
        return op_node;
    }

    template <typename T, typename Index>
    lnodeimpl *build_bit_extract(
        const lnode<T> &operand, const lnode<Index> &index,
        unsigned result_width, const std::string &name = "bit_extract",
        const std::source_location &sloc = std::source_location::current());

    template <typename T, typename U>
    lnodeimpl *build_operation(
        ch_op op, const lnode<T> &lhs, const lnode<U> &rhs,
        uint32_t result_width, bool is_signed = false,
        const std::string &name = "op",
        const std::source_location &sloc = std::source_location::current());

    template <typename T>
    lnodeimpl *build_unary_operation(
        ch_op op, const lnode<T> &operand, uint32_t,
        const std::string &name = "unary_op",
        const std::source_location &sloc = std::source_location::current());

    template <typename T>
    lnodeimpl *build_operation(
        ch_op op, const lnode<T> &operand, uint32_t result_width,
        bool is_signed, const std::string &name = "op",
        const std::source_location &sloc = std::source_location::current());

private:
    node_builder();

    // 私有成员 (移除不完整类型的 unique_ptr)
    bool debug_mode_;
    optimization_level opt_level_;
    bool caching_enabled_;
    bool statistics_enabled_;
    std::string name_prefix_;
    std::unique_ptr<build_statistics> statistics_;
    // 移除 node_cache 和 optimization_manager 相关成员

    // 辅助函数
    std::string prefixed_name_helper(const std::string &name,
                                     const std::string &prefix) {
        if (prefix.empty()) {
            return name;
        }
        return prefix + "_" + name;
    }

    template <typename T> constexpr uint32_t get_literal_width(T value) {
        if (value == 0)
            return 1;
        if constexpr (std::is_integral_v<T> && std::is_unsigned_v<T>) {
            return static_cast<uint32_t>(std::bit_width(value));
        } else if constexpr (std::is_integral_v<T> && std::is_signed_v<T>) {
            using U = std::make_unsigned_t<T>;
            return get_literal_width(static_cast<U>(value));
        } else {
            return 1; // fallback
        }
    }
};

// 构造函数实现
inline node_builder::node_builder()
    : debug_mode_(false), opt_level_(optimization_level::normal),
      caching_enabled_(false), statistics_enabled_(false), name_prefix_(""),
      statistics_(std::make_unique<build_statistics>()) {}

} // namespace core
} // namespace ch

// 在文件末尾包含模板实现
#include "../lnode/node_builder.tpp"

#endif // CH_CORE_NODE_BUILDER_H