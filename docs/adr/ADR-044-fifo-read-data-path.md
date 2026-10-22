# ADR-044: `fifo_bundle` 的读数据通路（新增独立的 `rdata` 字段）

- Status: Accepted
- Date: 2026-10-22
- Author: build-agent
- Parent: ADR-014（bundle 反射系统）、ADR-042（bundle 字段改为一等节点）、ADR-043（bundle 拼接/切片设施移除）
- OpenSpec change: `fifo-read-data-path-2026-10`（capability `bundle-master-slave-io`）

## Context

`fifo_bundle<T>`（`include/bundle/common_bundles.h`）是仓库里唯一一个
「**单总线、双工数据**」的 bundle —— 它的 `data` 字段被**读数据**和**写数据**
**共用**：

```cpp
CH_BUNDLE_FIELDS_T(data, push, full, pop, empty)

void as_master_direction() {
    this->make_output(data, push, pop);   // master 驱动写数据
    this->make_input(full, empty);
}
void as_slave_direction() {
    this->make_input(data, push, pop);    // slave 接收写数据
    this->make_output(full, empty);
}
```

⇒ **slave 侧无处输出读到的数据** ⇒ 该协议的**读通路根本不存在**。

`samples/fifo_bundle.cpp` 是这个缺陷的活标本 —— 它算出 `data_out` 之后
**直接丢弃**：

```cpp
auto data_out = mem.aread(rd_a);
// 注意：io.data 是输入，不能赋值，我们只需要使用它作为输入
// 数据输出通过其他方式提供，这里我们不直接连接 io.data
```

⚠️ 这个缺陷此前**不可见**，是因为 2026-10-22 的 `bundle-master-slave-io-2026-10`
（ADR-042）只修掉了「只读字段恒为 0」那一条，**没碰**「读数据通路缺失」这一条
⇒ 该 sample 现在**绿**了，却仍然**没有实现 FIFO 的读通路**。
这正是仓库自己的元教训：「**绿 ≠ 对**」—— 护栏只能守住「已定义的行为」，
守不住「**从未被定义**的行为」。

## Decision

### 1. 形态：加一个独立的 `rdata` 字段（形态 A）

| 形态 | 结论 |
|---|---|
| **A. 加一个 `rdata` 字段** | ✅ **采纳** —— 与 AXI 的「分离读写通道」方向一致，改动面最小 |
| B. 拆成两个 bundle（`fifo_wr` / `fifo_rd`） | ❌ 否决 —— 语义最干净，但破坏面远大于 A（9 个引用文件的连接点全部重写，`connect()` 调用从 1 次变 2 次），且 `fifo_bundle` 这个名字消失会让 ADR-042 的主 spec 叙述需要更大手术 |
| C. 保持单字段、用「方向」表达双工 | ❌ 不可行 —— 这正是缺陷本身 |

`fifo_bundle` 随之变为 **6 个字段**：

```cpp
CH_BUNDLE_FIELDS_T(data, push, full, pop, empty, rdata)
```

方向指认：

| 字段 | master | slave |
|---|---|---|
| `data`（写数据） | output | input |
| `push` / `pop` | output | input |
| `full` / `empty` | input | output |
| **`rdata`（读数据）** | **input** | **output** |

⇒ `rdata` 在两侧**互为镜像**（与 ADR-042 确立的「master/slave MUST 镜像」一致）。

### 2. `rdata` 的字段位置：**追加到末尾**

```cpp
CH_BUNDLE_FIELDS_T(data, push, full, pop, empty, rdata)
```

| 位置 | 位偏移后果 | 结论 |
|---|---|---|
| **追加末尾** | `data`(0..W-1) / `push`(W) / `full`(W+1) / `pop`(W+2) / `empty`(W+3) **全部不变**；`rdata` 占 W+4 .. W+4+W-1 | ✅ **采纳** —— 既有 5 个字段的偏移**完全不变** ⇒ 对外位语义破坏最小，`tests/test_bundle_master_slave.cpp` 的偏移常量只需**追加**而非重排 |
| 插在 `data` 之后 | `push`/`full`/`pop`/`empty` 全部 +W ⇒ 既有偏移常量**全部作废** | ❌ 否决 —— 无谓地破坏既有位语义 |

⇒ **总宽**：`W+4` → `2W+4`。

### 3. 读通路的时序：`aread`（0-cycle 组合）

`ch_mem::aread` 在 `sim-mem-read-timing-align-2026-10` 之后已是 **0-cycle**
（与生成的 Verilog `assign dout = mem[rd_ptr];` 完全一致）⇒ interpreter 与
Verilator 逐拍对齐 ⇒ **采纳 `aread`**。

⚠️ `sread`（1-cycle 同步）会把读数据推迟一拍，且需要额外的 valid 约定
⇒ 本次**不**采用。

### 4. `empty` 时 `rdata` 是 **don't-care**

组合读时 `rdata = mem[rd_ptr]`；FIFO 为空时 `rd_ptr` 指向一个**陈旧/未定义**
的槽位 ⇒ `rdata` 的值**无意义**。

