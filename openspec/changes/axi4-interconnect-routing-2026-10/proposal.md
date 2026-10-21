# Change: axi4-interconnect-routing-2026-10 — AXI4 4×4 互连的全路由补全与 2×2 矩阵的活地址解码修正

## Why

仓库有两个 AXI 路由组件，**当前的实现都只能支撑「一部分」路由**：

| 组件 | 文件 | 名义拓扑 | 实际可用拓扑 | 单元测试 |
|---|---|---|---|---|
| `AxiInterconnect4x4` | `include/axi4/axi_interconnect_4x4.h` | 4 master × 4 slave | **1 master × 4 slave** | **零** |
| `AxiLiteMatrix2x2` | `include/axi4/axi4_lite_matrix.h` | 2 master × 2 slave | 2 × 2（但响应路由依赖**活的**地址） | **零** |

`AxiInterconnect4x4` 是仓库中**最复杂的 AXI 组件**（520 行、每 master 20 个端口 × 4），
但它**从未被任何单元测试覆盖**，也从未被仿真跑通过 ——
`examples/axi4/phase3a_complete.cpp:75` 是唯一的实例化点，而该 example 的 `main()`
**只构建不仿真**（见 `axi4-lite-generic-slave-conformance-2026-10` 的 proposal）。

2026-10-21 的代码审计（逐条 grep 验证，2026-10-21 复审复核）确认
`AxiInterconnect4x4` 存在 **15 项缺陷**，其中 11 项属于「通道根本没接 / 事务静默丢失 /
响应静默错投」级：

| # | 缺陷 | 后果 | 证据 |
|---|---|---|---|
| C1 | **master 1/2/3 的 `wready` / `arready` 从未被赋值** | 这 3 个 master 的**写通道与读通道永远不被接受**（ready 恒 0）→ 只有 m0 能用 | grep `io().m[1-3]_(w\|ar)ready *=` 命中 **0**；`:477`（仅 `m0_wready`）、`:504`（仅 `m0_arready`） |
| C2 | **slave 1/2/3 的 AR 通道（`araddr` / `arvalid` / `arid` / `arlen` / `arsize` / `arburst`）从未被赋值** | slave 1/2/3 **收不到任何读地址** → 这 3 个从设备的读路径整体不存在 | grep `io().s[1-3]_ar*= ` 命中 **0**（`:489-503` 只写 `s0_ar*`） |
| C3 | **slave 1/2/3 的 `bready` / `rready` 从未被赋值** | slave 1/2/3 的响应**永不握手** → 其 `bvalid` / `rvalid` 永远挂着，主机也永远等不到 | grep `io().s[1-3]_(b\|r)ready *=` 命中 **0** |
| C4 | **只有 m0 能路由到 slave 1/2/3** | 名义 4×4，实际 **1×4** | `:383-385` 只有 `s1_sel_m0` / `s2_sel_m0` / `s3_sel_m0`（无 `_m1.._m3`）；`:404-421` `s1/s2/s3` 的 AW mux 只有 m0 分支；`:462-476` W 通道同样只有 m0 分支 |
| C5 | **读地址不参与解码** | `build_address_map` 只看 `m*_awaddr`（`:184-187`）→ 读事务的路由依据是**写地址** | `:180-188`（`build_address_map` 四个 master 全用 `awaddr`） |
| C6 | **响应路由依据「活的」地址** | `m*_slave` 是组合量（源自活的 `io().m*_awaddr`），`build_mux` 用它选 B/R 来源（`:231-363`）→ master 抖动 AWADDR 就会把**在途响应**改投到别的 slave | `:231-236`（`m0_bresp` 按 `m0_slave` 四选一）；`:184`（`m0_slave` 源自活的 `awaddr`） |
| C7 | **AW 仲裁与路由耦合，且轮转条件错误** | `grant_mX = (arb_state == X) & req`（`:195-198`）→ 授权只在指针指到该 master 时给出；`arb_state` 的轮转条件是 `s0_awready \|\| s1_awready`（`:200-205`），**与事务是否完成无关** → 授权可能在 W 数据仍在途时切换 → W 被转发到**别的 slave** → 写数据丢失 | `:195-206`（`build_arbiter`） |
| C8 | **W 通道按「活的 grant」转发，无路由锁存** | 与 C7 叠加：AW 握手后 grant 一变，W 就被改投 | `:442-476`（W 通道 mux 的选择信号是 `s0_sel_m*` / `s1_sel_m0` …，而 `s*_sel_m*` 由活的 grant 决定） |
| C9 | **AR 通道没有独立仲裁** | 读事务复用写通道的 grant（由 `awvalid` 驱动）→ 只要有写请求，读事务就永远拿不到 grant | `:375`（grant 由 `m*_awvalid` 求得）；`:504-507`（`m*_arready` 未考虑 grant） |
| C10 | **`build_decoder()` 是空壳死代码** | 函数体只有 8 个 `(void)` 赋值 + 一段注释，实际逻辑在 `describe()` 里内联 → 死代码 | `:210-221` |
| C11 | **slave 3 的写响应 `bresp` 恒为 `DECERR`** | select 链在 `m*_slave == 3` 时返回 `DECERR` 而**不是 `s3_bresp`**（m0/m1/m2/m3 四处皆然）→ 地址 ≥ `0xC000_0000` 的写事务**永远拿到 DECERR**，尽管 `bvalid` 正确来自 s3 | `:234-236`（m0）、`:271-276`（m1）、`:302-307`（m2）、`:333-338`（m3） |
| C12 | **`m*_rresp` 全通道从未被路由** | R 通道的 `rresp` 对 4 个 master 全部悬空（未驱动读出 0 = **假 OKAY**） | grep `io().m[0-3]_rresp *=` 命中 **0** |
| C13 | **`m*_awready` / `m0_wready` / `m0_arready` 无在途门控** | master 未获授权或上一笔在途时，AW 仍可在 boundary 握手而**不被转发** → **AW 静默丢失**（比 C1 更隐蔽的丢事务路径） | `:424-439`（`m*_awready` 无 grant/active 门控）、`:477-480`、`:504-507` |
| C14 | **仲裁器无让位逻辑 → 永久饿死** | `grant_mX = (arb_state == X) & req`，而 `arb_state` 只在 `transaction_done` 时轮转，`transaction_done` 又依赖 `s*_awready`（需要 grant 才会出现）→ **若指针指向的 master 无请求，其他 master 永远拿不到 grant** | `:195-198` + `:200-205`；对照 `AxiLiteMatrix2x2:181-193` 当年修过同款缺陷（`m0_only` / `m1_only` 让位逻辑），4x4 没有 |
| C15 | **零单元测试** | 上述缺陷中至少 C1-C4 属于「一接线就会立刻暴露」的级别，却从未被发现 | `ls tests/ \| grep -i "interconnect\|matrix"` → 无输出 |

