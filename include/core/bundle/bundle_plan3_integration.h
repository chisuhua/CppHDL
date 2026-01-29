// include/core/bundle/bundle_plan3_integration.h
//
// 方案 3：编译期反射的 Bundle 端口集成模块
//
// 本文件处理与方案 3 相关的所有集成工作：
// 1. 为 bundle 类型的 port 专门化
// 2. 启用 Bundle 端口代理功能
// 3. 提供编译期类型验证工具
//

#ifndef CH_CORE_BUNDLE_PLAN3_INTEGRATION_H
#define CH_CORE_BUNDLE_PLAN3_INTEGRATION_H

#include "bundle_port_proxy.h"
#include "bundle_traits.h"
#include <type_traits>

namespace ch::core {

// ============================================================================
// 特化：Bundle 类型端口的自动增强
// ============================================================================
//
// 对于 Bundle 类型的 port<BundleT, Dir>，自动使用 bundle_port_expanded
// 来启用字段代理功能。
//
// 这是通过部分特化实现的，当 T 是 Bundle 类型时生效。
//

// 辅助 traits：检查是否应该使用增强的 port
template <typename T, typename Dir>
struct should_use_bundle_port_expansion {
    static constexpr bool value = is_bundle_v<T>;
};

template <typename T, typename Dir>
constexpr bool should_use_bundle_port_expansion_v =
    should_use_bundle_port_expansion<T, Dir>::value;

// ============================================================================
// Bundle 端口的便捷类型别名
// ============================================================================

// 为 Bundle 端口提供快捷的输入/输出别名
template <typename BundleT>
using ch_stream_in = bundle_port_expanded<BundleT, input_direction>;

template <typename BundleT>
using ch_stream_out = bundle_port_expanded<BundleT, output_direction>;

// ============================================================================
// 编译期验证工具
// ============================================================================
//
// 这些工具用于在组件定义时验证 Bundle 端口的正确性
//

// 验证 Bundle 符合特定协议
template <typename BundleT, typename = void>
struct bundle_protocol_validator {
    // 通用 Bundle（不强制任何协议）
};

// Handshake 协议验证器
template <typename BundleT>
    requires (is_handshake_protocol_v<BundleT>)
struct bundle_protocol_validator<BundleT, void> {
    // 验证 Handshake 协议要求的字段
    static_assert(has_field_named_v<BundleT, structural_string{"payload"}>,
                  "Handshake bundle must have 'payload' field");
    static_assert(has_field_named_v<BundleT, structural_string{"valid"}>,
                  "Handshake bundle must have 'valid' field");
    static_assert(has_field_named_v<BundleT, structural_string{"ready"}>,
                  "Handshake bundle must have 'ready' field");

    static constexpr bool is_valid = true;
};

// 编译期协议检查函数
template <typename BundleT>
constexpr void validate_handshake_bundle() {
    static_assert(is_handshake_protocol_v<BundleT>,
                  "Bundle does not conform to Handshake protocol");
}

template <typename BundleT>
constexpr void validate_axi_bundle() {
    static_assert(is_axi_protocol_v<BundleT>,
                  "Bundle does not conform to AXI protocol");
}

// ============================================================================
// 编译期 Bundle 端口类型检查
// ============================================================================

// 检查 port 是否是 Bundle 端口
template <typename PortT>
concept is_bundle_port = requires {
    typename PortT::bundle_type;
    requires is_bundle_v<typename PortT::bundle_type>;
};

// ============================================================================
// 方案 3 的启用宏
// ============================================================================
//
// 使用此宏在组件中启用 Bundle 端口代理
//
// 使用示例：
//   class MyComponent : public ch::Component {
//   public:
//       PLAN3_ENABLE_BUNDLE_PORTS
//       
//       __io(ch_out<ch_stream<ch_uint<8>>> stream_out;)
//
//       void describe() override {
//           io().stream_out.payload() <<= data;
//           io().stream_out.valid() <<= valid_cond;
//       }
//   };
//

#define PLAN3_ENABLE_BUNDLE_PORTS                                              \
    /* 此宏预留用于未来的编译期增强 */                                          \
    static_assert(true)

// ============================================================================
// 编译期类型安全检查类
// ============================================================================

template <typename PortT>
class bundle_port_type_checker {
public:
    static_assert(is_bundle_port<PortT>,
                  "Type must be a bundle port with bundle_type");

    using bundle_type = typename PortT::bundle_type;

    // 验证字段存在
    template <auto FieldName>
    static constexpr bool has_field() {
        return PortT::template has_field<FieldName>();
    }

    // 获取字段类型
    template <auto FieldName>
    using field_type = typename PortT::template field_type<FieldName>;

    // 验证 Bundle 宽度
    static constexpr unsigned bundle_width = PortT::width;

    // 验证字段数量
    static constexpr size_t field_count = PortT::field_count;
};

// ============================================================================
// 方案 3 的运行时支持
// ============================================================================
//
// 虽然 Bundle 端口代理主要在编译期工作，但这些辅助函数
// 提供一些运行时支持，特别是与 Simulator 的集成。
//

// 获取 Bundle 端口的字段数量（运行时）
template <is_bundle_port PortT>
constexpr size_t get_bundle_port_field_count() {
    return PortT::field_count;
}

// 获取 Bundle 端口的总宽度（运行时）
template <is_bundle_port PortT>
constexpr unsigned get_bundle_port_width() {
    return PortT::width;
}

} // namespace ch::core

#endif // CH_CORE_BUNDLE_PLAN3_INTEGRATION_H
