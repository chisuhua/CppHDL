# Design: riscv-mini 两个 cache wrapper 的组合逻辑真正发射

- **change**：`riscv-mini-cache-wrapper-live-logic-2026-10`（**有** spec 变更 ⇒ 归档 **禁** `--skip-specs`）
- **架构依据**：无（**修 bug** —— 让组合逻辑真发射；不引入新机制 ⇒ 不需要 ADR）
- **5 段草案**：`.rddf/improvements/riscv-mini-cache-wrapper-live-logic-2026-10.md`
- **本文件定位**：**实施设计**（范围裁决 + 驱动表 + 变异计划）

## 1. 缺陷机制（已实测）

`include/cpu/cache/{i_cache_prefetch,d_cache_write_buffer}.h` 用**普通 C++ `if`** 包裹
`ch_bool` 条件来发射 RTL：

```cpp
if (io().req == ch_bool(true) && io().we == ch_bool(true)) {
    io().ready <<= ch_bool(true);      // ← 这行**从不发射**
}
```

`ch_bool` 有 `explicit operator bool()` ⇒ C++ `if` 拿到的是「**当前节点是否非零**」，
而 `describe()` 在**建图期**执行 ⇒ 该条件在建图期为**假** ⇒ **分支体永不发射**。

⚠️ 仓库内**全部 5 处**这样的 `if`（**全部**命中 spec delta 的 grep 护栏）：

| 文件 | 行 | 条件 | 分支内**从不发射**的驱动 | **含驱动**？ |
|---|---|---|---|---|
| `i_cache_prefetch.h` | 100 | `cache_miss && io().axi_ready` | `io().axi_addr` / `io().axi_valid` | 是（2 个） |
| `i_cache_prefetch.h` | 133 | `io().axi_resp_valid` | （空体，仅注释） | **否**（空体） |
| `d_cache_write_buffer.h` | 97 | `io().req && io().we` | `io().ready` | 是（1 个） |
| `d_cache_write_buffer.h` | 121 | `!axi_busy && io().axi_ready` | `io().axi_valid` | 是（1 个） |
| `d_cache_write_buffer.h` | 133 | `io().axi_resp_valid` | （空体，仅注释） | **否**（空体） |

⚠️ **计数口径（如实登记，避免与根 `AGENTS.md §NOTES` 漂移）**：
**5 处 `if`** 里只有 **3 处含驱动**（另 2 处是**空体**、仅注释）⇒
「含驱动的建图期 `if`」= **3 处** ⇒ 根 `AGENTS.md` 的 NOTES 按 **3 处**计。
（行号为**修复前**的行号；修复后两个 wrapper 的 C++ `if` 数量 = **0**。）

## 2. ⚠️ 范围裁决：wrapper 做到**哪一层**

### 2.1 三个候选

| 层次 | 内容 | 判定 |
|---|---|---|
| **(a) 最小** | 只把 `if` 换 `select`，让 `ready` / `axi_valid` 变活 | ❌ **不可接受** |
| **(b) 直通式** | (a) + **补齐所有未驱动的输出**（`axi_addr`/`axi_wdata`/`axi_we`）+ **驱动内层 cache 的全部输入** | ✅ **采纳** |
| **(c) 真 FIFO** | 用 `ch_mem` 实现真正的写缓冲队列（head/tail/occupancy/full/empty） | ⏸ **不做**（= 独立 feature） |

### 2.2 为什么 (a) 不可接受（**这是本 design 最重要的一条**）

现状（实测）：

| 输出 | 现状 | 只做 (a) 之后 |
|---|---|---|
| `ready` | 未驱动 | ✅ 变活 |
| `axi_valid` | 未驱动 | ✅ 变活 |
| `axi_addr` | **未驱动**（`d_cache_write_buffer.h:124` 的驱动是**注释掉的**） | ❌ **仍未驱动** |
| `axi_wdata` | **未驱动**（`:125` 同样是注释掉的） | ❌ **仍未驱动** |
| `axi_we` | **从未被驱动过**（端口存在，但 `describe()` 里**一次都没出现**） | ❌ **仍未驱动** |

⇒ (a) 会让 wrapper 发出一个「**`axi_valid`=1 但地址=0、写使能=0**」的 AXI 写请求
⇒ **比「恒未驱动」更危险**（下游可能把它当成一次合法事务）。

⇒ 故必须补齐（b）。

### 2.3 为什么 (c) 不在本 change

(c) 是**新增功能**（真正的缓冲队列 ⇒ 引入 `full`/`empty` 语义、合并策略、
背压）⇒ 与本 change 的性质（「**修 bug**，让已有端口真发射」）不同
⇒ 与 proposal 的 Non-Goal「不做 wrapper 的**性能**优化」一致
⇒ **记入 backlog**（见 §7）。

### 2.4 (b) 的语义契约（**本 change 的实现面**）

**原则：每个输出端口 MUST 被**无条件**驱动**（条件体现在 `select` 的取值里）
⇒ 否则条件为假时又变回「未驱动」⇒ 违反本 change 的 Requirement。

`ICachePrefetch`：

