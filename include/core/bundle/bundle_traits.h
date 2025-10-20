// include/core/bundle_traits.h (更新版本)
#ifndef CH_CORE_BUNDLE_TRAITS_H
#define CH_CORE_BUNDLE_TRAITS_H

#include <type_traits>
#include "bundle_meta.h"
#include "bool.h"

namespace ch::core {

// 检查是否为Bundle类型的特征
template<typename T>
struct is_bundle {
    template<typename U>
    static auto test(int) -> decltype(
        std::declval<U>().__bundle_fields(),
        std::declval<U>().is_valid(),
        std::true_type{}
    );
    
    template<typename>
    static std::false_type test(...);
    
    static constexpr bool value = decltype(test<T>(0))::value;
};

template<typename T>
constexpr bool is_bundle_v = is_bundle<T>::value;

// 获取Bundle字段数量
template<typename BundleT>
struct bundle_field_count {
    static constexpr size_t value = []() constexpr {
        if constexpr (is_bundle_v<BundleT>) {
            return std::tuple_size_v<decltype(std::declval<BundleT>().__bundle_fields())>;
        } else {
            return 0;
        }
    }();
};

template<typename BundleT>
constexpr size_t bundle_field_count_v = bundle_field_count<BundleT>::value;

// 计算Bundle总宽度
template<typename BundleT>
constexpr unsigned get_bundle_width() {
    if constexpr (is_bundle_v<BundleT>) {
        constexpr auto fields = BundleT::__bundle_fields();
        constexpr auto indices = std::make_index_sequence<std::tuple_size_v<decltype(fields)>>{};
        unsigned total_width = 0;
        [&]<std::size_t... I>(std::index_sequence<I...>) {
            ((total_width += []() constexpr {
                using FieldType = std::remove_cvref_t<decltype(BundleT{}.*(std::get<I>(fields).ptr))>;
                if constexpr (requires { FieldType::ch_width; }) {
                    return FieldType::ch_width;
                } else if constexpr (requires { FieldType::width; }) {
                    return FieldType::width;
                } else if constexpr (std::is_same_v<FieldType, ch_bool>) {
                    return 1u;
                } else if constexpr (is_bundle_v<FieldType>) {
                    return get_bundle_width<FieldType>();
                } else {
                    return 0u;
                }
            }()), ...);
        }(indices);
        return total_width;
    } else {
        return 0;
    }
}

// Bundle宽度的便捷模板变量
template<typename BundleT>
constexpr unsigned bundle_width_v = get_bundle_width<BundleT>();

// 获取字段宽度的辅助函数
template<typename T>
constexpr unsigned get_field_width() {
    if constexpr (requires { T::ch_width; }) {
        return T::ch_width;
    } else if constexpr (requires { T::width; }) {
        return T::width;
    } else if constexpr (std::is_same_v<T, ch_bool>) {
        return 1;
    } else if constexpr (is_bundle_v<T>) {
        return get_bundle_width<T>();
    } else {
        static_assert(sizeof(T) == 0, "Unsupported field type for width calculation");
        return 0;
    }
}


} // namespace ch::core

#endif // CH_CORE_BUNDLE_TRAITS_H
