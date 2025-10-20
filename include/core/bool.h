// include/core/bool.h
#ifndef CH_CORE_BOOL_H
#define CH_CORE_BOOL_H

#include "core/logic_buffer.h"
#include "core/traits.h"
#include "core/direction.h"
#include <source_location>
#include <variant>
#include <string>

namespace ch::core {

class lnodeimpl;

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
    direction_type direction() const { return dir_; }

    explicit ch_bool(lnodeimpl* node) : logic_buffer<ch_bool>(node) {}


    friend ch_bool make_bool_result(lnodeimpl* node);
    //friend lnode<ch_bool> get_lnode(const ch_bool&);
 
};

template<>
struct ch_width_impl<ch_bool, void> {
    static constexpr unsigned value = 1;
};

template<>
struct ch_width_impl<const ch_bool, void> {
    static constexpr unsigned value = 1;
};


} // namespace ch::core

#endif // CH_CORE_BOOL_H
