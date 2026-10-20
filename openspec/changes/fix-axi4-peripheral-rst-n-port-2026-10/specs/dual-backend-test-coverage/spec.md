# Capability: dual-backend-test-coverage

## Purpose

定义 CppHDL 单元测试统一跑 InterpreterBackend 与 VerilatorBackend 的契约（详见主 spec `openspec/specs/dual-backend-test-coverage/spec.md`）。

## ADDED Requirements

### Requirement: AXI4 peripheral MUST 含 rst_n + busy self-clear for dual 化

任何 `include/axi4/peripherals/*.h` 中的 `AxiLite<Peripheral>` MUST 含 `ch_in<ch_bool> rst_n` port + `busy` MUST self-clear 在 `any_ready` 时。否则 `DUAL_BACKEND_TEST_PAIR` macro 在第二次 transaction 后 busy 永久锁导致 fixture 卡死。

#### Scenario: 4 个 peripheral 包含 rst_n port

- **WHEN** reviewer 阅读 `axi_uart.h` / `axi_timer.h` / `axi_gpio.h` / `axi_dma.h` `__io` section
- **THEN** MUST 含 `ch_in<ch_bool> rst_n;`

#### Scenario: 状态 registers rst_n reset pattern

- **WHEN** reviewer 阅读 `busy->next` 等 state register update lines
- **THEN** MUST 包含 `select(!io().rst_n, false/init_val, ...)` reset pattern

#### Scenario: IO port `&&`/`||` 反模式禁止

- **WHEN** reviewer grep reading `axi_timer.h::describe()`
- **THEN** MUST NOT 含 `io().awvalid && io().awready` / `io().wvalid && io().wready` / `io().arvalid && io().arready` / `io().bready || io().rready` 等 IO port `&&`/`||` overload (per axi4/AGENTS.md ANTI-PATTERN)
- **AND** MUST 用 `select()` 替代

#### Scenario: dual 行为 parity 可跑

- **WHEN** peripheral design 含 rst_n + busy self-clear
- **THEN** `DUAL_BACKEND_TEST_PAIR` macro 可在 lambda body 内跑 ≥2 笔 transactions
- **AND** MUST NOT 出现 busy 永久锁导致 fixture 卡死