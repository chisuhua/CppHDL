# Change: fifo-read-data-path-2026-10 — `fifo_bundle` 的**读数据通路**

- Status: **草案（尚未实施）**
- 立项自 2026-10-22 的 `bundle-ecosystem-hardening-2026-10`（其 L10，用户裁决**不在该 change 处理** ⇒ 独立立项）
- 关联 capability：`bundle-master-slave-io`（**必然**需要 MODIFIED —— 见 D3；
  ⚠️ 主 spec 已把 `fifo_bundle` 的 5 字段与方向写死）

## Why

`fifo_bundle<T>`（`include/bundle/common_bundles.h`）只有**一个**数据字段：

```cpp
CH_BUNDLE_FIELDS_T(data, push, full, pop, empty)
```

而 `data` 被**两个方向**共用：master 用它**写**数据，slave 需要用它**读**数据。
⇒ 这是 AXI / FIFO 类 bundle 的**经典双工问题**。

后果（实测，见 `samples/fifo_bundle.cpp` 的实现）：slave 侧**无处输出**读到的
数据 ⇒ 该 sample 的实现里只能写：

```cpp
// 读操作（异步读取）
auto data_out = mem.aread(rd_a);
// 注意：io.data是输入，不能赋值，我们只需要使用它作为输入
// 数据输出通过其他方式提供，这里我们不直接连接io.data
```

⇒ **FIFO 永远无法把数据送出去**。`data_out` 被算出来就**丢弃**。

### 为什么它没被立刻发现

`samples/fifo_bundle.cpp` 在 2026-10-22 之前一直是红的（`empty=0` / exit=1），
那个失败被 `bundle-master-slave-io-2026-10`（ADR-042）修掉了 —— 但**只**修掉了
「只读字段恒为 0」那一条，**没碰**「读数据通路缺失」这一条 ⇒ 该 sample 现在
**绿**了，却仍然**没有实现 FIFO 的读通路**。

⚠️ 这正是「**绿 ≠ 对**」的典型：护栏只能守住「已定义的行为」，守不住
「**从未被定义**的行为」。

### 现状确认

| 项 | 现状 |
|---|---|
| `fifo_bundle` 的字段 | `data` / `push` / `full` / `pop` / `empty`（**5 个**） |
| 方向 | master: `data`/`push`/`pop` OUT，`full`/`empty` IN；slave: **镜像** |
| 读数据通路 | **不存在**（`data` 在 slave 侧是**输入**） |
| 其它 bundle 是否同病 | ⚠️ 待清点（`axi_*_channel` 用**分离**的读/写通道 ⇒ 不同病；`ch_stream` 用 `payload` + `valid`/`ready` ⇒ **同病** —— 它的 `payload` 也是双工方向，但 stream 协议本身规定 `payload` 由 master 驱动、slave 接收 ⇒ **不构成问题**） |

## What Changes

### D1: `fifo_bundle` MUST 有**独立的读数据字段**

⚠️ 这是**公开 API 的布局变更** ⇒ MUST 先有 ADR（仓库先例 ADR-036 / ADR-037 /
ADR-042 / ADR-043；**ADR-044 编号可用** —— `docs/adr/` 现存最大编号是 ADR-043）。

**候选形态**（MUST 在 ADR 里逐项裁决）：

| 形态 | 说明 | 代价 |
|---|---|---|
| **A. 加一个 `rdata` 字段** | master: `data` OUT / `rdata` IN；slave: `data` IN / `rdata` OUT | 位宽 +N；**破坏**现有 `fifo_bundle` 用户的位布局假设 |
| **B. 拆成两个 bundle**（`fifo_wr` / `fifo_rd`） | 读写分离 | 破坏性更大；但语义最干净（与 AXI 的分离读写通道一致） |
| **C. 保持单字段，用「方向」表达双工** | `data` 在 master 侧是 OUT、在 slave 侧是 **IN**（现状）⇒ slave 无法输出 | ❌ **不可行** —— 这正是缺陷本身 |

**倾向**：**A**（加 `rdata`）—— 与 AXI 的「分离读写通道」方向一致，但改动面小。

⚠️ **ADR MUST 逐项裁决的 6 件事**（原提案只列了「形态」一项）：

1. **形态**（A / B / C）
2. **`rdata` 的字段位置** —— ⚠️ **决定全部位偏移**：
   - 追加到**末尾**（`data, push, full, pop, empty, rdata`）⇒ 既有 5 个偏移
     **不变**（`kFifoDataBits..kFifoEmptyBit` 只需追加新常量，diff 最小）；
   - 插在 `data` **之后** ⇒ `push/full/pop/empty` 全部 +W ⇒
     `tests/test_bundle_master_slave.cpp` 的位偏移常量 + 9 处
     `1ULL << kFifoPushBit` / `field_bit(packed, kFifoEmptyBit)` 全改，
     且**打包 API 对外部用户的位语义被静默改变**。
   ⇒ ADR MUST **写死**位置，并写明取舍。
