# Design: RV32I D-L1 的 AXI4 full 主口 + 真 line

## 1. 现状（实测 · `d_cache_sync.h` 191 行全文已读）

```
前端（简化握手）：addr / wdata / req / we  →  rdata / ready / miss
AXI 侧（简化握手）：axi_addr / axi_wdata / axi_we / axi_valid
                    ← axi_ready / axi_rdata / axi_resp_valid
```

结构事实：
| 项 | 现状 | 后果 |
|---|---|---|
| line | `DATA_WIDTH = 128`（4 字）· `OFFSET_BITS = 4` | 名义 16 B line |
| fill | `zext<128>(axi_rdata)` ⇒ 只填低 32 位 | **3/4 的 lane 是零** |
| 读 | `bits<31,0>(cache_data)` | **`offset` 不进数据选择** ⇒ 同 line 4 地址读同一字 |
| 写 | `zext<128>(wdata)` | 同上（只写低 32 位） |
| AXI 口宽 | `axi_rdata` **32 bit** | 与 128-bit line 天然不匹配 |
| burst | **零**（`awlen`/`beats`/`last` 0 命中） | line 填充只能「一次 1 字」 |
| `miss` | `sread` 的 1 拍延迟结果 | 与当拍 `addr` **错位 1 拍** ⇒ 既有契约「调用方 MUST 在 miss 期间保持 addr 不变」 |

⇒ ⇒ **本 change 三件事**：AXI 侧换 full（含 burst）· 修 line 数据通路 · 加 `TcmAxiSlave`。

## 2. 新的 `DCacheSync`

### 2.1 端口

**前端不变**（内核接线零改动）：
```
addr[32] / wdata[32] / req / we   →   rdata[32] / ready / miss
```

**AXI 侧（full · 5 通道）**：
```
AR: ar_addr[32] ar_len[4] ar_size[3] ar_burst[2] ar_valid  →  ar_ready
R : r_data[32]  r_resp[2] r_last  r_valid                  ←  r_ready
AW: aw_addr[32] aw_len[4] aw_size[3] aw_burst[2] aw_valid  →  aw_ready
W : w_data[32]  w_strb[4] w_last  w_valid                  →  w_ready
B : b_resp[2] b_valid                                      ←  b_ready
```
⇒ ⇒ 组件端口 **+12 根**（原 7 根 ⇒ 19 根）。⚠️ 这是**组件级**端口，**不进核的 34 端口面**。

### 2.2 读 miss 的 burst fill（4 beat 铺满 128-bit line）

```
             ┌──────────┐  req&&!we&&miss   ┌───────────┐
   IDLE ────►│ 发 AR     ├─────────────────►│  WAIT_R   │
             │ (len=3)   │                  │ beat 0..3 │
             └──────────┘                  └─────┬─────┘
                   ▲                             │ r_valid
                   │                             ▼
                   │                    line[beat_idx] ← r_data
                   │                    beat_idx++
                   │                             │
                   └────── r_last ───────────────┘  ⇒ fill_done 脉冲 ⇒ ready
```

- `ar_len = 3`（4 beat）· `ar_size = 2`（4 B）· `ar_burst = INCR(01)`
- `ar_addr = addr & ~0xF`（line 对齐；**miss 期间锁存**，不跟随新 `addr`）
- 每个 `r_valid` beat 写进 `line[beat_idx]`（`bits_update` 或 4 路 `select` + 写整行）
- `r_last` ⇒ `fill_done` 脉冲 ⇒ `ready` 拉高 1 拍

⚠️ **`r_ready` MUST 恒 1**（本 cache 无 R 通道缓冲 ⇒ 不能反压上游 burst）。

### 2.3 `offset` 参与数据选择（**修「假 line」**）

