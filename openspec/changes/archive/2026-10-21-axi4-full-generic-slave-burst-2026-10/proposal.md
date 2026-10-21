# Change: axi4-full-generic-slave-burst-2026-10 — 通用 AXI4 Full slave 的规范形态与多 beat burst 语义

## Why

`Axi4Slave`（`include/axi4/axi4_full.h`）是仓库中**唯一的通用 AXI4 Full slave**
（5 通道：AW / W / B / AR / R）。它与 AXI4-Lite 的 `AxiLiteSlave` 是两个**平级**的通用
slave 组件，但当前状态差距极大：

- `AxiLiteSlave` 已在 `axi4-lite-generic-slave-conformance-2026-10`
  （archive `2026-10-21-axi4-lite-generic-slave-conformance-2026-10`）中收敛到仓库的
  规范形态：`rst_n` + AW→W 相位分离 + 地址锁存 + 响应保持 + 未映射 `DECERR`。
- `Axi4Slave` **至今一条都不满足**，且叠加了一类 AXI4 特有的、更严重的问题：
  **多 beat burst 语义基本不存在**。

2026-10-21 的代码审计（逐条 grep 验证，证据见下表）确认 `Axi4Slave` 存在 9 项缺陷，
其中 6 项属于「静默数据损坏 / 静默丢数据 / 静默死锁」级：

| # | 缺陷 | 后果 | 证据 |
|---|---|---|---|
| B1 | **无 `rst_n` 端口**，`ch_reg` 无复位 pattern | 寄存器永不复位；且未接线的 `ch_in<ch_bool>` 读出为 0 = **永久复位**（零编译错误的静默破坏） | `axi4_full.h:58-99`（`__io` 无 `rst_n`）；`:112-131`（`SlaveState` 构造函数全部 `0_d` 初值，**无** `select(!io().rst_n, ...)` pattern） |
| B2 | **写状态机每笔事务只接受 1 个 W beat** | `write_state == 1` 时 `wready = io().wvalid`；W 握手且 `!wlast` → next state = 2，而 state 2 的 `is_aw_wait` 为 0 → **`wready` 恒 0** → 后续 beat **静默丢弃** | `:360-361`（`is_aw_wait` / `w_ready_sig`）；`:164-171`（`next_write_state`：非 `wlast` → 2）；`:356-362`（`wready` 只在 state 1 有效） |
| B3 | **写地址用活的 `io().awaddr`**，且 burst 内**不递增** | master 在 AW 握手后改写 AWADDR（AXI 允许）→ 写落到**错误寄存器**；即使不改写，多 beat burst 的所有 beat 也写**同一个**寄存器 | `:198`（`addr_index = io().awaddr >> 2`）；`:217-232`（16 个 `weN` 全部用同一个 `addr_index`） |
| B4 | **读地址只锁存一次**，burst 内每个 beat 返回**同一个**寄存器值 | INCR 读 burst 拿回 N 份相同数据 → 静默数据错误（无任何报错） | `:146`（`read_addr_reg` 在 AR 握手时锁存一次）；`:316-333`（读 mux 只按 `read_addr_reg` 选一次）；`:345`（`rdata` 恒为该值） |
| B5 | **burst 类型完全未实现** | `AxiBurst` 枚举（FIXED/INCR/WRAP）定义在 `:41-45` 但**零引用**；`awburst` / `arburst` 端口存在却被忽略 → master 的 WRAP 请求被当作 INCR 处理 | `:41-45`（enum 定义）；`:58-99`（`awburst` / `arburst` 端口）；**本文件内** grep `AxiBurst` 仅 1 处命中（定义处） |
| B6 | **`awlen` / `arlen` 语义不完整** | 写侧 `awlen` **完全被忽略**（`write_burst_cnt` 计数但从不参与地址计算或结束判定，`:150-153`）；读侧仅用 `arlen` 做一次长度比较 | `:150-153`（`write_burst_cnt` 无消费者）；`:370`（`burst_len_match = (read_burst_cnt == io().arlen)`） |
| B7 | **`MAX_BURST_LEN` 模板参数未 honor** | 模板是「接口谎言」：`Axi4Slave<32,32,4,16,64>` 的 64 长度上限不生效 | `:52`（模板参数）；全文无引用 |
| B8 | **`NUM_REGS` 模板参数未 honor** | 硬编码 16 个寄存器（`reg0..reg15`），`NUM_REGS` 改了也没用 | `:51`（模板参数）；`:115-116`（硬编码 16 个 `ch_reg`） |
| B9 | **未映射地址静默丢弃 + 响应恒 `OKAY`** | `addr_index > 15` → 16 个 `selN` 全不命中 → 写被静默吞掉，寄存器值不变，而 `bresp` 仍返回 `OKAY`（`:342`）→ master 认为写成功 | `:200-215`（`selN`）；`:234-313`（无命中即保持原值）；`:342`（`bresp` 恒 `OKAY`） |

