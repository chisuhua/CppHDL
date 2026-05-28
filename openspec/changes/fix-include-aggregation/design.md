# Design: fix-include-aggregation

## Context

ADR-022 分析发现 `stream_pipeline.h` 管道函数已实现，但用户无法通过 `chlib.h` 聚合头访问。两个缺口：

1. **G1**: `include/chlib.h` 没有包含 `stream_pipeline.h`
2. **G2**: `stream_bundle_member_inlines.h` 是孤件，无聚合头包含

## Goals / Non-Goals

**Goals:**
- 将 `stream_pipeline.h` 添加到 `chlib.h` 聚合中
- 将 `stream_bundle_member_inlines.h` 合理集成到 include 体系中

**Non-Goals:**
- 不修改管道函数实现（已验证正确）
- 不改变现有 API

## Decisions

1. 在 `include/chlib.h` 末尾添加 `#include "chlib/stream_pipeline.h"`
2. 确认 `stream_bundle_member_inlines.h` 的正确包含路径（应在 stream bundle 之后）

## Risks / Trade-offs

- **风险**: 低。仅为 include 调整，无逻辑变更
- **验证**: 编译测试 + 检查管道示例是否正常包含