3. **读通路的时序**（`aread` 0-cycle 组合 vs `sread` 1-cycle 同步）
4. **`empty` 时 `rdata` 是否 don't-care** —— ⚠️ 组合读时 `rdata = mem[rd_ptr]`，
   空 FIFO 时它是**残留/垃圾** ⇒ MUST 定义，否则消费者会 latch 垃圾
5. **「pop 沿前采样」规则**（见 D2）
6. **打包 API 的宽度边界**（见下方 ⚠️）

⚠️ **连带影响**（MUST 在 ADR 里清点）：
- `fifo_bundle` 的**位宽**从 `W+4` 变成 `2W+4` ⇒ 影响
  `get/set_bundle_value` 的打包布局、`tests/test_bundle_master_slave.cpp` 的
  位偏移常量（`kFifoPushBit` 等）；
- ⚠️ **打包 API 的能力回退**（原提案完全没提）：`include/simulator.h:236-242`
  对总宽 > 64 的 bundle **显式拒绝**（`CHERROR` + return，**不**静默截断）。
  现状 `fifo_bundle<ch_uint<W>>` 宽 `W+4` ⇒ `W ≤ 60` 可用；
  加 `rdata` 后宽 `2W+4` ⇒ **`W ≤ 30`** 才可用
  ⇒ **`W ∈ [31, 60]` 从「能用」变成「报错」** ⇒ ADR MUST 裁决
  （接受该回退 / 提供逐字段替代路径 / 限制 `fifo_bundle` 的 T 宽度）。
  （实测：`fifo_bundle<ch_uint<32>>` 加 `rdata` 后 = 68 位 > 64。）
- `samples/fifo_bundle.cpp` 的实现 MUST 改（`rdata` ← `mem.aread(rd_a)`）；
- `tests/test_bundle_master_slave.cpp` 的 DUT（MUST 读 `rdata`）与断言。

### D2: 读通路的**时序** MUST 与「FIFO 协议」一致

- `aread`（0-cycle，组合）vs `sread`（1-cycle，同步）—— FIFO 的**读数据**用哪个？
  ⚠️ 现状 sample 用 `aread`；而 `ch_mem` 的 `aread` 在 2026-10-21 的
  `sim-mem-read-timing-align-2026-10` 后已是 0-cycle（与 Verilog `assign` 一致）✓
  ⇒ **裁决** MUST 写进 ADR（不能只写「现状用 aread」—— 那是描述，不是裁决）
- ⚠️ **FIFO 协议**：`dout` 在 pop 沿**之后**指向**下一个**队首 ⇒ 消费者 MUST 在
  pop 沿**之前**采样 `dout`（`samples/fifo_example.cpp` 的注释已记录这条；
  `samples/fifo_bundle.cpp:215-219` 也已有同款注释）
  ⇒ 读数据通路的 ADR MUST 写明这条协议。
  ⚠️ **sample 侧的结构冲突**：`samples/fifo_bundle.cpp:142-212` 是 `tick()`
  **之后**才采样 ⇒ 按现结构，第 2 个元素（第 1 次 pop 弹出的元素）在
  **cycle 2** 才可读、**cycle 3** 起 `rdata` 已指向下一个 ⇒ sample MUST
  **写死期望值表**，否则极易差一拍 ⇒ 「绿 ≠ 对」。

### D3: capability 的 delta —— **必然** MODIFIED（不是「可能」）

⚠️ **2026-10-22 实测更正**：原提案写「**可能**需要 MODIFIED」—— **低估**。
主 spec `openspec/specs/bundle-master-slave-io/spec.md` **已经把 `fifo_bundle`
的字段集与方向写死了**：
- `:106`「今天的 `fifo_bundle` 就是这样 —— **5 个字段**里 4 个已镜像，唯独 `push` 放错边」
- `:108-115`「#### Scenario: fifo_bundle 的方向指认」**逐字段**列出
  `data`/`push`/`pop`（master OUT）与 `full`/`empty`（master IN）

⇒ 加 `rdata` **必然**要 MODIFIED 该 Requirement（标题与 Scenario 名 MUST
**逐字**匹配主 spec —— `scripts/verify_openspec_delta.sh` 会查）。