### 连带影响：现有多 beat 写测试实际上只落了 1 拍

`tests/test_verilator_memory_e2e.cpp` 的 4-beat 写实际只落 1 拍（该测试只比较握手信号，
未断言数据），因此 B2 一直未被发现。这与 `axi4-slave-byte-lane-2026-10` 的 proposal
里记录的「多 beat burst 后续 beat 被静默丢弃」是**同一个缺陷**，本 change 是它的
正式修复立项。

### 与既有 spec 的关系

- `openspec/specs/axi-slave-conformance/spec.md` 的「通用 AXI4-Lite slave MUST 提供低有效复位」
  Requirement 已在正文中**显式声明** `Axi4Slave` 不在其管辖范围，
  并指向本 change（`axi4-full-generic-slave-burst-2026-10`）—— 本 change 是那份指向的兑现。
- `openspec/specs/dual-backend-test-coverage/spec.md` 的「AXI4 peripheral MUST 支持字节写」
  Requirement 已把 `Axi4Slave` 列入适用范围，并在 Scenario 中**显式记录**
  「多 beat burst 的逐 beat 语义当前**未实现**」—— 本 change 落地后该 Scenario 的
  限制说明 MUST 被移除（见本 change 的 spec delta）。

## What Changes

### D1: `Axi4Slave` 收敛到仓库的通用 slave 规范形态

与 `AxiLiteSlave`（`include/axi4/axi4_lite_slave.h`，已归档验证）**同源**、零发明：

- 新增 `ch_in<ch_bool> rst_n` 端口（置于 `__io` 末尾，仓库惯例），
  全部 `ch_reg` 加 `select(!io().rst_n, <init_val>, ...)` 复位 pattern
  （`<init_val>` MUST 带显式位宽，如 `ch_uint<32>(0_d)`）
- 写侧引入 `aw_done` 语义的寄存器（AW 握手后置位，事务真正完成时清零）
  → **AW→W 相位分离**：W 可在 AW 之后的任意周期被接受
  （⚠️ `aw_done` 是寄存器 → 同拍 AW+W 时该拍 W 不被接受，master MUST 保持 `WVALID` 到
  `WREADY` —— 与 6 个 peripheral、`AxiLiteSlave`、主 spec 完全一致，**不是缺陷**）
- 写/读地址**锁存**（`write_addr_reg` / `read_addr_reg`），解码 MUST 基于锁存值
- `bvalid` / `rvalid` 保持到 `bready` / `rready`；置 1 条件 MUST 优先于清 0 条件
- 未映射地址 → `DECERR`（写）/ `DECERR`（读），MUST NOT 静默丢弃
- ⚠️ **读写方向各自独立 outstanding**：写通道与读通道各自最多 1 笔在途，
  **互不阻塞**（AXI4 Full 的 AW/W/B 与 AR/R 是独立通道组）。
  `AxiLiteSlave` 的规范形态用的是**共享 `busy`**（读写互斥），
  照抄会把 `Axi4Slave` 现有的读写并发能力**降级**为互斥 → 属于行为回退，MUST NOT 发生。
  两者是「同源但不同形」：同源指 `rst_n` / 复位 pattern / 相位分离 / 地址锁存 /
  响应保持这 5 条；读写并发性因通道组不同而不同。

