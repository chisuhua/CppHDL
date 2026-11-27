# Design: S4b —— N×M Lite 互联（**chlib stream 组合**）+ 5 个外设

> **Change**：`rv32i-soc-peripherals-interconnect-2026-11`
> **前置**：S4a 已归档 · IP 冻结批已闭
> **决策**：用户 2026-11-27 = **③ 新写 N×M Lite 互联**
> **实现约束（用户 2026-11-27）**：**尽量复用 `chlib`**，特别是 **`ch_stream`** 组合子
> （参照 `examples/stream/` 的接线范式）⇒ **数据面 MUST NOT 手写 valid/ready 转发**。

---

## 0. 决策清单（两处 **待用户确认**）

| # | 决策 | 取值 | 状态 |
|---|---|---|---|
| **D-1** | 计时器地址落点 | **追加 `kTimer` 段**（`0x8010_7000` · 第 8 段）⇒ 既有 7 段**零移动** | ⏳ **待确认**（否决则 TIMER 延到 S5） |
| **D-2** | CLINT / DMA 段（设备分别 S5 / S4c） | 窗口照样映射，target = `ERR`（互连自应答 DECERR） | ✅ 推荐 |
| **D-3** | 外设**物理引脚**是否上根端口 | **本批不引**（G-8 未裁）⇒ 只判总线级回环 | ⏳ **待确认** |
| **D-4** | TCM 是否挂 Lite 总线 | **不挂**（沿用 G-6 §五：D-L1 前分流） | ✅ |
| **D-5** | 仲裁 | `chlib::round_robin_selector`（每方向独立 · 授权保持到握手） | ✅ |
| **D-6** | `bresp/rresp` 上报软件 | 本批不做（S5 与 trap 同批） | ✅ |
| **D-7** | A8 的 DECERR 探针 | 语义不变（CLINT 段 → `ERR`） | ✅ |
| **D-8** | SoC 根端口面 | 不变（10 根） | ✅ |
| **D-9** | 参数 | `N = 2`（核 / DMA 预留）· `M = 6`（gpio/uart/timer/spi/i2c/pwm）· `W = 8` | ✅ |

---

## 1. 分层：AXI 端口 ↔ **5 条 stream 平面**

AXI4-Lite 的通道语义与 `ch_stream<T>` **逐条对应**，因此互联内部**只用 stream**：

| AXI4-Lite | stream | 方向 |
|---|---|---|
| AW（addr/prot/valid/ready） | `ch_stream<AwPayload>` | master → slave |
| W（data/strb/valid/ready） | `ch_stream<WPayload>` | master → slave |
| B（resp/valid/ready） | `ch_stream<BPayload>` | slave → master |
| AR（addr/prot/valid/ready） | `ch_stream<ArPayload>` | master → slave |
| R（data/resp/valid/ready） | `ch_stream<RPayload>` | slave → master |

对应关系（**逐条落到既有契约**，不是自创）：

- `ch_stream::valid` 只依赖**输入侧/状态** ⇒ 满足 AXI「VALID MUST NOT 依赖 READY」。
- 握手期间 `payload` 由 stream 持有 ⇒ 满足「VALID 期间 payload 稳定」。
- `ready` 链由 `stream_mux` / `stream_demux` **自动传播**（`selected & out.ready`）
  ⇒ **不需要手写反压**（`chlib-stream-ready-ownership` 的既有契约）。
- `fire()` = `valid & ready`，直接用真值语义（**禁** C++ 控制流判断硬件值）。

### 1.0 端口面（固定 2×6）

`AxiLiteCrossbar` 的端口面 = **2 个 master 面 × 6 个 slave 面**，每面 19 根（+ `rst_n`）：

- 具名（`m0_awaddr{"m0_awaddr"}` …）⇒ 过 `verify_port_naming.sh` 的棘轮（**0 新增债**）。
- SoC 侧 6 个从口 = gpio / uart / timer / spi / i2c / pwm；`Map` 里多出来的段
  （clint / dma）指 `kErr`。
- 参数化只保留 `Map::windows[]`（`W` 任意）与地址 / 数据宽度（`static_assert` 32/32）。

### 1.1 边界适配器（唯一 `valid/ready` 手写处）

只做「19 根端口 ↔ 5 条 stream」的**搬运**：

- master 侧：`valid/payload` 取自 `ch_in`；`ready` 由互联**驱动**到 `ch_out`
  ⇒ 形如 `ch_bool(io().x_awvalid.impl())` / `io().x_awready <<= stream.ready`。
