#ifndef CH_CORE_BUNDLE_META_H
#define CH_CORE_BUNDLE_META_H

#include <string_view>

namespace ch::core {

template <typename BundleT, typename FieldType> struct bundle_field {
    std::string_view name;
    FieldType BundleT::*ptr;
};

} // namespace ch::core

// 字段条目宏：需要显式传入类型 T 和字段名
#define CH_BUNDLE_FIELD_ENTRY(T, name)                                         \
    ::ch::core::bundle_field<T, decltype(T::name)> { #name, &T::name }

// 支持 1~6 个字段的宏（带类型 T）
#define CH_BUNDLE_FIELDS_1(T, a) CH_BUNDLE_FIELD_ENTRY(T, a)
#define CH_BUNDLE_FIELDS_2(T, a, b)                                            \
    CH_BUNDLE_FIELD_ENTRY(T, a), CH_BUNDLE_FIELD_ENTRY(T, b)
#define CH_BUNDLE_FIELDS_3(T, a, b, c)                                         \
    CH_BUNDLE_FIELD_ENTRY(T, a), CH_BUNDLE_FIELD_ENTRY(T, b),                  \
        CH_BUNDLE_FIELD_ENTRY(T, c)
#define CH_BUNDLE_FIELDS_4(T, a, b, c, d)                                      \
    CH_BUNDLE_FIELD_ENTRY(T, a), CH_BUNDLE_FIELD_ENTRY(T, b),                  \
        CH_BUNDLE_FIELD_ENTRY(T, c), CH_BUNDLE_FIELD_ENTRY(T, d)
#define CH_BUNDLE_FIELDS_5(T, a, b, c, d, e)                                   \
    CH_BUNDLE_FIELD_ENTRY(T, a), CH_BUNDLE_FIELD_ENTRY(T, b),                  \
        CH_BUNDLE_FIELD_ENTRY(T, c), CH_BUNDLE_FIELD_ENTRY(T, d),              \
        CH_BUNDLE_FIELD_ENTRY(T, e)
#define CH_BUNDLE_FIELDS_6(T, a, b, c, d, e, f)                                \
    CH_BUNDLE_FIELD_ENTRY(T, a), CH_BUNDLE_FIELD_ENTRY(T, b),                  \
        CH_BUNDLE_FIELD_ENTRY(T, c), CH_BUNDLE_FIELD_ENTRY(T, d),              \
        CH_BUNDLE_FIELD_ENTRY(T, e), CH_BUNDLE_FIELD_ENTRY(T, f)

// 选择合适的宏（根据参数数量）
#define CH_GET_NTH_ARG(_1, _2, _3, _4, _5, _6, N, ...) N
#define CH_BUNDLE_FIELDS_CHOOSE_N(...)                                         \
    CH_GET_NTH_ARG(__VA_ARGS__, CH_BUNDLE_FIELDS_6, CH_BUNDLE_FIELDS_5,        \
                   CH_BUNDLE_FIELDS_4, CH_BUNDLE_FIELDS_3, CH_BUNDLE_FIELDS_2, \
                   CH_BUNDLE_FIELDS_1, )

// 主宏：生成 static constexpr __bundle_fields()
#define CH_BUNDLE_FIELDS(T, ...)                                               \
    static constexpr auto __bundle_fields() {                                  \
        return std::make_tuple(                                                \
            CH_BUNDLE_FIELDS_CHOOSE_N(__VA_ARGS__)(T, __VA_ARGS__));           \
    }

// 用于模板Bundle的字段定义宏
#define CH_BUNDLE_FIELD_ENTRY_T(name)                                          \
    ::ch::core::bundle_field<Self, decltype(Self::name)> { #name, &Self::name }

// 支持 1~6 个字段的模板宏
#define CH_BUNDLE_FIELDS_T_1(a) CH_BUNDLE_FIELD_ENTRY_T(a)
#define CH_BUNDLE_FIELDS_T_2(a, b)                                             \
    CH_BUNDLE_FIELD_ENTRY_T(a), CH_BUNDLE_FIELD_ENTRY_T(b)
#define CH_BUNDLE_FIELDS_T_3(a, b, c)                                          \
    CH_BUNDLE_FIELD_ENTRY_T(a), CH_BUNDLE_FIELD_ENTRY_T(b),                    \
        CH_BUNDLE_FIELD_ENTRY_T(c)
#define CH_BUNDLE_FIELDS_T_4(a, b, c, d)                                       \
    CH_BUNDLE_FIELD_ENTRY_T(a), CH_BUNDLE_FIELD_ENTRY_T(b),                    \
        CH_BUNDLE_FIELD_ENTRY_T(c), CH_BUNDLE_FIELD_ENTRY_T(d)
#define CH_BUNDLE_FIELDS_T_5(a, b, c, d, e)                                    \
    CH_BUNDLE_FIELD_ENTRY_T(a), CH_BUNDLE_FIELD_ENTRY_T(b),                    \
        CH_BUNDLE_FIELD_ENTRY_T(c), CH_BUNDLE_FIELD_ENTRY_T(d),                \
        CH_BUNDLE_FIELD_ENTRY_T(e)
#define CH_BUNDLE_FIELDS_T_6(a, b, c, d, e, f)                                 \
    CH_BUNDLE_FIELD_ENTRY_T(a), CH_BUNDLE_FIELD_ENTRY_T(b),                    \
        CH_BUNDLE_FIELD_ENTRY_T(c), CH_BUNDLE_FIELD_ENTRY_T(d),                \
        CH_BUNDLE_FIELD_ENTRY_T(e), CH_BUNDLE_FIELD_ENTRY_T(f)

// 模板Bundle的主宏：生成 static constexpr __bundle_fields()
#define CH_BUNDLE_FIELDS_T(...)                                                \
    using bundle_base<Self>::bundle_base;                                      \
    static constexpr auto __bundle_fields() {                                  \
        return std::make_tuple(CH_GET_NTH_ARG(                                 \
            __VA_ARGS__, CH_BUNDLE_FIELDS_T_6, CH_BUNDLE_FIELDS_T_5,           \
            CH_BUNDLE_FIELDS_T_4, CH_BUNDLE_FIELDS_T_3, CH_BUNDLE_FIELDS_T_2,  \
            CH_BUNDLE_FIELDS_T_1, )(__VA_ARGS__));                             \
    }

#endif // CH_CORE_BUNDLE_META_H