### D2: 写侧多 beat burst

- 写状态机 MUST 在一笔 AW 事务内接受 `awlen + 1` 个 W beat
  （`awlen` 是「beat 数 − 1」的 AXI 编码，MUST 按 `awlen + 1` 解释）
- 每一拍 MUST 用**当拍**的 `io().wstrb` 重新展开字节道掩码（已具备，MUST 保持）
- 目标地址 MUST 按 burst 类型逐 beat 计算（见 D4），MUST NOT 恒用同一个 `addr_index`
- `wlast` MUST 与预期的 beat 数一致；不一致（`wlast` 提前到达 / 缺失）→ `SLVERR`
- `awlen + 1 > MAX_BURST_LEN` → 该笔事务 MUST 以 `SLVERR` 结束，MUST NOT 静默截断

### D3: 读侧多 beat burst

- 读通道 MUST 在一笔 AR 事务内返回 `arlen + 1` 个 beat
- 每一拍的 `rdata` MUST 取**该拍对应地址**的寄存器值（MUST NOT 恒返回首拍的值 —— B4）
- `rlast` MUST 只在**最后一个 beat** 拉高
- 读 beat 计数 MUST 与 `arlen` 独立比较，MUST NOT 依赖「活」的 `arlen`
- **`rvalid` 形态 MUST 明确**：采用**流式**形态 —— `rvalid` 在整个 burst 期间连续保持为 1，
  每拍呈现一个 beat，直到 `rready` 接受该 beat 为止
  （MUST NOT 采用「每 beat 独立等待 `rready` 再出下一拍」的断续形态）
- ⚠️ **末拍 / 结束判定 MUST 用「下一个值」**（`next_beat_cnt == arlen` 之类），
  MUST NOT 用当前寄存器值比较 —— 仓库已登记该坑
  （`include/axi4/AGENTS.md` ANTI-PATTERNS：UART RX 引擎因用当前值判定结束
  而只采到 7 位）。burst 结束判定同理 MUST 基于组合出的**下一状态**。

### D4: burst 类型语义

- `INCR`（`0b01`）：地址每 beat 递增 `2^awsize` 字节（MUST 用 `awsize` / `arsize`，
  MUST NOT 硬编码 4）
  - ⚠️ `2^awsize` **MUST NOT 用运行期移位**计算 ——
    `ch_uint::operator<<` 按右操作数类型宽度推算结果宽度
    （`include/axi4/AGENTS.md` 已登记该坑）→ MUST 用 `select` 链查表
    （`awsize` 0..7 → 1/2/4/8/16/32/64/128 字节）
- `FIXED`（`0b00`）：地址每 beat 保持不变
- `WRAP`（`0b10`）：**本 change 的非目标**。MUST 以 `SLVERR` 明确拒绝
  （MUST NOT 静默当作 INCR —— 那是 B5 的静默数据损坏）
  → WRAP 的完整语义另开 change 立项
- **步长与总线宽度的关系**：解码步长固定 4 字节（`addr >> 2`），寄存器宽度为 `DATA_WIDTH`。
  `DATA_WIDTH == 64` 且 `awsize < 3` 时，相邻 beat 会落到**同一个** 64 位寄存器（窄传输）。
  本 change **不实现**窄传输的字节道重编号（Non-Goal 4），
  MUST 在文档中显式声明该限制，MUST NOT 静默产生错误的数据落点

### D5: 模板参数 honor 化

