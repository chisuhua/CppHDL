## MODIFIED Requirements

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
- **THEN** **MAY** 使用 `DUAL_BACKEND_TEST_PAIR`（Async variant 的 0-cycle `aread` 已与生成的 Verilog `assign proxy = mem[addr];` 对齐 ⇒ 两后端 MUST 逐周期一致；与上方「cache 双后端 MUST 按 Sync/Async 分类」中「Async dual-run 可以走 parity、禁令解除」一致）
- **AND** Async 的 dual-run **MUST** 覆盖 `miss` 直通（`axi_data` / `axi_rdata`）、`ready` 在 miss 时跟随 `axi_ready`、以及**填充后**的 hit（⚠️ 填充在**同一 `tick()` 的收尾组合求值**里即可见 ⇒ 「填充拍之后」`miss` / `axi_valid` **不可观测** ⇒ 要观测 miss MUST 用「冷地址 + `axi_resp_valid=0`」的拍）
- **AND** 若某个 Async dual-run **实测不绿**，则该用例 MUST 回退为 golden-value 路径并**记录**该分歧 —— 禁令只对**已被实测证伪的前提**失效，不是对未实测的变体无条件放行
