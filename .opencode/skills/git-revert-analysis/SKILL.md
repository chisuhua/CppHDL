# GIT-REVERT-ANALYSIS - Revert 引入的 Bug 分析技能

## 触发场景 (Trigger)

满足以下任一条件时激活本技能：
- 代码在某次 revert 后行为异常
- "revert" 出现在 git log 中
- 功能在 revert 后失效但没有明显的编译错误
- 涉及 `git revert`、`git checkout <commit> -- <file>`

## 核心原则

**Revert 经常引入新 bug**：revert 修复了一个问题，但可能引入其他问题。

## 分析流程

### 第一阶段：确认 Revert 的范围

```bash
# 查看最近的 revert commits
git log --oneline -10 | grep -i revert

# 查看 revert 前后的完整差异
git diff <good-commit>..<bad-commit> -- src/
```

### 第二阶段：识别 "Break vs Continue" 错误

Revert 常见错误是循环控制语句的改变：

```diff
# 错误示例：revert 后把 continue 改成了 break
-            continue;
+            break;

# 或者反过来
-            break;
+            continue;
```

**影响**：
- `break`：跳出当前代码块，处理下一个 node
- `continue`：跳到循环开头，**跳过** `block_comb.node_count++`

### 第三阶段：恢复并测试原始版本

```bash
# 1. 暂存当前修改
git stash

# 2. 恢复到"好"的版本
git checkout <good-commit> -- <file>

# 3. 测试是否原本就失败（pre-existing bug）
# 如果原本就失败，说明问题不是 revert 引入的
```

### 第四阶段：手动重建正确版本

如果 revert 删除了正确代码，需要手动恢复：

```bash
# 查看被删除的代码
git show <good-commit>:<file> | grep -A20 "type_lit"

# 应用正确的版本
git checkout <good-commit> -- <file>
```

## 常见 Revert 模式

| Revert 类型 | 常见问题 |
|------------|---------|
| 掩码 UB 修复 revert | 删除了正确的掩码代码，但引入了 break 错误 |
| 功能 revert | 删除了一些必要的初始化代码 |
| 样式 revert | 意外改变了控制流 |

## 验证清单

```bash
# 1. 确认 revert 的确切内容
git show <revert-commit> --stat

# 2. 对比前后差异
git diff <before-commit>..<after-commit> -- <file>

# 3. 找出所有被改变的 if/switch/break/continue
git diff <before>..<after> | grep -E "^\+.*break|^\-.*break|^\+.*continue|^\-.*continue"

# 4. 测试"好"版本是否真的工作
git checkout <good-commit> -- <file>
```

## 关键教训

1. **Revert 不是简单的"撤销"**：它创建了新 commit，可能引入新 bug
2. **Break vs Continue**：这是最常见的 revert 错误
3. **Pre-existing bug 检测**：先测试原始版本，确认问题是否原本就存在
4. **Git diff 比记忆更可靠**：不要相信自己的记忆，要看实际差异

## 退出条件

- 找到了被 revert 删掉的正确代码
- 恢复了正确版本的代码
- 测试确认修复有效