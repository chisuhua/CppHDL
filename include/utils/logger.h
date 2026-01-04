// include/logger.h
#ifndef LOGGER_H
#define LOGGER_H

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <iostream>
#include <memory>
#include <new>
#include <source_location>
#include <sstream>
#include <string>
#include <type_traits>

namespace ch {

// ===========================================================================
// 日志级别定义
// ===========================================================================

enum class log_level { debug = 0, info = 1, warning = 2, error = 3, fatal = 4 };

// ===========================================================================
// printf 风格的字符串格式化工具
// ===========================================================================

namespace detail {
// printf 风格的格式化函数
template <typename... Args>
std::string printf_format(const char *fmt, Args &&...args) {
    // 预估缓冲区大小
    size_t size = std::snprintf(nullptr, 0, fmt, args...) + 1;
    if (size <= 0) {
        return std::string(fmt); // 格式化失败，返回原始格式字符串
    }

    // 分配缓冲区并格式化
    auto buf = std::make_unique<char[]>(size);
    std::snprintf(buf.get(), size, fmt, args...);
    return std::string(buf.get());
}

// 通用的变量到字符串转换
template <typename T> std::string to_string(const T &value) {
    if constexpr (std::is_arithmetic_v<T>) {
        if constexpr (std::is_integral_v<T> && std::is_signed_v<T>) {
            return printf_format("%lld", static_cast<long long>(value));
        } else if constexpr (std::is_integral_v<T> && std::is_unsigned_v<T>) {
            return printf_format("%llu",
                                 static_cast<unsigned long long>(value));
        } else if constexpr (std::is_floating_point_v<T>) {
            return printf_format("%g", static_cast<double>(value));
        } else {
            return printf_format("%lld", static_cast<long long>(value));
        }
    } else {
        std::ostringstream oss;
        oss << value;
        return oss.str();
    }
}

// 提取简短的函数名
inline std::string short_function_name(const char *full_name) {
    std::string name(full_name);
    // 查找最后一个 '::' 或者 '('
    size_t pos = name.rfind("::");
    if (pos != std::string::npos) {
        pos += 2; // 跳过 "::"
    } else {
        pos = 0;
    }
    size_t end_pos = name.find('(', pos);
    if (end_pos != std::string::npos) {
        return name.substr(pos, end_pos - pos);
    }
    return name.substr(pos);
}

// Flag to indicate if we're in static destruction phase
inline bool &in_static_destruction() {
    static bool flag = false;
    return flag;
}

// Function to set the static destruction flag
inline void set_static_destruction() { in_static_destruction() = true; }
} // namespace detail

// ===========================================================================
// 日志控制
// ===========================================================================

// 控制是否显示详细位置信息
#ifndef CH_LOG_VERBOSE
#define CH_LOG_VERBOSE 1
#endif

// ===========================================================================
// 统一的日志输出宏（支持 printf 风格格式化）
// ===========================================================================

// 基础日志宏
#define CHLOG(level, fmt, ...)                                                 \
    do {                                                                       \
        if (ch::detail::in_static_destruction())                               \
            break;                                                             \
        auto loc = std::source_location::current();                            \
        std::string msg = ch::detail::printf_format(fmt, ##__VA_ARGS__);       \
        ch::detail::log_message(level, msg, loc);                              \
    } while (0)

// 简洁日志宏（不显示位置信息）
#define CHLOG_SIMPLE(level, fmt, ...)                                          \
    do {                                                                       \
        if (ch::detail::in_static_destruction())                               \
            break;                                                             \
        std::string msg = ch::detail::printf_format(fmt, ##__VA_ARGS__);       \
        ch::detail::log_message_simple(level, msg);                            \
    } while (0)

// 便捷日志宏
#if CH_LOG_VERBOSE
#define CHDBG(fmt, ...) CHLOG(ch::log_level::debug, fmt, ##__VA_ARGS__)
#define CHINFO(fmt, ...) CHLOG(ch::log_level::info, fmt, ##__VA_ARGS__)
#define CHWARN(fmt, ...) CHLOG(ch::log_level::warning, fmt, ##__VA_ARGS__)
#define CHERROR(fmt, ...) CHLOG(ch::log_level::error, fmt, ##__VA_ARGS__)
#define CHFATAL(fmt, ...) CHLOG(ch::log_level::fatal, fmt, ##__VA_ARGS__)
#else
#define CHDBG(fmt, ...) CHLOG_SIMPLE(ch::log_level::debug, fmt, ##__VA_ARGS__)
#define CHINFO(fmt, ...) CHLOG_SIMPLE(ch::log_level::info, fmt, ##__VA_ARGS__)
#define CHWARN(fmt, ...)                                                       \
    CHLOG_SIMPLE(ch::log_level::warning, fmt, ##__VA_ARGS__)
#define CHERROR(fmt, ...) CHLOG_SIMPLE(ch::log_level::error, fmt, ##__VA_ARGS__)
#define CHFATAL(fmt, ...) CHLOG_SIMPLE(ch::log_level::fatal, fmt, ##__VA_ARGS__)
#endif

// 带变量的日志宏（自动类型检测）
#define CHDBG_VAR(var)                                                         \
    do {                                                                       \
        if (!ch::detail::in_static_destruction()) {                            \
            std::string var_value = ch::detail::to_string(var);                \
            std::string msg =                                                  \
                ch::detail::printf_format("%s = %s", #var, var_value.c_str()); \
            ch::detail::log_message_simple(ch::log_level::debug, msg);         \
        }                                                                      \
    } while (0)

// 专门的指针变量输出宏
#define CHDBG_PTR(ptr)                                                         \
    do {                                                                       \
        if (!ch::detail::in_static_destruction()) {                            \
            std::string msg = ch::detail::printf_format(                       \
                "%s = 0x%llx", #ptr, (unsigned long long)(ptr));               \
            ch::detail::log_message_simple(ch::log_level::debug, msg);         \
        }                                                                      \
    } while (0)

#define CHDBG_FUNC()                                                           \
    do {                                                                       \
        if (!ch::detail::in_static_destruction()) {                            \
            auto loc = std::source_location::current();                        \
            std::string short_name =                                           \
                ch::detail::short_function_name(loc.function_name());          \
            ch::detail::log_message_simple(                                    \
                ch::log_level::debug,                                          \
                ch::detail::printf_format("[ENTER] %s", short_name.c_str()));  \
        }                                                                      \
    } while (0)

// ===========================================================================
// 统一的检查宏（不返回错误码，支持 printf 风格格式化）
// ===========================================================================

// 基础检查宏 - 不返回值，只记录错误（不终止程序）
#define CHCHECK(condition, fmt, ...)                                           \
    do {                                                                       \
        if (!(condition) && !ch::detail::in_static_destruction()) {            \
            auto loc = std::source_location::current();                        \
            std::string msg = ch::detail::printf_format(                       \
                "Check failed [%s]: " fmt, #condition, ##__VA_ARGS__);         \
            ch::detail::log_message(ch::log_level::error, msg, loc);           \
        }                                                                      \
    } while (0)

// 语义化检查宏
#define CHREQUIRE(condition, fmt, ...)                                         \
    do {                                                                       \
        if (!(condition) && !ch::detail::in_static_destruction()) {            \
            auto loc = std::source_location::current();                        \
            std::string msg = ch::detail::printf_format(                       \
                "Requirement failed [%s]: " fmt, #condition, ##__VA_ARGS__);   \
            ch::detail::log_message(ch::log_level::error, msg, loc);           \
        }                                                                      \
    } while (0)

#define CHENSURE(condition, fmt, ...)                                          \
    do {                                                                       \
        if (!(condition) && !ch::detail::in_static_destruction()) {            \
            auto loc = std::source_location::current();                        \
            std::string msg = ch::detail::printf_format(                       \
                "Ensure failed [%s]: " fmt, #condition, ##__VA_ARGS__);        \
            ch::detail::log_message(ch::log_level::error, msg, loc);           \
        }                                                                      \
    } while (0)

// 空指针检查专用宏
#define CHCHECK_NULL(ptr, fmt, ...)                                            \
    do {                                                                       \
        if ((ptr) == nullptr && !ch::detail::in_static_destruction()) {        \
            auto loc = std::source_location::current();                        \
            std::string msg = ch::detail::printf_format(                       \
                "Null pointer check failed: " fmt, ##__VA_ARGS__);             \
            ch::detail::log_message(ch::log_level::error, msg, loc);           \
        }                                                                      \
    } while (0)

// 可恢复的致命错误宏（记录错误但不终止程序）
#define CHFATAL_RECOVERABLE(fmt, ...)                                          \
    do {                                                                       \
        if (!ch::detail::in_static_destruction()) {                            \
            auto loc = std::source_location::current();                        \
            std::string msg =                                                  \
                ch::detail::printf_format("FATAL: " fmt, ##__VA_ARGS__);       \
            ch::detail::log_message(ch::log_level::fatal, msg, loc);           \
        }                                                                      \
    } while (0)

// 致命错误宏（记录错误并抛出异常）
#define CHFATAL_EXCEPTION(fmt, ...)                                            \
    do {                                                                       \
        if (!ch::detail::in_static_destruction()) {                            \
            auto loc = std::source_location::current();                        \
            std::string msg =                                                  \
                ch::detail::printf_format("FATAL: " fmt, ##__VA_ARGS__);       \
            ch::detail::log_message(ch::log_level::fatal, msg, loc);           \
            throw std::runtime_error(msg);                                     \
        }                                                                      \
    } while (0)

// 真正的终止程序宏
#define CHABORT(fmt, ...)                                                      \
    do {                                                                       \
        if (!ch::detail::in_static_destruction()) {                            \
            auto loc = std::source_location::current();                        \
            std::string msg =                                                  \
                ch::detail::printf_format("ABORT: " fmt, ##__VA_ARGS__);       \
            ch::detail::log_message(ch::log_level::fatal, msg, loc);           \
            std::abort();                                                      \
        }                                                                      \
    } while (false)

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
// 详细实现
// ===========================================================================

namespace detail {

// 日志级别到字符串的转换
inline const char *log_level_str(log_level level) {
    switch (level) {
    case log_level::debug:
        return "[DEBUG]";
    case log_level::info:
        return "[INFO]";
    case log_level::warning:
        return "[WARN]";
    case log_level::error:
        return "[ERROR]";
    case log_level::fatal:
        return "[FATAL]";
    default:
        return "[UNKNOWN]";
    }
}

// 带位置信息的日志输出
inline void log_message(log_level level, const std::string &message,
                        const std::source_location &loc) {
    // Check if we're in static destruction phase
    if (in_static_destruction()) {
        return;
    }

    // 根据日志级别决定输出流和是否输出
    switch (level) {
    case log_level::debug:
#ifndef CH_DEBUG
        return; // 调试模式下才输出debug信息
#endif
    case log_level::info:
        std::cout << log_level_str(level) << " " << message << " at "
                  << loc.file_name() << ":" << loc.line() << std::endl;
        break;

    case log_level::warning:
        std::cerr << log_level_str(level) << " " << message << " at "
                  << loc.file_name() << ":" << loc.line() << std::endl;
        break;

    case log_level::error:
        std::cerr << log_level_str(level) << " " << message << " at "
                  << loc.file_name() << ":" << loc.line() << std::endl;
        break;

    case log_level::fatal:
        std::cerr << log_level_str(level) << " " << message << " at "
                  << loc.file_name() << ":" << loc.line() << std::endl;
        break;
    }
}

// 简洁日志输出（不显示位置信息）
inline void log_message_simple(log_level level, const std::string &message) {
    // Check if we're in static destruction phase
    // if (in_static_destruction()) {
    //     return;
    // }

    // 根据日志级别决定输出流和是否输出
    switch (level) {
    case log_level::debug:
#ifndef CH_DEBUG
        return; // 调试模式下才输出debug信息
#endif
    case log_level::info:
        std::cout << log_level_str(level) << " " << message << std::endl;
        break;

    case log_level::warning:
        std::cerr << log_level_str(level) << " " << message << std::endl;
        break;

    case log_level::error:
        std::cerr << log_level_str(level) << " " << message << std::endl;
        break;

    case log_level::fatal:
        std::cerr << log_level_str(level) << " " << message << std::endl;
        break;
    }
}

} // namespace detail

// ===========================================================================
// 杂项工具
// ===========================================================================

template <typename... Args> void unused([[maybe_unused]] Args &&...args) {}

#define CHUNUSED(...) ch::unused(__VA_ARGS__)

template <std::unsigned_integral T>
constexpr unsigned bitwidth_v =
    std::numeric_limits<T>::digits + std::is_signed_v<T>;

// 作用域退出
class scope_exit {
public:
    explicit scope_exit(std::function<void()> func) : func_(std::move(func)) {}

    scope_exit(const scope_exit &) = delete;
    scope_exit &operator=(const scope_exit &) = delete;
    scope_exit(scope_exit &&) = default;
    scope_exit &operator=(scope_exit &&) = default;

    ~scope_exit() {
        // Additional safety check
        if (func_ && !detail::in_static_destruction()) {
            func_();
        }
    }

    static void *operator new(std::size_t) = delete;
    static void *operator new[](std::size_t) = delete;

private:
    std::function<void()> func_;
};

#define CHSCOPE_EXIT(func) ch::scope_exit _scope_exit_([&]() { func; })

template <typename X> struct type_print;

// 1. 声明一个未定义的模板结构体（关键）
template <auto V> struct print_constexpr_value;

// 2. 提供一个 consteval 辅助函数（可选，但更清晰）
template <auto V> consteval void constexpr_print() {
    print_constexpr_value<V>{}; // 故意实例化未定义模板 → 触发编译错误
}

} // namespace ch

#endif // LOGGER_H