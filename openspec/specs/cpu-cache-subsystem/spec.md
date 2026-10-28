# Capability: cpu-cache-subsystem

## Purpose

定义 CppHDL CPU cache subsystem 的可实例化实现契约：基于 `ch_mem` 原语构建的 4KB 2-way set-associative L1 cache（`ICacheComplete` + `DCacheComplete`），包括 Hit/Miss 检测、LRU 替换、AXI 填充、写策略。该能力让 RV32I 顶层集成（`riscv-mini` SOC）能以可预测、可追溯、可测量的方式实例化 cache 子模块。

In English: Defines the contract of CppHDL's CPU cache subsystem — `ICacheComplete` and `DCacheComplete` classes built on `ch_mem` primitives — supporting Hit/Miss detection, LRU replacement, AXI fill, and write strategies. Enables RV32I top-level integration to instantiate cache submodules with predictable, traceable, measurable behavior.

**2026-10-28 补充**：本 capability 除**cache 本体**（`ICacheComplete` / `DCacheComplete`）外，还管辖**两个 wrapper**（`include/cpu/cache/i_cache_prefetch.h` 的 `ICachePrefetch` = `ICacheSync` 的 wrapper；`include/cpu/cache/d_cache_write_buffer.h` 的 `DCacheWriteBuffer` = **AXI 写直通 shim**，**无**内层 cache、**无**读通路）的**信号发射契约**：wrapper 的组合逻辑 MUST **真正发射**（不得用建图期 C++ `if` 决定是否发射）、每个 `ch_out` MUST 被**无条件**驱动、AXI 主端的 `axi_valid` MUST NOT 依赖 `axi_ready`、内层 cache 的每个 `ch_in` MUST 被显式驱动。⚠️ 此前两个 wrapper 的输出**恒为未驱动**（功能上是死的）⇒ 该契约的由来与边界见归档 change `2026-10-28-riscv-mini-cache-wrapper-live-logic-2026-10` 的 `design.md`。

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

The `__io(...)` interface of `ICacheComplete` and `DCacheComplete` SHALL match the pre-rewrite signature exactly, so that wrappers (`ICachePrefetch` in `include/cpu/cache/i_cache_prefetch.h`, `DCacheWriteBuffer` in `include/cpu/cache/d_cache_write_buffer.h`) and any external consumer compile without modification.

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

### Requirement: cache variant selection (Sync default, Async for TCM-style)

The cache subsystem SHALL provide two latency variants for both instruction and data cache, allowing users to select based on integration context. ADR-041 documents the rationale (Fmax, RV32I pipeline pc_lag alignment, industry convention).

#### Scenario: Sync variant (1-cycle sread, default for RV32I L1)

- **WHEN** the cache is instantiated as `ICacheSync` or `DCacheSync` (or legacy alias `ICacheComplete` / `DCacheComplete`)
- **THEN** the hit-path SHALL use `ch_mem::sread(addr, enable=ch_bool(true))` for tag/data/valid read
- **AND** the hit latency SHALL be 1 cycle (sread is registered)
- **AND** RV32I 5-stage pipeline pc_lag SHALL be naturally aligned (no extra IF/ID reg adjustment needed)
- **AND** this variant SHALL be the default (recommended for RV32I L1 imem/dmem)

#### Scenario: Async variant (0-cycle aread, for soft-core TCM)

- **WHEN** the cache is instantiated as `ICacheAsync` or `DCacheAsync`
- **THEN** the hit-path SHALL use `ch_mem::aread(addr)` for tag/data/valid read
- **AND** the hit latency SHALL be 0 cycle (aread is combinational)
- **AND** the Fmax SHALL be lower than Sync (due to longer combinational read path)
- **AND** this variant SHALL be appropriate for soft-core TCM-style designs (low-frequency, latency-critical)
- **AND** this variant SHALL NOT be used for RV32I imem (verilator-e2e-coverage spec §R1 invariant 3 requires imem sread)

#### Scenario: legacy `*_complete` shim alias

- **WHEN** the legacy class name `ICacheComplete` or `DCacheComplete` is used
- **THEN** it SHALL resolve to `ICacheSync` / `DCacheSync` (1-cycle sread, default) via `using` type alias
- **AND** no behavior change SHALL occur for existing callers

