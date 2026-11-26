# CppHDL 文档中心

> **最后的目录重组**：2026-11-19（活文档区 40 篇 · 归档区 14 个分类目录）
> **判读纪律**：本目录树即当前态；历史文档一律在 [`archive/`](archive/README.md)，**不在此处**。

---

## 1. 从哪儿开始

| 你是 | 去这里 |
|---|---|
| **第一次用 CppHDL** | [项目概览](PROJECT-OVERVIEW.md) → [用户指南](usage_guide/README.md) |
| **写硬件设计** | [用户指南](usage_guide/README.md)（6 篇：Simulator / Component / Bundle / Testbench / Verilator / SpinalHDL 对比） |
| **改框架内部** | [开发者指南](developer_guide/README.md)（9 篇：架构 / 模式 / 集成） |
| **查某个 API 怎么用** | [速查表](skills/simulator-api-quickref.md) · [技能库](skills/README.md) |
| **找架构决策为什么这么定** | [`adr/`](adr/)（决策正本）· [`architecture/`](architecture/README.md)（主题推导） |
| **找旧计划 / 旧报告** | [`archive/`](archive/README.md)（**分类归档 · 含索引**） |

---

## 2. 活文档区（**只有这些是当前态**）

```
docs/
├── README.md                    ← 本文件
├── DOCUMENT-ARCHITECTURE.md     # 文档架构规范 v2.1（治理规则正本）
├── PROJECT-OVERVIEW.md          # 项目概览
├── CHANGELOG.md                 # 变更流水（按日期倒序）
│
├── usage_guide/                 # 📘 用户指南 · 6 篇
│   ├── README.md
│   ├── 01-simulator-api.md          # Simulator API（设置/读取信号、tick、故障排除）
│   ├── 02-component-design.md       # Component 设计（多层模块 / 跨层连线 / 成员接口 / RTL 输出）
│   ├── 03-bundle-patterns.md        # Bundle 使用模式
│   ├── 04-testbench-guide.md        # Testbench 编写
│   ├── 05-verilator-backend.md      # Verilator 仿真后端
│   └── 06-spinalhdl-comparison.md   # 与 SpinalHDL 的 Stream/Flow 用法对比
│
├── developer_guide/             # 🔧 开发者指南 · 9 篇
│   ├── README.md
│   ├── verilator-integration.md             # Verilator 子模块集成与构建
│   ├── openspec-specs-guide.md              # OpenSpec 主规范写法
│   ├── rv32i-rtl-delivery-and-sim.md        # RV32I RTL 交付 + 仿真环境
│   ├── architecture/verilog-codegen-architecture.md
│   └── patterns/                            # 4 篇
│       ├── COMPONENT-LIFECYCLE-GUIDE.md
│       ├── HIERARCHICAL-RTL-AUTHORING-GUIDE.md
│       ├── JIT-DEBUGGING-GUIDE.md
│       └── PORT-NAMING-GUIDE.md
│
├── simulation/                  # 仿真语义 · 1 篇
│   └── ARCHITECTURE.md          # 仿真架构 + 内存读端口时序（AGENTS.md 的权威引用源）
│
├── architecture/                # 🏛 架构主题 · 4 篇 + 索引
│   ├── README.md                # 索引与判读纪律
│   ├── rv32i-microarchitecture.md
│   ├── 2026-11-02-hierarchical-rtl-output.md
│   ├── 2026-11-17-rtl-verification-single-source.md
│   └── 2026-11-19-document-governance.md
│
├── skills/                      # ⚡ 技能库 · 5 活 skill + 1 速查 + 11 归档
│   ├── README.md
│   ├── simulator-api-quickref.md
│   ├── cpphdl-{bundle-patterns, component-lifecycle, input-port-assignment,
│   │          shift-fix, simulator-port-value}/SKILL.md
│   └── archive/                 # 11 个已退役 skill（历史修复模式）
│
├── riscv-mini/                  # riscv-mini **测试知识入口** · 1 篇
│   └── test-architecture.md     # 资产地图 / 完整清单 / 跑法 / 门禁 / 判据纪律 / 护栏 / 锚点
│
└── adr/                         # ⚖️ 决策正本 · 76 篇（ADR-NNN，**决策以它为准**）
```

---

## 3. 归档区（[`archive/`](archive/README.md)）

**历史计划 / 报告 / 复盘 / 早期快照** —— 14 个分类目录，**只作历史参考，不得作为实现依据**。

| 目录 | 内容 |
|---|---|
| `phase-plans/` | Phase 1–6 的实施计划与完成报告（35 篇） |
| `phase-reports/` | Phase 1–4 的验证报告与基准（20 篇） |
| `architecture-history/` | 早期方案快照 + Phase 6 复盘 + `phases/` 规划（27 篇） |
| `tech-reports/` | 技术分析报告（JIT/CONCAT/命名修复/bundle 问题…，10 篇） |
| `simulation-plans/` | 早期性能优化路线图与计划（7 篇） |
| `superpowers/` | 2026-05~09 的 plan/spec（7 篇） |
| `freertos-planning/` | FreeRTOS 支持方案（**未实施**，6 篇） |
| `old-usage-guides/` | 已被取代的旧版使用指南（7 篇） |
| `spinalhdl-comparison/` `agents-notes/` `daily-reports/` `learning/` `problem-reports/` `root-scratch/` | 其余历史材料 |

⚠️ **归档区内的相对链接按移动前路径书写**；如需精确定位，按文件名检索（`git log --follow` 可追溯）。

---

## 4. 根目录相关文档

| 文档 | 说明 |
|---|---|
| [../README.md](../README.md) | 项目简介（对外） |
| [../QUICK_REFERENCE.md](../QUICK_REFERENCE.md) | 开发快速参考 |
| [../READING-GUIDE.md](../READING-GUIDE.md) | 按阅读时长的路径推荐 |
| [../AGENTS.md](../AGENTS.md) | AI 代理指南（每会话注入，**当前态约束**） |

---

## 5. 维护规则

| 类型 | 去哪 |
|---|---|
| **新增活文档** | `usage_guide/`（用户）/ `developer_guide/`（内部）/ `skills/`（速查） |
| **新增架构主题** | `architecture/` + **在 `architecture/README.md` §1 登记** |
| **历史计划 / 完成报告** | **直接放 `archive/<类别>/`** —— 不要留在活文档区 |
| **删文档** | 先移 `archive/`（仓规：存量文档不追溯，历史要留） |
| **链接 / 引用守则** | 见 [DOCUMENT-ARCHITECTURE.md §9](DOCUMENT-ARCHITECTURE.md)；护栏 `scripts/verify_docs_links.sh` |

---

**维护**: AI Agent · **最后更新**: 2026-11-19
