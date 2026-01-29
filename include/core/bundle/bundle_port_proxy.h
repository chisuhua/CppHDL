// include/core/bundle/bundle_port_proxy.h
//
// Bundle IO 编译期反射方案 3 的核心实现
// 提供 Bundle 端口的字段代理和零开销访问接口
//
// 特性：
// - 编译期完全验证（字段存在、类型正确、宽度匹配）
// - 零运行时开销（代理完全内联）
// - 100% 向后兼容（可选特性）
// - 支持 Handshake/AXI 等标准协议
//

#ifndef CH_CORE_BUNDLE_PORT_PROXY_H
#define CH_CORE_BUNDLE_PORT_PROXY_H

#include "bundle_layout.h"
#include "bundle_protocol.h"
#include "bundle_traits.h"
#include <type_traits>

namespace ch::core {

// ============================================================================
// 第一部分：Bundle 端口字段代理
// ============================================================================
//
// bundle_port_field_proxy 为 Bundle 端口的单个字段提供访问接口
// 支持：读取、写入、HDL 连接、调试信息
//
// 使用示例：
//   auto proxy = port.get_field<"payload"_ss>();
//   auto value = (uint64_t)proxy;          // 读取
//   proxy <<= new_value;                    // 写入
//

template <typename PortType, typename FieldType, std::size_t FieldIndex>
class bundle_port_field_proxy {
public:
    // 显式构造函数
    explicit bundle_port_field_proxy(PortType &port) : port_(port) {}

    // ========================================================================
    // 读操作
    // ========================================================================

    // 隐式转换为字段值（支持 uint64_t, bool 等）
    operator FieldType() const {
        return read_field();
    }

    // 显式读取
    FieldType get() const {
        return read_field();
    }

    // ========================================================================
    // 写操作
    // ========================================================================

    // 标量赋值
    bundle_port_field_proxy &operator=(const FieldType &value) {
        write_field(value);
        return *this;
    }

    // 从其他代理赋值
    bundle_port_field_proxy &operator=(const bundle_port_field_proxy &other) {
        return operator=(static_cast<FieldType>(other));
    }

    // ========================================================================
    // HDL 连接操作符（用于 describe() 中的 <<= 操作）
    // ========================================================================

    bundle_port_field_proxy &operator<<=(const FieldType &value) {
        return operator=(value);
    }

    // 支持从其他代理进行 HDL 连接
    bundle_port_field_proxy &operator<<=(const bundle_port_field_proxy &other) {
        return operator=(static_cast<FieldType>(other));
    }

    // ========================================================================
    // 编译期信息访问（用于调试和验证）
    // ========================================================================

    // 字段名称
    static constexpr std::string_view field_name() {
        constexpr auto layout = get_bundle_layout<typename PortType::bundle_type>();
        constexpr auto field_info = std::get<FieldIndex>(layout);
        return field_info.name;
    }

    // 字段在 Bundle 中的位偏移
    static constexpr unsigned field_offset() {
        constexpr auto layout = get_bundle_layout<typename PortType::bundle_type>();
        constexpr auto field_info = std::get<FieldIndex>(layout);
        return field_info.offset;
    }

    // 字段宽度
    static constexpr unsigned field_width() {
        constexpr auto layout = get_bundle_layout<typename PortType::bundle_type>();
        constexpr auto field_info = std::get<FieldIndex>(layout);
        return field_info.width;
    }

    // 字段索引
    static constexpr std::size_t field_index() {
        return FieldIndex;
    }

private:
    PortType &port_;

    // 内部读取实现
    FieldType read_field() const {
        constexpr auto layout = get_bundle_layout<typename PortType::bundle_type>();
        constexpr auto field_info = std::get<FieldIndex>(layout);

        // 获取 Bundle 对象指针
        auto *bundle_ptr = static_cast<typename PortType::bundle_type *>(port_.impl());
        
        // 通过成员指针访问字段值
        return bundle_ptr->*(field_info.ptr);
    }

    // 内部写入实现
    void write_field(const FieldType &value) {
        constexpr auto layout = get_bundle_layout<typename PortType::bundle_type>();
        constexpr auto field_info = std::get<FieldIndex>(layout);

        // 获取 Bundle 对象指针
        auto *bundle_ptr = static_cast<typename PortType::bundle_type *>(port_.impl());
        
        // 通过成员指针设置字段值
        bundle_ptr->*(field_info.ptr) = value;
    }
};

// ============================================================================
// 第二部分：增强的 Bundle 端口类
// ============================================================================
//
// bundle_port_expanded 扩展了标准 port 类，支持 Bundle 字段的编译期检测
// 和便捷访问。
//
// 使用示例：
//   1. 编译期验证：
//      static_assert(bundle_port_expanded<ch_stream<T>, Dir>::has_field<"payload"_ss>());
//   
//   2. 字段访问：
//      io().stream_out.payload() <<= value;
//      io().stream_out.valid() <<= condition;
//      auto ready = io().stream_in.ready();
//   
//   3. 编译期类型查询：
//      using PayloadType = bundle_port_expanded<...>::field_type<"payload"_ss>;
//

template <typename BundleT, typename Dir>
class bundle_port_expanded : public port<BundleT, Dir> {
public:
    // 类型定义
    using bundle_type = BundleT;
    using base_type = port<BundleT, Dir>;
    using self_type = bundle_port_expanded<BundleT, Dir>;

