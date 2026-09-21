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

### 启用 Verilator（git submodule）

> **不要 `apt install verilator`** —— CppHDL 通过 `third_party/verilator` 子模块
> 自己构建 Verilator（ExternalProject_Add），保证版本一致（v5.048）且无需
> 系统权限。`apt install verilator` 拿到的版本可能与本项目不兼容。
>
> 完整流程参见 `docs/developer_guide/verilator-integration.md`。
> 本节只给最常见的两个场景。

#### 场景 A：启用子模块并构建 Verilator（开发/CI 完整路径）

```bash
# 一次性：初始化子模块（首次 clone 后）
./scripts/init-submodules.sh

# 一次性：构建 Verilator（10-30 min cold，warm 秒级）
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc) --target verilator

# 之后：构建 CppHDL（verilator-install/ 已在 build/ 下）
cmake --build build -j$(nproc)
```

#### 场景 B：跳过 Verilator（快速迭代，CI 默认路径）

```bash
# 注意：如果 build/ 之前用 BUILD_VERILATOR=ON 配置过，先 rm -rf build
# （CMake 会缓存旧 option）
cmake -B build -DBUILD_VERILATOR=OFF
cmake --build build -j$(nproc)
```

此时 `perf_tests` 的 Verilator 列会显示 `UNSUPPORTED`（与集成前行为一致）。

### 工具链依赖（apt：iverilog + 编译器）

Verilator 子模块的 CMake `find_program(... REQUIRED)` 会自动检查 `perl` /
`flex` / `bison`，缺一即报错。`iverilog` 用于 `test_verilog_external.cpp`
的端到端 codegen 验证（与 Verilator 集成独立，需要 apt 安装）：

```bash
# Ubuntu 24.04
sudo apt install iverilog
iverilog -V   # 12.0 或更高
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

## 当前实现状态（ADR-035 Phase 1-4.1 + v2.1 e2e 验证）

### ✅ 已完成

| 功能 | 状态 | 说明 |
|------|------|------|
| **Verilog codegen 修复** | ✅ | 5 个 Verilator 阻塞已修（默认 clock/reset 在 port list, 多输出发射, 走 proxy 链, bundle 字段名, input 重复声明移除） |
| **iverilog/verilator 验证** | ✅ | 6 个端到端测试通过 iverilog 和 verilator --lint-only |
| **IEvalBackend 抽象** | ✅ | `include/core/eval_backend.h` 定义统一后端接口 |
| **InterpreterBackend** | ✅ | 包装现有解释器循环的默认后端 |
| **Simulator::set_backend()** | ✅ | Phase 2.3 完整重构，Simulator 可通过 `ch::IEvalBackend` 接口切换后端 |
| **Verilog + sim_main.cpp 生成** | ✅ | `generate_verilog()` 写 top.v + sim_main.cpp（含 extern "C" factory/eval/delete/set_input/get_output 符号） |
| **verilator --cc + dlopen .so** | ✅ | 两步编译：verilator --cc 生成 obj_dir/，再 `g++ -shared -fPIC -o libVtop.so`；dlopen 符号 7 个 |
| **data_map_ ↔ Vtop 同步** | ✅ | `sync_inputs_to_vtop()` 将 `data_map_` 输入推入 Vtop；`sync_outputs_from_vtop()` 将 Vtop 输出拉回 `data_map_` |
| **时钟模型适配** | ✅ | `type_clock` 节点作为 input 纳入 port_access_；Simulator::tick() 通过 `default_clock_instr_->eval()` 翻转时钟，backend 仅 sync；`eval_fn_()` 触发 Verilator `always_ff @posedge` |
| **SHA-1 缓存** | ✅ | 缓存键含 verilog_source + sim_main template + flags + verilator --version；产物 `~/.cache/cpphdl/verilator/<hash>/libVtop.so` |
| **VCD 跟踪 API** | ✅ | `enable_vcd()` toggle + `dump_vcd()` 写端口值到 .vcd 文件 |
| **Simulator 集成 + 多 backend 切换** | ✅ | `set_backend()` 热替换；`sim_setbackend_*` 测试覆盖 default/null/interpreter 场景 |
| **VerilatorBackend 测试** | ✅ | **26 个测试, 25 passed, 1 skipped, 76/76 assertions**（含 `E2E CounterSimulator50Cycles`—50 ticks 后 counter==50 🔥） |
| **线程安全销毁** | ✅ | `close_top()` 在 `dlclose` 前调用 `delete_fn_()` 销毁 Vtop 对象，VlThreadPool 线程池干净退出，消除多测试间 SIGSEGV |

### ⏳ 待完成

| 功能 | 状态 | 说明 |
|------|------|------|
| `riscv-mini` 端到端 | ⏳ | 需要 ChipForge 仓集成 + ELF 加载 |
| 性能基准对比（解释器 vs JIT vs Verilator） | ⏳ | `perf_tests` 框架已支持，需在 Verilator 安装环境跑 |
| A/B 验证 | ⏳ | `set_ab_verification(true)` 可用但比对逻辑为警告占位 |

### 🔬 e2e 验证（v2.1 新增）

```cpp
#include "ch.hpp"
#include "core/verilator_backend.h"
#include "simulator.h"

