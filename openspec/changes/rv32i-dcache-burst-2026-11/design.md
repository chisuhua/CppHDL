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
fill_target_data->next = select(fill_beat_hit_target, io().r_data, fill_target_data);
```
⇒ `ready` 与 `fill_target_data` 同拍有效 ⇒ **miss 的额外延迟 = 4 beat**（不是 4 + 1）。

### 2.5 写（write-through · 单 beat）

- `aw_len = 0`（1 beat）· `aw_size = 2` · `aw_burst = INCR`
- `w_strb`：由 `addr[1:0]` + `we` 决定（**byte lane** 语义复用 `axi4::detail::byte_lane_*`）
- **hit 时同步更新 line 的对应 lane**（其余 lane 保持）
- `ready`：`aw_ready & w_ready`（或 `b_valid` 触发，按既有 write-through 语义）

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
pipeline.data_addr   → cache.addr          （字节地址）
pipeline.data_write_data → cache.wdata
pipeline.data_write_en / data_strbe → cache.we / w_strb
pipeline.data_read_data ← cache.rdata
pipeline.data_ready  ← cache.ready          ← **S2 的冻结链第一次真正被拉低**
cache.AR/R/AW/W/B ⇄ TcmAxiSlave ⇄ DataTCM
```

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
| 大改动（191 行 → ~450 行） | 分 3 阶段实施（见 tasks）· 每阶段独立验证 |
