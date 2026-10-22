#pragma once

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
//
// ⚠️ MUST NOT 把字段**重绑定**成源位串的 `bit_select` / `bits<>` 切片 ——
//    字段 MUST 拥有**自己的**节点（「字段一等节点」原则，ADR-042）。
//    切片只作为**驱动源**（数据流边），MUST NOT 成为字段的节点本身。
//    理由：被重绑定成切片的字段，其「节点身份」= 源位串的某个位区间 ⇒
//    ① 字段不再是独立可寻址的一等节点；② 与 bundle 的其余连接语义不一致。
// （OpenSpec change `bundle-ecosystem-hardening-2026-10` 的 D2 / L3）
template <typename BundleT, unsigned W>
void deserialize_bits_to_fields(const ch_uint<W> &src_bits, BundleT &bundle) {
    if constexpr (is_bundle_v<BundleT>) {
        static_assert(requires { bundle.create_field_nodes(); },
                      "deserialize_bits_to_fields() requires a "
                      "bundle_base-derived bundle type");

        // 先为所有叶子字段建立**自己的**节点（幂等：已有节点不动）
        bundle.create_field_nodes();

        unsigned offset = 0;
        constexpr auto layout = get_bundle_layout<BundleT>();

        std::apply(
            [&](auto &&...field_info_args) {
                (([&]() {
                     auto &field_ref = bundle.*(field_info_args.ptr);
                     using FieldType =
                         std::remove_reference_t<decltype(field_ref)>;

                     if constexpr (is_bundle_v<FieldType>) {
                         // 嵌套 Bundle：**就地**反序列化（其字段同样是一等节点）
                         constexpr unsigned field_width =
                             get_bundle_width<FieldType>();

                         auto field_bits = bits<field_width>(src_bits, offset);
                         deserialize_bits_to_fields(field_bits, field_ref);
                         offset += field_width;
                     } else if constexpr (std::is_same_v<FieldType, ch_bool>) {
                         // ch_bool 类型：使用 bit_select 作为**驱动源**
                         auto field_bit = bit_select(src_bits, offset);
                         field_ref <<= field_bit;
                         offset++;
                     } else {
                         // ch_uint 等其他类型：使用 bits 提取作为**驱动源**
                         constexpr unsigned field_width = ch_width_v<FieldType>;
                         auto field_bits = bits<field_width>(src_bits, offset);
                         field_ref <<= field_bits;
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

