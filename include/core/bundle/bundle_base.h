#ifndef CH_CORE_BUNDLE_BASE_H
#define CH_CORE_BUNDLE_BASE_H

#include "bundle.h"
#include "bundle_traits.h"
#include "direction.h"
#include "uint.h"
#include <memory>
#include <string>
#include <tuple>

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

template <typename Derived> class bundle_base : public bundle {
public:
    virtual ~bundle_base() = default;

    // 自动生成 flip()
    std::unique_ptr<bundle> flip() const override {
        auto flipped = std::make_unique<Derived>();
        // 在flip时自动设置为slave方向
        if constexpr (requires { flipped->as_slave(); }) {
            flipped->as_slave();
        }
        return flipped;
    }

    // 简化版本
    bool is_valid() const override {
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

    // 序列化支持
    template <unsigned W = 0> // 默认模板参数改为0
    ch_uint<W> to_bits() const {
        constexpr unsigned actual_width = get_bundle_width<Derived>();
        static_assert(actual_width > 0, "Bundle has zero width");
        static_assert(
            W == 0 || W == actual_width,
            "Template width parameter must match actual bundle width");
        if constexpr (W == 0) {
            return serialize(static_cast<const Derived &>(*this));
        } else {
            return serialize(static_cast<const Derived &>(*this));
        }
    }

    // 反序列化支持
    template <unsigned W = 0> // 默认模板参数改为0
    static Derived from_bits(const ch_uint<W> &bits) {
        constexpr unsigned actual_width = get_bundle_width<Derived>();
        static_assert(actual_width > 0, "Bundle has zero width");
        static_assert(
            W == 0 || W == actual_width,
            "Template width parameter must match actual bundle width");
        return deserialize<Derived>(bits);
    }

protected:
    virtual void as_master() = 0;
    virtual void as_slave() = 0;

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

protected:
    constexpr Derived *derived() { return static_cast<Derived *>(this); }
    constexpr const Derived *derived() const {
        return static_cast<const Derived *>(this);
    }
};

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