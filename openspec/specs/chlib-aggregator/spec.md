# chlib-aggregator Spec

## Purpose

Define `include/chlib.h` as the **aggregator header** for the chlib component library, ensuring single-include-point access to all public chlib components.

## Requirements

### Requirement: chlib.h include 完整性
用户通过 `#include "chlib.h"` **MUST** 能访问 `stream_m2s_pipe()`, `stream_s2m_pipe()`, `stream_half_pipe()` 等 chlib 命名空间下的管道函数。

#### Scenario: 聚合头暴露管道函数
- **WHEN** 用户 `#include "chlib.h"`
- **THEN** 管道函数 `stream_m2s_pipe()`, `stream_s2m_pipe()`, `stream_half_pipe()` 可直接调用

#### Scenario: 流成员函数通过聚合头链路可达
- **WHEN** 用户 `#include "chlib.h"`（聚合头间接包含 `bundle/stream_bundle.h`）
- **THEN** `ch_stream<T>::m2sPipe()`, `s2mPipe()`, `halfPipe()` 等成员函数可用

### Requirement: stream_bundle_member_inlines 包含顺序
`ch_stream<T>::m2sPipe()` 等成员函数的实现位于 `stream_bundle_member_inlines.h`，**MUST** 在 `stream_bundle.h` 之后包含。

#### Scenario: 包含顺序约束
- **WHEN** 用户先 `#include "bundle/stream_bundle.h"` 再 `#include "bundle/stream_bundle_member_inlines.h"`
- **THEN** 编译通过，成员函数定义可见

#### Scenario: 反向包含应失败
- **WHEN** 用户仅 `#include "bundle/stream_bundle_member_inlines.h"` 而不包含 `stream_bundle.h`
- **THEN** 编译失败（缺少前置类型定义）

## Source

Migrated from `openspec/changes/archive/2026-06-17-fix-include-aggregation/spec.md` on 2026-06-17.

The original `## ADDED Requirements` heading was stripped because main specs are absolute truth, not deltas.

## Validation

| Check | Command | Expected |
|-------|---------|----------|
| 聚合头包含管道 | `grep -c stream_pipeline include/chlib.h` | ≥ 1 |
| 成员函数内联包含 | `grep "stream_bundle_member_inlines" include/chlib.h` | ≥ 1 |
| 编译 | `cmake --build build -j$(nproc)` | exit 0 |
| 测试 | `ctest --output-on-failure` | 141/141 pass |
| 示例 | `./run_all_ported_tests.sh` | 28/28 pass |