- `NUM_REGS`：MUST 真正决定寄存器数量。MUST NOT 用「变长数组」实现
  （本仓库无寄存器数组原语，且会触发 CHWARN + O(N) 节点爆炸）；
  可行路径是把 16 个寄存器改为**具名的固定集合 + 编译期 `static_assert`**
  （与 `AxiLiteSlave` 对 `NUM_REGS` 的处理同源，见
  `openspec/specs/axi-slave-conformance/spec.md`），
  或改名为 `NUM_REGS_FIXED` 明确「固定 16」语义
  - ⚠️ **这是一个决策点，MUST 在 change 内显式裁决其一**（tasks Task 8），
    MUST NOT 由实施者临场随机选
  - 补充事实：现有 3 个实例化点全部使用**位置参数**
    （`Axi4Slave<32,32,4,16,16>`）→ 参数**改名**对它们源码兼容，
    实际 API 破坏面比直觉小
- `MAX_BURST_LEN`：MUST 真正作为 burst 长度上限被检查（配合 D2 的 `SLVERR`），
  并 MUST 加 `static_assert(MAX_BURST_LEN <= 16)`
  （`awlen` 端口宽度为 4 bit，物理上限 16 beat → `> 16` 恒空转）
- `DATA_WIDTH`：MUST 加 `static_assert(DATA_WIDTH <= 64)`
  （`byte_lane.h` 已封顶 64 + Verilator 后端拒 >64 bit 端口 ADR-035 R3），
  并 MUST 修正 `include/axi4/axi4_full.h:9` 的头注释 ——
  它当前宣称支持 128–1024 位数据宽度，与实际上限矛盾
- 两个参数**任一未被 honor** → MUST 用 `static_assert` 在编译期暴露，MUST NOT 静默接受

### D6: 响应语义

- `bresp` / `rresp` MUST NOT 恒为 `OKAY`：
  未映射地址 → `DECERR`；burst 长度不符 / 不支持的 burst 类型 → `SLVERR`
- `bid` / `rid` MUST 是握手时锁存的 ID（MUST NOT 组合直出）

### D7: 双后端覆盖

- 新增 `tests/test_axi4_full_burst.cpp`（`DUAL_BACKEND_TEST_PAIR`，
  interpreter + verilator 双跑），至少覆盖：
  - 4-beat INCR 写：4 个不同寄存器被正确写入（数据断言，非仅握手断言）
  - 4-beat INCR 读：4 个不同数据被依次读回，`rlast` 只在末拍
  - FIXED burst：4 beat 写同一寄存器
  - 逐 beat `wstrb` 变化：每拍用当拍掩码
  - `awlen` 超 `MAX_BURST_LEN` → `SLVERR`
  - 不支持的 burst 类型（WRAP）→ `SLVERR`
  - 未映射地址 → `DECERR`
  - `rst_n` 复位清零全部状态
  - 64 bit 总线（`DATA_WIDTH = 64`）的 4-beat INCR 写
    （补既有 `tests/test_axi_slave_byte_lane.cpp` 的 `Full64` 覆盖面）
- ⚠️ **master stimulus MUST 复用/仿照 `tests/axi_lite_slave_fixture.h`**
  （仓库已有的协议合规 master fixture，专治「master 用从设备的 ready
  gate 自己的 valid」这一 ANTI-PATTERN）
- ⚠️ **所有握手等待 MUST 是有界循环**（如 ≤ 64 拍），超时即 FAIL ——
  「握手永不完成」类缺陷的表现是 hang 而非断言失败
- ⚠️ 现有 `tests/test_verilator_memory_e2e.cpp` 的 4-beat 写只断言握手信号，
  MUST 补数据断言（否则 B2 的回归护栏不存在）
  → 该文件受 `dual-backend-test-coverage` 的 spec 锁约束，
  MUST 先在本 change 的 spec delta 中解锁

### D8: 文档同步

- `include/axi4/AGENTS.md`：CONVENTIONS 段的「通用 AXI slave 规范形态」第 9 条
  （`NUM_REGS` / `MAX_BURST_LEN` 未 honor）MUST 更新为已修复的表述，
  并新增 burst 语义段
