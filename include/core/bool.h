// include/core/bool.h
#ifndef CH_CORE_BOOL_H
#define CH_CORE_BOOL_H

#include "core/logic_buffer.h"
#include "core/traits.h"
#include <source_location>

namespace ch { namespace core {

template<unsigned N>
struct ch_uint; 

using ch_uint1 = ch_uint<1>;

struct ch_literal;

struct ch_bool : public logic_buffer<ch_bool> {
    static constexpr unsigned width = 1;
    static constexpr unsigned ch_width = 1;
    
    using logic_buffer<ch_bool>::logic_buffer;

    ch_bool(bool val, 
           const std::string& name = "bool_lit",
           const std::source_location& sloc = std::source_location::current());
    
    ch_bool(const ch_literal& val,
           const std::string& name = "bool_lit",
           const std::source_location& sloc = std::source_location::current());
    
    ch_bool() : logic_buffer<ch_bool>() {}
    
    // 显式转换为 bool
    explicit operator uint64_t() const;
    explicit operator bool() const;
    
    // 隐式转换为 ch_uint<1>（用于兼容性）
    operator ch_uint1() const ;

    explicit ch_bool(lnodeimpl* node) : logic_buffer<ch_bool>(node) {}

    friend ch_bool make_bool_result(lnodeimpl* node);
    friend lnode<ch_bool> get_lnode(const ch_bool&);
 
};

lnode<ch_bool> get_lnode(const ch_bool& bool_val) ;

template<>
struct ch_width_impl<ch_bool, void> {
    static constexpr unsigned value = 1;
};

template<>
struct ch_width_impl<const ch_bool, void> {
    static constexpr unsigned value = 1;
};

}} // namespace ch::core

#endif // CH_CORE_BOOL_H
