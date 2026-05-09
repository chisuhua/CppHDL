# ADR-029: 命名约定一致性

**状态**: ✅ 已采纳
**日期**: 2026-05-08
**决策人**: Sisyphus + 用户

---

## 1. 背景

议题 #23 审查 AGENTS.md 命名约定（CamelCase 类名）与代码库实际风格之间的不一致。

## 2. 现有命名模式

代码库中存在**三个系统性的命名模式**，而非偶然的不一致：

### 模式 A：CamelCase — 公开 API 组件

| 示例 | 位置 | 说明 |
|------|------|------|
| `Component`, `Simulator` | 公开 API | 顶层框架类 |
| `ForwardingUnit`, `HazardUnit` | cpu/ | CPU 模块 |
| `AxiLiteGpio`, `AxiDma` | axi4/ | AXI 外设 |
| `Counter`, `Pwm`, `UartTx` | examples/ | 用户模块 |

### 模式 B：snake_case — AST 内部实现

| 示例 | 位置 | 说明 |
|------|------|------|
| `lnodeimpl`, `regimpl`, `opimpl` | `ast/`, `core/lnodeimpl.h` | 逻辑节点实现 |
| `instr_base`, `instr_reg`, `instr_op` | `ast/` | 指令类 |
| `clockimpl`, `resetimpl`, `memimpl` | `ast/` | 时钟/复位/内存实现 |
| `inputimpl`, `outputimpl`, `proxyimpl` | `ast/` | IO 端口实现 |

这些类名全部以 `snake_case` + 类型后缀（`impl`、`instr_*`）结尾，形成一致性内部 DSL。

### 模式 C：snake_case — 工具/基础设施

| 示例 | 位置 | 说明 |
|------|------|------|
| `destruction_manager` | `utils/` | 生命周期管理 |
| `source_location`, `source_info` | `utils/` | 源码追踪 |
| `ch_exception`, `context_exception` | `utils/` | 异常 |
| `node_builder` | `core/` | 节点工厂 |
| `verilogwriter`, `dagwriter` | 顶层 | 代码生成器 |
| `ch_device`, `ch_module` | 顶层 | 设备/模块包装 |

这些类大多继承关系简单或为单一职责工具类。

### 模式 D：蛇形命名 + `_t` — 类型别名

| 示例 | 说明 |
|------|------|
| `data_map_t` | 数据映射类型 |
| `sdata_type` | 信号数据类型 |

---

## 3. 决议

**接受现状，一致性文档化**。理由：

1. **批量重命名风险高** — 85+ 类散布在 31+ 文件，重命名影响大量 git blame 记录
2. **模式有内在一致性** — `*impl` 家族和 `instr_*` 家族是系统性 snake_case，非偶然
3. **用户代码不受影响** — 公开 API（Component、Simulator、AxiLite* 等）已使用 CamelCase

### AGENTS.md 更新

在 AGENTS.md 命名规范中增加以下例外：

```markdown
## 命名规范（例外）
- AST 内部实现类（*impl、instr_*、clockimpl 等）使用 snake_case
- 工具/基础设施类使用 snake_case
- 类型别名使用 snake_case + _t 后缀
- 公开 API 组件类必须使用 CamelCase（新增代码）
```

### 适用范围

| 层面 | 风格 | 强制程度 |
|------|------|---------|
| **公开 API**（Component/Simulator/模块） | `CamelCase` | 严格（新增代码必须遵守） |
| **AST 内部**（`*impl`, `instr_*`） | `snake_case` | 允许（系统性质） |
| **工具/基础设施** | `snake_case` | 允许（历史原因） |
| **类型别名**（`*_t`） | `snake_case + _t` | 允许 |

---

## 4. 变更日志

| 日期 | 版本 | 变更 | 作者 |
|------|------|------|------|
| 2026-05-08 | v1.0 | 初始版本：记录命名约定分析，接受现状并文档化例外规则 | Sisyphus |

---

**相关链接**:
- `include/AGENTS.md` — 命名规范（需更新）
- `include/ast/ast_nodes.h` — `*impl` 家族（snake_case 系统性质）
- `include/ast/instr_base.h` — `instr_*` 家族（snake_case 系统性质）
- `include/codegen_verilog.h` — `verilogwriter`（工具类）
- `include/core/lnodeimpl.h` — `lnodeimpl`（AST 内部）
- `docs/adr/ADR-DISCUSSION-PLAN.md` — 议题 #23
