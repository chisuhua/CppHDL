#ifndef CH_CORE_BUNDLE_SERIALIZATION_H
#define CH_CORE_BUNDLE_SERIALIZATION_H

#include "core/bundle/bundle_meta.h"
#include "core/bundle/bundle_traits.h"
#include "core/uint.h"
#include "core/bool.h"
#include "core/bundle/bundle_utils.h"
#include "core/operators.h"
#include <tuple>
#include <type_traits>

namespace ch::core {

// 从Bundle序列化为位向量
template <typename BundleT>
auto serialize(const BundleT &bundle) {
    if constexpr (is_bundle_v<BundleT>) {
        constexpr auto fields = BundleT::__bundle_fields();
        constexpr unsigned W = get_bundle_width<BundleT>();
        ch_uint<W> result;
        
        // 计算偏移量并设置结果
        unsigned offset = 0;
        std::apply([&](auto &&...f) {
            auto set_field = [&](const auto &field_info) {
                using FieldType = std::remove_cvref_t<decltype(bundle.*(field_info.ptr))>;
                
                if constexpr (requires { bundle.*(field_info.ptr).to_bits(); }) {
                    // 如果字段有自己的to_bits方法（如其他bundle）
                    auto field_bits = (bundle.*(field_info.ptr)).to_bits();
                    result = assign_bits(result, field_bits, offset, field_info.width);
                } else {
                    // 否则直接使用字段值
                    auto field_val = bundle.*(field_info.ptr);
                    auto field_bits = to_bits_wrapper(field_val);
                    result = assign_bits(result, field_bits, offset, field_info.width);
                }
                
                offset += field_info.width;
            };
            
            (set_field(f), ...);
        }, fields);
        
        return result;
    } else {
        // 对非bundle类型返回空结果，但不会导致编译错误
        static_assert(is_bundle_v<BundleT>, "serialize() only works with Bundle types");
        return ch_uint<1>{0};  // 返回默认值，避免不完整类型错误
    }
}

// 将Bundle的字段序列化到位向量
template <typename BundleT, unsigned W>
void serialize_fields_to_bits(const BundleT &bundle, ch_uint<W> &result) {
    if constexpr (is_bundle_v<BundleT>) {
        constexpr auto fields = BundleT::__bundle_fields();
        unsigned offset = 0;
        
        std::apply([&](auto &&...f) {
            auto set_field = [&](const auto &field_info) {
                using FieldType = std::remove_cvref_t<decltype(bundle.*(field_info.ptr))>;
                
                if constexpr (requires { bundle.*(field_info.ptr).to_bits(); }) {
                    // 如果字段有自己的to_bits方法（如其他bundle）
                    auto field_bits = (bundle.*(field_info.ptr)).to_bits();
                    result = assign_bits(result, field_bits, offset, field_info.width);
                } else {
                    // 否则直接使用字段值
                    auto field_val = bundle.*(field_info.ptr);
                    auto field_bits = to_bits_wrapper(field_val);
                    result = assign_bits(result, field_bits, offset, field_info.width);
                }
                
                offset += field_info.width;
            };
            
            (set_field(f), ...);
        }, fields);
    }
}

// 从位向量反序列化到Bundle的字段
template <typename BundleT, unsigned W>
void deserialize_bits_to_fields(const ch_uint<W> &bits, BundleT &bundle) {
    if constexpr (is_bundle_v<BundleT>) {
        constexpr auto fields = BundleT::__bundle_fields();
        unsigned offset = 0;
        
        std::apply([&](auto &&...f) {
            auto extract_field = [&](const auto &field_info) {
                using FieldType = std::remove_cvref_t<decltype(bundle.*(field_info.ptr))>;
                
                // 使用正确的语法提取指定范围的位
                constexpr unsigned field_width = get_field_width<FieldType>();
                auto field_bits = bits(bits, offset, field_width);
                
                if constexpr (requires { FieldType::from_bits(field_bits); }) {
                    // 如果字段类型有from_bits静态方法
                    bundle.*(field_info.ptr) = FieldType::from_bits(field_bits);
                } else if constexpr (std::is_same_v<FieldType, ch_bool>) {
                    // 特殊处理ch_bool类型
                    bundle.*(field_info.ptr) = static_cast<bool>(field_bits);
                } else {
                    // 直接赋值
                    bundle.*(field_info.ptr) = field_bits;
                }
                
                offset += field_info.width;
            };
            
            (extract_field(f), ...);
        }, fields);
    }
}

// 从位向量反序列化到Bundle
template <typename BundleT, unsigned W>
BundleT deserialize(const ch_uint<W> &bits) {
    if constexpr (is_bundle_v<BundleT>) {
        BundleT bundle;
        deserialize_bits_to_fields(bits, bundle);
        return bundle;
    } else {
        static_assert(is_bundle_v<BundleT>, "deserialize() only works with Bundle types");
        return BundleT{};  // 返回默认值
    }
}

} // namespace ch::core

#endif // CH_CORE_BUNDLE_SERIALIZATION_H