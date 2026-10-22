# Change: fifo-read-data-path-2026-10 — `fifo_bundle` 的**读数据通路**

- Status: **草案（尚未实施）**
- 立项自 2026-10-22 的 `bundle-ecosystem-hardening-2026-10`（其 L10，用户裁决**不在该 change 处理** ⇒ 独立立项）
- 关联 capability：`bundle-master-slave-io`（**可能**需要 MODIFIED —— 见 D3）

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
ADR-042 / ADR-043）。

**候选形态**（MUST 在 ADR 里逐项裁决）：

| 形态 | 说明 | 代价 |
|---|---|---|
| **A. 加一个 `rdata` 字段** | master: `data` OUT / `rdata` IN；slave: `data` IN / `rdata` OUT | 位宽 +N；**破坏**现有 `fifo_bundle` 用户的位布局假设 |
| **B. 拆成两个 bundle**（`fifo_wr` / `fifo_rd`） | 读写分离 | 破坏性更大；但语义最干净（与 AXI 的分离读写通道一致） |
| **C. 保持单字段，用「方向」表达双工** | `data` 在 master 侧是 OUT、在 slave 侧是 **IN**（现状）⇒ slave 无法输出 | ❌ **不可行** —— 这正是缺陷本身 |

**倾向**：**A**（加 `rdata`）—— 与 AXI 的「分离读写通道」方向一致，但改动面小。

⚠️ **连带影响**（MUST 在 ADR 里清点）：
- `fifo_bundle` 的**位宽**从 `W+4` 变成 `2W+4` ⇒ 影响
  `get/set_bundle_value` 的打包布局、`tests/test_bundle_master_slave.cpp` 的
  位偏移常量（`kFifoPushBit` 等）；
- `samples/fifo_bundle.cpp` 的实现 MUST 改（`rdata` ← `mem.aread(rd_a)`）；
- `tests/test_bundle_master_slave.cpp` 的 DUT（MUST 读 `rdata`）与断言。

### D2: 读通路的**时序** MUST 与「FIFO 协议」一致

- `aread`（0-cycle，组合）vs `sread`（1-cycle，同步）—— FIFO 的**读数据**用哪个？
  ⚠️ 现状 sample 用 `aread`；而 `ch_mem` 的 `aread` 在 2026-10-21 的
  `sim-mem-read-timing-align-2026-10` 后已是 0-cycle（与 Verilog `assign` 一致）✓
- ⚠️ **FIFO 协议**：`dout` 在 pop 沿**之后**指向**下一个**队首 ⇒ 消费者 MUST 在
  pop 沿**之前**采样 `dout`（`samples/fifo_example.cpp` 的注释已记录这条）
  ⇒ 读数据通路的 ADR MUST 写明这条协议。

### D3: capability 的 delta

若选了形态 A ⇒ `fifo_bundle` 的字段集变了 ⇒ `bundle-master-slave-io` capability
里**提到字段布局**的 Requirement MUST MODIFIED（完整重述 + 全部 Scenario）。
⚠️ `bundle-base-integer-ctor` 的派生类**清单**（20 个）**不变**（`fifo_bundle`
还在）⇒ 不需要该 capability 的 delta。

## Impact

| 项 | 影响 |
|---|---|
| `include/bundle/common_bundles.h` | `fifo_bundle` 加 `rdata` 字段（**公开 API 布局变更**） |
| `docs/adr/ADR-044-*.md` | **新增**（形态裁决 + 时序协议） |
| `samples/fifo_bundle.cpp` | 实现读数据通路 |
| `tests/test_bundle_master_slave.cpp` | 位偏移常量 + DUT + 断言（**读数据必须真的到达 master 侧**） |
| `openspec/specs/bundle-master-slave-io/spec.md` | MODIFIED（方向指认 Requirement —— 本 change 的 delta 已起草） |
| 计数 | ⚠️ **CTEST_TOTAL 可能 +1**（若新增读通路的专门用例） |

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
> **另有两处消费方**（实测 `fifo_bundle` 共 **8** 个文件 / 45 处引用）：
> - `samples/bundle_advanced_features.cpp:79` **打印** bundle 的宽度 ⇒ 打印值会变
>   （12 → 20）⇒ 该 sample 的输出随之变化（无断言，但属可观测行为变化）；
> - `samples/minimal_fifo_bundle.cpp` 是一个 FIFO 消费方（只接 `full`/`empty`，
>   **不接读数据**）⇒ 加了 `rdata` 后它**必须**决定是否接线 ⇒ 否则新字段恒 0。
| 风险 | 🔴 **高**。① **公开 API 布局变更**（位宽变化 ⇒ 打包布局变化 ⇒ 既有断言的位偏移常量要改）；② 时序协议（FIFO 的 `dout` 语义）容易再次出错；③ 与 ADR-042/043 的「字段一等节点」模型必须保持一致 |

## Non-Goals

1. **不**引入「双工字段」的通用机制（那属**新能力**）
2. **不**改其它 bundle（`ch_stream` 的 `payload` 不构成问题 —— 见 Why 的清点）
3. **不**改 `ch_mem` 的 `aread`/`sread` 时序（已由
   `sim-mem-read-timing-align-2026-10` 对齐）
4. **不**把 `fifo_bundle` 拆成读写两个 bundle（除非 ADR 裁决形态 B）

## 落地顺序

MUST 在 `bundle-ecosystem-hardening-2026-10` **之后**落地（它把 prefix / 连接
语义 / 序列化路径都收口了 ⇒ 本 change 在**稳定的地基**上改布局）。
