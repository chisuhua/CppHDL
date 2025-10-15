// include/core/bundle_utils.h
#ifndef CH_CORE_BUNDLE_UTILS_H
#define CH_CORE_BUNDLE_UTILS_H

#include "bundle_base.h"
#include <type_traits>
#include <tuple>

namespace ch::core {

// 连接两个相同类型的bundle
template<typename BundleT>
void connect(const BundleT& src, BundleT& dst) {
    static_assert(std::is_base_of_v<bundle_base<BundleT>, BundleT>, 
                  "connect() only works with bundle_base-derived types");
    
    const auto& fields = src.__bundle_fields();
    std::apply([&](auto&&... f) {
        ((dst.*(f.ptr) = src.*(f.ptr)), ...);
    }, fields);
}

// Master/Slave 工厂函数
template<typename BundleT>
BundleT master(BundleT bundle) {
    bundle.as_master();
    return bundle;
}

template<typename BundleT>
BundleT slave(BundleT bundle) {
    bundle.as_slave();
    return bundle;
}

} // namespace ch::core

#endif // CH_CORE_BUNDLE_UTILS_H
