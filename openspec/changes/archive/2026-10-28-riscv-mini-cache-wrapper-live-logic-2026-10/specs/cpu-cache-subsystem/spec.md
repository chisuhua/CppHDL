# 变更规格：riscv-mini cache wrapper 的组合逻辑 MUST 真正发射

## MODIFIED Requirements

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

## ADDED Requirements

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