```cpp
// 读：line 的 128 bit 按 addr[3:2] 选 32-bit lane
auto lane_sel = bits<3, 2>(io().addr);
auto word = select(lane_sel == 0_d, bits<31, 0>(cache_data),
            select(lane_sel == 1_d, bits<63, 32>(cache_data),
            select(lane_sel == 2_d, bits<95, 64>(cache_data),
                                    bits<127, 96>(cache_data))));
io().rdata <<= select(hit, word, /* fill 路径见 2.4 */);
```
写同理：`line` 的对应 lane ← `wdata`（其余 lane **保持**）。

### 2.4 fill 完成后的数据返回

fill 的 4 个 beat 里**已经过手**目标 lane 的数据（`beat_idx == addr[3:2]` 的那拍）
⇒ ⇒ **直接锁存它**，不必再读一次 `ch_mem`：

```cpp
// ⚠️ 用**当拍**的 r_data，不是 beat 寄存器（`fill_complete` 那拍寄存器还没装载）
auto target_now = select(lane_sel == beat_idx, io().r_data, fill_target);
```
⇒ **miss 的额外延迟 = 4 个 beat**（不是 4 + 1）。

## 2.4b ⚠️ 实施期发现（**实测** · 与原设计不同的三处）

> 这三条都是「一接进流水线就暴露」的，**不可能**在组件级单元测试里发现
> （它们来自 `rv32i-memory-timing` 的 MEM 反压语义与 S2 的冻结链）。

### ① `miss` / `ready` MUST 是**组合**信号（原设计用 `sread` 的 1 拍延迟值）

原实现的 `miss` 来自 `sread` 的 tag 读 ⇒ **与当拍 `addr` 错位一拍**。
组件级用例「拍 N 设 addr、tick、读 miss」看不出差别（addr 不变），但流水线里
`addr` 每拍都变 ⇒ ⇒ 反压会判在**错误的那条请求**上。

⇒ ⇒ 改为 **tag/valid 走 `aread`（组合）**，`ready` / `miss` 对**当前 `addr`** 成立。
⇒ ⇒ `req` 因此成为**真握手**（MUST 保持到 `ready=1`）。

### ② `rdata` MUST 是**保持寄存器**（**S2 的冻结语义强制**）

已签批的 `rv32i-memory-timing` 有「**MEM/WB 寄存器 MUST 被冻结**」。
于是这条时序**必然可达**：

| 拍 | 事件 |
|---|---|
| N | load `i` 完成（`ready=1`）⇒ 沿后 `M2` 装载 `i` |
| N+1 | `M2` 持有 `i`（车道选择**组合**消费 `data_read_data`）；但 `EX/MEM` 里的 `i+1` **miss** ⇒ `mem_stall=1` ⇒ **MEM/WB 被冻结** ⇒ `i` 的 load 结果装不进去 |
| G | `i+1` 的 fill 完成 ⇒ 解冻 ⇒ `MEM/WB` 才装载 `i` ⇒ ⇒ **此时 MUST 还能读到 `i` 的数据** |

⇒ ⇒ 若 `rdata` 是「随 `addr` 变化的组合量」，拍 G 的 `addr` 已是 `i+1` 的 ⇒ **数据丢失**。
⇒ ⇒ 故 `rdata` = **寄存器**，在**完成那拍**捕获（hit ⇒ `lane_word`；fill ⇒ `target_now`），
**保持到下一次读完成**。

⚠️ 这条也解释了「为什么 data 数组必须 `aread`（组合）」：只有组合读才能在**请求拍**
拿到数据并当场捕获 ⇒ ⇒ **hit 仍是 1 拍**（若用 `sread` 再寄存一次就会多 1 拍）。

### ③ 前端 MUST 有 `strbe`（字节使能）

`SB` / `SH` 只写选中字节 ⇒ ⇒ 前端没有 `strbe` 时，cache 的 line 更新和 AXI 侧
`w_strb` 都只能整字 ⇒ ⇒ 字节写会**静默覆盖**同字其余字节。
⇒ ⇒ 新端口 `strbe[4]`（`wdata` 约定仍是「已按车道对齐」，与 `DataTCM` 一致）。

