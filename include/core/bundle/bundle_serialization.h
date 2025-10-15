// include/core/bundle_serialization.h
#ifndef CH_CORE_BUNDLE_SERIALIZATION_H
#define CH_CORE_BUNDLE_SERIALIZATION_H

#include "bundle_base.h"
#include "bundle_meta.h"
#include "bundle_traits.h"  // 包含统一的宽度计算
#include "uint.h"
#include <tuple>
#include <type_traits>

namespace ch::core {

// 序列化Bundle到位向量
template<typename BundleT>
auto serialize(const BundleT& bundle) {
    static_assert(is_bundle_v<BundleT>, "serialize() only works with Bundle types");
    
    constexpr unsigned total_width = bundle_width_v<BundleT>;
    static_assert(total_width > 0, "Bundle has zero width");
    
    // 创建结果位向量
    ch_uint<total_width> result;
    
    // TODO: 实际的序列化逻辑需要访问字段的底层节点
    // 这里需要更复杂的实现来连接IR节点
    
    return result;
}

// 从位向量反序列化到Bundle
template<typename BundleT, unsigned Width>
BundleT deserialize(const ch_uint<Width>& bits) {
    static_assert(is_bundle_v<BundleT>, "deserialize() only works with Bundle types");
    static_assert(Width == bundle_width_v<BundleT>, "Bit width mismatch");
    
    BundleT bundle;
    
    // TODO: 实际的反序列化逻辑需要设置字段的底层节点
    // 这里需要更复杂的实现来连接IR节点
    
    return bundle;
}

// Bundle到位向量的视图（不复制数据）
template<typename BundleT>
struct bundle_bits_view {
    BundleT& bundle;
    static constexpr unsigned width = bundle_width_v<BundleT>;
    
    explicit bundle_bits_view(BundleT& b) : bundle(b) {}
    
    // 转换为位向量
    template<unsigned W = bundle_width_v<BundleT>>
    operator ch_uint<W>() const {
        return serialize(bundle);
    }
};

// 工厂函数
template<typename BundleT>
auto to_bits(BundleT& bundle) {
    return bundle_bits_view<BundleT>(bundle);
}

template<typename BundleT, unsigned Width>
void from_bits(const ch_uint<Width>& bits, BundleT& bundle) {
    static_assert(Width == bundle_width_v<BundleT>, "Width mismatch");
    // TODO: 实现反序列化逻辑
}

} // namespace ch::core

#endif // CH_CORE_BUNDLE_SERIALIZATION_H
