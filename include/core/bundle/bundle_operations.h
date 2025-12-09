#ifndef CH_CORE_BUNDLE_OPERATIONS_H
#define CH_CORE_BUNDLE_OPERATIONS_H

#include "bundle_base.h"
#include <string>
#include <tuple>

namespace ch::core {

// Bundle字段提取 (bundle_slice)
template <size_t Start, size_t Count, typename BundleT>
struct bundle_slice_view
    : public bundle_base<bundle_slice_view<Start, Count, BundleT>> {
    BundleT &source_bundle;

    explicit bundle_slice_view(BundleT &bundle) : source_bundle(bundle) {}

    // 获取源Bundle的字段子集
    constexpr auto __bundle_fields() const {
        const auto &all_fields = source_bundle.__bundle_fields();
        return slice_fields<Start, Count>(all_fields);
    }

    template <size_t S, size_t C, typename Tuple>
    constexpr auto slice_fields(const Tuple &tuple) const {
        if constexpr (C == 1) {
            return std::make_tuple(std::get<S>(tuple));
        } else if constexpr (C == 2) {
            return std::make_tuple(std::get<S>(tuple), std::get<S + 1>(tuple));
        } else if constexpr (C == 3) {
            return std::make_tuple(std::get<S>(tuple), std::get<S + 1>(tuple),
                                   std::get<S + 2>(tuple));
        } else {
            static_assert(C <= 3,
                          "Currently only supports up to 3 fields in slice");
            return std::make_tuple(); // fallback
        }
    }

    void as_master() override {
        // 简化实现 - 实际应该设置子集字段方向
    }

    void as_slave() override {
        // 简化实现
    }

    bool is_valid() const override {
        return false; // first.is_valid() && second.is_valid();
    }

    void set_name_prefix(const std::string &prefix) override {}
};

// Bundle连接 (bundle_cat) - 简化版本
template <typename Bundle1, typename Bundle2>
struct bundle_concat : public bundle_base<bundle_concat<Bundle1, Bundle2>> {
    Bundle1 first;
    Bundle2 second;

    bundle_concat() = default;
    bundle_concat(Bundle1 b1, Bundle2 b2)
        : first(std::move(b1)), second(std::move(b2)) {}

    bundle_concat(const std::string &prefix) { this->set_name_prefix(prefix); }

    constexpr auto __bundle_fields() const {
        return std::tuple_cat(first.__bundle_fields(),
                              second.__bundle_fields());
    }

    void as_master() override {
        first.as_master();
        second.as_master();
    }

    void as_slave() override {
        first.as_slave();
        second.as_slave();
    }

    bool is_valid() const override {
        return first.is_valid() && second.is_valid();
    }

    void set_name_prefix(const std::string &prefix) override {
        first.set_name_prefix(prefix + ".first");
        second.set_name_prefix(prefix + ".second");
    }
};

// 工厂函数
template <size_t Start, size_t Count, typename BundleT>
auto bundle_slice(BundleT &bundle) {
    return bundle_slice_view<Start, Count, BundleT>(bundle);
}

template <typename Bundle1, typename Bundle2>
auto bundle_cat(Bundle1 b1, Bundle2 b2) {
    return bundle_concat<Bundle1, Bundle2>(std::move(b1), std::move(b2));
}

} // namespace ch::core

#endif // CH_CORE_BUNDLE_OPERATIONS_H
