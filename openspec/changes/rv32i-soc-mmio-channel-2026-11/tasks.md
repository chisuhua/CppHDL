# tasks · S4a（核的 MMIO 通道 + SoC 侧 Lite 互联 + GPIO 回环）

> **Change**：`rv32i-soc-mmio-channel-2026-11`
> **前置**：`rv32i-commit-event-pulse-2026-11`（**S4a-0**）· **MUST 先落地并归档**
> **纪律**：TDD 5 步（**先见红**）· 每步小迭代 · 每步跑验证 · 提交**单独成段**
> **零行为变化**（对非 MMIO 路径）⇒ ⇒ 核级 24 条判据**零变化**

## P0 · 前置确认

- [x] **T0.1** ✅ 确认 **S4a-0 已归档**（`openspec/changes/archive/2026-11-24-rv32i-commit-event-pulse-2026-11/`）（`openspec/changes/archive/**/rv32i-commit-event-pulse-2026-11/`）
  ⇒ ⇒ 未归档 ⇒ **STOP**（AC3 的计数口径依赖它）
- [x] **T0.2** ✅ 库层地址表已有 **7 个 MMIO 段**（P1.1 落地） + `kMmioBase` / `kMmioBytes`
  ⇒ ⇒ 缺失 ⇒ 先做 T1.1

## P1 · 库层（G-6）

- [x] **T1.1** ✅ `include/cpu/riscv/rv32i_mem_map.h` ⇒ 新增 7 个 MMIO 段 + `kMmioBase` / `kMmioBytes`
  ⇒ ⇒ 每段 4 KB · 自 `0x8010_0000` 顺延 · **禁手写 0x7000**（派生）
  ⇒ ⇒ 加 `static_assert` 连续性 + 与 TCM 段不重叠
- [x] **T1.2** ✅ `include/cpu/riscv/address_decoder.h` ⇒ 3 段硬编码改派生 + 补 4 片选
  ⇒ ⇒ **总片选 9**（2 TCM + 7 MMIO）+ `error_cs` ⇒ **10** 个输出端口
  ⇒ ⇒ 文件头表格的「I-TCM 64KB / D-TCM 64KB」**顺手订正为 16 KB**（同文件既有漂移）
- [x] **T1.3** ✅ 同步 **6 处** `AddressDecoder` 测试消费者（`examples/riscv-mini/tests/test_riscv_tests_soc.cpp`）
- [x] **T1.4** ✅ 登记公开面：`.rddf/checks/public-api-surface.md` + `docs/CHANGELOG.md`

> **P1 实施备注（2026-11-24）**：
> - ⚠️ **T1.2 附带修了一处**既有 **ODR 隐患**：`address_decoder.h` 原把 `rv32i_mem_map.h`
>   **include 在 `namespace riscv { … }` 内部** ⇒ ⇒ 符号落进 `riscv::riscv::*` ⇒ ⇒ 与直接
>   include 它的 TU **是两个不同实体** ⇒ ⇒ **ODR 违规** + **顺序依赖**（实测：测试文件用
>   `in_mmio` ⇒ 编译报「did you mean `riscv::riscv::in_mmio`」）⇒ ⇒ include 已移到 namespace **之外**。
> - **T1.3 的判据**：新增 2 个用例 ⇒ **7 段基址 + 每段首字节只命中自己 + 窗口最后一字节
>   （`0x8010_6FFF`）+ 第一个外字节（`0x8010_7000`）+ 空洞（`0x800F_FFFF`）+ `in_mmio` 边界
>   + **`local_addr` 的 7 路 mux（`base+0x40 ⇒ 0x40`）**
>   ⇒ ⇒ 期望值**全部手推** ⇒ ⇒ 实测 `test_address_decoder` **132 断言 / 10 用例全绿**。
> - ⚠️ **`local_addr` 判据为什么必要**：片选与 `local_addr` 是**两条独立的 mux 链**
>   ⇒ ⇒ 只判片选时「某分支减错基址（如 `spi_local` 用 `i2c_base`）」**不可见**
>   ⇒ ⇒ 判别力自证（2026-11-24 实测）：注入该变异 ⇒ **1 用例 / 4 断言转红** ⇒ 还原复绿。

## P2 · 核侧 master（**核侧自带** · G-9.4）

- [x] **T2.1** ✅ 新建 `examples/riscv-mini/src/axil_mmio_master.h`（**26 端口** = 请求面 7 + AXI4-Lite 19）
  ⇒ ⇒ 接口 = `req` / `we` / `addr` / `wdata` / `strbe` → `ready`（**完成脉冲**）/ `rdata`（**保持寄存器**）
  ⇒ ⇒ **AW/W 相位分离** · **VALID 保持到握手** · `bready`/`rready` **恒拉高** · `awprot`/`arprot` **恒 0**
  ⇒ ⇒ 复位 = 核的 `ext_rst`（高有效）⇒ ⇒ **不新增核端口**
