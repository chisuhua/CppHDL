# Change: dual-backend-cache-tests — 新建 Sync variant runtime test + dual 化

## Why

**现状**（截至 commit `5f634f2`，main HEAD）：

- ADR-041 已实装 `include/cpu/cache/{i,d}_cache_{sync,async}.h`（commit `91a0833` cache-complete-rewrite via ch_mem）
- ADR-037 计划 `cpu/cache/{i,d}_cache_stub.h` **2027-04-17** 物理退役
- **但是**：现有的 cache runtime test 全部基于 stub（`tests/chlib/test_cache_pipeline_integration.cpp` 等），或基于已废弃 API（`tests/chlib/test_cache_complete.cpp` 12 处 aread 命中 spec `dual-backend-test-coverage` R6 #3 排除清单）
- `tests/chlib/test_phase4_integration.cpp` 全部 SKIP-only stub，`tests/chlib/test_cache_simple.cpp` 是裸 context（R6 #2 排除）

**真实代价**：

| 历史痛点 | 暴露点 |
|---|---|
| ADR-041 Sync/Async 双变体实装后零 runtime coverage | 没有 test 验证 `ICacheSync` / `DCacheSync` 的 hit path / miss-fill / LRU 更新 / Write-Through 在 interpreter 跑通 |
| cache codegen 潜在 drift | Sync variant 1-cycle sread 对接 RV32I imem (`pc_lag` alignment)，verilator 0-cycle vs 1-cycle 任何 drift 阻断 riscv-mini 真集成 |
| `verilator-parity-all-tests` Phase 2 因无可 dual 化对象 hang | cache test 集当前不存在符合 R6 排除清单的 dual 候选，**新建** runtime test 是唯一可行路径 |

**预期收益**：

1. **覆盖率补齐**：`ICacheSync` / `DCacheSync` 4 个核心行为路径（hit / miss-fill / refilled-next-cycle / write-through）全部有 runtime test + dual backend 验证
2. **codegen 真集成信心**：cache sync codegen 走通 verilator 后，riscv-mini 真集成阶段不再有"cache codegen bug 首次暴露"风险
3. **架构清晰**：明确 Sync variant 是 RV32I L1 默认（per `include/cpu/AGENTS.md` CONVENTIONS §"Cache variant selection (ADR-041)"）
4. **Phase 5 cache-stub-removal 前置**：没有 Sync runtime test 就 stub 退役 = 退役无回归覆盖

**预期成本**：

- 一次性 CI 墙钟增量 ~30-90 sec/PR（每个新 dual test cold verilator compile 约 5-15 sec，warm cache <1 sec）
- 改造工作量 ~3-5 人天（单 builder）
- 维护模式：未来 Async variant 或新 cache feature 都需 follow "dual-backend 可行性"判定

**Out of Scope**（明确划线）：

- ❌ Async variant dual 化（`ICacheAsync` / `DCacheAsync` 是 0-cycle aread，命中 R6 #3 排除；如果未来要做，走 `DUAL_BACKEND_GOLDEN_TEST` macro，`verilator-parity-all-tests` design.md §2.2 计划但未实装）
- ❌ `DUAL_BACKEND_GOLDEN_TEST` macro 实装（属 change `verilator-parity-all-tests` Phase 2 重启，scope 独立）
- ❌ cache stub 物理退役（属 change `cache-stub-physical-removal`，ADR-037 deadline 2027-04-17）
- ❌ riscv-mini wrapper (`i_cache_prefetch.h` / `d_cache_write_buffer.h`) SIGSEGV 修复（属 backlog 阻塞 Phase 4）
- ❌ 把 `tests/chlib/test_cache_complete.cpp` 的 12 处 aread 改成 sread（属 ADR-039 兼容层重设计，超出 scope）

## What Changes

### 1. 新建 `tests/chlib/test_cache_sync.cpp`

`ICacheSync` / `DCacheSync` runtime test，通过 `DUAL_BACKEND_TEST_PAIR` macro 跑双后端：