| 端口 | 驱动 |
|---|---|
| `data` | `icache.io().data`（**已**无条件驱动） |
| `ready` | `icache.io().ready`（**已**无条件驱动） |
| `miss` | `icache.io().miss`（**已**无条件驱动） |
| `axi_addr` | `icache.io().axi_addr` —— **转发内层** |
| `axi_valid` | `icache.io().axi_valid` —— **转发内层** |

⚠️ `ICachePrefetch` 的 `ready` **本来就**是无条件驱动 ⇒ 本 change **不需要**动它
（初稿 proposal 把它与 `DCacheWriteBuffer::ready` 并列，是**不准确**的）。

#### ⚠️ 为什么 AXI 主端 MUST **转发**内层，而不是在 wrapper 层重推导

内层 `ICacheSync` **已经**是一个完整的 AXI 主端
（`include/cpu/cache/i_cache_sync.h:164-165`）：

```cpp
io().axi_valid <<= select(miss, ch_bool(true), ch_bool(false)) & io().req;
io().axi_addr  <<= icache_get_block_addr(io().addr);
```

⇒ 三条理由（**架构级**，故在此裁决）：

1. **AXI 协议合规**：内层的 `axi_valid` **不含** `axi_ready`（AXI 要求
   「VALID **不得**等 READY 才拉高」）。若 wrapper 重推导成
   `select(miss && io().axi_ready, io().req, false)` ⇒ **把 READY 引进 VALID**
   ⇒ **违反 AXI**。
2. **无死逻辑**：wrapper 重推导 ⇒ 内层的 `axi_addr` / `axi_valid` **无人消费**
   ⇒ 违反 ZERO-DEBT「无死代码」。
3. **值同源**：内层的 `icache_get_block_addr`（`i_cache_sync.h:195-197`，
   `addr & 0xFFFFFFF0_h`）与 wrapper 的 `get_block_addr`（`i_cache_prefetch.h`
   的 `addr & 0xFFFFFFF0_h`）**逐位相同** ⇒ 转发**不改变值**，只是消除重复实现；
4. **策略漂移防护**（2026-10-28 审稿第 1 轮补）：理由 3 是**快照式**论证
   （两个块地址算法**今天**逐位相同）；**结构性**理由是：
   **「什么算 miss」与「块地址怎么算」是内层 cache 的策略** ⇒ wrapper 重推导
   = **复制**它们 ⇒ 本 §7 已登记一条**会改内层 `axi_addr` 时序**的待裁决项
   ⇒ 那条一旦落地，wrapper 的重推导版本就会与内层**静默分叉**
   （请求的块 ≠ 内层会填的块 ⇒ **静默数据损坏**）⇒ **转发**让二者**结构上恒等**，
   策略变更自动继承。

⚠️ **反例（如实登记，不推翻裁决）**：转发会把 wrapper 顶层 AXI 主端与内层的
`miss` **耦合** —— `miss` 是 **1 拍陈旧**的 ⇒ `(axi_valid, axi_addr)` 在消费方
换地址时**不自洽**（`valid` 说的是上一拍的 miss，`addr` 说的是当拍地址）。
⇒ 「重推导」**理论上**能给出自洽的一对，但 wrapper **拿不到** `hit`
（内层只有 `miss` 没有 `hit`）⇒ **重推导不可行** ⇒ 裁决成立；
⇒ **代价**（消费方必须保持 `addr` 直到 `ready`）MUST 写进契约
⇒ 已写入 spec delta 的「AXI 主端的 `axi_valid` MUST NOT 依赖 `axi_ready`」
Scenario 的**读侧**子句。

⚠️ **已知边界（如实登记）**：内层的 `miss` 来自 `sread`（**1 拍**注册读）
而 `axi_addr` 取**当拍**地址 ⇒ 二者存在**1 拍错位**（CPU 换地址时会去取一个
「没 miss 的 block」）。这是**内层 cache 自身**的既有问题（cache 本体不在本 change
范围 ⇒ Non-Goal「不改 cache 控制器本体」）⇒ **转发不加剧它**，也不修它
⇒ 登记进 §7 backlog。

⚠️ **内层 cache 的输入 MUST 全部驱动**（现状：5 个输入端口**全部未驱动**）：

| 内层端口 | 现状 | 应驱动为 |
|---|---|---|
| `icache.io().addr` | ❌ 未驱动（只被**读**：`next_line_addr`） | `io().addr` |
| `icache.io().req` | ❌ 未驱动 | `io().req` |
| `icache.io().axi_ready` | ❌ 未驱动 | `io().axi_ready` |
| `icache.io().axi_data` | ❌ 未驱动 | `io().axi_data` |
| `icache.io().axi_resp_valid` | ❌ 未驱动 | `io().axi_resp_valid` |

⇒ 现状下内层 cache 的 `miss` **恒为「冷 miss」**（`addr` 恒 0）
⇒ 接上后 `hit`/`miss` 才**可区分**。

`DCacheWriteBuffer`：

