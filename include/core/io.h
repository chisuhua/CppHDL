// include/core/io.h
// Phase 4: Aggregator for HDL port (ch_in/ch_out) and connection API.
//
// After Phase 4 split, this file keeps the port class definitions
// (ch_logic_in, ch_logic_out, port<,>, ch_in/ch_out aliases, width
// trait specializations) and the operator<<= connection functions
// (which preserve the 4 CHREQUIRE assertions from commit bc0cbdb /
// ADR-010 Q3 port validation).
//
// The per-operator overloads (operator&, |, ^, ~, ==, !=, &&, ||, !,
// +, -, *, /, %, <<, >>, <, <=, >, >=, bit_select, select,
// and/or/xor_reduce, sext, zext, bits, rotate_*, popcount) and the
// port<->literal mixed operators live in the 4 per-category headers:
//   include/core/io_logical.h
//   include/core/io_arithmetic.h
//   include/core/io_shift.h
//   include/core/io_lit.h
//
// All functions remain in namespace ch::core.
#pragma once

#include "ast_nodes.h"
#include "core/context.h"
#include "core/direction.h"
#include "core/lnode.h"
#include "core/lnodeimpl.h"
#include "utils/logger.h"
#include "core/operators.h"
#include "core/traits.h"
#include "core/uint.h"
#include "node_builder.h"
#include <cstdint>
#include <source_location>
#include <string>

