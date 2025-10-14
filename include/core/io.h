// include/core/io.h - 修复后的完整版本
#ifndef IO_H
#define IO_H

#include "lnode.h"
#include "uint.h"
#include "core/context.h"
#include "lnodeimpl.h"
#include "traits.h"
#include "ast_nodes.h" 
#include "logger.h"
#include "direction.h"
#include <source_location>
#include <string>

namespace ch { namespace core {

// === 改进的 ch_logic_out 类 ===
template <typename T>
class ch_logic_out {
private:
    std::string name_;
    outputimpl* output_node_ = nullptr;

public:
    using value_type = T;
    
    // 拷贝构造函数 - 浅拷贝
    ch_logic_out(const ch_logic_out& other) 
        : name_(other.name_), output_node_(other.output_node_) {}
    
    // 赋值操作符 - 浅拷贝
    ch_logic_out& operator=(const ch_logic_out& other) {
        if (this != &other) {
            name_ = other.name_;
            output_node_ = other.output_node_;
        }
        return *this;
    }
    
    // 从节点构造（用于翻转等场景）
    ch_logic_out(outputimpl* existing_node, const std::string& name)
        : name_(name), output_node_(existing_node) {
        CHDBG("[ch_logic_out] Wrapping existing node for '%s'", name.c_str());
    }
    
    // 标准构造函数
    ch_logic_out(const std::string& name = "io", const std::source_location& sloc = std::source_location::current())
        : name_(name) {
        CHDBG_FUNC();
        auto ctx = ch::core::ctx_curr_;
        if (!ctx) {
            CHERROR("[ch_logic_out] Error: No active context for output '%s'!", name.c_str());
            return;
        }
        outputimpl* node_ptr = ctx->create_output(ch_width_v<T>, name, sloc);
        output_node_ = node_ptr;
        if (output_node_) {
            CHDBG("  [ch_logic_out] Created outputimpl node ID %u for '%s'", output_node_->id(), name.c_str());
        } else {
            CHERROR("  [ch_logic_out] Failed to create outputimpl node for '%s'", name.c_str());
        }
    }

    template <typename U>
    void operator=(const U& value) {
        CHDBG_FUNC();
        lnode<U> src_lnode = get_lnode(value);
        if (output_node_ && src_lnode.impl()) {
            output_node_->set_src(0, src_lnode.impl());
        } else {
            CHERROR("[ch_logic_out::operator=] Error: output_node_ or src_lnode is null for '%s'!", name_.c_str());
        }
    }

    lnodeimpl* impl() const { return output_node_; }
    const std::string& name() const { return name_; }

    ~ch_logic_out() = default;
};

// === 改进的 ch_logic_in 类 ===
template <typename T>
class ch_logic_in {
private:
    std::string name_;
    inputimpl* input_node_ = nullptr;

public:
    using value_type = T;
    
    // 拷贝构造函数 - 浅拷贝
    ch_logic_in(const ch_logic_in& other) 
        : name_(other.name_), input_node_(other.input_node_) {}
    
    // 赋值操作符 - 浅拷贝
    ch_logic_in& operator=(const ch_logic_in& other) {
        if (this != &other) {
            name_ = other.name_;
            input_node_ = other.input_node_;
        }
        return *this;
    }
    
    // 从节点构造
    ch_logic_in(inputimpl* existing_node, const std::string& name)
        : name_(name), input_node_(existing_node) {
        CHDBG("[ch_logic_in] Wrapping existing node for '%s'", name.c_str());
    }
    
    // 标准构造函数
    ch_logic_in(const std::string& name = "io", const std::source_location& sloc = std::source_location::current())
        : name_(name) {
        CHDBG_FUNC();
        auto ctx = ch::core::ctx_curr_;
        if (ctx) {
            inputimpl* node_ptr = ctx->create_input(ch_width_v<T>, name, sloc);
            input_node_ = node_ptr;
            if (input_node_) {
                CHDBG("  [ch_logic_in] Created inputimpl node ID %u for '%s'", input_node_->id(), name.c_str());
            } else {
                CHERROR("  [ch_logic_in] Failed to create inputimpl node for '%s'", name.c_str());
            }
        }
    }