| 端口 | 驱动 |
|---|---|
| `ready` | `io().req & io().we & io().axi_ready` —— 语义 =「**本次写已被 AXI 接受**」 |
| `axi_valid` | `io().req & io().we` —— ⚠️ **不含 `axi_ready`**<br>⚠️ **2026-10-28 实施期订正**：初稿写 `select(io().req & io().we, ch_bool(true), ch_bool(false))` ⇒ **改为直接的 `&`** ⇒ 理由见 §8.1 |
| `axi_addr` | `io().addr` |
| `axi_wdata` | `io().wdata` |
| `axi_we` | `io().we` |

⚠️ **`axi_valid` MUST NOT 依赖 `axi_ready`**（与上面 §2.4 的**理由 #1** 同一纪律）：
AXI 要求「VALID **不得**等 READY 才拉高」⇒ 若 `axi_valid` 里含 `axi_ready`，
则 `axi_ready` 为低时该写**根本不出现在 AXI 总线上**；而 `DCacheWriteBuffer`
**无任何状态/队列**可保留它（`WriteBufferEntry buffer[4]` 在直通式下是**死数据**）
⇒ **写被静默丢弃**。

⚠️ **`ready` 的契约（防止丢写）**：
`ready` = `req & we & axi_ready` ⇒ 它在「AXI 握手成立」的那一拍才拉高
⇒ **消费方（CPU）MUST 保持 `req` / `we` / `addr` / `wdata` 直到 `ready` 拉高**
（AXI 主端的既有义务）⇒ 配合「`axi_valid` 不等 READY」，
写**必然**在某一次 `axi_ready` 为高时被接受 ⇒ **不丢写**。

⚠️ **已知语义债务（如实登记）**：直通式下 `DCacheWriteBuffer` **并不缓冲**
（名字 over-promise）⇒ 记入 backlog（§7），**不在**本 change 解决。
⚠️ `ready` 的语义是「**此刻**可完成握手」，**不是**「buffer 有空位」⇒
真 FIFO 引入后 `ready` 才有背压语义。

## 3. 连带断言处置（**同 commit**，ADR-048 D11 同型纪律）

`examples/riscv-mini/tests/test_cache_smoke.cpp` 钉住了「未驱动 ⇒ 读 0」：

```cpp
REQUIRE(to_bool(sim.get_port_value(dev.instance().io().ready)) == false);      // ← 会转红
REQUIRE(to_bool(sim.get_port_value(dev.instance().io().axi_valid)) == false);  // ← 会转红
```

⇒ **必须反转**成「随输入变化」（`ready`/`axi_valid` == true）⇒ 否则修复后**必然转红**。
⚠️ 该文件**头部注释**也钉住了旧事实（「缺陷修好后本断言会转红 ⇒ 那时应连同
proposal §登记的框架缺陷 #5 一起更新」）⇒ **注释 MUST 同步更新**。

⚠️ 另有一条**可能**转红的断言：`ICachePrefetch instantiation + cold-miss surfaced`
钉住 `miss == true`。接上 `addr`/`req` 后，内层 `ICacheSync` 是 **1-cycle `sread`**
⇒ 首次访问仍是 miss ⇒ 预期仍为 true，**但 MUST 实测确认**（不可假定）。

## 4. 变异验证（每条先确认**变异后仍编译成功** ⇒ 否则跑旧二进制 ⇒ 假绿）

| # | 变异 | 预期转红 | 抓的是 |
|---|---|---|---|
| **M1** | 把 `select(...)` 退回 C++ `if` | 全部活性断言 | **「建图期 C++ `if`」这个形态本身**（本 change 的核心判据） |
| **M2** | ⚠️ 把 `axi_addr` 的驱动改成常量 `0` | `axi_addr` 断言 | **地址真的被驱动**（初稿的「M2 删掉驱动」**前提不成立** —— 驱动是**新增**的，没有可删） |
| **M3** | 把 `ready` 的条件取反 | `ready` 断言 | 条件语义 |
| **M4** | ⚠️ 把内层 cache 的 `addr`/`req` 驱动改成**常量 0**（**不是**「断开驱动」—— 见 §8.2） | `axi_addr` / `axi_valid` 断言 | 「内层 cache 被真正驱动」 |
| **M5** | ⚠️ 把 `axi_valid` 的条件**加上** `io().axi_ready` | **READY 依赖的判别力用例**（§2.4 的用例 2.2） | **AXI「VALID 不得等 READY」** ⇒ 堵住「静默丢写」 |
| **M6** | ⚠️ 去掉内层的块对齐（`axi_addr` 改为直传 `addr`）—— ⚠️ 变异的是**内层 cache**（不在本 change 的实现范围）⇒ 只作**临时变异**证明测试有判别力 ⇒ 事后 MUST 还原 | `axi_addr` 断言（`ICachePrefetch`） | **「块地址」这条契约**（§5 的 Scenario 3） |
| **M7** | ⚠️ 给 `DCacheWriteBuffer::axi_addr` **错加**块对齐 | `axi_addr` 断言（`DCacheWriteBuffer`） | **「直通（不对齐）」这条契约**（与 M6 **相反**的语义） |

