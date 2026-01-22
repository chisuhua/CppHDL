// include/core/bundle_traits.h (更新版本)
#ifndef CH_CORE_BUNDLE_TRAITS_H
#define CH_CORE_BUNDLE_TRAITS_H

#include "bundle_meta.h"
#include <iostream>
#include <type_traits>
#include <typeinfo>

namespace ch::core {
struct ch_bool;
}

namespace ch::core {

// 检查是否为Bundle类型的特征
template <typename T> struct is_bundle {
    template <typename U>
    static auto test(int) -> decltype(std::declval<U>().__bundle_fields(),
                                      std::declval<U>().is_valid(),
                                      std::true_type{});

    template <typename> static std::false_type test(...);

public:
    static constexpr bool value = decltype(test<T>(0))::value;
};

template <typename T> constexpr bool is_bundle_v = is_bundle<T>::value;

// 获取Bundle字段数量
template <typename BundleT> struct bundle_field_count {
    static constexpr size_t value = []() {
        if constexpr (is_bundle_v<BundleT>) {
            return std::tuple_size_v<
                decltype(std::declval<BundleT>().__bundle_fields())>;
        } else {
            return 0;
        }
    }();
};

template <typename BundleT>
constexpr size_t bundle_field_count_v = bundle_field_count<BundleT>::value;

// 计算Bundle总宽度
template <typename BundleT> constexpr unsigned get_bundle_width() {
    if constexpr (is_bundle_v<BundleT>) {
        return []() constexpr {
            constexpr auto fields = BundleT::__bundle_fields();
            constexpr auto indices =
                std::make_index_sequence<std::tuple_size_v<decltype(fields)>>{};
            unsigned total_width = 0;

            [&]<std::size_t... I>(std::index_sequence<I...>) {
                constexpr auto field_tuple = BundleT::__bundle_fields();
                (([&]() {
                     if constexpr (I <
                                   std::tuple_size_v<decltype(field_tuple)>) {
                         using FieldType = std::remove_cvref_t<
                             decltype(std::declval<BundleT>().*
                                      (std::get<I>(field_tuple).ptr))>;
                         if constexpr (requires { FieldType::ch_width; }) {
                             total_width += FieldType::ch_width;
                         } else if constexpr (requires { FieldType::width; }) {
                             total_width += FieldType::width;
                         } else if constexpr (std::is_same_v<FieldType,
                                                             ch_bool>) {
                             total_width += 1u;
                         } else if constexpr (is_bundle_v<FieldType> &&
                                              !std::is_same_v<BundleT,
                                                              FieldType>) {
                             total_width += get_bundle_width<FieldType>();
                         }
                     }
                 }()),
                 ...);
            }(indices);

            return total_width;
        }();
    } else {
        // is_bundle<BundleT>::debug_print(); // 已移除
        // is_bundle<BundleT>::print_type; // 保持编译时检查（可按需注释）
        return 0;
    }
}

// Bundle宽度的便捷模板变量
template <typename BundleT>
constexpr unsigned bundle_width_v = get_bundle_width<BundleT>();

// 获取字段宽度的辅助函数
template <typename T> constexpr unsigned get_field_width() {
    if constexpr (requires { T::ch_width; }) {
        return T::ch_width;
    } else if constexpr (requires { T::width; }) {
        return T::width;
    } else if constexpr (std::is_same_v<T, ch_bool>) {
        return 1;
    } else if constexpr (is_bundle_v<T>) {
        return get_bundle_width<T>();
    } else {
        static_assert(sizeof(T) == 0,
                      "Unsupported field type for width calculation");
        return 0;
    }
}

// 为Bundle字段添加特殊处理
template <typename Derived> struct bundle_field_traits {
    static constexpr bool is_bundle = false;
};

// 特化：识别Bundle类型
template <typename T>
concept is_bundle_type = requires(T t) {
    t.__bundle_fields();
    t.is_valid();
    t.as_master();
    t.as_slave();
};

// 为Bundle类型提供特化
template <typename BundleType>
    requires is_bundle_type<BundleType>
struct bundle_field_traits<BundleType> {
    static constexpr bool is_bundle = true;

    // 递归验证嵌套Bundle
    static bool check_valid(const BundleType &bundle) {
        return bundle.is_valid();
    }

    // 递归命名嵌套Bundle
    static void set_name(BundleType &bundle, const std::string &name) {
        bundle.set_name_prefix(name);
    }

    // 递归连接嵌套Bundle
    template <typename SrcBundleType>
    static void connect(SrcBundleType &&src, BundleType &dst) {
        const auto &src_fields = src.__bundle_fields();
        const auto &dst_fields = dst.__bundle_fields();

        // 连接对应的字段
        connect_fields(src, dst, src_fields, dst_fields);
    }

private:
    template <typename SrcBundle, typename DstBundle, typename... SrcFieldInfo,
              typename... DstFieldInfo>
    static void connect_fields(SrcBundle &&src, DstBundle &dst,
                               std::tuple<SrcFieldInfo...> src_fields,
                               std::tuple<DstFieldInfo...> dst_fields) {
        // 确保字段数量相同
        static_assert(std::tuple_size_v<decltype(src_fields)> ==
                      std::tuple_size_v<decltype(dst_fields)>);

        // 使用索引序列展开元组
        connect_fields_impl(std::forward<SrcBundle>(src), dst, src_fields,
                            dst_fields,
                            std::make_index_sequence<
                                std::tuple_size_v<decltype(src_fields)>>{});
    }

    template <typename SrcBundle, typename DstBundle, typename... SrcFieldInfo,
              typename... DstFieldInfo, size_t... Is>
    static void connect_fields_impl(SrcBundle &&src, DstBundle &dst,
                                    std::tuple<SrcFieldInfo...> src_fields,
                                    std::tuple<DstFieldInfo...> dst_fields,
                                    std::index_sequence<Is...>) {
        // 连接每个字段
        ((connect_single_field(src.*(std::get<Is>(src_fields).ptr),
                               dst.*(std::get<Is>(dst_fields).ptr))),
         ...);
    }

    template <typename SrcField, typename DstField>
    static void connect_single_field(SrcField &&src_field,
                                     DstField &dst_field) {
        if constexpr (is_bundle_v<std::decay_t<SrcField>>) {
            // 如果源字段是Bundle，递归连接
            bundle_field_traits<std::decay_t<SrcField>>::connect(
                std::forward<SrcField>(src_field), dst_field);
        } else {
            // 否则直接连接
            dst_field = std::forward<SrcField>(src_field);
        }
    }
};

} // namespace ch::core

#endif // CH_CORE_BUNDLE_TRAITS_H