> 📌 **C11 的订正**（2026-10-21 复审）：初版提案把 C11 描述为「DECERR 分支不可达」。
> 复核后更正：`awaddr >> 30` 在 ADDR_WIDTH=32 下取 0..3，**`== 3` 完全可达**
> （地址 ≥ `0xC000_0000` 即路由 s3）→ 真实缺陷是「**slave 3 的写响应恒为 DECERR**」，
> 不可达的只有 select 链末尾的 else 分支。

> 📌 **C1 的订正**（2026-10-21 复审）：初版提案把 `m*_bready` / `m*_rready` 也列入
> 「从未被赋值」。复核后更正：这两个是 **`ch_in`**（master 驱动，
> `:64` / `:70` 等），互连本就不该赋值 → 不构成缺陷。真正的缺失只有
> `m1-3` 的 `wready` / `arready`（`ch_out`）。

### 连带影响：`AxiLiteMatrix2x2` 的同类缺陷（已修过一半）

`include/axi4/axi4_lite_matrix.h` 在 `axi4-peripheral-followups-2026-10` 期间修过一批
「端口方向声明反 → 赋值 no-op」「AWVALID/WVALID 转发完全缺失」「写地址路由读事务」
「仲裁器只看 awvalid」等缺陷，并引入了 `m0_route_s1` / `m1_route_s1` 两个**路由锁存**
（`:146-149`、`:212-213`）—— 但**响应侧仍用活的地址**：

- `io().m0_awready = select(!select_s1_m0, io().s0_awready, io().s1_awready)`（`:251`）
  —— `select_s1_m0` 来自**活的** `io().m0_awaddr`（`:152-155`）
- `io().m0_rdata = select(!ar_select_s1_m0, io().s0_rdata, io().s1_rdata)`（`:318-324`）
  —— 读响应按**活的** `araddr` 选源，而读地址**从未锁存**（2x2 没有 `ar_route` 锁存）
