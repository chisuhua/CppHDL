#ifndef CH_CORE_BUNDLE_META_H
#define CH_CORE_BUNDLE_META_H

#include <string_view>

namespace ch::core {

template <typename BundleT, typename FieldType> struct bundle_field {
    std::string_view name;
    FieldType BundleT::*ptr;
};

} // namespace ch::core

// 辅助宏：选择适当的宏（根据参数数量）
#define CH_GET_NTH_ARG(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, N, ...) N

// 用于模板Bundle的字段定义宏
#define CH_BUNDLE_FIELD_ENTRY_T(name)                                          \
    ::ch::core::bundle_field<Self, decltype(Self::name)> { #name, &Self::name }

// 支持 1~10 个字段的模板宏
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
#define CH_BUNDLE_FIELDS_T_7(a, b, c, d, e, f, g)                              \
    CH_BUNDLE_FIELD_ENTRY_T(a), CH_BUNDLE_FIELD_ENTRY_T(b),                    \
        CH_BUNDLE_FIELD_ENTRY_T(c), CH_BUNDLE_FIELD_ENTRY_T(d),                \
        CH_BUNDLE_FIELD_ENTRY_T(e), CH_BUNDLE_FIELD_ENTRY_T(f),                \
        CH_BUNDLE_FIELD_ENTRY_T(g)
#define CH_BUNDLE_FIELDS_T_8(a, b, c, d, e, f, g, h)                           \
    CH_BUNDLE_FIELD_ENTRY_T(a), CH_BUNDLE_FIELD_ENTRY_T(b),                    \
        CH_BUNDLE_FIELD_ENTRY_T(c), CH_BUNDLE_FIELD_ENTRY_T(d),                \
        CH_BUNDLE_FIELD_ENTRY_T(e), CH_BUNDLE_FIELD_ENTRY_T(f),                \
        CH_BUNDLE_FIELD_ENTRY_T(g), CH_BUNDLE_FIELD_ENTRY_T(h)
#define CH_BUNDLE_FIELDS_T_9(a, b, c, d, e, f, g, h, i)                        \
    CH_BUNDLE_FIELD_ENTRY_T(a), CH_BUNDLE_FIELD_ENTRY_T(b),                    \
        CH_BUNDLE_FIELD_ENTRY_T(c), CH_BUNDLE_FIELD_ENTRY_T(d),                \
        CH_BUNDLE_FIELD_ENTRY_T(e), CH_BUNDLE_FIELD_ENTRY_T(f),                \
        CH_BUNDLE_FIELD_ENTRY_T(g), CH_BUNDLE_FIELD_ENTRY_T(h),                \
        CH_BUNDLE_FIELD_ENTRY_T(i)
#define CH_BUNDLE_FIELDS_T_10(a, b, c, d, e, f, g, h, i, j)                    \
    CH_BUNDLE_FIELD_ENTRY_T(a), CH_BUNDLE_FIELD_ENTRY_T(b),                    \
        CH_BUNDLE_FIELD_ENTRY_T(c), CH_BUNDLE_FIELD_ENTRY_T(d),                \
        CH_BUNDLE_FIELD_ENTRY_T(e), CH_BUNDLE_FIELD_ENTRY_T(f),                \
        CH_BUNDLE_FIELD_ENTRY_T(g), CH_BUNDLE_FIELD_ENTRY_T(h),                \
        CH_BUNDLE_FIELD_ENTRY_T(i), CH_BUNDLE_FIELD_ENTRY_T(j)

// 模板Bundle的主宏：生成 static constexpr __bundle_fields()
#define CH_BUNDLE_FIELDS_T(...)                                                \
    using bundle_base<Self>::bundle_base;                                      \
    static constexpr auto __bundle_fields() {                                  \
        return std::make_tuple(CH_GET_NTH_ARG(                                 \
            __VA_ARGS__, CH_BUNDLE_FIELDS_T_10, CH_BUNDLE_FIELDS_T_9,          \
            CH_BUNDLE_FIELDS_T_8, CH_BUNDLE_FIELDS_T_7, CH_BUNDLE_FIELDS_T_6,  \
            CH_BUNDLE_FIELDS_T_5, CH_BUNDLE_FIELDS_T_4, CH_BUNDLE_FIELDS_T_3,  \
            CH_BUNDLE_FIELDS_T_2, CH_BUNDLE_FIELDS_T_1, )(__VA_ARGS__));       \
    }

#endif // CH_CORE_BUNDLE_META_H