⚠️ **M6 / M7 为什么必需**（2026-10-28 审稿第 1 轮）：初稿的地址断言全部用
**已对齐**地址（`0x80000040` / `0x2000`）⇒ 「对齐」与「不对齐」这**两种相反**的
语义**给出同一个值** ⇒ **零判别力**（变异「去掉内层对齐」或「给 DWB 加上对齐」
都**不会**转红）⇒ 断言改用**不对齐**地址（`0x80000042` / `0x2003`）⇒
M6 与 M7 各自转红 ✓（实测）。

⚠️ **M5 是必需的**：现有双后端用例把 `axi_ready` **恒驱 1** ⇒ READY 依赖
**完全不可观测** ⇒ 没有 M5 对应的用例，M1–M4 全部抓不到「AXI 写被静默丢弃」。

⚠️ M2 的形式订正：初稿写「**删掉** `axi_addr` 的驱动 ⇒ 转红」⇒ **不成立**
（现在**根本没有**驱动可删 ⇒ 修复是**新增**驱动）⇒ 改为「**改成常量 0**」。

## 5. 规格

- `cpu-cache-subsystem` **MODIFIED ×1**（路径订正：ADR-046 收敛后的真位置）
  ⚠️ **MODIFIED 会整体替换 ⇒ 原标题与全部条款 MUST 逐字保留**
  （初稿曾漏抄 `ICacheComplete IO signature` 的 6 个端口
  ⇒ 「`miss` / `axi_addr` / `axi_valid` / `axi_ready` / `axi_data` / `axi_resp_valid`」
  ⇒ 与本 change 新增的「内层 cache 输入 MUST 全部驱动」**自相矛盾** ⇒ 已补回）
- `cpu-cache-subsystem` **ADDED ×1**（wrapper 的组合逻辑 MUST 以 `select` 发射
  ⇒ 输出 MUST 随输入变化）+ **5 个 Scenario**（见 delta）：
  1. 输出 MUST 随输入变化（**逐个输入 → 逐个输出**的粒度）
  2. ⚠️ **每个输出端口 MUST 被驱动**（不得恒为未驱动）—— 覆盖
     `axi_addr`/`axi_wdata`/`axi_we` 这三个「连 (a) 都覆盖不到」的口子
  3. ⚠️ **`axi_addr` MUST 是被请求地址的块地址** —— 把「地址**被驱动**」
     升级为「地址**正确**」（否则「驱动成垃圾值」仍满足契约）
  4. ⚠️ **内层 cache 的输入端口 MUST 被全部驱动** —— 覆盖 §2.4 的 5 个未驱动输入
  5. 建图期 C++ `if` MUST 被 grep 拦住（正则用 `[^_]if` ⇒ 不误伤 `_if(...)`）

## 6. 实施顺序（**同 commit**）

1. 改两个 wrapper（驱动表 §2.4）⇒ 5 处 `if` 全部消失；
2. 反转 `test_cache_smoke.cpp` 的 2 条断言 + 更新其头部注释；
3. **实测** `ICachePrefetch` 的 `miss == true` 断言是否仍绿（§3）；
4. **CMake 三属性**：`riscv_mini_cache_smoke` 加 `verilator_e2e` 标签 +
   `RESOURCE_LOCK verilator_backend` + verilator PATH 注入
   （照 `test_rv32i_soc_dual_parity` 的 `examples/riscv-mini/CMakeLists.txt:113-118`）
   ⇒ **双后端用例**；
5. **计数锚点**：加 `verilator_e2e` 标签 ⇒ `ctest -L verilator_e2e` **37 → 38**
   （`CTEST_TOTAL` **不变** —— 该 target **已**注册进 CTest，只是加标签）
   ⇒ MUST 同步根 `AGENTS.md` 的 `verilator_e2e 37` 锚点
   （`scripts/verify_test_counts.sh` 会拦）；
6. **两个 wrapper 的头注释** MUST 改为如实描述（§7 的第 1 条）；
7. 变异 M1–M4；
8. 回归 + 计数核对。

## 7. Backlog（本 change **不做**，如实登记）

- **两个 wrapper 的头注释仍是假的**（`i_cache_prefetch.h:1-18` 声称「预取下一个 line /
  预取缓冲 2 条目」；`d_cache_write_buffer.h:1-24` 声称「写缓冲队列 4 条目 / 写合并 /
  AXI 仲裁」）⇒ 直通式下这些**均未实现** ⇒ 本 change **MUST 改写头注释**
  （ZERO-DEBT「无未文档化变更」：头注释是这些文件的活文档）⇒ 任务见 `tasks.md` §6；
- **`DCacheWriteBuffer` 并不真缓冲**（直通式 ⇒ 名字 over-promise）⇒ 真 FIFO 队列
  ⇒ 独立立项（§2.3 的 (c)）；届时 `ready` 才有背压语义（§2.4 的 `ready` 语义说明）；