- `io().s0_rready = select(grant_m0_s0, ...)`（`:326-329`）—— 用**写**的 grant 选读通道的
  ready，读事务与写事务的路由会互相打架

同类缺陷在 `AxiInterconnect4x4` 上是 C6 / C8。

### 与既有 spec 的关系

- `openspec/specs/axi-slave-conformance/spec.md` 不管辖互连层；
  `dual-backend-test-coverage` 管辖「双后端一致性」，其适用对象是 peripheral 与通用 slave，
  **不含互连组件** → 本 change 需要**新 capability**。
- 根 `AGENTS.md` 的 NOTES 已把「`axi4-interconnect-routing-2026-10` —— 4×4 全路由」
  记为**后续立项**（未实施），本 change 是该立项的兑现。

## What Changes

### D1: 每个 master 的写/读路由 MUST 锁存

- 每个 master 引入 2 组状态寄存器：
  - 写侧：`write_route[m]`（2 bit，slave 索引）+ `write_active[m]`（在途标志）
  - 读侧：`read_route[m]`（2 bit）+ `read_active[m]`
- 路由 MUST 在 **AW / AR 握手当拍**由**该通道自己的地址**解码并锁存
- 后续的 W / B / R 转发 MUST 基于**锁存值**，
  MUST NOT 基于活的 `awaddr` / `araddr` / 活的 grant
- ⚠️ 形态参照 `AxiLiteMatrix2x2` 已验证的 `m0_route_s1` / `m1_route_s1` 做法
  （`include/axi4/axi4_lite_matrix.h:146-149`），但 MUST 覆盖**写侧与读侧两个方向**
  （2x2 只做了写侧）
- ⚠️ **「活的地址」在握手窗口内是合法的**：
  AXI 规定 master 在 `awvalid && !awready` 期间 MUST 保持 `awaddr` 稳定
  → 握手决策（`s*_awvalid` 转发）用当拍地址解码是唯一可行且正确的做法。
  详见 D2 的「握手决策 vs 握手后转发」分界。

### D2: 4×4 全扇出

**握手决策（AW/AR 通道）**：

- `s*_awvalid` / `s*_awaddr` 等正向转发 MUST 用**当拍**地址解码 + **当拍** grant
- `m*_awready` MUST 用**当拍**地址解码出的目标 slave 的 `awready`，
  **且 MUST 被在途标志门控**（`!write_active[m]`）——
  在途期间 MUST 为 0，否则 master 认为 AW 被接受而互连悄悄吞掉（修 C13）
- 同理 `m*_arready` MUST 被 `!read_active[m]` 门控

**握手后转发（W/B/R 通道）**：

- W 通道 MUST 按**写路由锁存**转发（16 对）
- B 通道 MUST 按**写路由锁存**反查来源（4 slave → 4 master）
- R 通道 MUST 按**读路由锁存**反查来源（4 slave → 4 master）
- `RREADY` MUST 按**读路由锁存**转发到对应 slave
- 每次 mux MUST **单次赋值**（MUST NOT 写成「先赋 m0 再读回 `io().s0_awaddr`」的形式 ——
  那是输出读自己的自环 → 拓扑排序报 `Cycle detected in IR graph`，
  参见 `AxiLiteMatrix2x2:226-233` 的修复注释）

**信号清单**（正向 6 + 反向 ready，MUST 全覆盖）：

| 通道 | 正向（master → slave） | 反向（slave → master） |
|---|---|---|
| AW | `awaddr` `awid` `awlen` `awsize` `awburst` `awvalid` | `awready` |
| W | `wdata` `wid` `wstrb` `wlast` `wvalid` | `wready` |
| B | — | `bid` `bresp` `bvalid` |
| AR | `araddr` `arid` `arlen` `arsize` `arburst` `arvalid` | `arready` |
| R | — | `rdata` `rid` `rresp` `rlast` `rvalid` |

> 📌 `WID` 是 AXI3 遗留（AXI4 已删除该信号），仓内 `Axi4Slave` 与互连都带着它
> → 本 change 只**透传**，MUST NOT 语义化。

### D3: 独立仲裁器

- **AW 通道**与 **AR 通道** MUST 各有一套独立的 round-robin 仲裁器
  （2 个 `ch_reg<ch_uint<2>>` 指针 + 各自的 grant）
- 仲裁 MUST 以**该通道的请求**为输入（写用 `awvalid`，读用 `arvalid`），
  MUST NOT 用写请求驱动读授权（C9）
