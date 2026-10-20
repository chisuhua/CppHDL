# Change: axi4-generic-slave-conformance-2026-10 — 通用 AXI slave 的协议规范形态收敛

## Why

`axi4-slave-byte-lane-2026-10`（archive `2026-10-20-axi4-slave-byte-lane-2026-10`）
让 2 个通用 AXI slave 消费了 `wstrb` 字节掩码，但**只解决了写掩码这一件事**。
审计（2026-10-21）发现这两个通用 slave 与仓库里 6 个 AXI4-Lite peripheral 的
**协议规范形态**差距很大，且多处缺陷属于"静默数据损坏"级别：

### A. `AxiLiteSlave`（`include/axi4/axi4_lite_slave.h`）

| # | 缺陷 | 后果 | 证据 |
|---|---|---|---|
| A1 | **无 `rst_n` 端口**，`ch_reg` 无复位 pattern | 寄存器永不复位；verilator 侧只靠 init 值 | `:17-41` 无 rst_n 端口 |
| A2 | **地址不锁存**：写解码用活的 `io().awaddr` | master 在 AW 握手后改写 AWADDR（AXI 允许）→ 写落到**错误寄存器** | `:61-62` `addr_bits = io().awaddr >> 2` |
| A3 | **`bvalid` 是组合单拍**（`= w_handshake`），不保持到 `BREADY` | 合规 master 看不到 BVALID → 永不拉 BREADY → `busy` 永久卡死 → 后续事务全阻塞 | `:107` `io().bvalid = w_handshake;` |
| A4 | **未映射地址别名**：`awaddr >> 2 & 3` | 地址 `0x100` 与 `0x00` 落到同一寄存器（静默别名），且响应恒 `OKAY` | `:61-62`；`:108` `bresp = 0` |
| A5 | **`NUM_REGS` 模板参数被忽略**（硬编码 4 个寄存器） | 模板是"接口谎言"：`AxiLiteSlave<32,32,8>` 仍然只有 4 个寄存器 | `:15` vs `:55-58` |
| A6 | AW 与 W 必须**同拍**握手（`wready` 由 `awready` 门控） | AXI 允许 AW 与 W 相隔任意周期 → 该 slave 无法与"相位分离"型 master 互操作 | `:70-71` |

### B. `Axi4Slave`（`include/axi4/axi4_full.h`）

| # | 缺陷 | 后果 | 证据 |
|---|---|---|---|
| B1 | **写 burst 每笔事务只接受 1 个 W beat** | `awlen > 0` 的写 burst，后续 beat 被**静默丢弃**；`tests/test_verilator_memory_e2e.cpp` 驱动的 4-beat 写实际只落 1 拍（该测试只比较握手信号、未断言数据，故一直没暴露） | `:361` `wready = is_aw_wait && wvalid`；`!wlast` 后进入的状态 2 不再拉 `wready`（`:164-172`） |
| B2 | **写地址不锁存** + **burst 地址不递增** | 写目标由活的 `io().awaddr` 决定 → AW 后改地址即写错寄存器；即便地址正确，所有 beat 也写**同一个**寄存器（真正的 INCR burst 未实现） | `:194` `addr_index = io().awaddr >> 2` |
| B3 | **读 burst 不工作** | `read_burst_cnt` 只在 `is_read_idle`（state==0）时递增；处于 R_DATA（state 2）时不递增 → `burst_len_match` 永远为假 → `arlen > 0` 时 **RVALID 永久保持**、rdata 恒为首拍地址的值、`rlast` 恒 0 | `:156-160`、`:370`、`:349` |
| B4 | **未映射地址静默丢弃**：`addr_index` 与 0..15 逐一比较，越界则无 `weN` 命中 | 写不生效但响应仍 `OKAY`；读回 0 且 `rresp = OKAY` | `:196-211`；`:342`/`:347` 响应恒 OKAY |
| B5 | **无 `rst_n`**，`ch_reg` 无复位 pattern | 同 A1 | `:57-98` 无 rst_n 端口 |
| B6 | **`NUM_REGS` 模板参数被忽略**（硬编码 16 个寄存器） | 同 A5 | `:51` vs `:115-116` |
| B7 | `write_burst_cnt` 只写不用 | 死状态（修完 B1 后要么用于校验，要么删） | `:151-152` |

### C. 总线层（既有问题，被本次字节道改动**放大**）

| # | 缺陷 | 后果 | 证据 |
|---|---|---|---|
| C1 | `AxiInterconnect4x4`（`include/axi4/axi_interconnect_4x4.h`）**只有 m0 能通达 s1/s2/s3**（AW/W/AR 全通道都是 `select(sN_sel_m0, m0_*, 0)`） | 名义 4×4 实际是 1×4；m1/m2/m3 只能通达 s0 | `:404-425`、`:435-476`；`grep -c "m1_to_s1" = 0` |
| C2 | 4×4 interconnect 与 2×2 matrix **零单元测试覆盖** | 上述路由缺陷没有任何回归护栏（只有 example 覆盖，且 example 只走 m0→s0） | `grep -rl "axi_interconnect_4x4\|axi4_lite_matrix" tests/` → 无命中 |

> ⚠️ **字节道改动放大的部分**：`s1_wstrb` 等未命中通路时输出 `0`。
> 改动前 slave 忽略 `wstrb`（整字写），改动后 `wstrb == 0` 意味着
> **"无字节被选中"的空写** → 若 C1 被单独修复而 `wstrb` 通路没同步补全，
> 下游 slave 会把合法写变成空写（静默丢数据）。
> **故 C1 的修复 MUST 与 `wstrb`/`wlast`/`wid` 通路同步落地。**

### D. 死代码

