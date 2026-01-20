#ifndef CH_CORE_BUNDLE_BASE_H
#define CH_CORE_BUNDLE_BASE_H

#include "core/logic_buffer.h"
#include "core/bundle/bundle_traits.h" // for get_bundle_width, __bundle_fields
#include "core/bundle/bundle_serialization.h" // 添加对bundle_serialization.h的包含
#include "core/direction.h"
#include <string>

namespace ch::core {

// 为Bundle字段添加特殊处理
template <typename Derived> struct bundle_field_traits {
    static constexpr bool is_bundle = false;
};

// 特化：识别Bundle类型
template <typename T>
concept is_bundle_type = requires(T t) {
    t.__bundle_fields();
    t.is_valid();
    t.as_master();
    t.as_slave();
};

// 为Bundle类型提供特化
template <typename BundleType>
    requires is_bundle_type<BundleType>
struct bundle_field_traits<BundleType> {
    static constexpr bool is_bundle = true;

    // 递归验证嵌套Bundle
    static bool check_valid(const BundleType &bundle) {
        return bundle.is_valid();
    }

    // 递归命名嵌套Bundle
    static void set_name(BundleType &bundle, const std::string &name) {
        bundle.set_name_prefix(name);
    }
};

template <typename Derived> class bundle_base : public logic_buffer<Derived> {
public:
    using Self = Derived;

    // (1) 默认构造：创建未驱动的字面量节点并通过assign操作连接
    bundle_base() {
        constexpr unsigned W = get_bundle_width<Derived>();
        // 创建一个零值字面量作为默认值
        sdata_type default_value = constants::zero(W);
        auto *literal_node = node_builder::instance().build_literal(
            default_value, "unnamed_bundle_literal");
        
        // 创建assign操作节点，将字面量值连接到bundle节点
        this->node_impl_ = node_builder::instance().build_unary_operation(
            ch_op::assign, lnode<ch_uint<W>>(literal_node), W, "unnamed_bundle");
            
        // 不再自动同步成员，以避免段错误
    }

    // (2) 拷贝构造：共享节点（硬件连接语义）
    bundle_base(const bundle_base &other)
        : logic_buffer<Derived>(other.impl()) {
        // 不再自动同步成员
    }

    // (3) 从底层节点直接构造（用于 <<= 或内部）
    explicit bundle_base(lnodeimpl *node) : logic_buffer<Derived>(node) {
        // 不再自动同步成员
    }

    // (4) 从 ch_uint<W> 反序列化（复用其节点）
    template <unsigned W> explicit bundle_base(const ch_uint<W> &bits) {
        static_assert(W == get_bundle_width<Derived>(),
                      "Bundle width mismatch");
        this->node_impl_ = bits.impl(); // 直接复用节点！
        // 不再自动同步成员
    }

    // (5) 从字面量类型参数构造：创建字面量节点
    explicit bundle_base(const sdata_type &literal_value) {
        constexpr unsigned W = get_bundle_width<Derived>();
        auto *literal_node = node_builder::instance().build_literal(
            literal_value, "literal_bundle_literal");
        
        // 创建assign操作节点，将字面量值连接到bundle节点
        this->node_impl_ = node_builder::instance().build_unary_operation(
            ch_op::assign, lnode<ch_uint<W>>(literal_node), W, "literal_bundle");
            
        // 不再自动同步成员
    }

    virtual ~bundle_base() = default;

    // 虚函数，子类需要实现
    virtual void as_master() = 0;
    virtual void as_slave() = 0;

    // 连接操作符 - 保持bundle整体连接语义
    Derived &operator<<=(const Derived &src) {
        this->node_impl_ = src.impl();
        // 移除sync_members_from_node()调用以避免段错误
        return static_cast<Derived &>(*this);
    }

    // 赋值操作符 - 保持一致性
    Derived &operator=(const Derived &other) {
        if (this != &other) {
            this->node_impl_ = other.impl();
            // 移除sync_members_from_node()调用以避免段错误
        }
        return static_cast<Derived &>(*this);
    }

    // 序列化支持
    auto to_bits() const {
        // 使用 bundle_traits 中的通用 serialize 逻辑
        constexpr unsigned W = get_bundle_width<Derived>();
        return ch_uint<W>(this->impl());
    }

    static Derived from_bits(const ch_uint<get_bundle_width<Derived>()> &bits) {
        // 使用 bundle_serialization.h 中的 deserialize 函数
        return deserialize<Derived>(bits);
    }

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
    constexpr unsigned width() const { return get_bundle_width<Derived>(); }

protected:
    // 子类可调用：从节点同步成员字段
    void sync_members_from_node() {
        if (!this->node_impl_) return;

        // 检查当前bundle是否已经初始化，避免递归调用
        // 我们可以通过检查impl()是否为有效的节点来判断
        if (this->impl() == nullptr) return;

        constexpr unsigned W = get_bundle_width<Derived>();
        ch_uint<W> all_bits(this->impl());
        
        // 检查节点是否有效，避免段错误
        if (all_bits.impl() == nullptr) return;
        
        // 使用 bundle_serialization.h 中的 deserialize 函数
        Derived temp = deserialize<Derived>(all_bits);

        // 将 temp 的字段赋值给 this 的成员
        const auto fields = Derived::__bundle_fields();
        std::apply([&](auto &&...f) {
            // 确保在赋值前 temp 对象已正确构造
            ((derived()->*(f.ptr) = temp.*(f.ptr)), ...);
        }, fields);
    }

    // 获取派生类指针的辅助函数
    constexpr Derived *derived() { return static_cast<Derived *>(this); }
    constexpr const Derived *derived() const {
        return static_cast<const Derived *>(this);
    }

    // 辅助：打包当前字段为 ch_uint<W>
    ch_uint<get_bundle_width<Derived>()> pack_current_fields() const {
        return this->to_bits();
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

    // 字段方向设置 - 支持1-4个字段
    template <typename Field1> void make_output(Field1 &field1) {
        set_field_direction(field1, output_direction{});
    }

    template <typename Field1, typename Field2>
    void make_output(Field1 &field1, Field2 &field2) {
        set_field_direction(field1, output_direction{});
        set_field_direction(field2, output_direction{});
    }

    template <typename Field1, typename Field2, typename Field3>
    void make_output(Field1 &field1, Field2 &field2, Field3 &field3) {
        set_field_direction(field1, output_direction{});
        set_field_direction(field2, output_direction{});
        set_field_direction(field3, output_direction{});
    }

    template <typename Field1, typename Field2, typename Field3,
              typename Field4>
    void make_output(Field1 &field1, Field2 &field2, Field3 &field3,
                     Field4 &field4) {
        set_field_direction(field1, output_direction{});
        set_field_direction(field2, output_direction{});
        set_field_direction(field3, output_direction{});
        set_field_direction(field4, output_direction{});
    }

    template <typename Field1> void make_input(Field1 &field1) {
        set_field_direction(field1, input_direction{});
    }

    template <typename Field1, typename Field2>
    void make_input(Field1 &field1, Field2 &field2) {
        set_field_direction(field1, input_direction{});
        set_field_direction(field2, input_direction{});
    }

    template <typename Field1, typename Field2, typename Field3>
    void make_input(Field1 &field1, Field2 &field2, Field3 &field3) {
        set_field_direction(field1, input_direction{});
        set_field_direction(field2, input_direction{});
        set_field_direction(field3, input_direction{});
    }

    template <typename Field1, typename Field2, typename Field3,
              typename Field4>
    void make_input(Field1 &field1, Field2 &field2, Field3 &field3,
                    Field4 &field4) {
        set_field_direction(field1, input_direction{});
        set_field_direction(field2, input_direction{});
        set_field_direction(field3, input_direction{});
        set_field_direction(field4, input_direction{});
    }
};

// 特化 ch_width_impl 和 get_lnode（必须在定义后）
template <typename Derived>
struct ch_width_impl<Derived, std::enable_if_t<is_bundle_type<Derived>>> {
    static constexpr unsigned value = get_bundle_width<Derived>();
};

template <typename Derived>
inline lnode<Derived> get_lnode(const Derived& b) {
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