# Pipeline Tick Crash Debugging Analysis

**文档版本**: 1.0  
**创建日期**: 2026-04-18  
**状态**: 分析完成，待修复

---

## 1. 问题概述

### 1.1 现象

当 `PipelineTop` (包含 `Rv32iPipeline` + `InstrTCM` + `DataTCM`) 调用 `sim.tick()` 时，仿真会挂起或崩溃。

```
test_pipeline_smoke: FAILED due to a fatal error condition: SIGTERM
```

### 1.2 Git 历史确认

此问题已知：

```
commit b1d075317f478378661b84091524ce6cf7ee4471
Author: Chi Suhua <chisuhua@gmail.com>
Date:   Thu Apr 16 01:52:24 2026 +0800

fix(pipeline): riscv-tests test architecture, pipeline tick crash remains
```

关键词：**"pipeline tick crash remains"** - 流水线 tick 崩溃问题仍然存在

---

## 2. 问题分析

### 2.1 ch_module 生命周期要求

**规则**：`ch_module<T>` 必须在 `Component::describe()` 方法内部调用。

**原因**：
1. `ch_module` 构造函数调用 `Component::current()` 获取父组件
2. 父组件用于建立上下文关系和管理生命周期
3. `ch_module` 内部创建的节点需要正确的上下文

**正确用法**：
```cpp
class PipelineTop : public ch::Component {
    void describe() override {
        ch::ch_module<Rv32iPipeline> pipeline{"pipeline"};
        ch::ch_module<InstrTCM<20, 32>> itcm{"itcm"};
        ch::ch_module<DataTCM<20, 32>> dtcm{"dtcm"};
        // ... connections
    }
};

// 测试中
ch::ch_device<PipelineTop> top;
Simulator sim(top.context());
```

**错误用法**（会导致 SIGSEGV）：
```cpp
// 在测试用例中直接调用 ch_module，没有父 Component
ch::ch_module<Rv32iPipeline> pipeline{"pipeline"}; // 错误！
```

### 2.2 最小复现测试

**通过测试** (`test_rv32i_pipeline_device.cpp`)：
```cpp
TEST_CASE("ch_device<Rv32iPipeline>: construct + tick") {
    context ctx("direct2");
    ctx_swap swap(&ctx);
    ch::ch_device<Rv32iPipeline> pipeline;
    Simulator sim(pipeline.context());
    sim.set_input_value(pipeline.instance().io().instr_data, 0x00000013);
    sim.set_input_value(pipeline.instance().io().instr_ready, 1);
    sim.set_input_value(pipeline.instance().io().data_read_data, 0);
    sim.set_input_value(pipeline.instance().io().data_ready, 1);
    sim.tick();  // ✅ 成功
    SUCCEED("OK");
}
```

**失败测试** (`test_pipeline_smoke.cpp`)：
```cpp
class PipelineTop : public ch::Component {
    void describe() override {
        ch::ch_module<Rv32iPipeline> pipeline{"pipeline"};
        ch::ch_module<InstrTCM<20, 32>> itcm{"itcm"};
        ch::ch_module<DataTCM<20, 32>> dtcm{"dtcm"};
        // ... connections
    }
};

// 测试中
ch::ch_device<PipelineTop> top;
Simulator sim(top.context());
sim.tick();  // ❌ 挂起
```

### 2.3 差异分析

| 组件 | `ch_device<Rv32iPipeline>` | `ch_device<PipelineTop>` |
|------|---------------------------|-------------------------|
| 包含模块 | 仅 Rv32iPipeline | Pipeline + ITCM + DTCM |
| 内部 ch_module | 0 个 | 3 个 (Pipeline, ITCM, DTCM) |
| 嵌套层级 | 1 层 (Pipeline) | 2 层 (Top → Pipeline → Stages) |
| tick() 结果 | ✅ 成功 | ❌ 挂起 |

