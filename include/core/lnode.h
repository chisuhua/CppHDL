// include/core/lnode.h
#ifndef CH_CORE_LNODE_H
#define CH_CORE_LNODE_H

#include <bit>
#include <type_traits>
#include <source_location>
#include <cstdint>

#include "lnodeimpl.h"
#include "traits.h"
#include "literal.h"
#include "node_builder.h"

namespace ch { namespace core {

// --- lnode wrapper ---
template<typename T>
struct lnode {
    lnodeimpl* impl() const { return impl_; }
    lnode(lnodeimpl* p) : impl_(p) {}
private:
    lnodeimpl* impl_ = nullptr;
};

// --- get_lnode overloads ---
template<typename T>
lnode<T> get_lnode(const lnode<T>& n) {
    return n;
}

template<typename T>
lnode<T> get_lnode(const T& t) requires requires { t.impl(); } {
    return lnode<T>(t.impl());
}

template<typename LiteralType>
requires std::is_arithmetic_v<LiteralType>
ch_literal get_lnode(LiteralType literal_val) {
    using Decayed = std::decay_t<LiteralType>;
    using UnsignedType = std::make_unsigned_t<Decayed>;
    
    uint32_t actual_width = get_literal_width(static_cast<UnsignedType>(literal_val));
    if (actual_width == 0) actual_width = 1;
    if (actual_width > 64) actual_width = 64;

    return ch_literal{
        static_cast<uint64_t>(literal_val),
        actual_width
    };
}

// Forward declaration of the internal implementation
template<typename T>
class ch_reg;

// Public alias for the user-facing register type
/*
template<typename T>
using ch_reg = const ch_reg_impl<T>;
*/

}} // namespace ch::core

#endif // CH_CORE_LNODE_H