- **内层 cache 的 `miss`（1 拍 `sread`）与 `axi_addr`（当拍地址）存在 1 拍错位**
  （§2.4）⇒ 属**内层 cache 自身**的问题 ⇒ **独立立项**
  （且需要架构裁决：是否把 `axi_addr` 改成 1 拍对齐）。
  ⚠️ **2026-10-28 审稿第 1 轮升级（后果被初稿低估）**：
  初稿写「转发**不加剧**、也不修它」—— **不准确**，两处都要改：
  ① **后果不止是「取了一个没 miss 的 block」** —— 内层的 fill 路径用**当拍**的
     `index` / `tag` 写入，而 `hit` 判定用的是**上一拍** `sread` 到的 tag
     ⇒ 消费方在 miss 落定前换 `addr` ⇒ **上一个地址的数据被写进当前地址的 entry**
     ⇒ **静默返回错误数据**（比「多取一个 block」严重一个量级）；
  ② **转发不是「不加剧」，而是「首次暴露」** —— 修复前 wrapper 顶层 `axi_valid`
     **恒为 0** ⇒ 下游**永远看不到**内层的 AXI 请求 ⇒ 该错位**不可达**；
     本 change 把内层 AXI 主端**接到顶层** ⇒ 错位从「不可达」变「**可达**」
     ⇒ 当前**无仓内消费者**（两个 wrapper 的唯一消费者是
     `examples/riscv-mini/tests/test_cache_smoke.cpp`）⇒ 仍是**潜伏**，
     但**契约必须写出来** ⇒ 已写入 spec delta 的读侧「保持 `addr` 直到 `ready`」
     子句（**唯一可行的缓解**）。
- `ICachePrefetch` 的**预取缓冲**（`prefetch_data`/`prefetch_addr`/`prefetch_valid`
  三个 `ch_mem` 缓冲区）目前是 `(void)` 掉的**死数据** ⇒ 真预取 ⇒ 独立立项；
- `d_cache_write_buffer.h` 的 `WriteBufferEntry buffer[4]`（普通 struct 数组）
  在直通式下**仍无消费者** ⇒ 应随真 FIFO 一起清理（或删除）。
- ⚠️ **2026-10-28 审稿第 1 轮补登（本次实施新产生 / 新暴露的债）**：
  ① **`DCacheWriteBuffer::axi_resp_valid` 现在完全无消费者** —— 此前唯一的读点
     在那段死 `if` 里 ⇒ **AXI B 响应被忽略** ⇒ `ready` 只是 **W 握手成立**，
     **不是**「写已提交」⇒ 消费方**不得**据此认为写已落盘
     ⇒ 已写进 `d_cache_write_buffer.h` 的头注释；
  ② **`DCacheWriteBuffer` 没有内层 cache、也没有读通路** —— 它是**纯 AXI 写
     直通 shim**（`__io` 里**没有** `rdata` / `axi_rdata`）⇒ 根
     `include/cpu/AGENTS.md` 的 STRUCTURE / KEY FILES 表原写「instance DCacheSync」
     是**假陈述** ⇒ 已订正；若要让它成为真正的 D-Cache wrapper
     （加读通路 + 实例化内层）⇒ **另一个 change**；
  ③ `d_cache_write_buffer.h` 的 `#include "d_cache_complete.h"` 是**未使用的
     include**（ZERO-DEBT「无死代码」）⇒ **本次已删**（唯一的消费者是
     `test_cache_smoke.cpp`，它**不**依赖该传递 include）；
  ④ `ICachePrefetch` 的三个预取 `ch_mem`（`prefetch_data` / `prefetch_addr` /
     `prefetch_valid`）在图里同样产生**死节点** ⇒ 与 §8.7 的 16 个死节点
     一并量化（**共 19 个**）。

## 8. ⚠️ 实施期订正（2026-10-28，实测）

> 本节记录**实施期**对上文（计划期）的订正 ⇒ 每条都给出「原表述 → 实测/裁决」。
> ⚠️ 与上一 change（`rv32i-load-byte-lane-2026-10`）**同型**：计划期草案里
> **确实**有前提不成立的条目 ⇒ 必须在归档前订正，否则会把错的契约冻进主 spec。

### 8.1 实现的表达式形式：`&` 而非 `select(...)`

- **原表述**（§2.4 / spec delta）：「该条件 MUST 用 `select(...)` 表达」，
  且 `axi_valid = select(io().req & io().we, ch_bool(true), ch_bool(false))`。
- **实测**：框架已把 `ch_bool` 的 `&&` 标为 **deprecated**
  （`include/core/operators_logic.h`：`[[deprecated("use operator& for 1-bit AND;
  select() is for value muxing")]]`）⇒ 布尔 AND 的自然写法是 **`&`**，
  而 `select` 的语义是「**值**多路选择」。
- **裁决**：实现用 `io().req & io().we`（**更**直白，且与 `d_cache_sync.h:143`
  的 `ch_bool do_write = hit & io().we;` 同款）；**spec delta 同步改为**
  「MUST 用**组合表达式**表达（`select(...)` / 布尔 `&` / `|` / `!` 皆可）」
  ⇒ ⚠️ 否则会把**正确**的实现判为违规（契约比实现**更窄** = 契约错）。

### 8.2 M4 的形式：「驱动改成常量 0」而非「断开驱动」

