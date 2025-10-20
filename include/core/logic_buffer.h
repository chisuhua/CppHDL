// include/core/logic_buffer.h
#ifndef CH_CORE_LOGIC_BUFFER_H
#define CH_CORE_LOGIC_BUFFER_H

namespace ch::core {

class ch_bool;
class lnodeimpl;

// ==================== logic_buffer 基类 ====================
template<typename T>
struct logic_buffer {
    // 获取底层 IR 节点
    lnodeimpl* impl() const { return node_impl_; }
    
    // 从现有节点构造
    logic_buffer(lnodeimpl* node);
    
    // 默认构造（创建空节点）
    logic_buffer();
    
    // 拷贝构造
    logic_buffer(const logic_buffer& other);
    
    // 赋值操作符
    logic_buffer& operator=(const logic_buffer& other);
    
    // 比较操作
    bool operator==(const logic_buffer& other) const;
    bool operator!=(const logic_buffer& other) const;
    
    // === 位操作增强 ===
    
    // 位选择操作符
    auto operator[](unsigned index) const;
    
    // 位切片操作符
    auto operator()(unsigned msb, unsigned lsb) const;
    
    // 获取最高位
    auto msb() const;
    
    // 获取最低位
    auto lsb() const;
    
    // === 类型转换增强 ===
    
    // 通用类型转换
    template<unsigned NewWidth>
    auto as() const;
    
    // 符号扩展
    template<unsigned NewWidth>
    auto sext() const;
    
    // 零扩展
    template<unsigned NewWidth>
    auto zext() const;
    
    // === 查询操作 ===
    
    // 是否为零
    bool is_zero() const;
    
    // 是否为全1
    bool is_ones() const;
    
    // 是否为2的幂
    bool is_power_of_two() const;
    
    // 获取位宽
    static constexpr unsigned width();

//protected:
    lnodeimpl* node_impl_;
};

} // namespace ch::core

#include "../lnode/logic_buffer.tpp"  // 包含模板实现

#endif // CH_CORE_LOGIC_BUFFER_H