    // 继承构造函数
    using base_type::base_type;

    // ========================================================================
    // 编译期属性（常量信息）
    // ========================================================================

    // Bundle 布局信息（编译期常量元组）
    static constexpr auto bundle_layout = get_bundle_layout<BundleT>();

    // Bundle 字段数量
    static constexpr size_t field_count = bundle_field_count_v<BundleT>;

    // Bundle 总宽度（所有字段的宽度之和）
    static constexpr unsigned width = bundle_width_v<BundleT>;

    // ========================================================================
    // 编译期字段检查和类型查询
    // ========================================================================

    // 检查指定名称的字段是否存在（编译期）
    template <auto FieldName>
    static constexpr bool has_field() {
        return has_field_named_v<BundleT, FieldName>;
    }

    // 获取指定字段的类型（编译期）
    template <auto FieldName>
    using field_type = typename get_field_type<BundleT, FieldName>::type;

    // ========================================================================
    // 动态字段访问接口
    // ========================================================================

    // 通过编译期已知的字段名访问字段（返回代理对象）
    // 
    // 使用示例：
    //   auto payload_proxy = port.get_field<"payload"_ss>();
    //   payload_proxy <<= value;
    //
    template <auto FieldName>
    auto get_field() {
        // 编译期验证字段存在
        static_assert(has_field<FieldName>(),
                      "Field not found in bundle");

        // 编译期查找字段索引
        constexpr std::size_t field_index = find_field_index<FieldName>();

        // 获取字段信息
        constexpr auto field_info = std::get<field_index>(bundle_layout);

        // 获取字段类型
        using FieldType = typename std::remove_reference_t<
            decltype(std::declval<BundleT>().*(field_info.ptr))>;

        // 返回代理对象
        return bundle_port_field_proxy<self_type, FieldType, field_index>(*this);
    }

    // ========================================================================
    // Handshake 协议的便捷访问接口
    // ========================================================================
    //
    // 为常见的 Handshake 协议字段（payload, valid, ready）提供
    // 零开销的静态方法。这些方法是编译期条件的，只在字段存在时生成。
    //

    // payload 字段访问（仅当字段存在时）
    auto payload() requires (has_field<structural_string{"payload"}>()) {
        return get_field<structural_string{"payload"}>();
    }

    // valid 字段访问（仅当字段存在时）
    auto valid() requires (has_field<structural_string{"valid"}>()) {
        return get_field<structural_string{"valid"}>();
    }

    // ready 字段访问（仅当字段存在时）
    auto ready() requires (has_field<structural_string{"ready"}>()) {
        return get_field<structural_string{"ready"}>();
    }

    // ========================================================================
    // AXI 协议的便捷访问接口
    // ========================================================================

    // AXI 写地址字段
    auto awaddr() requires (has_field<structural_string{"awaddr"}>()) {
        return get_field<structural_string{"awaddr"}>();
    }

    // AXI 读地址字段
    auto araddr() requires (has_field<structural_string{"araddr"}>()) {
        return get_field<structural_string{"araddr"}>();
    }

    // AXI 写数据字段
    auto wdata() requires (has_field<structural_string{"wdata"}>()) {
        return get_field<structural_string{"wdata"}>();
    }

    // AXI 读数据字段
    auto rdata() requires (has_field<structural_string{"rdata"}>()) {
        return get_field<structural_string{"rdata"}>();
    }

private:
    // ========================================================================
    // 编译期工具函数
    // ========================================================================

    // 在布局元组中查找指定字段的索引
    // 使用编译期递归搜索
    template <auto FieldName, std::size_t I = 0>
    static consteval std::size_t find_field_index() {
        if constexpr (I >= field_count) {
            // 不应该到达这里，因为 has_field<FieldName>() 已检查
            static_assert(I < field_count, "Field index out of range");
            return 0;
        } else {
            // 获取第 I 个字段的信息
            constexpr auto field_info = std::get<I>(bundle_layout);
            
            // 构造目标字段名的 string_view
            constexpr std::string_view field_view{
                FieldName.c_str(), 
                FieldName.size()
            };
            
            // 比较字段名
            if constexpr (field_info.name == field_view) {
                return I;  // 找到匹配的字段
            } else {
                return find_field_index<FieldName, I + 1>();  // 继续搜索
            }
        }
    }
};

} // namespace ch::core

#endif // CH_CORE_BUNDLE_PORT_PROXY_H
