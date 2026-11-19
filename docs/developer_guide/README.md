# 开发者指南 (Developer Guide)

> **目标读者**: 维护 / 扩展 CppHDL 框架的开发者
> **最后更新**: 2026-11-19（历史技术报告已移至 [`archive/tech-reports/`](../archive/tech-reports/)）

---

## 🔧 导航

### 指南（3 篇）

| 文档 | 说明 |
|---|---|
| [verilator-integration.md](verilator-integration.md) | Verilator 子模块集成、构建配方、支持的平台（Ubuntu-only）、常见构建问题 |
| [openspec-specs-guide.md](openspec-specs-guide.md) | OpenSpec 主规范的目录布局与写法（`openspec/specs/<capability>/spec.md`） |
| [rv32i-rtl-delivery-and-sim.md](rv32i-rtl-delivery-and-sim.md) | RV32I 核的 RTL 交付流程 + Verilator 仿真环境（一条命令跑通） |

### 内部架构（1 篇）

| 文档 | 说明 |
|---|---|
| [architecture/verilog-codegen-architecture.md](architecture/verilog-codegen-architecture.md) | Verilog 代码生成器（AST → SystemVerilog，`always_ff` + `logic`）的内部结构 |

### 开发模式（4 篇）

| 文档 | 说明 |
|---|---|
| [patterns/COMPONENT-LIFECYCLE-GUIDE.md](patterns/COMPONENT-LIFECYCLE-GUIDE.md) | `ch_device` vs `ch_module` 的生命周期与常见误用（SIGSEGV 根因） |
| [patterns/HIERARCHICAL-RTL-AUTHORING-GUIDE.md](patterns/HIERARCHICAL-RTL-AUTHORING-GUIDE.md) | 多层模块 / 层级粒度（成层声明）/ 层次化 RTL 的写法纪律 |
| [patterns/JIT-DEBUGGING-GUIDE.md](patterns/JIT-DEBUGGING-GUIDE.md) | JIT 编译器的调试方法 |
| [patterns/PORT-NAMING-GUIDE.md](patterns/PORT-NAMING-GUIDE.md) | 端口命名（具名构造写法 + 已知限制） |

---

## 📦 已归档（不在本节维护）

| 内容 | 位置 | 为什么归档 |
|---|---|---|
| T001 Simulator API 技术文档（含实现细节） | [`archive/tech-reports/T001-simulator-api.md`](../archive/tech-reports/T001-simulator-api.md) | 使用面已由 [`usage_guide/01-simulator-api.md`](../usage_guide/01-simulator-api.md) 覆盖 |
| T002 Verilog 命名修复分析 | [`archive/tech-reports/`](../archive/tech-reports/) | 一次性缺陷分析（已修复） |
| Bundle 连接问题 / pipeline tick crash | [`archive/tech-reports/`](../archive/tech-reports/) | 历史缺陷分析 |
| JIT / Verilog codegen 实施计划 | [`archive/tech-reports/`](../archive/tech-reports/) | 已完成 |

---

## 🔗 相关

- [用户指南](../usage_guide/README.md) —— API 使用方法
- [架构主题](../architecture/README.md) —— 设计推导与当前态权威
- [决策正本](../adr/) —— ADR
- [技能库](../skills/README.md) —— 速查与修复模式
- [仿真语义](../simulation/ARCHITECTURE.md) —— 内存读端口时序（`sread` / `aread`）

---

**维护**: AI Agent