**关键发现**：当 `ch_module<Rv32iPipeline>` 在 `PipelineTop::describe()` 中创建时，它内部又会创建 `ch_module<IfStage>`、`ch_module<IdStage>` 等子模块。这种**递归嵌套**可能是问题根源。

---

## 3. 流水线内部结构

### 3.1 Rv32iPipeline::describe() 内部模块

```cpp
void Rv32iPipeline::describe() override {
    // 5 级流水线模块
    ch::ch_module<IfStage>  if_stage{"if_stage"};
    ch::ch_module<IdStage>  id_stage{"id_stage"};
    ch::ch_module<ExStage>  ex_stage{"ex_stage"};
    ch::ch_module<MemStage> mem_stage{"mem_stage"};
    ch::ch_module<WbStage>  wb_stage{"wb_stage"};
    
    // 冒险检测单元
    ch::ch_module<HazardUnit> hazard{"hazard_unit"};
    
    // 流水线级寄存器文件 (32x32 bit)
    ch_mem<ch_uint<32>, 32> reg_file("reg_file");
    
    // EX/MEM 流水线寄存器
    ch_reg<ch_bool>     exmem_is_store(false, "exmem_is_store");
    ch_reg<ch_uint<32>> exmem_alu_result(0_d, "exmem_alu_result");
    ch_reg<ch_uint<32>> exmem_rs2_data(0_d, "exmem_rs2_data");
    
    // ... 连接逻辑
}
```

### 3.2 嵌套层级

```
ch_device<PipelineTop>
└── describe()
    └── ch_module<Rv32iPipeline> (parent: PipelineTop)
        └── describe()
            ├── ch_module<IfStage> (parent: Rv32iPipeline)
            ├── ch_module<IdStage> (parent: Rv32iPipeline)
            ├── ch_module<ExStage> (parent: Rv32iPipeline)
            ├── ch_module<MemStage> (parent: Rv32iPipeline)
            ├── ch_module<WbStage> (parent: Rv32iPipeline)
            ├── ch_module<HazardUnit> (parent: Rv32iPipeline)
            ├── ch_mem reg_file
            └── ch_reg exmem_*
```

**潜在问题**：每层嵌套的 `describe()` 调用中，`Component::current()` 需要正确维护。

---

## 4. 可能的问题根源

### 4.1 Component::current() 状态管理

在嵌套 `describe()` 调用中，`Component::current()` 的状态转换：

```cpp
void Component::build_internal(ch::core::context *target_ctx) {
    ch::core::ctx_swap ctx_guard(target_ctx);
    Component *old_comp = current_;  // 保存
    current_ = this;                   // 设置为当前
    
    describe();  // 调用用户的 describe()
    
    current_ = old_comp;  // 恢复
}
```

**潜在问题**：如果 `describe()` 内部创建的 `ch_module` 再次调用 `build()`，而 `current_` 没有正确恢复，会导致子模块找不到父组件。

### 4.2 流水线寄存器 (ch_reg) 时序

流水线内部定义了多个 `ch_reg`：

```cpp
ch_reg<ch_bool>     exmem_is_store(false, "exmem_is_store");
ch_reg<ch_uint<32>> exmem_alu_result(0_d, "exmem_alu_result");
ch_reg<ch_uint<32>> exmem_rs2_data(0_d, "exmem_rs2_data");
```

**潜在问题**：
1. 这些寄存器在仿真器 `tick()` 时如何更新？
2. 默认时钟 (`ctx->get_default_clock()`) 与外部 `clk` 输入的关系？
3. 复位信号如何传播到内部寄存器？

### 4.3 模块间连接

PipelineTop 中的连接：

```cpp
itcm.io().addr <<= pipeline.io().instr_addr;
pipeline.io().instr_data <<= itcm.io().data;
pipeline.io().instr_ready <<= itcm.io().ready;

dtcm.io().addr <<= pipeline.io().data_addr;
dtcm.io().wdata <<= pipeline.io().data_write_data;
dtcm.io().write <<= pipeline.io().data_write_en;
dtcm.io().valid <<= ch_bool(true);
pipeline.io().data_read_data <<= dtcm.io().rdata;
pipeline.io().data_ready <<= dtcm.io().ready;

pipeline.io().rst <<= ch_bool(false);
pipeline.io().clk <<= ch_bool(true);
```