using namespace ch;

int main() {
    auto ctx = std::make_unique<ch::core::context>("my_design");
    ch::core::ctx_swap guard(ctx.get());

    // 32-bit counter + output port
    ch_reg<ch_uint<32>> counter(0_d, "ctr");
    counter->next = counter + 1_d;
    ch_out<ch_uint<32>> out_port("io");
    out_port <<= counter;

    // Simulator + VerilatorBackend
    Simulator sim(ctx.get());
    auto backend = std::make_unique<VerilatorBackend>("/tmp/verilator_workdir");
    sim.set_backend(std::move(backend));

    // 50 ticks -> counter == 50
    sim.tick(50);
    uint64_t val = static_cast<uint64_t>(sim.get_value(out_port));
    printf("counter after 50 ticks: %lu (expected 50)\n", val);
    return (val == 50) ? 0 : 1;
}
```

**当前状态**: VerilatorBackend **已能实际驱动仿真**。通过 `Simulator::set_backend()` 将后端注入后，`sim.tick(N)` 的 4-eval 时序（comb-1 → clock toggle → seq → clock toggle → comb-2 → comb-3）全部委托给 VerilatorBackend，Vtop 的 `always_ff @(posedge default_clock)` 正确递增计数器，`sync_outputs_from_vtop()` 将结果写回 `data_map_` 供 `sim.get_value()` 读取。

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

**注**: v2.1 e2e 验证已完成，VerilatorBackend **已能实际驱动仿真**（50 ticks 后 counter==50 已通过）。性能基准对比（解释器 vs JIT vs Verilator）待 `perf_tests` 在 Verilator 安装环境运行。

---

## 架构图

```
┌───────────────────────────────────────────────────────┐
│                 ch::Simulator                          │
│                (public API 保持不变)                    │
│       set_input_value / get_value / tick               │
└──────────────────────┬────────────────────────────────┘
                       │ set_backend()
                       ▼
       ┌────────────────────────────────────┐
       │       IEvalBackend (Phase 2.1)     │
       │   interface: initialize/eval_*/reset│
       └────────────┬───────────┬────────────┘
                    │           │
       ┌────────────▼───┐  ┌────▼─────────────────┐
       │ Interpreter    │  │ VerilatorBackend     │
       │ (Phase 2.2)    │  │ (Phase 3.1-3.6, v2.1)│
       │                │  │ real .so dispatch ✅  │
       └────────────────┘  └────┬─────────────────┘
                                │
                    ┌───────────▼───────────┐
                    │ verilator --cc        │
                    │ g++ -shared -fPIC     │
                    │ → libVtop.so          │
                    │ SHA-1 cache           │
                    └───────────┬───────────┘
                                │
                    ┌───────────▼───────────┐
                    │ Vtop eval()           │
                    │ sync_inputs →         │
                    │ eval_fn_() →          │
                    │ sync_outputs          │
                    │ delete_fn_() →        │
                    │ clean thread pool exit│
                    └───────────────────────┘
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
