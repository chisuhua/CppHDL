#ifndef CH_CORE_BUNDLE_OPERATIONS_H
#define CH_CORE_BUNDLE_OPERATIONS_H

#include "bundle_base.h"
#include "bundle_meta.h"
#include "core/bool.h"
#include "core/uint.h"
#include <array>
#include <type_traits>

namespace ch::core {

// 从bundle中选择一部分字段组成新的bundle
template <size_t Start, size_t Count, typename BundleT>
struct bundle_slice_view
    : public bundle_base<bundle_slice_view<Start, Count, BundleT>> {
    using Self = bundle_slice_view<Start, Count, BundleT>;
    BundleT original_bundle;

    bundle_slice_view(BundleT &bundle) : original_bundle(bundle) {}

    CH_BUNDLE_FIELDS_T(original_bundle)

    void as_master_direction() {
        // Master: 将原始bundle设置为主模式
        original_bundle.as_master();
    }

    void as_slave_direction() {
        // Slave: 将原始bundle设置为从模式
        original_bundle.as_slave();
    }

    // 获取源Bundle的字段子集
    // constexpr auto __bundle_fields() const {
    //     const auto &all_fields = original_bundle.__bundle_fields();
    //     return slice_fields<Start, Count>(all_fields);
    // }

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
};

// 连接两个bundle
template <typename Bundle1, typename Bundle2>
struct bundle_concat : public bundle_base<bundle_concat<Bundle1, Bundle2>> {
    using Self = bundle_concat<Bundle1, Bundle2>;
    Bundle1 bundle1;
    Bundle2 bundle2;

    bundle_concat(Bundle1 &b1, Bundle2 &b2) : bundle1(b1), bundle2(b2) {}

    CH_BUNDLE_FIELDS_T(bundle1, bundle2)

    void as_master_direction() {
        // Master: 将两个bundle都设置为主模式
        bundle1.as_master();
        bundle2.as_master();
    }

    void as_slave_direction() {
        // Slave: 将两个bundle都设置为从模式
        bundle1.as_slave();
        bundle2.as_slave();
    }
    void set_name_prefix(const std::string &prefix) override {
        bundle1.set_name_prefix(prefix + ".first");
        bundle2.set_name_prefix(prefix + ".second");
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
