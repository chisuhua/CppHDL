#ifndef CH_CORE_BUNDLE_BASE_H
#define CH_CORE_BUNDLE_BASE_H

#include "core/bundle/bundle_serialization.h"
#include "core/bundle/bundle_traits.h"
#include "core/direction.h"
#include "core/literal.h" // 添加对literal.h的包含
#include "core/logic_buffer.h"
#include <source_location>
#include <string>

namespace ch::core {

// Bundle的角色枚举，用于区分内部连接、主设备和从设备
enum class bundle_role {
    unknown, // 内部连接，可读可写
    master,  // 主设备，输出信号
    slave    // 从设备，输入信号
};

template <typename Derived> class bundle_base : public logic_buffer<Derived> {
protected:
    bundle_role role_ = bundle_role::unknown; // 默认为内部角色

public:
    // (1) 默认构造：创建未驱动的字面量节点并通过assign操作连接
    bundle_base() : logic_buffer<Derived>(){};
    bundle_base(const std::string &prefix) : logic_buffer<Derived>() {
        this->set_name_prefix(prefix);
    }

    // (2) 拷贝构造：共享节点（硬件连接语义）
    bundle_base(const bundle_base &other)
        : logic_buffer<Derived>(other.impl()) {
        // 不再自动同步成员
    }

    // (3) 从底层节点直接构造（用于 <<= 或内部）
    bundle_base(lnodeimpl *node) : logic_buffer<Derived>(node) {
        // 不再自动同步成员
    }

    // (5) 从字面量类型参数构造：创建字面量节点
    // 接受编译时字面量类型，如 0_d, 0x11_h 等
    template <uint64_t V, uint32_t W>
    bundle_base(
        ch_literal_impl<V, W> literal_value,
        const std::string &name = "bundle_lit",
        const std::source_location &sloc = std::source_location::current()) {
        auto runtime_lit = make_literal<V, W>();
        auto *literal_node =
            node_builder::instance().build_literal(runtime_lit, name, sloc);

        this->node_impl_ = literal_node;
    }

    // (6) 从运行时字面量类型构造：创建字面量节点
    bundle_base(
        const ch_literal_runtime &literal_value,
        const std::string &name = "bundle_lit",
        const std::source_location &sloc = std::source_location::current()) {
        auto runtime_lit =
            make_literal(literal_value.value(), literal_value.width());
        auto *literal_node =
            node_builder::instance().build_literal(runtime_lit, name, sloc);

        this->node_impl_ = literal_node;
    }

    virtual ~bundle_base() = default;

    void as_master() {
        set_role(bundle_role::master);
        static_cast<Derived *>(this)->as_master_direction();
    };
    void as_slave() {
        set_role(bundle_role::slave);
        static_cast<Derived *>(this)->as_slave_direction();
    };

    // 连接操作符 - 保持bundle整体连接语义
    Derived &operator<<=(const Derived &src) {
        lnode<Derived> src_lnode = get_lnode(src);
        unsigned W = get_bundle_width<Derived>();
        if (src_lnode.impl()) {
            if (!this->node_impl_) {
                this->node_impl_ =
                    node_builder::instance().build_unary_operation(
                        ch_op::assign, src_lnode, W,
                        src_lnode.impl()->name() + "_bundle");
            } else {
                CHERROR("[bundle::operator<<=] Error: node_impl_ or "
                        "src_lnode is not null for !");
            }
        } else {
            CHERROR("[bundle::operator<<=] Error: node_impl_ or "
                    "src_lnode is null for !");
        }
        return static_cast<Derived &>(*this);
    }

    // 赋值操作符 - 保持一致性
    Derived &operator=(const Derived &other) {
        if (this != &other) {
            this->node_impl_ = other.impl();
        }
        return static_cast<Derived &>(*this);
    }

    // 序列化支持
    // 使用 bundle_serialization.h 中的 deserialize 函数

    // 自动生成 flip()
    std::unique_ptr<Derived> flip() const {
        auto flipped = std::make_unique<Derived>();
        // 在flip时自动设置为slave方向
        if constexpr (requires { flipped->as_slave(); }) {
            flipped->as_slave();
        }
        return flipped;
    }

    // 简化版本
    bool is_valid() const {
        const auto &fields = derived()->__bundle_fields();
        bool valid = true;
        std::apply(
            [&](auto &&...f) {
                ((valid &= check_field_valid(derived()->*(f.ptr))), ...);
            },
            fields);
        return valid;
    }

    // 命名支持
    virtual void set_name_prefix(const std::string &prefix) {
        const auto &fields = derived()->__bundle_fields();
        std::apply(
            [&](auto &&...f) {
                ((set_field_name(derived()->*(f.ptr),
                                 prefix + "." + std::string(f.name))),
                 ...);
            },
            fields);
    }

    // 获取Bundle宽度（使用统一的函数）
    unsigned width() const { return get_bundle_width<Derived>(); }

    // 获取bundle角色
    bundle_role get_role() const { return role_; }

protected:
    // 子类可调用：从节点同步成员字段
    // void sync_members_from_node() {
    //     if (!this->node_impl_)
    //         return;

    //     // 检查当前bundle是否已经初始化，避免递归调用
    //     // 我们可以通过检查impl()是否为有效的节点来判断
    //     if (this->impl() == nullptr)
    //         return;

    //     constexpr unsigned W = get_bundle_width<Derived>();
    //     ch_uint<W> all_bits(this->impl());

    //     // 检查节点是否有效，避免段错误
    //     if (all_bits.impl() == nullptr)
    //         return;

    //     // 使用 bundle_serialization.h 中的 deserialize 函数
    //     Derived temp = deserialize<Derived>(all_bits);

    //     // 将 temp 的字段赋值给 this 的成员
    //     const auto fields = Derived::__bundle_fields();
    //     std::apply(
    //         [&](auto &&...f) {
    //             // 确保在赋值前 temp 对象已正确构造
    //             ((derived()->*(f.ptr) = temp.*(f.ptr)), ...);
    //         },
    //         fields);
    // }

    // 获取派生类指针的辅助函数
    constexpr Derived *derived() { return static_cast<Derived *>(this); }
    constexpr const Derived *derived() const {
        return static_cast<const Derived *>(this);
    }

    // 方向控制辅助函数
    template <typename T> void set_field_direction(T &field, input_direction) {
        if constexpr (requires { field.as_input(); }) {
            field.as_input();
        }
    }

    template <typename T> void set_field_direction(T &field, output_direction) {
        if constexpr (requires { field.as_output(); }) {
            field.as_output();
        }
    }

    // 更新字段验证辅助函数
    template <typename T> bool check_field_valid(const T &field) const {
        if constexpr (bundle_field_traits<T>::is_bundle) {
            return bundle_field_traits<T>::check_valid(field);
        } else if constexpr (requires { field.is_valid(); }) {
            return field.is_valid();
        } else {
            return true; // 基本类型总是有效
        }
    }

    // 更新字段命名辅助函数
    template <typename T>
    void set_field_name(T &field, const std::string &name) {
        if constexpr (bundle_field_traits<T>::is_bundle) {
            bundle_field_traits<T>::set_name(field, name);
        } else if constexpr (requires { field.set_name(name); }) {
            field.set_name(name);
        }
    }

    // 字段方向设置 - 支持任意数量字段 (C++20)
    template <typename... Fields> void make_output(Fields &...fields) {
        (set_field_direction(fields, output_direction{}), ...);
    }

    template <typename... Fields> void make_input(Fields &...fields) {
        (set_field_direction(fields, input_direction{}), ...);
    }

protected:
    void as_master_direction() {
        static_assert(sizeof(Derived) == 0,
                      "Subclass must implement as_master_direction()");
    }

    void as_slave_direction() {
        static_assert(sizeof(Derived) == 0,
                      "Subclass must implement do_slave_direction()");
    }

private:
    void set_role(bundle_role new_role) {
        CHCHECK(role_ == bundle_role::unknown, "Bundle role already set");
        role_ = new_role;

        // 创建总线节点（若尚未存在）
        if (!this->node_impl_) {
            unsigned W = get_bundle_width<Derived>();
            const std::string &name = "bundle_lit";
            auto lit = make_literal(0, W);
            this->node_impl_ =
                node_builder::instance().build_literal(lit, name);
        }

        // 将字段初始化为总线的位切片
        // create_field_slices_from_node();
    }
};

// 特化 ch_width_impl 和 get_lnode（必须在定义后）
// template <typename Derived>
// struct ch_width_impl<Derived, std::enable_if_t<is_bundle_type<Derived>>>
// {
//     static constexpr unsigned value = get_bundle_width<Derived>();
// };

template <typename Derived> inline lnode<Derived> get_lnode(const Derived &b) {
    return lnode<Derived>(b.impl());
}

// 连接两个相同类型的bundle
template <typename BundleT> void connect(BundleT &src, BundleT &dst) {
    static_assert(std::is_base_of_v<bundle_base<BundleT>, BundleT>,
                  "connect() only works with bundle_base-derived types");

    const auto &fields = src.__bundle_fields();
    std::apply([&](auto &&...f) { ((dst.*(f.ptr) = src.*(f.ptr)), ...); },
               fields);
}

} // namespace ch::core

#endif // CH_CORE_BUNDLE_BASE_H