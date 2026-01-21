#ifndef CHLIB_FRAGMENT_H
#define CHLIB_FRAGMENT_H

#include "../ch.hpp"
#include "stream.h"
#include <array>
#include <type_traits>

namespace chlib {

// Fragment Bundle定义 - 用于表示数据流中的片段
template<typename T>
struct Fragment : public ch::core::bundle_base<Fragment<T>> {
    using Self = Fragment<T>;
    T fragment;  // 片段数据
    ch::core::ch_bool last;  // 标记是否为最后一个片段

    Fragment() = default;
    
    // 从节点和偏移构造（用于嵌套bundle）
    explicit Fragment(ch::core::lnodeimpl* node, unsigned base_offset = 0) 
        : ch::core::bundle_base<Fragment<T>>(node, this->get_role()) {
        // 初始化字段，如果是bundle类型则递归处理
        initialize_fields_with_offset(base_offset);
    }
    
    explicit Fragment(const std::string& prefix = "fragment") {
        this->set_name_prefix(prefix);
    }

    CH_BUNDLE_FIELDS(Self, fragment, last)

    void as_master() override {
        this->role_ = ch::core::bundle_role::master;
        this->make_output(fragment, last);
    }

    void as_slave() override {
        this->role_ = ch::core::bundle_role::slave;
        this->make_input(fragment, last);
    }

private:
    void initialize_fields_with_offset(unsigned base_offset) {
        unsigned offset = base_offset;
        
        // 根据fragment类型决定如何初始化
        if constexpr (ch::core::is_bundle_v<T>) {
            // 如果fragment是一个bundle，递归初始化它
            fragment = T(this->impl(), offset);
            offset += fragment.width();
        } else {
            // 否则是基本类型，创建位切片
            unsigned w = ch::core::get_field_width<T>();
            if (w == 1) {
                fragment = ch::core::ch_bool(this->bit_select(offset++));
            } else {
                fragment = T(ch::core::bits(this->impl(), offset + w - 1, offset));
                offset += w;
            }
        }
        
        // 初始化last信号
        last = ch::core::ch_bool(this->bit_select(offset));
    }
};

// Stream Bundle定义 - 用于表示带反压的数据流
template<typename DataT>
struct Stream : public ch::core::bundle_base<Stream<DataT>> {
    using Self = Stream<DataT>;
    DataT payload;
    ch::core::ch_bool valid;
    ch::core::ch_bool ready;

    Stream() = default;
    
    // 从节点和偏移构造（用于嵌套bundle）
    explicit Stream(ch::core::lnodeimpl* node, unsigned base_offset = 0) 
        : ch::core::bundle_base<Stream<DataT>>(node, this->get_role()) {
        // 初始化字段，如果是bundle类型则递归处理
        initialize_fields_with_offset(base_offset);
    }
    
    explicit Stream(const std::string& prefix = "stream") {
        this->set_name_prefix(prefix);
    }

    CH_BUNDLE_FIELDS(Self, payload, valid, ready)

    void as_master() override {
        this->role_ = ch::core::bundle_role::master;
        this->make_output(payload, valid);
        this->make_input(ready);
    }

    void as_slave() override {
        this->role_ = ch::core::bundle_role::slave;
        this->make_input(payload, valid);
        this->make_output(ready);
    }
    
    // 自定义连接行为以处理反向信号
    void do_connect_from(const Stream<DataT>& src) {
        // 正向连接：payload 和 valid
        if constexpr (ch::core::is_bundle_v<DataT>) {
            // 如果payload是bundle，递归连接
            this->do_recursive_connect(this->payload, src.payload);
        } else {
            // 如果是基本类型，直接连接
            this->payload.impl()->add_src(src.payload.impl());
        }
        this->valid.impl()->add_src(src.valid.impl());
        
        // 反向连接：ready
        src.ready.impl()->add_src(this->ready.impl());
    }
    
    [[nodiscard]] ch::core::ch_bool fire() const { 
        return this->valid && this->ready; 
    }

private:
    void initialize_fields_with_offset(unsigned base_offset) {
        unsigned offset = base_offset;
        
        // 初始化payload
        if constexpr (ch::core::is_bundle_v<DataT>) {
            // 如果payload是一个bundle，递归初始化它
            payload = DataT(this->impl(), offset);
            offset += payload.width();
        } else {
            // 否则是基本类型，创建位切片
            unsigned w = ch::core::get_field_width<DataT>();
            if (w == 1) {
                payload = ch::core::ch_bool(this->bit_select(offset++));
            } else {
                payload = DataT(ch::core::bits(this->impl(), offset + w - 1, offset));
                offset += w;
            }
        }
        
        // 初始化valid信号
        valid = ch::core::ch_bool(this->bit_select(offset++));
        
        // 初始化ready信号
        ready = ch::core::ch_bool(this->bit_select(offset));
    }
};

// 将Flow<Fragment<T>>转换为Flow<T>的函数
template<typename T>
ch::Flow<T> fragment_to_payload(ch::Flow<Fragment<T>> flow) {
    ch::Flow<T> result;
    result.payload = flow.value().payload.fragment;
    result.valid = flow.value().valid;
    return result;
}

// 将Flow<T>转换为Fragment的函数
template<typename T>
ch::Flow<Fragment<T>> payload_to_fragment(T payload, ch::core::ch_bool last) {
    ch::Flow<Fragment<T>> result;
    result.payload.fragment = payload;
    result.payload.last = last;
    result.valid = true;  // 假设总是有效
    return result;
}

// Fragment处理相关的工具函数
// 将多个Fragment组合成一个序列
template<typename T, size_t N>
std::array<ch::Flow<Fragment<T>>, N> fragment_sequence(const std::array<T, N>& data) {
    std::array<ch::Flow<Fragment<T>>, N> result;
    for(size_t i = 0; i < N; ++i) {
        result[i].payload.fragment = data[i];
        result[i].payload.last = (i == N - 1);  // 最后一个元素标记为last
        result[i].valid = true;
    }
    return result;
}

// 检查Fragment是否为最后一个
template<typename T>
ch::core::ch_bool is_last_fragment(ch::Flow<Fragment<T>> flow) {
    return flow.value().payload.last;
}

// 从Flow<Fragment<T>>中提取last信号
template<typename T>
ch::core::ch_bool get_last_signal(ch::Flow<Fragment<T>> flow) {
    return flow.value().payload.last;
}

// 从Flow<Fragment<T>>中提取fragment数据
template<typename T>
T get_fragment_data(ch::Flow<Fragment<T>> flow) {
    return flow.value().payload.fragment;
}

} // namespace chlib

#endif // CHLIB_FRAGMENT_H