**潜在问题**：
1. `clk` 和 `rst` 是常量 `ch_bool(true/false)`，但流水线内部的 `ch_reg` 使用 `ctx->get_default_clock()`。两者是否同步？
2. ITCM/DTCM 的 `valid` 信号为常量 `true`，是否会阻止流水线正常工作？

### 4.4 组合逻辑环

由于采用 Chisel 风格的延迟赋值 (`<<=`) 和组合逻辑推断，可能存在组合逻辑环导致仿真器无法收敛。

---

## 5. 已验证的测试结果

| 测试 | 结果 | 说明 |
|------|------|------|
| Phase 1 (RegFile/ALU/Decoder) | ✅ 通过 | 独立模块 |
| Phase 2 (Branch/Jump) | ✅ 通过 | 独立模块 |
| Phase 3 (Load/Store) | ✅ 通过 | 独立模块 |
| `ch_device<Rv32iPipeline>` + tick | ✅ 通过 | 无 ITCM/DTCM |
| `ch_device<PipelineTop>` + tick | ❌ 挂起 | 包含 ITCM/DTCM |

---

## 6. 后续调试建议

### 6.1 短期调试步骤

1. **添加详细日志**
   - 在 `Simulator::tick()` 和 `Simulator::eval()` 添加日志
   - 追踪每个时钟边沿的执行情况
   - 标识挂起发生的具体阶段

2. **使用 GDB 调试**
   ```bash
   gdb --args ./build/examples/riscv-mini/test_pipeline_smoke
   (gdb) run
   # 当挂起时 Ctrl+C
   (gdb) bt  # 打印堆栈
   ```

3. **简化测试场景**
   - 移除 `HazardUnit`，仅保留流水线核心逻辑
   - 逐步添加 ITCM、DTCM，定位哪个模块导致挂起

### 6.2 中期调查方向

1. **检查 `Component::current()` 状态**
   - 在 `ch_module` 构造函数中添加断言
   - 验证父组件指针在嵌套场景中是否正确

2. **验证流水线寄存器行为**
   - 检查 `ch_reg` 的 `next` 赋值是否正确传播
   - 验证时钟域切换是否正常工作

3. **检查 ITCM/DTCM 连接**
   - `instr_ready` 信号是否为常量 `true`？
   - `data_ready` 信号是否为常量 `true`？
   - 这些常量是否会阻止流水线正常暂停？

### 6.3 长期解决方案

1. **优化仿真速度**（如果确实是性能问题）
   - 当前 ~500ms/tick，需要 100x 提速
   - 考虑 IR 图简化、节点缓存

2. **修复流水线连接**
   - 如果确认是连接问题，重构模块间接口
   - 添加明确的握手信号

---

## 7. 相关文件

| 文件 | 说明 |
|------|------|
| `include/cpu/pipeline/rv32i_pipeline.h` | 流水线顶层模块 |
| `include/cpu/pipeline/stages/*.h` | 各流水线阶段定义 |
| `include/cpu/pipeline/rv32i_tcm.h` | ITCM/DTCM 内存模块 |
| `include/module.h` | `ch_module` 实现 |
| `src/component.cpp` | `Component::describe()` 实现 |
| `src/simulator.cpp` | `Simulator::tick()` 实现 |

---

## 8. 参考文献

- **ch_module 生命周期**: `include/module.h`
- **Component 上下文管理**: `src/component.cpp`
- **已知问题**: Git commit `b1d0753`
- **相关讨论**: `docs/riscv-mini/test-architecture.md`

---

**维护**: AI Agent  
**最后更新**: 2026-04-18
