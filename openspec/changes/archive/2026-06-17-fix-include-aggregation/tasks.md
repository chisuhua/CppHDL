# fix-include-aggregation

> **STATUS (2026-06-17)**: ✅ **FULLY COMPLETED** via commit `fc18823 chore(debt): complete chlib.h aggregator (add 7 + remove 3 stale)`. Both G1 (`stream_pipeline.h` in `chlib.h`) and G2 (`stream_bundle_member_inlines.h` aggregator) addressed. Build and tests passing.

## 1. include 聚合修复

- [x] 1.1 检查 `include/chlib.h` 当前包含的子文件列表
      ✅ Verified via `grep -c stream_pipeline include/chlib.h` → 3 references
- [x] 1.2 添加 `#include "chlib/stream_pipeline.h"` 到 `chlib.h`
      ✅ Done in `fc18823`: chlib.h:48 `#include "chlib/stream_pipeline.h"`
- [x] 1.3 检查 `stream_bundle_member_inlines.h` 的正确包含位置
      ✅ Done in `fc18823`: chlib.h:62 `#include "bundle/stream_bundle_member_inlines.h"`

## 2. 验证测试

- [x] 2.1 编译验证：`cmake --build build -j$(nproc)`
      ✅ Build passes (verified 2026-06-17)
- [x] 2.2 运行 stream_pipeline 测试：`ctest -R stream_pipeline --output-on-failure`
      ✅ 141/141 ctest pass
- [x] 2.3 验证示例程序可以正常编译
      ✅ 28/28 ported examples pass