| TEST_CASE | 验证场景 |
|---|---|
| `ICacheSync - Cold miss → AXI request` | cycle 1: req=1, addr=X, ready=AXI ready, data=axi_data, miss=1, axi_valid=1 |
| `ICacheSync - Cold miss → AXI resp → fill` | cycle 2: axi_resp_valid=1 → cache 内部填 way0 (valid=1) |
| `ICacheSync - Refilled, same addr → hit` | cycle 3: req=1, same addr → ready=1, miss=0, data=AXI 写入值（不是 axi_data，而是已 fill 进 cache 的值） |
| `ICacheSync - Hit updates LRU` | LRU update 是内部行为 → 仅通过 refilled same addr 验证 |
| `ICacheSync - Different addr still miss` | cycle 4: req=1, new addr → ready=axi_ready, miss=1 |
| `DCacheSync - Cold miss read → AXI request` | 同步 cache 读 |
| `DCacheSync - Cold miss read → AXI resp → fill` | 同步 cache 读 fill |
| `DCacheSync - Hit read returns cached` | 命中读返回 cached data |
| `DCacheSync - Hit write write-through` | cycle N: req=1, we=1, addr=hit_addr → axi_we=1, axi_wdata=wdata |

### 2. CMakeLists.txt 注册

通过 `tests/CMakeLists.txt::add_dual_backend_test()` 函数注册，2 个 test target：
- `test_cache_sync`
- TIMEOUT 默认 60s（Sync variant codegen 较小，无需长 timeout）

### 3. Reference impl 同步

新建 test 在 `tests/chlib/` 目录下，与现有 `test_phase4_integration.cpp` / `test_cache_pipeline_integration.cpp` 并列；不修这些文件（避免 scope creep）。

### 4. AGENTS.md / spec 同步

- `openspec/specs/dual-backend-test-coverage/spec.md` 不改（macro 接口不变）
- `include/cpu/cache/AGENTS.md` (如果有) 加一行提到新增 test fixture

## Acceptance

#### Scenario: 双后端 PASS

- **WHEN** `cd build && cmake --build . --target test_cache_sync && ctest -L verilator_e2e -R test_cache_sync`
- **THEN** test_cache_sync 通过；interpreter half + verilator half 都 PASS

#### Scenario: cache Sync 特性覆盖

- **WHEN** developer 阅读 `tests/chlib/test_cache_sync.cpp`
- **THEN** 至少 9 个 TEST_CASE（5 ICacheSync + 4 DCacheSync）覆盖 hit / miss / fill / write-through

#### Scenario: 编译无错

- **WHEN** `cmake --build build -j$(nproc) 2>&1 | grep -E "error|Error"`
- **THEN** 0 行错误；warnings 在已接受 baseline 内

#### Scenario: 资源锁同步

- **WHEN** `ctest -N -L verilator_e2e -R test_cache_sync`
- **THEN** 输出包含 RESOURCE_LOCK "verilator_backend"；与现有 18 个 verilator_e2e 测试共享同一锁

## Capabilities

### ADDED Requirements

#### Requirement: cache Sync runtime test MUST 走 DUAL_BACKEND_TEST_PAIR macro

`tests/chlib/test_cache_sync.cpp` 必须使用 `tests/dual_backend_fixture.h::DUAL_BACKEND_TEST_PAIR` macro，让 `ICacheSync` / `DCacheSync` 每个 TEST_CASE 都跑 interpreter + verilator 双后端。不允许用裸 `TEST_CASE` 单独跑 interpreter；不允许手写 dual half（避免与 macro 漂移）。

#### Scenario: macro 9 个新 TEST_CASE_PAIR

- **WHEN** 测试文件 grep `DUAL_BACKEND_TEST_PAIR` 计数
- **THEN** 至少 9 个 macro invocation（5 ICacheSync + 4 DCacheSync）

#### Scenario: register 接口固定

- **WHEN** 测试用 `sim.set_input_value(io().addr, ...)` + `sim.get_port_value(io().ready)`
- **THEN** 不允许直接访问 `data_map_[node_id]`（与 macro R3 per-cycle REQUIRE 契约一致）

## Tasks

(后续 builder 阶段填)
