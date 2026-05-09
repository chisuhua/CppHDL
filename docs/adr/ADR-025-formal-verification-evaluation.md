# ADR-025: 形式验证接口评估

**状态**: ✅ 已采纳（低优先级搁置）
**日期**: 2026-05-08
**决策人**: Sisyphus + 用户

---

## 1. 背景

议题 #19 评估是否在 CppHDL 中实现形式验证接口：`assert`/`assume`/`cover` DSL 原语和 Verilog SVA（SystemVerilog Assertions）代码生成。

### 1.1 当前状态

| 功能 | 位置 | 状态 | 说明 |
|------|------|------|------|
| `AssertChecker` 仿真断言 | `chlib/assert.h:20-41` | ✅ | Component 子类，仿真时检查信号条件 |
| `CH_ASSERT` 宏 | `chlib/assert.h:47-51` | ⚠️ 空壳 | 仅实现 `do { cout << msg; } while(0)`，无实际条件校验 |
| `assume()` / `cover()` DSL | — | ❌ | 不存在 |
| SVA codegen | `codegen_verilog.cpp` | ❌ | 仅生成寄存器 `always`，无 `assert property` |

### 1.2 SpinalHDL 对照

| SpinalHDL | 语义 | CppHDL |
|-----------|------|--------|
| `assert(cond, msg)` | 始终校验 | ⚠️ 仅打印 |
| `assume(cond, msg)` | formal 约束输入 | ❌ |
| `cover(cond)` | 验证可达性 | ❌ |
| SVA 代码生成 | `assert property (...)` | ❌ |

---

## 2. 分析

### 2.1 形式验证的前提条件

形式验证（Formal Verification）需要以下基础设施：

1. **Formal 工具链** — SymbiYosys (SBY)、JasperGold、VC Formal 等
2. **SVA 代码生成** — Verilog codegen 输出 `assert property`, `assume property`, `cover property`
3. **绑定文件** — 将断言绑定到设计模块
4. **约束环境** — 时钟定义、复位序列、输入约束

当前 CppHDL 项目：
- 无 formal 工具链集成
- Verilog codegen 尚在补齐基本操作（ADR-019 阶段二）
- 无 formal 验证脚本或 Makefile 目标

### 2.2 用户需求评估

17 个移植示例和全部 AXI/CPU 组件使用 **Catch2 测试** + **仿真自测试** 进行验证，无形式验证需求。

**结论：当前零需求。**

### 2.3 `CH_ASSERT` 宏的空壳问题

当前宏定义（`chlib/assert.h:47-51`）：

```cpp
#define CH_ASSERT(cond, msg) \
    do { \
        std::cout << "[CH_ASSERT] " << msg << " @ " \
                  << __FILE__ << ":" << __LINE__ << std::endl; \
    } while(0)
```

`cond` 参数被完全忽略。这是一个轻微的技术债务，但与形式验证问题**独立**——它是仿真断言（run-time assertion）而非形式验证（formal property）。

---

## 3. 决议

**搁置（低优先级）**。待到以下条件之一满足时再实施：

1. 有具体的 formal 验证需求（如安全关键设计需要完整的形式化覆盖）
2. Verilog codegen 先完成全部操作补齐（ADR-019），codegen 稳定后再接入 SVA
3. 需要与 SymbiYosys / JasperGold 等 formal 工具联合工作

### 现有断言基础设施的注意事项

| 组件 | 建议 | 理由 |
|------|------|------|
| `AssertChecker` | 保留现状 | 对仿真调试有价值，可用于 Component 内部信号校验 |
| `CH_ASSERT` 宏 | 添加 TODO 标注 | 空壳不影响功能，修复是独立的 trivial 任务 |
| SVA / assume/cover | 搁置 | 需 formal 工具链依赖确定后再设计 |

---

## 4. 变更日志

| 日期 | 版本 | 变更 | 作者 |
|------|------|------|------|
| 2026-05-08 | v1.0 | 初始版本：记录形式验证接口评估结果，标记为低优先级搁置 | Sisyphus |

---

**相关链接**:
- `include/chlib/assert.h` — 当前断言实现（`AssertChecker` + `CH_ASSERT` 宏）
- `src/codegen_verilog.cpp:258-260` — codegen footer（SVA 的插入位置）
- `examples/spinalhdl-ported/assert/assert_example.cpp` — 断言使用示例
- `docs/adr/ADR-019-verilog-codegen-completeness.md` — codegen 操作补齐（formal 的前置依赖）
- `docs/adr/ADR-DISCUSSION-PLAN.md` — 议题 #19
