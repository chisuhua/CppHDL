// include/core/uint.h
#ifndef UINT_H
#define UINT_H

#include <cstdint>
#include <source_location>

// 前向声明必要的类型
namespace ch { 
    namespace core {
        class lnodeimpl;
        template<typename T> struct lnode;
        struct sdata_type;
    }
}

namespace ch { namespace core {

// ==================== logic_buffer 基类 ====================
template<typename T>
struct logic_buffer {
    // 获取底层 IR 节点
    lnodeimpl* impl() const { return node_impl_; }
    
    // 从现有节点构造
    logic_buffer(lnodeimpl* node) : node_impl_(node) {}
    
    // 默认构造（创建空节点）
    logic_buffer() : node_impl_(nullptr) {}
    
    // 拷贝构造
    logic_buffer(const logic_buffer& other) : node_impl_(other.node_impl_) {}
    
    // 赋值操作符
    logic_buffer& operator=(const logic_buffer& other) {
        if (this != &other) {
            node_impl_ = other.node_impl_;
        }
        return *this;
    }
    
    // 比较操作
    bool operator==(const logic_buffer& other) const {
        return node_impl_ == other.node_impl_;
    }
    
    bool operator!=(const logic_buffer& other) const {
        return node_impl_ != other.node_impl_;
    }

    lnodeimpl* node_impl_ = nullptr;
};

// ==================== ch_uint 定义 ====================
template<unsigned N>
struct ch_uint : public logic_buffer<ch_uint<N>> {
    static constexpr unsigned width = N;
    static constexpr unsigned ch_width = N;
    
    // 继承基类构造函数
    using logic_buffer<ch_uint<N>>::logic_buffer;

    // 从 uint64_t 值构造 - 创建常量节点（构建 IR）
    ch_uint(uint64_t val, const std::source_location& sloc = std::source_location::current());
    
    // 从 sdata_type 构造 - 创建常量节点
    ch_uint(const sdata_type& val, const std::source_location& sloc = std::source_location::current());
    
    // 默认构造函数
    ch_uint() : logic_buffer<ch_uint<N>>() {}
    
    // 显式转换为 uint64_t（用于获取常量值）
    explicit operator uint64_t() const;
};

// ==================== ch_width 特化 ====================
template<unsigned N>
constexpr unsigned ch_width_v<ch_uint<N>> = N;

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

}} // namespace ch::core

#endif // UINT_H
