## ADDED Requirements

### Requirement: chlib.h include 完整性
用户通过 `#include "chlib.h"` 应能访问 `stream_m2s_pipe()`, `stream_s2m_pipe()`, `stream_half_pipe()` 等管道函数。

#### Scenario: 聚合头包含管道
- **WHEN** 用户 `#include "chlib.h"`
- **THEN** 管道函数 `stream_m2s_pipe()`, `stream_s2m_pipe()`, `stream_half_pipe()` 可用

### Requirement: stream_bundle_member_inlines 可访问
`ch_stream<T>::m2sPipe()` 等成员函数应通过正确的包含路径可达。

#### Scenario: 成员函数包含顺序
- **WHEN** 用户 `#include "bundle/stream_bundle.h"`
- **THEN** 之后包含 `stream_bundle_member_inlines.h` 可正常编译