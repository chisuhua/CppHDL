## 1. JIT FAIL FAST 实施

- [x] 1.1 从 `jit_compiler.cpp:generate_ir()` line 移除 concat 映射
      💡 已移除 concat → JitOp::CONCAT 映射 (原 line 362-363)

- [x] 1.2 确认 concat 在 `compile_to_llvm()` 中返回 `UNSUPPORTED_OP`
      💡 compile_to_llvm line 574 已有检查：CONCAT → UNSUPPORTED_OP

- [ ] 1.3 更新 AGENTS.md 规则 1：明确 CALL_EXTERNAL 不可接受
      💡 将 concat 标记为不可 JIT 的操作

## 2. Mask 补全

- [x] 2.1 在 `compile_to_llvm()` 的 AND case 添加 mask
      💡 已添加：`res = builder.CreateAnd(res, builder.getInt64(mask), "mask_and")`

- [x] 2.2 在 OR case 添加 mask
      💡 已添加：`res = builder.CreateAnd(res, builder.getInt64(mask), "mask_or")`

- [x] 2.3 在 XOR case 添加 mask
      💡 已添加：`res = builder.CreateAnd(res, builder.getInt64(mask), "mask_xor")`

## 3. 验证测试

- [ ] 3.1 编译验证：`cmake --build build -j$(nproc)`

- [ ] 3.2 运行 JIT 相关测试：`ctest -R jit --output-on-failure`

- [ ] 3.3 验证现有测试仍通过（无回归）