#### Scenario: variant equivalence on identical fill sequence

- **WHEN** the same fill sequence (addr + axi_data + axi_resp_valid=1) is executed on Sync and Async variants
- **THEN** the final cache state (memory contents) SHALL be identical after fill + 1-cycle NOP
- **AND** the test_case `Sync vs Async same fill behavior` in `tests/chlib/test_cache_complete.cpp` SHALL verify this equivalence

### Requirement: riscv-mini cache wrapper MUST NOT use C-style ch_reg array

`include/cpu/cache/i_cache_prefetch.h` 与 `include/cpu/cache/d_cache_write_buffer.h` **不允许**使用 `ch_reg<T>[N]` C-style 数组（runtime init 触发 SIGSEGV）。MUST 用 `ch_mem<T, N>` 替代（per ADR-039 + cache-complete-doa-note §6.1 R1）。

#### Scenario: line-level grep 检查

- **WHEN** 对 `include/cpu/cache/i_cache_prefetch.h` 与
  `include/cpu/cache/d_cache_write_buffer.h` 执行下列 grep（**不加反引号** ——
  见 `scripts/verify_openspec_delta.sh` 的 R2：反引号内含 `<...>` 且含路径的片段会被
  判为**占位符路径** ⇒ 命令用正文书写）：

      grep -nE "ch_reg<[^>]+>\s*\[" include/cpu/cache/i_cache_prefetch.h include/cpu/cache/d_cache_write_buffer.h

- **THEN** 输出必须为空（无 C-style ch_reg 数组）
- **AND** 如有命中，则该 wrapper MUST 用 `ch_mem<T, N>` 替换

#### Scenario: instantiation smoke 测试必通过

- **WHEN** `cd build && ./examples/riscv-mini/riscv_mini_cache_smoke "[fixed]"`
- **THEN** instantiation 必通过；不允许 SIGSEGV

### Requirement: cache stub 物理移除且行为覆盖由 Sync 变体承接

`include/cpu/cache/{i,d}_cache_stub.h` MUST 从 repo 物理移除，cache 行为覆盖 MUST
由 `ICacheSync` / `DCacheSync` 变体承接（ADR-039 ch_mem 重写 + ADR-041 Sync/Async
双变体）。本能力 MUST NOT 引入运行时 config 通道或 BYPASS mode —— stub 的
pass-through 语义是测试夹具语义，随 stub 一并消失。

#### Scenario: stub 头文件物理不存在

- **WHEN** reviewer 执行 `git ls-files include/cpu/cache/*_stub.h`
- **THEN** 输出为空（0 个 stub 头文件）

#### Scenario: 代码树无 stub 引用

- **WHEN** reviewer 执行
  `grep -rn "#include.*_cache_stub\.h" --include='*.h' --include='*.cpp' include/ src/ tests/ examples/`
- **THEN** 输出为 0 行
- **AND** reviewer 执行
  `grep -rn "chlib::ICache\b\|chlib::DCache\b" --include='*.h' --include='*.cpp' include/ src/ tests/ examples/`
  输出为 0 行
- **AND** `chlib::ICacheSync` / `chlib::ICacheAsync` / `chlib::DCacheSync` /
  `chlib::DCacheAsync` 引用 MUST NOT 被上述检查误伤
- **AND** 历史注释 MAY 提及被删除的 stub 路径（`tests/chlib/test_cache_sync.cpp`
  的历史说明），但该路径 MUST NOT 出现在任何 `#include` 或代码引用中

#### Scenario: cache 集成测试覆盖真实 cache 语义

- **WHEN** `ctest -R test_cache_pipeline --output-on-failure` 运行
- **THEN** 测试 MUST 实例化 `ICacheSync` / `DCacheSync`
- **AND** MUST 断言真实 cache 时序（cold miss → AXI fill → refilled hit），
  而非 stub 的 pass-through 语义
- **AND** 该测试 MUST NOT 依赖 `[[deprecated]]` 抑制（`-Wdeprecated-declarations`
  在该 TU 上为 0）

#### Scenario: 活文档无 stub 引用

