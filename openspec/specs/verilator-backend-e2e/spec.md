# verilator-backend-e2e Specification

## Purpose
TBD - created by archiving change adr-035-phase-3-complete. Update Purpose after archive.
## Requirements
### Requirement: dlopen 真实 Vtop 与 extern "C" 符号解析
`VerilatorBackend::initialize()` 在 `verilator --cc --build` 成功生成 `obj_dir/Vtop` 后，**MUST** 真实 `dlopen` 该二进制并 `dlsym` 解析 5 个符号 (`new_Vtop`, `eval_Vtop`, `final_Vtop`, `delete_Vtop`, `set_input_Vtop`, `get_output_Vtop`, `get_field_ptr_Vtop`)。任一符号缺失 **MUST** `dlclose` + 返回 false，且 `CHWARN` log 缺失符号名。

#### Scenario: 7 符号全部成功解析
- **WHEN** `dlopen obj_dir/Vtop` 成功且 7 个 dlsym 都返回非 nullptr
- **THEN** `factory()` 创建 Vtop 实例，top_instance_ 非 nullptr，eval_fn_/final_fn_ 非 nullptr
- **THEN** `build_port_access_table()` 调 `get_field_ptr_Vtop` 填充所有 port 的 field_ptr
- **THEN** 第二次 `initialize()` 复用 dl_handle_ 不重新 dlopen

#### Scenario: 符号缺失优雅失败
- **WHEN** dlsym 任意一个返回 nullptr
- **THEN** `dlclose(dl_handle_)` + `dl_handle_ = nullptr` + 返回 false
- **THEN** `CHWARN("dlsym missing symbol: factory=%p eval=%p ...", ...)` log 指针地址

### Requirement: data_map ↔ Vtop 双向同步 (Phase 3.3 核心契约)
`VerilatorBackend::sync_inputs_to_vtop()` 与 `sync_outputs_from_vtop()` **MUST** 真正遍历 `port_access_` 表，把 `data_map_->at(id)` 写入 Vtop 字段、从 Vtop 字段读回 `data_map_->at(id)`。**MUST NOT** 仅 `CHINFO` log 而不读写数据。

#### Scenario: input field 写入生效
- **WHEN** `data_map_->at(id) = new_value`，然后 `eval_combinational()` 被调用
- **THEN** Vtop 内部对应 field_ptr 指向的位在 eval 后反映 new_value（通过 `get_output_Vtop` 读回能验）

#### Scenario: output field 读回正确
- **WHEN** Vtop 内部某个 output field 在 eval 后产生新值
- **THEN** `sync_outputs_from_vtop()` 把该新值写回 `data_map_->at(id)`

#### Scenario: 多 port 顺序与 SpinalHDL 一致
- **WHEN** port_access_ 有 N 个 entry（按 port id 升序）
- **THEN** sync_inputs 顺序写入 id=0, 1, ..., N-1（与 SpinalHDL VerilatorBackend `ISignalAccess*[]` 模式一致）

### Requirement: 3-eval/tick 时钟模型 (Phase 3.4 契约)
1 cycle **MUST** 等于 3 次 eval 调用，对应 ADR-009 仿真求值顺序：`eval_combinational → eval → eval_combinational`。

#### Scenario: 1 cycle 后 sequential 信号翻转
- **WHEN** 一个 4-bit counter 设计跑 Verilator sim 1 cycle (3 evals)
- **THEN** counter_value 增 1（与解释器 cycle-1 后 counter_value == 1 一致）

#### Scenario: default_clock 字段在 eval 前翻转
- **WHEN** `eval_sequential()` 被调用
- **THEN** 第一次 sync_inputs 前 Vtop.default_clock 字段被置 1 (posedge 模拟)
- **THEN** `eval_fn_()` 调用后 Vtop.default_clock 字段被置 0（falling edge）

#### Scenario: clock_node_id_ 未命中时跳过
- **WHEN** context 没有 type_clock 节点
- **THEN** clock_node_id_ == UINT32_MAX，eval_sequential 仅 sync + eval 不翻转 default_clock

### Requirement: SHA-1 cache hit 不触发 verilator 子进程 (Phase 3.5 契约)
第二次 `initialize()` 对同一 verilog source **MUST** 检测 `~/.cache/cpphdl/verilator/<key>/Vtop` 存在并跳过 `verilator --cc --build` 调用。

#### Scenario: cache miss 触发编译
- **WHEN** cache_path_for_key() 返回路径不存在
- **THEN** invoke_verilator() 被调用，`verilator --cc --build` 子进程 spawn
- **THEN** 编译成功后 Vtop 拷贝到 cache 路径

#### Scenario: cache hit 跳过编译 (< 100ms)
- **WHEN** 同一 verilog source 第二次 initialize()，cache 路径存在
- **THEN** invoke_verilator() **NOT** 被调用
- **THEN** dlopen_top() 直接 load cached Vtop
- **THEN** 整个 initialize() 路径耗时 < 100ms

### Requirement: VCD trace dump 写出有效文件 (Phase 3.6 契约)
`enable_vcd(true)` 后每个 cycle 写入 `obj_dir/sim.vcd`，文件 **MUST** 包含所有 type_input/type_output/type_reg 信号的 VCD 头 + 时间戳 + 值。

#### Scenario: VCD header 写入
- **WHEN** `enable_vcd(true)` + `initialize()` + 0 cycle
- **THEN** `sim.vcd` 文件存在，包含 `$timescale`, `$scope module top`, `$var` declarations

#### Scenario: cycle 信号 dump
- **WHEN** 跑 100 cycle
- **THEN** `sim.vcd` 文件包含 100 个 `#<time>` 时间戳 + 信号值行
- **THEN** 文件 size > 1KB

### Requirement: e2e 端到端与解释器结果一致 (Phase 7.5 契约)
Verilator sim 后端跑 `samples/counter.cpp` 与 ChipForge `tests/cpu/test_cpu_chmem_vendored_elf.cpp` **MUST** 产生与 CppHDL 解释器**逻辑等价**的结果（counter_value 相等、tohost=1、cycle 数 ±10%）。

#### Scenario: counter sim 50 cycle 一致
- **WHEN** `samples/counter.cpp` 跑 Verilator sim 50 cycle (150 evals)
- **THEN** counter_value == 50（与 CppHDL 解释器跑 50 cycle 的 counter_value 一致）

#### Scenario: ChipForge 5-stage CPU vendored ELF tohost=1
- **WHEN** `rv32ui-p-add` ELF 跑 Verilator sim 50 cycle
- **THEN** DMem[0x1000] (tohost offset) == 1
- **THEN** cycle 数与 CppHDL sim 跑同 ELF 的 cycle 数相差 ≤ 10%