- **接线纪律**（`examples/stream/stream_mux_top.h` 的原样复制）：
  1. 每个被组合子消费的 stream MUST **先 `as_master()`**；
  2. 组合子返回的 `input_streams[i].ready` MUST 用 **`<<=`** 覆盖
     （`operator=` 只重绑本地句柄 ⇒ 反压静默丢失）；
  3. 消费端覆盖组合子的默认 ready MUST 直接 `<<=` 到 `result.output_stream.ready`
     （ADR-054 形态 E · 单阶段）。

## 2. payload 打包（**packed `ch_uint`**，不走 struct mux）

`stream_mux` 对非 `ch_uint` 的 `T` 走字段级 `select` 回退路径；为避免依赖该路径并让
`mux<N>` 走快路径，payload 一律**打包成 wide `ch_uint`**，边界处 pack/unpack：

| 通道 | 布局 | 宽度 |
|---|---|---|
| AW / AR | `{prot[1:0], addr[31:0]}` | 34 |
| W | `{strb[3:0], data[31:0]}` | 36 |
| B | `{resp[1:0]}` | 2 |
| R | `{resp[1:0], data[31:0]}` | 34 |

- pack/unpack helper 全部 `constexpr` 宽度（`static_assert` 钉住）。
- ⚠️ `AxiLiteCrossbar<Map, ADDR_WIDTH = 32, DATA_WIDTH = 32>`：宽度由 `static_assert`
  限定为 `DATA_WIDTH == 32`（本批），**不做**未验证的位宽泛化。
- ⚠️ **端口面固定为 2 master × 6 slave**（不按 `N` / `M` 模板展开）：端口 MUST **具名**
  （ADR-075 E1 · `port-naming-ratchet` 的「新增端口 MUST 具名」）⇒ 数组化端口只能得到
  `unnamed_input_N` ⇒ 会新增命名债。参数化的自由度保留在 **`Map::windows[W]`**
  （窗口数任意，`target` 越界指 `kErr`）。

## 3. 平面拓扑（5 条 stream 平面 · 两级组合）

记 `M = 6`、`N = 2`；`ERR` 是 demux 的**第 `M` 路输出**（在范围内 ⇒ 不会被
`stream_demux` 的「select 越界 ⇒ ready=0」规则卡死）。

```
AW: m_i.aw ──[stream_mux<2>  (select = grant_idx)]──> aw_sel
        └─[stream_demux<7>  (select = decode(aw_sel.addr))]─> s_j.aw  (j<6)
                                                              └─> ERR 路（ready=1）
W : m_i.w  ──[stream_demux<7> (select = wr_route_i)]──> w_ij
        s_j.w <──[stream_mux<2>  (select = wr_owner_i(j))]── w_ij      （两张表：i × j）
B : s_j.b ──[stream_demux<2> (select = wr_owner_i(j))]──> b_ji
        b_i <- [stream_mux<7>(select = wr_route_i)] <- {b_0i … b_5i, local DECERR B}
AR/R: 与 AW / B 同构（读侧独立：`rd_route_i` / `rd_owner` / `err_r_pending`）
```

- **AW 的 `decode(addr)` 取自 mux 之后**（被选中 master 的 payload）⇒ 路由与授权一致。
- **W 只在 `wr_active_i` 期间有效**（AW 握手前 `wready=0` ⇒ master 持有 W，AXI 合法）；
  `wr_active` 由 AW `fire` 置位、B `fire` 清零。
- **W 的两级是必需的**：`stream_demux` / `stream_mux` 的 `ready` 由组合子用 `<<=` 驱动
  （last-wins）⇒ ⇒ **一个 ready 节点只能有一个驱动者**。若写成「每 slave 一个
  `stream_mux` 直接从 master 的 W 取」，同一个 master 的 `ready` 会被 6 个 mux 各驱动一次
  ⇒ ⇒ 静默错乱。故：**每 master 一次 demux（唯一驱动者）→ 每 slave 一次 mux（唯一消费）**。
- **owner 由状态组合派生**（不额外存寄存器）：
  `owner_j = (wr_active_0 ∧ wr_route_0 = j) ? 0 : (wr_active_1 ∧ wr_route_1 = j ? 1 : 0)`；
  W/B/R 的 mux/demux select 都用它。
- **同一 slave 的写 / 读各自串行**（`wr_busy_j` / `rd_busy_j`）：目标 slave 已有在途事务时，
  该方向的 `s_j_*valid` **和** master 侧 `ready` **一起**被掐掉（只掐 `ready` 不够 ——
  slave 看到 `valid ∧ ready` 就是一次握手）。若不串行，两个 master 的 W 会争同一个
  `select = owner_j` ⇒ owner 唯一性不成立。**串行只发生在「同一 slave」**，不同 slave 的
  并发事务不受影响。
