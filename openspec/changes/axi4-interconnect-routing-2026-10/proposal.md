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

2026-10-21 的代码审计（逐条 grep 验证）确认 `AxiInterconnect4x4` 存在 12 项缺陷，
其中 9 项属于「通道根本没接 / 事务静默丢失 / 响应静默错投」级：

| # | 缺陷 | 后果 | 证据 |
|---|---|---|---|
| C1 | **master 1/2/3 的 `wready` / `arready` / `bready` / `rready` 从未被赋值** | 这 3 个 master 的**写通道与读通道永远不被接受**（ready 恒 0）→ 只有 m0 能用 | grep `io().m{1,2,3}_(w\|ar\|b\|r)ready *=` 命中 **0**；`:477`（仅 `m0_wready`）、`:504`（仅 `m0_arready`）、`:483`（仅 `s0_bready`）、`:510`（仅 `s0_rready`） |
| C2 | **slave 1/2/3 的 AR 通道（`araddr` / `arvalid` / `arid` / `arlen` / `arsize` / `arburst`）从未被赋值** | slave 1/2/3 **收不到任何读地址** → 这 3 个从设备的读路径整体不存在 | grep `io().s{1,2,3}_ar*= ` 命中 **0**（`:489-503` 只写 `s0_ar*`） |
| C3 | **slave 1/2/3 的 `bready` / `rready` 从未被赋值** | slave 1/2/3 的响应**永不握手** → 其 `bvalid` / `rvalid` 永远挂着，主机也永远等不到 | grep `io().s{1,2,3}_(b\|r)ready *=` 命中 **0** |
| C4 | **只有 m0 能路由到 slave 1/2/3** | 名义 4×4，实际 **1×4** | `:383-385` 只有 `s1_sel_m0` / `s2_sel_m0` / `s3_sel_m0`（无 `_m1.._m3`）；`:404-421` `s1/s2/s3` 的 AW mux 只有 m0 分支；`:462-476` W 通道同样只有 m0 分支 |
| C5 | **读地址不参与解码** | `build_address_map` 只看 `m*_awaddr`（`:184-187`）→ 读事务的路由依据是**写地址** | `:180-188`（`build_address_map` 四个 master 全用 `awaddr`） |
| C6 | **响应路由依据「活的」地址** | `m*_slave` 是组合量（源自活的 `io().m*_awaddr`），`build_mux` 用它选 B/R 来源（`:231-363`）→ master 抖动 AWADDR 就会把**在途响应**改投到别的 slave | `:231-236`（`m0_bresp` 按 `m0_slave` 四选一）；`:184`（`m0_slave` 源自活的 `awaddr`） |
| C7 | **AW 仲裁与路由耦合，且轮转条件错误** | `grant_mX = (arb_state == X) & req`（`:195-198`）→ 授权只在指针指到该 master 时给出；`arb_state` 的轮转条件是 `s0_awready \|\| s1_awready`（`:200-205`），**与事务是否完成无关** → 授权可能在 W 数据仍在途时切换 → W 被转发到**别的 slave** → 写数据丢失 | `:195-206`（`build_arbiter`） |
| C8 | **W 通道按「活的 grant」转发，无路由锁存** | 与 C7 叠加：AW 握手后 grant 一变，W 就被改投 | `:442-476`（W 通道 mux 的选择信号是 `s0_sel_m*` / `s1_sel_m0` …，而 `s*_sel_m*` 由活的 grant 决定） |
| C9 | **AR 通道没有独立仲裁** | 读事务复用写通道的 grant（由 `awvalid` 驱动）→ 只要有写请求，读事务就永远拿不到 grant | `:375`（grant 由 `m*_awvalid` 求得）；`:504-507`（`m*_arready` 由 `m*_slave` 与 `s*_arready` 组合，未考虑 grant） |
| C10 | **`build_decoder()` 是空壳死代码** | 函数体只有 8 个 `(void)` 赋值 + 一段注释，实际逻辑在 `describe()` 里内联 → 死代码 | `:210-221` |
| C11 | **DECERR 分支不可达** | `m*_slave` 由 `awaddr >> 30` 得到，恒为 0..3 → `:234-236` 的 DECERR 分支永不触发 | `:184-187`（`>> 30`，ADDR_WIDTH=32 时恒 2 bit）；`:234-236` |
| C12 | **零单元测试** | 上述 11 项缺陷中至少 C1-C4 属于「一接线就会立刻暴露」的级别，却从未被发现 | `ls tests/ \| grep -i "interconnect\|matrix"` → 无输出 |

### 连带影响：`AxiLiteMatrix2x2` 的同类缺陷（已修过一半）

