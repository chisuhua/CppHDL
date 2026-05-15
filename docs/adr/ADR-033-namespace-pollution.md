# ADR-033: `using namespace ch::core` 命名空间污染

**状态**: ✅ 已采纳（记录现状，暂不修复）
**日期**: 2026-05-11
**决策人**: Sisyphus + 用户（Critic 分析参考）

---

## 1. 背景

议题 #27 审查 `using namespace ch::core;` 出现在 `namespace ch { ... }` 内部的问题。这会将 `ch::core` 中所有名称（~50+ 类型/函数/概念）直接注入 `ch` 命名空间，破坏命名空间隔离。

### 1.1 当前状态

`if_stmt.h` 已修复（改为 `namespace chlib { using namespace ch::core; }`），但仍有 **8 个文件** 存在该问题：

| 文件 | 说明 |
|------|------|
| `include/ast/instr_mem.h` | 内存指令实现 |
| `include/bundle/axi_bundle.h` | AXI bundle 定义 |
| `include/bundle/axi_lite_bundle.h` | AXI-Lite bundle 定义 |
| `include/bundle/common_bundles.h` | 通用 bundle 类型 |
| `include/bundle/flow_bundle.h` | Flow bundle 定义 |
| `include/bundle/fragment.h` | Fragment bundle 定义 |
| `include/bundle/stream_bundle.h` | Stream bundle 定义 |
| `include/chlib/assert.h` | 断言组件 |

### 1.2 Critic 分析结论

| 维度 | 评估 |
|------|------|
| **风险等级** | HIGH |
| **实际影响** | 当前无编译错误，但存在隐式碰撞和 ADL 风险 |
| **推荐修复** | 完全限定名或按需 `using ch::core::ch_bool;` |
| **修复工作量** | 8 个文件，每个约 10-30 分钟（替换 + 验证编译） |

---

## 2. 决议

**记录现状，暂不修复**。待下次项目结构化重构时一并处理。

### 修复方案（未来参考）

对每个文件：

```cpp
// ❌ 当前（命名空间污染）
namespace ch {
using namespace ch::core;
// ... 使用 ch_bool, ch_uint 等
}

// ✅ 修复后（按需引入）
namespace ch {
using ch::core::ch_bool;
using ch::core::ch_uint;
// ... 其他实际使用的类型
}
```

或完全限定：

```cpp
namespace ch {
// ... 使用 ch::core::ch_bool, ch::core::ch_uint 等
}
```

### 触发修复的条件

1. 下次对 bundle 层进行大规模重构时
2. 出现命名碰撞导致的编译错误时
3. 需要引入第三方库且存在命名冲突时

---

## 3. 变更日志

| 日期 | 版本 | 变更 | 作者 |
|------|------|------|------|
| 2026-05-11 | v1.0 | 初始版本：记录 using namespace ch::core 命名空间污染现状和修复方案 | Sisyphus |

---

**相关链接**:
- `include/ast/instr_mem.h` — 文件 1/8
- `include/bundle/axi_bundle.h` — 文件 2/8
- `include/bundle/stream_bundle.h` — 文件 7/8
- `include/chlib/assert.h` — 文件 8/8
- `include/chlib/if_stmt.h` — 已修复的参考案例
- `docs/adr/ADR-DISCUSSION-PLAN.md` — 议题 #27
