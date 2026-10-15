## ADDED Requirements

### Requirement: VerilatorBackend MUST 脉冲 default_reset 在 initialize 完成时

`VerilatorBackend::try_initialize()` MUST 在 Vtop 实例构造后、首次 `eval_fn_()` 之前，对 `default_reset` port 执行一次 1→eval→0→eval 脉冲（4-step 序列），让 `always_ff @(posedge clk or posedge default_reset)` 块在 `@(posedge default_reset)` 沿加载 ch_reg 的初值。复位脉冲的 4-step 序列 MUST 与 `VerilatorBackend::reset()` (src/core/verilator_backend.cpp:791-813) 复用同一实现（避免逻辑漂移）。此行为 MUST 在 `reset_field_ptr_` 为空（设计无 async reset port）时回退到 `set_input_fn_` 路径。

#### Scenario: ch_reg<ch_uint<N>>(INIT_d) 仿真后 val=INIT

- **WHEN** fixture 为 `ch_reg<ch_uint<N>> reg(10_d); reg->next = reg + 1_d; io().out = reg;`（N ∈ {4, 8, 16, 32, 64}）
- **AND** 用 `VerilatorBackend` 跑 1 cycle（tick(1)）
- **THEN** `data_map_.at(io().out.impl()->id())` == 11（N=8 时），interpreter 侧跑同样 fixture 也是 11
- **AND** 0_d fixture 行为不变（interpreter == verilator == 0 + 1 = 1）

#### Scenario: 多 ch_reg 非零起始值仿真

- **WHEN** fixture 包含 3 个 `ch_reg<ch_uint<8>>(0_d/10_d/20_d)`，每个 `next = reg + N_d`，`io().a/b/c/d = reg`
- **AND** 用 `VerilatorBackend` 跑 1 cycle
- **THEN** 三个 output port 的值分别为 `1, 12, 23`（interpreter 一致）
- **NOTE**: 这正是 `tests/test_verilator_trace_parity.cpp::TraceParity - BundleMultiFieldTwentyCycles` 的 D8 (b) descope 复归测试形态

#### Scenario: Simulator::set_backend 二次 initialize 幂等

- **WHEN** 同一 context 在两个 `ch_device<T>` 上各调一次 `Simulator::set_backend(VerilatorBackend(...))`
- **THEN** 第二次 initialize 的 reset 脉冲对同一 reg 加载相同 INIT 值，**无累积副作用**
- **AND** tick 后行为与单次 initialize 完全一致

#### Scenario: 无 default_reset port 设计 fallback

- **WHEN** fixture 为纯组合逻辑（无 `ch_reg`，无 reset 路径触发）
- **THEN** reset 脉冲的 `reset_field_ptr_ == nullptr` 分支走 `set_input_fn_` 路径（`reset_node_id_ != UINT32_MAX` 仍为 false），4-step 序列静默 no-op
- **AND** 现有 `test_verilog_external.cpp` 的 5 个真仿真测试不受影响

### Requirement: print_reg 字面量 MUST 按 reg 宽规范化

`src/codegen_verilog.cpp::print_reg()` 在生成复位分支字面量时 MUST 先用 `sdata_type(static_cast<uint64_t>(lit->value()), node->size())` 规范化到 reg 宽，再调用 `get_literal_str()` emit。生成的 Verilog 字面量 MUST 形如 `<reg_width>'h<value>`，不允许 `4'ha` 这类窄宽（< reg 宽）字面量出现在复位分支。

#### Scenario: ch_reg<ch_uint<8>>(10_d) 复位字面量宽度 == 8

- **WHEN** fixture 为 `ch_reg<ch_uint<8>> reg(10_d); reg->next = reg;`
- **AND** 生成 Verilog
- **THEN** 复位分支形如 `reg <= 8'ha;`（宽度 8，与 reg 宽匹配），**不是** `4'ha`

#### Scenario: ch_reg<ch_uint<64>>(0xFFFFFFFFFFFFFFFFULL) 复位字面量宽度 == 64

- **WHEN** fixture 为 `ch_reg<ch_uint<64>> reg(0xFFFFFFFFFFFFFFFFULL);`（边界 max 值）
- **AND** 生成 Verilog
- **THEN** 复位分支形如 `reg <= 64'hffffffffffffffff;`（宽度 64）

#### Scenario: ch_reg<ch_uint<8>>(ch_bool(true)) init 不走 type_lit 路径

- **WHEN** fixture init 为非字面量表达式（如 `ch_bool(true)`）
- **AND** 生成 Verilog
- **THEN** 复位分支回落到 `1'b0`，且触发 `[CHWARN] ch_reg '<name>' init_node is not type_lit (type=N)` 警告
- **NOTE**: 当前 `regimpl` 的 init_val 只支持 `type_lit`（src/core/node_builder.h:160-200），非 type_lit 路径实际不可达；警告是防御性的。

### Requirement: try_initialize 与 reset() MUST 共享 4-step 脉冲实现

`VerilatorBackend::reset()` 与 `VerilatorBackend::try_initialize()` 的复位脉冲序列 MUST 共享同一 helper 函数（避免逻辑漂移）。helper 的位置 SHOULD 在 `verilator_backend.cpp` 内的匿名 namespace，函数签名 `static void pulse_reset(VerilatorBackend *self)` 或等价形式。

#### Scenario: reset() 与 try_initialize 共用 helper

- **WHEN** 阅读 `src/core/verilator_backend.cpp`
- **THEN** `try_initialize` 与 `reset()` 的 4-step 序列（reset=1→eval→reset=0→eval）通过同一 helper 实现，无重复代码

#### Scenario: reset_node_id_ 路径与 reset_field_ptr_ 路径等价

- **WHEN** 设计有 reset_field_ptr_（有 async reset port）
- **THEN** helper 走 `*reset_field_ptr_` 直写路径
- **AND** 当 `reset_field_ptr_ == nullptr` 但 `reset_node_id_ != UINT32_MAX` 时，helper 走 `set_input_fn_` 路径
- **AND** 两条路径 4-step 序列的语义等价（都是 reset 1→eval→reset 0→eval）