// include/core/logic_buffer.h
#ifndef CH_CORE_LOGIC_BUFFER_H
#define CH_CORE_LOGIC_BUFFER_H

#include "core/lnodeimpl.h"  // 包含 lnodeimpl

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

//protected:
    lnodeimpl* node_impl_ = nullptr;
};

}}

#endif // CH_CORE_LOGIC_BUFFER_H