- 轮转条件 MUST 是**事务真正完成**（写：目标 slave 的 `bvalid && bready`；
  读：目标 slave 的 `rvalid && rready`），MUST NOT 用「某个 slave 的 `awready` 为 1」
  （C7）—— 后者与事务无关，会在 W 数据仍在途时切换授权
  - ⚠️ 「该 slave」MUST 指**当前 grant 指向的**那个 slave，
    MUST NOT 写成「任一 slave 的 `bvalid && bready`」（那是 C7 的换皮）
- **在途事务的 master MUST 保持授权**（`active` 优先于轮询），
  与 `AxiLiteMatrix2x2:184-193` 的形态一致
- **MUST NOT 饿死**：当至少一个 master 有请求且无在途事务时，
  MUST 至少有一个 grant 为 1（修 C14 —— 需要 `m*_only` 让位逻辑）
  - ⚠️ 若采用**全局单 outstanding**（见 Non-Goal 1），则「无在途事务」时必然满足；
    若将来放宽为 per-slave 并发，本条 MUST 重新审视

### D4: 地址映射 MUST 可配置 + 未映射地址由互连层应答（error responder）

**地址映射**：

- 当前的 `awaddr >> 30`（硬编码 top-2-bit，`:184-187`）MUST 改为**编译期可配置的
  base + size 映射**（模板参数或 `static constexpr` 参数）
- 读通道 MUST 用**读地址**解码，MUST NOT 用写地址（C5）
- 窗口内的地址 MUST 原样转发给对应 slave

**未映射地址的应答（新增设计，2026-10-21 复审补充）**：

- 互连层是**纯路由**：未映射地址没有下游 slave 可应答 →
  必须由互连层自己扮演 **error responder**
- 写侧：未映射地址 MUST 被接受（`awready` 拉高），
  并由互连层产生 `bvalid = 1` + `bresp = DECERR`，
  **`bvalid` MUST 保持到 `bready`**（MUST NOT 用组合单拍脉冲 ——
  合规 master 看不到 BVALID → 永久死锁，与 `AxiLiteSlave` 修复过的 A4 同类）
- 读侧：未映射地址 MUST 被接受（`arready` 拉高），
  并由互连层返回 **`arlen + 1` 个 beat**：每拍 `rvalid = 1` + `rresp = DECERR`，
  末拍 `rlast = 1`（MUST NOT 只回 1 拍 —— master 等不到 `rlast` 会挂死）
- 该 responder 的状态 MUST 是**寄存器**（MUST NOT 组合直出）
- ⚠️ **默认映射与 DECERR 的张力**：默认映射若与 `>>30` 等价，
  则**全部** 32 位地址空间都被映射 → 默认配置下 DECERR **不可测**。
  → DECERR 用例 MUST 用**非默认配置**实例化（tasks Task 2 MUST 明确该配置）；
  同时 MUST 记录「默认配置 = 4 个 1GB 窗口，覆盖全空间」这一事实。

### D5: 互连层的复位契约 MUST 显式裁决

- 本 change 引入 4×(write_route + write_active + read_route + read_active)
  + 2 个仲裁指针 + error responder 状态 → **全是状态寄存器**
- 仓库刚把「状态寄存器 MUST 有 `rst_n` 复位 pattern」确立为规范形态
  （`openspec/specs/axi-slave-conformance/spec.md`）
  → **决策点**：互连层是否暴露 `ch_in<ch_bool> rst_n`？
  - 选项 (a) 暴露并透传 → 公开 API 变更 + `examples/axi4/phase3a_complete.cpp`
    的顶层 `rst_n` 需接线
  - 选项 (b) 不暴露 → MUST 在 spec 中**显式豁免**并写明理由
    （互连为纯路由，复位由寄存器 INIT + backend 的 reset pulse 保证）
- ⚠️ 该决策 MUST 在 change 内显式裁决，MUST NOT 由实施者临场随机选

### D6: 死代码清理

- `build_decoder()`（`:210-221`，空壳）MUST 删除
- 任何在本 change 中被替代的旧路径 MUST 一并删除（MUST NOT 留「以后再用」的死代码，
  根 AGENTS.md ZERO-DEBT POLICY）

### D7: `AxiLiteMatrix2x2` 的活地址解码修正

- 读侧 MUST 引入 `ar_route` 锁存（2x2 当前**完全没有**），
  `m*_rdata` / `m*_rresp` / `m*_rvalid` / `s*_rready` MUST 基于该锁存
