# Capability: cpu-cache-subsystem

## Purpose

定义 CppHDL CPU cache subsystem 的可实例化实现契约：基于 `ch_mem` 原语构建的 4KB 2-way set-associative L1 cache（`ICacheComplete` + `DCacheComplete`），包括 Hit/Miss 检测、LRU 替换、AXI 填充、写策略。该能力让 RV32I 顶层集成（`riscv-mini` SOC）能以可预测、可追溯、可测量的方式实例化 cache 子模块。

In English: Defines the contract of CppHDL's CPU cache subsystem — `ICacheComplete` and `DCacheComplete` classes built on `ch_mem` primitives — supporting Hit/Miss detection, LRU replacement, AXI fill, and write strategies. Enables RV32I top-level integration to instantiate cache submodules with predictable, traceable, measurable behavior.

## Requirements

### Requirement: SRAM storage uses ch_mem primitives

The `ICacheComplete` / `DCacheComplete` cache classes SHALL use `ch_mem<T, N>` for tag / data / valid / LRU storage, where N equals `NUM_SETS = 128`. Static C-style `ch_reg<T>[N]` arrays with runtime indexing SHALL NOT be used.

#### Scenario: ch_mem instantiation pattern

- **WHEN** the cache class declares its tag RAM
- **THEN** it SHALL use `ch_mem<ch_uint<TAG_BITS>, NUM_SETS> tag_mems[ASSOCIATIVITY];` (one `ch_mem` per way)
- **AND** data RAM SHALL use `ch_mem<ch_uint<DATA_WIDTH>, NUM_SETS> data_mems[ASSOCIATIVITY];`
- **AND** valid bits SHALL use `ch_mem<ch_bool, NUM_SETS> valid_mems[ASSOCIATIVITY];`
- **AND** LRU bits SHALL use `ch_mem<ch_bool, NUM_SETS> lru_mem;`

#### Scenario: ch_mem usage compared to dead-on-arrival predecessor

- **WHEN** the previous design (`include/cpu/cache/{i,d}_cache_complete.h` pre-rewrite) used `ch_reg<ch_uint<...>> tag_sram[NUM_SETS][ASSOCIATIVITY]` with `ch_uint<7>` runtime index
- **THEN** such pattern SHALL NOT be present in the post-rewrite code
- **AND** the post-rewrite code SHALL instead expose `tag_mems[way].aread(index)` for hit-path tag read (see Requirement: hit-path uses aread)

### Requirement: hit-path uses aread (combinational read)

The cache hit-path tag and data read SHALL use `ch_mem::aread(addr)` (combinational `assign`, 0-cycle) so that hit/miss decision and data output are available in the same tick. This aligns with the `mem-read-timing` capability's `aread emits combinational assign in Verilog` requirement.

#### Scenario: aread usage in hit detection

- **WHEN** the cache describes the hit-path
- **THEN** tag read SHALL be `tag_mems[way].aread(index)`
- **AND** data read SHALL be `data_mems[way].aread(index)`
- **AND** valid bit read SHALL be `valid_mems[way].aread(index)`
- **AND** LRU read SHALL be `lru_mem.aread(index)`

#### Scenario: Hit/miss decision is same-tick

- **WHEN** `req=1` and `addr=X` are set as inputs in the same `tick()` input phase
- **THEN** `io().miss` output SHALL reflect whether `addr=X` hit or missed in the same cycle (no 1-cycle lag)
- **AND** on hit, `io().data` / `io().rdata` SHALL reflect the cached value in the same cycle

### Requirement: miss-path fill uses ch_mem::write

The cache miss-path fill (AXI response-driven write) SHALL use `ch_mem::write(addr, data, enable)` (registered write, 1-cycle latency). The fill SHALL occur when `axi_resp_valid=1` and the request was a miss.

#### Scenario: write API usage

- **WHEN** the cache detects miss + AXI response valid in the same tick
- **THEN** it SHALL call `tag_mems[victim_way].write(index, tag, ch_bool(true))`
- **AND** `data_mems[victim_way].write(index, axi_data_zext, ch_bool(true))` (axi_data 32-bit SHALL be zero-extended to DATA_WIDTH=128 via `zext<DATA_WIDTH>(io().axi_data)`)
- **AND** `valid_mems[victim_way].write(index, ch_bool(true), ch_bool(true))`
- **AND** `lru_mem.write(index, victim_way, ch_bool(true))` (newly filled way becomes MRU)

