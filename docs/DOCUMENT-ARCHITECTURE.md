# CppHDL 文档架构规范 v2.2

**文档编号**: CPPHDL-DOC-ARCH-001  
**创建日期**: 2026-04-09  
**最后更新**: 2026-11-19（**v2.2**：目录重组 —— 活文档区 483→40 篇，历史材料全部移入 `docs/archive/`；见 §10）  
**状态**: ✅ 已批准（⚠️ **2026-11-19 订正**：§8.1 的批准记录原写「✅ 已批准」而同表「审稿 / 批准 = 待定」⇒ 矛盾**原样保留**并在此标注；**签署状态属事实史料，本规范不替用户改状态**）  
**维护人**: AI Agent  


---

## 📋 1. 概述

本规范定义了 CppHDL 项目的完整文档体系架构，确保文档的组织结构清晰、易于维护、方便读者查找。

### 1.1 文档分类原则

文档按**目标读者**和**内容性质**两个维度分类：

```
维度 1: 目标读者
├─ 用户 (User) → 使用 CppHDL 设计硬件的工程师
└─ 开发者 (Developer) → 维护/扩展 CppHDL 框架的工程师

维度 2: 内容性质
├─ 概念性 (Conceptual) → "是什么"、"为什么"
├─ 指导性 (Instructional) → "怎么做"、步骤指南
├─ 参考性 (Reference) → API 规格、参数说明
└─ 诊断性 (Diagnostic) → 故障排除、常见问题
```

### 1.2 文档矩阵

