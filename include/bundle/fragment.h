#ifndef CHLIB_FRAGMENT_H
#define CHLIB_FRAGMENT_H

#include "../ch.hpp"
#include "flow_bundle.h"
#include "stream_bundle.h"
#include <array>
#include <type_traits>

namespace ch {

// Fragment Bundle定义 - 用于表示数据流中的片段
template <typename T>
struct Fragment : public ch::core::bundle_base<Fragment<T>> {
    using Self = Fragment<T>;

    T fragment;             // 片段数据
    ch::core::ch_bool last; // 标记是否为最后一个片段

    Fragment() = default;

    // 从节点和偏移构造（用于嵌套bundle）
    // explicit Fragment(ch::core::lnodeimpl *node, unsigned base_offset = 0)
    //     : ch::core::bundle_base<Fragment<T>>(node) {
    //     // 初始化字段，如果是bundle类型则递归处理
    //     initialize_fields_with_offset(base_offset);
    // }

    explicit Fragment(const std::string &prefix) {
        this->set_name_prefix(prefix);
    }

    CH_BUNDLE_FIELDS_T(fragment, last)

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
                fragment =
                    T(ch::core::bits(this->impl(), offset + w - 1, offset));
                offset += w;
            }
        }

        // 初始化last信号
        last = ch::core::ch_bool(this->bit_select(offset));
    }
};

// // Stream Bundle定义 - 用于表示带反压的数据流
// template <typename DataT>
// struct Stream : public ch::core::bundle_base<Stream<DataT>> {
//     using Self = Stream<DataT>;
//     DataT payload;
//     ch::core::ch_bool valid;
//     ch::core::ch_bool ready;

//     Stream() = default;

//     // 从节点和偏移构造（用于嵌套bundle）
//     explicit Stream(ch::core::lnodeimpl *node, unsigned base_offset = 0)
//         : ch::core::bundle_base<Stream<DataT>>(node) {
//         // 初始化字段，如果是bundle类型则递归处理
//         initialize_fields_with_offset(base_offset);
//     }

//     explicit Stream(const std::string &prefix = "stream") {
//         this->set_name_prefix(prefix);
//     }

//     CH_BUNDLE_FIELDS_T(payload, valid, ready)

//     void as_master() override {
//         this->role_ = ch::core::bundle_role::master;
//         this->make_output(payload, valid);
//         this->make_input(ready);
//     }

//     void as_slave() override {
//         this->role_ = ch::core::bundle_role::slave;
//         this->make_input(payload, valid);
//         this->make_output(ready);
//     }

//     // 递归连接功能，用于处理嵌套bundle
//     template <typename OtherStream>
//     void do_recursive_connect(const OtherStream &src) {
//         // 根据当前角色确定连接方向
//         if (this->get_role() == ch::core::bundle_role::master) {
//             // 如果是主设备，输出payload和valid，输入ready
//             if constexpr (ch::core::is_bundle_v<DataT>) {
//                 ch::core::bundle_field_traits<DataT>::connect(src.payload,
//                                                               this->payload);
//             } else {
//                 this->payload = src.payload;
//             }
//             this->valid = src.valid;
//             src.ready = this->ready; // 反向连接
//         } else {
//             // 如果是从设备，输入payload和valid，输出ready
//             if constexpr (ch::core::is_bundle_v<DataT>) {
//                 ch::core::bundle_field_traits<DataT>::connect(src.payload,
//                                                               this->payload);
//             } else {
//                 this->payload = src.payload;
//             }
//             this->valid = src.valid;
//             this->ready = src.ready;
//         }
//     }

//     [[nodiscard]] ch::core::ch_bool fire() const {
//         return this->valid && this->ready;
//     }

// private:
//     void initialize_fields_with_offset(unsigned base_offset) {
//         unsigned offset = base_offset;

//         // 初始化payload
//         if constexpr (ch::core::is_bundle_v<DataT>) {
//             // 如果payload是一个bundle，递归初始化它
//             payload = DataT(this->impl(), offset);
//             offset += payload.width();
//         } else {
//             // 否则是基本类型，创建位切片
//             unsigned w = ch::core::get_field_width<DataT>();
//             if (w == 1) {
//                 payload = ch::core::ch_bool(this->bit_select(offset++));
//             } else {
//                 payload =
//                     DataT(ch::core::bits(this->impl(), offset + w - 1,
//                     offset));
//                 offset += w;
//             }
//         }

//         // 初始化valid信号
//         valid = ch::core::ch_bool(this->bit_select(offset++));

//         // 初始化ready信号
//         ready = ch::core::ch_bool(this->bit_select(offset));
//     }
// };

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