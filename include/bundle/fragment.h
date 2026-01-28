#ifndef CHLIB_FRAGMENT_H
#define CHLIB_FRAGMENT_H

#include "ch.hpp"
#include "flow_bundle.h"
#include "stream_bundle.h"
#include <array>
#include <type_traits>

namespace ch {

// ch_fragment Bundle定义 - 用于表示数据流中的片段
template <typename T>
struct ch_fragment : public core::bundle_base<ch_fragment<T>> {
    using Self = ch_fragment<T>;
    T data_beat;        // 片段数据
    core::ch_bool last; // 标记是否为最后一个片段

    ch_fragment() = default;
    explicit ch_fragment(const std::string &prefix) {
        this->set_name_prefix(prefix);
    }

    CH_BUNDLE_FIELDS_T(data_beat, last)

    void as_master_direction() { this->make_output(data_beat, last); }

    void as_slave_direction() { this->make_input(data_beat, last); }
};

// 将ch_flow<ch_fragment<T>>转换为ch_flow<T>的函数
template <typename T>
ch_flow<T> fragment_to_payload(ch_flow<ch_fragment<T>> flow) {
    ch_flow<T> result;
    result.payload = flow.payload.data_beat;
    result.valid = flow.valid;
    return result;
}

// 将ch_flow<T>转换为ch_fragment的函数
template <typename T>
ch_flow<ch_fragment<T>> payload_to_fragment(T payload, core::ch_bool last) {
    ch_flow<ch_fragment<T>> result;
    result.payload.data_beat = payload;
    result.payload.last = last;
    result.valid = true; // 假设总是有效
    return result;
}

// fragment处理相关的工具函数
// 将多个fragment组合成一个序列
template <typename T, size_t N>
std::array<ch_flow<ch_fragment<T>>, N>
fragment_sequence(const std::array<T, N> &data) {
    std::array<ch_flow<ch_fragment<T>>, N> result;
    for (size_t i = 0; i < N; ++i) {
        result[i].payload.data_beat = data[i];
        result[i].payload.last = (i == N - 1); // 最后一个元素标记为last
        result[i].valid = true;
    }
    return result;
}

// 检查fragment是否为最后一个
template <typename T>
core::ch_bool is_last_fragment(ch_flow<ch_fragment<T>> flow) {
    return flow.payload.last;
}

// 从ch_flow<ch_fragment<T>>中提取last信号
template <typename T>
core::ch_bool get_last_signal(ch_flow<ch_fragment<T>> flow) {
    return flow.payload.last;
}

// 从ch_flow<fragment<T>>中提取fragment数据
template <typename T> T get_fragment_data(ch_flow<ch_fragment<T>> flow) {
    return flow.payload.data_beat;
}

} // namespace ch

#endif // CHLIB_FRAGMENT_H