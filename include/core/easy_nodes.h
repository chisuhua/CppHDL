// include/core/easy_nodes.h
#ifndef CH_CORE_EASY_NODES_H
#define CH_CORE_EASY_NODES_H

#include "ch.hpp"
#include "node_builder.h"
#include <string>
#include <source_location>

namespace ch { namespace core {

// 简化的节点创建函数

// 简化的寄存器创建
template<typename T>
ch_reg<T> make_register(const T& initial_value, 
                       const std::string& name = "reg",
                       const std::source_location& sloc = std::source_location::current()) {
    return ch_reg<T>(initial_value, name, sloc);
}

template<typename T>
ch_reg<T> make_register(const std::string& name = "reg",
                       const std::source_location& sloc = std::source_location::current()) {
    return ch_reg<T>(name, sloc);
}

// 简化的输入创建
template<typename T>
ch_in<T> make_input(const std::string& name = "input",
                   const std::source_location& sloc = std::source_location::current()) {
    return ch_in<T>(name, sloc);
}

// 简化的输出创建
template<typename T>
ch_out<T> make_output(const std::string& name = "output",
                     const std::source_location& sloc = std::source_location::current()) {
    return ch_out<T>(name, sloc);
}

// 简化的文字创建
template<typename T>
ch_literal make_literal(T value,
                       const std::string& name = "literal",
                       const std::source_location& sloc = std::source_location::current()) {
    return ch_literal(value);
}

}} // namespace ch::core

// 为用户提供一个简单的包含文件
namespace ch {
    using namespace ch::core;
    
    // 重用现有的操作符和函数
    using ch::core::make_register;
    using ch::core::make_input;
    using ch::core::make_output;
    using ch::core::make_literal;
}

#endif // CH_CORE_EASY_NODES_H