### ④ 核侧 MUST 加 `data_req`（否则 miss 风暴）

cache 的**读有副作用**（miss ⇒ 发 AXI burst）。若像 `DataTCM` 那样「无条件读」，
则**每条** ALU/NOP 指令都会触发一次 cache 查找 ⇒ ⇒ 冷启动即 miss 风暴
⇒ ⇒ 故 `Rv32iPipeline` 新增 `data_req = MEM 级有效 & (load | store)`。

### ⑤ `data_ready` MUST 有「零请求放行」

`data_ready` 在 S2 的语义里是**无条件**冻结信号（拉低即冻结）。
cache 的 `ready` 在**空闲拍**为 0（无请求就没有完成的请求）⇒ ⇒ 若直接接
`data_ready ← cache.ready` ⇒ **整条流水线永久冻结**（实测：24 条核级判据 23 条红）。
⇒ ⇒ 核侧接线 MUST 是 `data_ready ← !data_req | cache.ready`。

### 2.5 写（write-through · 单 beat）

- `aw_len = 0`（1 beat）· `aw_size = 2` · `aw_burst = INCR`
- `w_strb`：**直接用前端 `strbe`**（byte lane 语义复用 `axi4::detail::byte_lane_*`）
- **hit 时同步更新 line 的对应 lane**（其余 lane 保持；lane 内按 `strbe` 合并）
- `ready` = **`aw_fire`**（`aw_valid & aw_ready`）：store 在 **AW 被接收那拍**完成
  ⚠️ **AW 与 W 分拍**（本仓的 `TcmAxiSlave` 先收 AW、下一拍收 W）⇒ ⇒ cache MUST
  在 AW 握手时**锁存 W 数据**并保持到 W 握手（`w_hold` / `w_data_lat` / `w_strb_lat`）
  ⇒ ⇒ 副作用：**连续 store 会各多 1 拍**（第二个 AW 要等上一个 W 收完）
  ⇒ ⇒ 这是 `data_write_en` 观测呈**电平**（多拍）的根因，已按「事务」口径改写测试判据。

⚠️ **写不 burst**：write-through 的语义是「逐字提交」，`AWLEN=0` 最直接；
若日后要写合并（write buffer）⇒ 独立立项（`d_cache_write_buffer.h` 的已知债务）。

## 3. `TcmAxiSlave`（新 · `include/axi4/tcm_axi_slave.h`）

把 `DataTCM` 包成 AXI4 full slave：

```
AR/AW 的 addr（字节）→ 字地址（addr >> 2）
burst：ARLEN/AWLEN 递增 → 逐个地址访问 ch_mem（1 拍 sread / 写）
R/B：按 beat 返回；r_last 在第 N-1 拍
```

- **读**：1 拍 `sread` ⇒ 逐 beat 返回
- **写**：`w_strb` → `DataTCM.strbe`；逐 beat 写
- ⚠️ **`DataTCM` 的读口是 `sread`（1 拍）** ⇒ burst 内每 beat 需 1 拍 ⇒
  `ARLEN=3` 的 4-beat 读 = **4 拍数据**（+ 握手）⇒ 这是**存储器带宽的真实限制**，
  不是本 change 引入的（TCM 就是同步 SRAM）

## 4. 核接线（`rv32i_core_top.h`）

```
pipeline.data_addr        → cache.addr          （字节地址）
pipeline.data_write_data  → cache.wdata         （已按车道对齐）
pipeline.data_write_en    → cache.we
pipeline.data_strbe       → cache.strbe         ← **新增前端端口**
pipeline.data_req         → cache.req           ← **新增流水线端口**（load|store & MEM 有效）
pipeline.data_read_data   ← cache.rdata         （**保持寄存器**）
pipeline.data_ready       ← !data_req | cache.ready   ← **S2 的冻结链第一次真正被拉低**
cache.AR/R/AW/W/B ⇄ TcmAxiSlave ⇄ DataTCM
```