⇒ 该 capability 共 15 条 Requirement；MUST 逐条清点哪些**提到字段布局**：
`方向指认`（:99，**必改**）、`打包式 bundle API 的适用范围`（:455，**可能**要改 ——
因为 2W+4 的宽度后果）、`bundle 字段 MUST 拥有自己的节点`（:21，rdata 也是一等节点）。

⚠️ `bundle-base-integer-ctor` 的派生类**清单**（20 个）**不变**（`fifo_bundle`
还在）⇒ 不需要该 capability 的 delta。

## Impact

| 项 | 影响 |
|---|---|
| `include/bundle/common_bundles.h` | `fifo_bundle` 加 `rdata` 字段（**公开 API 布局变更**） |
| `docs/adr/ADR-044-fifo-read-data-path.md` | **新增**（**6 项裁决**：形态 + `rdata` 字段位置 + 时序协议 + `empty` 语义 + pop 沿前采样 + 宽度边界） |
| `samples/fifo_bundle.cpp` | 实现读数据通路（不再丢弃 `data_out`）+ **写死期望值表**（见 D2） |
| `samples/minimal_fifo_bundle.cpp` | MUST **显式驱动** `rdata`（否则新增字段恒 0）；⚠️ 该文件 `:26-27` 的 `io.full = false; io.empty = true;` 是**节点重绑定**（ADR-042 已把 `operator=` 定性为「不是连接手段」）⇒ 顺带修 |
| `tests/test_bundle_master_slave.cpp` | 位偏移常量 + DUT + 断言（**读数据必须真的到达 master 侧**）+ **方向元数据断言 MUST 补 `rdata`**（现有 `:281-305` 只断言 5 个字段） |
| `openspec/specs/bundle-master-slave-io/spec.md` | MODIFIED（**必然**，见 D3）—— Requirement 标题与 Scenario 名 MUST 逐字匹配主 spec |
| 计数 | ⚠️ **CTEST_TOTAL 按新增 TEST_CASE 数同步**（原提案写「可能 +1」—— 行为级 + 时序级**至少 +2**；若新建文件还要注册 target）⇒ MUST 同步根 `AGENTS.md` 的 `CTEST_TOTAL:` 行 + NOTES 计数 + `scripts/verify_test_counts.sh` |