- [x] **T2.2** ✅ 头文件**自包含**（`scripts/verify_header_self_contained.sh`）· 禁 `&&`/`||`（ADR-036）⇒ 用 `&`/`|`
- [x] **T2.3** ✅ 🆕 ⚠️ **master 的 `ready` MUST 是「单拍完成脉冲」**（不是电平）——
  理由（S4a-0 的教训）：`ready` 进了 `data_ready_calc` ⇒ ⇒ 若它是电平 ⇒ ⇒
  观测面的 store 事件会在**整个事务期间**持续为高 ⇒ ⇒ **灌水**（事件次数 = 事件时长之和）
  ⇒ ⇒ 判据：**一次事务 ⇒ 恰好一拍 `ready`**（与事务时长无关）⇒ ⇒
  变异（把 `ready` 改成电平）⇒ ⇒ 该判据**必红**
  ⇒ ⇒ ⚠️ **禁**用 `!data_ready` 门控 `pipeline.data_write_en`（**组合环** ⇒ 仿真 `FATAL`）

> **P2 实施备注（2026-11-24）**：
> - **载体** = 新 target `test_axil_mmio_master`（`examples/riscv-mini/tests/test_axil_mmio_master.cpp`）
>   ⇒ 5 用例 / **58 断言**全绿 ⇒ `CTEST_TOTAL` **275 → 276** · `EXAMPLES_TEST_SOURCES` **20 → 21**
>   （锚点已同批同步）。
> - **逐拍期望值手推**（`eval_combinational` + `tick` 各一次 ⇒ 采样的是**本拍**组合值；
>   `get_port_value` 不自己重算 ⇒ 不显式 `eval_combinational` 会**读到上一拍**）。
> - **判别力自证**（实测转红后还原复绿）：
>   ① **M-a** `ready = in_wr_resp | in_rd_resp`（电平）⇒ **2 用例转红**（读用例 rvalid 延迟
>   4 拍 ⇒ 电平实现给 5 拍）—— 这是 T2.3 的靶子；
>   ② **M-b** `awaddr = io().paddr`（不锁存）⇒ **1 用例转红**（VALID 窗口内改 `paddr`
>   ⇒ ⇒ `awaddr` MUST 不变 ⇒ ⇒ 直接接输入会跟着变）。

## P3 · 核内 pre-cache 分流（G-9.3 · **订正后的判据**）

- [x] **T3.1** ✅ `examples/riscv-mini/src/rv32i_core_top.h` ⇒ 加 **19 根** `axil_*` 端口
  ⇒ ⇒ **11 OUT**（`awaddr`/`awprot`/`awvalid`/`wdata`/`wstrb`/`wvalid`/`bready`/`araddr`/`arprot`/`arvalid`/`rready`）
  · **8 IN**（`awready`/`wready`/`bresp`/`bvalid`/`arready`/`rdata`/`rresp`/`rvalid`）
- [x] **T3.2** ✅ 分流判据 = `in_mmio(data_addr)`（**派生自库层 MMIO 段** · **禁手写字面量**）
  ⇒ ⇒ **判据方向 MUST 是「MMIO 段」**（不是「D-TCM 段」）⇒ ⇒ 见 G-9 订正块
- [x] **T3.3** ✅ **两个 mux**：`data_ready`（`in_mmio ? master.ready : dcache.ready`）
  · `data_read_data`（`in_mmio ? master.rdata : dcache.rdata`）
  ⇒ ⇒ ⚠️ **`data_write_en` / `data_write_data` / `data_strbe` / `data_req` MUST NOT mux**
  ⇒ ⇒ ⇒ `data_write_en` 是**观测事件**（store 事件**与路径无关**）· `data_req` MUST 对两条路径都成立
  ⇒ ⇒ ⇒ master 的 `req` 输入 = `pipeline.data_req & in_mmio`（**在 master 内部门控**）
  ⇒ ⇒ ⚠️ **2026-11-24 订正（S4a-0 落地后的派生式）**：观测面的 store 事件 =
  `pipeline.io().data_write_en & data_ready_calc` ⇒ ⇒ **`data_ready_calc` MUST 是
  **同一个**路径 mux 后的表达式**（`in_mmio ? master.ready : dcache.ready` · 含
  `!data_req` 项）⇒ ⇒ **禁**另写一份口径 ⇒ ⇒ 否则 MMIO store 的事件会落在
  D-L1 的就绪拍上 ⇒ ⇒ **计数错** ⇒ ⇒ AC3 转红
  ⇒ ⇒ ⇒ ⚠️ **禁**把 `pipeline.io().data_write_en` 门控 `!mem_stall`（**组合环** ⇒
  仿真 `FATAL`，S4a-0 实测）⇒ ⇒ **功能信号 MUST 保持电平且不分流**
