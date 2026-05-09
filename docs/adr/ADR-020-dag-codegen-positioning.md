# ADR-020: DAG 代码生成器定位

**状态**: ✅ 已采纳
**日期**: 2026-05-08
**决策人**: Sisyphus + 用户

---

## 1. 背景

`src/codegen_dag.cpp`（301 行）和 `include/codegen_dag.h`（81 行）定义了一个 `dagwriter` 类，用于从 CppHDL 的内部 DAG 节点图生成 Graphviz DOT 格式输出。

### 1.1 与 `verilogwriter` 的对比

| 特性 | `verilogwriter` | `dagwriter` |
|------|----------------|-------------|
| 输出格式 | **Verilog** (.v) | **DOT 图** (.gv) |
| 实际用途 | 代码生成（综合/仿真） | 调试可视化 |
| 代码行数 | 530 行 | 301 行 |
| 空 `catch` 块 | 8 处 | 4 处 |
| 输出目标 | 可综合 RTL | Graphviz 渲染 |
| 接受 Simulator | 否 | ✅ 可标注仿真值 |

### 1.2 代码重复分析

`dagwriter` 与 `verilogwriter` 之间存在大量重复代码：

| 重复逻辑 | `dagwriter` 位置 | `verilogwriter` 位置 |
|---------|-----------------|---------------------|
| 构建 `node_uses_` 映射 | `codegen_dag.cpp:80-88` | `codegen_verilog.cpp:52-60` |
| 名称去重逻辑 | `codegen_dag.cpp:92-103` | `codegen_verilog.cpp:64-75` |
| `sanitize_name()` | `codegen_dag.cpp:165-178` | `codegen_verilog.cpp:96-109` |
| `sorted_nodes_` 初始化 | `codegen_dag.cpp:105-106` | `codegen_verilog.cpp:77-78` |
| 空 `catch(...){}` | `codegen_dag.cpp:107-109, 148-149, 160-162` | 8 处 |

### 1.3 命名问题

文件命名为 `codegen_dag.h/cpp` 暗示其属于"代码生成"类，实际输出的是 Graphviz DOT 可视化格式，并非可综合/可仿真的代码。这容易造成误解。

---

## 2. 讨论过程

### 2.1 初始分析（Sisyphus）

识别了四个关键问题：
1. 定位不明确（可视化工具被归类为代码生成器）
2. 代码大量重复（构造函数逻辑、`sanitize_name()` 与 `verilogwriter` 几乎一致）
3. 空 `catch` 块（与 ADR-019 相同的反模式）
4. 命名具有误导性（`codegen_dag` 暗示代码生成）

### 2.2 Oracle 分析

| # | 问题 | 推荐方案 |
|---|------|---------|
| Q1 | 定位？ | **A) 调试可视化工具** — 重新命名以消除误导 |
| Q2 | 代码重复？ | **A) 提取共享基类** — 抽取 `node_writer_base` |
| Q3 | 空 catch？ | **A) 同步修复** — 同 ADR-019 策略 |
| Q4 | 保留？ | **A) 保留** — 对调试有实际价值 |

---

## 3. 决议

### 3.1 Q1: 定位为调试可视化工具

`dagwriter` 的定位是 **调试可视化工具**，而非代码生成器。它的输出是 DOT 格式的节点图，供开发者在调试设计时查看 DAG 结构、节点类型、位宽、操作码和运行时的仿真值。

**决议**：重新命名以消除误导。建议方案：
- 文件重命名：`codegen_dag.h/cpp` → `dag_viz.h/cpp` 或 `dag_viewer.h/cpp`
- 类重命名：`dagwriter` → `dag_viz_writer` 或 `dag_viewer`
- 函数重命名：`toDAG()` → `view_dag()` 或 `dump_dag()`
- 保留旧名称为别名（向后兼容）

### 3.2 Q2: 提取共享基类

两个 writer 之间有约 70 行完全相同的代码（构造函数的节点遍历、名称去重、sanitize_name）。建议提取共享基类：

**新增**: `include/node_writer_base.h` + `src/node_writer_base.cpp`

