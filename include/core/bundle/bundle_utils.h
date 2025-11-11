// include/core/bundle_utils.h
#ifndef CH_CORE_BUNDLE_UTILS_H
#define CH_CORE_BUNDLE_UTILS_H

#include "bundle_base.h"
#include <type_traits>
#include <tuple>

namespace ch::core {

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

// 序列化Bundle到bits
template<typename BundleT>
auto serialize_to_bits(const BundleT& bundle) {
    static_assert(std::is_base_of_v<bundle_base<BundleT>, BundleT>, 
                  "serialize_to_bits() only works with bundle_base-derived types");
                  
    // 获取Bundle的总宽度
    constexpr unsigned width = get_bundle_width<BundleT>();
    
    // 创建结果类型
    using result_type = ch_uint<width>;
    result_type result{0};
    
    // 遍历所有字段并连接它们
    const auto& fields = bundle.__bundle_fields();
    unsigned offset = 0;
    
    std::apply([&](auto&&... f) {
        (((result = result | (static_cast<lnode<typename std::decay_t<decltype(bundle.*(f.ptr))>::value_type>>(bundle.*(f.ptr)) << offset)),
          offset += ch_width_v<std::decay_t<decltype(bundle.*(f.ptr))>>), ...);
    }, fields);
    
    return result;
}

// 从bits反序列化到Bundle
template<typename BundleT>
BundleT deserialize_from_bits(const auto& bits_value) {
    static_assert(std::is_base_of_v<bundle_base<BundleT>, BundleT>, 
                  "deserialize_from_bits() only works with bundle_base-derived types");
                  
    BundleT bundle{};
    
    // 遍历所有字段并从bits中提取值
    const auto& fields = bundle.__bundle_fields();
    unsigned offset = 0;
    
    std::apply([&](auto&&... f) {
        (((bundle.*(f.ptr) = bits_value.template slice<ch_width_v<std::decay_t<decltype(bundle.*(f.ptr))>>>(offset)), 
          offset += ch_width_v<std::decay_t<decltype(bundle.*(f.ptr))>>), ...);
    }, fields);
    
    return bundle;
}

} // namespace ch::core

#endif // CH_CORE_BUNDLE_UTILS_H