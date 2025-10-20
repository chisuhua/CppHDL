// include/core/bundle_layout.h
#ifndef CH_CORE_BUNDLE_LAYOUT_H
#define CH_CORE_BUNDLE_LAYOUT_H

#include "bundle_traits.h"
#include "bundle_meta.h"
#include <tuple>
#include <array>

namespace ch::core {

// 增强的bundle_field结构，包含布局信息
template<typename BundleT, typename FieldType>
struct bundle_field_with_layout {
    std::string_view name;
    FieldType BundleT::* ptr;
    unsigned offset;  // 字段在Bundle中的位偏移
    unsigned width;   // 字段宽度
};

// 字段布局信息
template<typename BundleT>
struct bundle_layout {
    static constexpr auto get_layout_info() {
        if constexpr (is_bundle_v<BundleT>) {
            constexpr auto fields = BundleT::__bundle_fields();
            return calculate_layout_with_offsets<BundleT>(fields);
        } else {
            return std::make_tuple();
        }
    }
    
    template<typename T>
    static constexpr unsigned get_field_width() {
        if constexpr (requires { T::ch_width; }) {
            return T::ch_width;
        } else if constexpr (requires { T::width; }) {
            return T::width;
        } else if constexpr (std::is_same_v<T, ch_bool>) {
            return 1;
        } else if constexpr (is_bundle_v<T>) {
            return bundle_width_v<T>;
        } else {
            return 0;
        }
    }
    
private:
    template<typename T, typename FieldTuple>
    static constexpr auto calculate_layout_with_offsets(FieldTuple fields) {
        constexpr auto indices = std::make_index_sequence<std::tuple_size_v<FieldTuple>>{};
        unsigned current_offset = 0;
        
        return calculate_field_layout<T>(fields, indices, current_offset);
    }
   
    // 改 2：捕获 fields（按值）
    template<typename T, typename FieldTuple, std::size_t... I>
    static constexpr auto calculate_field_layout(FieldTuple fields, 
                                                std::index_sequence<I...>, 
                                                unsigned& current_offset) {
        return std::make_tuple(
            [&current_offset, fields]() constexpr {
                using FieldType = std::remove_cvref_t<decltype(T{}.*(std::get<I>(fields).ptr))>;
                constexpr unsigned field_width = get_field_width<FieldType>();
                auto result = bundle_field_with_layout<T, FieldType>{
                    std::get<I>(fields).name,
                    std::get<I>(fields).ptr,
                    current_offset,
                    field_width
                };
                current_offset += field_width;
                return result;
            }()...
        );
    }
};

// 获取Bundle布局信息的便捷函数
template<typename BundleT>
constexpr auto get_bundle_layout() {
    return bundle_layout<BundleT>::get_layout_info();
}

// Bundle字段偏移查找
template<typename BundleT, const char* FieldName>
constexpr unsigned get_field_offset() {
    constexpr auto layout = get_bundle_layout<BundleT>();
    constexpr auto indices = std::make_index_sequence<std::tuple_size_v<decltype(layout)>>{};
    
    return find_field_offset(layout, indices, FieldName);
}

template<typename LayoutTuple, std::size_t... I, const char* FieldName>
constexpr unsigned find_field_offset(const LayoutTuple& layout, 
                                    std::index_sequence<I...>, 
                                    const char* field_name) {
    unsigned offset = 0;
    bool found = ((std::string_view(std::get<I>(layout).name) == std::string_view(field_name) 
                   ? (offset = std::get<I>(layout).offset, true) : false) || ...);
    return found ? offset : static_cast<unsigned>(-1); // 无效偏移
}

} // namespace ch::core

#endif // CH_CORE_BUNDLE_LAYOUT_H
