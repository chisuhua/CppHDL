# CppHDL 文档中心

**最后更新**: 2026-04-08  
**文档状态**: 🟢 最新 | 🟡 部分更新 | 🔴 待更新

---

## 1. 快速导航

### 新入门？从这里开始

1. [项目概览](PROJECT-OVERVIEW.md) - 了解 CppHDL 是什么
2. [快速参考](../QUICK_REFERENCE.md) - 常用 API 速查
3. [CppHDL_UsageGuide.md](CppHDL_UsageGuide.md) - 详细使用指南

### 修复问题？

- [Bundle 连接问题](problem-reports/bundle-connection-issue.md) - 问题分析与修复方案
- [实施计划](plans/bundle-connection-fix-plan.md) - 详细修复步骤

---

## 2. 文档结构

```
docs/
├── README.md                        # 本文件
├── PROJECT-OVERVIEW.md              # 项目概览
├── Bundle_UsageGuide.md             # Bundle 使用指南
├── Bundle_DeveloperGuide.md         # Bundle 开发者指南
├── CHLib_UsageGuide.md              # Chlib 库指南
├── CppHDL_UsageGuide.md             # 综合使用指南
├── CppHDL_Testing_Guide.md          # 测试指南
├── problem-reports/                 # 问题报告
│   └── bundle-connection-issue.md   # Bundle 连接问题
├── plans/                           # 实施计划
│   └── bundle-connection-fix-plan.md # Bundle 连接修复计划
├── architecture/                    # 架构文档
│   ├── decisions/                   # 架构决策
│   └── plans/                       # 架构计划
└── archive/                         # 归档文档 (历史报告)
```

---

## 3. 文档分类

### 使用指南 (面向用户)

| 文档 | 内容 | 目标读者 |
|------|------|----------|
| [CppHDL_UsageGuide.md](CppHDL_UsageGuide.md) | CppHDL 综合指南 | 所有用户 |
| [Bundle_UsageGuide.md](Bundle_UsageGuide.md) | Bundle 定义与连接 | 用户 |
| [CHLib_UsageGuide.md](CHLib_UsageGuide.md) | Chlib 组件库使用 | 用户 |

### 开发者指南 (面向维护者)

| 文档 | 内容 | 目标读者 |
|------|------|----------|
| [Bundle_DeveloperGuide.md](Bundle_DeveloperGuide.md) | Bundle 元编程与扩展 | 框架维护者 |
| [architecture/](architecture/) | 架构设计与决策 | 架构师 |

### 测试与质量

| 文档 | 内容 | 目标读者 |
|------|------|----------|
| [CppHDL_Testing_Guide.md](CppHDL_Testing_Guide.md) | 测试框架与最佳实践 | QA |

---

## 4. 问题报告与计划

### 当前问题

| 问题 | 状态 | 优先级 | 负责人 |
|------|------|--------|--------|
| [Bundle 连接失败](problem-reports/bundle-connection-issue.md) | 🔴 待修复 | P0 | - |
| RV32I Pipeline 测试失败 | 🔴 待分析 | P1 | - |

### 实施计划

| 计划 | 状态 | 预计完成 |
|------|------|----------|
| [Bundle 连接修复](plans/bundle-connection-fix-plan.md) | 🔴 未开始 | 2026-04-09 |

---

## 5. 文档维护

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

## 6. 相关资源

### 代码库

| 目录 | 内容 |
|------|------|
| `include/` | 核心头文件 |
| `src/` | 实现文件 |
| `tests/` | 单元测试 |
| `samples/` | 快速示例 |
| `examples/` | 完整项目 |

### 外部资源

- [SpinalHDL 官网](https://spinalhdl.github.io/SpinalDoc-RTD/master/index.html)
- [AGENTS.md](../AGENTS.md) - 开发代理指南

---

## 7. 文档索引

### 根目录文档

| 文档 | 说明 |
|------|------|
| [../README.md](../README.md) | 项目简介 |
| [../PROJECT-OVERVIEW.md](../PROJECT-OVERVIEW.md) | 项目概览 |
| [../QUICK_REFERENCE.md](../QUICK_REFERENCE.md) | 快速参考 |
| [../Compare.md](../Compare.md) | CppHDL vs SpinalHDL |
| [../AGENTS.md](../AGENTS.md) | 开发代理指南 |

---

**维护**: AI Agent  
**联系**: 提交 Issue 获取帮助

