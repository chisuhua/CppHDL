#pragma once

#include <string_view>

namespace ch::core {

template <typename BundleT, typename FieldType> struct bundle_field {
    std::string_view name;
    FieldType BundleT::*ptr;
};

} // namespace ch::core

// 辅助宏：选择适当的宏（根据参数数量）
// 支持 1~20 个字段，N 是第 11+ 个参数（用于 dispatch）
#define CH_GET_NTH_ARG(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, N, ...) N

// 用于模板Bundle的字段定义宏
#define CH_BUNDLE_FIELD_ENTRY_T(name)                                          \
    ::ch::core::bundle_field<Self, decltype(Self::name)> { #name, &Self::name }

// 支持 1~20 个字段的模板宏
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
#define CH_BUNDLE_FIELDS_T_11(a, b, c, d, e, f, g, h, i, j, k)                 \
    CH_BUNDLE_FIELDS_T_10(a, b, c, d, e, f, g, h, i, j),                      \
        CH_BUNDLE_FIELD_ENTRY_T(k)
#define CH_BUNDLE_FIELDS_T_12(a, b, c, d, e, f, g, h, i, j, k, l)              \
    CH_BUNDLE_FIELDS_T_11(a, b, c, d, e, f, g, h, i, j, k),                    \
        CH_BUNDLE_FIELD_ENTRY_T(l)
#define CH_BUNDLE_FIELDS_T_13(a, b, c, d, e, f, g, h, i, j, k, l, m)           \
    CH_BUNDLE_FIELDS_T_12(a, b, c, d, e, f, g, h, i, j, k, l),                \
        CH_BUNDLE_FIELD_ENTRY_T(m)
#define CH_BUNDLE_FIELDS_T_14(a, b, c, d, e, f, g, h, i, j, k, l, m, n)       \
    CH_BUNDLE_FIELDS_T_13(a, b, c, d, e, f, g, h, i, j, k, l, m),              \
        CH_BUNDLE_FIELD_ENTRY_T(n)
#define CH_BUNDLE_FIELDS_T_15(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o)     \
    CH_BUNDLE_FIELDS_T_14(a, b, c, d, e, f, g, h, i, j, k, l, m, n),          \
        CH_BUNDLE_FIELD_ENTRY_T(o)
#define CH_BUNDLE_FIELDS_T_16(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p)  \
    CH_BUNDLE_FIELDS_T_15(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o),       \
        CH_BUNDLE_FIELD_ENTRY_T(p)
#define CH_BUNDLE_FIELDS_T_17(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q) \
    CH_BUNDLE_FIELDS_T_16(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p),    \
        CH_BUNDLE_FIELD_ENTRY_T(q)
#define CH_BUNDLE_FIELDS_T_18(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r) \
    CH_BUNDLE_FIELDS_T_17(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q), \
        CH_BUNDLE_FIELD_ENTRY_T(r)
#define CH_BUNDLE_FIELDS_T_19(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s) \
    CH_BUNDLE_FIELDS_T_18(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r), \
        CH_BUNDLE_FIELD_ENTRY_T(s)
#define CH_BUNDLE_FIELDS_T_20(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t) \
    CH_BUNDLE_FIELDS_T_19(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s), \
        CH_BUNDLE_FIELD_ENTRY_T(t)

// 模板Bundle的主宏：生成 static constexpr __bundle_fields()
#define CH_BUNDLE_FIELDS_T(...)                                                \
    using bundle_base<Self>::bundle_base;                                      \
    static constexpr auto __bundle_fields() {                                  \
        return std::make_tuple(CH_GET_NTH_ARG(                                 \
            __VA_ARGS__, CH_BUNDLE_FIELDS_T_20, CH_BUNDLE_FIELDS_T_19,        \
            CH_BUNDLE_FIELDS_T_18, CH_BUNDLE_FIELDS_T_17,                     \
            CH_BUNDLE_FIELDS_T_16, CH_BUNDLE_FIELDS_T_15,                     \
            CH_BUNDLE_FIELDS_T_14, CH_BUNDLE_FIELDS_T_13,                     \
            CH_BUNDLE_FIELDS_T_12, CH_BUNDLE_FIELDS_T_11,                     \
            CH_BUNDLE_FIELDS_T_10, CH_BUNDLE_FIELDS_T_9,                      \
            CH_BUNDLE_FIELDS_T_8, CH_BUNDLE_FIELDS_T_7,                       \
            CH_BUNDLE_FIELDS_T_6, CH_BUNDLE_FIELDS_T_5,                       \
            CH_BUNDLE_FIELDS_T_4, CH_BUNDLE_FIELDS_T_3,                       \
            CH_BUNDLE_FIELDS_T_2, CH_BUNDLE_FIELDS_T_1, )(__VA_ARGS__));      \
    }