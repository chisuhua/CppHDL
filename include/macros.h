// include/macros.h
#ifndef MACROS_H
#define MACROS_H

// SpinalHDL-style factory functions (recommended over macros)
namespace ch {
    template<typename T> auto in(const T& = T{}) { return ch::core::ch_in<T>{}; }
    template<typename T> auto out(const T& = T{}) { return ch::core::ch_out<T>{}; }
}

// Keep old macros if needed
#define __in(...)   ch::core::ch_in<__VA_ARGS__>
#define __out(...)  ch::core::ch_out<__VA_ARGS__>

// include/macros.h
#define CH_IO(...) \
    struct { __VA_ARGS__ } io; \
    void __register_ports() { \
        /* 自动生成端口创建代码 */ \
        /* 例如： */ \
        if constexpr (requires { io.valid; }) { \
            if constexpr (std::is_same_v<decltype(io.valid), ch::core::ch_out<ch_uint<1>>>) { \
                auto* node = context()->create_output(1, "io_valid"); \
                io.valid.bind(node); \
            } \
        } \
        /* ... 递归处理其他字段 ... */ \
    }

#endif // MACROS_H
