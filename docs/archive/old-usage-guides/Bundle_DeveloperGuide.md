# CppHDL Bundle 开发者指南

本指南面向框架维护者与高级用户，描述 Bundle 的内部机制、元编程基础设施与扩展建议。

## 1. 核心文件结构

Bundle 相关核心实现集中在以下头文件：

- include/core/bundle/bundle_base.h：Bundle 基类与连接逻辑
- include/core/bundle/bundle_meta.h：字段元数据与宏
- include/core/bundle/bundle_traits.h：类型识别、字段计数、宽度计算
- include/core/bundle/bundle_layout.h：字段布局（位宽/偏移）
- include/core/bundle/bundle_protocol.h：协议检测（Handshake/AXI 等）
- include/core/bundle/bundle_serialization.h：序列化/反序列化
- include/core/bundle/bundle_operations.h：bundle_slice / bundle_cat
- include/core/bundle/bundle_utils.h：辅助工具

## 2. 元编程基础设施

### 2.1 字段元数据与反射入口

`CH_BUNDLE_FIELDS_T(...)` 宏会生成 `__bundle_fields()`，返回字段元数据元组。元数据类型为 `bundle_field<BundleT, FieldType>`，包含字段名与成员指针。

```cpp
struct MyBundle : public ch::core::bundle_base<MyBundle> {
    ch_uint<8> payload;
    ch_bool valid;
    ch_bool ready;

    CH_BUNDLE_FIELDS_T(payload, valid, ready)
};
```

### 2.2 类型识别与字段计数

- `is_bundle_v<T>`：编译期判断类型是否为 Bundle
- `bundle_field_count_v<T>`：字段数量
- `get_bundle_width<T>()` / `bundle_width_v<T>`：位宽计算（支持嵌套）

这些功能定义于 bundle_traits.h，配合 `std::index_sequence` 做编译期展开。

### 2.3 字段布局

`get_bundle_layout<T>()` 返回字段布局元组（含位偏移与宽度），用于序列化与仿真映射。字段结构为 `bundle_field_with_layout`。

## 3. Bundle 基类与连接语义

`bundle_base` 提供以下关键能力：

- `as_master()` / `as_slave()`：设置角色并调用派生类方向配置
- `operator<<=`：字段级连接，根据方向自动选择驱动端
- `connect(src, dst)`：同类型 Bundle 的字段级赋值连接
- `set_name_prefix()`：批量设置字段命名
- `is_valid()`：递归检查字段有效性

连接逻辑基于字段方向（`input_direction` / `output_direction`）进行判断，可递归处理嵌套 Bundle。

## 4. 协议检测与字段查询

bundle_protocol.h 提供编译期字段查询与协议检测：

- `has_field_named_v<T, Name>`：检查字段名
- `get_field_type_t<T, Name>`：提取字段类型
- `is_handshake_protocol_v<T>` / `validate_handshake_protocol<T>()`
- `is_axi_protocol_v<T>` / `validate_axi_protocol<T>()`

AXI-Lite 的完整检测在 include/bundle/axi_protocol.h 中实现：

- `is_axi_lite_v<T>`
- `is_axi_lite_write_v<T>`
- `is_axi_lite_read_v<T>`

## 5. 序列化与反序列化

bundle_serialization.h 使用编译期布局完成零运行时开销的序列化：

- `serialize<T>(bundle)`：转为 `ch_uint<W>`
- `deserialize<T, W>(bits)`：从位向量恢复 Bundle

该过程依赖 `get_bundle_layout<T>()` 与字段宽度计算，支持嵌套 Bundle。

## 6. 操作工具（bundle_slice / bundle_cat）

bundle_operations.h 提供两个常用工具：

- `bundle_slice<Start, Count>(bundle)`：从 bundle 中切片字段
- `bundle_cat(b1, b2)`：组合两个 bundle

注意：当前 `bundle_slice` 仅支持最多 3 个字段。

## 7. 扩展建议

- 新协议 Bundle 建议提供协议检测 trait 与 `validate_xxx_protocol` 校验函数
- 嵌套 Bundle 请确保字段命名一致性与方向配置正确
- 连接语义若需改变，优先在 `bundle_base::connect_field_based_on_direction_and_source` 中统一处理

## 8. 与使用指南的关系

- 使用方式与推荐模式：见 Bundle_UsageGuide.md
- 框架实现与扩展：见本文件
