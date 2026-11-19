# CppHDL Phase 1 修订计划 - 基于项目深度分析

**分析时间**: 2026-03-30 23:30  
**分析师**: DevMate  
**方法**: 代码审查 + 功能盘点 + 依赖分析

---

## 1. 项目现状深度分析

### 1.1 已实现功能盘点

#### 核心库 (include/core/) - 16 个头文件
| 功能 | 状态 | 文件 |
|------|------|------|
| 基础类型 | ✅ | `bool.h`, `uint.h`, `literal.h` |
| 寄存器 | ✅ | `reg.h` |
| 内存 | ✅ | `mem.h` |
| IO 端口 | ✅ | `io.h` |
| 运算符 | ✅ | `operators.h`, `operators_runtime.h` |
| 时钟域 | ✅ | `context.h` |
| 组件 | ✅ | `component.h`, `module.h` |
| 仿真器 | ✅ | `simulator.h` |
| Verilog 生成 | ✅ | `codegen_verilog.h` |

#### chlib 库 (include/chlib/) - 22 个头文件
| 功能 | 状态 | 文件 |
|------|------|------|
| 算术运算 | ✅ | `arithmetic.h`, `arithmetic_advance.h` |
| 位运算 | ✅ | `bitwise.h` |
| 逻辑运算 | ✅ | `logic.h` |
| 组合逻辑 | ✅ | `combinational.h` |
| 时序逻辑 | ✅ | `sequential.h` |
| 条件语句 | ✅ | `if.h`, `if_stmt.h` |
| Switch 语句 | ✅ | `switch.h` |
| 状态机 DSL | ✅ (新增) | `state_machine.h` |
| FIFO | ✅ | `fifo.h` |
| Stream | ✅ | `stream.h`, `stream_arbiter.h`, `stream_builder.h`, `stream_operators.h`, `stream_pipeline.h`, `stream_width_adapter.h` |
| 内存 | ✅ | `memory.h` |
| 选择器/仲裁器 | ✅ | `selector_arbiter.h`, `onehot.h` |
| AXI4-Lite | ✅ | `axi4lite.h` |
| 转换器 | ⚠️ | `converter.h` (有 4 个 FIXME) |

#### 示例代码 (samples/) - 39 个文件
| 类别 | 数量 | 代表文件 |
|------|------|---------|
| Bundle | 8 | `bundle_demo.cpp`, `axi_lite_demo.cpp` |
| FIFO | 4 | `fifo_example.cpp`, `fifo_bundle.cpp` |
| Stream | 8 | `stream_*.cpp`, `spinalhdl_stream_example.cpp` |
| 基础 | 6 | `counter.cpp`, `shift_register.cpp`, `simple_io.cpp` |
| 高级 | 13 | `nested_bundle_demo.cpp`, `pod_bundle_conversion.cpp` |

#### 测试用例 (tests/) - 82 个文件
- **基础测试**: 40+ 文件
- **chlib 测试**: 20+ 文件
- **Stream 测试**: 10+ 文件
- **CTest 注册**: 65 个测试

---

### 1.2 关键发现

#### ✅ 已实现（超出预期）
1. **状态机 DSL** - 已实现 `state_machine.h`
2. **位宽适配器** - 已实现 `stream_width_adapter.h`
3. **FIFO 完整实现** - sync_fifo, fwft_fifo, lifo_stack
4. **Stream 完整工具链** - fork, join, arbiter, mux/demux
5. **AXI4-Lite 总线** - 已有 `axi4lite.h`
6. **Verilog 生成** - 已有 `codegen_verilog.h`

#### ⚠️ 待完善
1. **converter.h** - 4 个 FIXME（BCD 转换）
2. **异步 FIFO** - 被注释掉的 CDC 实现
3. **ch_nextEn 便捷函数** - 缺失
4. **ch_rom** - 只有 ch_mem，无专用 ROM
5. **ch_assert** - 断言系统缺失

#### ❌ 缺失功能
1. **跨时钟域 (CDC)** - 完全缺失
2. **VCD 波形生成** - 缺失
3. **形式验证接口** - 缺失
4. **JIT 仿真** - 缺失