- `s*_rready` MUST NOT 用**写**通道的 grant（`:326-329`）
- ⚠️ `m0_awready` / `m1_awready`（`:251-252`）的处方 MUST 是
  「**`!m*_active` 门控 + 按当拍地址解码的目标 slave 的 `awready`**」，
  **MUST NOT** 写成「按锁存的写路由」——
  反例：m0 空闲、锁存器残留上一笔的 `route = s1`，m0 发起一笔**新**的到 s0 的 AW
  → 按「锁存路由」处方 `awready = s1_awready`（错误 slave），
  若 s1 空闲则 m0 侧握手完成但 `s0_awvalid` 的转发从未被 s0 接受 → **AW 半丢失**。
  真实缺陷是 `:251` 的 `awready` **没有被 `!m0_active` 门控**（与 C13 同源）。

### D8: 单元测试（当前零覆盖）

- 新增 `tests/test_axi_interconnect_4x4.cpp`：
  - 4 个 master 各自能到达全部 4 个 slave（16 对全扇出）
  - 地址映射正确（每个 slave 拿到自己窗口内的地址）
  - 写事务：AW → W → B 全链路，**数据断言**（不只握手断言）
  - 读事务：AR → R 全链路，**数据断言**
  - 未映射地址 → 写 `DECERR`；读 → `arlen + 1` 个 `DECERR` beat 且末拍 `rlast`
  - 写/读并发（不同 master 同时发起）不串扰
  - 路由锁存：AW 握手后 master 改写 AWADDR，数据仍落到原 slave
  - W 通道不被仲裁轮转改投（回归 C7/C8）
  - 仲裁不饿死：指针指向无请求的 master 时，其他 master 仍能拿到 grant（回归 C14）
  - 在途期间 `awready` MUST 为 0（回归 C13）
- 新增 `tests/test_axi_lite_matrix_2x2.cpp`：
  - 写/读路由锁存（回归 D7）
  - 读写并发不串扰
- 两者的 MUST 走 `DUAL_BACKEND_TEST_PAIR`（interpreter + verilator 双跑）
- ⚠️ **DUT 形态 MUST 明确**：本 change 的互连测试 MUST 用 **testbench 扮演 slave**
  （驱动 4×~13 个 slave 侧输入），MUST NOT 实例化 `Axi4Slave` 当 fixture
  —— 后者会隐含依赖 `axi4-full-generic-slave-burst-2026-10` 落地（`rst_n` 端口）
- ⚠️ **所有握手等待 MUST 是有界循环**（如 ≤ 64 拍），超时即 FAIL ——
  C1 类缺陷的表现是握手**永不完成**（hang），不是断言失败；
  无界 `while(!awready) tick();` 会挂到 ctest TIMEOUT
- ⚠️ **master stimulus MUST 复用/仿照 `tests/axi_lite_slave_fixture.h`**
  （仓库已有的协议合规 master fixture，专治「master 用从设备的 ready
  gate 自己的 valid」这一 ANTI-PATTERN）

### D9: `phase3a_complete` example 的真实可运行化

- `examples/axi4/phase3a_complete.cpp` 的 `main()` 当前**只构建不仿真**
  → 本 change MUST 至少让它的 AXI-Lite 路径**真的跑起来**并断言结果
  （否则改了也无法验证）
- ⚠️ **该 example 有 2 个已确认的现存缺陷**，MUST 在本 Task 一并处理，
  否则「跑起来」会卡在未诊断的问题上：
  1. `io().done` 被 `<<=` 驱动**两次**（`:165` = `m0_bvalid`，`:187` = `m0_rvalid`）
     → 同一个 `ch_out` 双 driver
  2. `done` 组合直出 + `awvalid` / `wvalid` 同拍驱动
     → 正是上一轮 change 在 `axi_soc_example` 里修过的两类反模式；
     同拍 AW+W 会被相位分离的 `AxiToAxiLiteConverter` 拒收
- ⚠️ **工作量校准**：该 example 没有 `Simulator`、没有 master stimulus
  （ASCII 图里的 "Test Master (External)" 是概念）、s1-3 被 tie-off
  → 「跑起来」≈ 写一个 AXI4 Full master testbench + 驱动顶层 `rst_n` + 数据断言，
  且只能验证 s0 一路（**验不到本 change 的 16 对核心交付**）
  → 该 Task 的价值是**打通一条端到端链路**，核心交付的验证由 D8 的单元测试承担。
  MUST NOT 把 example 当作 16 对路由的验收手段。