| 读者类型 | 概念性 | 指导性 | 参考性 | 诊断性 |
|---------|-------|-------|-------|-------|
| **用户** | 01-introduction.md | 02-quickstart.md<br>usage_guide/*.md | api-reference/*.md | troubleshooting.md |
| **开发者** | architecture/*.md | contributing.md<br>dev-guide/*.md | internal-api/*.md | debugging-guide.md |

---

## 📁 2. 目录结构

### 2.1 完整目录树（**2026-11-19 重组后实测**）

> ⚠️ 本节曾两度失真：2026-04-09 版列了 **21 个从未存在的文件**（导致 27 处死链）；
> 2026-11-19 上午版仍把 **141 篇历史计划混在活区**。本版按**重组后**的磁盘状态重写。
>
> **判据**：活文档区只放「导航入口 / 当前态权威 / 现行指南」；**历史计划与报告一律进 `archive/`**。

```
CppHDL/
├── README.md · QUICK_REFERENCE.md · READING-GUIDE.md · AGENTS.md
│
├── docs/
│   ├── README.md                # 文档中心（按角色 / 任务导航）
│   ├── DOCUMENT-ARCHITECTURE.md # 本规范（治理规则正本）
│   ├── PROJECT-OVERVIEW.md      # 项目概览
│   ├── CHANGELOG.md             # 变更流水（按日期倒序）
│   │
│   ├── usage_guide/             # 📘 用户指南 · 6 篇 + README（编号 01–06 连续）
│   │   ├── 01-simulator-api.md · 02-component-design.md · 03-bundle-patterns.md
│   │   └── 04-testbench-guide.md · 05-verilator-backend.md · 06-spinalhdl-comparison.md
│   │
│   ├── developer_guide/         # 🔧 开发者指南 · 9 篇
│   │   ├── verilator-integration.md · openspec-specs-guide.md · rv32i-rtl-delivery-and-sim.md
│   │   ├── architecture/verilog-codegen-architecture.md
│   │   └── patterns/（COMPONENT-LIFECYCLE · HIERARCHICAL-RTL-AUTHORING · JIT-DEBUGGING · PORT-NAMING）
│   │
│   ├── simulation/              # 仿真语义 · 1 篇（ARCHITECTURE.md = memory read timing 权威）
│   ├── architecture/            # 🏛 当前态架构主题 · 4 篇 + 索引（README 有判读纪律）
│   ├── skills/                  # ⚡ 技能库 · 6 项活（5 skill 目录 + 1 速查）+ archive/（11 篇）
│   ├── riscv-mini/              # riscv-mini **测试知识入口** · 1 篇
│   ├── adr/                     # ⚖️ 决策正本 · 76 篇（**决策以它为准**）
│   │
│   └── archive/                 # 📦 历史归档 · 141 篇 · **14 个分类目录 + 索引**
│       ├── phase-plans/(35) · phase-reports/(20) · architecture-history/(27)
│       ├── tech-reports/(10) · simulation-plans/(7) · superpowers/(7)
│       ├── freertos-planning/(6) · old-usage-guides/(7) · daily-reports/(13)
│       └── spinalhdl-comparison/(2) · problem-reports/(2) · agents-notes/(1)
│           · learning/(1) · root-scratch/(3)
│
├── openspec/（specs/ 正典 · changes/ 流程）· .rddf/（路线图 / 改进 / 登记表 / 验收）
└── examples/ · tests/ · samples/ · include/ · src/ · scripts/（护栏）
```

> ⚠️ **分级口径（L1/L2/L3/L4）见 §9.1** ⇒ ⇒ 「哪些文档的死链算缺陷」由它决定。
> ⚠️ **归档区内部**的相对链接按移动前路径书写（`docs/archive/README.md` §2 有旧→新对照表）。


---

## 📝 3. 文档编写规范

### 3.1 文件命名规范

```
格式：<序号>-<短标题>.md  或  <任务编号>-<内容>.md

示例:
✅ 01-introduction.md          # 带序号的主文档
✅ 01-simulator-api.md         # 带序号的使用指南
✅ T001-simulator-api.md       # T001 任务相关 API 参考
✅ T001-analysis.md            # T001 技术分析报告
❌ simulatorAPI.md             # 缺少序号
❌ 04_Simulator_API.md         # 使用下划线而非短横线
```

### 3.2 文档头部规范

每份文档必须包含标准头部：

```markdown
# 文档标题

**文档编号**: CPPHDL-XXX-001  
**创建日期**: 2026-04-09  
**最后更新**: 2026-04-09  
**状态**: Draft | Review | Approved | Deprecated  
**维护人**: 姓名/角色  
**目标读者**: User | Developer | Both  
```

### 3.3 文档结构规范

```markdown
# 标题

## 1. 概述/简介
- 文档目的
- 适用范围
- 快速链接

## 2. 核心内容
- 分章节展开
- 代码示例
- 图表说明

## 3. 使用指南/实现细节
- 步骤说明 (用户)
- 技术细节 (开发者)

## 4. 最佳实践/设计决策
- 推荐做法
- 决策理由

## 5. 常见问题/已知限制
- Q&A
- 注意事项

## 6. 参考资源
- 相关链接
- 进一步阅读

## 附录
- 补充材料
```

---

## 🎯 4. 文档质量要求

### 4.1 内容质量标准

| 质量维度 | 要求 | 检查方式 |
|---------|------|---------|
| **准确性** | 代码示例可运行，数据准确 | 自动化测试 + 人工审查 |
| **完整性** | 覆盖所有使用场景 | 检查清单验证 |
| **一致性** | 术语、格式统一 | 文档审查工具 |
| **可读性** | 结构清晰，语言简洁 | Flesch 可读性 > 60 |
| **可维护性** | 模块化，易更新 | 文档结构审查 |

### 4.2 代码示例规范

```cpp
// ✅ 好的示例：完整、可运行、有注释
#include "ch.hpp"

using namespace ch::core;

TEST_CASE("Simulator API - Basic Usage", "[example]") {
    auto ctx = std::make_unique<ch::core::context>("test");
    ch::core::ctx_swap ctx_guard(ctx.get());
    
    ch_uint<8> data(0_d);
    Simulator sim(ctx.get());
    
    sim.set_input_value(data, 0xA5);  // 设置值
    auto val = sim.get_input_value(data);  // 读取值
    
    REQUIRE(static_cast<uint64_t>(val) == 0xA5);
}

// ❌ 差的示例：不完整、无注释、不可运行
ch_uint<8> data;
sim.set_input_value(data, x);  // x 未定义
```

---

## 🔄 5. 文档维护流程

### 5.1 文档生命周期

```
创建 → 审稿 → 发布 → 维护 → 归档/废弃
 │      │      │      │       │
 │      │      │      │       └─ 内容过时
 │      │      │      └─ 定期审查 (季度)
 │      │      └─ CI 检查 ✅ 已实现（2026-11-19）：`scripts/verify_docs_links.sh`
 │      │         （C1 链接可达 · C2 引用行号 · C3 本机路径 · C4 自检 · C5 分级覆盖）
 │      └─ 至少 2 人审稿
 └─ 创建者起草
```

### 5.2 更新触发条件

| 触发事件 | 受影响文档 | 更新时限 |
|---------|-----------|---------|
| 新功能发布 | usage_guide, api-reference | 发布前 |
| API 变更 | api-reference, skills | 变更同步 |
| Bug 修复 | troubleshooting | 修复后 1 周内 |
| Phase 完成 | plans, tech-reports | Phase 结束时 |

### 5.3 文档审查清单

**发布前检查**:
- [ ] 头部信息完整
- [ ] 代码示例可运行
- [ ] 内部链接有效
- [ ] 术语使用一致
- [ ] 语法拼写正确
- [ ] 目标读者明确

---

## 📊 6. 文档统计指标

| 类别 | 数量 | 说明 |
|------|-----|------|
| usage_guide | **6 篇** + README | 编号 01–06 连续（旧 04/05/06/09/10/11 已重排） |
| developer_guide | **9 篇** | 3 指南 + 1 架构 + 4 模式 + 1 README |
| simulation | **1 篇** | 只留 `ARCHITECTURE.md` |
| architecture | **4 篇** + 索引 | 只留当前态权威（历史 26 篇已归档） |
| skills | **6 项活 + 11 归档** | 仓根 `skills/` 已并入 |
| riscv-mini | **1 篇** | `test-architecture.md`（**测试知识入口**：资产地图 / 完整清单 / 跑法 / 门禁 / 判据纪律 / 护栏矩阵 / 锚点）|
| adr（决策正本） | **76 篇** | L2 历史快照 |
| **archive（归档）** | **141 篇** | 14 个分类目录 + 索引 |
| **活文档区合计** | **约 40 篇** | 重组前 483 篇 |

### 6.2 质量指标

| 指标 | 目标值 | 测量方式 |
|------|-------|---------|
| 文档覆盖率 | ≥90% API 有文档 | API 扫描 |
| 示例可运行率 | 100% | CI 测试 |
| 平均更新周期 | ≤90 天 | Git 历史 |
| 读者满意度 | ≥4.0/5.0 | 调查反馈 |

---

## 🗺️ 7. T401 文档迁移计划（**历史记录 · 2026-04**）

> ⚠️ **本节是 2026-04-09 的一次性迁移计划**，其中的文件名（`04-simulator-api.md` / `07-stream-patterns.md` 等）
> 反映**当时**的规划 ⇒ **不是**当前结构。
> **当前结构以 §2.1 为准**；旧编号 → 新编号的对照见 [`archive/README.md` §2](archive/README.md)。
> 本节保留原文，仅作「那次迁移怎么做的」的记录。

### 7.1 现有文档映射

| 现有文档 | 迁移目标 | 内容调整 |
|---------|---------|---------|
| `docs/tech/T401-TECHNICAL-DOC.md` | `archive/tech-reports/T001-simulator-api.md` | 强化实现细节，添加架构图 |
| `docs/tech/T401-TECHNICAL-DOC.md` 部分 | `usage_guide/03-bundle-patterns.md` | 提取 Bundle 使用示例 |
| `docs/skills/SIMULATOR-API-GUIDE.md` | `skills/simulator-api-quickref.md` | 简化为速查表 |
| 无 | `usage_guide/01-simulator-api.md` | 新建完整使用指南 |
| `docs/plans/T401-COMPLETION-REPORT.md` | `archive/phase-plans/T001-completion-report.md` | 重命名，格式调整 |

### 7.2 新增文档列表

| 文档 | 位置 | 优先级 | 工时估算 |
|------|------|--------|---------|
| docs/README.md | 文档中心 | 🔴 高 | 30min |
| usage_guide/README.md | 使用指南索引 | 🔴 高 | 20min |
| developer_guide/README.md | 开发指南索引 | 🔴 高 | 20min |
| usage_guide/01-simulator-api.md | 完整使用指南 | 🔴 高 | 1h |
| developer_guide/architecture/simulator-internals.md | 架构文档 | 🟡 中 | 1h |
| skills/README.md | 技能索引 | 🟡 中 | 15min |

### 7.3 执行时间线

```
第 1 阶段：规划 (当前)
└─ 完成文档架构规范 ✅

第 2 阶段：基础结构 (1h)
├─ 创建目录结构
├─ 创建 README 索引
└─ 建立文档模板

第 3 阶段：T401 迁移 (2h)
├─ 迁移技术文档
├─ 重写使用指南
├─ 创建速查技能
└─ 更新内部链接

第 4 阶段：验证 (30min)
├─ 检查链接
├─ 验证示例
└─ 提交重构

总计：4.5 小时
```

---

## ✅ 8. 批准与实施

### 8.1 批准记录

| 角色 | 姓名 | 日期 | 状态 |
|------|------|------|------|
| 作者 | AI Agent | 2026-04-09 | ✅ 已创建 |
| 审稿 | 待指定 | 待定 | ⏳ 待审稿 |
| 批准 | 用户 | 待定 | ⏳ 待批准 |

> ⚠️ **2026-11-19 订正（本规范不替用户改状态）**：本表**自相矛盾** —— 文档头部标「✅ 已批准」，而本表「审稿 / 批准」两行仍「⏳ 待定」⇒ ⇒ **签署状态属事实史料** ⇒ ⇒ **原样保留 + 在此标注**（不代用户把「待批准」改成「已批准」）。后续若要正式批准，由**用户**在本表两行签署 ⇒ 本规范随之去掉本注。

### 8.2 实施检查清单

- [ ] 创建目录结构
- [ ] 迁移现有文档
- [ ] 新增必要文档
- [ ] 更新内部链接
- [ ] 验证所有示例
- [ ] 提交重构
- [ ] 通知团队成员

---

**版本**: v2.0  
**下次审查**: 2026-07-09  
**相关文档**: PHASE4-PLAN-2026-04-09.md


---

## 📐 9. 文档治理口径（2026-11-19 新增）

> ⚠️ 本节是**治理规则的正本**；`docs/architecture/2026-11-19-document-governance.md` 承载**本次审计与裁决**（证据 / 选项对比 / 实施范围）⇒ ⇒ 两者冲突时**以本节为准**（规则），审计过程以那份为准。

### 9.1 文档分级（L1 / L2 / L3 / L4）

| 级别 | 判据 | 读者义务 | 治理义务 |
|---|---|---|---|
| **L1 活文档** | 导航入口或当前态权威 | **可直接作实现依据** | 死链 MUST 归零；行号引用 MUST 准；示例 MUST 可编译 |
| **L2 历史快照** | 记录当时的判断 / 计划 / 评审 | 只作历史参考 | **不改写正文**；dated addendum 追落；死链 / 行号漂移**不追** |
| **L3 过程记录** | 计划 / 报告 / 迁移计划 | 同 L2 | 同 L2 |
| **L4 外部** | 第三方文档（`external/**`） | 同上游 | 不归本仓管 |

**分级来源两级**：

① **目录前缀默认**（零维护）
   - **L1** = `docs/usage_guide/` · `docs/developer_guide/` · `docs/skills/` · `docs/simulation/` · `docs/riscv-mini/`
   - **L2/L3** = `docs/adr/` · `docs/archive/`（**含全部 14 个子目录**）· `.rddf/` · `openspec/changes/archive/`
② **混合目录逐文件登记**（`docs/architecture/` · `docs/` 根 · 仓根散落 `*.md`）⇒ ⇒ 登记表 MUST 由护栏 `--render` **派生**、禁手改（`.rddf/checks/docs-link-scope.md`）。

⚠️ **2026-11-19 重组后的效果**：活文档区只剩 ~40 篇 L1 ⇒ C1/C2 的扫描面**从 483 降到 62**，
伪缺陷随之归零（历史材料的引用漂移不再进护栏视野）。

### 9.2 引用有效性（`file:line`）

| 级别 | 口径 | 理由 |
|---|---|---|
| L1 | MUST **准**（逐条重定位 + 核实被引行内容） | 是实现依据 |
| L2 / L3 | **不追**（历史锚点） | 改成今天的行号 = 篡改历史语境 |

✅ **正向模式**：**L1 负责刷新 pin · ADR 保留历史 pin** ⇒ L1 引用 ADR 决策时 MUST **按符号 / 内容重新锚定**。

### 9.3 死链处置

| 死链所在 | 处置 |
|---|---|
| L1 导航 | MUST 修（改指核实过的现存文件）；目标不存在 ⇒ **delink**（去链保留文本 + 标「文件尚未创建」）⇒ 不删行 |
| L2 / L3 | 不追（加 dated 注记即可） |
| L4 | 不动 |

### 9.4 本机路径禁令（P1 · 级别无关）

任何**被跟踪文档** MUST NOT 含本机绝对路径（`/workspace/` · `/home/`）。⚠️ **豁免 `docs/archive/**`**（仓规「历史条目逐字保留」区）。

| 级别 | 处置 |
|---|---|
| L1 | 改为仓内相对路径 |
| L2 / L3 | **delink**（行内代码 + dated 注记）⇒ 不得 relink 到不存在的目标 |

### 9.5 自动化门（`scripts/verify_docs_links.sh`）

| # | 判据 |
|---|---|
| C1 | L1 的 Markdown 链接 MUST 全可达（排除代码块内的 `[..](..)`） |
| C2 | L1 的 `file:line` 引用 MUST 文件存在 + 行号在范围内（L2/L3 不扫） |
| C3 | 全仓 MUST NOT 含本机绝对路径（排除 `archive/**`） |
| C4 | `--self-test` MUST 自证判别力（投毒 ⇒ 必红） |
| C5 | **分级覆盖率**（fail-closed）：任一 `.md` 无法归级 ⇒ 红 |

---

## 📋 10. 变更记录（v2.2 · 2026-11-19）

| 章节 | 变更 | 依据 |
|---|---|---|
| §2.1 目录树 | 从**规划态**校准为**当前态**（原列的 21 个文件不存在 ⇒ 删；计划中者标「规划中」） | 导航照它建 ⇒ 实测 27 处死链 |
| §5.1「CI 检查」 | 从**空头支票**改为指向实际护栏 `scripts/verify_docs_links.sh`（C1–C5） | 仓内此前无任何文档类护栏 |
| §6.1 统计表 | 按实测回填（usage_guide 7 · developer_guide 19 · adr 76 · architecture 32 …） | 原表停在「0 篇」 |
| §8.1 批准记录 | **如实标注矛盾**（不改写签署状态） | 签署状态属事实史料 |
| **§9（新增）** | 文档治理口径：分级 / 引用有效性 / 死链处置 / 本机路径禁令 / 自动化门 | `architecture/2026-11-19-document-governance.md`（oracle APPROVE） |
| 全文 | 删本机绝对路径（§2.1 的 `«本机»/`） | P1 本机路径禁令 |
| **§2.1 / §6.1（v2.2）** | 按 **2026-11-19 目录重组**重写：活文档区 483→40 篇；141 篇历史材料移入 `docs/archive/` 的 14 个分类目录；usage_guide 重编号 01–06；两个 skills 目录合并到 `docs/skills/` | 用户要求「合并 / 拆分 / 归档 / 重组框架」 |
| **§9.1（v2.2）** | 分级前缀表随重组更新（`archive/` 含全部子目录；新列 `riscv-mini/`） | 同上 |