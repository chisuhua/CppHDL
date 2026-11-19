# 快速参考技能 (Skills)

> **目标读者**: 需要速查用法 / 修复模式的所有用户
> **最后更新**: 2026-11-19（仓根 `skills/` 已并入本目录，**这是唯一的技能库**）

---

## ⚡ 活跃技能（5 个 + 1 速查表）

| 技能 | 用途 | 何时看 |
|---|---|---|
| [simulator-api-quickref.md](simulator-api-quickref.md) | Simulator API 一页速查（位宽参考、完整示例） | 写测试时随手查 |
| [cpphdl-simulator-port-value/](cpphdl-simulator-port-value/SKILL.md) | `set_port_value` / `get_port_value` 的正确用法与陷阱 | 端口读写结果不对时 |
| [cpphdl-input-port-assignment/](cpphdl-input-port-assignment/SKILL.md) | 输入端口必须用 `set_input_value`（`io().x = v` 静默无效） | 测试里输入没生效时 |
| [cpphdl-component-lifecycle/](cpphdl-component-lifecycle/SKILL.md) | `ch_device` vs `ch_module` 的选择（用错会 SIGSEGV） | 组件创建报「child destroyed unexpectedly」时 |
| [cpphdl-bundle-patterns/](cpphdl-bundle-patterns/SKILL.md) | Bundle 连接模式（直连 / 字段级 / master-slave） | 接口连不上 / 字段读不到时 |
| [cpphdl-shift-fix/](cpphdl-shift-fix/SKILL.md) | 位移操作的 UB 与位宽陷阱（含修复手法） | 位移结果异常时 |

> ⚠️ `archive/cpphdl-shift-fix/` 是**旧版本**（201 行）；活版本是本目录的 254 行版本。

---

## 📦 归档技能（11 个 · 历史修复模式）

以下技能已退役，但**保留了根因分析 + 修复方案 + 验证流程** —— 遇到类似问题时仍可参考。

| 技能 | 历史主题 |
|---|---|
| [archive/cpphdl-bundle-io-pattern/](archive/cpphdl-bundle-io-pattern/SKILL.md) | Bundle IO 模式（早期版本） |
| [archive/cpphdl-assert-static-destruction/](archive/cpphdl-assert-static-destruction/SKILL.md) | 断言宏在静态析构期的日志崩溃 |
| [archive/cpphdl-fifo-timing-fix/](archive/cpphdl-fifo-timing-fix/SKILL.md) | FIFO 时序逻辑缺陷 |
| [archive/cpphdl-chop-type-safety/](archive/cpphdl-chop-type-safety/SKILL.md) | `ch_op` 操作符类型安全 |
| [archive/cpphdl-mem-init-dataflow/](archive/cpphdl-mem-init-dataflow/SKILL.md) | `ch_mem` 初始化数据流 |
| [archive/cpphdl-type-system-patterns/](archive/cpphdl-type-system-patterns/SKILL.md) | 类型系统设计模式 |
| [archive/rv32i-branch-compare-fix/](archive/rv32i-branch-compare-fix/SKILL.md) | 分支比较的有符号性 |
| [archive/rv32i-branch-pattern/](archive/rv32i-branch-pattern/SKILL.md) | RV32I 分支预测模式 |
| [archive/rv32i-decoder-pattern/](archive/rv32i-decoder-pattern/SKILL.md) | RV32I 指令译码器 |
| [archive/acp-rv32i-workflow/](archive/acp-rv32i-workflow/SKILL.md) | RV32I 多轮交互开发流程 |

---

## 📖 相关

- 完整用法 → [用户指南](../usage_guide/README.md)
- 内部机制 → [开发者指南](../developer_guide/README.md)
- 设计与推导 → [架构主题](../architecture/README.md)

---

**维护**: AI Agent
