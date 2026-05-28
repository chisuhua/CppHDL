## 1. include 聚合修复

- [ ] 1.1 检查 `include/chlib.h` 当前包含的子文件列表
- [ ] 1.2 添加 `#include "chlib/stream_pipeline.h"` 到 `chlib.h`
- [ ] 1.3 检查 `stream_bundle_member_inlines.h` 的正确包含位置

## 2. 验证测试

- [ ] 2.1 编译验证：`cmake --build build -j$(nproc)`
- [ ] 2.2 运行 stream_pipeline 测试：`ctest -R stream_pipeline --output-on-failure`
- [ ] 2.3 验证示例程序可以正常编译