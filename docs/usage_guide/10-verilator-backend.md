# Verilator 仿真后端 使用指南

**文档编号**: USAGE-10
**版本**: v1.0
**最后更新**: 2026-06-07
**目标读者**: CppHDL 用户（需要高性能仿真的硬件设计者）
**前置知识**: CppHDL 基础, Verilog, Verilator 工具链

---

## 本章目标

学完本章，你将能够：

1. ✅ 理解 CppHDL 的三种仿真后端（解释器 / JIT / Verilator）
2. ✅ 启用 Verilator 仿真后端
3. ✅ 验证你的设计在 Verilator 中可综合
4. ✅ 解读 Verilator 错误并修复
5. ✅ 理解当前实现状态与未来工作

---

## 快速开始

### 安装 Verilator

```bash
# Ubuntu 24.04 (apt 仓库自带 5.020-1)
sudo apt install verilator iverilog

# 验证
verilator --version   # 5.020 或更高
iverilog -V            # 12.0 或更高
```

### 最简示例

```cpp
#include "ch.hpp"
#include "codegen_verilog.h"
#include "core/verilator_backend.h"
#include "core/eval_backend.h"

using namespace ch::core;

int main() {
    auto ctx = std::make_unique<context>("my_design");
    ctx_swap guard(ctx.get());

    ch_out<ch_uint<4>> out_port("io");
    ch_reg<ch_uint<4>> reg_c(0_d, "counter");
    reg_c->next = reg_c + 1_d;
    out_port <<= reg_c;

    ch::data_map_t data_map;
    ch::VerilatorBackend backend("/tmp/my_verilator_workdir");
    if (!backend.initialize(ctx.get(), data_map)) {
        std::cerr << "Failed to initialize Verilator backend\n";
        return 1;
    }

    // Phase 3.2-3.3: eval() 会通过 Vtop 驱动 design
    std::vector<std::pair<uint32_t, ch::instr_base *>> empty;
    backend.eval_combinational(data_map, empty, empty);
    return 0;
}
```

---

## 当前实现状态（ADR-035 Phase 1-4.1）

### ✅ 已完成

| 功能 | 状态 | 说明 |
|------|------|------|
| **Verilog codegen 修复** | ✅ | 5 个 Verilator 阻塞已修（默认 clock/reset 在 port list, 多输出发射, 走 proxy 链, bundle 字段名, input 重复声明移除） |
| **iverilog/verilator 验证** | ✅ | 6 个端到端测试通过 iverilog 和 verilator --lint-only |
| **IEvalBackend 抽象** | ✅ | `include/core/eval_backend.h` 定义统一后端接口 |
| **InterpreterBackend** | ✅ | 包装现有解释器循环的默认后端 |
| **VerilatorBackend 脚手架** | ✅ | 生成 Verilog, 调用 verilator --cc, SHA-1 缓存键计算 |
| **8 个后端测试** | ✅ | 7 通过, 1 跳过（verilator 慢构建时） |

### ⏳ 待完成（Phase 3.2-3.6）

| 功能 | 状态 | 说明 |
|------|------|------|
| dlopen .so 加载 Vtop* | ⏳ | 需要生成 C++ wrapper 暴露 factory + eval 函数 |
| ISignalAccess*[] 端口表 | ⏳ | O(1) 端口访问（SpinalHDL 模式） |
| data_map_ ↔ Vtop 同步 | ⏳ | 输入写入 Vtop, 输出读回 data_map_ |
| 时钟模型适配 | ⏳ | CppHDL 的 3-eval/tick vs Verilator 单步 |
| SHA-1 缓存命中 | ⏳ | 增量构建，避免重复 verilator 编译 |
| Simulator::set_backend() | ⏳ | 完整重构，让 ch::Simulator 使用 IEvalBackend |

**当前性能预期**: VerilatorBackend 脚手架可生成 Verilog 并触发 verilator 编译，但**不能**实际驱动仿真。等待 Phase 3.2-3.3 完成。

---

## Verilog 可综合性要求

CppHDL 的 Verilog 输出必须满足以下条件才能被 Verilator 接受（已通过 `tests/test_verilog_external.cpp` 验证）：

| 要求 | 验证测试 | 状态 |
|------|----------|------|
| `default_clock` 在 port list | `DefaultClockDeclaredInPortList` | ✅ |
| `default_reset` 在 port list | `DefaultResetDeclaredInPortList` | ✅ |
| 所有 `ch_out` 端口都在 port list | `AllOutputsInPortList` | ✅ |
| 端口名不被 body 重复声明 | `NoOutputRedeclarationInBody` | ✅ |
| 寄存器无重复声明 | (iverilog/verilator 验证) | ✅ |
| 输出连到正确的 driver（多 reg 设计） | `OutputConnectsToActualSource` | ✅ |
| Bundle 字段名被保留 | `BundleFieldNamesPreserved` | ✅ |
| 关键字冲突（如 `reg`） | 测试已用非关键字名称 | ⚠️ 需用户注意 |

### 关键字冲突

