# CppHDL 文档架构规范 v2.0

**文档编号**: CPPHDL-DOC-ARCH-001  
**创建日期**: 2026-04-09  
**状态**: ✅ 已批准  
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

### 2.1 完整目录树

```
/workspace/project/CppHDL/
│
├── README.md                          # 项目首页 (对外)
├── AGENTS.md                          # AI 代理指南 (对内)
│
├── docs/                              # 文档根目录
│   ├── README.md                      # 📑 文档导航中心
│   │
│   ├── usage_guide/                   # 📘 用户使用指南
│   │   ├── README.md                  # 使用指南导航
│   │   ├── 01-introduction.md         # CppHDL 简介
│   │   ├── 02-quickstart.md           # 5 分钟快速入门
│   │   ├── 03-core-concepts.md        # 核心概念
│   │   │                              #
│   │   ├── 04-simulator-api.md        # Simulator API 使用
│   │   ├── 05-component-design.md     # Component 设计指南
│   │   ├── 06-bundle-patterns.md      # Bundle 使用模式
│   │   ├── 07-stream-patterns.md      # Stream 使用模式
│   │   ├── 08-state-machine.md        # 状态机设计
│   │   ├── 09-testbench-guide.md      # Testbench 编写
│   │   ├── 10-verilog-synthesis.md    # Verilog 生成与综合
│   │   └── 11-examples-gallery.md     # 示例代码集
│   │
│   ├── developer_guide/               # 🔧 开发者指南
│   │   ├── README.md                  # 开发指南导航
│   │   ├── contributing.md            # 贡献指南
│   │   │                              #
│   │   ├── architecture/              # 架构文档
│   │   │   ├── overview.md            # 系统架构概览
│   │   │   ├── core-design.md         # 核心层设计
│   │   │   ├── simulator-internals.md # Simulator 内部机制
│   │   │   ├── node-lifecycle.md      # 节点生命周期
│   │   │   └── context-management.md  # Context 管理
│   │   │                              #
│   │   ├── api-reference/             # API 参考
│   │   │   ├── T001-simulator-api.md  # T001: Simulator API 规格
│   │   │   ├── T002-verilog-codegen.md# T002: Verilog 代码生成
│   │   │   └── ...                    # 未来 T0xx 系列
│   │   │                              #
│   │   ├── tech-reports/              # 技术报告
│   │   │   ├── T001-analysis.md       # T001 技术分析报告
│   │   │   └── ...                    # 未来报告
│   │   │                              #
│   │   └── patterns/                  # 开发模式
│   │       ├── module-patterns.md     # 模块设计模式
│   │       ├── test-patterns.md       # 测试编写模式
│   │       └── debug-patterns.md      # 调试模式
│   │
│   ├── skills/                        # ⚡ 快速参考技能
│   │   ├── README.md                  # 技能列表
│   │   ├── simulator-api-quickref.md  # Simulator API 速查
│   │   ├── component-template.md      # Component 模板
│   │   └── testbench-template.md      # Testbench 模板
│   │
│   ├── plans/                         # 📅 计划与报告
│   │   ├── README.md                  # 计划文档索引
│   │   ├── PHASE4-PLAN-2026-04-09.md  # Phase 4 总计划
│   │   ├── T001-completion-report.md  # T001 完成报告
│   │   └── ...                        # 其他计划
│   │
│   └── archive/                       # 🗄️ 归档文档
│       └── ...                        # 历史文档
```

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
 │      │      └─ CI 检查
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
|------|---------|---------|--------|
| usage_guide | 11 篇 | 0 篇 | 0% |
| developer_guide | 15 篇 | 0 篇 | 0% |
| skills | 5 篇 | 1 篇 | 20% |
| plans | 按需 | 2 篇 | - |

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
