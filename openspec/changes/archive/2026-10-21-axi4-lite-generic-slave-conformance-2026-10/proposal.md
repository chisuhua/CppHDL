# Change: axi4-lite-generic-slave-conformance-2026-10 — 通用 AXI4-Lite slave 的协议规范形态收敛

## Why

`AxiLiteSlave`（`include/axi4/axi4_lite_slave.h`）是仓库中**唯一的通用 AXI4-Lite slave**。
6 个 `include/axi4/peripherals/*.h` 的 AXI4-Lite peripheral 已在
`axi4-peripheral-deferred-2026-10` / `fix-axi4-peripheral-rst-n-port-2026-10` 中收敛到
「`rst_n` + AW→W 相位分离 + 响应保持 + 地址锁存 + 未映射 `DECERR`」的规范形态
（见 `include/axi4/AGENTS.md` CONVENTIONS 的 Handshake pattern 段），
而 `AxiLiteSlave` **至今一条都不满足**。

2026-10-21 的审计（metis + oracle 双路独立复核，结论已逐条 grep 验证）确认
`AxiLiteSlave` 存在 9 项缺陷，其中 5 项属于「静默数据损坏 / 静默死锁」级：

| # | 缺陷 | 后果 | 证据 |
|---|---|---|---|
| A1 | **无 `rst_n` 端口**，`ch_reg` 无复位 pattern | 寄存器永不复位；且未接线的 `ch_in<ch_bool>` 读出为 0 = **永久复位**（静默、无告警） | `axi4_lite_slave.h:18-42`（`__io` 无 `rst_n`）；`:53-59`（`busy` / `reg0..reg3` 无复位 pattern） |
| A2 | **写地址不锁存**（解码用活的 `io().awaddr`） | master 在 AW 握手后改写 AWADDR（AXI 允许）→ 写落到**错误寄存器** | `:62-63` `addr_bits = io().awaddr >> 2` |
| A3 | **读地址不锁存**（`rdata` 是组合输出） | 读事务的目标随活的 `io().araddr` 抖动 → 读回错误寄存器 | `:129-130` `io().rdata = read_val`（组合） |
| A4 | **`bvalid` 是组合单拍**（`= w_handshake`） | 合规 master 看不到 BVALID → 永不拉 BREADY → `busy` 永久卡死 → 后续事务全阻塞 | `:107` `io().bvalid = w_handshake;` |
| A5 | **`rvalid` 是组合单拍**（`= ar_handshake`） | 与 A4 同类：读通道永久卡死 | `:131` `io().rvalid = ar_handshake;` |
| A6 | **`busy` 在裸 `bready` / `rready` 电平上清零** | master 常态拉高 BREADY → busy 提前释放 → 旧 B 响应未被取走就接受新 AW → **两笔写事务重叠** | `:135-137` `any_ready = select(io().bready, true, io().rready)` |
| A7 | **AW 与 W 必须同拍握手**（`wready` 由 `awready` 门控） | AXI 允许 AW 与 W 相隔任意周期 → 无法与「相位分离」型 master 互操作。**这直接导致 `examples/axi4/phase3a_complete.cpp` 的 AXI-Lite 读写路径今天是死的**（见下节「连带影响」） | `:71-72` `w_handshake = select(io().wvalid, aw_handshake, false)` |
| A8 | **未映射地址静默别名 + 响应恒 `OKAY`** | `awaddr >> 2 & 3` → 地址 `0x100` 与 `0x00` 落到同一寄存器（静默别名）；`bresp` / `rresp` 恒 0 | `:63` `& ch_uint<32>(3_d)`；`:108` / `:132` 响应恒 `ch_uint<2>(0_d)` |
| A9 | **`NUM_REGS` 模板参数被忽略**（硬编码 4 个寄存器） | 模板是「接口谎言」：`AxiLiteSlave<32,32,8>` 仍然只有 4 个寄存器 | `:15`（模板参数）vs `:56-59`（硬编码 4 个） |

