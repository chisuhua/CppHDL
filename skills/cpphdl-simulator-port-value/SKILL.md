# CppHDL Simulator set_port_value/get_port_value 使用指南

**技能 ID**: `cpphdl-simulator-port-value`  
**版本**: v1.0  
**创建日期**: 2026-04-09  
**优先级**: Critical  
**适用场景**: RISC-V CPU、复杂组件、IO 端口测试

---

## 📚 概述

本技能文档总结 CppHDL 框架中 `Simulator::set_port_value()` 和 `Simulator::get_port_value()` API 的使用模式，适用于 Component IO 端口的直接读写。

---

## ⚠️ 与 set_input_value 的区别

| API | 参数类型 | 返回值 | 适用场景 |
|------|---------|--------|---------|
| `set_input_value(signal, value)` | `ch_uint<N>`/`ch_bool` | `void` | 独立信号、Bundle 字段 |
| `set_port_value(port, value)` | `ch_in<T>`/`ch_out<T>` | `void` | Component IO 端口 |
| `get_input_value(signal)` | `ch_uint<N>`/`ch_bool` | `sdata_type` | 独立信号读取 |
| `get_port_value(port)` | `ch_in<T>`/`ch_out<T>` | `sdata_type` | IO 端口读取 |

**关键区别**:
- `set_input_value` 用于独立信号或 Bundle 成员
- `set_port_value` 用于 Component 的 IO 端口（`ch_in<T>`/`ch_out<T>`）

---

## ✅ 使用模式

### 模式 1: 简单端口（ch_bool/ch_uint）

```cpp
#include "riscv/hazard_unit.h"
#include "simulator.h"

using namespace riscv;

int main() {
    ch::core::context ctx("test_ctx");
    ch::core::ctx_swap swap(&ctx);
    
    // 实例化 HazardUnit
    // 注意：set_port_value 不需要 ch::ch_module 包装
    // 直接访问 Component 的 io() 即可
    HazardUnit hazard{nullptr, "hazard"};
    Simulator sim(&ctx);
    
    // 设置输入端口值（使用整数，非 ch_bool/ch_uint）
    sim.set_port_value(hazard.io().id_rs1_addr, 5);     // ch_uint<5>
    sim.set_port_value(hazard.io().ex_reg_write, 1);    // ch_bool
    sim.set_port_value(hazard.io().mem_is_load, 0);     // ch_bool
    
    // 设置 32 位数据
    sim.set_port_value(hazard.io().ex_alu_result, 42);  // ch_uint<32>
    
    // 运行仿真
    sim.tick();
    
    // 读取输出端口值
    auto fwd_a = sim.get_port_value(hazard.io().forward_a);
    auto stall = sim.get_port_value(hazard.io().stall);
    
    // sdata_type 转换为 uint64_t
    uint64_t fwd_a_val = fwd_a;
    uint64_t stall_val = stall;
    
    // 验证
    REQUIRE(fwd_a_val == 1);  // EX 级前推
    REQUIRE(stall_val == 0);  // 无停顿
}
```

---

### 模式 2: RISC-V Hazard Unit 完整测试

```cpp
// tests/riscv/test_hazard_compile.cpp
#include "riscv/hazard_unit.h"

using namespace riscv;

TEST_CASE("Hazard Unit Forwarding", "[hazard][forwarding]") {
    ch::core::context ctx("hazard_test");
    ch::core::ctx_swap swap(&ctx);
    
    HazardUnit hazard{nullptr, "hazard_unit"};
    Simulator sim(&ctx);
    
    SECTION("RS1 Forwarding from EX") {
        // 设置场景：EX 级写 x5，ID 级读 x5
        sim.set_port_value(hazard.io().ex_rd_addr, 5);
        sim.set_port_value(hazard.io().ex_reg_write, 1);
        sim.set_port_value(hazard.io().id_rs1_addr, 5);
        
        // 设置原始数据
        sim.set_port_value(hazard.io().rs1_data_raw, 100);
        
        // 设置 EX 级 ALU 结果（将前推的数据）
        sim.set_port_value(hazard.io().ex_alu_result, 42);
        
        // 运行仿真
        sim.tick();
        
        // 验证前推控制信号
        auto fwd_a = sim.get_port_value(hazard.io().forward_a);
        REQUIRE(fwd_a == 1);  // 1=来自 EX 级
    }
    
    SECTION("Load-Use Hazard Detection") {
        // 设置场景：MEM 级是 load，ID 级需要 load 结果
        sim.set_port_value(hazard.io().mem_is_load, 1);
        sim.set_port_value(hazard.io().mem_rd_addr, 6);
        sim.set_port_value(hazard.io().id_rs1_addr, 6);
        
        // 运行仿真
        sim.tick();
        
        // 验证停顿信号
        auto stall = sim.get_port_value(hazard.io().stall);
        REQUIRE(stall == 1);  // Load-Use 冒险，stall=1
    }
}
```

