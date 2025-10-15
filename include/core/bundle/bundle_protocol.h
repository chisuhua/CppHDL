// include/core/bundle_protocol.h
#ifndef CH_CORE_BUNDLE_PROTOCOL_H
#define CH_CORE_BUNDLE_PROTOCOL_H

#include "bundle_meta.h"
#include <type_traits>
#include <string_view>
#include <tuple>

namespace ch::core {

// 概念：类型 T 必须提供 static constexpr __bundle_fields()
template<typename T>
concept HasField = requires {
    T::__bundle_fields();
};

// Helper: 检查字段元组中是否包含指定名称的字段
template<typename FieldTuple, std::size_t... I>
constexpr bool tuple_has_field_named(const FieldTuple& fields, std::index_sequence<I...>, const char* name) {
    return ((std::string_view{std::get<I>(fields).name} == name) || ...);
}

// Handshake 协议检测：必须包含 "payload", "valid", "ready"
template<typename T>
struct is_handshake_protocol {
    static constexpr bool value = []() constexpr {
        if constexpr (!HasField<T>) {
            return false;
        } else {
            constexpr auto fields = T::__bundle_fields();
            constexpr auto indices = std::make_index_sequence<std::tuple_size_v<decltype(fields)>>{};
            return tuple_has_field_named(fields, indices, "payload") &&
                   tuple_has_field_named(fields, indices, "valid") &&
                   tuple_has_field_named(fields, indices, "ready");
        }
    }();
};

template<typename T>
inline constexpr bool is_handshake_protocol_v = is_handshake_protocol<T>::value;

// AXI 协议检测：包含任意一个 AXI 信号字段即视为 AXI（可按需调整为“必须包含地址+数据”等）
template<typename T>
struct is_axi_protocol {
    static constexpr bool value = []() constexpr {
        if constexpr (!HasField<T>) {
            return false;
        } else {
            constexpr auto fields = T::__bundle_fields();
            constexpr auto indices = std::make_index_sequence<std::tuple_size_v<decltype(fields)>>{};
            return tuple_has_field_named(fields, indices, "awaddr") ||
                   tuple_has_field_named(fields, indices, "araddr") ||
                   tuple_has_field_named(fields, indices, "wdata") ||
                   tuple_has_field_named(fields, indices, "rdata");
        }
    }();
};

template<typename T>
inline constexpr bool is_axi_protocol_v = is_axi_protocol<T>::value;

// 编译期验证函数（可用于模板约束或显式检查）
template<typename BundleT>
constexpr void validate_handshake_protocol() {
    static_assert(is_handshake_protocol_v<BundleT>,
                  "Bundle does not conform to Handshake protocol (missing 'payload', 'valid', or 'ready')");
}

template<typename BundleT>
constexpr void validate_axi_protocol() {
    static_assert(is_axi_protocol_v<BundleT>,
                  "Bundle does not conform to AXI protocol (missing AXI signal fields like 'awaddr', 'wdata', etc.)");
}

template<std::size_t N>
struct structural_string {
    const char data[N];

    // 辅助构造函数：通过索引序列展开初始化
    template<std::size_t... I>
    constexpr structural_string(const char (&str)[N], std::index_sequence<I...>) 
        : data{str[I]...} 
    {}

    // 主构造函数
    constexpr structural_string(const char (&str)[N])
        : structural_string(str, std::make_index_sequence<N>{})
    {}

    constexpr const char* c_str() const noexcept { return data; }
    constexpr std::size_t size() const noexcept { return N - 1; }

    constexpr operator std::string_view() const noexcept {
        return std::string_view(data, N - 1);
    }
};

template<std::size_t N>
structural_string(const char (&)[N]) -> structural_string<N>;

// --- has_field_named using auto NTTP ---
template<typename T, auto Name>
struct has_field_named {
    static constexpr bool value = []() constexpr {
        if constexpr (!HasField<T>) {
            return false;
        } else {
            constexpr auto fields = T::__bundle_fields();
            constexpr auto indices = std::make_index_sequence<std::tuple_size_v<decltype(fields)>>();
            constexpr std::string_view target{Name.c_str(), Name.size()};
            return tuple_has_field_named(fields, indices, target.data());
        }
    }();
};

template<typename T, auto Name>
inline constexpr bool has_field_named_v = has_field_named<T, Name>::value;
} // namespace ch::core

#endif // CH_CORE_BUNDLE_PROTOCOL_H
