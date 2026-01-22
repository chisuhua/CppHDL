#ifndef CHLIB_FRAGMENT_H
#define CHLIB_FRAGMENT_H

#include "../ch.hpp"
#include "flow_bundle.h"
#include "stream_bundle.h"
#include <array>
#include <type_traits>

namespace ch {

// Fragment Bundle定义 - 用于表示数据流中的片段
template <typename T> struct Fragment : public core::bundle_base<Fragment<T>> {
    using Self = Fragment<T>;
    T fragment;         // 片段数据
    core::ch_bool last; // 标记是否为最后一个片段

    Fragment() = default;
    explicit Fragment(const std::string &prefix) {
        this->set_name_prefix(prefix);
    }

    CH_BUNDLE_FIELDS_T(fragment, last)

    void as_master_direction() {
        this->role_ = core::bundle_role::master;
        this->make_output(fragment, last);
    }

    void as_slave_direction() {
        this->role_ = core::bundle_role::slave;
        this->make_input(fragment, last);
    }
};

// Stream Bundle定义 - 用于表示带反压的数据流
template <typename DataT>
struct StreamBundle : public core::bundle_base<StreamBundle<DataT>> {
    using Self = StreamBundle<DataT>;
    DataT payload;
    core::ch_bool valid;
    core::ch_bool ready;

    StreamBundle() = default;
    explicit StreamBundle(const std::string &prefix) {
        this->set_name_prefix(prefix);
    }

    CH_BUNDLE_FIELDS_T(payload, valid, ready)

    void as_master_direction() {
        this->role_ = core::bundle_role::master;
        this->make_output(payload, valid);
        this->make_input(ready);
    }

    void as_slave_direction() {
        this->role_ = core::bundle_role::slave;
        this->make_input(payload, valid);
        this->make_output(ready);
    }

    // 递归连接功能，用于处理嵌套bundle
    template <typename OtherStream>
    void do_recursive_connect(const OtherStream &src) {
        // 根据当前角色确定连接方向
        if (this->get_role() == core::bundle_role::master) {
            // 如果是主设备，输出payload和valid，输入ready
            if constexpr (core::is_bundle_v<DataT>) {
                core::bundle_field_traits<DataT>::connect(src.payload,
                                                          this->payload);
            } else {
                this->payload = src.payload;
            }
            this->valid = src.valid;
            src.ready = this->ready; // 反向连接
        } else {
            // 如果是从设备，输入payload和valid，输出ready
            if constexpr (core::is_bundle_v<DataT>) {
                core::bundle_field_traits<DataT>::connect(src.payload,
                                                          this->payload);
            } else {
                this->payload = src.payload;
            }
            this->valid = src.valid;
            this->ready = src.ready;
        }
    }

    [[nodiscard]] core::ch_bool fire() const {
        return this->valid && this->ready;
    }
};

// 将Flow<Fragment<T>>转换为Flow<T>的函数
template <typename T>
ch::Flow<T> fragment_to_payload(ch::Flow<Fragment<T>> flow) {
    ch::Flow<T> result;
    result.payload = flow.value().payload.fragment;
    result.valid = flow.value().valid;
    return result;
}

// 将Flow<T>转换为Fragment的函数
template <typename T>
ch::Flow<Fragment<T>> payload_to_fragment(T payload, ch::core::ch_bool last) {
    ch::Flow<Fragment<T>> result;
    result.payload.fragment = payload;
    result.payload.last = last;
    result.valid = true; // 假设总是有效
    return result;
}

// Fragment处理相关的工具函数
// 将多个Fragment组合成一个序列
template <typename T, size_t N>
std::array<ch::Flow<Fragment<T>>, N>
fragment_sequence(const std::array<T, N> &data) {
    std::array<ch::Flow<Fragment<T>>, N> result;
    for (size_t i = 0; i < N; ++i) {
        result[i].payload.fragment = data[i];
        result[i].payload.last = (i == N - 1); // 最后一个元素标记为last
        result[i].valid = true;
    }
    return result;
}

// 检查Fragment是否为最后一个
template <typename T>
ch::core::ch_bool is_last_fragment(ch::Flow<Fragment<T>> flow) {
    return flow.value().payload.last;
}

// 从Flow<Fragment<T>>中提取last信号
template <typename T>
ch::core::ch_bool get_last_signal(ch::Flow<Fragment<T>> flow) {
    return flow.value().payload.last;
}

// 从Flow<Fragment<T>>中提取fragment数据
template <typename T> T get_fragment_data(ch::Flow<Fragment<T>> flow) {
    return flow.value().payload.fragment;
}

} // namespace ch

#endif // CHLIB_FRAGMENT_H