## Context

ADR-033 记录了 `using namespace ch::core;` 在 `namespace ch { ... }` 内部造成的命名空间污染问题。共有 8 个文件受影响：

- `include/ast/instr_mem.h`
- `include/bundle/axi_bundle.h`
- `include/bundle/axi_lite_bundle.h`
- `include/bundle/common_bundles.h`
- `include/bundle/flow_bundle.h`
- `include/bundle/fragment.h`
- `include/bundle/stream_bundle.h`
- `include/chlib/assert.h`

参考实现：`include/chlib/if_stmt.h` 已正确修复（改为 `namespace chlib { using namespace ch::core; }`）。

## Goals / Non-Goals

**Goals:**
- 消除 `namespace ch { ... }` 内的 `using namespace ch::core;` 污染
- 保持功能行为不变，仅修改命名空间引用方式
- 验证编译通过且现有测试无回归

**Non-Goals:**
- 不修改 `ch::core` 命名空间本身
- 不添加新功能
- 不修复 `if_stmt.h`（已正确）

## Decisions

### D1: 修复策略

**选择**：按需引入（selective using）vs 完全限定（fully qualified）

| 方案 | 优点 | 缺点 |
|------|------|------|
| 完全限定 `ch::core::ch_bool` | 无命名空间污染，最干净 | 代码冗长，修改量大 |
| 按需引入 `using ch::core::ch_bool` | 折中方案，改动适中 | 仍有部分污染 |
| selective using（本选择） | 最小改动，保持可读性 | 需要分析每个文件实际使用的符号 |

**决策**：对每个文件，先统计实际使用的 `ch::core` 符号，选择按需引入或完全限定（视情况而定）。

### D2: 修改顺序

按依赖关系排序，优先修改被其他文件 include 的头文件：

1. `include/core/types.h`（如有污染）
2. `include/bundle/fragment.h`（其他 bundle 的基类）
3. `include/bundle/stream_bundle.h`
4. `include/bundle/flow_bundle.h`
5. `include/bundle/common_bundles.h`
6. `include/bundle/axi_lite_bundle.h`
7. `include/bundle/axi_bundle.h`
8. `include/ast/instr_mem.h`
9. `include/chlib/assert.h`

## Risks / Trade-offs

- **风险**：修改遗漏导致编译失败 → **缓解**：每次修改后运行 `cmake --build build`
- **风险**：部分符号未被引入导致使用时找不到 → **缓解**：使用 IDE 的 LSP 跳转确认
- **风险**：回归问题 → **缓解**：运行完整测试套件

## Migration Plan

1. 逐文件修改，每次修改后验证编译
2. 全部完成后运行 `ctest --output-on-failure`
3. 如有问题，使用 git stash 回滚

## Open Questions

- 无