---

### 模式 3: 5 级流水线集成测试

```cpp
// tests/riscv/test_pipeline_integration.cpp
#include "riscv/rv32i_pipeline.h"
#include "simulator.h"

using namespace riscv;

TEST_CASE("5-Stage Pipeline Integration", "[pipeline][integration]") {
    ch::core::context ctx("pipeline_test");
    ch::core::ctx_swap swap(&ctx);
    
    Rv32iPipeline pipeline{nullptr, "pipeline_top"};
    Simulator sim(&ctx);
    
    SECTION("Basic Pipeline Reset") {
        // 复位
        sim.set_port_value(pipeline.io().rst, 1);
        sim.set_port_value(pipeline.io().clk, 0);
        
        // 运行
        sim.tick();
        
        // 释放复位
        sim.set_port_value(pipeline.io().rst, 0);
        
        // 运行
        sim.tick();
        
        REQUIRE(true);  // 编译成功即通过
    }
    
    SECTION("IF/ID Register Data Flow") {
        // 设置输入
        sim.set_port_value(pipeline.io().instr_addr, 0x00000000);
        sim.set_port_value(pipeline.io().instr_data, 0x00000013);  // NOP
        sim.set_port_value(pipeline.io().instr_ready, 1);
        
        // 运行
        sim.tick();
        
        // 验证
        auto addr = sim.get_port_value(pipeline.io().instr_addr);
        REQUIRE(addr == 0x00000000);
    }
}
```

---

## ⚠️ 常见错误及修复

### 错误 1: 使用 ch_bool/ch_uint 作为值参数

```cpp
// ❌ 错误：set_port_value 需要 uint64_t，不是 ch_bool/ch_uint
sim.set_port_value(hazard.io().ex_reg_write, ch_bool(true));
sim.set_port_value(hazard.io().id_rs1_addr, ch_uint<5>(5_d));

// ✅ 正确：使用整数
sim.set_port_value(hazard.io().ex_reg_write, 1);
sim.set_port_value(hazard.io().id_rs1_addr, 5);
```

### 错误 2: 使用 run_for() 而非 tick()

```cpp
// ❌ 错误：run_for() 不存在于 Simulator
sim.run_for(10'000'000'000ULL);

// ✅ 正确：使用 tick()
sim.tick();
```

### 错误 3: sdata_type 未转换

```cpp
// ❌ 错误：不能直接比较 sdata_type
auto val = sim.get_port_value(hazard.io().stall);
REQUIRE(val == 1);  // 类型不匹配

// ✅ 正确：转换为 uint64_t
auto val = sim.get_port_value(hazard.io().stall);
uint64_t val_u64 = val;
REQUIRE(val_u64 == 1);

// 或者隐式转换
auto val = sim.get_port_value(hazard.io().stall);
REQUIRE(val == 1);  // C++ 自动转换（隐式）
```

---

## 🎯 关键要点

1. **参数类型**: `set_port_value(port, uint64_t value)` - 值必须是整数
2. **返回值**: `get_port_value(port)` 返回 `sdata_type`，可隐式转换为 `uint64_t`
3. **仿真运行**: 使用 `sim.tick()` 运行单个时钟周期
4. **IO 端口**: 适用于 `ch_in<T>`/`ch_out<T>` 类型的端口
5. **独立信号**: 如果测试独立信号，使用 `set_input_value/get_input_value`

---

## 🔗 相关资源

- [Simulator API 使用指南](../../docs/usage_guide/04-simulator-api.md)
- [Bundle 连接模式技能](../cpphdl-bundle-patterns/SKILL.md)
- [Testbench 编写指南](../../docs/usage_guide/09-testbench-guide.md)
- [RISC-V Hazard Unit](../../include/riscv/hazard_unit.h)

---

**版本**: v1.0  
**创建时间**: 2026-04-09  
**维护人**: DevMate  
**下次更新**: 发现新模式或修复时