> ⚠️ **2026-10-22 实测补充的 4 处「硬编码断言」**（加字段后**必然**要改，
> 否则**当场红**；其中 1 处是 `STATIC_REQUIRE` ⇒ **编译期**就断）：
>
> | 文件 | 断言 | 加 `rdata` 后 |
> |---|---|---|
> | `tests/test_bundle_bitstream.cpp:85` | `REQUIRE(get_bundle_width<fifo_bundle<ch_uint<4>>>() == 8)` | → **12** |
> | `tests/test_bundle_bitstream.cpp:99` | 同上 | → **12** |
> | `tests/test_bundle_advanced_features.cpp:75` | `STATIC_REQUIRE(bundle_field_count_v<fifo_bundle<ch_uint<32>>> == 5)` | → **6**（**编译期**断） |
> | `tests/test_bundle_ecosystem.cpp:487` | `REQUIRE(bundle_field_count_v<fifo_bundle<ch_uint<8>>> == 5)` | → **6** |
>
> ⇒ **更好的做法**：这 4 处断言 SHOULD 改成**不依赖具体字段数**的写法
> （例如断言「字段集合」而非「个数」）⇒ 否则**每加一个字段都要回来改一遍**。
> ⚠️ 但那是**独立的重构**（4 处断言的语义改写）⇒ MUST NOT 与本 change 混做
> （会让「加一个字段」这件事的回归面变大）⇒ 建议**先按数字改**，把
> 「断言不依赖字段数」记为 follow-up。
>
> **另有两处消费方**（实测 `fifo_bundle` 共 **9** 个文件 / 约 **49** 处引用 ——
> 原提案写「8 个文件 / 45 处」，实为 9 个，含 `include/core/bundle/bundle_base.h:112`
> 的一处注释引用）：
> - `samples/bundle_advanced_features.cpp:79` **打印** bundle 的宽度 ⇒ 打印值会变
>   （12 → 20）⇒ 该 sample 的输出随之变化（无断言，但属可观测行为变化）；
> - `samples/minimal_fifo_bundle.cpp` 是一个 FIFO 消费方（只接 `full`/`empty`，
>   **不接读数据**）⇒ 加了 `rdata` 后它**必须**决定是否接线 ⇒ 否则新字段恒 0。
>
> ⚠️ **第 5 处硬编码（原提案漏掉）** —— `tests/test_bundle_master_slave.cpp:178-184`：
> ```
> // `fifo_bundle` 的位布局：data(8) push(1) full(1) pop(1) empty(1)
> // → push 在 bit 8、full 在 bit 9、pop 在 bit 10、empty 在 bit 11
> constexpr unsigned kFifoDataBits = 8; kFifoPushBit = 8;
> constexpr unsigned kFifoFullBit = 9; kFifoPopBit = 10; kFifoEmptyBit = 11;
> ```
> 且 `:201/:209/:212/:234/:243/:250/:260/:266/:389` 共 **9 处**
> `1ULL << kFifoPushBit` / `field_bit(packed, kFifoEmptyBit)`。
> ⇒ **这 5 个偏移是否作废，完全取决于 ADR 对 `rdata` 字段位置的裁决**
> （追加末尾 ⇒ 原偏移不变；插入 ⇒ 全表重排）⇒ 见 D1 的裁决项 2。
>
> ⚠️ **打包 API 的能力回退**（见 D1 裁决项 6）：`W ∈ [31, 60]` 的
> `fifo_bundle` 从「`set/get_bundle_value` 可用」变成「**显式报错**」
> （`include/simulator.h:236-242`）⇒ ADR MUST 裁决。
>
> ⚠️ **生成 Verilog 的验收**（原提案漏掉）：读通路由 `ch_mem::aread` 驱动，
> 而 `aread` 正是 `sim-mem-read-timing-align-2026-10` 里踩过 codegen 三处配套坑
> 的路径（`print_proxy` 跳过 mem read port / `print_output` 的代理链遍历
> 不得越过 / `type_mem_read_port` MUST NOT 进 wire 声明列表）⇒
> MUST 有一条「生成的 Verilog 含 `rdata` 输出且 MUST NOT 引用未声明信号」的
> 验收（`tests/test_mem_comb_read.cpp` 已有同型护栏可抄）—— 现有
> `test_bundle_master_slave.cpp` **只跑 interpreter** ⇒ Verilog 路径零覆盖。
>
> | 风险 | 🔴 **高**。① **公开 API 布局变更**（位宽变化 ⇒ 打包布局变化 ⇒ 既有断言的位偏移常量要改 + **打包 API 能力回退**）；② 时序协议（FIFO 的 `dout` 语义 + sample 的「tick 后采样」结构）容易再次出错；③ 与 ADR-042/043 的「字段一等节点」模型必须保持一致；④ ⚠️ **TDD 排序问题**：行为级 RED 用例必然引用 `io.rdata` ⇒ 字段不存在 ⇒ **编译错误**而非「测试红」⇒ 而 `test_bundle_master_slave` 是**单一 target**（8 个 TEST_CASE）⇒ 编译断 ⇒ 既有 8 个用例全部无法运行 ⇒ 见 tasks.md Task 2 |

## Non-Goals

1. **不**引入「双工字段」的通用机制（那属**新能力**）
2. **不**改其它 bundle（`ch_stream` 的 `payload` 不构成问题 —— 见 Why 的清点）
3. **不**改 `ch_mem` 的 `aread`/`sread` 时序（已由
   `sim-mem-read-timing-align-2026-10` 对齐）
4. **不**把 `fifo_bundle` 拆成读写两个 bundle（除非 ADR 裁决形态 B）

## 落地顺序

MUST 在 `bundle-ecosystem-hardening-2026-10` **之后**落地（它把 prefix / 连接
语义 / 序列化路径都收口了 ⇒ 本 change 在**稳定的地基**上改布局）。

⚠️ **建议排在 `examples-runtime-coverage-2026-10` 与
`disabled-samples-remediation-2026-10` 之后**：
- 它会改 `samples/fifo_bundle.cpp` / `minimal_fifo_bundle.cpp` 的**运行行为**
  ⇒ 需要 examples/samples 侧的运行护栏**已就位**（否则改坏了没人知道）；
- 它是三者中唯一的**公开 API 布局变更**（🔴 高风险）⇒ 应当在其余两个
  **低风险、机械性**的改动稳定下来之后再动 ⇒ 归因更容易；
- ⚠️ 若 `disabled-samples-remediation` 采纳了「删源文件」的归宿 ⇒
  `samples/*.cpp` 的总数会变 ⇒ 本 change 的 `samples/` 侧改动 MUST 排在它之后
  （避免两边同时动 `samples/CMakeLists.txt`）。

⇒ **推荐总序**：① examples-runtime-coverage → ② disabled-samples-remediation
→ ③ fifo-read-data-path（三者可并行**开发**，但 MUST 串行**合并** ——
都写根 `AGENTS.md`）。
