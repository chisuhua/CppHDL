# fix-bundle-reflection

> **STATUS (2026-06-18)**: ✅ **FULLY COMPLETED**. `CH_BUNDLE_FIELDS_T` 宏从 10 字段扩展到 **40 字段**（实际只需 23，余量 17）。HazardUnitBundle 的潜在静默数据丢失 bug 已消除。`tests/test_bundle_large.cpp` 提供 1/20/23/25/40 字段边界回归测试。ADR-014 v1.1 已更新。

## 1. 分析阶段

- [x] 1.1 找到 Bundle 反射系统实现代码
      📍 include/core/bundle/bundle_meta.h
- [x] 1.2 识别 10 字段限制的位置和原因
      📍 CH_GET_NTH_ARG 宏 + CH_BUNDLE_FIELDS_T_1~10 宏
- [x] 1.3 ~~找到 add_user 调用的位置~~ (已取消 - ADR-014 P2 延期)
      📍 include/core/io.h:494（已迁移位置，c-class refactor 后）

## 2. 修复 10 字段限制

- [x] 2.1 确定是固定数组限制还是其他原因
      ✅ 根因：`CH_GET_NTH_ARG` 宏参数数量硬编码（v1.0 限定 10 → 早期扩展到 20）
- [x] 2.2 修复为动态或更大限制
      ✅ **扩展到 40 字段**（满足当前 23 字段 HazardUnitBundle + 余量 17）
      📍 `include/core/bundle/bundle_meta.h:23-152`（新增 CH_BUNDLE_FIELDS_T_21~T_40 + CH_GET_NTH_ARG 40 args）
      📝 未来 variadic 重构方案保留在 ADR-014 §4.2 / §6.1（非阻塞）
- [x] 2.3 验证修复
      ✅ `tests/test_bundle_large.cpp` — 5 个 TEST_CASE / 21 assertions
      ✅ Bundle 元组大小 = 字段数（无静默截断）
      ✅ bundle_width_v 正确计算总位宽

## 3. ~~实现 add_user~~ (已取消 - ADR-014 P2 延期)

~~- [ ] 3.1 确定 add_user 应该在何处被调用~~
~~- [ ] 3.2 实现 add_user 逻辑~~
~~- [ ] 3.3 验证用户追踪正确~~

> **决议保留**：add_user 修复 P2 延期不变。Bundle 连接走 `logic_buffer::operator=` 路径不影响追踪。

## 4. 验证测试

- [x] 4.1 编译验证：`cmake --build build -j$(nproc)`
      ✅ test_bundle_large 链接成功（仅预存在的 nonnull-compare warnings）
- [x] 4.2 运行 Bundle 相关测试
      ✅ **8/8 pass**：test_bundle, **test_bundle_large** (新), test_bundle_advanced,
      test_bundle_advanced_features, test_bundle_bitstream, test_bundle_connection,
      test_bundle_node_management, test_bundle_serialization
      ✅ **3/3 pass**：test_axi4lite, test_nested_bundle, test_axi_lite_bundle
- [x] 4.3 测试大 Bundle（>10 字段）
      ✅ 25 字段（LargeBundle25）和 23 字段（HazardUnitLike23）回归测试通过
      ✅ 1/20/40 字段边界均验证
