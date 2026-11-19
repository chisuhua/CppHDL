# 用户使用指南 (Usage Guide)

> **目标读者**: 用 CppHDL 设计硬件的工程师
> **最后更新**: 2026-11-19（编号已重排为 01–06 连续）

---

## 📖 六篇正文

| # | 文档 | 讲什么 | 什么时候读 |
|---|---|---|---|
| **01** | [01-simulator-api.md](01-simulator-api.md) | `Simulator` 的全部 API：`set_input_value` / `get_value` / `get_port_value` / `tick` / `run_*`；读**输出**端口的正路；3 类常见故障排除 | **第一个读** —— 没有它跑不起任何测试 |
| **02** | [02-component-design.md](02-component-design.md) | 组件基本形态 · 多层装配成树 · 跨层连线 · 用成员变量当接口 · **RTL 三种输出形态**（扁平 / 根组件 / 层次化）· 反模式清单 · 仓内范例索引 | 开始写多模块设计时 |
| **03** | [03-bundle-patterns.md](03-bundle-patterns.md) | Bundle 定义 / 字段注册 / `as_master()` / `as_slave()` / 连接方式（直连、字段级、`connect`） | 设计总线接口时 |
| **04** | [04-testbench-guide.md](04-testbench-guide.md) | Testbench 五种模式（含 `.verif` 验证源驱动）、断言、双后端跑法 | 写测试时 |
| **05** | [05-verilator-backend.md](05-verilator-backend.md) | Verilator 后端的用法与边界（64 位端口上限、`sread` vs `aread`、reset 脉冲） | 需要 RTL 级仿真时 |
| **06** | [06-spinalhdl-comparison.md](06-spinalhdl-comparison.md) | 与 SpinalHDL 的 Stream / Flow 用法逐条对照 | 从 SpinalHDL 迁移过来时 |

---

## 🎯 推荐路径

| 背景 | 路径 |
|---|---|
| **完全新手** | 01 → 02 → 04 → 03 |
| **写一个多模块设计** | 01 → 02 → 03 → 04 |
| **从 SpinalHDL 迁过来** | 06 → 01 → 02 |
| **要出 RTL** | 01 → 02（§5 RTL 输出形态）→ 05 |

---

## 🔗 相关

- [开发者指南](../developer_guide/README.md) —— 框架内部机制（Verilog codegen / JIT / 层次化分析）
- [技能库](../skills/README.md) —— 速查表与修复模式
- [架构主题](../architecture/README.md) —— 设计为什么这么做的推导
- [示例代码](../../examples/) · [samples/](../../samples/)

---

**维护**: AI Agent