- **WHEN** reviewer 执行
  `grep -rn "cache_stub" AGENTS.md include/cpu/AGENTS.md`
- **THEN** 输出为 0 行
- **AND** `openspec/specs/` 不在本检查范围内 —— 本 capability 的 Requirement
  按设计需要指名被删除的 stub 路径以保证可验证性
- **AND** `docs/adr/ADR-037-cache-stub-removal.md` SHALL 保留其历史 Decision 记录，
  但其 Status 章节 MUST 含 `Executed` 标记

#### Scenario: 既有 cache 变体契约不变

- **WHEN** reviewer 检查本 change 的 diff
- **THEN** `include/cpu/cache/{i,d}_cache_{sync,async,complete}.h` MUST NOT 出现
  在 diff 中（`ICacheComplete` / `DCacheComplete` 继续是 `ICacheSync` /
  `DCacheSync` 的 legacy alias）
- **AND** `tests/chlib/test_cache_sync.cpp` MUST NOT 出现在 diff 中

#### Scenario: 回归不退化

- **WHEN** `ctest -LE perf --output-on-failure` 运行
- **THEN** 全部 PASS，测试可执行数与改动前一致（无 test 文件被删除）

### Requirement: riscv-mini cache wrapper 的组合逻辑 MUST 真正发射（不得用建图期 C++ `if` 决定是否发射）

- **WHEN** wrapper 的 `describe()` 里有由**输入端口取值**决定的条件
      （`ch_bool` 输入，或任何**比较结果为 `ch_bool`** 的表达式）
- **THEN** 该条件 MUST 用**组合表达式**表达（`select` / 布尔 `&` / `|` / `!` 皆可）
- **AND** 该条件 MUST 体现在**输出端口的取值**里 ⇒ 「无条件 `<<=`，条件在表达式内」
      —— **不是**「条件为真才 `<<=`」（条件为假时又变回未驱动）
- **AND** MUST NOT 用 C++ `if` 决定「是否发射」（此类判定在**建图期**恒为假）
- **AND** 作为 AXI **主端**的 `axi_valid` MUST NOT 依赖 `axi_ready`（见下方 Scenario）

#### Scenario: 输出 MUST 随输入变化

- **WHEN** wrapper 的**某一个**输入取**不同**的值
      （`ICachePrefetch`：`req` / `addr` / `axi_ready` / `axi_data`；
      `DCacheWriteBuffer`：`req` / `we` / `addr` / `wdata` / `axi_ready`）
- **THEN** 依赖该输入的输出 MUST **随之不同**
      （⇒ 断言 MUST 做到「**逐个输入 → 逐个输出**」的粒度，
      **不是**只断言「输出非零」—— 后者只证明「有驱动」，证不出「随输入变化」）
- **AND** 反向期望：若输出恒为未驱动（读 0）⇒ 两次都相同 ⇒ **违反**本 Scenario
- **NOTE**: ⚠️ 「组合表达式」**不**限定为 `select(...)` —— 布尔 AND 的自然写法是 `&`
  （框架已把 `&&` 标为 deprecated）⇒ 否则会把**正确**的 `req & we` 判为违规。
- **NOTE**: 上文 WHEN 的「条件」**不限于** `ch_bool` 类型的输入端口 ——
  `ch_uint` 本身**没有** bool 转换（`include/core/bool.h` 是全仓唯一的
  `explicit operator bool()`）⇒ `if (io().addr)` 编译不过；
  但**比较**的结果**是** `ch_bool` ⇒ `if (io().addr == ...)` 编译通过且
  **同样**建图期恒假 ⇒ 正文 MUST 覆盖它。

#### Scenario: 每个输出端口 MUST 被驱动（不得恒为未驱动）

- **WHEN** 检视 wrapper `__io(...)` 里声明的**每一个** `ch_out` 端口
- **THEN** 该端口 MUST 在 `describe()` 里被**无条件**驱动
      （条件 MUST 体现在表达式的**取值**里 ⇒ 条件为假时也有确定值
      ⇒ **不是**「条件为真才 `<<=`」那种「条件为假又变回未驱动」的形态）
