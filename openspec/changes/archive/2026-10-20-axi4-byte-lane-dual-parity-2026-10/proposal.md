# Change: axi4-byte-lane-dual-parity-2026-10 — 字节道写语义补双后端覆盖

## Why

`axi4-peripheral-deferred-2026-10`（archive `2026-10-20-axi4-peripheral-deferred-2026-10`）
落地了 AXI4-Lite 的字节道（`wstrb`）写语义，但当时的验证只覆盖了 **interpreter 后端**：

- `tests/test_axi_byte_lane.cpp` 用 `add_catch_test` 注册 → 只跑 interpreter
- 既有 dual-backend behavior 测试（`test_axi_{gpio,timer,uart}_behavior.cpp`）虽然
  会经由 `byte_lane_apply<32>` 走一遍 verilator，但它们的 `wstrb` 恒为 `0xF`
  （整字写）→ **部分字节道的掩码路径与窄寄存器截断路径从未在 verilator 下跑过**

而这两条路径恰恰是 codegen 风险最高的地方：

- 窄寄存器截断：`ch_uint<8>`（i2c `TX_DATA`）/ `ch_uint<16>`（i2c `PRESCALE`、
  spi `BAUD`）的 `byte_lane_apply<W>` 结果要再截断到目标宽度
  —— Verilog codegen 的宽度推导与 interpreter 不同
- 字节道掩码展开：`wstrb` 逐字节展开成 32-bit 掩码的 `or` 链

该 Requirement 落在 `dual-backend-test-coverage` capability 下（名实要求双后端），
但缺少双后端断言 → spec 与验证手段不匹配。

## What Changes

- `tests/test_axi_byte_lane.cpp` 改为 **双后端**：
  - 7 个 `TEST_CASE` → 7 个 `DUAL_BACKEND_TEST_PAIR`（interpreter + verilator 共 14 个）
  - `tests/CMakeLists.txt`：`add_catch_test` → `add_dual_backend_test`（继承
    `verilator_e2e` 标签 + `RESOURCE_LOCK verilator_backend` + `_vl_path_env`）
- 语义不变：6 个 peripheral 的字节道契约（部分字节写 / 只读寄存器 DECERR /
  亚 32 位寄存器有效道 / `wstrb=0` 空写）全部保留，仅补上 verilator 半边
- spec delta：在「AXI4 peripheral MUST 支持字节写」Requirement 上补
  **双后端一致性** 与 **窄寄存器截断** 两个 Scenario

### 非目标

- ❌ 改任何 peripheral 的写路径语义（已由 `axi4-peripheral-deferred-2026-10` 落地）
- ❌ 扩到 `include/axi4/axi4_lite_slave.h` / `axi4_full.h`（这两处仍整字覆盖写，
  属独立的一致性缺口，另开 change）
- ❌ 改 `wstrb` 语义规则本身

## Capabilities

### MODIFIED Requirements

见 `specs/dual-backend-test-coverage/spec.md`。
