#ifndef CHLIB_FRAGMENT_H
#define CHLIB_FRAGMENT_H

#include "../ch.hpp"
#include "stream.h"
#include <array>
#include <type_traits>

namespace chlib {

// Fragment Bundle定义 - 用于表示数据流中的片段
template <typename T>
struct FragmentBundle : public bundle_base<FragmentBundle<T>> {
    using Self = FragmentBundle<T>;
    T fragment;   // 片段数据
    ch_bool last; // 标记是否为最后一个片段

    FragmentBundle() = default;
    explicit FragmentBundle(const std::string &prefix = "fragment") {
        this->set_name_prefix(prefix);
    }

    CH_BUNDLE_FIELDS_T(fragment, last)

    void as_master() override { this->make_output(fragment, last); }

    void as_slave() override { this->make_input(fragment, last); }
};

// 将Flow<FragmentBundle<T>>转换为Flow<T>的函数
template <typename T>
Flow<T> fragment_to_payload(Flow<FragmentBundle<T>> flow) {
    Flow<T> result;
    result.payload = flow.value().payload.fragment;
    result.valid = flow.value().valid;
    return result;
}

// 将Flow<T>转换为FragmentBundle的函数
template <typename T>
Flow<FragmentBundle<T>> payload_to_fragment(T payload, ch_bool last) {
    Flow<FragmentBundle<T>> result;
    result.payload.fragment = payload;
    result.payload.last = last;
    result.valid = true; // 假设总是有效
    return result;
}

// Fragment处理相关的工具函数
// 将多个FragmentBundle组合成一个序列
template <typename T, size_t N>
std::array<Flow<FragmentBundle<T>>, N>
fragment_sequence(const std::array<T, N> &data) {
    std::array<Flow<FragmentBundle<T>>, N> result;
    for (size_t i = 0; i < N; ++i) {
        result[i].payload.fragment = data[i];
        result[i].payload.last = (i == N - 1); // 最后一个元素标记为last
        result[i].valid = true;
    }
    return result;
}

// 检查Fragment是否为最后一个
template <typename T> ch_bool is_last_fragment(Flow<FragmentBundle<T>> flow) {
    return flow.value().payload.last;
}

// 从Flow<FragmentBundle<T>>中提取last信号
template <typename T> ch_bool get_last_signal(Flow<FragmentBundle<T>> flow) {
    return flow.value().payload.last;
}

// 从Flow<FragmentBundle<T>>中提取fragment数据
template <typename T> T get_fragment_data(Flow<FragmentBundle<T>> flow) {
    return flow.value().payload.fragment;
}

} // namespace chlib

#endif // CHLIB_FRAGMENT_H