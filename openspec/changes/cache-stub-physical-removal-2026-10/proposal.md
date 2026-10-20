# Change: cache-stub-physical-removal-2026-10 — 执行 ADR-037 cache stub 物理删除（提前到 2026-10）

## Why

**现状**（截至 commit `7d69f56` 2026-10-20）：

`include/cpu/cache/{i,d}_cache_stub.h` 是 ADR-037 标记的 pass-through smoke fixture：

```
ready := req, miss := false, data := axi_data
```

按 ADR-037 6 个月 deadline 是 **2027-04-17**，但以下三个 blocker 已解除，**可提前执行**：

1. **dual-backend-test-coverage Phase 2 已落地**（commit `aed33be` archive `2026-10-19-dual-backend-cache-tests/`）：
   - `tests/chlib/test_cache_sync.cpp` 用 `ICacheSync` / `DCacheSync` 完整 dual 覆盖（hit / miss / refill / write-through / read-back 等 9 dual pairs × 2 halves）
   - `ICacheSync` / `DCacheSync` 是 ADR-041 1-cycle sread 路径，性能等同 Stop AD
2. **fix-codegen-ch-reg-no-next-hold-state**（commit `7d69f56` 2026-10-20）保证 `ICacheComplete` / `DCacheComplete` 的 init value 在 VerilatorBackend 应用
3. **fix-riscv-mini-cache-prefetch-sigsegv**（commit `b987bb7` 2026-10-19）保证 riscv-mini wrapper instantiate 成功

**真实代价**（不退化 vs 计算回报）：

| 历史痛点 | 暴露点 |
|---|---|
| `test_cache_pipeline_integration.cpp` 仅结构测 / SKIP 占位 | 5 TEST_CASE 3 SKIP，2 TRUE 验证"stub pass-through 语义" — 与 real ICacheComplete 行为相反 |
| `test_cache_simple.cpp` / `test_phase4_integration.cpp` 仅 smoke instantiate | 2 / 7 TEST_CASE 全 `REQUIRE(true)` — 无 runtime 价值 |
| `*_stub.h` 持续被 deprecated 标注 | root AGENTS.md §KNOWN LIMITATIONS 长期 debt 占位 |
| Stub 用户误用：以为 stub = ICacheComplete | 缺明确术语区分 (`ready := req, miss := false` ≠ `ICacheComplete` 真 Hit/Miss) |

**预期收益**：

1. **cache-complete 重构闭环**：stub 物理删除，**`include/cpu/cache/` 只剩 `{i,d}_cache_{sync,async,complete}.h` 4 个真实现**
2. **root AGENTS.md §KNOWN LIMITATIONS 减 1 项**：cache stub 段删除
3. **3 个 test 文件 pragma 升级**：不再 deprecated-suppress
4. **CTEST_TOTAL 可能 -3**：删除 2 个 SKIP-only test + 1 个 stub-specific test

**预期成本**：

- 修改 6 文件（`include/cpu/cache/*.h` + 3 测试文件 + 2 docs）
- 改造工作量 ~1.5 人天（per ADR-037 §4 3 atomic commits）
- ~3 个 PR (3 commit 各自 buildable 独立)

**预期 ADR-037 §4 3 个 atomic commit**：

1. **Add `BYPASS` mode to `ICacheConfig` / `DCacheConfig`**：
   - `*_complete.h` 加 `bool bypass_mode` 字段
   - bypass 时 emit stub 的 pass-through semantics: `ready := req, miss := false, data := axi_data`
   - 新 factory helper `make_passthrough_icache()` / `make_passthrough_dcache()`
   - 新 `tests/chlib/test_cache_bypass_mode.cpp` smoke test 验证 bypass 时 `ready==req, miss==0, data==axi_data`
2. **Rewrite `test_cache_pipeline_integration.cpp`**：
   - L45-69 + L74-100 改用 `ICacheComplete` with `BYPASS=true`
   - 删 `#pragma GCC diagnostic ignored "-Wdeprecated-declarations"` wrapper（不再引用 stub）
   - 2 SKIP TEST_CASE 保持 SKIP（等 `Rv32iPipelineWithCache` 重建）
3. **`git rm` stub files + 2 SKIP-only tests**：
   - `include/cpu/cache/{i,d}_cache_stub.h` 删除
   - `tests/chlib/test_cache_simple.cpp` 删除（2 个 `REQUIRE(true)` smoke）
   - `tests/chlib/test_phase4_integration.cpp` 删除（7 个 `REQUIRE(true)` smoke）
   - `tests/CMakeLists.txt` 删 `add_catch_test()` 行
   - `include/cpu/AGENTS.md` STRUCTURE 表更新
   - root `AGENTS.md` §KNOWN LIMITATIONS 删 cache stubs 段

**Out of Scope**（明确划线）：

- ❌ `ICacheComplete` / `DCacheComplete` Hit/Miss 行为修改（属独立 change）
- ❌ ADR-039 ch_mem 重写（已完成）
- ❌ ADR-041 Sync/Async variant split（已完成）
- ❌ ADR-035 VerilatorBackend QData accessor（与 stub 无关）
- ❌ riscv-mini wrapper instantiate 修复（已 commit `b987bb7`）
- ❌ `examples/riscv-mini/src/i_cache_prefetch.h` 删 stub 引用（应在更前面 commit `b987bb7` 中已完成）

## What Changes

### 1. Add BYPASS mode to `*_complete.h`

`include/cpu/cache/i_cache_complete.h`：
```cpp
struct ICacheConfig {
    unsigned cache_size = 1024;
    unsigned ways = 2;
    bool bypass_mode = false;  // new: emit stub pass-through semantics
};
```