- **原表述**（§4 的 M4）：「断开内层 cache 的 `addr`/`req` 驱动」。
- **问题**：「断开」⇒ 端口变成**未驱动** ⇒ 读到的是**未定义值**
  （根 `AGENTS.md §ANTI-PATTERNS` 登记：实测 `strbe` 读**全 1**、`rst_n` 读 0）
  ⇒ 变异**是否转红取决于未定义值的读值** ⇒ **不确定**。
- **裁决**：改为「把内层 `addr` / `req` 的驱动改成**常量 0**」⇒ 确定性转红
  （`axi_addr` → 0、`axi_valid` → 0）⇒ 实测**已转红** ✓。

### 8.3 判别力最强的一条用例：**内层 cache 的 fill → hit 路径**（计划期未设计）

- **原表述**（§2.4 的「内层 cache 的输入 MUST 全部驱动」表）：只说
  「接上后 `hit`/`miss` 才**可区分**」—— **没有**给出**可执行**的断言。
- **实测补齐**：内层 `ICacheSync` 的填充条件是
  `do_fill = io().axi_resp_valid & miss` ⇒ 驱动 `axi_resp_valid` + `axi_data`
  ⇒ 内层填 way0 ⇒ **再读同一块 ⇒ hit**（`miss` → 0、`axi_valid` → 0、
  `data` 返回**缓存值**）。
  ⇒ 这给出一条**端到端**的判别证据：**内层的 5 个输入端口真的被驱动了**
  （否则内层永远停在冷 miss ⇒ 「hit 可达」**不可观测**）。
- ⚠️ 拍数：内层是 **1-cycle `sread`** ⇒ hit 到达的拍数属于**内层**的实现细节
  （**不属于**本 wrapper 的契约）⇒ 用**有界等待**（≤ 8 拍）而不是钉死拍数；
  契约本身（hit **可达** + `data` 返回**缓存值**）由 `REQUIRE` 保证。

### 8.4 删掉的死代码（计划期未列）

- `i_cache_prefetch.h` 的 `next_line_addr`（**死局部变量**，从未被使用
  ⇒ `-Wunused-but-set-variable`）—— 计划期未列 ⇒ 实施时删。
- `i_cache_prefetch.h` 的 `get_block_addr()`（**私有辅助函数**）——
  转发内层 `axi_addr` 后**再无消费者** ⇒ ZERO-DEBT「无死代码」⇒ 删。
  （§2.4 的理由 #3「值同源」正是说它与内层的 `icache_get_block_addr`
  **逐位相同** ⇒ 转发后重复实现没有存在意义。）

### 8.5 ⚠️ 顺带**消掉 3 条既有 warning**（计划期未预期）

改动前 `i_cache_prefetch.h` / `d_cache_write_buffer.h` 各有 **warning**
（ZERO-DEBT：「changed files MUST warning-clean」⇒ 本 change **必须**消掉）：

| warning | 位置 | 处置 |
|---|---|---|
| `operator&&` **deprecated** | `i_cache_prefetch.h:100` | 随 `if` 一起删掉 ⇒ 消失 |
| `operator&&` **deprecated** | `d_cache_write_buffer.h:97` | 同上 |
| `next_line_addr` set but not used | `i_cache_prefetch.h:97` | 删死变量 ⇒ 消失 |

⇒ 改动后两个文件**零 warning** ✓（`i_cache_sync.h:116` / `d_cache_sync.h:102`
的 `offset` 未用 warning **仍在** —— 属**内层 cache 本体**
⇒ Non-Goal「不改 cache 控制器本体」⇒ **不在**本 change 范围 ⇒ 记入 §7）。

### 8.6 ⚠️ `VERILATOR_E2E_TOTAL` 锚点**新建**（tasks 6.5 的前提不成立）

- **原表述**（tasks 6.5）：「按实测更新根 `AGENTS.md` 的计数锚点
  （`verilator_e2e 37 → 38`）⇒ `scripts/verify_test_counts.sh` exit 0」。
- **实测**：根 `AGENTS.md` **没有** `verilator_e2e` 的**活**锚点 ——
  `37` 只出现在**历史 NOTES 条目**里（记录当时的真值 ⇒ **不可**改）。
- **裁决**：**新建**一个活锚点 `- VERILATOR_E2E_TOTAL: 38`（放在 `CTEST_TOTAL`
  之后，带 CI-anchor 注释），并在 `scripts/verify_test_counts.sh` 里加**第 2 条**
  检查（与 `CTEST_TOTAL` 同型：漂移 ⇒ `::error::` + exit 1）。
  ⇒ ⚠️ **为什么需要独立锚点**：给一个**已注册**的 target 加/删 `verilator_e2e`
  标签**不改变** `CTEST_TOTAL` ⇒ 只改标签的话 `CTEST_TOTAL` 护栏**完全看不见**
  ⇒ 双后端覆盖会**静默缩面**（本仓反复登记的那类漂移）。
  ⇒ 实测护栏自检：把锚点改成 37 ⇒ exit **1**；改回 38 ⇒ exit **0** ✓。
  ⇒ `CTEST_TOTAL` **不变**（202）—— 该 target **已**注册进 CTest，只是**加标签**。