#### Scenario: LRU replacement policy

- **WHEN** a miss occurs and `lru_mem.aread(index)` returns the current LRU bit
- **THEN** `victim_way` SHALL be a `ch_uint<1>` (1-bit width) with value `0` (way0 is LRU) or `1` (way1 is LRU)
- **AND** the new fill SHALL overwrite `victim_way`
- **AND** the new LRU state SHALL be `victim_way` (just-filled way is now MRU)

### Requirement: external IO signature unchanged

The `__io(...)` interface of `ICacheComplete` and `DCacheComplete` SHALL match the pre-rewrite signature exactly, so that wrappers (`ICachePrefetch` in `examples/riscv-mini/src/i_cache_prefetch.h`, `DCacheWriteBuffer` in `examples/riscv-mini/src/d_cache_write_buffer.h`) and any external consumer compile without modification.

#### Scenario: ICacheComplete IO signature

- **WHEN** the cache class declares its `__io(...)`
- **THEN** it SHALL contain:
  - `ch_in<ch_uint<32>> addr`
  - `ch_in<ch_bool> req`
  - `ch_out<ch_uint<32>> data`
  - `ch_out<ch_bool> ready`
  - `ch_out<ch_bool> miss`
  - `ch_out<ch_uint<32>> axi_addr`
  - `ch_out<ch_bool> axi_valid`
  - `ch_in<ch_bool> axi_ready`
  - `ch_in<ch_uint<32>> axi_data`
  - `ch_in<ch_bool> axi_resp_valid`

#### Scenario: DCacheComplete IO signature

- **WHEN** the cache class declares its `__io(...)`
- **THEN** it SHALL additionally contain:
  - `ch_in<ch_uint<32>> wdata`
  - `ch_in<ch_bool> we`
  - `ch_out<ch_uint<32>> rdata` (vs `ICacheComplete::data`)
  - `ch_out<ch_uint<32>> axi_wdata`
  - `ch_out<ch_bool> axi_we`
  - `ch_in<ch_uint<32>> axi_rdata` (vs `ICacheComplete::axi_data`)

### Requirement: address decomposition is width-correct

The cache SHALL decompose the 32-bit input address into offset (4 bits, `[3:0]`), index (7 bits, `[10:4]`), and tag (21 bits, `[31:11]`) without redundant `ch_uint<N>(port)` wrapping that may trigger ambiguity or width mismatch.

#### Scenario: bits extraction

- **WHEN** the cache extracts `offset` / `index` / `tag` from `io().addr`
- **THEN** `offset` SHALL be `bits<OFFSET_BITS-1, 0>(io().addr)` (no `ch_uint<32>` wrap)
- **AND** `index` SHALL be `bits<OFFSET_BITS+INDEX_BITS-1, OFFSET_BITS>(io().addr)` (no `ch_uint<32>` wrap)
- **AND** `tag` SHALL be `bits<31, OFFSET_BITS+INDEX_BITS>(io().addr)` (no `ch_uint<32>` wrap)

### Requirement: no deprecated logical operators in cache code

The cache class source code SHALL NOT use `operator&&` or `operator||` on `ch_bool` or port values, per ADR-036 deprecation. Mixed `ch_bool + port` patterns SHALL be expressed via `select()` or explicit `ch_bool(...)` conversion.

#### Scenario: ch_bool ch_bool logic

- **WHEN** the cache computes hit detection (`valid_way0 && (tag_way0 == tag)`)
- **THEN** it SHALL use `&` (bit-identical to `&&` per `deprecate-hdl-logical-operators-redo` design.md rationale)
- **AND** hit-way OR (`hit_way0 || hit_way1`) SHALL use `|`

#### Scenario: mixed ch_bool port logic

- **WHEN** the cache computes `hit || io().axi_ready` (mixed `ch_bool` + `port<ch_bool>`)
- **THEN** it SHALL NOT use `||` directly
- **AND** it SHALL express as `select(hit, hit, io().axi_ready)` (value mux)
- **AND** similar pattern SHALL be used for `miss && io().req` → `select(miss, miss, ch_bool(false)) & io().req`

### Requirement: literal suffix is `_h` not `_d`

The cache class source code SHALL use `_h` suffix for hex literals (e.g., `0xFFFFFFF0_h` for block-aligned address mask). The pre-rewrite typo `0xFFFFFFF0_d` (with `_d` suffix) SHALL NOT be present.