    operator lnode<T>() const { return lnode<T>(input_node_); }
    lnodeimpl* impl() const { return input_node_; }
    const std::string& name() const { return name_; }

    ~ch_logic_in() = default;
};

// === 改进的 port 系统 ===
template<typename T, typename Dir>
class port;

// 特化：输入端口
template<typename T>
class port<T, input_direction> {
private:
    ch_logic_in<T> impl_;

public:
    using value_type = T;
    using direction = input_direction;

    port() = default;
    
    // 从名称构造
    explicit port(const std::string& name, const std::source_location& sloc = std::source_location::current())
        : impl_(ch_logic_in<T>(name, sloc)) {}
    
    // 拷贝构造函数
    port(const port& other) : impl_(other.impl_) {}
    
    // 赋值操作符
    port& operator=(const port& other) {
        if (this != &other) {
            impl_ = other.impl_;
        }
        return *this;
    }

    operator lnode<T>() const { 
        return static_cast<lnode<T>>(impl_);
    }

    template<typename U>
    void operator=(const U& value) {
        static_assert(is_input_v<input_direction>, "Cannot assign to input port!");
        CHERROR("Cannot assign value to input port!");
    }

    lnodeimpl* impl() const { 
        return impl_.impl(); 
    }

    auto flip() const {
        // 简化实现：创建对应方向的新端口，不共享节点
        port<T, output_direction> flipped_port;
        return flipped_port;
    }

    const std::string& name() const { 
        return impl_.name();
    }

    bool is_valid() const { 
        return impl_.impl() != nullptr; 
    }
};

// 特化：输出端口
template<typename T>
class port<T, output_direction> {
private:
    ch_logic_out<T> impl_;

public:
    using value_type = T;
    using direction = output_direction;

    port() = default;
    
    // 从名称构造
    explicit port(const std::string& name, const std::source_location& sloc = std::source_location::current())
        : impl_(ch_logic_out<T>(name, sloc)) {}
    
    // 拷贝构造函数
    port(const port& other) : impl_(other.impl_) {}
    
    // 赋值操作符
    port& operator=(const port& other) {
        if (this != &other) {
            impl_ = other.impl_;
        }
        return *this;
    }

    template<typename U>
    void operator=(const U& value) {
        impl_ = value;
    }

    lnodeimpl* impl() const { 
        return impl_.impl(); 
    }

    auto flip() const {
        // 简化实现：创建对应方向的新端口，不共享节点
        port<T, input_direction> flipped_port;
        return flipped_port;
    }

    const std::string& name() const { 
        return impl_.name();
    }

