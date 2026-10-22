// include/core/bundle_utils.h
#pragma once

#include "bundle_base.h"  // 需要 bundle_base（detach_field_nodes）
#include "bundle_serialization.h"  // Include the main serialization header

namespace ch::core {

// Master/Slave 工厂函数
//
// ⚠️ 契约（OpenSpec change `bundle-ecosystem-hardening-2026-10` 的 D4）：
//   工厂 MUST 产出**字段节点独立**的 bundle —— 每个字段节点 MUST NOT 与源
//   bundle 的对应字段共享节点。
//   拷贝构造会**共享**字段节点 ⇒ 直接 `as_master()` 会让副本与原件共享
//   字段节点 ⇒ MUST 先 `detach_field_nodes()`。
//   ⚠️ **总线节点**按拷贝构造语义**仍然共享**（既有测试
//   `tests/test_bundle_node_management.cpp` 钉住该语义）⇒ 本契约**只**约束
//   **字段**节点，MUST NOT 要求总线节点独立。
//   ⚠️ 现状下 12 个调用点全部传**临时对象**（源字段无节点）⇒ 解绑对它们是
//   no-op ⇒ **零行为变化**；本契约是**面向未来**的（调用方可能传入已设角色
//   的 bundle）。
template <typename BundleT> BundleT master(BundleT bundle) {
    static_assert(std::is_base_of_v<bundle_base<BundleT>, BundleT>,
                  "master() only works with bundle_base-derived types");
    bundle.detach_field_nodes();
    bundle.as_master();
    return bundle;
}

template <typename BundleT> BundleT slave(BundleT bundle) {
    static_assert(std::is_base_of_v<bundle_base<BundleT>, BundleT>,
                  "slave() only works with bundle_base-derived types");
    bundle.detach_field_nodes();
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
