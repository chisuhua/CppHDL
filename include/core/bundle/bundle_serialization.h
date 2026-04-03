#ifndef CH_CORE_BUNDLE_SERIALIZATION_H
#define CH_CORE_BUNDLE_SERIALIZATION_H

#include "bundle_layout.h" // 添加 bundle_layout 支持
#include "bundle_meta.h"
#include "bundle_traits.h"
#include "core/bool.h"
#include "core/operators.h"
#include "core/traits.h" // 为了 ch_width_v
#include "core/uint.h"

#include <tuple>
#include <type_traits>

namespace ch::core {

// 从 Bundle 序列化为位向量
template <typename BundleT> auto serialize(const BundleT &bundle) {
    static_assert(is_bundle_v<BundleT>,
                  "serialize() only works with Bundle types");

    if constexpr (is_bundle_v<BundleT>) {
        // 使用带有布局信息的字段元数据
        constexpr auto layout = get_bundle_layout<BundleT>();
        constexpr unsigned W = get_bundle_width<BundleT>();
        ch_uint<W> result{0};

        // 计算偏移量并设置结果
        unsigned offset = 0;
        std::apply(
            [&](auto &&...f) {
                auto set_field = [&](const auto &field_info) {
                    auto &field_ref = bundle.*(field_info.ptr);
                    using FieldType =
                        std::remove_cvref_t<decltype(bundle.*(field_info.ptr))>;

                    if constexpr (is_bundle_v<FieldType>) {
                        // 对于嵌套的 Bundle 类型
                        constexpr unsigned field_width =
                            get_bundle_width<FieldType>();

                        auto sub_bundle_result =
                            serialize<FieldType>(bundle.*(field_info.ptr));
                        result = bits_update<field_width>(result, sub_bundle_result,
                                                          offset);
                        offset += field_width;
                    } else {
                        constexpr unsigned field_width = ch_width_v<FieldType>;
                        auto field_val = bundle.*(field_info.ptr);
                        result = bits_update<field_width>(result, field_val, offset);
                        offset += field_width;
                    }
                };

                (set_field(f), ...);
            },
            layout);

        return result;
    } else {
        return ch_uint<1>{0}; // 返回默认值，避免不完整类型错误
    }
}

// 从位向量反序列化到 Bundle 的字段
template <typename BundleT, unsigned W>
void deserialize_bits_to_fields(const ch_uint<W> &src_bits, BundleT &bundle) {
    if constexpr (is_bundle_v<BundleT>) {
        unsigned offset = 0;
        constexpr auto layout = get_bundle_layout<BundleT>();

        std::apply(
            [&](auto &&...field_info_args) {
                (([&]() {
                     auto &field_ref = bundle.*(field_info_args.ptr);
                     using FieldType =
                         std::remove_reference_t<decltype(field_ref)>;

                     if constexpr (is_bundle_v<FieldType>) {
                         // 对于嵌套的 Bundle 类型
                         constexpr unsigned field_width =
                             get_bundle_width<FieldType>();

                         // 提取对应宽度的位并反序列化嵌套 Bundle
                         auto field_bits = bits<field_width>(src_bits, offset);
                         field_ref = deserialize<FieldType>(field_bits);
                         offset += field_width;
                     } else if constexpr (std::is_same_v<FieldType, ch_bool>) {
                         // ch_bool 类型：使用 bit_select
                         auto field_bit = bit_select(src_bits, offset);
                         field_ref = ch_bool(field_bit.impl());
                         offset++;
                     } else {
                         // ch_uint 等其他类型：使用 bits 提取
                         constexpr unsigned field_width = ch_width_v<FieldType>;
                         auto field_bits = bits<field_width>(src_bits, offset);
                         field_ref = FieldType(field_bits);
                         offset += field_width;
                     }
                 }()),
                 ...);
            },
            layout);
    }
}

// 从位向量反序列化到 Bundle
template <typename BundleT, unsigned W>
BundleT deserialize(const ch_uint<W> &src_bits) {
    static_assert(is_bundle_v<BundleT>,
                  "deserialize() only works with Bundle types");

    if constexpr (is_bundle_v<BundleT>) {
        BundleT bundle;
        deserialize_bits_to_fields(src_bits, bundle);
        return bundle;
    } else {
        return BundleT{}; // 返回默认值
    }
}

} // namespace ch::core

#endif // CH_CORE_BUNDLE_SERIALIZATION_H