- 根 `AGENTS.md` 的 KNOWN LIMITATIONS / NOTES MUST NOT 再残留
  「`Axi4Slave` 仍有 `NUM_REGS` / `MAX_BURST_LEN` 两个未 honor 的参数」

## Non-Goals（非目标）

1. **WRAP burst 的完整语义**（地址回卷对齐）—— 另开 change；本 change 只做「明确 `SLVERR` 拒绝」
2. **AXI4 Exclusive Access（`awlock` / `arlock`）** —— 端口存在但语义未实现，
   本 change MUST NOT 声称支持；应明确以 `SLVERR` 拒绝或标注为非目标
3. **QoS（`awcache` / `awprot` / `arprot` 语义）** —— 端口转发即可，语义不实现
4. **窄传输（`awsize < log2(DATA_WIDTH/8)`）的字节 lane 重编号** ——
   AXI 允许窄传输；本 change 维持「按整字 lane」语义并在文档中显式声明
5. **`Axi4Slave` 的 `NUM_REGS` 变长数组化** —— 见 D5 的可行路径说明
6. **interconnect / matrix 的路由补全** —— 属独立 change
   `axi4-interconnect-routing-2026-10`

## Impact

| 项 | 影响 |
|---|---|
| `include/axi4/axi4_full.h` | 主要改动：新增 `rst_n` 端口、全部 `ch_reg` 加复位 pattern、状态机重写（写侧 4 态、读侧 3 态）、地址锁存 + 逐 beat 计算、burst 类型判定、`bresp` / `rresp` 语义化 |
| 模板参数 | `NUM_REGS` / `MAX_BURST_LEN` 的语义收口（可能改名）→ **公开 API 变更**（但现有 3 个实例化点全用位置参数 → 改名源码兼容）；另加 `static_assert(DATA_WIDTH <= 64)` / `MAX_BURST_LEN <= 16` |
| 端口 | 新增 `ch_in<ch_bool> rst_n` → **公开 API 变更**；未接线的实例化点读出 0 = 永久复位（**静默破坏**）→ MUST 全量核查实例化点 |
| 实例化点 | 全仓仅 3 处 MUST 接线 `rst_n`：`examples/axi4/axi4_full_example.cpp:77`、`tests/test_axi_slave_byte_lane.cpp:49-50`（`Full32` / `Full64` 别名，**双后端**测试）、`tests/test_verilator_memory_e2e.cpp:194` |
| 测试 | 新增 `tests/test_axi4_full_burst.cpp`（**1 个 ctest 条目**）；`tests/test_verilator_memory_e2e.cpp` 补数据断言（**受 spec 锁约束 → 需 delta 解锁**）；`tests/CMakeLists.txt` 注册 |
| spec | 新 capability `axi4-full-slave-burst`（ADDED ×7）；`dual-backend-test-coverage`（MODIFIED ×3：解锁 `test_verilator_memory_e2e.cpp` + 移除「多 beat 未实现」限制说明 + 「不破既有 spec 锁」追加例外）；`axi-slave-conformance`（MODIFIED ×1：把指向 change 名的引用改为指向 capability，避免 archive 后 dangling） |
| 文档 | `include/axi4/AGENTS.md`、根 `AGENTS.md`（NOTES 计数 + KNOWN LIMITATIONS） |
| 风险 | ⚠️ 中。**「同拍 AW+W 不被接受」不是行为变化** —— `Axi4Slave` 今天的 `wready` 已由 `write_state == 1` 门控（state 1 只能在 AW 握手后一拍进入），同拍 W 本就不被接受，3 个实例化点也已是两拍驱动 → 本 change 在该维度**行为不变**，与仓库规范形态天然一致。真正的风险在：写状态机重写 + 24 个 `ch_reg` 的复位 pattern 漏配（静默破坏），以及 burst 末拍判定踩「用当前值判定结束」的已登记坑 |
