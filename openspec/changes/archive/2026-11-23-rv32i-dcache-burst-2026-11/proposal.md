# Change: RV32I D-L1 的 AXI4 full 主口 + 真 line（S3）

> **Change 名**：`rv32i-dcache-burst-2026-11`
> **母 improvement**：`rv32i-soc-platform-2026-11`（批次 **S3** · 状态 `in-progress`）
> **决策依据**：**G-7 已裁决 = full AXI4 主干 · S3 一次做完**（2026-11-23 · 用户）
> ⇒ `.rddf/checks/decision-request-g7-bus-class-2026-11.md`
> **日期**：2026-11-23 · builder
> **性质**：**组件能力升级 + 新增 AXI slave**（含 spec **ADDED** · 见下）

## Why

1. **G-7 已定 full AXI4** ⇒ D-L1 直接接 full 主口 + 4-beat burst
   （`ARLEN=3` / `ARSIZE=2`）⇒ S6 的 L2 / DRAM **零返工**。

2. **现状两个实测事实（都在 `d_cache_sync.h`）**：
   - **无 burst**：全文 191 行**零** burst 逻辑（`awlen` / `beats` / `last` 0 命中）
   - **「假 line」**：内部 line = **128 bit**（4 字），但
     ① fill 是 `zext<128>(axi_rdata)` ⇒ 只填**低 32 位**；
     ② 读是 `bits<31,0>(cache_data)` ⇒ **`offset` 完全不参与数据选择**
     ⇒ ⇒ **同一 line 的 4 个地址恒读到第一个字**（功能错误）
     ⇒ ⇒ 当前不可观测（D-Cache 未接入流水线）⇒ 本 change 一接上就暴露。

3. **S2 打通的 `data_ready` 反压正是 cache miss 的正确等待通道**：
   - cache **hit** ⇒ `ready=1` ⇒ 流水线 1 拍拿到数据（与 TCM 语义一致）
   - cache **miss** ⇒ `ready=0` ⇒ S2 的冻结链让整条流水线停住等 fill
   ⇒ ⇒ **`rv32i-memory-timing` 的「TCM 固定 1 拍」不破**（TCM 本身没变），
   cache 层的多拍等待由**反压**表达，而不是「延长读延迟」。

## What Changes

| 面 | 内容 |
|---|---|
| `DCacheSync`（`include/cpu/cache/d_cache_sync.h`） | **前端不变**（`addr` / `wdata` / `req` / `we` → `rdata` / `ready` / `miss`）；**AXI 侧改 full 5 通道**（AR / R / AW / W / B）；读 = **4-beat burst fill**；写 = **write-through 单 beat**（`AWLEN=0`）；**修「假 line」**（读按 `addr[3:2]` 选 lane · fill 写满 4 lane · 写按 lane 更新） |
| **新增** `include/axi4/tcm_axi_slave.h` | `DataTCM` 包成 **AXI4 full slave**（支持 `ARLEN`/`AWLEN` burst ⇒ 一次 burst 铺满 line） |
| `examples/riscv-mini/src/rv32i_core_top.h` | 数据通路改为 `pipeline → DCacheSync → TcmAxiSlave → DataTCM`（**TCM 旁路**：不再直连）；`data_ready ← !data_req | cache.ready`（S2 通路第一次真正生效；⚠️ **必须带零请求放行** —— 见 AC11）；`prog_*` 仍直写 I-TCM |
| 测试 | `tests/chlib/test_cache_sync.cpp` 等的 DCache 激励从「1 beat」改「4 beat」；新增 burst 事务 / line lane / hit-miss 拍数 / TCM slave 用例；核级 24 判据保持 |

## spec delta（**ADDED** · 不是修改已签批 Requirement）

- **新增 capability `rv32i-dcache-timing`**（或扩展现有 `cpu-cache-subsystem`）：
  - `hit` ⇒ `ready` 同拍有效（**1 拍**）
  - `miss` ⇒ `ready=0` 直到 fill 完成（**反压**，不是固定延迟）
  - line = 16 B，`offset` MUST 参与数据选择；burst = `ARLEN=3` / `ARSIZE=2`
- ⚠️ **不改** `rv32i-memory-timing`：它约束的是 **TCM 的读口**（仍是 1 拍 `sread`），
  cache 是 TCM **之上**的层次 ⇒ 两者不冲突（本 change 在 design §3 给出论证）

## 验收标准

> 期望值 MUST **手推**（禁抄仿真输出）· 事务参数 MUST 从波形/端口声明派生。

- [ ] **AC1** 🔴 **burst fill 铺满 line**：miss 后 4 个 beat 的 `rdata` 全部落进 line
  ⇒ 断言：对同一 line 的 **4 个不同 offset**（0/4/8/12）写不同值后读回，**4 个值都正确**