`include/axi4/axi4_lite_matrix.h` 在 `axi4-peripheral-followups-2026-10` 期间修过一批
「端口方向声明反 → 赋值 no-op」「AWVALID/WVALID 转发完全缺失」「写地址路由读事务」
「仲裁器只看 awvalid」等缺陷，并引入了 `m0_route_s1` / `m1_route_s1` 两个**路由锁存**
（`:146-149`、`:212-213`）—— 但**响应侧仍用活的地址**：

- `io().m0_awready = select(!select_s1_m0, io().s0_awready, io().s1_awready)`（`:251`）
  —— `select_s1_m0` 来自**活的** `io().m0_awaddr`（`:152-155`），
  master 在 AW 握手后改写 AWADDR（AXI 允许）→ `awready` 指向**另一个 slave**
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
- 后续的 W / B / R 转发 MUST 基于**锁存的路由**，
  MUST NOT 基于活的 `awaddr` / `araddr` / 活的 grant
- ⚠️ 形态参照 `AxiLiteMatrix2x2` 已验证的 `m0_route_s1` / `m1_route_s1` 做法
  （`include/axi4/axi4_lite_matrix.h:146-149`），但 MUST 覆盖**写侧与读侧两个方向**
  （2x2 只做了写侧）

### D2: 4×4 全扇出

- **AW 通道**：4 个 master → 4 个 slave 的**全部 16 对** MUST 被路由
  （`s*_awaddr` / `s*_awid` / `s*_awlen` / `s*_awsize` / `s*_awburst` / `s*_awvalid`）
- **W 通道**：同上 16 对（含 `wdata` / `wid` / `wstrb` / `wlast` / `wvalid`）
- **AR 通道**：同上 16 对（含 `araddr` / `arid` / `arlen` / `arsize` / `arburst` / `arvalid`）
- **B 通道**（反向）：4 个 slave → 4 个 master，按**写路由锁存**反查
  （含 `bid` / `bresp` / `bvalid` / `bready`）
- **R 通道**（反向）：同上，按**读路由锁存**反查
  （含 `rdata` / `rid` / `rresp` / `rlast` / `rvalid` / `rready`）
- 每次 mux MUST **单次赋值**（MUST NOT 写成「先赋 m0 再读回 `io().s0_awaddr`」的形式 ——
  那是输出读自己的自环 → 拓扑排序报 `Cycle detected in IR graph`，
  参见 `AxiLiteMatrix2x2:226-233` 的修复注释）

### D3: 独立仲裁器

- **AW 通道**与 **AR 通道** MUST 各有一套独立的 round-robin 仲裁器
  （2 个 `ch_reg<ch_uint<2>>` 指针 + 各自的 grant）
- 仲裁 MUST 以**该通道的请求**为输入（写用 `awvalid`，读用 `arvalid`），
  MUST NOT 用写请求驱动读授权（C9）
- 轮转条件 MUST 是**事务真正完成**（写： slave 的 `bvalid && bready`；
  读： slave 的 `rvalid && rready`），MUST NOT 用「某个 slave 的 `awready` 为 1」
  （C7）—— 后者与事务无关，会在 W 数据仍在途时切换授权
- **在途事务的 master MUST 保持授权**（`active` 优先于轮询），
  与 `AxiLiteMatrix2x2:184-193` 的形态一致

### D4: 地址映射 MUST 可配置且能产生 DECERR

- 当前的 `awaddr >> 30`（硬编码 top-2-bit，`:184-187`）MUST 改为**编译期可配置的
  base + size 映射**（模板参数或 `static constexpr` 参数）
- MUST 存在**未映射区域**的语义（否则 DECERR 分支永远不可达，C11）
- 未映射地址的写/读 MUST 返回 `DECERR`（与 `AxiLiteSlave` 的未映射语义一致，
  见 `openspec/specs/axi-slave-conformance/spec.md`）
- 读通道 MUST 用**读地址**解码，MUST NOT 用写地址（C5）

### D5: 死代码清理

- `build_decoder()`（`:210-221`，空壳）MUST 删除
- 任何在本 change 中被替代的旧路径 MUST 一并删除（MUST NOT 留「以后再用」的死代码，
  根 AGENTS.md ZERO-DEBT POLICY）

### D6: `AxiLiteMatrix2x2` 的活地址解码修正

- `m0_awready` / `m1_awready`（`:251-252`）MUST 基于**锁存的写路由**，
  MUST NOT 基于活的 `awaddr`
- 读侧 MUST 引入 `ar_route` 锁存（2x2 当前**完全没有**），
  `m*_rdata` / `m*_rresp` / `m*_rvalid` / `s*_rready` MUST 基于该锁存
- `s*_rready` MUST NOT 用**写**通道的 grant（`:326-329`）

