// include/core/axi_protocol.h
#ifndef CH_CORE_AXI_PROTOCOL_H
#define CH_CORE_AXI_PROTOCOL_H

#include "core/bundle/bundle_traits.h"
#include "core/bundle/bundle_meta.h"
#include "core/bundle/bundle_protocol.h"
#include <type_traits>

namespace ch::core {

// AXI-Lite写协议检查
template<typename T>
struct is_axi_lite_write {
    static constexpr bool value = []() constexpr {
        if constexpr (is_bundle_v<T>) {
            return has_field_named_v<T, structural_string{"aw"}> && 
                   has_field_named_v<T, structural_string{"w"}> && 
                   has_field_named_v<T, structural_string{"b"}>;
        } else {
            return false;
        }
    }();
};

template<typename T>
constexpr bool is_axi_lite_write_v = is_axi_lite_write<T>::value;

// AXI-Lite读协议检查
template<typename T>
struct is_axi_lite_read {
    static constexpr bool value = []() constexpr {
        if constexpr (is_bundle_v<T>) {
            return has_field_named_v<T, structural_string{"ar"}> && 
                   has_field_named_v<T, structural_string{"r"}>;
        } else {
            return false;
        }
    }();
};

template<typename T>
constexpr bool is_axi_lite_read_v = is_axi_lite_read<T>::value;

// AXI-Lite完整协议检查
template<typename T>
struct is_axi_lite {
    static constexpr bool value = []() constexpr {
        if constexpr (is_bundle_v<T>) {
            return has_field_named_v<T, structural_string{"aw"}> && 
                   has_field_named_v<T, structural_string{"w"}> && 
                   has_field_named_v<T, structural_string{"b"}> &&
                   has_field_named_v<T, structural_string{"ar"}> && 
                   has_field_named_v<T, structural_string{"r"}>;
        } else {
            return false;
        }
    }();
};

template<typename T>
constexpr bool is_axi_lite_v = is_axi_lite<T>::value;

// AXI协议验证函数
template<typename BundleT>
void validate_axi_lite_protocol(const BundleT& bundle) {
    static_assert(is_axi_lite_v<BundleT>, 
                  "Bundle does not conform to AXI-Lite protocol");
    (void)bundle; // 避免未使用警告
}

template<typename BundleT>
void validate_axi_lite_write_protocol(const BundleT& bundle) {
    static_assert(is_axi_lite_write_v<BundleT>, 
                  "Bundle does not conform to AXI-Lite write protocol");
    (void)bundle; // 避免未使用警告
}

template<typename BundleT>
void validate_axi_lite_read_protocol(const BundleT& bundle) {
    static_assert(is_axi_lite_read_v<BundleT>, 
                  "Bundle does not conform to AXI-Lite read protocol");
    (void)bundle; // 避免未使用警告
}

} // namespace ch::core

#endif // CH_CORE_AXI_PROTOCOL_H