### 连带影响：`phase3a_complete` 的 AXI-Lite 路径今天就是断的

`examples/axi4/phase3a_complete.cpp` 的拓扑是
`AxiInterconnect4x4` → `AxiToAxiLiteConverter` → `AxiLiteSlave`。
而 `AxiToAxiLiteConverter` 的写状态机（`include/axi4/axi_to_axilite.h:107-141`）是
**相位分离**的：

- `write_state == 0`：拉 `axil_awvalid`（`:133`）
- `write_state == 1`：**下一拍**才拉 `axil_wvalid`（`:151`）

而 `AxiLiteSlave` 要求 AW 与 W **同拍**（A7）→ W 拍 `awvalid` 已被 converter 拉低 →
`wready` 恒 0 → converter 永远停在 `write_state == 1` → **写路径死锁**。
读路径同理（converter 在 `read_state == 1` 才拉 `axil_rready`，而 slave 的 `rvalid`
是组合单拍（A5），在 `arvalid` 已拉低的那一拍必为 0）→ **读路径同样死锁**。

该 example 的 `main()` 只构建不仿真，所以从未暴露。

### 连带影响：`rst_n` 是「零编译错误的静默破坏」

新增 `ch_in<ch_bool> rst_n` 后，**未接线**的实例化点读出为 0 = 永久复位
（`include/axi4/AGENTS.md` 已登记该陷阱）→ 写不进去、读全 0，
**编译通过、CI 里表现为「测试集体红」，错误信息不指向 `rst_n`**。
因此 `rst_n` 的接线范式必须在 tasks 里写死，并有回归护栏。

## What Changes

### D1: `AxiLiteSlave` 收敛到仓库的 peripheral 规范形态

对齐 `include/axi4/peripherals/axi_gpio.h:96-196` 已验证的
`busy` / `aw_done` / `b_valid` / `r_valid` 四件套（照抄，零发明、零新原语、
已被双后端验证）：

- 新增 `ch_in<ch_bool> rst_n` 端口（置于 `__io` 末尾，仓库惯例），
  全部 `ch_reg` 加 `select(!io().rst_n, <init_val>, ...)` 复位 pattern
- `aw_done` / `b_valid` / `r_valid` 三个状态寄存器，实现 **AW→W 相位分离**
  （W 可在 AW 之后的任意周期被接受）
- **握手形式与 `axi_gpio.h` 逐字同形**：
  `w_handshake = select(io().wvalid, aw_done, ch_bool(false))`。
  由于 `aw_done` 是**寄存器**（表达式读到当前值而非 next 值），AW 握手当拍
  `aw_done` 仍为 0 → **同拍的 W 不被接受**（这是规范形态的固有性质，
  与 6 个 peripheral 及主 spec `dual-backend-test-coverage:347-354` 一致）
- `bvalid` / `rvalid` 改为由寄存器驱动，**保持到 `bready` / `rready`**；
  四个状态寄存器的 next MUST **置 1 条件优先于清 0 条件**
- `busy` 的清除条件改为「事务真正完成」（`b_valid && bready` / `r_valid && rready`），
  MUST NOT 在裸 `bready` / `rready` 上清零
- 新增 `addr_reg` / `ar_addr_reg`：AW / AR 握手时锁存地址，
  写侧与读侧解码 MUST 基于**锁存**地址；解码步长**固定 4 字节**（`addr >> 2`），
  与 `DATA_WIDTH` 无关
- 未映射地址 → `bresp` / `rresp` 返回 `DECERR`（`0b11`），MUST NOT 别名到已实现寄存器

### D2: `AxiToAxiLiteConverter` 加 `rst_n` 透传 + 状态寄存器复位

- 新增 `ch_in<ch_bool> rst_n` 端口并透传给下游 `AxiLiteSlave`（D1 的 `rst_n` 是必需端口）
- `write_state` / `read_state` 两个 `ch_reg` 加复位 pattern
  （当前二者**无任何复位 pattern** → 复位后 converter 可能停在非 IDLE 态）
