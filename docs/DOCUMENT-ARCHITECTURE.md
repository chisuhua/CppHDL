# CppHDL 文档架构规范 v2.1

**文档编号**: CPPHDL-DOC-ARCH-001  
**创建日期**: 2026-04-09  
**最后更新**: 2026-11-19（**v2.1**：按《文档治理口径》校准到当前态 —— 见 §9 变更记录）  
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

### 2.1 完整目录树（**2026-11-19 实测校准**）

> ⚠️ **本节曾长期停留在「规划态」**（2026-04-09 的版本列了 **21 个此后从未存在或已改名的文件**）⇒ ⇒ 导航照它建 ⇒ ⇒ 必然指向不存在的文件（实测 **27 处死链**）⇒ ⇒ 本次校准为**当前态**（判据 = 逐项核对磁盘存在性）。

```
CppHDL/
├── README.md · AGENTS.md · QUICK_REFERENCE.md · READING-GUIDE.md · Compare.md
│
├── docs/
│   ├── README.md                # 📑 文档中心（导航）
│   ├── DOCUMENT-ARCHITECTURE.md # 本规范（**治理正本**）
│   ├── PROJECT-OVERVIEW.md · CHANGELOG.md · phase_f_status.md
│   │
│   ├── usage_guide/             # 📘 用户指南（7 篇 · **L1**）
│   │   └── README · 04-simulator-api · 05-component-design · 06-bundle-patterns
│   │       09-testbench-guide · 10-verilator-backend · 11-stream-flow-comparison
│   ├── developer_guide/         # 🔧 开发者指南（19 篇 · **L1**）
│   │   └── README · verilator-integration · openspec-specs-guide
│   │       rv32i-rtl-delivery-and-sim · api-reference/ · architecture/
│   │       patterns/（4 篇）· tech-reports/（7 篇）· plans/（2 篇 · **L3**）
│   ├── skills/                  # ⚡ 快速参考（14 篇 = 3 活 + 11 归档 · 归档 **L2**）
│   ├── simulation/              # 仿真语义（8 篇 · **L1**：ARCHITECTURE.md 是 memory read timing 权威）
│   ├── adr/                     # ⚖️ 决策正本（76 篇 · **L2**）
│   ├── architecture/            # 🏛 主题文档（32 篇 · **L1/L2 混居** · 判读纪律见其 README）
│   ├── riscv-mini/（2）· freertos/（5 · **L3**）· plans/（35 · **L3**）· reports/（7 · **L3**）· superpowers/（7 · **L3**）
│   └── archive/                 # 40 篇（**逐字保留区** · 历史条目一字不改）
│
├── openspec/（specs/ 正典 · changes/ 流程）· .rddf/（路线图 / 改进 / 登记表 / 验收）
└── examples/ · tests/ · samples/ · include/ · src/ · scripts/（护栏）
```

> ⚠️ **分级口径（L1/L2/L3/L4）见 §9.1** ⇒ ⇒ 「哪些文档的死链算缺陷」由它决定。


---

## 📝 3. 文档编写规范

### 3.1 文件命名规范

```
格式：<序号>-<短标题>.md  或  <任务编号>-<内容>.md

示例:
✅ 01-introduction.md          # 带序号的主文档
✅ 04-simulator-api.md         # 带序号的使用指南
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

### 6.1 数量指标

| 类别 | 目标数量 | 当前数量 | 完成率 |
|------|---------|--------|--------|
| usage_guide | 按需 | **7 篇** | ✅（原规划的 01/02/03/07/08 已并入现有篇目） |
| developer_guide | 按需 | **19 篇** | ✅（`architecture/` 下 3 篇「规划中」见 §2.1） |
| skills | 按需 | **3 篇活 + 11 篇归档** | ✅ |
| simulation | 按需 | **8 篇** | ✅ |
| adr（决策正本） | 按需 | **76 篇** | ✅ |
| architecture（主题文档） | 按需 | **32 篇** | ✅ |
| plans | 按需 | **35 篇** | ✅ |
| archive（逐字保留区） | 按需 | **40 篇** | ✅ |

### 6.2 质量指标

| 指标 | 目标值 | 测量方式 |
|------|-------|---------|
| 文档覆盖率 | ≥90% API 有文档 | API 扫描 |
| 示例可运行率 | 100% | CI 测试 |
| 平均更新周期 | ≤90 天 | Git 历史 |
| 读者满意度 | ≥4.0/5.0 | 调查反馈 |

---

## 🗺️ 7. T401 文档迁移计划

### 7.1 现有文档映射

| 现有文档 | 迁移目标 | 内容调整 |
|---------|---------|---------|
| `docs/tech/T401-TECHNICAL-DOC.md` | `developer_guide/api-reference/T001-simulator-api.md` | 强化实现细节，添加架构图 |
| `docs/tech/T401-TECHNICAL-DOC.md` 部分 | `usage_guide/06-bundle-patterns.md` | 提取 Bundle 使用示例 |
| `docs/skills/SIMULATOR-API-GUIDE.md` | `skills/simulator-api-quickref.md` | 简化为速查表 |
| 无 | `usage_guide/04-simulator-api.md` | 新建完整使用指南 |
| `docs/plans/T401-COMPLETION-REPORT.md` | `plans/T001-completion-report.md` | 重命名，格式调整 |

### 7.2 新增文档列表

| 文档 | 位置 | 优先级 | 工时估算 |
|------|------|--------|---------|
| docs/README.md | 文档中心 | 🔴 高 | 30min |
| usage_guide/README.md | 使用指南索引 | 🔴 高 | 20min |
| developer_guide/README.md | 开发指南索引 | 🔴 高 | 20min |
| usage_guide/04-simulator-api.md | 完整使用指南 | 🔴 高 | 1h |
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

**分级来源两级**：① **目录前缀默认**（L1 = `usage_guide/` `developer_guide/` `skills/` `simulation/`；L2/L3 = `adr/` `archive/` `plans/` `reports/` `freertos/` `.rddf/` `openspec/changes/archive/`）② **混合目录逐文件登记**（`architecture/` · `docs/` 根 · 仓根散落 `*.md`）⇒ ⇒ 登记表 MUST 由护栏 `--render` **派生**、禁手改（`.rddf/checks/docs-link-scope.md`）。

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

## 📋 10. 变更记录（v2.1 · 2026-11-19）

| 章节 | 变更 | 依据 |
|---|---|---|
| §2.1 目录树 | 从**规划态**校准为**当前态**（原列的 21 个文件不存在 ⇒ 删；计划中者标「规划中」） | 导航照它建 ⇒ 实测 27 处死链 |
| §5.1「CI 检查」 | 从**空头支票**改为指向实际护栏 `scripts/verify_docs_links.sh`（C1–C5） | 仓内此前无任何文档类护栏 |
| §6.1 统计表 | 按实测回填（usage_guide 7 · developer_guide 19 · adr 76 · architecture 32 …） | 原表停在「0 篇」 |
| §8.1 批准记录 | **如实标注矛盾**（不改写签署状态） | 签署状态属事实史料 |
| **§9（新增）** | 文档治理口径：分级 / 引用有效性 / 死链处置 / 本机路径禁令 / 自动化门 | `architecture/2026-11-19-document-governance.md`（oracle APPROVE） |
| 全文 | 删本机绝对路径（§2.1 的 `«本机»/`） | P1 本机路径禁令 |