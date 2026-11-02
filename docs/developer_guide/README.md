# 开发者指南 (Developer Guide)

> **目标读者**:维护和扩展 CppHDL 框架的开发者

---

## 🔧 导航

### 架构文档

| 文档 | 说明 | 状态 |
|------|------|------|
| [overview.md](architecture/overview.md) | 系统架构概览 | 🟡 规划中 |
| [simulator-internals.md](architecture/simulator-internals.md) | Simulator 内部机制 | 🟡 规划中 |
| [node-lifecycle.md](architecture/node-lifecycle.md) | 节点生命周期管理 | 🟡 规划中 |

### API 参考

| 文档 | 说明 | 状态 |
|------|------|------|
| [T001-simulator-api.md](api-reference/T001-simulator-api.md) | T001: Simulator API 规格 | ✅ 已完成 |

### 技术报告

| 文档 | 说明 | 状态 |
|------|------|------|
| [T001-analysis.md](tech-reports/T001-analysis.md) | T001 技术分析报告 | 🟡 规划中 |

### 开发模式

| 文档 | 说明 | 状态 |
|------|------|------|
| [COMPONENT-LIFECYCLE-GUIDE.md](patterns/COMPONENT-LIFECYCLE-GUIDE.md) | `ch_device` vs `ch_module` 的生命周期与常见错误 | ✅ 已完成 |
| [HIERARCHICAL-RTL-AUTHORING-GUIDE.md](patterns/HIERARCHICAL-RTL-AUTHORING-GUIDE.md) | 多层模块 / 层级粒度（成层声明）/ 层次化 RTL 的写法纪律与方案审查 | ✅ 已完成 |
| [JIT-DEBUGGING-GUIDE.md](patterns/JIT-DEBUGGING-GUIDE.md) | JIT 编译器的调试方法 | ✅ 已完成 |

### 集成

| 文档 | 说明 | 状态 |
|------|------|------|
| [verilator-integration.md](verilator-integration.md) | Verilator 子模块集成与构建 | ✅ 已完成 |

---

## 📊 完成状态

| 类别 | 总数 | 已完成 | 进行中 | 规划中 | 完成率 |
|------|------|--------|--------|--------|--------|
| 架构文档 | 4 | 0 | 1 | 3 | 0% |
| API 参考 | 按需 | 1 | 0 | 0 | 100% |
| 技术报告 | 按需 | 0 | 1 | 0 | 0% |
| 开发模式 | 3 | 3 | 0 | 0 | 100% |
| 集成 | 按需 | 1 | 0 | 0 | 100% |
| **总计** | **按需** | **5** | **2** | **3** | **~50%** |

---

## 🔗 相关资源

- [用户使用指南](../usage_guide/README.md) - API 使用方法
- [快速参考技能](../skills/README.md) - 速查手册
- [贡献指南](contributing.md) - 如何贡献代码

---

**维护**: AI Agent  
**版本**: v2.0  
**最后更新**: 2026-04-09
