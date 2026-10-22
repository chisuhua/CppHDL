# Capability: verilator-e2e-coverage

## MODIFIED Requirements

### Requirement: 代表性 Memory 设计在 Verilator 仿真下行为契约

`tests/test_verilator_memory_e2e.cpp` **MUST** 覆盖至少 2 个 memory/burst 类场景：sync FIFO（`ch_mem` + rd/wr pointer + full/empty）和 AXI4-Full Slave（5-channel handshake + burst=4 笔 write/read）。每个场景 **MUST** 同时跑 `InterpreterBackend` 与 `VerilatorBackend`，对比 `data_map_` 终值；burst 写读场景验证 `rdata` 序列与写入顺序一致。

#### Scenario: Sync FIFO trace parity 16 cycle

- **WHEN** fixture 为 `SyncFifoFixture`（`ch_mem<N>` + rd/wr pointer + `full`/`empty` 标志绑独立 `ch_out`），写入 1 笔、读出 1 笔
- **AND** interpreter + verilator 各跑 16 cycle
- **THEN** 16 cycle 内 `data_map_` 中 `full`/`empty` 状态序列在两后端必须一致
- **AND** 读出 data 终值在两后端必须等于写入 data

> ⚠️ **归因订正（2026-10-22，`chlib-stream-semantics-2026-10`）**：原文本称该 fixture
> 「简化自 `examples/spinalhdl-ported/fifo/stream_fifo_example.cpp::FiFo`」——
> **该归因不准确**：fixture **已实现**空/满门控
> （`will_write = wr_en & !full_reg` / `will_read = rd_en & !empty_reg`，双指针受门），
> 而它所「简化自」的 example **没有**该门控 ⇒ example 的指针在满/空时照常前进 ⇒
> cycle 5 起进入**假满**并永久卡死。
> ⇒ fixture 与 example **不是**同一语义；example 只能作为「0-cycle 数据通路」的参照，
> **MUST NOT** 被当作「正确实现」的参照。

#### Scenario: AXI4-Full burst write+read 4 笔

- **WHEN** fixture 为 `AxiFullFixture`（简化自 `examples/axi4/axi4_full_example.cpp`，5-channel handshake + burst=4 笔），先后发 1 次 burst write 4 笔（`0x11, 0x22, 0x33, 0x44`）+ 1 次 burst read 4 笔
- **AND** interpreter + verilator 各跑到 burst read 完成
- **THEN** `rdata` 序列（4 笔）在两后端必须 == `[0x11, 0x22, 0x33, 0x44]`（按写入顺序）

#### Scenario: chlib stream FIFO 的 FWFT 逐拍 parity

- **WHEN** fixture 为 `chlib::stream_fifo`（`include/chlib/stream.h`），序列覆盖
  push / pop / 空 FIFO / 满 FIFO
- **AND** 用 `DUAL_BACKEND_TEST_PAIR` 跑 interpreter + verilator
- **THEN** 两后端的 `payload` 与 `valid` **逐拍**序列必须完全一致

> ⚠️ FWFT 的 0-cycle 语义是历史上漂移过的地方
> （`sim-mem-read-timing-align-2026-10` 才发现 interpreter 与 Verilog 差 1 拍）
> ⇒ 只跑 interpreter 的断言 MUST NOT 被当作该契约已被守住的证据。
> 详见 capability `chlib-stream-semantics` 的「`chlib::stream_fifo` 的行为 MUST 有
> 双后端 parity 覆盖」。
