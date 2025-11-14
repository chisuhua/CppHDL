// include/core/bundle_utils.h
#ifndef CH_CORE_BUNDLE_UTILS_H
#define CH_CORE_BUNDLE_UTILS_H

#include "bundle_serialization.h"  // Include the main serialization header

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

// Remove the compatibility interface functions - they are now in bundle_serialization.h

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