- **R 用 `rd_owner`** ⇒ 读响应跟随**读路由锁存**；R 也只在 `rd_active_i` 期间有效。
- `ERR` 路的 AW/AR 立即 `ready=1`；W 就地消费；B/R 由**本地 DECERR 流**产生
  （`resp = 2'b11` · `rdata = 0`），保持到 `ready`。

> ⚠️ **为什么 `ERR` 是 demux 的第 M 路而不是「越界 select」**：`stream_demux` 的契约是
> 「越界 ⇒ 全 valid=0 **且 input.ready=0**」⇒ 用越界表示 ERR 会让事务**永久挂死**。

## 4. 仲裁与状态（用 chlib 的已测算子）

| 状态 | 更新时机 |
|---|---|
| `last_grant_oh`（N 位 one-hot） | 每次**握手**（`fire`）后 = `grant_oh` |
| `grant_oh` / `grant_valid` | 未授权时取 `round_robin_selector(req, last_grant_oh).grant`；授权后**保持到握手** |
| `wr_active_i` / `rd_active_i` | AW/AR `fire` 置位；B/R `fire` 清零 |
| `wr_route_i` / `rd_route_i` | AW/AR `fire` 锁存 target（含 `ERR`） |
| `wr_owner[j]` / `rd_owner[j]`（**组合派生**） | 由 `active_i ∧ route_i = j` 算出，无需寄存器 |
| `err_b_pending` / `err_r_pending` | ERR 路：W `fire`（写）/ AR `fire`（读）置位；B/R `fire` 清零 |

- `req_i = m_i.aw.valid & !wr_active_i`（读侧同型）⇒ **单 outstanding**（与 S4a 一致）。
- **授权的可观测契约**（实现是拍级组合仲裁 + 握手当拍锁存，与 S4a 的 2×2 一致）：
  ① 握手前**不落任何状态**（`active` / `route` 只在 `fire` 更新）⇒ 仲裁在拍间变化**不影响**
  已接受的事务；② 未被接受的请求 MUST **保持**（level 信号，MUST NOT 吞）；③ 轮转只在
  握手后推进 `last_grant_oh`（复位值 0 ⇒ 首轮从**索引 1** 起，见 §7.1 的手推）。
  ⇒ ⇒ **MUST NOT** 把「授权保持」理解成「组合 grant 在拍间不变」——那需要额外锁存寄存器，
  且与 2×2 的既有语义不一致。
- 索引提取用 `chlib::onehot_to_binary<N>`（`converter.h`）。
- **全部状态更新用 `->next` + `select`**（禁 C++ `if`；禁对 `ch_reg` 做 C++ 赋值）。

## 5. 复用清单（**这一节是「尽量用 chlib」的可核对落点**）

| 用途 | 复用件 | 位置 | 为什么不用手写 |
|---|---|---|---|
| 数据面选主 | `chlib::stream_mux<N, T>` | `chlib/stream.h:864` | ready 归属 + 默认驱动已由组合子实现（ADR-054/056） |
| 数据面选从 | `chlib::stream_demux<T, N>` | `chlib/stream.h:988` | 越界语义 / ready 只回选中路 已有契约 |
| 轮转仲裁 | `chlib::round_robin_selector<N>` | `chlib/selector_arbiter.h:188` | 轮转 + 回绕判据已实现（`chlib-stream-arbitration` 有判别力用例） |
| one-hot → 索引 | `chlib::onehot_to_binary<N>` | `chlib/converter.h:38` | 避免手写优先级编码 |
| 接线范式 | `examples/stream/stream_mux_top.h` · `stream_demux_top.h` · `tb_stream_*` | `examples/stream/` | `as_master()` + `<<=` 的纪律已在那里验证过 |

⚠️ **不用的**：`stream_arbiter_lock` —— 它的选择是 `priority_encoder`（**固定优先级**），
不满足 AC3 的轮转；本设计只用它的**锁存思想**，仲裁交给 `round_robin_selector`。

## 6. 地址表与 SoC 装配

（与上一版相同，仅列要点）

```cpp
// rv32i_mem_map.h（D-1）
inline constexpr MemRegion kTimer{kMmioBase + 7 * kMmioSegmentBytes, kMmioSegmentBytes};
inline constexpr uint64_t kMmioBytes = 8ull * kMmioSegmentBytes;

// SoC
ch::ch_layer<axi4::AxiLiteCrossbar<SocCrossbarMap>> lite{"lite", "AxiLiteCrossbar"};
```