- [ ] **AC2** 🔴 **`offset` 参与数据选择**（修「假 line」的直接判据）
  ⇒ 断言：line 的 4 个 lane 各存不同值 ⇒ 按 `addr[3:2]` 读回对应 lane
- [ ] **AC3** 🔴 **hit = 1 拍**：hit 时 `ready` 在**请求拍**即为 1；`rdata` 在**下一拍**给出
  对应 lane 的数据（与 TCM 的「地址 N ⇒ 数据 N+1」对齐）
  ⚠️ **2026-11-23 口径订正**（实施期发现）：`rdata` MUST 是**保持寄存器** ——
  `rv32i-memory-timing` 的「MEM/WB MUST 被冻结」会让 load 结果晚若干拍才装进 MEM/WB
  ⇒ ⇒ 数据 MUST 可在解冻拍重新读出（详见 `design.md` §2.4b ②）
- [ ] **AC4** 🔴 **miss = 反压**：miss 期间 `ready=0`，且流水线（S2 的冻结链）停住；
  fill 完成后 `ready=1` 并给出正确数据
- [ ] **AC5** 🔴 **burst 事务参数手推**：`ARLEN=3` · `ARSIZE=2`（4 B）· `ARBURST=INCR`
  ⇒ 断言：`ar_len`/`ar_size`/`ar_burst` 的驱动值 == 手推常量；地址 == line 对齐基址
- [ ] **AC6** 🔴 **`TcmAxiSlave` 的 burst 读写**：一次 `ARLEN=3` 读回 4 个字；一次
  `AWLEN=3` 写 4 个字 ⇒ 手推地址与值
- [ ] **AC7** 🔴 **核级零回归**：`tb_rv32i_soc` 的 **24 条判据**全过；
  `test_rv32i_soc_dual_parity` 逐拍等价（两后端）；`prog_*` 装载一致（I-TCM 不受影响）
- [ ] **AC8** **双后端**：新用例走 `DUAL_BACKEND_TEST_PAIR`（interpreter + verilator）

> **实施期新增的 3 条**（都是「一接进流水线才暴露」的，见 `design.md` §2.4b）：

- [ ] **AC9** 🔴 **握手逐请求对齐**：`ready` / `miss` MUST 是**组合**（对当拍 `addr`）；
  `req` MUST 保持到 `ready`（真握手）⇒ 连续两条访存 MUST NOT 错位
- [ ] **AC10** 🔴 **`strbe` 字节写**：前端字节使能 MUST 生效（`SB` 不破坏同字其余字节；
  AXI 侧 `w_strb` 同源）
- [ ] **AC11** 🔴 **核侧零请求放行 + 访存门控**：`data_ready = !data_req | cache.ready`
  （空闲拍 MUST NOT 冻结流水线）；`data_req` 只在 MEM 级 load/store 时为 1
  （ALU/NOP MUST NOT 触发 AXI 事务）

## Out of Scope

- **取指侧总线化 + I-L1**：S4（母 improvement 明确分工）
- **外部总线暴露到根端口**：**G-8**（未裁）⇒ 本 change 的 AXI 全在**核内部**
- **扩展互连 / 外设 / DMA**：S4
- **write-back 策略**：本 change 保持 write-through（母 improvement 未要求变更）
- **TCM 归属 / 成层决策**：母 improvement 说在 S3 一起定 ⇒ **本 change 先不动**
  （成层集合撞已签批 `rtl-hierarchical-output` ⇒ 需单独裁）

## 用户可见变更

- **`chlib::DCacheSync` 的前端多一根输入 `strbe[4]`**（字节使能）⇒ ⇒ 直连它的代码 MUST 驱动
  （未驱动 ⇒ 读回全 1 ⇒ 字节写退化为整字覆盖）。
- **前端握手语义变了**：`ready` / `miss` 是**组合**（对**当拍 `addr``）· `rdata` 是**保持寄存器**
  （完成那拍捕获、保持到下一次读完成）· `req` MUST 保持到 `ready=1`（真握手，不再是「固定延迟」）。
- **AXI 侧从简化握手（7 根）换成 AXI4 full 5 通道（19 根）** ⇒ ⇒ 接旧口的代码 MUST 改；
  读 miss = 4-beat burst（`ARLEN=3` / `ARSIZE=2` / `ARBURST=INCR`）· 写 = write-through 单 beat。
- **`riscv::Rv32iPipeline` 新增输出 `data_req`**（`MEM 级有效 & (load|store)`）⇒ ⇒ 旧消费者 MUST 驱动。
- **新增公开头 `include/axi4/tcm_axi_slave.h`**（`axi4::TcmAxiSlave`）。
- **核的行为变了**：D-L1 **首次接在数据通路上** ⇒ 命中照旧 1 拍；**miss 会反压整条流水线**
  （S2 的冻结链第一次真正生效）。⚠️ 核的 **34 端口面逐字不变**，但**产物字节会变**。
- 新 capability `rv32i-dcache-timing`（5 个 Requirement）成为**已签批规格**。