    bool is_valid() const { 
        return impl_.impl() != nullptr; 
    }
};

// 类型别名
template<typename T> using ch_in = port<T, input_direction>;
template<typename T> using ch_out = port<T, output_direction>;

// --- 宽度特质特化 ---
template <typename T>
struct ch_width_impl<ch_logic_out<T>, void> {
    static constexpr unsigned value = ch_width_v<T>;
};

template <typename T>
struct ch_width_impl<ch_logic_in<T>, void> {
    static constexpr unsigned value = ch_width_v<T>;
};

// 为新 port 系统特化宽度
template<typename T, typename Dir>
struct ch_width_impl<port<T, Dir>, void> {
    static constexpr unsigned value = ch_width_v<T>;
};

// --- 保持原来的宏定义 ---
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


// 为端口添加按位与操作符 (&)
template<typename T1, typename Dir1, typename T2, typename Dir2>
auto operator&(const port<T1, Dir1>& lhs, const port<T2, Dir2>& rhs) {
    //static_assert(ch_width_v<T1> == 1 && ch_width_v<T2> == 1, 
    //              "Bitwise AND only supported for 1-bit types");
    //return static_cast<lnode<T1>>(lhs) & static_cast<lnode<T2>>(rhs);
    //auto lhs_lnode = get_lnode(static_cast<lnode<T1>>(lhs));
    //auto rhs_lnode = get_lnode(static_cast<lnode<T2>>(rhs));
    //return lhs_lnode & rhs_lnode;
    //return (*lhs.impl()) & (*rhs.impl());  // 这可能还需要进一步实现
    // 直接使用 port 的 impl() 方法获取底层节点
    auto lhs_impl = lhs.impl();
    auto rhs_impl = rhs.impl();
    
    // 使用现有的 createOpNodeImpl 创建操作节点
    if (lhs_impl && rhs_impl) {
        lnodeimpl* op_node = createOpNodeImpl(
            ch_op::and_, 
            std::max(ch_width_v<T1>, ch_width_v<T2>), 
            false,
            lnode<T1>(lhs_impl), 
            lnode<T2>(rhs_impl),
            "and_port_op", 
            std::source_location::current()
        );
        // 返回适当类型的结果
        using result_type = std::conditional_t<
            (ch_width_v<T1> >= ch_width_v<T2>), 
            ch_uint<ch_width_v<T1>>, 
            ch_uint<ch_width_v<T2>>
        >;
        return result_type(op_node);
    }
    
    // 错误情况返回默认值
    //return ch_uint<1>(0);
}

// 为端口添加按位或操作符 (|)
template<typename T1, typename Dir1, typename T2, typename Dir2>
auto operator|(const port<T1, Dir1>& lhs, const port<T2, Dir2>& rhs) {
    //static_assert(ch_width_v<T1> == 1 && ch_width_v<T2> == 1, 
    //              "Bitwise OR only supported for 1-bit types");
    return get_lnode(lhs) | get_lnode(rhs);
}

// 为端口添加按位异或操作符 (^)
template<typename T1, typename Dir1, typename T2, typename Dir2>
auto operator^(const port<T1, Dir1>& lhs, const port<T2, Dir2>& rhs) {
    //static_assert(ch_width_v<T1> == 1 && ch_width_v<T2> == 1, 
    //              "Bitwise XOR only supported for 1-bit types");
    return get_lnode(lhs) ^ get_lnode(rhs);
}

// 为端口添加按位取反操作符 (~)
template<typename T, typename Dir>
auto operator~(const port<T, Dir>& operand) {
    //static_assert(ch_width_v<T> == 1, 
    //              "Bitwise NOT only supported for 1-bit types");
    return ~get_lnode(operand);
}

// 为端口添加相等比较操作符 (==)
template<typename T1, typename Dir1, typename T2, typename Dir2>
auto operator==(const port<T1, Dir1>& lhs, const port<T2, Dir2>& rhs) {
    return get_lnode(lhs) == get_lnode(rhs);
}

// 为端口添加不等比较操作符 (!=)
template<typename T1, typename Dir1, typename T2, typename Dir2>
auto operator!=(const port<T1, Dir1>& lhs, const port<T2, Dir2>& rhs) {
    return get_lnode(lhs) != get_lnode(rhs);
}

// 为端口添加逻辑操作符支持
template<typename T1, typename Dir1, typename T2, typename Dir2>
auto operator&&(const port<T1, Dir1>& lhs, const port<T2, Dir2>& rhs) {
    static_assert(ch_width_v<T1> == 1 && ch_width_v<T2> == 1, 
                  "Logical AND only supported for 1-bit types");
    // 在硬件中，逻辑与和按位与效果相同
    return get_lnode(lhs) & get_lnode(rhs);
}

template<typename T1, typename Dir1, typename T2, typename Dir2>
auto operator||(const port<T1, Dir1>& lhs, const port<T2, Dir2>& rhs) {
    static_assert(ch_width_v<T1> == 1 && ch_width_v<T2> == 1, 
                  "Logical OR only supported for 1-bit types");
    // 在硬件中，逻辑或和按位或效果相同
    return get_lnode(lhs) | get_lnode(rhs);
}

// 端口与常量的操作符支持
template<typename T, typename Dir, typename Lit>
requires std::is_arithmetic_v<Lit>
auto operator&(const port<T, Dir>& lhs, Lit rhs) {
    static_assert(ch_width_v<T> == 1, 
                  "Bitwise AND only supported for 1-bit types");
    return get_lnode(lhs) & rhs;
}

template<typename T, typename Dir, typename Lit>
requires std::is_arithmetic_v<Lit>
auto operator&(Lit lhs, const port<T, Dir>& rhs) {
    static_assert(ch_width_v<T> == 1, 
                  "Bitwise AND only supported for 1-bit types");
    return lhs & get_lnode(rhs);
}

// 添加更多混合操作符支持...

}} // namespace ch::core

#endif // IO_H