```cpp
class node_writer_base {
public:
    explicit node_writer_base(ch::core::context *ctx);
    virtual ~node_writer_base() = default;
    virtual void print(std::ostream &out) = 0;

protected:
    std::string sanitize_name(const std::string &name) const;
    std::string get_width_str(uint32_t size) const;

    ch::core::context *ctx_;
    std::unordered_map<ch::core::lnodeimpl *, std::string> node_names_;
    std::vector<ch::core::lnodeimpl *> sorted_nodes_;
    std::unordered_map<ch::core::lnodeimpl *,
                       std::unordered_set<ch::core::lnodeimpl *>> node_uses_;
};
```

继承关系：
```
node_writer_base  (共享: 构造函数逻辑, sanitize_name, node_names_, sorted_nodes_, node_uses_)
  ├── verilogwriter  (新增: get_op_str, print_header/body/footer, print_op/reg/proxy/input/output)
  └── dagwriter      (新增: get_node_type_str, print_header/nodes/edges/footer, data_map_)
```

**收益**：
- 消除 ~70 行重复代码
- 统一名称处理逻辑（修改一处即生效）
- 为未来可能的新 writer 提供基础（如 VCD writer、UHDM writer）

### 3.3 Q3: 移除空 `catch` 块

同 ADR-019 策略：移除 `dagwriter` 中 4 处空 `catch(...){}`，仅在 `toDAG()` 顶层保留异常日志。

| 位置 | 当前 | 目标 |
|------|------|------|
| `dagwriter()` 构造函数:107-109 | 空 catch | **移除** |
| `dagwriter(ctx, sim)` 构造函数:148-149 | 空 catch | **移除** |
| `print()`:160-162 | 空 catch | **移除** |
| `toDAG()`:37-43, 65-71 | 顶层空 catch | **改为日志** `std::cerr << e.what()` |

### 3.4 Q4: 保留

`dagwriter` 对调试和设计验证有实际价值，特别是通过 Graphviz 可视化 DAG 结构并在边上标注仿真值，可以帮助开发者：
- 验证连接正确性
- 理解拓扑排序结果
- 调试循环检测问题
- 直观查看节点间依赖关系

**决议**：保留并改善，而非删除。

---

## 4. 执行计划

| 步骤 | 内容 | 涉及文件 | 工作量 | 优先级 |
|------|------|---------|--------|--------|
| 1 | 提取 `node_writer_base` 共享基类 | 新增 `include/node_writer_base.h`, `src/node_writer_base.cpp` | 2h | 中 |
| 2 | `verilogwriter` 继承 `node_writer_base`，删除重复代码 | `codegen_verilog.h/cpp` | 1h | 中 |
| 3 | `dagwriter` 继承 `node_writer_base`，删除重复代码 | `codegen_dag.h/cpp` | 1h | 中 |
| 4 | 移除 `dagwriter` 空 `catch` 块 | `codegen_dag.cpp` | 10 min | 低 |
| 5 | 文件重命名：`codegen_dag` → `dag_viz` | 文件名 + 所有引用 | 30 min | 低 |
| 6 | 更新 CMakeLists.txt | `CMakeLists.txt` | 10 min | 低 |

---

## 5. 变更日志

| 日期 | 版本 | 变更 | 作者 |
|------|------|------|------|
| 2026-05-08 | v1.0 | 初始版本，记录 DAG 生成器定位分析和决议 | Sisyphus + 用户 |

---

**相关链接**:
- `src/codegen_dag.cpp` — DAG 生成器实现（将重命名为 `dag_viz.cpp`）
- `include/codegen_dag.h` — DAG 生成器头文件（将重命名为 `dag_viz.h`）
- `src/codegen_verilog.cpp` — Verilog 代码生成器（将共享基类）
- `include/codegen_verilog.h` — Verilog 代码生成器头文件
- `docs/adr/ADR-019-verilog-codegen-completeness.md` — 关联的 Verilog 代码生成 ADR
- `docs/adr/ADR-DISCUSSION-PLAN.md` — 议题 #14