#### Scenario: block address mask

- **WHEN** the cache computes block-aligned address (`addr & 0xFFFFFFF0`)
- **THEN** the literal SHALL be `ch_uint<32>(0xFFFFFFF0_h)` (or equivalent construction)

### Requirement: cache_complete.h is buildable

The cache header files (`include/cpu/cache/{i,d}_cache_complete.h`) SHALL compile cleanly via `cmake --build build -j$(nproc)` when included by any test or example target, with 0 errors and 0 warnings on the cache files themselves. The pre-rewrite dead-on-arrival state (11 build errors blocking instantiation) SHALL NOT be present.

#### Scenario: clean build

- **WHEN** `examples/riscv-mini/tests/test_cache_smoke.cpp` instantiates `ICachePrefetch` and `DCacheWriteBuffer`
- **THEN** `cmake --build build --target riscv_mini_cache_smoke -j$(nproc)` SHALL exit 0
- **AND** no warning on the cache files (no `-Wdeprecated-declarations` from ADR-036's 8 overloads; no unused variable; no sign comparison)

### Requirement: regression coverage via test_cache_complete

The codebase SHALL provide at least one Catch2 test file (`tests/chlib/test_cache_complete.cpp`) with TEST_CASEs covering:

1. `icache miss-fill-then-hit` `[cache][complete][icache][miss-fill]`
2. `dcache read miss-fill` `[cache][complete][dcache][miss-fill]`
3. `2-way multi-port aread isolation` `[cache][complete][isolation][g1]` (covers R2 gap G1)
4. `same-cycle hit-aread + miss-fill-write` `[cache][complete][concurrency][g2]` (covers R2 gap G2)

#### Scenario: 4 test cases pass

- **WHEN** `ctest -R test_cache_complete --output-on-failure` runs after `cmake --build build -j$(nproc)`
- **THEN** the test count SHALL be ≥ 4
- **AND** all 4 TEST_CASEs SHALL pass
- **AND** `ctest -N | tail -1` SHALL report CTEST_TOTAL ≥ 160 (159 baseline + ≥ 1 new test executable)

#### Scenario: g1 gap coverage

- **WHEN** test case `[cache][complete][isolation][g1]` runs
- **THEN** it SHALL instantiate a 2-way cache with two independent `aread` operations in the same tick
- **AND** it SHALL verify that the two `aread` results return distinct set data (no cross-port contamination)
- **AND** failure of this test SHALL indicate a `ch_mem::aread` isolation regression

#### Scenario: g2 gap coverage

- **WHEN** test case `[cache][complete][concurrency][g2]` runs
- **THEN** it SHALL trigger a hit on set X (way 0) and a miss-fill-write on set Y (way 1) in the same tick
- **AND** it SHALL verify in the next tick that set X's hit data is unchanged (not contaminated by the fill write)
- **AND** it SHALL verify that set Y's miss is now a hit (the fill write completed)
- **AND** failure of this test SHALL indicate a same-cycle concurrency bug

### Requirement: stub_stub.h deprecation is preserved

The stub classes (`include/cpu/cache/{i,d}_cache_stub.h`) SHALL remain marked `[[deprecated]]` (set by ADR-037 on 2026-10-17). This change SHALL NOT modify the stub files or remove the deprecation marker. Physical stub deletion remains scheduled per ADR-037 §2 (deadline 2027-04-17).

#### Scenario: stub files unchanged

- **WHEN** this change's commits are reviewed
- **THEN** `include/cpu/cache/i_cache_stub.h` and `include/cpu/cache/d_cache_stub.h` SHALL NOT appear in the diff
- **AND** both files SHALL retain their `[[deprecated("...")]]` attribute on the stub class

## See also

- `docs/adr/ADR-039-cache-complete-rewrite-via-ch-mem.md` (commit `bfb3960`) — parent decision
- `docs/adr/ADR-037-cache-stub-removal.md` — sibling (stub deprecation timeline)
- `.rddf/improvements/cache-complete-doa-note.md` — dead-on-arrival incident analysis
- `.rddf/improvements/cache-complete-rewrite-session-handoff.md` — Session B handoff with 6-step TDD checklist
- `openspec/specs/mem-read-timing/spec.md` — `aread`/`sread` timing contract (dependency)
- `openspec/specs/verilator-e2e-coverage/spec.md` §R1 invariant 3 — imem `sread` requirement (context)