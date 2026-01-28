// include/bundle/axi_protocol.h
#ifndef CH_BUNDLE_AXI_PROTOCOL_H
#define CH_BUNDLE_AXI_PROTOCOL_H

#include "core/bundle/bundle_traits.h"
#include "core/bundle/bundle_protocol.h"
#include <type_traits>

namespace ch::core {

// AXI-Lite写协议检查
template <typename T> struct is_axi_lite_write {
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

template <typename T>
constexpr bool is_axi_lite_write_v = is_axi_lite_write<T>::value;

// AXI-Lite读协议检查
template <typename T> struct is_axi_lite_read {
    static constexpr bool value = []() constexpr {
        if constexpr (is_bundle_v<T>) {
            return has_field_named_v<T, structural_string{"ar"}> &&
                   has_field_named_v<T, structural_string{"r"}>;
        } else {
            return false;
        }
    }();
};

template <typename T>
constexpr bool is_axi_lite_read_v = is_axi_lite_read<T>::value;

// AXI-Lite完整协议检查 - 修复原始错误
template <typename T> struct is_axi_lite {
private:
    // 检查嵌套字段的辅助函数
    template<typename BundleType>
    static constexpr bool check_nested_axi_fields() {
        // 获取write和read字段的类型
        using WriteType = get_field_type_t<BundleType, structural_string{"write"}>;
        using ReadType = get_field_type_t<BundleType, structural_string{"read"}>;
        
        // 检查write子结构是否包含必要的字段
        constexpr bool has_write_fields = is_bundle_v<WriteType> &&
            has_field_named_v<WriteType, structural_string{"aw"}> &&
            has_field_named_v<WriteType, structural_string{"w"}> &&
            has_field_named_v<WriteType, structural_string{"b"}>;
            
        // 检查read子结构是否包含必要的字段
        constexpr bool has_read_fields = is_bundle_v<ReadType> &&
            has_field_named_v<ReadType, structural_string{"ar"}> &&
            has_field_named_v<ReadType, structural_string{"r"}>;
            
        return has_write_fields && has_read_fields;
    }

public:
    static constexpr bool value = []() constexpr {
        if constexpr (is_bundle_v<T>) {
            // 检查是否是完整的axi_lite_bundle（具有write和read字段）
            if constexpr (has_field_named_v<T, structural_string{"write"}> &&
                          has_field_named_v<T, structural_string{"read"}>) {
                // 使用更可靠的方法检查嵌套字段
                return check_nested_axi_fields<T>();
            } else {
                // 否则直接检查字段是否存在
                return has_field_named_v<T, structural_string{"aw"}> &&
                       has_field_named_v<T, structural_string{"w"}> &&
                       has_field_named_v<T, structural_string{"b"}> &&
                       has_field_named_v<T, structural_string{"ar"}> &&
                       has_field_named_v<T, structural_string{"r"}>;
            }
        } else {
            return false;
        }
    }();
};

template <typename T> 
constexpr bool is_axi_lite_v = is_axi_lite<T>::value;

// AXI协议验证函数
template <typename BundleT>
void validate_axi_lite_protocol(const BundleT &bundle) {
    static_assert(is_axi_lite_v<BundleT>,
                  "Bundle does not conform to AXI-Lite protocol");
    (void)bundle; // 避免未使用警告
}

template <typename BundleT>
void validate_axi_lite_write_protocol(const BundleT &bundle) {
    static_assert(is_axi_lite_write_v<BundleT>,
                  "Bundle does not conform to AXI-Lite write protocol");
    (void)bundle; // 避免未使用警告
}

template <typename BundleT>
void validate_axi_lite_read_protocol(const BundleT &bundle) {
    static_assert(is_axi_lite_read_v<BundleT>,
                  "Bundle does not conform to AXI-Lite read protocol");
    (void)bundle; // 避免未使用警告
}

} // namespace ch::core

#endif // CH_CORE_AXI_PROTOCOL_H