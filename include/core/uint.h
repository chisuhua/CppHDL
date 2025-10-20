// include/core/uint.h
#ifndef CH_CORE_UINT_H
#define CH_CORE_UINT_H

#include <cstdint>
#include <source_location>

#include "core/traits.h"
#include "core/logic_buffer.h"
#include "core/bool.h"

namespace ch { namespace core {

class lnodeimpl;

template<typename T> struct lnode;
struct ch_literal;


template<unsigned N>
struct ch_uint : public logic_buffer<ch_uint<N>> {
    static constexpr unsigned width = N;
    static constexpr unsigned ch_width = N;
    
    using logic_buffer<ch_uint<N>>::logic_buffer;

    ch_uint() : logic_buffer<ch_uint<N>>() {}
    ch_uint(const ch_literal& val,
           const std::string& name = "uint_lit",
           const std::source_location& sloc = std::source_location::current());

    template<unsigned M>
    ch_uint(const ch_uint<M>& other,
           const std::string& name = "uint_conv",
           const std::source_location& sloc = std::source_location::current()) {
        if constexpr (M <= N) {
            // 零扩展
            this->node_impl_ = zero_extend(other, N).impl();
        } else {
            // 截断
            this->node_impl_ = bits(other, N-1, 0).impl();
        }
    }

    explicit operator uint64_t() const;

    template<bool Enable = (N == 1), typename = std::enable_if_t<Enable>>
    operator ch_bool() const {
        return ch_bool(this->impl());
    }

    explicit ch_uint(lnodeimpl* node) : logic_buffer<ch_uint<N>>(node) {}

    // use as io
    using direction_type = std::variant<std::monostate, input_direction, output_direction>;
    mutable direction_type dir_ = std::monostate{};

    void set_direction(input_direction) const { dir_ = input_direction{}; }
    void set_direction(output_direction) const { dir_ = output_direction{}; }

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
    template<unsigned U>
    friend inline lnode<ch_uint<U>> get_lnode(const ch_uint<U>&);
    friend ch_bool;

    template<unsigned Width>
    friend constexpr auto make_uint_result(lnodeimpl* node);
};

// ==================== get_lnode 特化 ====================
template<unsigned N>
inline lnode<ch_uint<N>> get_lnode(const ch_uint<N>& uint_val) {
    return lnode<ch_uint<N>>(uint_val.impl());
}

// ==================== ch_width 特化 ====================
template<unsigned N>
struct ch_width_impl<ch_uint<N>, void> {
    static constexpr unsigned value = N;
};

// ==================== 便捷类型别名 ====================
using ch_uint1 = ch_uint<1>;
using ch_uint8 = ch_uint<8>;
using ch_uint16 = ch_uint<16>;
using ch_uint32 = ch_uint<32>;
using ch_uint64 = ch_uint<64>;

}}

#endif // CH_CORE_UINT_H
