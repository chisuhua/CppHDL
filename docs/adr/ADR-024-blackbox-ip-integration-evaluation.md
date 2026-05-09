# ADR-024: BlackBox/IP 集成评估

**状态**: ✅ 已采纳（低优先级搁置）
**日期**: 2026-05-08
**决策人**: Sisyphus + 用户

---

## 1. 背景

议题 #18 评估是否在 CppHDL 中实现 BlackBox（外部 Verilog/VHDL IP 包装）机制。

### 1.1 当前状态

| 组件 | 状态 | 说明 |
|------|------|------|
| Component 层次 | ✅ | `create_child()` / `children_shared_` 支持嵌套组件 |
| Verilog codegen | ⚠️ 仅 flat | `src/codegen_verilog.cpp:192-260` — 始终生成单一 `module top`，无模块实例化 |
| DPI/VPI/cosim | ❌ | 零实现 |
| 外部 IP 依赖 | N/A | 17 个示例 + 全部 AXI/CPU 组件均纯 C++ 描述 |

### 1.2 SpinalHDL BlackBox 对比

| 功能 | SpinalHDL | CppHDL 差距 | 实现难度 |
|------|-----------|-------------|---------|
| IO 接口声明 | `val io = new Bundle { ... }` | 已通过 Component + IO 端口支持 | — |
| Verilog 参数 | `addGeneric("WIDTH", 8)` | 无机制 | 低 |
| 模块实例化代码生成 | `module #(...) inst(...)` | codegen 仅 flat `module top` | 中 |
| 仿真行为 | DPI / 回调 | 无机制 | 高 |

---

## 2. 分析

### 2.1 用户需求评估

当前代码库所有设计的 HDL 逻辑完全在 C++ 中描述：

```cpp
// examples/spinalhdl-ported/ 中的 17 个示例
// include/axi4/, include/cpu/ 中的所有组件
// 全部使用 Component::describe() 定义行为，无外部 IP 依赖
```

**结论：当前零需求。**

### 2.2 实施成本

| 层面 | 所需变更 | 工作量 |
|------|---------|--------|
| API 设计 | 定义 `BlackBox<T>` 基类 + 参数声明 DSL | 0.5 天 |
| Codegen | verilogwriter 需支持：非 top 模块生成、端口连接、参数实例化 | 1-2 天 |
| 仿真 | 需决定策略：callback / shared lib / DPI / skip，实现或预留钩子 | 1-3 天 |

### 2.3 可行实施方案（未来参考）

```cpp
// 未来如要实施，最小 API 设计
class BlackBox : public Component {
    // 不覆盖 describe() — 无内部逻辑
    // codegen 时识别为外部模块并生成实例化
};

class MyExternalIp : public BlackBox {
    ch_in<ch_uint<8>> data_in;
    ch_out<ch_uint<8>> data_out;
    
    // Verilog 参数
    int WIDTH = 8;
    
    // 仿真回调（可选）
    std::function<void(Simulator&)> on_tick;
};
```

---

## 3. 决议

**搁置（低优先级）**。待到以下条件之一满足时再实施：

1. 有具体的外部 IP 集成需求（如 DDR 控制器、Ethernet MAC 的 Verilog 包装）
2. Verilog codegen 先完成批量操作补齐（ADR-019），再重构实例化支持
3. 需要与其他 Verilog 仿真器联合仿真（co-simulation）

### 触发条件

| 触发条件 | 预计时间 | 说明 |
|---------|---------|------|
| 外部 IP 集成需求出现 | 未知 | DDR/Ethernet/USB 控制器等 |
| Verilog codegen 重构 | ADR-019 阶段二实施后 | codegen 稳定性提升后再加实例化 |
| co-simulation 需求 | 未知 | 与 iverilog/vcs 联合仿真 |

---

## 4. 变更日志

| 日期 | 版本 | 变更 | 作者 |
|------|------|------|------|
| 2026-05-08 | v1.0 | 初始版本：记录 BlackBox/IP 集成评估结果，标记为低优先级搁置 | Sisyphus |

---

**相关链接**:
- `src/codegen_verilog.cpp:192-260` — 当前 flat module 输出
- `include/codegen_verilog.h` — verilogwriter 类
- `include/component.h` — Component 基类（BlackBox 的父类候选）
- `docs/adr/ADR-019-verilog-codegen-completeness.md` — codegen 操作补齐（优先于 BlackBox）
- `docs/adr/ADR-DISCUSSION-PLAN.md` — 议题 #18
