# fix-ns-pollution 实施计划

## Scope

**IN:**
- 修复以下 8 个文件中的 `using namespace ch::core;` 命名空间污染：
  - `include/bundle/fragment.h`
  - `include/bundle/stream_bundle.h`
  - `include/bundle/flow_bundle.h`
  - `include/bundle/common_bundles.h`
  - `include/bundle/axi_lite_bundle.h`
  - `include/bundle/axi_bundle.h`
  - `include/ast/instr_mem.h`
  - `include/chlib/assert.h`
- 验证编译通过
- 验证测试无回归
- 提交变更

**OUT:**
- 不修复 `include/chlib/if_stmt.h`（已正确）
- 不修改 `ch::core` 命名空间本身
- 不添加新功能

## Dependency Graph

```
[1.1-1.6, 2.1-2.2] ← 可并行执行（文件修复）
        ↓
[3.1-3.3] ← 顺序执行（验证）
```

## Work Units

### 阶段 1：文件修复（可并行）

| 任务 | 文件 | 依赖 | 预计工作量 |
|------|------|------|-----------|
| 1.1 | `include/bundle/fragment.h` | 无 | ~15 min |
| 1.2 | `include/bundle/stream_bundle.h` | 无 | ~15 min |
| 1.3 | `include/bundle/flow_bundle.h` | 无 | ~15 min |
| 1.4 | `include/bundle/common_bundles.h` | 无 | ~15 min |
| 1.5 | `include/bundle/axi_lite_bundle.h` | 无 | ~15 min |
| 1.6 | `include/bundle/axi_bundle.h` | 无 | ~15 min |
| 2.1 | `include/ast/instr_mem.h` | 无 | ~15 min |
| 2.2 | `include/chlib/assert.h` | 无 | ~15 min |

**修复步骤（每文件）：**
1. 读取文件，找到 `using namespace ch::core;` 语句
2. 统计文件中实际使用的 `ch::core` 符号（如 `ch_bool`, `ch_uint` 等）
3. 替换为按需引入或完全限定（根据实际使用情况选择）
4. 运行 `cmake --build build` 验证编译
5. 如编译失败，调试并修复

### 阶段 2：验证（顺序）

| 任务 | 说明 | 依赖 | 预计工作量 |
|------|------|------|-----------|
| 3.1 | `cmake --build build` | 1.1-2.2 全部完成 | ~3 min |
| 3.2 | `ctest --output-on-failure` | 3.1 通过 | ~5 min |
| 3.3 | 提交变更 | 3.2 通过 | ~2 min |

## QA Scenarios

### Q1: 单文件修复验证
```bash
# 修复后验证编译
cmake --build build -j$(nproc)
```
**期望**：编译成功，无错误

### Q2: 完整构建验证
```bash
# 所有文件修复后
cmake --build build -j$(nproc)
```
**期望**：构建成功

### Q3: 测试回归验证
```bash
ctest --output-on-failure
```
**期望**：所有测试通过（允许 1 个 pre-existing 超时：`perf_tests`）

## 风险点和缓解

| 风险 | 缓解措施 |
|------|---------|
| 遗漏符号导致编译失败 | 每文件修复后立即编译验证 |
| 修复引入新的命名冲突 | 使用 LSP 跳转确认符号来源 |
| 回归问题 | 运行完整测试套件 |

## 参考

- ADR-033: `docs/adr/ADR-033-namespace-pollution.md`
- 参考实现: `include/chlib/if_stmt.h`（已正确修复）
- tasks.md: `openspec/changes/fix-ns-pollution/tasks.md`