// include/macros.h
#ifndef MACROS_H
#define MACROS_H

#include <new>
#include <type_traits>
#include <functional>
#include <cstdint>
#include <cassert>
#include <iostream>
#include <sstream>
#include <string>
#include <source_location>

namespace ch {

// ===========================================================================
// 日志级别定义
// ===========================================================================

enum class log_level {
    debug = 0,
    info = 1,
    warning = 2,
    error = 3,
    fatal = 4
};

// ===========================================================================
// 统一的日志输出宏
// ===========================================================================

// 基础日志宏
#define LOG(level, msg) \
    do { \
        auto loc = std::source_location::current(); \
        ch::detail::log_message(level, msg, loc); \
    } while(0)

// 便捷日志宏
#define DBG(msg) LOG(ch::log_level::debug, msg)
#define INFO(msg) LOG(ch::log_level::info, msg)
#define WARN(msg) LOG(ch::log_level::warning, msg)
#define ERROR(msg) LOG(ch::log_level::error, msg)
#define FATAL(msg) LOG(ch::log_level::fatal, msg)

// 带变量的日志宏
#define DBG_VAR(var) \
    do { \
        auto loc = std::source_location::current(); \
        ch::detail::log_message(ch::log_level::debug, #var " = " << (var), loc); \
    } while(0)

#define DBG_FUNC() \
    do { \
        auto loc = std::source_location::current(); \
        ch::detail::log_message(ch::log_level::debug, "Entering function: " << loc.function_name(), loc); \
    } while(0)

// ===========================================================================
// 统一的检查宏（不返回错误码）
// ===========================================================================

// 基础检查宏 - 不返回值，只记录错误
#define CHECK(condition, msg) \
    do { \
        if (!(condition)) { \
            ERROR("Check failed [" << #condition << "]: " << msg); \
        } \
    } while(0)

// 语义化检查宏
#define REQUIRE(condition, msg) \
    do { \
        if (!(condition)) { \
            ERROR("Requirement failed [" << #condition << "]: " << msg); \
        } \
    } while(0)

#define ENSURE(condition, msg) \
    do { \
        if (!(condition)) { \
            ERROR("Ensure failed [" << #condition << "]: " << msg); \
        } \
    } while(0)

// 空指针检查专用宏
#define CHECK_NULL(ptr, msg) \
    do { \
        if ((ptr) == nullptr) { \
            ERROR("Null pointer check failed: " << msg); \
        } \
    } while(0)

// ===========================================================================
// 断言宏（调试时启用）
// ===========================================================================

#ifdef NDEBUG
    #define ASSERT(condition, msg) do {} while(0)
    #define INVARIANT(condition, msg) do {} while(0)
#else
    #define ASSERT(condition, msg) \
        do { \
            if (!(condition)) { \
                auto loc = std::source_location::current(); \
                ch::detail::log_message(ch::log_level::error, "Assertion failed [" << #condition << "]: " << msg, loc); \
                assert(condition && msg); \
            } \
        } while(0)
    
    #define INVARIANT(condition, msg) ASSERT(condition, "Invariant violation: " << msg)
#endif

// 致命错误宏
#define ABORT(...) \
    do { \
        auto loc = std::source_location::current(); \
        ch::detail::log_message(ch::log_level::fatal, __VA_ARGS__, loc); \
        std::abort(); \
    } while(false)

// ===========================================================================
// 错误码定义（保留供需要的函数使用）
// ===========================================================================

enum class error_code {
    success = 0,
    null_pointer = 1,
    invalid_argument = 2,
    memory_error = 3,
    context_error = 4,
    node_error = 5,
    simulation_error = 6,
    component_error = 7
};

// ===========================================================================
// IO 宏定义
// ===========================================================================

#define __io(...) \
    struct io_type { __VA_ARGS__; }; \
    alignas(io_type) char io_storage_[sizeof(io_type)]; \
    [[nodiscard]] io_type& io() { return *reinterpret_cast<io_type*>(io_storage_); }

namespace core {
    template<typename T> 
    [[nodiscard]] auto in(const T& = T{}) { return ch::core::ch_in<T>{}; }
    
    template<typename T> 
    [[nodiscard]] auto out(const T& = T{}) { return ch::core::ch_out<T>{}; }
}

#define __in(...)   ch::core::ch_in<__VA_ARGS__>
#define __out(...)  ch::core::ch_out<__VA_ARGS__>

// ===========================================================================
// 详细实现
// ===========================================================================

namespace detail {

// 日志级别到字符串的转换
inline const char* log_level_str(log_level level) {
    switch (level) {
        case log_level::debug:   return "[DEBUG]";
        case log_level::info:    return "[INFO]";
        case log_level::warning: return "[WARNING]";
        case log_level::error:   return "[ERROR]";
        case log_level::fatal:   return "[FATAL]";
        default:                 return "[UNKNOWN]";
    }
}

// 日志输出实现
inline void log_message(log_level level, const auto& message, const std::source_location& loc) {
    // 根据日志级别决定输出流和是否输出
    switch (level) {
        case log_level::debug:
#ifndef CH_DEBUG
            return; // 调试模式下才输出debug信息
#endif
        case log_level::info:
            std::cout << log_level_str(level) << " " << message
                      << " at " << loc.file_name() << ":" << loc.line()
                      << " in " << loc.function_name() << std::endl;
            break;
            
        case log_level::warning:
        case log_level::error:
        case log_level::fatal:
            std::cerr << log_level_str(level) << " " << message
                      << " at " << loc.file_name() << ":" << loc.line()
                      << " in " << loc.function_name() << std::endl;
            break;
    }
}

} // namespace detail

// ===========================================================================
// 杂项工具
// ===========================================================================

template <typename... Args>
void unused([[maybe_unused]] Args&&... args) {}

#define UNUSED(...) ch::unused(__VA_ARGS__)

template <std::unsigned_integral T>
constexpr unsigned bitwidth_v = std::numeric_limits<T>::digits + std::is_signed_v<T>;

// 作用域退出
class scope_exit {
public:
    explicit scope_exit(std::function<void()> func) : func_(std::move(func)) {}
    
    scope_exit(const scope_exit&) = delete;
    scope_exit& operator=(const scope_exit&) = delete;
    scope_exit(scope_exit&&) = default;
    scope_exit& operator=(scope_exit&&) = default;
    
    ~scope_exit() { 
        if (func_) func_(); 
    }
    
    static void* operator new(std::size_t) = delete;
    static void* operator new[](std::size_t) = delete;

private:
    std::function<void()> func_;
};

#define SCOPE_EXIT(func) ch::scope_exit _scope_exit_([&](){ func; })

} // namespace ch

#endif // MACROS_H