- ⚠️ **MUST NOT 把 `rst_n` tie 成常量**：那会绕过 D1 的复位语义并掩盖问题

### D3: `NUM_REGS` 改为编译期诊断（`static_assert`）

`AxiLiteSlave` 实现的就是 4 个寄存器，而模板参数允许写 `8` —— 这是「接口谎言」。
决策：**保留参数 + `static_assert` 钉死**（仓库已有先例：
`include/axi4/axi_to_axilite.h:35` / `include/axi4/byte_lane.h:42-43`）。

- `static_assert(NUM_REGS == 4, "...")`
- 零行为变化、零模板元编程、零实例化点改动（5 处调用点本就传 4）
- 让「谎言」变成**编译期诊断**而不是静默接受
- ❌ 不采用「让 `NUM_REGS` 真正生效」：`ch_reg` 持 `std::unique_ptr<next_type> __next__`
  （`include/core/reg.h:59`，不可拷贝），要生成寄存器数组需本仓库**不存在**的原语，
  且默认构造会触发两条 `CHWARN`（`src/core/context.cpp:300-303`）、
  Verilog codegen 侧也会 `CHWARN`（`src/codegen_verilog.cpp:694-698`），
  违反 ZERO-DEBT 的「0 warnings」；组合节点数 O(N) 爆炸；**全仓库零使用场景**
- ❌ 不采用「直接删除参数」：会破坏 `AxiLiteSlave<32,32,4>` 的 3 参数写法（5 处调用点）

### D4: 死代码清理

- `include/axi4/byte_lane.h` 的 `byte_lane_write()` —— 全仓库零调用点
  （`grep -rn byte_lane_write` 只命中定义本身）→ 违反 ZERO-DEBT 的「无死代码」
  （⚠️ 本项与 change 主题无关，是独立的 ZERO-DEBT 清理项；已核实零风险）

### D5: 双后端覆盖 + 文档同步

- 新增 `tests/test_axi_lite_slave_conformance.cpp`（`DUAL_BACKEND_TEST_PAIR`，
  interpreter + verilator）
- 既有受影响的用例按新规范同步：
  `tests/test_axi_slave_byte_lane.cpp`（Lite 半边）、
  `tests/test_axi_lite_verilator_e2e.cpp`、`tests/test_verilator_trace_parity.cpp`
- `include/axi4/AGENTS.md`：补「通用 AXI slave 规范形态」小节 + anti-pattern + verify 命令
- 根 `AGENTS.md`：测试计数同步 + NOTES 条目

## 影响面与风险

### 端口集变化：`AxiLiteSlave` 新增 `rst_n`

全部实例化点（已 grep 全集确认，无遗漏）：

| 位置 | 形态 | 处理 |
|---|---|---|
| `examples/axi4/axi4_lite_example.cpp:53` | `ch_module<AxiLiteSlave<32,32,4>>` | 顶层暴露 `rst_n` + 透传 + TB 驱动 |
| `examples/axi4/phase3a_complete.cpp:84` | `ch_module<AxiLiteSlave<32,32,4>>`（经 converter） | 同上；**converter 是必经之路**（D2） |
| `tests/test_axi_lite_verilator_e2e.cpp:111` | `ch_module<AxiLiteSlave<32,32,4>>`（`AxiLiteTop` fixture） | fixture 暴露 `rst_n` + TB 驱动（**该文件被既有 spec 锁定**，见下） |
| `tests/test_verilator_trace_parity.cpp:165` | `ch_module<AxiLiteSlave<32,32,4>>` | 同上（**该文件被既有 spec 锁定**） |
| `tests/test_axi_slave_byte_lane.cpp:41-42` | `typedef`（`Lite32` / `Lite64`） | fixture 暴露 `rst_n` + TB 驱动 |

`Axi4Slave`（`include/axi4/axi4_full.h`）**不在本 change 范围**（见「非目标」）。

### 行为变化