namespace ch::core {

// === ch_logic_out 类 ===
template <typename T> class ch_logic_out {
private:
    std::string name_;
    outputimpl *output_node_ = nullptr;

public:
    using value_type = T;

    // 默认构造函数：创建具名输出节点
    ch_logic_out(
        const std::source_location &sloc = std::source_location::current())
        : name_("unnamed_output") {
        CHDBG_FUNC();
        auto ctx = ch::core::ctx_curr_;
        if (!ctx) {
            CHDBG("[ch_logic_out] No active context for output '%s', will be null",
                  name_.c_str());
            return;
        }
        outputimpl *node_ptr = ctx->create_output(ch_width_v<T>, name_, sloc);
        output_node_ = node_ptr;
        if (output_node_) {
            CHDBG("  [ch_logic_out] Created outputimpl node ID %u for '%s'",
                  output_node_->id(), name_.c_str());
        } else {
            CHERROR(
                "  [ch_logic_out] Failed to create outputimpl node for '%s'",
                name_.c_str());
        }
    }

    // 拷贝构造函数 - 浅拷贝
    ch_logic_out(const ch_logic_out &other)
        : name_(other.name_), output_node_(other.output_node_) {}

    // 赋值操作符 - 浅拷贝
    ch_logic_out &operator=(const ch_logic_out &other) {
        if (this != &other) {
            name_ = other.name_;
            output_node_ = other.output_node_;
        }
        return *this;
    }

    // 从节点构造（用于翻转等场景）
    ch_logic_out(outputimpl *existing_node, const std::string &name)
        : name_(name), output_node_(existing_node) {
        CHDBG("[ch_logic_out] Wrapping existing node for '%s'", name.c_str());
    }

    // 标准构造函数（带名称）
    ch_logic_out(
        const std::string &name,
        const std::source_location &sloc = std::source_location::current())
        : name_(name) {
        CHDBG_FUNC();
        auto ctx = ch::core::ctx_curr_;
        if (!ctx) {
            CHDBG("[ch_logic_out] No active context for output '%s', will be null",
                  name_.c_str());
            return;
        }
        outputimpl *node_ptr = ctx->create_output(ch_width_v<T>, name, sloc);
        output_node_ = node_ptr;
        if (output_node_) {
            CHDBG("  [ch_logic_out] Created outputimpl node ID %u for '%s'",
                  output_node_->id(), name_.c_str());
        } else {
            CHERROR(
                "  [ch_logic_out] Failed to create outputimpl node for '%s'",
                name_.c_str());
        }
    }

    template <typename U> void operator=(const U &value) {
        CHDBG_FUNC();
        lnode<U> src_lnode = get_lnode(value);
        if (output_node_ && src_lnode.impl()) {
            output_node_->set_src(0, src_lnode.impl());
        } else {
            CHERROR("[ch_logic_out::operator=] Error: output_node_ or "
                    "src_lnode is null for '%s'!",
                    name_.c_str());
        }
    }

    lnodeimpl *impl() const { return output_node_; }
    const std::string &name() const { return name_; }

    ~ch_logic_out() = default;
};

// === ch_logic_in 类 ===
template <typename T> class ch_logic_in {
private:
    std::string name_;
    inputimpl *input_node_ = nullptr;

public:
    using value_type = T;

    // 默认构造函数：创建具名输入节点
    ch_logic_in(
        const std::source_location &sloc = std::source_location::current())
        : name_("unnamed_input") {
        CHDBG_FUNC();
        auto ctx = ch::core::ctx_curr_;
        if (ctx) {
            inputimpl *node_ptr = ctx->create_input(ch_width_v<T>, name_, sloc);
            input_node_ = node_ptr;
            if (input_node_) {
                CHDBG("  [ch_logic_in] Created inputimpl node ID %u for '%s'",
                      input_node_->id(), name_.c_str());
            } else {
                CHERROR(
                    "  [ch_logic_in] Failed to create inputimpl node for '%s'",
                    name_.c_str());
            }
        }
    }

    // 拷贝构造函数 - 浅拷贝
    ch_logic_in(const ch_logic_in &other)
        : name_(other.name_), input_node_(other.input_node_) {}

    // 赋值操作符 - 浅拷贝
    ch_logic_in &operator=(const ch_logic_in &other) {
        if (this != &other) {
            name_ = other.name_;
            input_node_ = other.input_node_;
        }
        return *this;
    }

    // 从节点构造
    ch_logic_in(inputimpl *existing_node, const std::string &name)
        : name_(name), input_node_(existing_node) {
        CHDBG("[ch_logic_in] Wrapping existing node for '%s'", name.c_str());
    }

    // 标准构造函数（带名称）
    ch_logic_in(
        const std::string &name,
        const std::source_location &sloc = std::source_location::current())
        : name_(name) {
        CHDBG_FUNC();
        auto ctx = ch::core::ctx_curr_;
        if (ctx) {
            inputimpl *node_ptr = ctx->create_input(ch_width_v<T>, name, sloc);
            input_node_ = node_ptr;
            if (input_node_) {
                CHDBG("  [ch_logic_in] Created inputimpl node ID %u for '%s'",
                      input_node_->id(), name_.c_str());
            } else {
                CHERROR(
                    "  [ch_logic_in] Failed to create inputimpl node for '%s'",
                    name_.c_str());
            }
        }
    }

    operator lnode<T>() const { return lnode<T>(input_node_); }
    lnodeimpl *impl() const { return input_node_; }
    const std::string &name() const { return name_; }

    ~ch_logic_in() = default;
};

// === port 系统 ===
template <typename T, typename Dir> class port;

// 特化：输入端口
template <typename T> class port<T, input_direction> {
private:
    ch_logic_in<T> impl_;

public:
    using value_type = T;
    using direction = input_direction;

    port() : impl_() {}

    explicit port(const std::string &name, const std::source_location &sloc =
                                               std::source_location::current())
        : impl_(ch_logic_in<T>(name, sloc)) {}

    port(const port &other) : impl_(other.impl_) {}

    port &operator=(const port &other) = delete;

    operator lnode<T>() const { return static_cast<lnode<T>>(impl_); }

    template<unsigned W>
    operator ch_uint<W>() const requires(std::is_same_v<T, ch_uint<W>>)
    {
        return ch_uint<W>(impl());
    }

    template <typename U> void operator=(const U &value) {
        static_assert(is_input_v<input_direction>,
                      "Cannot assign to input port!");
        CHERROR("Cannot assign value to input port!");
    }

    lnodeimpl *impl() const { return impl_.impl(); }

    auto flip() const {
        port<T, output_direction> flipped_port;
        return flipped_port;
    }

    const std::string &name() const { return impl_.name(); }

    bool is_valid() const { return impl_.impl() != nullptr; }
};

// 特化：输出端口
template <typename T> class port<T, output_direction> {
private:
    ch_logic_out<T> impl_;

public:
    using value_type = T;
    using direction = output_direction;

    port() : impl_() {}

    explicit port(const std::string &name, const std::source_location &sloc =
                                               std::source_location::current())
        : impl_(ch_logic_out<T>(name, sloc)) {}

    port(const port &other) : impl_(other.impl_) {}

    port &operator=(const port &other) = delete;

    template <typename U> void operator=(const U &value) { impl_ = value; }

    template<unsigned W>
    operator ch_uint<W>() const requires(std::is_same_v<T, ch_uint<W>>)
    {
        return ch_uint<W>(impl());
    }

    lnodeimpl *impl() const { return impl_.impl(); }

    auto flip() const {
        port<T, input_direction> flipped_port;
        return flipped_port;
    }

    const std::string &name() const { return impl_.name(); }

    bool is_valid() const { return impl_.impl() != nullptr; }
};

// 类型别名
template <typename T> using ch_in = port<T, input_direction>;
template <typename T> using ch_out = port<T, output_direction>;

// --- 宽度特质特化 ---
template <typename T> struct ch_width_impl<ch_logic_out<T>, void> {
    static constexpr unsigned value = ch_width_v<T>;
};

template <typename T> struct ch_width_impl<ch_logic_in<T>, void> {
    static constexpr unsigned value = ch_width_v<T>;
};

template <typename T, typename Dir> struct ch_width_impl<port<T, Dir>, void> {
    static constexpr unsigned value = ch_width_v<T>;
};

// --- 宏定义 ---
#define __io(...)                                                              \
    struct io_type {                                                           \
        __VA_ARGS__;                                                           \
    };                                                                         \
    alignas(io_type) char io_storage_[sizeof(io_type)];                        \
    [[nodiscard]] io_type &io() {                                              \
        return *reinterpret_cast<io_type *>(io_storage_);                      \
    }

namespace core {
template <typename T> [[nodiscard]] auto in(const T & = T{}) {
    return ch::core::ch_in<T>{};
}

template <typename T> [[nodiscard]] auto out(const T & = T{}) {
    return ch::core::ch_out<T>{};
}
} // namespace core

#define __in(...) ch::core::ch_in<__VA_ARGS__>
#define __out(...) ch::core::ch_out<__VA_ARGS__>

} // namespace ch::core

