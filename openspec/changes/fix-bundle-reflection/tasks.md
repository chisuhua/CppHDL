## 1. 分析阶段

- [x] 1.1 找到 Bundle 反射系统实现代码
      📍 include/core/bundle/bundle_meta.h

- [x] 1.2 识别 10 字段限制的位置和原因
      📍 CH_GET_NTH_ARG 宏 (line 15) + CH_BUNDLE_FIELDS_T_1~10 宏

- [x] 1.3 ~~找到 add_user 调用的位置~~ (已取消 - ADR-014 P2 延期)
      📍 include/core/io.h:704 (故意注释) - 不影响 Bundle 用户追踪

## 2. 修复 10 字段限制

- [ ] 2.1 确定是固定数组限制还是其他原因
      💡 根因：CH_GET_NTH_ARG 只支持 10 个参数

- [ ] 2.2 修复为动态或更大限制
      💡 方案：添加 CH_BUNDLE_FIELDS_T_11~20 宏 + 更新 CH_GET_NTH_ARG

- [ ] 2.3 验证修复
      💡 需要编译验证 + 大 Bundle 测试

## 3. ~~实现 add_user~~ (已取消 - ADR-014 P2 延期)

~~- [ ] 3.1 确定 add_user 应该在何处被调用~~
~~    💡 位置：include/core/io.h:704 需取消注释~~

~~- [ ] 3.2 实现 add_user 逻辑~~
~~    💡 逻辑已存在于 lnodeimpl.h:140~~

~~- [ ] 3.3 验证用户追踪正确~~
~~    💡 需要调试输出或测试验证~~

> **注意**: add_user 修复已从本 change 范围移除。
> Bundle 连接走 `logic_buffer::operator=` 路径，不经过 io.h 端口操作符，
> 所以缺失的 add_user 调用不影响 Bundle 用户追踪。
> 按 ADR-014 分类，此项为 P2 延期。

## 4. 验证测试

- [ ] 4.1 编译验证：`cmake --build build -j$(nproc)`

- [ ] 4.2 运行 Bundle 相关测试

- [ ] 4.3 测试大 Bundle（>10 字段）