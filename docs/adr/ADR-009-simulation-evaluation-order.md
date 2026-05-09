# ADR-009: 仿真评估顺序

**状态**: ✅ 已采纳  
**日期**: 2026-05-06  
**决策人**: Sisyphus + 用户  
**优先级**: P0（关联 PRD T1）

---

## 1. 背景

`Simulator::tick()` 的评估顺序决定了 HDL 仿真的正确性。当前实现存在三个耦合的问题：电平检测 vs 边沿检测、双 `eval()` 调用的正确性、以及输入节点分类启发式的可靠性。

本文档记录这些问题的分析过程和决议。

## 2. 当前设计

### 2.1 `tick()` 执行流程

```
Simulator::tick()                    // simulator.cpp:804
├── eval_combinational()             // ① 组合逻辑
│   ├── input_instr_list_            //    顶层输入
│   └── combinational_instr_list_    //    组合节点
├── eval()                           // ② 第一次时钟求值
│   ├── default_clock_instr_->eval() //    时钟 0→1（上升沿）
│   ├── other clocks...
│   └── if clock==0 → eval_combinational()
│       else        → eval_sequential()
├── eval()                           // ③ 第二次时钟求值
│   ├── default_clock_instr_->eval() //    时钟 1→0（下降沿）
│   └── 同上分支判断
└── trace()                          // ④ 波形记录
```

### 2.2 电平检测

```cpp
// simulator.cpp:871-874
if (default_clock_data_->is_zero()) {
    eval_combinational();    // 时钟=0 → 组合
} else {
    eval_sequential();       // 时钟=1 → 时序
}
```

### 2.3 输入分类

```cpp
// simulator.cpp:606-612
case type_input:
    if (!node->get_parent())      → input_instr_list_
    else                           → combinational_instr_list_
```

## 3. 决策

### D1: 采用真正的边沿检测

**决议**: ✅ **将电平检测改为边沿检测**（PRD T1）。

**当前问题**: `is_zero()` 检测的是时钟的电平状态，而非边沿事件。这使得：
- 寄存器在时钟高电平的整个周期都可能更新（事实上寄存器应在上升沿的瞬间更新）
- 组合逻辑和时序逻辑的分离不够严格

**目标设计**:
```cpp
// 每次 tick 时检测时钟边沿
void Simulator::tick() {
    // 1. 运行组合逻辑（纯函数）
    eval_combinational();

    // 2. 检测时钟上升沿
    bool posedge = (old_clock_value_ == 0) && (new_clock_value_ == 1);
    
    // 3. 更新时钟值
    default_clock_instr_->eval();
    
    // 4. 如果是上升沿，运行时序逻辑
    if (posedge) {
        eval_sequential();
    }

    // 5. 波形记录
    if (trace_on_) trace();
}
```

**理由**:
1. 与真实硬件行为一致——寄存器在时钟上升沿采样
2. 消除双 `eval()` 的副作用——时序逻辑只在边沿触发时执行一次
3. 与 SpinalHDL/Verilog 的仿真语义一致

**影响**:
- `instr_reg::eval()` 中的时钟检测逻辑需要相应修改
- 边沿检测的具体实现（保存前一次时钟值 + 比较）需要增加状态

### D2: 将双 `eval()` 重新设计为边沿触发

**决议**: ✅ **用边沿触发模型替代双 `eval()`**。

**理由**:
1. 双 `eval()` 的设计源于电平检测的局限——需要两次切换时钟值来完成一个周期
2. 边沿检测模型只需一次时钟更新 + 一次边沿检查，流程更清晰
3. 消除了两次 `eval()` 之间的隐含状态依赖

**新的 `tick()` 流程**:

```
Simulator::tick()
├── eval_combinational()          // 1. 组合逻辑（每次 tick 执行）
├── evaluate_clocks()             // 2. 更新时钟值，返回是否上升沿
├── if (posedge):
│   └── eval_sequential()         // 3. 时序逻辑（仅上升沿时）
└── trace()                       // 4. 波形记录
```

**CRITICAL 规则**:
- 组合逻辑必须在时钟更新**之前**执行（因为时序逻辑的输入是组合逻辑的输出）
- 时序逻辑在时钟更新**之后**执行（寄存器在时钟边沿采样组合逻辑的结果）
- 一个 `tick()` = 一个时钟周期（非两个半周期）

### D3: 输入分类启发式保持现状

**决议**: ✅ **保持 `get_parent()` 启发式，增加文档说明其假设**。

**理由**:
1. CppHDL 的组件层次通常较浅（1-2 层），该启发式在浅层次下工作良好
2. 当前没有发现该启发式导致错误的实际案例
3. 改进方案（递归遍历父组件链）会增加运行时开销且收益有限

**假设**: 根组件的 IO 端口有 `parent_ == nullptr`，嵌套组件的 IO 端口有非空 `parent_`。

**限制**: 对于深度嵌套的层次化设计，此分类可能不准确。目前不属于 P0/P1 问题。

## 4. 完整的 `tick()` 目标设计

```cpp
void Simulator::tick() {
    if (!initialized_ || disconnected_ || !ctx_) {
        CHERROR("Simulator not initialized or disconnected");
        return;
    }

    // 1. 组合逻辑求值（使用当前时钟状态下的输入）
    eval_combinational();

    // 2. 时钟更新 + 边沿检测
    bool posedge = false;
    if (default_clock_instr_) {
        auto old_val = *default_clock_data_;
        default_clock_instr_->eval();
        auto new_val = *default_clock_data_;
        // 上升沿检测：旧值=0 且 新值=1
        posedge = (old_val.is_zero() && !new_val.is_zero());
    }

    // 3. 上升沿触发时序逻辑
    if (posedge) {
        eval_sequential();
    }

    // 4. 波形记录
    if (trace_on_) {
        trace();
    }
}
```

## 5. 架构影响

| 影响领域 | 说明 |
|---------|------|
| **`instr_reg::eval()`** | 寄存器指令不再需要自己检测时钟边沿——边沿检测由 Simulator 统一处理 |
| **`instr_clock::eval()`** | 时钟指令只需切换值，不负责触发逻辑 |
| **`Simulator` 状态** | 需要保存前一时钟值用于边沿比较 |
| **双 `eval()` 调用** | 移除（simulator.cpp:841-842），改为单次时钟更新 + 边沿检查 |
| **JIT 模式** | `tick_comb()` 和 `tick_seq()` 的执行顺序不变，但触发条件变化 |
| **PRD 任务** | 实现此决策即为 PRD T1（时钟边沿检测修复） |

## 6. 变更日志

| 日期 | 版本 | 变更 | 作者 |
|------|------|------|------|
| 2026-05-06 | v1.0 | 初始版本，记录评估顺序分析、边沿检测决策 | Sisyphus + 用户 |

---

**相关链接**:
- `src/simulator.cpp` — `tick()`、`eval()`、`eval_combinational()`、`eval_sequential()` 实现
- ADR-005 — 双模式仿真设计（影响 JIT 模式下的 tick 流程）
- PRD T1 — 时钟边沿检测修复任务
- `include/ast/instr_reg.h` — 寄存器指令（受边沿检测变化影响）

---

**决策人**: AI Agent  
**审阅人**: 用户  
**状态**: ✅ 已采纳