- `SocCrossbarMap::windows[8]` **派生**自库层表（8 条 `static_assert` 逐段钉住）：
  `kClint→ERR` · `kUart→1` · `kGpio→0` · `kSpi→3` · `kI2c→4` · `kPwm→5` · `kDma→ERR` · `kTimer→2`。
- m1（DMA 预留）tie 0；外设物理输入 tie 常量；物理输出在 lint `lint_off` 白名单登记。

## 7. 判据（手推 · 禁抄仿真）

### 7.1 单元级（**stream 平面 + AXI 端口两层** · 2×6 = 12 对）

1. `AxiLiteCrossbar<CrossTestMap>`（6 slave + 1 ERR 窗口）：**12 对**路由完备（每对 1 写 + 1 读）。
2. 锁存：AW / AR 握手后改 `awaddr` / `araddr` ⇒ W/B（写侧）与 R（读侧）仍去原 slave。
3. 在途：同 master 第二笔 AW **同一目标**与**另一个空闲目标**都不被接受、且不丢。
4. 仲裁：两 master 同时请求同一 slave ⇒ **AW 与 AR 各自**轮转（连续 4 轮手推）。
   手推顺序 = **m1, m0, m1, m0**：`round_robin_selector` 的起点 = `(last_grant_idx + 1) % N`，
   `last_grant` 复位 0 ⇒ 首轮起点 = 1 ⇒ 授予 m1，之后每次握手把 `last_grant` 推进。
5. DECERR：未映射 + `ERR` 窗口 ⇒ `bresp/rresp=0b11`、不挂死、不落到真实 slave。
6. W 跟随写路由：AW 与 W 相隔 1 拍。
7. 同 slave 串行：两 master 同时写同一 slave ⇒ 第二个 `awready=0` **且** slave 看不到
   第二次 `awvalid`；第一笔 B 完成后第二个才被接受。

### 7.2 外设总线回环（各 1 条 · 手推）

| 外设 | 写地址 | 写值 | 读回 |
|---|---|---|---|
| UART | `kUart+0x0C` | `0x3` | `0x3` |
| TIMER | `kTimer+0x00` | `0x1234_5678` | 同值 |
| SPI | `kSpi+0x10` | `0x7` | `0x7` |
| I2C | `kI2c+0x10` | `0x64` | `0x64` |
| PWM | `kPwm+0x00` | `0x00FF` | `0x00FF` |
| GPIO | `kGpio+0x00` / `+0x04` | `0xA5` / `gpio_in=0x5A` | A7 / A9（沿用） |

### 7.3 判别力（反向判据 · 每条**实测**转红）

| 变异 | 实测转红的用例 |
|---|---|
| M1 `ERR` 窗口目标改成 s0 | `ERR window: DECERR without touching any slave`（双后端） |
| M2 B 平面 demux select 换成**读** owner | `fanout: all 12 write pairs` + `arbitration`（双后端） |
| M3 去掉 AW 在途门控（`& !wr_active_i`） | `in-flight: awready low …`（双后端） |
| M4a 去掉 `aw_last_grant` 更新（固定优先） | `arbitration`（写相位） |
| M4b 去掉 `ar_last_grant` 更新 | `arbitration`（读相位） |
| M5 `m*_rvalid` 恒 0 | 全部读用例（5 用例 × 2 后端 = 10） |
| M6 窗口 `size` 非 2 的幂 | **编译失败**：`static_assert(Map::windows 非法…)` |
| M7 去掉 `s_j_awvalid` 的忙门控 | `same-slave serialization: second AW waits for the first B`（双后端） |

> 变异纪律：每条都**先确认仍能编译**再跑；跑完按 `git`/备份还原并**重建**（二进制也是状态）。

## 8. 影响面与回滚

- **新增**：`include/axi4/axi_lite_crossbar.h` · `tests/test_axi_lite_crossbar.cpp`。
- **修改**：`rv32i_mem_map.h`（D-1）· `rv32i_soc_top.h` · `soc-rtl-expectations.md` ·
  `examples-rtl-sim-targets.tsv` · `public-api-surface.md` · `docs/CHANGELOG.md` · 计数锚点。
- **回滚**：crossbar 是新文件、SoC 装配单处替换 ⇒ 回退 = 换回 `AxiLiteMatrix2x2`（一次 revert）。