// === Per-operator-category port overloads ===
#include "core/io_logical.h"
#include "core/io_arithmetic.h"
#include "core/io_shift.h"
#include "core/io_lit.h"

namespace ch::core {

// === 显式连接函数 (operator<<=) ===
// 保留 4 个 CHREQUIRE 断言（来自 commit bc0cbdb / ADR-010 Q3）

// 连接函数：输出端口驱动输入端口
template <typename T1, typename T2>
void operator<<=(const port<T1, output_direction> &receiver,
                 const port<T2, input_direction> &driver) {
    CHDBG_FUNC();
    auto *driver_impl = driver.impl();
    auto *receiver_impl = receiver.impl();

    if (!driver_impl || !receiver_impl) {
        CHERROR("Invalid port connection: null node encountered");
        return;
    }

    CHREQUIRE(receiver_impl->get_parent() == lnodeimpl::current_component(),
              "out <<= in: receiver must be a port of the current module");

    outputimpl *input_receiver = dynamic_cast<outputimpl *>(receiver_impl);
    if (input_receiver) {
        input_receiver->set_src(0, driver_impl);
    }

    CHDBG("Connected output port '%s' to input port '%s'",
          driver.name().c_str(), receiver.name().c_str());
}

// 连接函数：输入端口驱动输入端口
template <typename T1, typename T2>
void operator<<=(const port<T1, input_direction> &receiver,
                 const port<T2, input_direction> &driver) {
    CHDBG_FUNC();
    auto *driver_impl = driver.impl();
    auto *receiver_impl = receiver.impl();

    if (!driver_impl || !receiver_impl) {
        CHERROR("Invalid port connection: null node encountered");
        return;
    }

    CHREQUIRE(driver_impl->get_parent() == lnodeimpl::current_component(),
              "in <<= in (passthrough): driver must be a port of the current module");

    inputimpl *input_receiver = dynamic_cast<inputimpl *>(receiver_impl);
    if (input_receiver) {
        input_receiver->set_driver(driver_impl);
    }

    CHDBG("Connected input port '%s' to input port '%s'", driver.name().c_str(),
          receiver.name().c_str());
}

// 连接函数：输出端口驱动输出端口
template <typename T1, typename T2>
void operator<<=(const port<T1, output_direction> &receiver,
                 const port<T2, output_direction> &driver) {
    CHDBG_FUNC();
    auto *driver_impl = driver.impl();
    auto *receiver_impl = receiver.impl();

    if (!driver_impl || !receiver_impl) {
        CHERROR("Invalid port connection: null node encountered");
        return;
    }

    CHREQUIRE(receiver_impl->get_parent() == lnodeimpl::current_component(),
              "out <<= out (passthrough): receiver must be a port of the current module");

    receiver_impl->set_src(0, driver_impl);

    CHDBG("Connected output port '%s' to output port '%s'",
          driver.name().c_str(), receiver.name().c_str());
}

// 连接函数：输入端口驱动输出端口
template <typename T1, typename T2>
void operator<<=(const port<T1, input_direction> &receiver,
                 const port<T2, output_direction> &driver) {
    CHDBG_FUNC();
    auto *driver_impl = driver.impl();
    auto *receiver_impl = receiver.impl();

    if (!driver_impl || !receiver_impl) {
        CHERROR("Invalid port connection: null node encountered");
        return;
    }

    CHREQUIRE(receiver_impl->get_parent() != lnodeimpl::current_component(),
              "in <<= out: receiver must be a port of a submodule, not the current module");

    inputimpl *input_receiver = dynamic_cast<inputimpl *>(receiver_impl);
    if (input_receiver) {
        input_receiver->set_driver(driver_impl);
    }

    CHDBG("Connected input port '%s' to input port '%s'", driver.name().c_str(),
          receiver.name().c_str());
}

// 连接函数：输出端口连接到非端口类型
template <typename T, typename U>
void operator<<=(const port<T, output_direction> &receiver, const U &driver) {
    CHDBG_FUNC();
    auto *receiver_impl = receiver.impl();

    if (!receiver_impl) {
        CHERROR("Invalid port connection: receiver node is null");
        return;
    }

    auto driver_lnode = get_lnode(driver);
    auto *driver_impl = driver_lnode.impl();

    if (!driver_impl) {
        CHERROR("Invalid port connection: driver node is null");
        return;
    }

    outputimpl *output_receiver = dynamic_cast<outputimpl *>(receiver_impl);
    if (output_receiver) {
        output_receiver->set_src(0, driver_impl);
    }

    CHDBG("Connected non-port value to output port '%s'", receiver.name().c_str());
}

// 连接函数：输入端口连接到非端口类型
template <typename T, typename U>
void operator<<=(const port<T, input_direction> &receiver, const U &driver) {
    CHDBG_FUNC();
    auto *receiver_impl = receiver.impl();

    if (!receiver_impl) {
        CHERROR("Invalid port connection: receiver node is null");
        return;
    }

    auto driver_lnode = get_lnode(driver);
    auto *driver_impl = driver_lnode.impl();

    if (!driver_impl) {
        CHERROR("Invalid port connection: driver node is null");
        return;
    }

    inputimpl *input_receiver = dynamic_cast<inputimpl *>(receiver_impl);
    if (input_receiver) {
        input_receiver->set_driver(driver_impl);

        driver_impl->add_user(input_receiver);
    }

    CHDBG("Connected non-port value to input port '%s'", receiver.name().c_str());
}

} // namespace ch::core
