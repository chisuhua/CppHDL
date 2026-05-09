# ASSERTION-DEBUGGING - Assertion 失败系统性调试技能

## 触发场景 (Trigger)

满足以下任一条件时激活本技能：
- `Assertion failed`
- `SIGABRT` (abnormal termination)
- `ch_check()` 失败
- 代码编译通过但运行时崩溃

## 调试流程

### 第一阶段：定位 Assertion 位置

```
错误信息通常包含：
test_jit_compiler: /workspace/project/CppHDL/include/bv/utils.h:842: void ch::internal::ch_check(bool, const char*): Assertion `condition && msg' failed.
```

定位源：
```cpp
// include/bv/utils.h:838-843
inline void ch_check(bool condition, const char *msg = "Assertion failed") {
    if (!condition) {
        assert(condition && msg);  // line 842
    }
}
```

**关键**：向上追溯，找到谁调用了 `ch_check(false, ...)`。

### 第二阶段：追溯触发条件

搜索 `CH_CHECK` 或 `ch_check` 的调用：

```bash
# 搜索所有 ch_check 调用
grep -rn "CH_CHECK\|ch_check" src/ include/ | head -30

# 检查触发条件（第一个参数）
# 例如: CH_CHECK(result.ir_instr_count > 0, "...")
# 意味着 ir_instr_count 为 0 或负数
```

### 第三阶段：确认是否 Pre-existing Bug

```bash
# 1. 保存当前状态
git stash

# 2. 恢复到"好"的版本
git checkout <suspect-good-commit> -- <file>

# 3. 重新编译测试
cmake --build build --target <test>

# 4. 运行测试
./build/tests/<test>

# 如果原本就失败 → Pre-existing bug，与你的修改无关
# 如果原本正常 → 你的修改引入了问题
```

### 第四阶段：交叉验证

即使原始版本失败，也要确认你的修改没有让情况更糟：

```bash
# 1. 恢复你的修改
git stash pop

# 2. 测试当前版本
./build/tests/<test>

# 3. 对比两次的错误信息是否相同
# 相同 → 你的修改没有恶化问题
# 不同 → 你的修改引入了新问题
```

## 常见 Assertion 类型

| Assertion | 含义 | 可能原因 |
|-----------|------|---------|
| `CH_CHECK(ptr != nullptr)` | 空指针 | 未初始化的指针 |
| `CH_CHECK(result > 0)` | 无效结果 | 初始化失败、内存分配失败 |
| `CH_CHECK(ir_instr_count > 0)` | JIT 无指令 | eval_list 为空、IR 生成失败 |
| `CH_CHECK(condition && msg)` | 通用失败 | 上述任一原因 |

## 调试技巧

### 1. 添加日志

在 assertion 前添加日志：

```cpp
CHINFO("Debug: ir_instr_count=%u, vreg_count=%u", result.ir_instr_count, result.vreg_count);
CH_CHECK(result.ir_instr_count > 0, "No instructions generated");
```

### 2. 使用 GDB

```bash
# 设置断点在 assertion 函数
gdb ./build/tests/test_jit_compiler
(gdb) break ch_check
(gdb) run
(gdb) bt  # 查看调用栈
```

### 3. 二分法定位

如果不确定是哪个修改引入的问题：

```bash
# 逐步恢复修改
git stash
git checkout <good-commit>

# 逐步 cherry-pick 修改
git cherry-pick <你的修改 commit 1>
# 测试
git cherry-pick <你的修改 commit 2>
# 测试
# ...
```

## 验证清单

- [ ] 找到 assertion 的触发位置
- [ ] 理解触发条件（什么检查失败了）
- [ ] 确认是 Pre-existing bug 还是新引入的
- [ ] 如果是新引入的，追溯到具体修改

## 关键教训

1. **Assertion 失败是好事**：它告诉你代码有逻辑错误
2. **SIGABRT vs 其他 crash**：SIGABRT 通常是 assertion，SIGSEGV 是空指针
3. **Pre-existing bug 检测**：每次调试都要先确认问题是否原本就存在
4. **日志先行**：在 assertion 处添加日志可以快速定位触发条件

## 退出条件

- 找到了触发 assertion 的确切位置和条件
- 确认了问题是新引入的还是 pre-existing
- 如果是新引入的，修复并验证