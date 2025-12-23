#ifndef IO_H
#define IO_H

#include "ast_nodes.h"
#include "core/context.h"
#include "direction.h"
#include "lnode.h"
#include "lnodeimpl.h"
#include "logger.h"
#include "operators.h"
#include "traits.h"
#include "uint.h"
#include <cstdint>
#include <source_location>
#include <string>

#include "node_builder.h"
namespace ch::core {

// === 改进的 ch_logic_out 类 ===
template <typename T> class ch_logic_out {
private:
    std::string name_;
    outputimpl *output_node_ = nullptr;

public:
    using value_type = T;

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

    // 标准构造函数
    ch_logic_out(
        const std::string &name = "io",
        const std::source_location &sloc = std::source_location::current())
        : name_(name) {
        CHDBG_FUNC();
        auto ctx = ch::core::ctx_curr_;
        if (!ctx) {
            CHERROR("[ch_logic_out] Error: No active context for output '%s'!",
                    name.c_str());
            return;
        }
        outputimpl *node_ptr = ctx->create_output(ch_width_v<T>, name, sloc);
        output_node_ = node_ptr;
        if (output_node_) {
            CHDBG("  [ch_logic_out] Created outputimpl node ID %u for '%s'",
                  output_node_->id(), name.c_str());
        } else {
            CHERROR(
                "  [ch_logic_out] Failed to create outputimpl node for '%s'",
                name.c_str());
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

// === 改进的 ch_logic_in 类 ===
template <typename T> class ch_logic_in {
private:
    std::string name_;
    inputimpl *input_node_ = nullptr;

public:
    using value_type = T;

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

    // 标准构造函数
    ch_logic_in(
        const std::string &name = "io",
        const std::source_location &sloc = std::source_location::current())
        : name_(name) {
        CHDBG_FUNC();
        auto ctx = ch::core::ctx_curr_;
        if (ctx) {
            inputimpl *node_ptr = ctx->create_input(ch_width_v<T>, name, sloc);
            input_node_ = node_ptr;
            if (input_node_) {
                CHDBG("  [ch_logic_in] Created inputimpl node ID %u for '%s'",
                      input_node_->id(), name.c_str());
            } else {
                CHERROR(
                    "  [ch_logic_in] Failed to create inputimpl node for '%s'",
                    name.c_str());
            }
        }
    }

    operator lnode<T>() const { return lnode<T>(input_node_); }
    lnodeimpl *impl() const { return input_node_; }
    const std::string &name() const { return name_; }

    ~ch_logic_in() = default;
};

// === 改进的 port 系统 ===
template <typename T, typename Dir> class port;

// 特化：输入端口
template <typename T> class port<T, input_direction> {
private:
    ch_logic_in<T> impl_;

public:
    using value_type = T;
    using direction = input_direction;

    port() = default;

    // 从名称构造
    explicit port(const std::string &name, const std::source_location &sloc =
                                               std::source_location::current())
        : impl_(ch_logic_in<T>(name, sloc)) {}

    // 拷贝构造函数
    port(const port &other) : impl_(other.impl_) {}

    // ... existing code ...
    port &operator=(const port &other) = delete;
    // ... existing code ...
    // 赋值操作符
    // port &operator=(const port &other) {
    //     if (this != &other) {
    //         impl_ = other.impl_;
    //     }
    //     return *this;
    // }

    operator lnode<T>() const { return static_cast<lnode<T>>(impl_); }

    template <typename U> void operator=(const U &value) {
        static_assert(is_input_v<input_direction>,
                      "Cannot assign to input port!");
        CHERROR("Cannot assign value to input port!");
    }

    lnodeimpl *impl() const { return impl_.impl(); }

    auto flip() const {
        // 简化实现：创建对应方向的新端口，不共享节点
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

    port() = default;

    // 从名称构造
    explicit port(const std::string &name, const std::source_location &sloc =
                                               std::source_location::current())
        : impl_(ch_logic_out<T>(name, sloc)) {}

    // 拷贝构造函数
    port(const port &other) : impl_(other.impl_) {}

    // 赋值操作符
    port &operator=(const port &other) = delete;
    // port &operator=(const port &other) {
    //     if (this != &other) {
    //         impl_ = other.impl_;
    //     }
    //     return *this;
    // }

    template <typename U> void operator=(const U &value) { impl_ = value; }

    lnodeimpl *impl() const { return impl_.impl(); }

    auto flip() const {
        // 简化实现：创建对应方向的新端口，不共享节点
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

// 为新 port 系统特化宽度
template <typename T, typename Dir> struct ch_width_impl<port<T, Dir>, void> {
    static constexpr unsigned value = ch_width_v<T>;
};

// --- 保持原来的宏定义 ---
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

// 为端口添加按位与操作符 (&)
template <typename T1, typename Dir1, typename T2, typename Dir2>
auto operator&(const port<T1, Dir1> &lhs, const port<T2, Dir2> &rhs) {
    return get_lnode(lhs) & get_lnode(rhs);
}

// 为端口添加按位或操作符 (|)
template <typename T1, typename Dir1, typename T2, typename Dir2>
auto operator|(const port<T1, Dir1> &lhs, const port<T2, Dir2> &rhs) {
    return get_lnode(lhs) | get_lnode(rhs);
}

// 为端口添加按位异或操作符 (^)
template <typename T1, typename Dir1, typename T2, typename Dir2>
auto operator^(const port<T1, Dir1> &lhs, const port<T2, Dir2> &rhs) {
    return get_lnode(lhs) ^ get_lnode(rhs);
}

// 为端口添加按位取反操作符 (~)
template <typename T, typename Dir>
auto operator~(const port<T, Dir> &operand) {
    return ~get_lnode(operand);
}

// 为端口添加相等比较操作符 (==)
template <typename T1, typename Dir1, typename T2, typename Dir2>
auto operator==(const port<T1, Dir1> &lhs, const port<T2, Dir2> &rhs) {
    return get_lnode(lhs) == get_lnode(rhs);
}

// 为端口添加不等比较操作符 (!=)
template <typename T1, typename Dir1, typename T2, typename Dir2>
auto operator!=(const port<T1, Dir1> &lhs, const port<T2, Dir2> &rhs) {
    return get_lnode(lhs) != get_lnode(rhs);
}

// 为端口添加逻辑操作符支持
template <typename T1, typename Dir1, typename T2, typename Dir2>
auto operator&&(const port<T1, Dir1> &lhs, const port<T2, Dir2> &rhs) {
    static_assert(ch_width_v<T1> == 1 && ch_width_v<T2> == 1,
                  "Logical AND only supported for 1-bit types");
    // 在硬件中，逻辑与和按位与效果相同
    return get_lnode(lhs) & get_lnode(rhs);
}

template <typename T1, typename Dir1, typename T2, typename Dir2>
auto operator||(const port<T1, Dir1> &lhs, const port<T2, Dir2> &rhs) {
    static_assert(ch_width_v<T1> == 1 && ch_width_v<T2> == 1,
                  "Logical OR only supported for 1-bit types");
    // 在硬件中，逻辑或和按位或效果相同
    return get_lnode(lhs) | get_lnode(rhs);
}

template <typename T, typename Dir>
auto operator!(const port<T, Dir> &operand) {
    static_assert(ch_width_v<T> == 1,
                  "Logical NOT only supported for 1-bit types");
    return ~get_lnode(operand);
}

// 端口与常量的操作符支持
template <typename T, typename Dir, typename Lit>
    requires std::is_arithmetic_v<Lit>
auto operator&(const port<T, Dir> &lhs, Lit rhs) {
    return get_lnode(lhs) & rhs;
}

template <typename T, typename Dir, typename Lit>
    requires std::is_arithmetic_v<Lit>
auto operator&(Lit lhs, const port<T, Dir> &rhs) {
    return lhs & get_lnode(rhs);
}

// 补全其他算术操作符
template <typename T1, typename Dir1, typename T2, typename Dir2>
auto operator+(const port<T1, Dir1> &lhs, const port<T2, Dir2> &rhs) {
    return get_lnode(lhs) + get_lnode(rhs);
}

template <typename T1, typename Dir1, typename T2, typename Dir2>
auto operator-(const port<T1, Dir1> &lhs, const port<T2, Dir2> &rhs) {
    return get_lnode(lhs) - get_lnode(rhs);
}

template <typename T1, typename Dir1, typename T2, typename Dir2>
auto operator*(const port<T1, Dir1> &lhs, const port<T2, Dir2> &rhs) {
    return get_lnode(lhs) * get_lnode(rhs);
}

template <typename T1, typename Dir1, typename T2, typename Dir2>
auto operator/(const port<T1, Dir1> &lhs, const port<T2, Dir2> &rhs) {
    return get_lnode(lhs) / get_lnode(rhs);
}

template <typename T1, typename Dir1, typename T2, typename Dir2>
auto operator%(const port<T1, Dir1> &lhs, const port<T2, Dir2> &rhs) {
    return get_lnode(lhs) % get_lnode(rhs);
}

template <typename T1, typename Dir1, typename T2, typename Dir2>
auto operator<<(const port<T1, Dir1> &lhs, const port<T2, Dir2> &rhs) {
    return get_lnode(lhs) << get_lnode(rhs);
}

template <typename T1, typename Dir1, typename T2, typename Dir2>
auto operator>>(const port<T1, Dir1> &lhs, const port<T2, Dir2> &rhs) {
    return get_lnode(lhs) >> get_lnode(rhs);
}

// 比较操作符
template <typename T1, typename Dir1, typename T2, typename Dir2>
auto operator<(const port<T1, Dir1> &lhs, const port<T2, Dir2> &rhs) {
    return get_lnode(lhs) < get_lnode(rhs);
}

template <typename T1, typename Dir1, typename T2, typename Dir2>
auto operator<=(const port<T1, Dir1> &lhs, const port<T2, Dir2> &rhs) {
    return get_lnode(lhs) <= get_lnode(rhs);
}

template <typename T1, typename Dir1, typename T2, typename Dir2>
auto operator>(const port<T1, Dir1> &lhs, const port<T2, Dir2> &rhs) {
    return get_lnode(lhs) > get_lnode(rhs);
}

template <typename T1, typename Dir1, typename T2, typename Dir2>
auto operator>=(const port<T1, Dir1> &lhs, const port<T2, Dir2> &rhs) {
    return get_lnode(lhs) >= get_lnode(rhs);
}

// 一元操作符
template <typename T, typename Dir>
auto operator-(const port<T, Dir> &operand) {
    return -get_lnode(operand);
}

// 添加混合操作符支持（端口与常量）
template <typename T, typename Dir, typename Lit>
    requires std::is_arithmetic_v<Lit>
auto operator+(const port<T, Dir> &lhs, Lit rhs) {
    return get_lnode(lhs) + rhs;
}

template <typename T, typename Dir, typename Lit>
    requires std::is_arithmetic_v<Lit>
auto operator+(Lit lhs, const port<T, Dir> &rhs) {
    return lhs + get_lnode(rhs);
}

template <typename T, typename Dir, typename Lit>
    requires std::is_arithmetic_v<Lit>
auto operator-(const port<T, Dir> &lhs, Lit rhs) {
    return get_lnode(lhs) - rhs;
}

template <typename T, typename Dir, typename Lit>
    requires std::is_arithmetic_v<Lit>
auto operator-(Lit lhs, const port<T, Dir> &rhs) {
    return lhs - get_lnode(rhs);
}

template <typename T, typename Dir, typename Lit>
    requires std::is_arithmetic_v<Lit>
auto operator*(const port<T, Dir> &lhs, Lit rhs) {
    return get_lnode(lhs) * rhs;
}

template <typename T, typename Dir, typename Lit>
    requires std::is_arithmetic_v<Lit>
auto operator*(Lit lhs, const port<T, Dir> &rhs) {
    return lhs * get_lnode(rhs);
}

template <typename T, typename Dir, typename Lit>
    requires std::is_arithmetic_v<Lit>
auto operator/(const port<T, Dir> &lhs, Lit rhs) {
    return get_lnode(lhs) / rhs;
}

template <typename T, typename Dir, typename Lit>
    requires std::is_arithmetic_v<Lit>
auto operator/(Lit lhs, const port<T, Dir> &rhs) {
    return lhs / get_lnode(rhs);
}

template <typename T, typename Dir, typename Lit>
    requires std::is_arithmetic_v<Lit>
auto operator%(const port<T, Dir> &lhs, Lit rhs) {
    return get_lnode(lhs) % rhs;
}

template <typename T, typename Dir, typename Lit>
    requires std::is_arithmetic_v<Lit>
auto operator%(Lit lhs, const port<T, Dir> &rhs) {
    return lhs % get_lnode(rhs);
}

template <typename T, typename Dir, typename Lit>
    requires std::is_arithmetic_v<Lit>
auto operator|(const port<T, Dir> &lhs, Lit rhs) {
    return get_lnode(lhs) | rhs;
}

template <typename T, typename Dir, typename Lit>
    requires std::is_arithmetic_v<Lit>
auto operator|(Lit lhs, const port<T, Dir> &rhs) {
    return lhs | get_lnode(rhs);
}

template <typename T, typename Dir, typename Lit>
    requires std::is_arithmetic_v<Lit>
auto operator^(const port<T, Dir> &lhs, Lit rhs) {
    return get_lnode(lhs) ^ rhs;
}

template <typename T, typename Dir, typename Lit>
    requires std::is_arithmetic_v<Lit>
auto operator^(Lit lhs, const port<T, Dir> &rhs) {
    return lhs ^ get_lnode(rhs);
}

template <typename T, typename Dir, typename Lit>
    requires std::is_arithmetic_v<Lit>
auto operator<<(const port<T, Dir> &lhs, Lit rhs) {
    return get_lnode(lhs) << rhs;
}

template <typename T, typename Dir, typename Lit>
    requires std::is_arithmetic_v<Lit>
auto operator>>(const port<T, Dir> &lhs, Lit rhs) {
    return get_lnode(lhs) >> rhs;
}

// 添加更多混合操作符支持...

// === 添加对bit_select操作的支持 ===
template <unsigned Index, typename T, typename Dir>
auto bit_select(const port<T, Dir> &operand) {
    // return bit_select<T, Index>(get_lnode(operand()));
    // lnode<T> node = static_cast<lnode<T>>(operand);
    auto node = to_operand(operand);
    // 直接构建位选择节点（不经过 operators.h 的重载）
    auto *op_node = node_builder::instance().build_bit_select(
        node, Index, "bit_select", std::source_location::current());

    return make_uint_result<1>(op_node);
}

// === 添加对concat操作的支持 ===
template <typename T1, typename Dir1, typename T2, typename Dir2>
auto concat(const port<T1, Dir1> &lhs, const port<T2, Dir2> &rhs) {
    return concat(get_lnode(lhs), get_lnode(rhs));
}

template <typename T, typename Dir, typename U>
    requires std::is_arithmetic_v<U>
auto concat(const port<T, Dir> &lhs, U rhs) {
    return concat(get_lnode(lhs), rhs);
}

template <typename T, typename Dir, typename U>
    requires std::is_arithmetic_v<U>
auto concat(U lhs, const port<T, Dir> &rhs) {
    return concat(lhs, get_lnode(rhs));
}

// === 添加对select操作的支持 ===
template <typename CondType, typename T1, typename Dir1, typename T2,
          typename Dir2>
auto select(const CondType &condition, const port<T1, Dir1> &true_val,
            const port<T2, Dir2> &false_val) {
    return select(condition, get_lnode(true_val), get_lnode(false_val));
}

// === 添加对归约操作的支持 ===
template <typename T, typename Dir>
auto and_reduce(const port<T, Dir> &operand) {
    return and_reduce(get_lnode(operand));
}

template <typename T, typename Dir>
auto or_reduce(const port<T, Dir> &operand) {
    return or_reduce(get_lnode(operand));
}

template <typename T, typename Dir>
auto xor_reduce(const port<T, Dir> &operand) {
    return xor_reduce(get_lnode(operand));
}

// 添加端口类型的重载版本
template <typename T, unsigned NewWidth, typename Dir>
auto sext(const port<T, Dir> &operand) {
    auto lnode_operand = to_operand(operand);
    auto *impl = lnode_operand.impl();
    return make_uint_result<NewWidth>(node_builder::instance().build_operation(
        ch_op::sext, lnode_operand, NewWidth, true, "sext",
        std::source_location::current()));
}

// 修改端口类型的重载版本
template <typename T, unsigned NewWidth, typename Dir>
auto zext(const port<T, Dir> &operand) {
    auto lnode_operand = to_operand(operand);
    auto *impl = lnode_operand.impl();
    return make_uint_result<NewWidth>(node_builder::instance().build_operation(
        ch_op::zext, lnode_operand, NewWidth, false, "zext",
        std::source_location::current()));
}

// 修改端口类型的重载版本，使用build_bits函数
template <typename T, unsigned MSB, unsigned LSB, typename Dir>
auto bits(const port<T, Dir> &operand) {
    auto lnode_operand = to_operand(operand);

    auto *op_node = node_builder::instance().build_bits(
        lnode_operand, MSB, LSB, "bits", std::source_location::current());

    return make_uint_result<MSB - LSB + 1>(op_node);
}

// === 添加对循环移位操作的支持 ===
template <typename T1, typename Dir1, typename T2, typename Dir2>
auto rotate_left(const port<T1, Dir1> &lhs, const port<T2, Dir2> &rhs) {
    return rotate_left(get_lnode(lhs), get_lnode(rhs));
}

template <typename T1, typename Dir1, typename T2, typename Dir2>
auto rotate_right(const port<T1, Dir1> &lhs, const port<T2, Dir2> &rhs) {
    return rotate_right(get_lnode(lhs), get_lnode(rhs));
}

// === 添加对popcount操作的支持 ===
template <typename T, typename Dir> auto popcount(const port<T, Dir> &operand) {
    return popcount(get_lnode(operand));
}

// === 添加显式连接函数 ===
// 连接函数：输出端口驱动输入端口
template <typename T1, typename T2>
void operator<<=(const port<T1, output_direction> &receiver,
                 const port<T2, input_direction> &driver) {
    CHDBG_FUNC();
    // 获取驱动方和接收方的实现节点
    auto *driver_impl = driver.impl();
    auto *receiver_impl = receiver.impl();

    // 检查节点有效性
    if (!driver_impl || !receiver_impl) {
        CHERROR("Invalid port connection: null node encountered");
        return;
    }

    // TODO: check the port is module port

    // 建立连接：设置接收方的驱动源为驱动方
    // receiver_impl 是 inputimpl 类型，有 set_driver 方法
    outputimpl *input_receiver = dynamic_cast<outputimpl *>(receiver_impl);
    if (input_receiver) {
        input_receiver->set_src(0, driver_impl);

        // 同时在驱动方添加用户关系
        // driver_impl->add_user(input_receiver);
    }

    CHDBG("Connected output port '%s' to input port '%s'",
          driver.name().c_str(), receiver.name().c_str());
}

// 连接函数：输入端口驱动输入端口
template <typename T1, typename T2>
void operator<<=(const port<T1, input_direction> &receiver,
                 const port<T2, input_direction> &driver) {
    CHDBG_FUNC();
    // 获取驱动方和接收方的实现节点
    auto *driver_impl = driver.impl();
    auto *receiver_impl = receiver.impl();

    // 检查节点有效性
    if (!driver_impl || !receiver_impl) {
        CHERROR("Invalid port connection: null node encountered");
        return;
    }

    // TODO: check receiver is submodule port, driver is module port

    // 建立连接：设置接收方的驱动源为驱动方
    // receiver_impl 是 inputimpl 类型，有 set_driver 方法
    inputimpl *input_receiver = dynamic_cast<inputimpl *>(receiver_impl);
    if (input_receiver) {
        input_receiver->set_driver(driver_impl);
    }

    CHDBG("Connected input port '%s' to input port '%s'", driver.name().c_str(),
          receiver.name().c_str());
}

// 连接函数：输出端口驱动输出端口（用于连接子模块输出到顶层输出）
template <typename T1, typename T2>
void operator<<=(const port<T1, output_direction> &receiver,
                 const port<T2, output_direction> &driver) {
    CHDBG_FUNC();
    // 获取驱动方和接收方的实现节点
    auto *driver_impl = driver.impl();
    auto *receiver_impl = receiver.impl();

    // 检查节点有效性
    if (!driver_impl || !receiver_impl) {
        CHERROR("Invalid port connection: null node encountered");
        return;
    }

    // TODO: check receiver is module port, driver is submodule port

    // 对于输出到输出的连接，设置接收方的源为驱动方
    receiver_impl->set_src(0, driver_impl);

    CHDBG("Connected output port '%s' to output port '%s'",
          driver.name().c_str(), receiver.name().c_str());
}

// 连接函数：输入端口驱动输出端口
template <typename T1, typename T2>
void operator<<=(const port<T1, input_direction> &receiver,
                 const port<T2, output_direction> &driver) {
    CHDBG_FUNC();
    // 获取驱动方和接收方的实现节点
    auto *driver_impl = driver.impl();
    auto *receiver_impl = receiver.impl();

    // 检查节点有效性
    if (!driver_impl || !receiver_impl) {
        CHERROR("Invalid port connection: null node encountered");
        return;
    }

    // TODO: check receiver is submodule port, driver is submodule port

    // 建立连接：设置接收方的驱动源为驱动方
    // receiver_impl 是 outputimpl 类型，需要添加源
    outputimpl *output_receiver = dynamic_cast<outputimpl *>(receiver_impl);
    if (output_receiver) {
        // 清除旧的源并添加新的源
        // 注意：这里简化处理，实际应用中可能需要更复杂的源管理
        output_receiver->add_src(driver_impl);
    }

    CHDBG("Connected input port '%s' to output port '%s'",
          driver.name().c_str(), receiver.name().c_str());
}

} // namespace ch::core

#endif // IO_H
