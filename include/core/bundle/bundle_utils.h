// include/core/bundle_utils.h
#ifndef CH_CORE_BUNDLE_UTILS_H
#define CH_CORE_BUNDLE_UTILS_H

#include "bundle_base.h"
#include "uint.h"
#include "bool.h"
#include <type_traits>
#include <tuple>
#include <vector>
#include <cstring>

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
template<typename BundleT, typename BitsType>
BundleT deserialize_from_bits(const BitsType& bits_value) {
    static_assert(std::is_base_of_v<bundle_base<BundleT>, BundleT>, 
                  "deserialize_from_bits() only works with bundle_base-derived types");
                  
    BundleT bundle{};
    
    // 遍历所有字段并从bits中提取值
    const auto& fields = bundle.__bundle_fields();
    unsigned offset = 0;
    
    std::apply([&](auto&&... f) {
        (((bundle.*(f.ptr) = bits(bits_value, offset + ch_width_v<std::decay_t<decltype(bundle.*(f.ptr))>> - 1, offset)), 
          offset += ch_width_v<std::decay_t<decltype(bundle.*(f.ptr))>>), ...);
    }, fields);
    
    return bundle;
}

// POD结构体与Bundle之间的转换工具
namespace detail {
    // 检查类型是否为POD
    template<typename T>
    struct is_pod_struct {
        static constexpr bool value = std::is_standard_layout_v<T> && std::is_trivial_v<T>;
    };
    
    // 将POD结构体序列化为uint64_t数组（处理任意大小的POD）
    template<typename T>
    std::vector<uint64_t> serialize_pod_to_uint64_array(const T& pod) {
        static_assert(std::is_standard_layout_v<T> && std::is_trivial_v<T>, 
                      "Type must be POD");
        
        size_t byte_size = sizeof(T);
        size_t uint64_count = (byte_size + sizeof(uint64_t) - 1) / sizeof(uint64_t); // 向上取整
        
        std::vector<uint64_t> result(uint64_count, 0);
        std::memcpy(result.data(), &pod, byte_size);
        
        return result;
    }
    
    // 从uint64_t数组反序列化为POD结构体
    template<typename T>
    T deserialize_pod_from_uint64_array(const std::vector<uint64_t>& data) {
        static_assert(std::is_standard_layout_v<T> && std::is_trivial_v<T>, 
                      "Type must be POD");
        
        T result{};
        size_t byte_size = std::min(sizeof(result), data.size() * sizeof(uint64_t));
        std::memcpy(&result, data.data(), byte_size);
        
        return result;
    }
}

} // namespace ch::core

#endif // CH_CORE_BUNDLE_UTILS_H