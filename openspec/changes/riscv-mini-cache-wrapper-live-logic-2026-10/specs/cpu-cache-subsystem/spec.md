# 变更规格：riscv-mini cache wrapper 的组合逻辑 MUST 真正发射

## MODIFIED Requirements

### Requirement: external IO signature unchanged

- **MODIFIED**: 本 Requirement 的**路径引用**被订正 —— 两个 wrapper 在 ADR-046 收敛后
  已从 `examples/riscv-mini/src/` 迁到 `include/cpu/cache/`（2026-10-26，单源化）
  ⇒ 正文里的旧路径**已失效**（`examples/riscv-mini/src/i_cache_prefetch.h` 不存在）。
  ⚠️ MODIFIED 会**整体替换**该 Requirement ⇒ **原标题与全部条款逐字保留**
  （本 Requirement 的语义 ——「cache 本体的 `__io` 签名不变」—— **不变**，
  只订正**指向 wrapper 的路径**）；既有 Scenario 一条不删。

The `__io(...)` interface of `ICacheComplete` and `DCacheComplete` SHALL match the pre-rewrite signature exactly, so that wrappers (`ICachePrefetch` in `include/cpu/cache/i_cache_prefetch.h`, `DCacheWriteBuffer` in `include/cpu/cache/d_cache_write_buffer.h`) and any external consumer compile without modification.

#### Scenario: ICacheComplete IO signature

- **WHEN** the cache class declares its `__io(...)`
- **THEN** it SHALL contain:
  - `ch_in<ch_uint<32>> addr`
  - `ch_in<ch_bool> req`
  - `ch_out<ch_uint<32>> data`
  - `ch_out<ch_bool> ready`

#### Scenario: DCacheComplete IO signature

- **WHEN** the cache class declares its `__io(...)`
- **THEN** it SHALL additionally contain:
  - `ch_in<ch_uint<32>> wdata`
  - `ch_in<ch_bool> we`
  - `ch_out<ch_uint<32>> rdata` (vs `ICacheComplete::data`)
  - `ch_out<ch_uint<32>> axi_wdata`
  - `ch_out<ch_bool> axi_we`
  - `ch_in<ch_uint<32>> axi_rdata` (vs `ICacheComplete::axi_data`)

### Requirement: riscv-mini cache wrapper MUST NOT use C-style ch_reg array

- **MODIFIED**: 与上一条同理，**路径引用**被订正为 ADR-046 收敛后的真位置
  （`include/cpu/cache/…`）⇒ 其余条款（禁用 `ch_reg<T>[N]`、MUST 用 `ch_mem<T, N>`、
  grep 检查、smoke 测试）**逐字保留**；既有 Scenario 一条不删、标题不改。

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

## ADDED Requirements

### Requirement: riscv-mini cache wrapper 的组合逻辑 MUST 以 `select` 发射

- **ADDED**: 两个 wrapper 此前用**普通 C++ `if`** 包裹 `ch_bool` 条件发射 RTL
  ⇒ 建图期求值为「节点是否非零」⇒ **分支体从不发射** ⇒ 输出**恒为未驱动**。
  ⚠️ 与既有「MUST NOT use C-style ch_reg array」是**不同**条款（那条管**存储**）。

- **WHEN** wrapper 的 `describe()` 里有由 `ch_bool` 输入决定的条件
- **THEN** 该条件 MUST 用 `select(...)`（或 `_if(...)`）表达
- **AND** 分支内的 `io().xxx <<= ...` MUST 被**真正发射**（⇒ 输出随输入变化）
- **AND** MUST NOT 用 C++ `if` 决定「是否发射」（建图期求值 ⇒ 恒假）

#### Scenario: 输出 MUST 随输入变化

- **WHEN** wrapper 的 `req`（或等价输入）在两次运行中取**不同**的值
- **THEN** 对应的输出（如 `ready` / `axi_valid`） MUST **不同**
- **AND** 反向期望：若输出恒为未驱动（读 0）⇒ 两次都相同 ⇒ **违反**本 Scenario

#### Scenario: 建图期 C++ `if` MUST 被 grep 拦住

- **WHEN** `grep -nE "if\s*\(.*io\(\)\." include/cpu/cache/i_cache_prefetch.h include/cpu/cache/d_cache_write_buffer.h`
- **THEN** 输出必须为空（wrapper 内不得有以 `io()` 条件决定的 C++ `if`）
