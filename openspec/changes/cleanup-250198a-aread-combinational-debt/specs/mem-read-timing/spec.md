# Capability: mem-read-timing

## Purpose

定义内存读端口（`aread` / `sread`）在生成 Verilog 与 Simulator 中的时序语义，并明确两者间的已知 drift。该能力保证下游用户对生成的 Verilog 与 Simulator 行为有可预期、可追溯的契约。

## ADDED Requirements

### Requirement: aread emits combinational assign in Verilog

The `ch_mem::aread` (async_read) port SHALL emit `assign proxy = mem[addr];`（组合读，0-cycle 延迟）作为生成 Verilog 中的输出。

#### Scenario: aread Verilog codegen

- **WHEN** 用户调用 `mem.aread(addr, name)` 且端口有数据代理输出
- **THEN** 生成的 Verilog SHALL 包含 `assign <proxy_name> = <mem_name>[<addr_name>]; // Comb read <proxy_name>`

### Requirement: sread emits always_ff in Verilog

The `ch_mem::sread` (sync_read) port SHALL emit `always_ff @(posedge default_clock or posedge default_reset)`（寄存读，1-cycle 延迟）作为生成 Verilog 中的输出。

#### Scenario: sread Verilog codegen

- **WHEN** 用户调用 `mem.sread(addr, enable, name)` 且端口有数据代理输出
- **THEN** 生成的 Verilog SHALL 包含 `always_ff ... <proxy_name> <= <mem_name>[<addr_name>];` 块，且包含复位分支清除代理

### Requirement: Simulator classifies mem read ports as sequential

The Simulator SHALL 将 `type_mem_read_port`（包含 sync 与 async 两类）放入 `sequential_instr_list_`，并在时钟高电平阶段的 `eval_sequential()` 中求值（`src/simulator_init.cpp:116-121`）。

#### Scenario: aread 与 sread 在 Simulator 中行为

- **WHEN** 在 tick 前设置 `addr_async` / `addr_sync` 输入，然后调用 `simulator.tick()`
- **THEN** 两类端口的代理在 tick 内同步更新；tick 末尾的 `eval_combinational()`（`src/simulator.cpp:328`）将代理传播到输出端口
- **THEN** 在 Simulator 中 `get_port_value(rdata_async)` 与 `get_port_value(rdata_sync)` 在 tick 调用后均可见新值（观察端行为相同）

### Requirement: known simulator-vs-Verilog drift is documented

The following drift between Simulator 与生成 Verilog SHALL 在 `docs/simulation/ARCHITECTURE.md` 的 "Memory read port timing" 小节和 root `AGENTS.md` CONVENTIONS 中明确记录：

#### Scenario: Register-driven read address skew

- **WHEN** 读地址由寄存器驱动（FIFO `raddr` 模式）
- **THEN** Verilog 中 `assign` 立刻反映 `mem[new_addr]`（0-cycle）
- **THEN** Simulator 中代理在下一个 posedge 才更新 `mem[addr_at_posedge]`（有效 1-cycle 滞后）

#### Scenario: Same-cycle write+read race

- **WHEN** `we=1` 在同一 tick 写 `mem[X]` 且 `aread` 同时读 `mem[X]`
- **THEN** Simulator aread 返回写前值
- **THEN** Verilog `assign` 在 NBA 更新后返回写后值
- **THEN** 该差异 SHALL NOT 触发任何回归 fail（设计意图：由下游 register 提供 1-cycle 依赖，如 ChipForge CPU `reg_file <<=`）