#ifndef CH_CORE_BUNDLE_BASE_H
#define CH_CORE_BUNDLE_BASE_H

#include "bundle_traits.h"
#include "core/bundle/bundle_layout.h"
#include "core/bundle/bundle_serialization.h"
#include "core/bundle/bundle_traits.h"
#include "core/direction.h"
#include "core/literal.h" // 添加对literal.h的包含
#include "core/logic_buffer.h"
#include "core/operators.h" // 添加operators.h的包含
#include <concepts>
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

    // 连接操作符 - 重构以支持字段级别连接，根据字段方向进行连接
    Derived &operator<<=(Derived &src) {
        // 对bundle的每个字段进行单独连接，根据字段方向决定连接方式
        const auto &fields = derived()->__bundle_fields();

        // 使用C++20的pack expansion特性来展开字段连接
        std::apply(
            [this, &src](auto &&...field_ptrs) {
                (connect_field_based_on_direction_and_source(
                     derived()->*(field_ptrs.ptr),
                     src.derived()->*(field_ptrs.ptr)),
                 ...);
            },
            fields);

        return static_cast<Derived &>(*this);
    }
    
    // 一个新的连接函数，处理双向连接场景
    template <typename LeftField, typename RightField>
    void connect_field_based_on_direction_and_source(LeftField &left_field, 
                                                     RightField &right_field) {
        using LeftFieldType = std::remove_const_t<std::remove_reference_t<decltype(left_field)>>;
        using RightFieldType = std::remove_const_t<std::remove_reference_t<decltype(right_field)>>;

        if constexpr (is_bundle_v<RightFieldType>) {
            // 如果字段是bundle类型，递归调用operator<<=
            left_field <<= right_field;
        } else {
            // 对于基本类型字段，根据字段方向决定连接方式
            // 获取左右字段的方向信息
            auto left_dir = left_field.direction();
            auto right_dir = right_field.direction();

            // 根据方向连接：右操作数的输出连接到左操作数的输入
            // 如果右字段是输出方向，左字段是输入方向，则右字段驱动左字段
            if (std::holds_alternative<output_direction>(right_dir) &&
                std::holds_alternative<input_direction>(left_dir)) {
                left_field = right_field;
            }
            // 如果右字段是输入方向，左字段是输出方向，则左字段驱动右字段
            else if (std::holds_alternative<input_direction>(right_dir) &&
                     std::holds_alternative<output_direction>(left_dir)) {
                right_field = left_field;
            }
            // 默认情况下，按原始方式连接（如果方向相同或未指定方向）
            else {
                left_field = right_field;
            }
        }
    }

    // 赋值操作符 - 保持不变
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
        create_field_slices_from_node();
    }

protected:
    // 创建字段的位切片节点
    void create_field_slices_from_node() {
        unsigned offset = 0;
        const auto layout = get_bundle_layout<Derived>();
        std::apply(
            [&](auto &&...field_info) {
                (([&]() {
                     auto &field_ref = derived()->*(field_info.ptr);
                     using FieldType = std::decay_t<decltype(field_ref)>;
                     auto bundle_lnode = get_lnode(*derived());
                     if constexpr (is_bundle_v<FieldType>) {
                         static_assert(!std::is_same_v<FieldType, Derived>,
                                       "Bundle cannot contain itself");
                         constexpr unsigned W = get_bundle_width<FieldType>();
                         if constexpr (W == 0) {
                             static_assert(W != 0,
                                           "Field width cannot be zero!");
                         }

                         // 使用get_lnode获取当前bundle的节点
                         auto field_slice = bits<W>(bundle_lnode, offset);
                         field_ref = FieldType(field_slice.impl());
                         offset += W;
                     } else {
                         constexpr unsigned W = ch_width_v<FieldType>;
                         if constexpr (W == 0) {
                             static_assert(W != 0,
                                           "Field width cannot be zero!");
                         }

                         if constexpr (W == 1) {
                             field_ref = bit_select(bundle_lnode, offset);
                             offset++;
                         } else {
                             field_ref = bits<W>(bundle_lnode, offset);
                             offset += W;
                         }
                     }
                 }()),
                 ...);
            },
            layout);
    }

};

// 特化 ch_width_impl 和 get_lnode（必须在定义后）
template <typename Derived>
struct ch_width_impl<Derived, std::enable_if_t<is_bundle_type<Derived>>> {
    static constexpr unsigned value = get_bundle_width<Derived>();
};

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