⚠️ **`prog_*` 不受影响**（仍直写 I-TCM）⇒ AC7 的「装载一致性」判据。

### 4.1 实施后的**观测面**变化（如实登记）

| 面 | 变化 | 处置 |
|---|---|---|
| `data_write_en` / `wb_write_en` 的**电平** | 多拍访存 ⇒ 同一事务的观测会**连续重复** | 测试判据从「逐拍计数」改为「**事务计数**」（连续相同 `(addr,data)` 折一）+ 保留序列/配对断言 |
| `perf_load_use_stalls` | 两处 load→use 的 load **都 miss** ⇒ 保持拍归入**内存反压**（S2 口径 `& !mem_stall`）⇒ 每处只剩 1 检测拍 | 按「每处 1 拍」手推订正；**并在固件末尾补一条 hit 形态的 load→use**（critic SHOULD_FIX #1）⇒ 恢复「4 拍」形态的固件级载体 ⇒ 合计 **6**（2 + 4）|
| `perf_cycles` / 退休数 | +18 拍（3 次 miss × 8 + 2 保持）+ 4 拍（新增的 hit 形态 load-use） | 计时预算 23 → **45**（分段分解已写进测试注释） |
| 核的 34 端口面 | **零变化** | `verify_rv32i_soc_core_ports` / `verify_hierarchical_rtl` R8′ |
| 三份产物 | **字节会变**（数据通路换了组件） | `verify_flat_rtl_emit` / `verify_hierarchical_rtl` / `rv32i_rtl_sim_env` 全绿 |

⚠️ **`prog_*` 不受影响**（仍直写 I-TCM）⇒ AC7 的「装载一致性」判据。

## 5. spec 边界（为什么不改 `rv32i-memory-timing`）

`rv32i-memory-timing` 的「存储器的读 MUST 是**固定 1 拍**寄存读」约束的是
**TCM 的读口**（`DataTCM.sread`）⇒ ⇒ **本 change 不改 TCM** ⇒ 该 Requirement **不破**。

流水线看到的延迟由 **`data_ready` 反压**表达：
- `hit` ⇒ `ready=1` ⇒ 1 拍（与 TCM 一致）
- `miss` ⇒ `ready=0`（多拍）⇒ **这不是「存储器延迟」而是「cache 层的等待」**
  ⇒ ⇒ 对应**新增** capability（`rv32i-dcache-timing` 或 `cpu-cache-subsystem` 的 ADDED Requirement）

⚠️ **这也正是 S2 的价值**：没有 S2 的冻结链，cache miss 只能靠「延长读延迟」
⇒ 会直接违反「固定 1 拍」的**字面**。

## 6. 风险

| 风险 | 处置 |
|---|---|
| `miss` 与 `addr` 的 1 拍错位（既有契约） | **miss 期间锁存 line 基址**；调用方仍 MUST 保持 addr（既有契约不变） |
| burst 中途 `addr` 变化 | 同上（锁存 + 契约） |
| `r_ready` 恒 1 导致上游溢出 | 本 cache 无 R 缓冲 ⇒ 恒 1 是**唯一合法**选择（AXI 允许） |
| 既有 5 个 DCache 测试（1 beat 激励） | **必须改**（这是契约变更的应有代价）⇒ 机械改 4 beat |
| `d_cache_async.h` 的同样缺陷 | **本 change 不动** ⇒ **登记为已知边界**（async 未接入生产路径） |
| 大改动（191 行 → 296 行） | 分 3 阶段实施（见 tasks）· 每阶段独立验证 |
| **反压与冻结语义的交互**（§2.4b ①②） | 组件级测试**看不见** ⇒ ⇒ 由**核级**用例（`test_rv32i_soc_dual_parity` 的 load/store 固件）兜住 |
| 连续 store 的 1 拍代价 | 已知代价（AW/W 分拍）⇒ ⇒ 真写缓冲 = 独立立项（`d_cache_write_buffer.h` 的既有债务） |