### D10: 文档同步

- `include/axi4/AGENTS.md`：
  - 新增「互连层」段（地址映射、路由锁存、仲裁、error responder、复位契约）
  - STRUCTURE 表中 `axi_interconnect_4x4.h` 的行注 MUST 更新
- 根 `AGENTS.md` NOTES 中「`axi4-interconnect-routing-2026-10` —— 4×4 全路由」
  MUST 改写为已实施

## Non-Goals（非目标）

1. **多笔 outstanding 事务** —— 本 change 采用**全局单 outstanding**：
   写侧全互连最多 1 笔在途，读侧全互连最多 1 笔在途
   （D3 的单 AW 仲裁器 + 单 AR 仲裁器 + active 保持的直接后果）。
   ⚠️ 这**不是**真正的 crossbar 并发（真实 crossbar 用 per-slave 仲裁器实现并发）。
   若将来要放开，路由反查索引的数量与 per-master 锁存都要重做 → 另开 change。
2. **AXI4 Exclusive Access**（`awlock` / `arlock`）—— 端口存在但语义不实现
3. **QoS / `awcache` / `awprot` / `arprot` 语义** ——
   ⚠️ `AxiInterconnect4x4` 的 `__io`（`:40-161`）**根本没有**这些端口
   （下游 `Axi4Slave` 有）→ 本 change **不转发、也不增设**这些端口
   （增设属公开 API 变更，另开 change）
4. **窄传输（`awsize < log2(DATA_WIDTH/8)`）的字节道重编号**
5. **把 `AxiInterconnect4x4` 改造成 AXI4-Lite 协议**（它是 AXI4 Full 5 通道组件）
6. **`AxiToAxiLiteConverter` 的改造** —— 属 `axi4-lite-generic-slave-conformance-2026-10`
   已完成范围，本 change MUST NOT 回退其行为
7. **互连层的性能优化 / 流水线化** —— 本 change 只求**正确性**
8. **perf 基线** —— 互连此前零测试 → 无 perf 回归面，
   `perf_baseline.json` 无需更新（显式记录，避免 reviewer 追问）

## Impact

| 项 | 影响 |
|---|---|
| `include/axi4/axi_interconnect_4x4.h` | 主要改动：新增路由锁存与两套仲裁器状态、error responder 状态；16 对 AW/W/AR mux 全量展开；B/R 反向 mux 按锁存索引重写；地址映射参数化；删除 `build_decoder` 死代码。**文件规模会显著增长**（当前 520 行 → 预计 900+ 行）→ MUST 保留并扩展现有 4 个 helper 的切分点（`build_address_map` / `build_arbiter` / `build_decoder` / `build_mux`） |
| `include/axi4/axi4_lite_matrix.h` | 读侧路由锁存 + 响应侧改用锁存；`awready` 加 `!active` 门控 |
| 公开 API | `AxiInterconnect4x4` 的**端口集合**原则上不变（无需用户改接线）；但**地址映射从硬编码改为可配置**、**是否新增 `rst_n`** → 若新增则是**公开 API 变更**（默认值 MUST 保持与当前 `>>30` 行为等价，避免破坏 `examples/axi4/phase3a_complete.cpp`） |
| 实例化点 | `examples/axi4/phase3a_complete.cpp:75`（4×4）、`examples/axi4/axi_soc_example.cpp:195`（2×2）—— MUST 保持可编译 |
| 测试 | 新增 `tests/test_axi_interconnect_4x4.cpp` + `tests/test_axi_lite_matrix_2x2.cpp`（**各 1 个 ctest 条目** → CTEST_TOTAL **+2**）；`tests/CMakeLists.txt` 注册（`add_dual_backend_test` MUST 用 `SOURCES` 关键字形式，`TIMEOUT 600` —— 4×4 全展开的组合 mux 编译量大，参照 `test_verilator_complex_e2e` 的 600） |
| spec | 新 capability `axi-interconnect-routing`（ADDED ×5） |
| 文档 | `include/axi4/AGENTS.md`、根 `AGENTS.md`（NOTES 计数 + 立项状态） |
| 风险 | ⚠️ **高**。涉及 5 个通道 × 16 对的组合展开，逻辑密度远高于此前任何一个 change；且当前**零测试基线** → MUST 先建 RED 基线（能观测到 C1-C4 的具体症状）再改实现。另：`phase3a_complete` 的「跑起来」是**独立于**本 change 核心交付的一笔工作量（D9） |