- **AND** MUST NOT 存在「驱动语句被注释掉」或「端口从未被驱动」的情况
- **AND** 反向期望：某个 `ch_out` 端口在**其依赖的输入变化时**仍**恒读 0**
      ⇒ **违反**本 Scenario
- **NOTE**: ⚠️ 反向期望**刻意**不写成「恒读 0」⇒ 被**正确驱动**的端口也可能
  合法读 0（例如 `DCacheWriteBuffer::axi_we = io().we`，`we = 0` 时它就是 0）
  ⇒ 判据必须是「**依赖的输入变了，它却不变**」。
- **NOTE**: ⚠️ 本条覆盖 `axi_addr` / `axi_wdata` / `axi_we` 这类
  「只把 C++ `if` 换掉也**仍然**未驱动」的端口
  —— 只满足「输出随输入变化」**不足以**让 wrapper 可用。

#### Scenario: `axi_addr` MUST 是被请求地址的块地址

- **WHEN** `ICachePrefetch` 发出一次 AXI 请求
- **THEN** 它的 `axi_addr` MUST 是**被请求地址**按缓存块大小对齐后的地址
      （16 字节对齐的块地址，与内层 cache 的 `axi_addr` **同源** ⇒ MUST 直接**转发**内层值，
      不得在 wrapper 层重算）
- **AND** `DCacheWriteBuffer` 的 `axi_addr` MUST 等于其 `addr` 输入（直通式，不做对齐）
- **AND** 反向期望：`axi_addr` 恒为 `0`、或与被请求地址无关 ⇒ **违反**本 Scenario
- **NOTE**: 本条把「地址**被驱动**」升级为「地址**正确**」——
  前者只排除「未驱动」，排除不了「被驱动成垃圾值」。
  ⚠️ 两条子句**按组件分别适用**（`ICachePrefetch` 对齐 / `DCacheWriteBuffer` 直通）
  ⇒ **不得**把「块对齐」套到 `DCacheWriteBuffer` 上。
  ⇒ ⚠️ 判别力要求：断言 MUST 用**不对齐**的地址（否则「对齐」与「不对齐」两种
  语义**都验不出来** —— 例如 `0x80000040` 对齐 ⇒ 块地址与原地址相同）。

#### Scenario: AXI 主端的 `axi_valid` MUST NOT 依赖 `axi_ready`

- **WHEN** wrapper 作为 AXI **主端**驱动 `axi_valid`
- **THEN** `axi_valid` MUST NOT 依赖 `axi_ready`
      （AXI 要求「VALID **不得**等 READY 才拉高」⇒ READY 为低时，
      VALID 仍 MUST 保持拉高，等待握手成立）
- **AND**（**仅 `DCacheWriteBuffer`** —— 它是**写**主端）`ready` MUST 表示
      「本次写**已被 AXI 接受**」⇒ 消费方 MUST **保持（锁存）**
      `req` / `we` / `addr` / `wdata` 直到 `ready` 拉高
- **AND**（**仅 `ICachePrefetch`** —— 它是**读**主端）`ready` MUST 表示
      「数据可用（hit 或 AXI 已接受本次读）」⇒ 消费方 MUST **保持（锁存）**
      `addr` / `req` 直到 `ready` 拉高
      （⚠️ 内层 `miss` / `hit` 来自 **1 拍 `sread`** ⇒ 消费方在 miss 落定前
      换 `addr` 会与 `axi_addr` 错位 ⇒ 可能把数据填进**错误**的 entry）
- **AND** 反向期望：`axi_ready` 为低时 `axi_valid` 变 0 ⇒ 该次事务**根本不出现在
  AXI 总线上**；若无状态/队列保留它 ⇒ **写被静默丢弃** ⇒ **违反**本 Scenario
- **NOTE**: 本条是「输出 MUST 随输入变化」的**对偶**：输出**不**该随 `axi_ready` 变化。
  ⚠️ 两条 `ready` 子句**按组件分别适用**（`ICachePrefetch` 是读主端、
  **没有** `we` / `wdata` 端口 ⇒ 「保持 `we`/`wdata`」对它**无法满足**）。

#### Scenario: 内层 cache 的输入端口 MUST 被全部驱动