- [x] **T3.4** ✅ 核级 TB 的 **8 根** AXI-Lite 输入**显式 tie 0**
  （`examples/riscv-mini/tb_rv32i_soc.cpp` + `tb_rv32i_soc.sv`）⇒ ⇒ 核级 24 条**零变化**

## P4 · SoC 侧装配

- [x] **T4.1** ✅ `examples/riscv-soc/src/rv32i_soc_top.h` ⇒ +2 根根端口（`gpio_out` / `gpio_in` · 8 bit）
  ⇒ ⇒ 根自有 6 → **8** ⇒ 产品端口 8 → **10**
- [x] **T4.2** ✅ 装配 `AxiLiteMatrix2x2<32,32,Map>`（`ch_layer` · 给模块类型名）+ `AxiLiteGpio<8>`
  ⇒ ⇒ `Map::windows[0]` = `{kGpio.base, 4 KB}`（**派生**）
  ⇒ ⇒ `Map::windows[1]` = `{0xFFFFF000, 0x1000}`（**最高 4 KB** ⇒ 核永不发该地址 ⇒ s1 永不被选中）
  ⇒ ⇒ ⚠️ **MUST NOT** 用低位窗口（那会让地址 0 路由到未接的 s1 ⇒ 挂死）
- [x] **T4.3** ✅ 驱动清点（**零未驱动**）：matrix 的 **m1 面 11 根** + **s1 面 8 根** = **tie 0**
  · matrix 的 `rst_n` / GPIO 的 `rst_n` = `!ext_rst` · GPIO 的 `gpio_in` ← 根端口
- [x] **T4.4** ✅ `AxiLiteGpio.irq` **不接**（G-5：IRQ 实施在 S5）· `gpio_dir` **不落根**

## P5 · 判据（**先见红**）

- [x] **T5.1** ✅ SoC 级程序扩 **4 字**（12 字 · 编码见 `proposal.md` §程序）
  ⇒ ⇒ 编码**机器复验** + `static_assert`（同 `examples/riscv-mini/tb_rv32i_soc.cpp` 形态）
- [x] **T5.2** ✅ `examples/riscv-soc/tb_riscv_soc.cpp` ⇒ 判据 **7 → 9** 条
  ⇒ ⇒ AC1（`gpio_out == 0xA5`）· AC2（读完成）· AC3（计数 `(2,6)`）· AC8（DECERR 不挂死）
  ⇒ ⇒ `.sv` TB **逐条同值**（`interp_checks == min_checks == 9`）
- [x] **T5.3** ✅ `tb_riscv_soc.sv` 头部「程序与手推台账**同源于核级产品**」**MUST 改写**
  ⇒ ⇒ SoC 级程序**已分叉**（含 MMIO 访问）⇒ ⇒ 指向 `.rddf/checks/soc-rtl-expectations.md#riscv-soc`
- [x] **T5.4** ✅ `.rddf/checks/soc-rtl-expectations.md#riscv-soc` ⇒ §1 产物形态 · §2 程序（**独立登记**）
  · §3 判据（9 条）· §4 变异（重做）· §5 盲区
- [x] **T5.5** ⚠️（见下方备注：实现先行 ⇒ 以 M1–M6 的变异自证替代「实现前见红」） 先见红：AC1 / AC2 / AC3 / AC8 在实现前**必红** ⇒ ⇒ 记录

## P6 · 载体重算（**同批**）

- [x] **T6.1** ✅ 核端口契约表：`examples/riscv-mini/AGENTS.md` 34 → **53** 行
- [x] **T6.2** ✅ C6 快照：`scripts/verify_no_duplicate_rv32i_headers.sh` 32 → **51** 条
- [x] **T6.3** ✅ `scripts/verify_riscv_soc_core_ports.sh` 的硬编码 34 → **53**（**多处**）
- [x] **T6.4** ✅ `examples/riscv-soc/AGENTS.md` 产品面表（8 → 10 端口 · 10 → 12 module · 手推计数）
- [x] **T6.5** ✅ `scripts/verify_examples_rtl_sim_coverage.sh` 的登记表追加 S4a 行
- [x] **T6.6** ✅ **ADR-077 dated addendum**（V1 8 → 10 端口 · V2 10 → 12 module）⇒ ⇒ 决策**不变**

## P7 · 验证（先窄后宽）

