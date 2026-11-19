# 文档阅读指南

> **按你手上有的时间和要做的事选路径**。文档全貌见 [`docs/README.md`](docs/README.md)。
> 最后更新：2026-11-19（旧版本是 2026-04 的一次性交付说明，已过时）

---

## 只有 10 分钟

| 读什么 | 为什么 |
|---|---|
| [`docs/PROJECT-OVERVIEW.md`](docs/PROJECT-OVERVIEW.md) | 项目是什么、现在做到哪 |
| [`docs/usage_guide/01-simulator-api.md`](docs/usage_guide/01-simulator-api.md) | 跑起第一个测试的最小 API 集 |

---

## 半天：写完第一个能跑的设计

1. [`docs/usage_guide/01-simulator-api.md`](docs/usage_guide/01-simulator-api.md) —— 设置输入 / 读输出 / `tick`
2. [`docs/usage_guide/02-component-design.md`](docs/usage_guide/02-component-design.md) —— 组件怎么装成树、跨层怎么连
3. [`docs/usage_guide/04-testbench-guide.md`](docs/usage_guide/04-testbench-guide.md) —— 写 TB、断言、双后端跑
4. 手边放 [`docs/skills/simulator-api-quickref.md`](docs/skills/simulator-api-quickref.md) 速查

---

## 一天：要出 RTL

1. [`docs/usage_guide/02-component-design.md`](docs/usage_guide/02-component-design.md) §5 —— **三种 RTL 出口怎么选**（扁平 / 根组件 / 层次化）
2. [`docs/developer_guide/patterns/HIERARCHICAL-RTL-AUTHORING-GUIDE.md`](docs/developer_guide/patterns/HIERARCHICAL-RTL-AUTHORING-GUIDE.md) —— 层次化写法的纪律
3. [`docs/usage_guide/05-verilator-backend.md`](docs/usage_guide/05-verilator-backend.md) —— 要 RTL 级仿真时的后端用法与边界
4. [`docs/developer_guide/rv32i-rtl-delivery-and-sim.md`](docs/developer_guide/rv32i-rtl-delivery-and-sim.md) —— 完整跑通一个 SoC 的例子

---

## 改框架内部

| 要改什么 | 读什么 |
|---|---|
| Verilog 发射面 | [`docs/developer_guide/architecture/verilog-codegen-architecture.md`](docs/developer_guide/architecture/verilog-codegen-architecture.md) |
| JIT 编译器 | [`docs/developer_guide/patterns/JIT-DEBUGGING-GUIDE.md`](docs/developer_guide/patterns/JIT-DEBUGGING-GUIDE.md) |
| Verilator 后端 | [`docs/developer_guide/verilator-integration.md`](docs/developer_guide/verilator-integration.md) |
| 端口命名 | [`docs/developer_guide/patterns/PORT-NAMING-GUIDE.md`](docs/developer_guide/patterns/PORT-NAMING-GUIDE.md) |
| 组件生命周期（SIGSEGV 排查） | [`docs/developer_guide/patterns/COMPONENT-LIFECYCLE-GUIDE.md`](docs/developer_guide/patterns/COMPONENT-LIFECYCLE-GUIDE.md) |
| 仿真时序语义（`sread` / `aread`） | [`docs/simulation/ARCHITECTURE.md`](docs/simulation/ARCHITECTURE.md) |

---

## 想知道「为什么这么设计」

| 主题 | 读什么 |
|---|---|
| 层次化 RTL 输出 | [`docs/architecture/2026-11-02-hierarchical-rtl-output.md`](docs/architecture/2026-11-02-hierarchical-rtl-output.md) |
| RTL 验证单一真相源 / 生成式 TB | [`docs/architecture/2026-11-17-rtl-verification-single-source.md`](docs/architecture/2026-11-17-rtl-verification-single-source.md) |
| RV32I 微架构 | [`docs/architecture/rv32i-microarchitecture.md`](docs/architecture/rv32i-microarchitecture.md) |
| 文档治理 | [`docs/architecture/2026-11-19-document-governance.md`](docs/architecture/2026-11-19-document-governance.md) |
| **所有已签批决策** | [`docs/adr/`](docs/adr/)（ADR 是**决策正本**，主题文档只承载推导） |

---

## 找旧资料（计划 / 报告 / 复盘）

全部在 [`docs/archive/`](docs/archive/README.md)，**按类别分 14 个目录 + 索引**。
索引里有「**旧路径 → 新位置**」对照表 ⇒ 从 ADR / CHANGELOG 里看到旧路径时按表换算。

---

## 从 SpinalHDL 迁过来

1. [`docs/usage_guide/06-spinalhdl-comparison.md`](docs/usage_guide/06-spinalhdl-comparison.md) —— Stream / Flow 逐条对照
2. [`docs/usage_guide/01-simulator-api.md`](docs/usage_guide/01-simulator-api.md) —— 仿真 API 的差异
3. [`docs/usage_guide/02-component-design.md`](docs/usage_guide/02-component-design.md) —— 组件装配的差异
