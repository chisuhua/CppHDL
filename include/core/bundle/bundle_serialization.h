#ifndef CH_CORE_BUNDLE_SERIALIZATION_H
#define CH_CORE_BUNDLE_SERIALIZATION_H

#include "bool.h"
#include "bundle_layout.h"
#include "bundle_traits.h"
#include "uint.h"
#include <cstring>
#include <tuple>

namespace ch::core {

// 前向声明
template <typename BitVectorT, typename BundleT>
void serialize_fields_to_bits(const BundleT &bundle, BitVectorT &result);

template <typename BundleT, typename BitVectorT>
void deserialize_bits_to_fields(const BitVectorT &bits, BundleT &bundle);

// 序列化Bundle到位向量
template <typename BundleT> auto serialize(const BundleT &bundle) {
    static_assert(is_bundle_v<BundleT>,
                  "serialize() only works with Bundle types");

    constexpr unsigned total_width = bundle_width_v<BundleT>;
    static_assert(total_width > 0, "Bundle has zero width");

    ch_uint<total_width> result;
    serialize_fields_to_bits(result, bundle);

    return result;
}

// 从位向量反序列化到Bundle
template <typename BundleT, unsigned Width>
BundleT deserialize(const ch_uint<Width> &bits) {
    static_assert(is_bundle_v<BundleT>,
                  "deserialize() only works with Bundle types");
    static_assert(Width == bundle_width_v<BundleT>, "Bit width mismatch");

    BundleT bundle;
    deserialize_bits_to_fields(bundle, bits);

    return bundle;
}

// 字段序列化实现
template <typename BitVectorT, typename BundleT>
void serialize_fields_to_bits(const BundleT &bundle, BitVectorT &result) {
    constexpr auto layout = get_bundle_layout<BundleT>();
    constexpr auto indices =
        std::make_index_sequence<std::tuple_size_v<decltype(layout)>>{};

    std::apply(
        [&](auto &&...field_info) {
            ((serialize_single_field(result, bundle, field_info)), ...);
        },
        layout);
}

template <typename BitVectorT, typename BundleT, typename FieldInfo>
void serialize_single_field(BitVectorT &result, const BundleT &bundle,
                            const FieldInfo &field_info) {
    const auto &field_value = bundle.*(field_info.ptr);
    // TODO: 实际的位操作需要访问底层节点
    // 这里简化处理
    (void)result;
    (void)field_value;
    (void)field_info;
}

// 字段反序列化实现
template <typename BundleT, typename BitVectorT>
void deserialize_bits_to_fields(const BitVectorT &bits, BundleT &bundle) {
    constexpr auto layout = get_bundle_layout<BundleT>();
    constexpr auto indices =
        std::make_index_sequence<std::tuple_size_v<decltype(layout)>>{};

    std::apply(
        [&](auto &&...field_info) {
            ((deserialize_single_field(bits, bundle, field_info)), ...);
        },
        layout);
}

template <typename BitVectorT, typename BundleT, typename FieldInfo>
void deserialize_single_field(const BitVectorT &bits, BundleT &bundle,
                              const FieldInfo &field_info) {
    auto &field_value = bundle.*(field_info.ptr);
    // TODO: 实际的位操作需要设置底层节点
    // 这里简化处理
    (void)bits;
    (void)field_value;
    (void)field_info;
}

// 位向量与字节数组转换工具
template <unsigned Width>
void bits_to_bytes(const ch_uint<Width> &bits, unsigned char *bytes,
                   size_t byte_count) {
    static_assert(Width > 0, "Invalid bit width");

    // 简化实现 - 实际需要根据底层表示进行转换
    uint64_t value = static_cast<uint64_t>(bits);
    std::memset(bytes, 0, byte_count);
    std::memcpy(bytes, &value, std::min(byte_count, sizeof(value)));
}

template <unsigned Width>
ch_uint<Width> bytes_to_bits(const unsigned char *bytes, size_t byte_count) {
    // 简化实现
    uint64_t value = 0;
    std::memcpy(&value, bytes, std::min(byte_count, sizeof(value)));
    return ch_uint<Width>(value);
}

// Bundle到位向量的视图
template <typename BundleT> struct bundle_bits_view {
    BundleT &bundle;
    static constexpr unsigned width = bundle_width_v<BundleT>;

    explicit bundle_bits_view(BundleT &b) : bundle(b) {}

    template <unsigned W = bundle_width_v<BundleT>>
    operator ch_uint<W>() const {
        return serialize(bundle);
    }
};

// Alias模板以保持向后兼容性
template <typename BundleT> using bundle_to_bits = bundle_bits_view<BundleT>;

// 工厂函数
template <typename BundleT> auto to_bits(BundleT &bundle) {
    return bundle_bits_view<BundleT>(bundle);
}

// 兼容性工厂函数
template <typename BundleT> auto to_bits_view(BundleT &bundle) {
    return bundle_bits_view<BundleT>(bundle);
}

template <typename BundleT, unsigned Width>
void from_bits(const ch_uint<Width> &bits, BundleT &bundle) {
    static_assert(Width == bundle_width_v<BundleT>, "Width mismatch");
    deserialize_bits_to_fields(bundle, bits);
}

} // namespace ch::core

#endif // CH_CORE_BUNDLE_SERIALIZATION_H
