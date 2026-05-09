# SYSTEMATIC-JIT-DEBUG - 系统化 JIT 调试流程技能

## 触发场景 (Trigger)

满足以下任一条件时激活本技能：
- JIT 相关测试失败
- 编译成功但运行时行为异常
- "JIT compilation successful" 但后续 crash
- 需要完整调试 JIT 编译器

## 概述

本技能整合 cpp-jit-debugging、git-revert-analysis、assertion-debugging 形成完整调试流程。

## 完整调试流程

### 阶段 1：环境确认

```bash
# 1. 确认 JIT 是否启用
grep "CH_JIT_ENABLED" include/jit/jit_compiler.h

# 2. 确认 LLVM 可用
ls /usr/lib/llvm-18/include/llvm/ExecutionEngine/Orc/LLJIT.h

# 3. 检查 cmake 配置
grep -i "jit" build/CMakeCache.txt
```

### 阶段 2：确认问题范围

```bash
# 1. 运行单个 JIT 测试
cd build
ctest -R test_jit --output-on-failure

# 2. 检查是否是 JIT 问题
# 如果 "[WARN] JIT compilation failed" → JIT 生成失败
# 如果 "[INFO] JIT compilation successful" 后 crash → JIT 生成成功但代码有 bug
```

### 阶段 3：JIT 生成调试

#### 检查点 1：IR 生成 (generate_ir)

```bash
# 检查代码
grep -n "type_lit\|type_input\|type_op\|type_mux\|type_proxy" src/jit/jit_compiler.cpp | head -30

# 检查关键错误：
# 1. type_lit 是否在 switch 内？
# 2. 是否使用 make_const 而非 make_load？
# 3. 是否使用 break 而非 continue？
```

#### 检查点 2：LLVM 代码生成 (compile_to_llvm)

```bash
# 检查 load_node 实现
grep -A10 "auto load_node" src/jit/jit_compiler.cpp

# 必须使用 CreateTrunc（安全），不是手动掩码
# ✓ 正确: builder.CreateTrunc(val, builder.getIntNTy(bw), "trunc")
# ✗ 错误: uint64_t mask = (1ULL << bw) - 1  // UB
```

#### 检查点 3：位宽处理

```bash
# 搜索所有位宽相关代码
grep -n "bw < 64\|bw == 64\|bitwidth < 64" src/jit/jit_compiler.cpp

# 规则：
# - bw < 64: 使用 CreateTrunc/CreateZExt
# - bw == 64: 保持原值（不截断）
# - bw == 0: 通常返回 0
```

### 阶段 4：Revert 检测

```bash
# 1. 检查是否有 revert commits
git log --oneline -5 | grep -i revert

# 2. 对比当前与"好"版本
git diff <good-commit>..HEAD -- src/jit/jit_compiler.cpp

# 3. 检查常见的 revert 错误
# - break vs continue
# - make_const vs make_load
# - 删除的代码 vs 添加的代码
```

### 阶段 5：Pre-existing Bug 检测

```bash
# 1. 保存当前修改
git stash

# 2. 恢复到"好"的版本
git checkout <suspect-good-commit>

# 3. 重新编译
cmake --build build --target cpphdl
cmake --build build --target test_jit_compiler

# 4. 运行测试
./build/tests/test_jit_compiler

# 5. 分析结果
# 如果原本就失败 → Pre-existing bug
# 如果原本正常 → 你的修改引入了问题
```

### 阶段 6：Assertion 调试

如果 JIT 编译成功但后续 crash：

```bash
# 1. 找到 assertion 位置
# 从错误信息获取文件:行号

# 2. 添加调试日志
# 在 assertion 前添加：
CHINFO("Debug: value=%u, expected=%u", value, expected);
CH_CHECK(value > 0, "value should be positive");

# 3. 重新编译运行
```

### 阶段 7：回归测试

修复后必须完整测试：

```bash
# 1. 编译所有
cmake --build build --parallel 2

# 2. 运行所有测试
ctest --output-on-failure -j$(nproc)

# 3. 重点关注：
# - test_jit_compiler
# - 其他 JIT 相关测试
# - 之前因 assertion 失败的所有测试
```

## 决策树

```
JIT 测试失败？
├─ "[WARN] JIT compilation failed"
│   └─ 检查 generate_ir() 返回值和 ir_instr_count
│
├─ "[INFO] JIT compilation successful" + SIGABRT
│   └─ 检查 compile_to_llvm 中的代码生成
│       ├─ load_node/store_node 实现
│       ├─ 位宽处理
│       └─ 是否有手动掩码计算
│
└─ "[INFO] JIT compilation successful" + 测试失败
    └─ 检查实际行为
        ├─ 结果值错误
        └─ 时序问题
```

## 验证清单

- [ ] JIT 编译成功日志出现
- [ ] ir_instr_count > 0
- [ ] 无 SIGABRT/SIGSEGV
- [ ] 测试用例通过
- [ ] 无不安全的掩码计算 `(1ULL << bw) - 1`
- [ ] type_lit 使用 make_const
- [ ] 位宽处理正确（< 64 截断，= 64 保持）

## 关键教训

1. **JIT 成功 ≠ JIT 正确**：编译成功可能生成错误代码
2. **完整的调试流程**：不要跳过任何阶段
3. **Pre-existing bug 检测**：每次都要确认问题是否原本就存在
4. **Systematic approach**：按照决策树逐步排查

## 退出条件

所有验证清单项目通过，且：
- JIT 测试通过
- 全量测试通过（> 95%）
- 无 regression