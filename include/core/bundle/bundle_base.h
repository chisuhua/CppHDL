#pragma once

#include "ast/ast_nodes.h"
#include "bundle_traits.h"
#include "core/bundle/bundle_layout.h"
#include "core/bundle/bundle_serialization.h"
#include "core/bundle/bundle_traits.h"
#include "core/context.h" // create_node() 需要 context 的完整类型
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

    // (7) BUGFIX (fix-bundle-base-null-ptr-hijack): SFINAE-restricted
    // integer literal ctor. Without this, `bundle_base<Derived>(0)`
    // matches `bundle_base(lnodeimpl *node)` via null pointer
    // conversion, creating a node with node_impl_ = nullptr.
    //
    // Through `using bundle_base<Self>::bundle_base;` in the
    // CH_BUNDLE_FIELDS_T macro (bundle_meta.h:155), this ctor is
    // inherited by all 22 bundle derived classes (ch_stream, ch_flow,
    // all AXI/axi_lite channels, fifo_bundle, interrupt_bundle,
    // config_bundle, ch_fragment, bundle_slice_view, bundle_concat).
    //
    // `bool` is excluded (mirror P0 ch_bool pattern) so that
    // `bundle_base<Derived>(false)` doesn't become ambiguous.
    //
    // Identity match for integer args wins over the standard-conversion
    // null pointer path. The existing `bundle_base(lnodeimpl *node)`
    // ctor remains available for legitimate lnodeimpl* arguments.
    template <typename IntT,
              typename = std::enable_if_t<
                  std::is_integral_v<std::decay_t<IntT>> &&
                  !std::is_same_v<std::decay_t<IntT>, bool>>>
    explicit bundle_base(
        IntT v, const std::string &name = "bundle_lit",
        const std::source_location &sloc = std::source_location::current())
        : logic_buffer<Derived>() {
        ch_literal_runtime lit(static_cast<std::uint64_t>(v));
        this->node_impl_ =
            node_builder::instance().build_literal(lit, name, sloc);
        if (!this->node_impl_) {
            CHERROR("[bundle_base] Failed to create literal node "
                    "from integer value");
        }
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
            // ⚠️ MUST 用 `operator<<=`（**数据流边**）而不是 `operator=`
            // （**节点重绑定**）—— 后者只改指针，会把字段原有的驱动**孤儿化**
            // （该节点从设计图里消失），而新指向的节点没有驱动 ⇒ 连接静默失效。
            // （OpenSpec change `bundle-master-slave-io-2026-10` 的 D3）
            if (std::holds_alternative<output_direction>(right_dir) &&
                std::holds_alternative<input_direction>(left_dir)) {
                left_field <<= right_field;
            }
            // 如果右字段是输入方向，左字段是输出方向，则左字段驱动右字段
            else if (std::holds_alternative<input_direction>(right_dir) &&
                     std::holds_alternative<output_direction>(left_dir)) {
                right_field <<= left_field;
            }
            // 默认情况下（方向相同或未指定方向）：仍 MUST 建立数据流边
            else {
                left_field <<= right_field;
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
        // 允许设置或改变 role
        role_ = new_role;

        // 创建总线节点（若尚未存在）
        //
        // ⚠️ 该总线节点**不再**是字段的来源 —— 字段现在是**一等节点**
        // （见 `create_field_proxy_nodes()`）。它保留下来是为了
        // ① `bundle_base::impl()` 始终非空（既有测试断言这一点）；
        // ② 打包 / 序列化路径仍需要一个「整体」节点。
        if (!this->node_impl_) {
            unsigned W = get_bundle_width<Derived>();
            const std::string &name = "bundle_lit";
            auto lit = make_literal(0, W);
            this->node_impl_ =
                node_builder::instance().build_literal(lit, name);
        }

        // 为每个叶子字段建立**自己的**节点（值代理）
        create_field_proxy_nodes();
    }

protected:
    // 为每个叶子字段创建一个**独立的无 src 值代理**节点。
    //
    // 设计要点（OpenSpec change `bundle-master-slave-io-2026-10` 的 D0/D7）：
    //   - 字段 MUST 是一等节点：它拥有自己的节点身份，不依赖「总线 + 位切片」。
    //     旧实现把字段**重绑定**成零字面量总线的 `bit_select` / `bits<>` 切片 ⇒
    //     设计**只读不写**的字段恒为 0（切片读的是那条无人驱动的零字面量总线）。
    //   - 节点种类 MUST 是 `proxyimpl`（值代理）：
    //     ① 无 src 时没有指令 ⇒ 槽位保持确定的初值 0（可寻址、确定）；
    //     ② 被 `set_src(0, ...)` 驱动后，`instr_proxy` 把源的值搬进本节点；
    //     ③ codegen 对有 src 的代理发射 `assign field = <src>;`（不是悬空 wire）。
    void create_field_proxy_nodes() {
        const auto layout = get_bundle_layout<Derived>();
        std::apply(
            [&](auto &&...field_info) {
                (([&]() {
                     auto &field_ref = derived()->*(field_info.ptr);
                     using FieldType = std::decay_t<decltype(field_ref)>;
                     if constexpr (is_bundle_v<FieldType>) {
                         // 嵌套 bundle：它**自己**的字段由它自己的
                         // `as_master()` / `as_slave()` 建立 ⇒ 这里不动它。
                         static_assert(!std::is_same_v<FieldType, Derived>,
                                       "Bundle cannot contain itself");
                     } else {
                         constexpr unsigned W = ch_width_v<FieldType>;
                         static_assert(W != 0, "Field width cannot be zero!");
                         // ⚠️ MUST **幂等**：字段已有节点时 MUST NOT 重建 ——
                         // 否则 `as_master()` 之后再 `as_slave()` 会换掉字段的
                         // **节点身份** ⇒ 已建立的数据流边仍指向被孤儿化的旧节点
                         // ⇒ 连接静默失效。
                         // （节点身份 MUST 稳定，见 ADR-042 Decision 1）
                         if (field_ref.impl() != nullptr) {
                             return;
                         }
                         auto *ctx = this->node_impl_->ctx();
                         // `ctx` 的类型依赖 Derived ⇒ 需要 `template` 关键字
                         auto *field_proxy =
                             ctx->template create_node<proxyimpl>(
                                 W, std::string(field_info.name),
                                 std::source_location::current());
                         field_ref = FieldType(field_proxy);
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

    // ⚠️ MUST **方向感知**：旧实现无条件 `dst.field = src.field` ⇒ 对 master/slave
    // 对会把 slave 的**输出**（full/empty）用 master 的**未驱动输入**覆盖。
    // 参数顺序约定：`connect(src, dst)` ≡
    //     `dst.connect_field_based_on_direction_and_source(dst.field, src.field)`
    // （第三分支「同方向」下两种顺序结果相反，故必须写明）
    // （OpenSpec change `bundle-master-slave-io-2026-10` 的 D4）
    const auto &fields = src.__bundle_fields();
    std::apply(
        [&](auto &&...f) {
            (dst.connect_field_based_on_direction_and_source(dst.*(f.ptr),
                                                             src.*(f.ptr)),
             ...);
        },
        fields);
}

} // namespace ch::core