- **WHEN** wrapper 在 `describe()` 里实例化了一个内层 cache 组件
      （⚠️ 当前只有 `ICachePrefetch` 实例化内层 `ICacheSync`；
      `DCacheWriteBuffer` **没有**内层 cache ⇒ 本条对它**不适用**）
- **THEN** 该内层组件 `__io(...)` 里的**每一个** `ch_in` 端口 MUST 被显式驱动
- **AND** MUST NOT 出现「内层输入端口只被**读**、从不被**驱动**」的情况
- **AND** 反向期望：内层 `addr` / `req` 未驱动 ⇒ 内层的 `miss` 恒为「冷 miss」
      ⇒ `hit` / `miss` **不可区分** ⇒ **违反**本 Scenario
- **NOTE**: 未驱动的输入端口读到的是**未定义值**（根 `AGENTS.md §ANTI-PATTERNS`
  登记：实测 `strbe` 读**全 1**、`rst_n` 读 0）⇒ 依赖默认值会让测试「**碰巧**通过」
  而跨后端行为可能分叉。

#### Scenario: 建图期 C++ `if` MUST 被 grep 拦住

- **WHEN** `grep -nE "[^_]if\s*\(.*io\(\)\." include/cpu/cache/i_cache_prefetch.h include/cpu/cache/d_cache_write_buffer.h`
- **THEN** 输出必须为空（wrapper 内不得有以 `io()` 条件决定的 C++ `if`）
- **NOTE**: ⚠️ 正则用 `[^_]if`（**不是** `if`）⇒ 否则 `_if(...)` 这个
  **合法**的建图期条件块构造也会被匹配 ⇒ 与「MUST 用组合表达式」互斥。
  ⚠️ **本条的边界（如实声明）**：它是**形态探针**（抓「以 `io()` 为条件的
  C++ `if`」这个具体形态），**不是**通用形态守卫 ——
  ① `if` 位于**行首第 0 列**时前一个字符不存在 ⇒ 该正则**不匹配**
  （⇒ 实施时 MUST 确认目标 `if` 仍在缩进后，否则护栏**静默失效**）；
  ② `[^_]` 会**误报** `notif(` 这类**前一个字符不是 `_`** 的标识符
  （`notif(` 的 `if` 前是 `t` ⇒ 会被匹配 ⇒ 假阳性方向）；
  ③ ⚠️ 该 grep 是**行内**匹配 ⇒ 条件**跨行**时（`io().` 落在下一行）
  **不被匹配** ⇒ 护栏会**静默放过**该形态。
  ⇒ ⇒ **规范性判据是 Requirement 正文的 MUST NOT**（覆盖所有形态）；
  本 Scenario 只是**可执行的探针** ⇒ **不是**「不存在 `if`」的证明。
  ⇒ ⚠️ **修复后两个 wrapper 里的 C++ `if` 数量 = 0** ⇒ 该 grep **恒空**
  ⇒ 护栏从「抓现状」变成「**防复发**」（任何人再加回 `if (io()...)` 就会被抓到）。
  ⚠️ **护栏的已知边界（如实登记）**：该 grep **不区分代码与注释** ⇒
  **注释里逐字引用那个 `if` 形态也会被判为命中** ⇒ 护栏**恒红**。
  ⇒ ⇒ **两个 wrapper 的注释 MUST NOT 逐字引用**「`if` + `(` + `io().`」的形态
  （只能**描述**它，如「条件形如『`cache_miss` 与 `io().axi_ready` 同时为真』」）
  ⇒ 这是**护栏的可执行约束**，不是文档偏好。

## See also

- `docs/adr/ADR-039-cache-complete-rewrite-via-ch-mem.md` (commit `bfb3960`) — parent decision
- `docs/adr/ADR-037-cache-stub-removal.md` — sibling (stub deprecation timeline)
- `.rddf/improvements/cache-complete-doa-note.md` — dead-on-arrival incident analysis
- `.rddf/improvements/cache-complete-rewrite-session-handoff.md` — Session B handoff with 6-step TDD checklist
- `openspec/specs/mem-read-timing/spec.md` — `aread`/`sread` timing contract (dependency)
- `openspec/specs/verilator-e2e-coverage/spec.md` §R1 invariant 3 — imem `sread` requirement (context)