`include/cpu/cache/i_cache_complete.h::describe()` 顶层增加：
```cpp
if (s.config.bypass_mode) {
    // Pass-through stub semantics for the optional port in / out
    io().ready = io().req;
    io().miss = ch_bool(false);
    io().data = io().axi_data;
    // skip Hit/Miss state machine entirely
    return;
}
// ... 原有 Hit/Miss 逻辑
```

类似 `d_cache_complete.h`。

### 2. factory helpers

`include/cpu/cache/i_cache_complete.h`：
```cpp
inline std::unique_ptr<ch::Component> make_passthrough_icache(
    ch::Component *parent = nullptr, const std::string &name = "passthrough_icache") {
    // returns ICacheComplete with ICacheConfig{bypass_mode=true}
}
```

### 3. New test `tests/chlib/test_cache_bypass_mode.cpp`

```cpp
TEST_CASE("ICacheComplete bypass mode emits stub semantics") {
    // ch_device<ICacheComplete> with bypass_mode=true
    // req=1, axi_data=0xCAFE
    // REQUIRE(ready==1, miss==0, data==0xCAFE)
}
// 类似的 DCache test
```

### 4. Rewrite `test_cache_pipeline_integration.cpp`

3 active TEST_CASE (line 45-69, 74-100) 改用 `ICacheComplete` with bypass_mode=true
- 删除 `#pragma GCC diagnostic` wrapper
- 2 SKIP TEST_CASE 保留（等 `Rv32iPipelineWithCache` 重建）

### 5. Delete `*_stub.h` + 2 test files

```
git rm include/cpu/cache/i_cache_stub.h
git rm include/cpu/cache/d_cache_stub.h
git rm tests/chlib/test_cache_simple.cpp
git rm tests/chlib/test_phase4_integration.cpp
```

`tests/CMakeLists.txt` 删对应 `add_catch_test()` 行。

### 6. Doc sync

- `include/cpu/AGENTS.md` STRUCTURE 表删除 `i_cache_stub.h` / `d_cache_stub.h` 行
- root `AGENTS.md` §KNOWN LIMITATIONS 删除 cache stubs 段（确认 ADR-037 已 stub 被删除）

## Acceptance

#### Scenario: ctest 全量不退化（中间步骤 1, 2）

- **WHEN** `ctest -LE perf --output-on-failure`
- **THEN** 全 PASS（基线 158/158 PASS 在中间步骤 1-2 不退化）

#### Scenario: 最终 ctest 全量 + stub 测试消失（步骤 3）

- **WHEN** `ctest -LE perf --output-on-failure` after step 3
- **THEN** 155/155 PASS（158 - 3 deleted = 155）

#### Scenario: compile 无错

- **WHEN** `cmake --build build -j$(nproc) 2>&1 | grep -E "error|Error"`
- **THEN** 0 行错误

#### Scenario: stub 头文件不存在

- **WHEN** `git ls-files include/cpu/cache/*_stub.h`
- **THEN** 空（0 stubs）

#### Scenario: test_cache_simple.cpp / test_phase4_integration.cpp 不存在

- **WHEN** `git ls-files tests/chlib/test_cache_simple.cpp tests/chlib/test_phase4_integration.cpp`
- **THEN** 空（0 tests）

#### Scenario: 不破既有 spec 锁

- **WHEN** reviewer 检查本 change 的 diff
- **THEN** MUST NOT 改 `include/cpu/cache/{i,d}_cache_{sync,async}.h`（已 archive cap）
- **AND** MUST NOT 改 `examples/riscv-mini/src/i_cache_prefetch.h` / `d_cache_write_buffer.h`（riscv-mini wrapper，依赖 commit `b987bb7` 修复后已正确实例化 ICacheSync / DCacheSync）
- **AND** MUST NOT 改 `tests/chlib/test_cache_sync.cpp`（Phase 2 已 archive）
- **AND** MUST NOT 改 `tests/test_cache_complete_bypass.cpp` 等新建 bypass test 的 4 个测试

#### Scenario: openspec validate 通过

- **WHEN** `openspec validate cache-stub-physical-removal-2026-10 --strict`
- **THEN** valid

## Capabilities

### ADDED Requirements

#### Requirement: cache stub 物理删除 MUST 在 6 个月内完成 per ADR-037

`include/cpu/cache/{i,d}_cache_stub.h` 在 ADR-037 6 个月 deadline 内 MUST 物理删除，并通过 `ICacheConfig::bypass_mode` 让 stub 行为**仿真**在 stub 上而非**取代** stub。

#### Scenario: stub 头文件物理不存在

- **WHEN** reviewer `git ls-files include/cpu/cache/*_stub.h`
- **THEN** 空（0 stubs）

#### Scenario: bypass_mode 验证 stub 行为

- **WHEN** `ICacheComplete` with `bypass_mode=true`
- **THEN** MUST emit `ready := req, miss := false, data := axi_data`（与原 stub 完全一致）
- **AND** Hit/Miss state machine MUST NOT 触发

#### Scenario: 6 个月 calendar deadline

- **WHEN** `date +%Y-%m-%d >= 2027-04-17`（per ADR-037 §3）
- **THEN** stub MUST 已删除
- **AND** CI MUST 已 commit this change 至少 1 天前

#### Scenario: doc sync

- **WHEN** reviewer 阅读 root `AGENTS.md` §KNOWN LIMITATIONS
- **THEN** MUST NOT 含 "Cache stubs i_cache_stub.h / d_cache_stub.h are pass-through" 段
- **AND** `include/cpu/AGENTS.md` STRUCTURE 表 MUST NOT 含 `i_cache_stub.h` / `d_cache_stub.h` 行