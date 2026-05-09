# ADR-015: 拓扑排序作为单一 IR

**状态**: ✅ 已采纳
**日期**: 2026-05-07
**决策人**: Sisyphus + 用户（参考 Oracle 分析）

---

## 1. 背景

CppHDL 的仿真和代码生成共享同一个中间表示（IR）—— `context` 中的 `eval_list`，通过拓扑排序驱动。

**关键文件**：
- `src/core/context.cpp:170-399` — 拓扑排序实现（`topological_sort_visit()`、`get_eval_list()`）
- `src/simulator.cpp:538` — 模拟器获取 `eval_list` 的位置
- `src/simulator.cpp:604-639` — 模拟器将节点重新分类为 `input_instr_list_`、`combinational_instr_list_`、`sequential_instr_list_`
- `src/codegen_verilog.cpp:65` — Verilog 代码生成器获取 `eval_list` 存储为 `sorted_nodes_`

---

## 2. Q1: 单一 IR 策略分析

### 2.1 现状

**单一 IR 的证据**：

| 消费者 | 位置 | 使用方式 |
|--------|------|---------|
| 模拟器 | `simulator.cpp:538` | `ctx->get_eval_list()` 获取后重新分类到 input/combinational/sequential 三组 |
| Verilog 代码生成器 | `codegen_verilog.cpp:65` | 直接使用 `sorted_nodes_` 顺序生成声明和逻辑 |

**当前工作原理**：

拓扑排序已在寄存器（`type_reg`）和内存（`type_mem`）处断开循环（`context.cpp:276-289`），因此组合逻辑自然排在顺序元件之前。模拟器随后按节点类型（`node->type()`）重新分类，不依赖 `eval_list` 的原始顺序进行实际求值。

### 2.2 风险评估

ADR-009（边沿触发仿真）可能需要交错式的组合/顺序求值（如异步复位、时钟门控），但这是推测性的。在 ADR-009 实施并证明当前顺序不足之前，拆分 IR 是过早优化。

### 2.3 决议

**保持单一 IR**。当前架构已有清晰分离：context 提供 DAG + 有效拓扑序，消费者在其上叠加自己的语义层。如 ADR-009 需要不同顺序，添加 `get_eval_list(strategy)` 重载，而非完整的并行 IR。

---

## 3. Q2: 排序稳定性分析

### 3.1 实现证据

| 容器 | 类型 | 在排序中的作用 | 影响输出？ |
|------|------|--------------|-----------|
| `node_storage_` | `std::vector<std::unique_ptr<lnodeimpl>>`（`context.h:146`） | `for (const auto &node_ptr : node_storage_)`（`context.cpp:131`）迭代，插入顺序 | **是** |
| `srcs_` | `std::vector<lnodeimpl*>`（`lnodeimpl.h:211`） | `for (uint32_t i = 0; i < node->num_srcs(); ++i)`（`context.cpp:338`）索引顺序 | **是** |
| `visited`/`temp_mark` | `std::unordered_map<lnodeimpl*, bool>` | 仅用于 `count()`/`operator[]`/`insert()` 查询 | **否** |
| `cyclic_nodes` | `std::unordered_set<lnodeimpl*>` | 仅用于 `count()`/`insert()` 查询 | **否** |

### 3.2 分析结论

**排序是确定性**的。所有 `std::unordered_*` 容器仅用于 O(1) 查找，从不迭代。遍历顺序完全由 `std::vector` 的插入顺序（`node_storage_`）和索引顺序（`srcs_`）控制。

唯一潜在风险：`node_storage_` 的插入顺序依赖上游节点创建顺序。已验证代码库中节点构建路径无 `std::unordered_map` 迭代，不会向 `node_storage_` 引入非确定性。

### 3.3 决议

**无需代码修改**。在 `get_eval_list()`（`context.cpp:113`）处添加确定性保证注释。可选优化：将 `visited`/`temp_mark` 替换为 `std::vector<char>` 按 `node->id()` 索引，可提升性能并消除未来风险。

---

## 4. Q3: 循环检测与恢复分析

### 4.1 当前实现

| 方面 | 位置 | 行为 |
|------|------|------|
| 检测触发 | `context.cpp:274` | `if (temp_mark[node])` — 经典 DFS 回边检测 |
| 允许的循环 | `context.cpp:276-288` | `type_reg` 和 `type_mem` 作为顺序逻辑循环断路器（正确） |
| 禁止的循环 | `context.cpp:290-292` | `CHFATAL_EXCEPTION("Cycle detected... (node type: %s, id: %u, name: %s)")` |
| 冗余检查 | `context.cpp:298-316` | `cyclic_nodes` 集合重复相同逻辑 |
| 恢复机制 | — | **无**。致命异常，不返回部分排序 |

### 4.2 错误信息质量

当前消息报告检测到循环的**节点 type、ID 和名称**，但不报告：
- 完整循环路径（哪些节点形成环路）
- 问题连接的原位置
- 设计中的所有环路（仅在第一个失败）

### 4.3 决议

1. **立即（短工作量）**：增强错误信息以打印完整循环路径。`temp_mark` 已跟踪 DFS 栈，将其捕获为 `std::vector<lnodeimpl*>` 并追加到异常消息。
2. **远期（中等工作量）**：添加 `get_eval_list(strict=true)` 参数，`strict=false` 时返回部分排序 + 环路节点列表，支持一次报告所有环路。

---

## 5. 决议总结

| 问题 | 决议 | 优先级 | 工作量 |
|------|------|--------|--------|
| **Q1** 单一 IR | **保持** — 模拟器已通过类型分类叠加语义 | 低 | — |
| **Q2** 稳定性 | **文档化** — 添加确定性注释；可选优化为 `std::vector<char>` | 低 | 快 |
| **Q3** 环路错误 | **改进** — 添加环路路径跟踪到异常消息 | 🔴 高 | 短 |

**首要行动**：实施 Q3 的环路路径打印。这是最高价值的改动（最小工作量，最大调试价值）。

---

## 6. 变更记录

| 日期 | 版本 | 变更 | 作者 |
|------|------|------|------|
| 2026-05-07 | v1.0 | 初始版本，记录 Q1-Q3 Oracle 分析与决议 | Sisyphus + 用户 |

---

**相关链接**:
- `src/core/context.cpp:113` — `get_eval_list()` 定义
- `src/core/context.cpp:170-399` — 拓扑排序实现
- `src/core/context.cpp:274-292` — 循环检测和错误报告
- `src/simulator.cpp:538` — 模拟器获取 eval_list
- `src/simulator.cpp:604-639` — 模拟器节点重新分类
- `src/codegen_verilog.cpp:65` — Verilog 代码生成器使用 eval_list
- `docs/adr/ADR-DISCUSSION-PLAN.md` — 议题 #10