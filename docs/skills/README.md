# 快速参考技能 (Skills)

> **目标读者**: 需要快速查找用法的所有用户  
> **版本**: v2.0 | **最后更新**: 2026-04-09  
> **特点**: 简洁、便于速查、覆盖常用 API 和修复模式

---

## ⚡ 可用技能

### API 参考技能

| 技能 | 文件 | 说明 | 适用场景 |
|------|------|------|---------|
| Simulator API 速查 | [simulator-api-quickref.md](simulator-api-quickref.md) | Simulator API 快速参考 | 查找 API 用法 |

---

### 修复模式技能 (已归档)

以下技能已归档到 `archive/` 目录，记录历史修复模式：

| 技能 | 归档位置 | 说明 |
|------|---------|------|
| Bundle IO 模式 | [archive/cpphdl-bundle-io-pattern/](archive/cpphdl-bundle-io-pattern/SKILL.md) | Bundle IO 正确使用 |
| 断言静态析构 | [archive/cpphdl-assert-static-destruction/](archive/cpphdl-assert-static-destruction/SKILL.md) | 静态析构期间日志 |
| FIFO 时序修复 | [archive/cpphdl-fifo-timing-fix/](archive/cpphdl-fifo-timing-fix/SKILL.md) | FIFO 时序逻辑 |
| 位移操作 UB | [archive/cpphdl-shift-fix/](archive/cpphdl-shift-fix/SKILL.md) | 位移未定义行为 |
| Chop 类型安全 | [archive/cpphdl-chop-type-safety/](archive/cpphdl-chop-type-safety/SKILL.md) | 类型系统安全 |
| 内存初始化 | [archive/cpphdl-mem-init-dataflow/](archive/cpphdl-mem-init-dataflow/SKILL.md) | 内存初始化数据流 |
| 类型系统模式 | [archive/cpphdl-type-system-patterns/](archive/cpphdl-type-system-patterns/SKILL.md) | 类型系统设计 |
| RV32I 分支比较 | [archive/rv32i-branch-compare-fix/](archive/rv32i-branch-compare-fix/SKILL.md) | 分支比较有符号 |
| RV32I 分支模式 | [archive/rv32i-branch-pattern/](archive/rv32i-branch-pattern/SKILL.md) | 分支预测模式 |
| RV32I 解码器 | [archive/rv32i-decoder-pattern/](archive/rv32i-decoder-pattern/SKILL.md) | 解码器设计 |
| ACP RV32I 工作流 | [archive/acp-rv32i-workflow/](archive/acp-rv32i-workflow/SKILL.md) | RV32I 开发工作流 |

---

## 📖 使用建议

### 快速查找

- **API 用法**: [simulator-api-quickref.md](simulator-api-quickref.md)
- **完整指南**: [使用指南](../usage_guide/README.md)
- **技术细节**: [开发者指南](../developer_guide/README.md)

### 历史修复模式

归档的技能包含历史修复模式和经验教训：
- 访问 `archive/` 目录查看所有归档技能
- 每个技能包含问题根因、修复方案和验证流程
- 适用于遇到类似问题时参考

---

## 📊 技能统计

| 类别 | 数量 | 状态 |
|------|------|------|
| API 参考 | 1 | ✅ 活跃 |
| 修复模式 | 11 | 📦 已归档 |
| **总计** | **12** | - |

---

## 🔗 相关资源

- [usage_guide/04-simulator-api.md](../usage_guide/04-simulator-api.md) - Simulator API 完整使用指南
- [developer_guide/api-reference/T001-simulator-api.md](../developer_guide/api-reference/T001-simulator-api.md) - API 技术规格
- [archive/](archive/) - 所有归档技能

---

**维护**: AI Agent  
**版本**: v2.0  
**下次审查**: 2026-05-09