- [ ] **T7.1** **全量重建**（⚠️ 禁只重建被点名的 target ⇒ 旧二进制 ⇒ 假绿）
- [ ] **T7.2** SoC 级 **9** 条全绿 · 核级 **24** 条**零变化**（`checks=24 fails=0`）
- [ ] **T7.3** `ctest` 全绿（**275**）· `run_all_ported_tests` PASS
- [ ] **T7.4** **全部** `scripts/verify_*.sh` 全绿（含 C6 · 核端口 · 未驱动 · 双形态）

## P8 · 判别力自证（变异 · MUST 实测转红）

- [x] **T8.1** ✅ **M1** 分流判据**取反**（写成「D-TCM 窗口 ⇒ D-L1」）⇒ ⇒ **AC4**（核级挂死）· **AC3**
- [x] **T8.2** ✅ **M2** MMIO **不走旁路**（接进 D-L1）⇒ ⇒ **AC2**（MMIO 读挂死）
- [x] **T8.3** ✅ **M3** 两条兄弟边**对调** ⇒ ⇒ **AC3**（`(2,6)` ↔ `(6,2)`）
- [x] **T8.4** ✅ **M4** 两条边**接同一源** ⇒ ⇒ **AC3**（两值相同）
- [x] **T8.5** ✅ **M5** `windows[1]` 改成低位 `{0, 4KB}` ⇒ ⇒ 组合变异（与 M1 同时）
- [x] **T8.6** ✅ **M6** GPIO 的 `gpio_in` 未驱动 ⇒ ⇒ **AC6**
- [x] **T8.7** ✅ 每个变异 MUST **先确认仍能编译** + **重建后**复跑（还原源码 ≠ 复原）

> **P3–P5 实施备注（2026-11-24）**：
> - **P3**：核顶层端口 **34 → 53** · 分流用「地址 ∈ MMIO 段」的**硬件比较器**（`riscv::in_mmio()` 是
>   **constexpr** ⇒ 不能作用于硬件值；边界常量仍取自库层表 ⇒ **单一真相不变**）。
> - 🔴 **P3 附带发现（必须记）**：`DCacheSync` 的 `aw_valid = we & req`（`d_cache_sync.h:287`）
>   ⇒ ⇒ **store-to-MMIO 若进 D-L1 会在 D-TCM 的 AXI 从口发起别名地址的写** ⇒ ⇒ **静默污染 D-TCM**
>   ⇒ ⇒ 故 `dcache.req` MUST 用 `& !in_mmio` 门控（不只是「省一次无谓 fill」）。
> - **P5**：`T5.5` 的「实现前见红」**未按字面执行**（P3/P4 先落地）⇒ 以 **M1–M6 变异自证**替代
>   （其中 M1 在 SoC 侧**只有 A7 转红** ⇒ ⇒ 证明「只判计数判不出写错段」⇒ A7 必需）。
> - 判据：C++ **9/9** · `.sv` **9/9** · 核级 **24/24** · `--check-products` PASS（12 module / 10 端口 / 0 未驱动）。

> **critic 轮次 1（2026-11-24）→ 1 BLOCKER + 1 SHOULD_FIX**：
> - 🔴 **BLOCKER（已闭合）**：读数据 mux 的选择线用**当拍** `addr_in_mmio` ⇒ 而 `data_read_data`
>   在 **M2** 消费（那拍已是后继指令的地址）⇒ ⇒ **静默错源**。
>   ⇒ **先红后绿**：新增 **A9**（读值回路）+ 不改实现 ⇒ `checks=10 fails=1`（只有 A9 红）；
>   改实现（`rd_path_mmio` 与 `m2_*` 同门控锁存）⇒ C++ `10/10` · `.sv` `10/10` · 核级仍 `24/24`。
> - **SHOULD_FIX（已登记 · 本批不修）**：master 的 `bresp` / `rresp` 被忽略 ⇒ DECERR / SLVERR
>   对软件不可见（A8 恰好依赖「错误响应当成功完成、数据取 0」这一**未写下来**的语义）
>   ⇒ ⇒ 已登记 `soc-rtl-expectations.md` §5 盲区；错误观测面 / 陷入口归 **S5**。
> - **plan 面连带**：AC7 的 `L = 9` → **10**，AC2 的「读值归组件级用例」**不成立**
>   ⇒ ⇒ 已在 proposal 追加 **D4 / D5** 订正块（不改决策 · planner 复核点已写明）。

## P9 · 收口

- [ ] **T9.1** `openspec validate --all` ⇒ ⇒ 绿
- [ ] **T9.2** `scripts/verify_openspec_delta.sh` ⇒ ⇒ 绿（**实际门**）
- [ ] **T9.3** 过 **critic**（`review_profile: build`）—— ⚠️ 触及**公开面** + **核端口面** ⇒ MUST 审
- [ ] **T9.4** 母 improvement 批次表 / 进度行同步（S4a 已闭 ⇒ 下一批 = S4b）
- [ ] **T9.5** 归档（`openspec archive`）
