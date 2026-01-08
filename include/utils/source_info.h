#ifndef UTILS_SOURCE_INFO_H
#define UTILS_SOURCE_INFO_H

#include <ostream>
#include <source_location> // 引入C++20 source_location
#include <string>

namespace ch::utils {

// 重新定义CH_REQUIRES宏，避免依赖bv/utils.h
#define CH_REQUIRES(...) std::enable_if_t<(__VA_ARGS__), int>

// 使用std::source_location进行封装，保持原有接口
class source_location {
private:
    const char *file_;
    int line_;
    int column_;

public:
    // 只保留这一个构造函数，使用 std::source_location 默认参数
    explicit source_location(
        const std::source_location &loc = std::source_location::current())
        : file_(loc.file_name()), line_(loc.line()), column_(loc.column()) {}

    const char *file_name() const { return file_; }
    int line() const { return line_; }
    int column() const { return column_; }

    // 添加 empty 方法来检查是否为空
    bool empty() const {
        return (file_ == nullptr || *file_ == '\0') && line_ == 0;
    }
    friend std::ostream &operator<<(std::ostream &out,
                                    const source_location &sloc) {
        if (!sloc.empty()) {
            out << sloc.file_name() << ":" << sloc.line();
            if (sloc.column() > 0) {
                out << ":" << sloc.column();
            }
        } else {
            out << "<unknown source location>";
        }
    }
};

// 更新宏定义以使用std::source_location
#define CH_CUR_SLOC ch::utils::source_location(std::source_location::current())

#define CH_SLOC const ch::utils::source_location &sloc = CH_CUR_SLOC

template <typename T> struct sloc_arg {
    sloc_arg(const T &p_data, const source_location &p_sloc = CH_CUR_SLOC)
        : data(p_data), sloc(p_sloc) {}

    template <typename U, CH_REQUIRES(std::is_convertible_v<U, T>)>
    sloc_arg(const sloc_arg<U> &other) : data(other.data), sloc(other.sloc) {}

    const T &data;
    source_location sloc;
};

///////////////////////////////////////////////////////////////////////////////

class source_info {
public:
    explicit source_info(const source_location &sloc = source_location(),
                         const std::string &name = "")
        : sloc_(sloc), name_(name) {}

    source_info(const source_info &other)
        : sloc_(other.sloc_), name_(other.name_) {}

    source_info(source_info &&other)
        : sloc_(std::move(other.sloc_)), name_(std::move(other.name_)) {}

    source_info &operator=(const source_info &other) {
        sloc_ = other.sloc_;
        name_ = other.name_;
        return *this;
    }

    source_info &operator=(source_info &&other) {
        sloc_ = std::move(other.sloc_);
        name_ = std::move(other.name_);
        return *this;
    }

    const source_location &sloc() const { return sloc_; }

    const std::string &name() const { return name_; }

    bool hasName() const { return !name_.empty(); }

    bool hasLocation() const { return !sloc_.empty(); }

    // 检查是否为空：当文件名为空字符串或行号为0时认为是无效位置
    bool empty() const { return !hasName() && sloc_.empty(); }

private:
    friend std::ostream &operator<<(std::ostream &out,
                                    const source_info &srcinfo) {
        if (srcinfo.hasName()) {
            out << "'" << srcinfo.name() << "' in " << srcinfo.sloc();
        } else {
            out << srcinfo.sloc();
        }
        return out;
    }

    source_location sloc_;
    std::string name_;
};

// 使用nameof库定义一个宏来创建source_info，支持传入变量
#define CH_MAKE_SOURCE_INFO(var)                                               \
    ch::utils::source_info(CH_CUR_SLOC,                                        \
                           std::string(::nameof::NAMEOF_RAW(var).c_str()))

// 保持默认的source_info创建方式，当没有传入变量时使用空名称
#define CH_CUR_SRC_INFO ch::utils::source_info(CH_CUR_SLOC, "")

#define CH_SRC_INFO const ch::utils::source_info &srcinfo = CH_CUR_SRC_INFO

// 定义一个特殊宏用于从变量创建source_info
#define CH_SRC_INFO_VAR(var)                                                   \
    const ch::utils::source_info &srcinfo = CH_MAKE_SOURCE_INFO(var)

template <typename T> struct srcinfo_arg {
    srcinfo_arg(const T &p_data, const source_info &p_srcinfo = CH_CUR_SRC_INFO)
        : data(p_data), srcinfo(p_srcinfo) {}

    template <typename U, CH_REQUIRES(std::is_convertible_v<U, T>)>
    srcinfo_arg(const srcinfo_arg<U> &other)
        : data(other.data), srcinfo(other.srcinfo) {}

    const T &data;
    source_info srcinfo;
};

} // namespace ch::utils

#endif