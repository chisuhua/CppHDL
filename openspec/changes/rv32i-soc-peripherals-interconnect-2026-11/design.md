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
- ⚠️ `AxiLiteCrossbar` 的模板参数接受 `DATA_WIDTH` / `ADDR_WIDTH`；宽度由 `requires`
  或 `static_assert` 限定为 32/32（本批），**不做**未验证的位宽泛化。

## 3. 平面拓扑（每平面 = **mux 选主 + demux 选从**）

记 `M` 个 slave、`N` 个 master，`ERR` 是 demux 的**第 `M` 路输出**（在范围内 ⇒ 不会被
`stream_demux` 的「select 越界 ⇒ ready=0」规则卡死）。

```
AW: m_i.aw ──[stream_mux<N> (select = grant_idx)]──> aw_sel
        └─[stream_demux<M+1> (select = decode(aw_sel.addr))]─> s_j.aw  (j<M)
                                                             └─> ERR 路（ready=1）
W : m_i.w  ──[err_route ? 就地消费 : 原样]──> w'_i
        s_j.w <──[stream_mux<N> (select = wr_owner[j])]── w'_i
B : s_j.b ──[stream_demux<N> (select = wr_owner[j])]──> b_i(from s_j)
        b_i <- [stream_mux<2>(select = err_b_pending_i)] <- {b_i(from s_j), local DECERR B}
AR/R: 与 AW/W/B 同构（读侧独立：`rd_owner` / `err_r_pending`）
```

- **AW 的 `decode(addr)` 取自 mux 之后**（被选中 master 的 payload）⇒ 路由与授权一致。
- **W 只在 `wr_active_i` 期间有效**（AW 握手前 `wready=0` ⇒ master 持有 W，AXI 合法）；
  `wr_active` 由 AW `fire` 置位、B `fire` 清零。
- **W 用 `wr_owner`（按 slave）**，B 用同一个 owner 反查 ⇒ W/B 跟随**写路由锁存**。
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
| `wr_owner[j]` / `rd_owner[j]` | AW/AR `fire` 且 target == j 时锁存 master 索引 |
| `err_aw_seen/w_seen` · `err_b_pending` · `err_r_pending` | ERR 路的小状态机（见 §3） |

- `req_i = m_i.aw.valid & !wr_active_i`（读侧同型）⇒ **单 outstanding**（与 S4a 一致）。
- 授权保持：`grant_valid` 期间 `req` 置 0，`round_robin_selector` 的输入稳定
  ⇒ 选中不会中途漂移；握手后才把 `last_grant_oh` 推进一步 ⇒ **轮转发生在握手之后**。
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
ch::ch_layer<axi4::AxiLiteCrossbar<2, 6, SocCrossbarMap>> lite{"lite", "AxiLiteCrossbar"};
```

- `SocCrossbarMap::windows[8]` **派生**自库层表（8 条 `static_assert` 逐段钉住）：
  `kClint→ERR` · `kUart→1` · `kGpio→0` · `kSpi→3` · `kI2c→4` · `kPwm→5` · `kDma→ERR` · `kTimer→2`。
- m1（DMA 预留）tie 0；外设物理输入 tie 常量；物理输出在 lint `lint_off` 白名单登记。

## 7. 判据（手推 · 禁抄仿真）

### 7.1 单元级（**stream 平面 + AXI 端口两层**）

1. `AxiLiteCrossbar<2,3,SparseMap>`：N×M 路由完备（每对 1 写 + 1 读）。
2. 锁存：AW 握手后改 `awaddr` ⇒ W/B 仍去原 slave。
3. 在途：同 master 第二笔 AW 不被接受且不丢。
4. 仲裁：两 master 同时请求 ⇒ 轮转（连续 4 轮手推顺序）。
5. DECERR：未映射 + `ERR` 窗口 ⇒ `bresp/rresp=0b11`、不挂死。
6. W 跟随写路由：AW 与 W 相隔 1 拍。

### 7.2 外设总线回环（各 1 条 · 手推）

| 外设 | 写地址 | 写值 | 读回 |
|---|---|---|---|
| UART | `kUart+0x0C` | `0x3` | `0x3` |
| TIMER | `kTimer+0x00` | `0x1234_5678` | 同值 |
| SPI | `kSpi+0x10` | `0x7` | `0x7` |
| I2C | `kI2c+0x10` | `0x64` | `0x64` |
| PWM | `kPwm+0x00` | `0x00FF` | `0x00FF` |
| GPIO | `kGpio+0x00` / `+0x04` | `0xA5` / `gpio_in=0x5A` | A7 / A9（沿用） |

### 7.3 判别力（反向判据 · 每条实测转红）

| 变异 | 期望转红 |
|---|---|
| M1 target 改 `ERR` | 该外设回环 |
| M2 `wr_owner` 用 `rd_owner` 驱动 | 读写配对 |
| M3 去 `active` 门控 | 同 master 连续两笔 |
| M4 仲裁改固定优先（去掉 `last_grant_oh` 更新） | 轮转用例 |
| M5 ERR 只在 B 回 DECERR、R 不回 | A8 读侧 |
| M6 窗口手写字面量（差 1） | `static_assert` 编译失败 |

## 8. 影响面与回滚

- **新增**：`include/axi4/axi_lite_crossbar.h` · `tests/test_axi_lite_crossbar.cpp`。
- **修改**：`rv32i_mem_map.h`（D-1）· `rv32i_soc_top.h` · `soc-rtl-expectations.md` ·
  `examples-rtl-sim-targets.tsv` · `public-api-surface.md` · `docs/CHANGELOG.md` · 计数锚点。
- **回滚**：crossbar 是新文件、SoC 装配单处替换 ⇒ 回退 = 换回 `AxiLiteMatrix2x2`（一次 revert）。