- **未映射地址由「静默别名 / 恒 `OKAY`」变为 `DECERR`** —— 现有 5 处实例化点
  使用的地址均落在窗口内（`0x00` / `0x04`），逐个复核后确认无破坏面
- **AW 与 W 相位分离** —— 既有测试若假设 AW/W 同拍，必须改为两拍
- **同拍 AW+W 不再被接受**（⚠️ 行为回退点）—— 新形态要求 master 保持 `wvalid`
  直到 `wready` 拉高（AXI 合规 master 的义务）。
  - `examples/axi4/axi4_lite_example.cpp` 已是相位分离（AW cycle 0 / W cycle 1）→ 无需改
  - `tests/test_axi_slave_byte_lane.cpp` 的 `lite_write` 助手当前同拍驱动
    AW+W（`:60-68`）→ MUST 改为 AW 一拍 + W 一拍
  - `tests/test_axi_lite_verilator_e2e.cpp` / `tests/test_verilator_trace_parity.cpp`
    同拍驱动 AW+W → MUST 改为两拍（两文件均在本 change 的 MODIFIED 解锁范围内）
- **响应保持** —— `bvalid` / `rvalid` 的时序从「组合单拍」变为「保持到握手」
- **`busy` 释放时机后移** —— 事务真正完成才释放，不再被裸 `bready` 提前释放

### 既有 spec 锁冲突（**必须用 MODIFIED 解锁**）

`openspec/specs/dual-backend-test-coverage/spec.md` 有两条 Requirement 会挡住本 change：

- `:138` 「现有 9 个 verilator 测试 MUST 保留原状」——
  名单含 `test_verilator_trace_parity.cpp` / `test_axi_lite_verilator_e2e.cpp`
  （Scenario 要求 `git diff` 仅限「加 label + 顶注释」两类变更）
- `:281` 「AXI4 peripheral behavior test SHALL 走 DUAL_BACKEND_TEST_PAIR」的
  Scenario「不破既有 spec 锁」——
  「MUST NOT 改 `tests/test_verilator_*.cpp` 测试体」+「MUST NOT 改
  `tests/test_axi_lite_verilator_e2e.cpp`」

由于 `rst_n` 是**必需端口**，这两个文件**必须**改（否则 slave 永久复位、测试必红），
不改则违反已归档的主 spec → 本 change 的 spec delta MUST 包含对应的
`## MODIFIED Requirements`（见 `specs/dual-backend-test-coverage/spec.md`）。

另有 `openspec/specs/verilator-backend-e2e/spec.md:132` 的 Requirement
锁定了 `WriteReadReg0ThroughBackend` 在写拍后看到 `awready_v == 1` / `wready_v == 1` /
`bvalid_v == 1` —— 该期望与「相位分离 + busy 保持」的规范形态**互斥**
（写拍后 `awready` MUST 为 0），MUST 一并 MODIFIED
（见 `specs/verilator-backend-e2e/spec.md`）。

### 工作量与风险

D1 是**照抄** `axi_gpio.h` 的既有形态（该形态已被双后端验证），风险低；
D2 是接口扩展（新增 1 个端口 + 2 条复位 pattern），风险低；
D3 / D4 是清理，风险低。主要风险在**既有 spec 锁的解锁**与
**既有测试的时序改写**（AW/W 由同拍改两拍）。

## 非目标（Out of Scope）

- ❌ **`Axi4Slave`（`include/axi4/axi4_full.h`）** —— AXI4 **Full** slave 的
  规范形态 + burst 语义（多 beat 写 / 读、`awlen`/`awburst` 元数据锁存、
  `MAX_BURST_LEN` 死参数）拆到独立 change `axi4-full-generic-slave-burst-2026-10`。
  理由：(a) burst 语义（FIXED/INCR/WRAP、步长 `2^awsize`、读侧 `arburst`）
  **必须先定义协议语义再实现**，是独立的工作项；
  (b) `Axi4Slave` 与 burst 改造改的是**同一批状态机与寄存器**，
  拆成两轮会在同一文件上产生两轮 churn，且中间态是「半规范形态」；
  (c) 它的 spec 冲突面（`test_verilator_memory_e2e.cpp` 被锁、
  `verilator-e2e-coverage:86` 的 `rdata` 序列既存违规）与本 change 独立。
