# CppHDL 文档中心 v2.0

> **CppHDL 文档中心** - 基于 C++20 的层次化综合 (HLS) 库  
> **文档版本**: v2.1 | **最后更新**: 2026-04-25

---

## 📚 快速导航

### 新入门？从这里开始

1. [项目概览](PROJECT-OVERVIEW.md) - 了解 CppHDL 是什么
2. [文档架构规范](DOCUMENT-ARCHITECTURE.md) - 了解文档体系
3. [使用指南](usage_guide/README.md) - 学习如何使用 CppHDL

### 开发者？

- [开发者指南](developer_guide/README.md) - 框架内部机制
- [API 参考](developer_guide/api-reference/) - 技术规格文档
- [贡献指南](developer_guide/contributing.md) - 如何贡献代码

### 查找特定内容？

- [Simulator API](usage_guide/04-simulator-api.md) - T001 任务成果
- [Bundle 模式](usage_guide/06-bundle-patterns.md) - Bundle 设计模式
- [测试指南](CppHDL_Testing_Guide.md) - 测试编写指南

---

## 📁 文档分类

### 📘 用户使用指南 (Usage Guide)

**目标读者**: 使用 CppHDL 设计硬件的工程师

| 文档 | 说明 | 状态 |
|------|------|------|
| [进入使用指南 →](usage_guide/README.md) | 完整使用指南索引 | 📖 导航 |

### 🔧 开发者指南 (Developer Guide)

**目标读者**: 维护和扩展 CppHDL 框架的开发者

| 文档 | 说明 | 状态 |
|------|------|------|
| [进入开发者指南 →](developer_guide/README.md) | 完整开发指南索引 | 🔧 导航 |

### ⚡ 快速参考技能 (Skills)

**目标读者**: 需要快速查找用法的所有用户

| 文档 | 说明 | 状态 |
|------|------|------|
| [进入技能库 →](skills/README.md) | 快速参考技能索引 | ⚡ 导航 |

### 📅 计划与报告 (Plans)

**目标读者**: 项目参与者、管理者

| 文档 | 说明 | 状态 |
|------|------|------|
| [Phase 4 计划](plans/PHASE4-PLAN-2026-04-09.md) | Phase 4 工作计划 | ✅ 完成 |
| [T001 完成报告](plans/T001-completion-report.md) | T001 任务完成报告 | ✅ 完成 |

---

## 🗺️ 文档目录结构

```
docs/
├── README.md                        # 文档中心 (当前位置)
├── DOCUMENT-ARCHITECTURE.md        # 文档架构规范 v2.0
│
├── usage_guide/                    # 📘 用户使用指南
│   ├── README.md                   # 使用指南导航
│   └── ...                         # 使用文档
│
├── developer_guide/                # 🔧 开发者指南
│   ├── README.md                   # 开发指南导航
│   ├── architecture/               # 架构文档
│   ├── api-reference/              # API 参考
│   ├── tech-reports/               # 技术报告
│   └── patterns/                   # 开发模式
│
├── skills/                         # ⚡ 快速参考技能
│   └── README.md                   # 技能索引
│
├── plans/                          # 📅 计划与报告
│   └── README.md                   # 计划索引
│
├── simulation/                     # 🔬 仿真架构与性能
│   ├── ARCHITECTURE.md            # 仿真架构深入分析
│   ├── ROADMAP.md                 # 实施路线图与优化方案
│   └── PERFORMANCE_TESTS.md       # 性能测试计划 (TC-01~TC-06)
│
├── archive/                        # 🗄️ 归档文档
│
└── learning/                       # 学习资料
└── problem-reports/                # 问题报告
```

---

## 🔍 快速查找

### 按任务查找

| 任务 | 相关文档 |
|------|---------|
| **T001: Simulator API** | [使用指南](usage_guide/04-simulator-api.md), [API 规格](developer_guide/api-reference/T001-simulator-api.md), [技术报告](developer_guide/tech-reports/T001-analysis.md) |

### 按主题查找

- **Bundle**: [使用模式](usage_guide/06-bundle-patterns.md), [设计指南](Bundle_DeveloperGuide.md)
- **Stream**: [使用指南](CppHDL_UsageGuide.md), [对比分析](CppHDL_vs_SpinalHDL_Stream_Flow_Usage.md)
- **测试**: [测试指南](CppHDL_Testing_Guide.md)
- **仿真性能**: [架构分析](simulation/ARCHITECTURE.md), [实施路线图](simulation/ROADMAP.md), [性能测试](simulation/PERFORMANCE_TESTS.md)

---

## 📊 文档状态统计

| 类别 | 总数 | 已完成 | 进行中 | 规划中 | 完成率 |
|------|------|--------|--------|--------|--------|
| usage_guide | 11 | 4 | 1 | 6 | 36% |
| developer_guide | 15 | 6 | 1 | 8 | 40% |
| skills | 5 | 1 | 1 | 3 | 20% |
| plans | 按需 | 2 | 0 | 0 | 100% |
| simulation | 2 | 2 | 0 | 0 | 100% |

---

## 📝 文档维护

### 更新指南

1. **新增文档**: 根据类型选择正确目录
2. **更新文档**: 在文件头更新"最后更新"日期
3. **归档文档**: 过时文档移至 `archive/`

### 文档审查

| 文档类型 | 审查频率 | 负责人 |
|----------|----------|--------|
| 使用指南 | 每季度 | 技术作者 |
| 问题报告 | 问题解决后 | 问题解决人 |
| 架构决策 | 架构变更时 | 架构师 |

---

## 🔗 相关资源

### 根目录文档

| 文档 | 说明 |
|------|------|
| [../README.md](../README.md) | 项目简介 |
| [../QUICK_REFERENCE.md](../QUICK_REFERENCE.md) | 快速参考 |
| [../AGENTS.md](../AGENTS.md) | AI 代理指南 |

### 外部资源

- [SpinalHDL 官网](https://spinalhdl.github.io/SpinalDoc-RTD/master/index.html)
- [CppHDL GitHub](https://github.com/your-repo/CppHDL)

---

**维护**: AI Agent  
**文档版本**: v2.0  
**下次审查**: 2026-07-09  
**联系**: 提交 Issue 获取帮助

