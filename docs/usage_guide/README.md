# 用户使用指南 (Usage Guide)

> **目标读者**: 使用 CppHDL 设计硬件的工程师  
> **版本**: v2.0 | **最后更新**: 2026-04-09

---

## 📖 使用指南导航

### 入门系列

| 编号 | 文档 | 说明 | 状态 |
|------|------|------|------|
| 01 | 01-introduction.md | CppHDL 简介 | 🟡 规划中 |
| 02 | 02-quickstart.md | 5 分钟快速入门 | 🟡 规划中 |
| 03 | 03-core-concepts.md | 核心概念 | 🟡 规划中 |

### 核心 API

| 编号 | 文档 | 说明 | 状态 |
|------|------|------|------|
| 04 | [04-simulator-api.md](04-simulator-api.md) | Simulator API 使用 | ✅ 已完成 |
| 05 | 05-component-design.md | Component 设计指南 | 🟡 规划中 |
| 06 | [06-bundle-patterns.md](06-bundle-patterns.md) | Bundle 使用模式 | ✅ 已完成 |

### 测试与验证

| 编号 | 文档 | 说明 | 状态 |
|------|------|------|------|
| 07 | 07-stream-patterns.md | Stream 使用模式 | 🟡 规划中 |
| 08 | 08-state-machine.md | 状态机设计 | 🟡 规划中 |
| 09 | [09-testbench-guide.md](09-testbench-guide.md) | Testbench 编写指南 | ✅ 已完成 |

### 高级主题

| 编号 | 文档 | 说明 | 状态 |
|------|------|------|------|
| 10 | 10-verilog-synthesis.md | Verilog 生成与综合 | 🟡 规划中 |
| 11 | [11-stream-flow-comparison.md](11-stream-flow-comparison.md) | Stream/Flow 对比 | ✅ 已完成 |

---

## 🎯 推荐学习路径

### 初学者路径

```
01-introduction → 02-quickstart → 04-simulator-api → 06-bundle-patterns → 09-testbench-guide
```

### Component 设计路径

```
04-simulator-api → 05-component-design → 06-bundle-patterns → 09-testbench-guide
```

### 完整项目路径

```
02-quickstart → 04-simulator-api → 05-component-design → 09-testbench-guide → 10-verilog-synthesis
```

---

## 📊 完成状态

| 类别 | 总数 | 已完成 | 进行中 | 规划中 | 完成率 |
|------|------|--------|--------|--------|--------|
| 入门系列 | 3 | 0 | 0 | 3 | 0% |
| 核心 API | 3 | 2 | 0 | 1 | 67% |
| 测试与验证 | 3 | 1 | 0 | 2 | 33% |
| 高级主题 | 2 | 1 | 0 | 1 | 50% |
| **总计** | **11** | **4** | **0** | **7** | **36%** |

---

## 🔗 相关资源

- [开发者指南](../developer_guide/README.md) - 了解内部实现
- [快速参考技能](../skills/README.md) - 速查手册
- [示例代码](../../samples/) - 动手实践

---

**维护**: AI Agent  
**版本**: v2.0