### 8.7 `WriteBufferEntry buffer[4]`：**保留**（§7 backlog 已裁决）

- 该数组（普通 struct 数组）在直通式下**无消费者** ⇒ §7 已裁决「随真 FIFO
  一起清理（或删除）」⇒ **本 change 不动它**，只加 `(void)buffer;` 让
  「预留状态」这一事实**显式**（否则读者会以为它在起作用）。
- ⚠️ 记录：`logic_buffer()` 默认构造**会建空节点** ⇒ 该数组在图里产生
  **16 个死节点**（4 × (2×`ch_uint<32>` + 2×`ch_bool`)）⇒ 这是 §7 该条
  的**代价量化**，供真 FIFO 立项时参考。
  ⚠️ **2026-10-28 审稿第 1 轮补充**：`ICachePrefetch` 的 **3 个预取 `ch_mem`**
  同样产生**死节点**（3 个 mem 节点）⇒ 合计 **19 个死节点**。

### 8.8 对抗审稿第 1 轮的处置（2026-10-28）

**审稿结论 = REJECT**（2 BLOCKER + 7 SHOULD_FIX + 6 NIT；`mergeable: no`）⇒
本节记录**逐条**处置（BLOCKER 必须闭合；SHOULD_FIX / NIT 同批闭合留痕）。