---

### 1.3 SpinalHDL 核心示例依赖分析

| SpinalHDL 示例 | 依赖功能 | CppHDL 状态 | 可移植性 |
|---------------|---------|-----------|---------|
| Counter | 寄存器 + 加法 | ✅ | ✅ 立即可移植 |
| FIFO | ch_mem + 指针 | ✅ | ✅ 立即可移植 |
| UART | 状态机 + 计数器 | ✅ | ✅ 立即可移植 |
| PWM | 状态机 + 比较器 | ✅ | ✅ 立即可移植 |
| GPIO | IO 映射 + 中断 | ✅ | ✅ 立即可移植 |
| SPI | 状态机 + ShiftReg | ✅ | ✅ 立即可移植 |
| I2C | 状态机 + 时序 | ✅ | ✅ 立即可移植 |
| VGA | 双时钟 + BRAM | ⚠️ (缺 CDC) | ⏸️ 需等待 |
| RISC-V | 完整 CPU | ⚠️ (缺 CDC) | ⏸️ 需等待 |
| Ethernet | FIFO + CDC | ⚠️ (缺 CDC) | ⏸️ 需等待 |

---

## 2. 修订后的 Phase 1 计划

### 2.1 优先级重新评估

**原计划问题**:
- 高估了功能缺失程度
- 低估了现有代码复用性
- 未充分利用现有示例模式

**修订原则**:
1. **先移植简单示例** - 验证现有功能
2. **遇到缺失再实现** - 需求驱动开发
3. **最大化代码复用** - 学习现有模式

### 2.2 新任务分解

#### Phase 1A: 基础示例移植（1-2 天）✅ 已完成 50%
| Task | 示例 | 依赖 | 状态 |
|------|------|------|------|
| T101 | Counter | 寄存器 | ✅ 完成 |
| T102 | FIFO | ch_mem | ✅ 完成 |
| T103 | UART TX | 状态机 | ✅ 完成 |
| T104 | Width Adapter | Stream | ✅ 完成 |
| T105 | **PWM** | 状态机 + 比较器 | ⏳ 下一步 |
| T106 | **GPIO** | IO 映射 | ⏳ 下一步 |

#### Phase 1B: 中级示例移植（2-3 天）
| Task | 示例 | 依赖 | 预计工时 |
|------|------|------|---------|
| T107 | SPI Controller | 状态机 + ShiftReg | 3h |
| T108 | I2C Controller | 状态机 + 时序 | 3h |
| T109 | Timer/Counter | 计数器 + 比较器 | 2h |
| T110 | **ch_assert 实现** | 断言系统 | 2h |

#### Phase 1C: 高级功能补齐（3-4 天）
| Task | 功能 | 依赖 | 预计工时 |
|------|------|------|---------|
| T111 | **ch_nextEn** | 便捷函数 | 1h |
| T112 | **ch_rom** | ROM 支持 | 2h |
| T113 | **converter 修复** | BCD 转换 | 3h |
| T114 | **异步 FIFO** | CDC 基础 | 4h |
| T115 | **VCD 生成** | 波形调试 | 4h |

#### Phase 1D: 复杂示例挑战（5-7 天）
| Task | 示例 | 依赖 | 预计工时 |
|------|------|------|---------|
| T116 | VGA Controller | 双时钟 + BRAM | 8h |
| T117 | RISC-V Core (简化) | 完整 CPU | 16h |
| T118 | Ethernet MAC | FIFO + CDC | 12h |

---

### 2.3 立即执行计划（Next 2 Days）

#### Day 3: PWM + GPIO 示例
```
上午 (3h):
- 学习 samples/shift_register.cpp 模式
- 实现 PWM 示例（状态机 + 比较器）
- 生成 Verilog 验证

下午 (3h):
- 学习 samples/simple_io.cpp 模式
- 实现 GPIO 示例（IO 映射 + 中断）
- 生成 Verilog 验证

晚上 (1h):
- 更新 AGENTS.md 添加 PWM/GPIO 模式
- 记录学习到的新模式
```