- `include/axi4/byte_lane.h` 的 `byte_lane_write()` —— 全仓库零调用点
  （`grep -rn byte_lane_write` 只命中定义本身）→ 违反 ZERO-DEBT 的"无死代码"

## What Changes

### D1: `AxiLiteSlave` 收敛到仓库的 peripheral 规范形态

对齐 `include/axi4/AGENTS.md` CONVENTIONS 已登记的形态
（`rst_n` 低有效异步复位 + 全部 `ch_reg` 复位 pattern / 地址锁存 /
`bvalid` 保持到 `bready` / 未映射地址 `DECERR`）：

- 新增 `ch_in<ch_bool> rst_n` 端口，4 个 `ch_reg` 加 `select(!io().rst_n, <init>, ...)`
- 新增 `aw_addr_reg`（AW 握手时锁存 `io().awaddr`），写/读解码基于**锁存**地址
- `bvalid` 改为寄存器：写握手时置位，**保持到 `bready`**，握手后清零
- 未映射地址 → `bresp` / `rresp` 返回 `DECERR`，值不变
- `NUM_REGS` 真正生效（按 `NUM_REGS` 生成寄存器组）或删除该模板参数（见任务 2 的决策点）

### D2: `Axi4Slave` 收敛到规范形态 + burst 语义

- 新增 `ch_in<ch_bool> rst_n` 端口 + 全部 `ch_reg` 复位 pattern
- 新增 `aw_addr_reg`：AW 握手时锁存 `io().awaddr`；写目标基于锁存地址
- 写 burst：W 通道在 `aw_wait` 与 `burst_data` 两态都可握手，直到 `wlast`；
  每个 beat 的地址按 `awburst` 推进（INCR → +`DATA_WIDTH/8` 字节 / FIXED → 不变）
- 读 burst：`read_burst_cnt` 在 R_DATA 态按 R 握手递增，`arlen` 匹配才回 idle；
  `rdata` 按当前 beat 地址更新；`rlast` 在最后一拍为 1
- 未映射地址 → `bresp` / `rresp` 返回 `DECERR`
- `NUM_REGS` 真正生效或删除（决策点）

### D3: 总线层路由完整度

- `AxiInterconnect4x4` 补 m1/m2/m3 → s1/s2/s3 的全通道路由
  （AW / W（含 `wstrb`/`wlast`/`wid`）/ B / AR / R），与 m0 通路同构
- `axi4_lite_matrix.h`（2×2）复核同样的通路完整性
- 补单元测试：4×4 interconnect 与 2×2 matrix 的路由（当前零单测）

### D4: 死代码清理

- `byte_lane_write()`：删除，或补一个真实调用点 + 用例
- `Axi4Slave::write_burst_cnt`：修完 burst 后用于校验 beat 数，或删除

### D5: 双后端覆盖 + 文档同步

- 新增/受影响的用例统一走 `add_dual_backend_test`（interpreter + verilator）
- `include/axi4/AGENTS.md` 补"通用 AXI slave 规范形态"小节 + anti-pattern
- 根 `AGENTS.md` 测试计数同步 + NOTES 条目

## 影响面与风险

- **端口集变化**：`AxiLiteSlave` / `Axi4Slave` 新增 `rst_n` → 所有实例化点必须接线，
  未驱动的 `ch_in<ch_bool>` 读出为 0 = **永久复位**（`include/axi4/AGENTS.md` 已登记该陷阱）：
  - `examples/axi4/axi4_lite_example.cpp`
  - `examples/axi4/axi4_full_example.cpp`
  - `examples/axi4/phase3a_complete.cpp`
  - `tests/test_axi_lite_verilator_e2e.cpp`
  - `tests/test_verilator_memory_e2e.cpp`
  - `tests/test_verilator_trace_parity.cpp`
  - `tests/test_axi_slave_byte_lane.cpp`（本次新增）
- **行为变化**：未映射地址由"静默别名 / 静默丢弃"变为 `DECERR` —— 现有 example 若
  依赖别名行为会失败（需在实施时逐个确认）
- **burst 语义变化**：`Axi4Slave` 写 burst 由"只落 1 拍"变为"落全部 beat" ——
  `tests/test_verilator_memory_e2e.cpp` 的 4-beat 写序列的最终值会变（当前无数据断言）
- **工作量**：D1/D2 是协议状态机改造，风险集中在 `Axi4Slave` 的读写 burst；
  D3 是纯组合路由补全，风险低但面积大（4×4 通道 × 3 个 slave × 3 个 master）

## 非目标（Out of Scope）

- ❌ `AxiDma`（AXI4 **master**，无寄存器写路径，字节道语义不适用）
- ❌ 6 个 `include/axi4/peripherals/*.h`（已在
  `axi4-peripheral-deferred-2026-10` 收敛到规范形态）
- ❌ `axi_to_axilite.h`：`static_assert(DATA_WIDTH == 32)` → 其 4-bit `wstrb` 无截断问题
- ❌ capability 拆分：把既有 AXI4 行为契约从名不副实的
  `dual-backend-test-coverage` 迁到新 capability —— 上一轮 change 已判定"单独立项"，
  本次**不与行为改造混做**（避免 MODIFIED 整条替换导致 spec 内容丢失）；
  新增的规范形态契约落在新 capability `axi-slave-conformance`
- ❌ AXI4 其它协议特性（outstanding、QoS、exclusive access、cache 属性…）

## Capabilities

### ADDED Requirements

见 `specs/axi-slave-conformance/spec.md`（新 capability）：
通用 AXI slave 的复位、地址锁存、响应保持、未映射地址 `DECERR`、burst 语义，
以及总线层路由对 `wstrb` 的完整透传。