| # | 类别 | 摘要 | 处置 |
|---|---|---|---|
| B1 | **BLOCKER** | delta 正文里出现**自己的 change 名** ⇒ `scripts/verify_openspec_delta.sh` 的 **R1** 命中 ⇒ **CI 硬门必红**；且归档时该 NOTE 会被**逐字并入主 spec** | ✅ **已闭合**：删掉含 change 名的 `#### Scenario: 路径引用已按 ADR-046 收敛订正` 与第二条 MODIFIED 开头的 `- **MODIFIED**: …` 元注释（**过程记录**不是契约）⇒ 路径订正的**理由**改记在**本 design.md**；护栏复跑 **R1 ✓ / exit 0** |
| B2 | **BLOCKER** | 「`axi_valid` MUST NOT 依赖 `axi_ready`」Scenario 的 `ready` 语义子句是**写**语义 ⇒ 字面命中**两个** wrapper ⇒ `ICachePrefetch`（**读**主端、**没有** `we`/`wdata`）**无法满足** ⇒ 归档会**冻错主 spec** | ✅ **已闭合**：spec delta 的该 Scenario 拆成**按组件分别适用**的两条子句（`DCacheWriteBuffer` = 写主端 ⇒ 保持 `req`/`we`/`addr`/`wdata`；`ICachePrefetch` = 读主端 ⇒ 保持 `addr`/`req`，并注明 1 拍 `sread` 的错位后果） |
| S1 | SHOULD_FIX | `include/cpu/AGENTS.md` 声称 `d_cache_write_buffer.h` **instance `DCacheSync`** —— **假陈述**（它**没有**内层 cache、**没有**读通路）⇒ 且**本 change 新加**的契约段也重复了该假陈述 | ✅ **已闭合**：STRUCTURE 表 + KEY FILES 表 + 新契约段三处订正为「AXI 写直通 shim（**无**内层 cache / **无**读通路）」，并标注「内层输入 MUST 被驱动」**仅适用于** `i_cache_prefetch.h` |
| S2 | SHOULD_FIX | §7 的 1 拍错位条目**低估后果**（「转发不加剧」也不准确） | ✅ **已闭合**：§7 该条升级为「**静默返回错误数据**」+「转发是**首次暴露**（此前下游看不到内层 AXI 请求 ⇒ 不可达）」；契约侧写入读侧「保持 `addr`」子句 |
| S3 | SHOULD_FIX | 本次新产生/新暴露但**未登记**的债（`axi_resp_valid` 无消费者 / DWB 无内层无读通路 / 未使用的 include / 3 个死 `ch_mem` 未量化） | ✅ **已闭合**：§7 补 4 条；`d_cache_write_buffer.h` 头注释补「`axi_resp_valid` 不被观察 ⇒ `ready` = W 握手成立，**不是**写提交」；**未使用的 include 已删**；§8.7 的死节点数 16 → **19** |
| S4 | SHOULD_FIX | §2.4 的三条理由缺**第 4 条**（策略漂移防护），且缺**反例** | ✅ **已闭合**：§2.4 补第 4 条（转发让二者**结构上恒等** ⇒ 策略变更自动继承）+ 反例（转发会把顶层主端与**陈旧**的 `miss` 耦合 ⇒ 代价 = 消费方必须保持 `addr` ⇒ 已进契约） |
| S5 | SHOULD_FIX | ADDED Requirement 的 WHEN **过窄**（只说「`ch_bool` 输入」⇒ 管不到 `if (io().addr == ...)` 这种**比较结果为 `ch_bool`** 的形态） | ✅ **已闭合**：WHEN 改为「由**输入端口取值**决定的条件（`ch_bool` 输入，或任何**比较结果为 `ch_bool`** 的表达式）」；细节移入 Scenario 的 NOTE（顺带把 Requirement 正文压到 **386** 字符 ⇒ **不再触发** CLI 的「>500 字符」warning） |
| S6 | SHOULD_FIX | 「每个输出端口 MUST 被驱动」的反向期望**过宽**（「恒读 0」—— 被**正确驱动**的端口也可能合法读 0，如 `axi_we = we` 在 `we=0` 时） | ✅ **已闭合**：反向期望改为「在**其依赖的输入变化时**仍**恒读 0**」+ NOTE 说明为何不能写成「恒读 0」 |
| S7 | SHOULD_FIX | grep Scenario 的 NOTE 漏了一条边界：**跨行**条件（`io().` 在下一行）**不被匹配** ⇒ 护栏会静默放过 | ✅ **已闭合**：NOTE 补第 ③ 条（跨行不匹配）⇒ 并明确「**规范性判据是 Requirement 正文的 MUST NOT**（覆盖所有形态）；本 Scenario 只是**可执行的探针** ⇒ **不是**「不存在 `if`」的证明」；另把 ② 的「误伤」措辞订正为「**误报**方向」 |
| S8 | SHOULD_FIX | **地址正确性零判别力**：所有被断言的地址**本来就 16B 对齐** ⇒ 「对齐」与「不对齐」两种**相反**语义都验不出来 | ✅ **已闭合**：断言改用**不对齐**地址（`ICachePrefetch`：`0x80000042` → 期望 `0x80000040`、换块 `0x80000052` → `0x80000050`；`DCacheWriteBuffer`：`0x2003` → 期望 `0x2003`、`0x3005` → `0x3005`）⇒ 并新增 **M6 / M7** 两条变异（见 §4）⇒ 实测**各自转红** |
| S9 | SHOULD_FIX | 「8 个用例全绿」**不证明** verilator 半边真的跑了（fixture 在工具缺失 / 初始化失败时是 **WARN + 提前 return** ⇒ 跳过也全绿） | ✅ **已闭合**：**正向证据**取自**断言数差** —— verilator 在 PATH ⇒ **58 断言**；不在 PATH（半边 WARN 跳过）⇒ **29 断言** ⇒ 差值 **29** = verilator 半边 ⇒ **它真的执行了**。另在用例体里加 `CACHE_SMOKE_BACKEND_INFO()`（打印当前后端）⇒ 失败时能直接看出是哪个后端 |
| N1 | NIT | test ① 的「反向期望：内层 `addr`/`req` 未被驱动 ⇒ `miss` 恒 0」不准确（未驱动端口读到**未定义值**，不保证是 0） | ✅ 已改：改为「**若内层被真正驱动** ⇒ 会填充 ⇒ 再读会 hit ⇒ `miss` 变 0」，并注明判别力由 test ③ 的 fill→hit 路径承担 |
| N2 | NIT | grep NOTE 的 ② 「误伤 `notif(` 之类以 `_` 前缀结尾的标识符」机制描述与措辞对不上 | ✅ 已改：`notif(` 的 `if` 前是 `t` ⇒ 会被匹配 ⇒ 措辞订正为「**误报**方向」 |
| N3 | NIT | `verify_test_counts.sh` 的新锚点检查是 **fail-open**（锚点缺失 ⇒ 跳过） | ✅ **如实登记**：与既有第 1 条（`CTEST_TOTAL`）**同型** ⇒ 与仓内既有风格一致；若要 fail-closed 可参照 `scripts/verify_perf_baseline_anchors.sh` |
| N4 | NIT | 根 `AGENTS.md` 里 `VERILATOR_E2E_TOTAL: 38` 出现 **2 次**（锚点 + NOTES 散文）⇒ `head -1` 依赖「锚点排在散文之前」这一偶然顺序 | ✅ 已改：NOTES 散文里**不再**逐字写该锚点行（只写「一个 `VERILATOR_E2E_TOTAL` 锚点（值 38）」）⇒ 锚点**唯一** |
| N5 | NIT | §1 的表没区分「空体 `if`」与「含驱动的 `if`」⇒ 与根 NOTES 的「3 处」口径可能漂移 | ✅ 已改：§1 的表加「**含驱动**？」列 + 计数口径说明（5 处 `if`，其中 **3 处含驱动**） |
| N6 | NIT | 「输出 MUST 随输入变化」Scenario 的输入清单**不适用于** `ICachePrefetch`（它没有 `wdata`/`we`） | ✅ 已改：按组件分别列出两套输入清单 |

⚠️ **范围外观察（backlog，本 change 不处理）**：主 spec `cpu-cache-subsystem` 的
Requirement「hit-path uses **aread** (combinational read)」与 **ADR-041**
（Sync 变体用 1-cycle `sread`）**直接矛盾**，且该 Requirement 还要求
「同拍 hit/miss 决策」⇒ 归档后主 spec 内部会同时存在两条**互斥**契约
（本次新增的 fill→hit 路径恰恰依赖 `sread` 的 1 拍延迟）⇒ **既有**漂移
⇒ **独立立项**修正，**不在**本 change 范围。