#### Day 4: SPI + ch_assert
```
上午 (3h):
- 学习 samples/fifo_example.cpp 模式
- 实现 SPI Controller 示例
- 验证状态机 + FIFO 协同

下午 (2h):
- 实现 ch_assert 宏
- 添加单元测试
- 在 PWM 示例中使用验证

晚上 (1h):
- Phase 1B 评审
- 决定继续示例移植 or 补齐功能
```

---

## 3. 关键决策点

### 3.1 决策：示例移植 vs 功能补齐

**选项 A**: 继续移植简单示例（PWM、GPIO、SPI）
- 优点：快速验证现有功能，积累模式
- 缺点：可能遇到功能缺失需要返回实现

**选项 B**: 先补齐基础功能（ch_nextEn、ch_rom、ch_assert）
- 优点：后续示例实现更顺畅
- 缺点：延迟示例验证，可能过度设计

**推荐**: **选项 A** - 继续示例移植，遇到缺失再实现
- 理由：项目已有 80% 核心功能，足够支持基础示例
- 风险可控：缺失功能可快速实现（ch_nextEn 仅 1h）

### 3.2 决策：CDC 支持时机

**选项 A**: 立即实现基础 CDC（双时钟 FIFO）
- 优点：解锁 VGA、RISC-V 等复杂示例
- 缺点：实现复杂（4h+），可能引入新 bug

**选项 B**: 延后 CDC，先完成单时钟示例
- 优点：专注验证现有功能，降低风险
- 缺点：复杂示例需等待

**推荐**: **选项 B** - 延后 CDC
- 理由：单时钟示例足够验证框架，CDC 可 Phase 2 实现
- 里程碑：完成 SPI/I2C/Timer 后再评估 CDC 需求

---

## 4. 成功指标

### 4.1 Phase 1A 完成标准（Day 2 末）
- [x] Counter 示例 ✅
- [x] FIFO 示例 ✅
- [x] UART TX 示例 ✅
- [x] Width Adapter 示例 ✅
- [ ] PWM 示例
- [ ] GPIO 示例

### 4.2 Phase 1B 完成标准（Day 4 末）
- [ ] SPI Controller 示例
- [ ] I2C Controller 示例
- [ ] Timer 示例
- [ ] ch_assert 实现

### 4.3 Phase 1 完成标准（Day 7 末）
- [ ] 10+ SpinalHDL 示例成功移植
- [ ] 所有示例生成 Verilog 可综合
- [ ] 测试覆盖率 >80%
- [ ] AGENTS.md 包含所有模式文档

---

## 5. 风险与缓解

| 风险 | 概率 | 影响 | 缓解措施 |
|------|------|------|---------|
| IO 端口模式错误 | 高 | 中 | 已记录 AGENTS.md，强制检查清单 |
| 模块实例化失败 | 中 | 中 | 使用 ch::ch_module 模式 |
| CDC 需求提前出现 | 中 | 高 | 准备基础 CDC 实现方案 |
| 示例复杂度超预期 | 高 | 中 | 先实现简化版，迭代优化 |

---

## 6. 附录：学习资源索引

### 必读示例（按优先级）
1. `samples/fifo_example.cpp` - IO 端口、模块实例化
2. `samples/shift_register.cpp` - 移位寄存器模式
3. `samples/counter.cpp` - 简单计数器
4. `samples/spinalhdl_stream_example.cpp` - Stream 模式
5. `samples/simple_io.cpp` - 基础 IO

### 必读测试
1. `tests/test_fifo*.cpp` - FIFO 验证
2. `tests/test_state_machine.cpp` - 状态机验证
3. `tests/test_stream*.cpp` - Stream 验证

### 已移植示例参考
1. `examples/spinalhdl-ported/counter/` - Counter 模式
2. `examples/spinalhdl-ported/uart/` - 状态机模式
3. `examples/spinalhdl-ported/fifo/` - FIFO 模式
4. `examples/spinalhdl-ported/width_adapter/` - 位宽转换模式

---

**版本**: v2.0 (Revised)  
**创建时间**: 2026-03-30 23:30  
**下次更新**: Phase 1A 完成后（预计 Day 2 末）