- ❌ **总线层路由** —— `AxiInterconnect4x4`（4×4）与 `AxiLiteMatrix2x2`（2×2）
  拆到独立 change `axi4-interconnect-routing-2026-10`。理由：(a) 主题不同
  （crossbar 路由 vs slave 协议形态）；(b) 审计发现 4×4 的实际损坏**远超**本 change
  原始 proposal 的描述 —— AR 通道 18 个端口 0 赋值、`m1/m2/m3` 的
  `wready/arready/bready/rready` 与 `s1/s2/s3` 的 `bready/rready` 共 18 个端口
  0 赋值、R 通道用 **AW 地址**路由、AW 仲裁**不含 AR**（纯读事务永远拿不到总线）、
  `build_decoder()` 是空壳死代码 —— 规模与风险都是独立量级；
  (c) 该组件当前**零单元测试覆盖**，无回归网。
- ❌ 6 个 `include/axi4/peripherals/*.h`（已在 `axi4-peripheral-deferred-2026-10` 收敛）
- ❌ `AxiLiteToAxiAdapter`（`axi_to_axilite.h` 的反向转换器，纯组合，无状态）
- ❌ `AxiDma`（AXI4 **master**，无寄存器写路径，字节道语义不适用）
- ❌ capability 拆分：既有 AXI4 行为契约仍留在 `dual-backend-test-coverage`，
  本次只新增 `axi-slave-conformance` 并 MODIFIED 3 条被挡住的 Requirement
- ❌ AXI4 其它协议特性（outstanding、QoS、exclusive access、cache 属性…）
- ❌ `include/axi4/AGENTS.md` 中「组合求值是单遍、无拓扑序」的**措辞修正**
  （审计发现该措辞不准确 —— `src/core/context.cpp:157` 有拓扑排序；
  真实约束是「用当前值 vs 用下一个值」的时序语义），
  列入后续 change `axi4-eval-order-doc-correction`

## Capabilities

### ADDED Requirements

见 `specs/axi-slave-conformance/spec.md`（新 capability `axi-slave-conformance`）：
通用 AXI slave 的复位、地址锁存、AW→W 相位分离、写/读响应保持、未映射地址
`DECERR`、单处 `next` 赋值、转换器复位透传、未实现能力的编译期暴露。

> ⚠️ **archive 注意**：OpenSpec 的 archive 流程**不合并 delta 文件的 `## Purpose`**
> （新 capability 的主 spec 会被建成 `TBD - created by archiving change ...` 骨架，
> 已核实于 `@fission-ai/openspec@1.4.1` 的 `dist/core/specs-apply.js:289`）。
> 因此所有 MUST 长期存在的契约性内容 MUST 写进 **Requirement 正文**（正文会被整条合并），
> 且 archive 后 MUST 另行补写主 spec 的 `## Purpose`（tasks.md Task 9）。

### MODIFIED Requirements

见 `specs/dual-backend-test-coverage/spec.md` 与 `specs/verilator-backend-e2e/spec.md`：
解锁 2 个被锁的 verilator 测试文件（`rst_n` 接线 + AW/W 时序改写），
并修正 `WriteReadReg0ThroughBackend` 的写拍信号期望。

> ⚠️ **commit 注意**：本 change 的 2 个 delta 目录
> （`specs/dual-backend-test-coverage/`、`specs/verilator-backend-e2e/`）当前是
> **untracked** → `git commit -a` **不会**包含它们 → archive 时 3 条 MODIFIED 全部丢失
> （主 spec 的锁原封不动 → 实施结果直接违反已归档主 spec，且无任何报错）。
> commit 前 MUST `git add openspec/changes/axi4-lite-generic-slave-conformance-2026-10/specs/`。