Verilog 关键字（`reg`, `wire`, `input`, `output`, `module` 等）不能用作信号名。如果你的 CppHDL 设计有：

```cpp
ch_reg<ch_uint<4>> reg(0_d, "reg");  // ❌ Verilog 编译失败
ch_reg<ch_uint<4>> reg_(0_d, "reg_"); // ✅ OK
```

请使用非关键字名称。

---

## 调试 Verilator 错误

### 错误 1: "default_clock is not declared"

**原因**: 旧版 codegen bug（ADR-035 Phase 1.1 已修）

**修复**: 升级到最新 CppHDL 或参考 [docs/adr/ADR-035-verilator-backend.md](../adr/ADR-035-verilator-backend.md)

### 错误 2: "Duplicate declaration of signal"

**原因**: 旧版 codegen 重复声明 reg（ADR-035 Phase 1.6 已修）

**修复**: 升级到最新 CppHDL

### 错误 3: Bundle 字段名变 `top_io_N`

**原因**: 旧版 set_name_prefix 不传播到 slice lnode（ADR-035 Phase 1.4 部分修）

**修复**: 简单 Bundle（`ch_in<>`/`ch_out<>`）已修。复杂嵌套 Bundle（AXI4 等）仍是已知限制，Phase 3+ 工作。

### 错误 4: `iverilog` 报"语法错误"但 `cat file.v` 看起来正确

**原因**: 可能与关键字冲突（见上）

**调试**:
```bash
# 用 iverilog 详细输出定位
iverilog -g2012 -Wall -o /dev/null your_design.v 2>&1 | head -20
```

---

## 验证你的设计

### 步骤 1: 生成 Verilog

```cpp
ch::toVerilog("my_design.v", ctx.get());
```

### 步骤 2: 用 iverilog 验证

```bash
iverilog -g2012 -Wall -o /dev/null my_design.v
```

### 步骤 3: 用 Verilator lint

```bash
verilator --lint-only -Wno-WIDTH -Wno-UNOPTFLAT my_design.v
```

### 步骤 4: 用 Verilator 编译（可选）

```bash
verilator --cc my_design.v
# 输出: obj_dir/Vtop.cpp, Vtop.h, Vtop__Syms.cpp, Vtop__ALL.a
```

这些步骤与 `tests/test_verilog_external.cpp` 完全对应。可以在你的 CI 中复用：

```cpp
TEST_CASE("MyDesign - VerilatorLints", "[verilator]") {
    auto ctx = std::make_unique<context>("my_design");
    // ... build your design ...

    ch::toVerilog("/tmp/my_design.v", ctx.get());
    ToolResult r = run_tool("verilator --lint-only /tmp/my_design.v");
    REQUIRE(r.exit_code == 0);
}
```

---

## 性能预期（基于 SpinalHDL 实测）

| 后端 | 仿真速度 | 启动时间 | 外部依赖 |
|------|----------|----------|----------|
| 解释器（默认） | 265 ticks/sec @ depth=1000 | <1ms | 无 |
| LLVM JIT | 5-10x 解释器 | ~50ms | LLVM 20+ |
| Verilator（理论） | 10-100x 解释器 | ~50ms（缓存命中） | Verilator 5.020+ |
| Verilator（首次构建） | 同上 | ~30s（verilate） | 同上 |

**注**: Verilator 后端的实际仿真（Phase 3.2+）尚未实现。当前 Phase 1-4.1 完成了 codegen 修复 + 脚手架。

---

## 架构图

```
┌─────────────────────────────────────────────────────────┐
│                   ch::Simulator                          │
│                  (public API 保持不变)                    │
│         set_input_value / get_value / tick               │
└────────────────────────┬────────────────────────────────┘
                         │ Phase 2.3 (pending)
                         ▼
        ┌────────────────────────────────────┐
        │       IEvalBackend (Phase 2.1)     │
        │   interface: initialize/eval_*/reset│
        └────────────┬───────────┬────────────┘
                     │           │
        ┌────────────▼───┐  ┌────▼─────────────┐
        │ Interpreter    │  │ VerilatorBackend │
        │ (Phase 2.2)    │  │ (Phase 3.1)      │
        │                │  │ scaffolding only │
        └────────────────┘  └──────────────────┘
                                     │
                                     ▼
                          ┌──────────────────────┐
                          │ verilator --cc       │
                          │ (Phase 3.5 缓存)     │
                          └──────────────────────┘
```

---

## 参考资料

- [ADR-035-verilator-backend.md](../adr/ADR-035-verilator-backend.md) — 完整决策记录
- [Verilator 用户指南](https://verilator.org/guide/latest/) — 官方文档
- [SpinalHDL VerilatorBackend](https://github.com/SpinalHDL/SpinalHDL/blob/dev/sim/src/main/scala/spinal/sim/VerilatorBackend.scala) — 生产级参考实现
- [CppHDL 测试](../tests/test_verilog_external.cpp) — 端到端验证测试

---

**维护**: AI Agent
**版本**: v1.0
