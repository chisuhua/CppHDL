## ADDED Requirements

### Requirement: Bundle 字段数无限制 (Bug A)
反射系统应支持任意数量字段的 Bundle。

#### Scenario: 大 Bundle
- **WHEN** 定义包含超过 10 个字段的 Bundle
- **THEN** 反射系统正常工作

### Requirement: add_user 实现 (P2 延期 - 见 ADR-014)
~~Bundle 节点应正确追踪其用户节点。~~

> **状态**: ADR-014 将此标记为 P2 延期，分类为"增强，非紧急修复"。
> Bundle 连接走 `logic_buffer::operator=` 路径，不经过 io.h 端口操作符，
> 所以缺失的 add_user 调用不影响 Bundle 用户追踪。
> 此项延后至后续 change 处理。