### D7: 单元测试（当前零覆盖）

- 新增 `tests/test_axi_interconnect_4x4.cpp`：
  - 4 个 master 各自能到达全部 4 个 slave（16 对全扇出）
  - 地址映射正确（每个 slave 拿到自己窗口内的地址）
  - 写事务：AW → W → B 全链路，**数据断言**（不只握手断言）
  - 读事务：AR → R 全链路，**数据断言**
  - 未映射地址 → `DECERR`
  - 写/读并发（不同 master 同时发起）不串扰
  - 路由锁存：AW 握手后 master 改写 AWADDR，数据仍落到原 slave
  - W 通道不被仲裁轮转改投（回归 C7/C8）
- 新增 `tests/test_axi_lite_matrix_2x2.cpp`：
  - 写/读路由锁存（回归 D6）
  - 读写并发不串扰
- 两者的 MUST 走 `DUAL_BACKEND_TEST_PAIR`（interpreter + verilator 双跑）

### D8: 文档同步

- `include/axi4/AGENTS.md`：
  - 新增「互连层」段（地址映射、仲裁、路由锁存、DECERR 语义）
  - STRUCTURE 表中 `axi_interconnect_4x4.h` 的行注 MUST 更新（不再声称是 4×4 全路由，
    或更新为修复后的真实语义）
  - 根 `AGENTS.md` NOTES 中「`axi4-interconnect-routing-2026-10` —— 4×4 全路由」
    MUST 改写为已实施

## Non-Goals（非目标）

1. **多笔 outstanding 事务**（每 master 同时挂多笔写 + 多笔读）—— 本 change 维持
   「每 master 写侧 1 笔 + 读侧 1 笔在途」
2. **AXI4 Exclusive Access**（`awlock` / `arlock`）—— 端口存在但语义不实现
3. **QoS / `awcache` / `awprot` / `arprot` 语义** —— 仅端口转发
4. **窄传输（`awsize < log2(DATA_WIDTH/8)`）的字节道重编号**
5. **把 `AxiInterconnect4x4` 改造成 AXI4-Lite 协议**（它是 AXI4 Full 5 通道组件）
6. **`AxiToAxiLiteConverter` 的改造** —— 属 `axi4-lite-generic-slave-conformance-2026-10`
   已完成范围，本 change MUST NOT 回退其行为
7. **互连层的性能优化 / 流水线化** —— 本 change 只求**正确性**

## Impact

| 项 | 影响 |
|---|---|
| `include/axi4/axi_interconnect_4x4.h` | 主要改动：新增路由锁存与两套仲裁器状态；16 对 AW/W/AR mux 全量展开；B/R 反向 mux 按锁存索引重写；地址映射参数化；删除 `build_decoder` 死代码。**文件规模会显著增长**（当前 520 行 → 预计 900+ 行）→ MUST 考虑按 C-CLASS REFACTOR 的做法把 5 个通道拆成独立 helper（现有 `build_address_map` / `build_arbiter` / `build_decoder` / `build_mux` 4 个 helper 的切分点保留并扩展） |
| `include/axi4/axi4_lite_matrix.h` | 读侧路由锁存 + 响应侧改用锁存；`awready` 改用锁存路由 |
| 公开 API | `AxiInterconnect4x4` 的**端口集合不变**（无需用户改接线）；但**地址映射从硬编码改为可配置** → 若新增模板参数则是**公开 API 变更**（默认值 MUST 保持与当前 `>>30` 行为等价，避免破坏 `examples/axi4/phase3a_complete.cpp`） |
| 实例化点 | `examples/axi4/phase3a_complete.cpp:75`（4×4）、`examples/axi4/axi_soc_example.cpp:195`（2×2）—— MUST 保持可编译；`phase3a_complete` 的 `main()` 目前**只构建不仿真**，本 change MUST 至少让 example 的 AXI-Lite 路径**真的跑起来**（否则改了也无法验证） |
| 测试 | 新增 `tests/test_axi_interconnect_4x4.cpp` + `tests/test_axi_lite_matrix_2x2.cpp`；`tests/CMakeLists.txt` 注册（`add_dual_backend_test` MUST 用 `SOURCES` 关键字形式） |
| spec | 新 capability `axi-interconnect-routing`（ADDED） |
| 文档 | `include/axi4/AGENTS.md`、根 `AGENTS.md`（NOTES 计数 + 立项状态） |
| 风险 | ⚠️ **高**。涉及 5 个通道 × 16 对的组合展开，逻辑密度远高于此前任何一个 change；且当前**零测试基线** → MUST 先建 RED 基线（能观测到 C1-C4 的具体症状）再改实现 |
