#ifndef CH_CORE_UINT_H
#define CH_CORE_UINT_H

#include "core/literal.h"
#include "core/operators.h" // 添加 operators.h 包含以使用 zext 等函数
#include <cstdint>
#include <source_location>
#include <string>
#include <variant>

// 前向声明，避免包含循环
namespace ch::core {
template <uint64_t V, uint32_t W> struct ch_literal_impl;
struct ch_literal_runtime;
struct ch_bool;
} // namespace ch::core

#include "core/bool.h"
#include "core/logic_buffer.h"
#include "core/traits.h"

namespace ch::core {

class lnodeimpl;

template <typename T> struct lnode;
struct ch_literal_runtime;

template <unsigned N> struct ch_uint : public logic_buffer<ch_uint<N>> {
    static constexpr unsigned width = N;
    static constexpr unsigned ch_width = N;

    using logic_buffer<ch_uint<N>>::logic_buffer;

    ch_uint() : logic_buffer<ch_uint<N>>() {}
    ch_uint(const ch_literal_runtime &val, const std::string &name = "uint_lit",
            const std::source_location &sloc = std::source_location::current())
        requires(N > 1);

    // 添加接受 ch_bool 类型的构造函数，只对 N==1 的情况启用
    ch_uint(const ch_bool &val, const std::string &name = "bool_to_uint",
            const std::source_location &sloc = std::source_location::current())
        requires(N == 1)
    {
        if (name == "bool_to_uint") {
            this->node_impl_ = val.impl();
        } else {
            // create new node with provided name
            this->node_impl_ = zext<N, ch_bool>(val, name).impl();
        }
    }

    // 提供一个工厂函数用于创建 ch_uint<1>，用户可以进一步扩展
    static ch_uint<1> make_bool(
        const ch_bool &val, const std::string &name = "bool_to_uint_1",
        const std::source_location &sloc = std::source_location::current()) {
        ch_uint<1> result;
        result.node_impl_ = val.impl();
        return result;
    }

    template <uint64_t V, uint32_t W>
    ch_uint(const ch_literal_impl<V, W> &val,
            const std::string &name = "uint_lit",
            const std::source_location &sloc = std::source_location::current());

    template <unsigned M>
    ch_uint(
        const ch_uint<M> &other, const std::string &name = "uint_conv",
        const std::source_location &sloc = std::source_location::current()) {
        if constexpr (M == N) {
            this->node_impl_ = other.impl();
            // if (name == "uint_conv") {
            //     this->node_impl_ = other.impl();
            // } else {
            //     this->node_impl_ = zext<N>(other, name).impl();
            // }
        } else if constexpr (M < N) {
            // 零扩展
            this->node_impl_ = zext<N>(other, name).impl();
        } else {
            // 截断
            this->node_impl_ = bits<N - 1, 0>(other).impl();
        }
    }

    explicit operator uint64_t() const;

    template <bool Enable = (N == 1), typename = std::enable_if_t<Enable>>
    operator ch_bool() const {
        return ch_bool(this->impl());
    }

    explicit ch_uint(lnodeimpl *node) : logic_buffer<ch_uint<N>>(node) {}

    // use as io
    using direction_type =
        std::variant<std::monostate, input_direction, output_direction>;
    mutable direction_type dir_ = std::monostate{};

    void set_direction(input_direction) const { dir_ = input_direction{}; }
    void set_direction(output_direction) const { dir_ = output_direction{}; }

    // 添加赋值操作符，代表硬件连接
    template <typename U> ch_uint &operator<<=(const U &value) {
        lnode<U> src_lnode = get_lnode(value);
        if (src_lnode.impl()) {
            if (this->node_impl_) {
                //       this->node_impl_->set_src(0, src_lnode.impl());
                CHERROR("[ch_uint::operator<<=] Error: node_impl_ or "
                        "src_lnode is not null for ch_uint<%d>!",
                        N);
            } else {
                this->node_impl_ =
                    node_builder::instance().build_unary_operation(
                        ch_op::assign, src_lnode, N,
                        src_lnode.impl()->name() + "_wire");

                // 使用编译期判断位宽差异
                // if constexpr (ch_width_v<U> < N) {
                //     // 源节点位宽小于目标节点，使用零扩展
                //     auto extended = zext<N>(value);
                //     this->node_impl_ = extended.impl();
                // } else if constexpr (ch_width_v<U> > N) {
                //     // 源节点位宽大于目标节点，使用bits操作提取低位
                //     auto truncated = bits<N - 1, 0>(value);
                //     this->node_impl_ = truncated.impl();
                // } else {
                //     // 位宽相同，直接使用源节点
                //     this->node_impl_ = src_lnode.impl();
                // }
            }
        } else {
            CHERROR("[ch_uint::operator=] Error: node_impl_ or "
                    "src_lnode is null for ch_uint<%d>!",
                    N);
        }
        return *this;
    }

    void flip_direction() const {
        if (std::holds_alternative<input_direction>(dir_)) {
            dir_ = output_direction{};
        } else if (std::holds_alternative<output_direction>(dir_)) {
            dir_ = input_direction{};
        }
    }
    // 获取当前方向（用于 connect）
    direction_type direction() const { return dir_; }

    // friend auto to_operand(const ch_uint<N>&);
    template <unsigned U>
    friend inline lnode<ch_uint<U>> get_lnode(const ch_uint<U> &);
    friend ch_bool;

    template <unsigned Width>
    friend constexpr auto make_uint_result(lnodeimpl *node);

    // Bool -> UInt
};

template <uint32_t W> inline constexpr auto make_uint(uint64_t value) {
    return ch_uint<W>(make_literal(value, W));
}

// ==================== get_lnode 特化 ====================
template <unsigned N>
inline lnode<ch_uint<N>> get_lnode(const ch_uint<N> &uint_val) {
    return lnode<ch_uint<N>>(uint_val.impl());
}

// ==================== ch_width 特化 ====================
template <unsigned N> struct ch_width_impl<ch_uint<N>, void> {
    static constexpr unsigned value = N;
};

// ==================== 便捷类型别名 ====================
using ch_uint1 = ch_uint<1>;
using ch_uint8 = ch_uint<8>;
using ch_uint16 = ch_uint<16>;
using ch_uint32 = ch_uint<32>;
using ch_uint64 = ch_uint<64>;

} // namespace ch::core

#include "../lnode/uint.tpp"

#endif // CH_CORE_UINT_H