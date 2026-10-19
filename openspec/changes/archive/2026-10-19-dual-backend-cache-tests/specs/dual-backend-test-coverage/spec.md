# Capability: dual-backend-test-coverage

## ADDED Requirements

### Requirement: cache Sync runtime test SHALL 走 DUAL_BACKEND_TEST_PAIR macro

`tests/chlib/test_cache_sync.cpp` MUST 使用 `tests/dual_backend_fixture.h::DUAL_BACKEND_TEST_PAIR` macro，让 `ICacheSync` / `DCacheSync` 每个 TEST_CASE 都跑 interpreter + verilator 双后端。**不允许**用裸 `TEST_CASE` 单独跑 interpreter；**不允许**手写 dual half（避免与 macro 漂移）。

#### Scenario: macro invocation 计数

- **WHEN** 在 `tests/chlib/test_cache_sync.cpp` grep `DUAL_BACKEND_TEST_PAIR`
- **THEN** 必须 ≥ 9 个 macro invocation（5 ICacheSync + 4 DCacheSync）

#### Scenario: register 接口约束

- **WHEN** 测试用 `sim.set_input_value(io().port, value)` 和 `sim.get_port_value(io().port)`
- **THEN** 不允许直接访问 `sim.data_map()[node_id]`（与 macro R3 per-cycle REQUIRE 契约一致）；不允许用 `node_id` 跳过 ch_out 接口

#### Scenario: spec R6 排除清单遵守

- **WHEN** runtime test 涉及 `ICacheAsync` 或 `DCacheAsync`
- **THEN** MUST NOT 使用 `DUAL_BACKEND_TEST_PAIR`（Async variant 是 0-cycle aread，命中 R6 #3，verilator 0-cycle vs simulator posedge 1-cycle skew）；runtime test 仅适用 Sync variant（1-cycle sread，RV32I L1 默认）

### Requirement: cache Sync runtime test MUST 用 Sync variant 而非 stub

`tests/chlib/test_cache_sync.cpp` SHALL 实例化 `chlib::ICacheSync` 和 `chlib::DCacheSync`（来自 `include/cpu/cache/i_cache_sync.h` 与 `d_cache_sync.h`，ADR-041 双变体之一）。**不允许**实例化 `cpu/cache/i_cache_stub.h` 或 `d_cache_stub.h`（stub 是 ADR-037 待退役对象，2027-04-17 deadline）。允许 `#pragma GCC diagnostic ignored "-Wdeprecated-declarations"` 包裹（与 `tests/chlib/test_cache_pipeline_integration.cpp` 一致），但不进 stub 头文件。

#### Scenario: include 头文件约束

- **WHEN** runtime test 用 `#include "cpu/cache/i_cache_sync.h"` 和 `#include "cpu/cache/d_cache_sync.h"`
- **THEN** 不允许 `#include "cpu/cache/i_cache_stub.h"` 或 `#include "cpu/cache/d_cache_stub.h"`

#### Scenario: instance 类型约束

- **WHEN** runtime test 用 `ch::ch_device<ICacheSync>` 和 `ch::ch_device<DCacheSync>`
- **THEN** 不允许 `ch::ch_device<ICache>` (stub 别名) 或 `ch::ch_device<DCache>` (stub 别名)