⇒ **契约**：消费者 MUST 只在 **`!empty`** 时采样 `rdata`。
⇒ 实现 MUST NOT 在 `empty` 时把 `rdata` 强制为 0（那会引入一个**假的**
「数据是 0」的语义，掩盖 FIFO 空的真实状态）。

### 5. 「pop 沿前采样」规则

`aread` 是 0-cycle ⇒ `dout`（即 `rdata`）在 pop 沿**之后**立即指向
**下一个**队首 ⇒ 消费者 MUST 在 pop 沿**之前**采样 `rdata` 才能拿到**正在
被弹出**的那个元素。

（与 `samples/fifo_example.cpp` 的既有注释、以及
`docs/simulation/ARCHITECTURE.md` 的「Memory read port timing」一致。）

### 6. 打包 API 的宽度边界：`W ≥ 31` 时打包式 API 不可用

`Simulator::set_bundle_value()` / `get_bundle_value()` 是**打包式** API，
总宽 > 64 位时**显式拒绝**（`include/simulator.h` 的 `CHERROR` + return，
**不**静默截断）。

| | 现状 | 加 `rdata` 后 |
|---|---|---|
| 总宽 | `W+4` | `2W+4` |
| 打包 API 可用的最大 `W` | **60** | **30** |

⇒ **`W ∈ [31, 60]` 从「可用」变成「显式报错」** —— 这是一处**能力回退**。

⇒ **裁决：接受该边界，但 MUST 显式记录**：
1. 本 ADR 记录该边界；
2. capability `bundle-master-slave-io` 的「打包式 bundle API 的适用范围
   MUST 写明」Requirement MUST 补上「`fifo_bundle` 的 `W ≥ 31` 落在该
   适用范围之外」；
3. 逐字段的槽位读写（`get_bundle_field_value()` / 逐字段驱动）**不受**该
   限制 ⇒ 大位宽 `fifo_bundle` 的消费者 MUST 走逐字段路径。

⚠️ 该回退**不**通过「缩小 `fifo_bundle` 的可用 `W`」来规避 —— 那会让
「加一个读数据字段」这件事变成「让某类 `fifo_bundle` 直接不能用」。

## Consequences

### 正面

- `fifo_bundle` 的**读通路第一次被定义**：协议（谁在什么时候能看到什么）、
  方向、时序、边界语义全部写死；
- 与 AXI 的「分离读写通道」同构 ⇒ 后续若要支持 AXI 风格的 FIFO bundle，
  形态是一致的；
- `rdata` 追加在末尾 ⇒ 既有 5 个字段的位偏移**零变化**。

### 负面 / 代价

- **公开 API 的布局变更**：`fifo_bundle` 从 5 字段变 6 字段 ⇒
  - 4 处硬编码的宽度/字段数断言 MUST 同步
    （`tests/test_bundle_bitstream.cpp:85`/`:99` 的 `== 8` → `== 12`；
    `tests/test_bundle_advanced_features.cpp:75` 的
    `STATIC_REQUIRE(... == 5)` → `== 6`（**编译期**断）；
    `tests/test_bundle_ecosystem.cpp:487` 的 `== 5` → `== 6`）；
  - `tests/test_bundle_master_slave.cpp:178-184` 的位偏移常量**追加**
    `kFifoRdataBit` / `kFifoRdataBits`（既有 5 个**不变**）；
  - `samples/bundle_advanced_features.cpp:79` **打印**的宽度 12 → 20
    （可观测输出变化）。
- **打包 API 的能力回退**（见裁决 6）。
- `samples/fifo_bundle.cpp` 的实现 MUST 改（`rdata` ← `mem.aread(rd_a)`）
  且 MUST **写死期望值表**（该 sample 是 `tick()` **之后**才采样 ⇒ 极易差
  一拍）。

## Alternatives rejected

| 备选 | 否决理由 |
|---|---|
| 形态 B（拆两个 bundle） | 破坏面远大于 A（9 个引用文件的连接点全部重写），收益仅是「语义更干净」 |
| 形态 C（单字段双工） | 这正是缺陷本身 |
| `rdata` 插在 `data` 之后 | 无谓地作废既有 5 个字段的位偏移 |
| `sread`（1-cycle） | 与 `aread` 的 0-cycle Verilog 语义不一致 ⇒ interpreter/Verilator 逐拍不对齐；且需要额外 valid 约定 |
| `empty` 时强制 `rdata = 0` | 引入「数据是 0」的**假**语义，掩盖 FIFO 空的真实状态 |
| 缩小 `fifo_bundle` 的可用 `W` 以保住打包 API | 把「加一个读数据字段」变成「让某类 `fifo_bundle` 直接不能用」 |
| 用「双工字段」的**通用机制** | 属**新能力**（非本 ADR 范围）；且会把 `axi_lite_bundle` 等**读写分离**的 bundle 变成「违规」⇒ 通用规则不成立（见 capability delta 的作用域限定） |
