# tasks · S4a（核的 MMIO 通道 + SoC 侧 Lite 互联 + GPIO 回环）

> **Change**：`rv32i-soc-mmio-channel-2026-11`
> **前置**：`rv32i-commit-event-pulse-2026-11`（**S4a-0**）· **MUST 先落地并归档**
> **纪律**：TDD 5 步（**先见红**）· 每步小迭代 · 每步跑验证 · 提交**单独成段**
> **零行为变化**（对非 MMIO 路径）⇒ ⇒ 核级 24 条判据**零变化**

## P0 · 前置确认

- [ ] **T0.1** 确认 **S4a-0 已归档**（`openspec/changes/archive/**/rv32i-commit-event-pulse-2026-11/`）
  ⇒ ⇒ 未归档 ⇒ **STOP**（AC3 的计数口径依赖它）
- [ ] **T0.2** 确认库层地址表已有 **7 个 MMIO 段** + `kMmioBase` / `kMmioBytes`
  ⇒ ⇒ 缺失 ⇒ 先做 T1.1

## P1 · 库层（G-6）

- [ ] **T1.1** `include/cpu/riscv/rv32i_mem_map.h` ⇒ 新增 7 个 MMIO 段 + `kMmioBase` / `kMmioBytes`
  ⇒ ⇒ 每段 4 KB · 自 `0x8010_0000` 顺延 · **禁手写 0x7000**（派生）
  ⇒ ⇒ 加 `static_assert` 连续性 + 与 TCM 段不重叠
- [ ] **T1.2** `include/cpu/riscv/address_decoder.h` ⇒ 3 段硬编码改派生 + 补 4 片选
  ⇒ ⇒ **总片选 9**（2 TCM + 7 MMIO）+ `error_cs` ⇒ **10** 个输出端口
  ⇒ ⇒ 文件头表格的「I-TCM 64KB / D-TCM 64KB」**顺手订正为 16 KB**（同文件既有漂移）
- [ ] **T1.3** 同步 **6 处** `AddressDecoder` 测试消费者（`examples/riscv-mini/tests/test_riscv_tests_soc.cpp`）
- [ ] **T1.4** 登记公开面：`.rddf/checks/public-api-surface.md` + `docs/CHANGELOG.md`

## P2 · 核侧 master（**核侧自带** · G-9.4）

- [ ] **T2.1** 新建 `examples/riscv-mini/src/axil_mmio_master.h`
  ⇒ ⇒ 接口 = `req` / `we` / `addr` / `wdata` / `strbe` → `ready`（**完成脉冲**）/ `rdata`（**保持寄存器**）
  ⇒ ⇒ **AW/W 相位分离** · **VALID 保持到握手** · `bready`/`rready` **恒拉高** · `awprot`/`arprot` **恒 0**
  ⇒ ⇒ 复位 = 核的 `ext_rst`（高有效）⇒ ⇒ **不新增核端口**
- [ ] **T2.2** 头文件**自包含**（`scripts/verify_header_self_contained.sh`）· 禁 `&&`/`||`（ADR-036）⇒ 用 `&`/`|`

## P3 · 核内 pre-cache 分流（G-9.3 · **订正后的判据**）

- [ ] **T3.1** `examples/riscv-mini/src/rv32i_core_top.h` ⇒ 加 **19 根** `axil_*` 端口
  ⇒ ⇒ **11 OUT**（`awaddr`/`awprot`/`awvalid`/`wdata`/`wstrb`/`wvalid`/`bready`/`araddr`/`arprot`/`arvalid`/`rready`）
  · **8 IN**（`awready`/`wready`/`bresp`/`bvalid`/`arready`/`rdata`/`rresp`/`rvalid`）
- [ ] **T3.2** 分流判据 = `in_mmio(data_addr)`（**派生自库层 MMIO 段** · **禁手写字面量**）
  ⇒ ⇒ **判据方向 MUST 是「MMIO 段」**（不是「D-TCM 段」）⇒ ⇒ 见 G-9 订正块
- [ ] **T3.3** **两个 mux**：`data_ready`（`in_mmio ? master.ready : dcache.ready`）
  · `data_read_data`（`in_mmio ? master.rdata : dcache.rdata`）
  ⇒ ⇒ ⚠️ **`data_write_en` / `data_write_data` / `data_strbe` / `data_req` MUST NOT mux**
  ⇒ ⇒ ⇒ `data_write_en` 是**观测事件**（store 事件**与路径无关**）· `data_req` MUST 对两条路径都成立
  ⇒ ⇒ ⇒ master 的 `req` 输入 = `pipeline.data_req & in_mmio`（**在 master 内部门控**）
- [ ] **T3.4** 核级 TB 的 **8 根** AXI-Lite 输入**显式 tie 0**
  （`examples/riscv-mini/tb_rv32i_soc.cpp` + `tb_rv32i_soc.sv`）⇒ ⇒ 核级 24 条**零变化**

## P4 · SoC 侧装配

- [ ] **T4.1** `examples/riscv-soc/src/rv32i_soc_top.h` ⇒ +2 根根端口（`gpio_out` / `gpio_in` · 8 bit）
  ⇒ ⇒ 根自有 6 → **8** ⇒ 产品端口 8 → **10**
- [ ] **T4.2** 装配 `AxiLiteMatrix2x2<32,32,Map>`（`ch_layer` · 给模块类型名）+ `AxiLiteGpio<8>`
  ⇒ ⇒ `Map::windows[0]` = `{kGpio.base, 4 KB}`（**派生**）
  ⇒ ⇒ `Map::windows[1]` = `{0xFFFFF000, 0x1000}`（**最高 4 KB** ⇒ 核永不发该地址 ⇒ s1 永不被选中）
  ⇒ ⇒ ⚠️ **MUST NOT** 用低位窗口（那会让地址 0 路由到未接的 s1 ⇒ 挂死）
- [ ] **T4.3** 驱动清点（**零未驱动**）：matrix 的 **m1 面 11 根** + **s1 面 8 根** = **tie 0**
  · matrix 的 `rst_n` / GPIO 的 `rst_n` = `!ext_rst` · GPIO 的 `gpio_in` ← 根端口
- [ ] **T4.4** `AxiLiteGpio.irq` **不接**（G-5：IRQ 实施在 S5）· `gpio_dir` **不落根**

## P5 · 判据（**先见红**）

- [ ] **T5.1** SoC 级程序扩 **4 字**（12 字 · 编码见 `proposal.md` §程序）
  ⇒ ⇒ 编码**机器复验** + `static_assert`（同 `examples/riscv-mini/tb_rv32i_soc.cpp` 形态）
- [ ] **T5.2** `examples/riscv-soc/tb_riscv_soc.cpp` ⇒ 判据 **7 → 9** 条
  ⇒ ⇒ AC1（`gpio_out == 0xA5`）· AC2（读完成）· AC3（计数 `(2,6)`）· AC8（DECERR 不挂死）
  ⇒ ⇒ `.sv` TB **逐条同值**（`interp_checks == min_checks == 9`）
- [ ] **T5.3** `tb_riscv_soc.sv` 头部「程序与手推台账**同源于核级产品**」**MUST 改写**
  ⇒ ⇒ SoC 级程序**已分叉**（含 MMIO 访问）⇒ ⇒ 指向 `.rddf/checks/soc-rtl-expectations.md#riscv-soc`
- [ ] **T5.4** `.rddf/checks/soc-rtl-expectations.md#riscv-soc` ⇒ §1 产物形态 · §2 程序（**独立登记**）
  · §3 判据（9 条）· §4 变异（重做）· §5 盲区
- [ ] **T5.5** 先见红：AC1 / AC2 / AC3 / AC8 在实现前**必红** ⇒ ⇒ 记录

## P6 · 载体重算（**同批**）

- [ ] **T6.1** 核端口契约表：`examples/riscv-mini/AGENTS.md` 34 → **53** 行
- [ ] **T6.2** C6 快照：`scripts/verify_no_duplicate_rv32i_headers.sh` 32 → **51** 条
- [ ] **T6.3** `scripts/verify_riscv_soc_core_ports.sh` 的硬编码 34 → **53**（**多处**）
- [ ] **T6.4** `examples/riscv-soc/AGENTS.md` 产品面表（8 → 10 端口 · 10 → 12 module · 手推计数）
- [ ] **T6.5** `scripts/verify_examples_rtl_sim_coverage.sh` 的登记表追加 S4a 行
- [ ] **T6.6** **ADR-077 dated addendum**（V1 8 → 10 端口 · V2 10 → 12 module）⇒ ⇒ 决策**不变**

## P7 · 验证（先窄后宽）

- [ ] **T7.1** **全量重建**（⚠️ 禁只重建被点名的 target ⇒ 旧二进制 ⇒ 假绿）
- [ ] **T7.2** SoC 级 **9** 条全绿 · 核级 **24** 条**零变化**（`checks=24 fails=0`）
- [ ] **T7.3** `ctest` 全绿（**275**）· `run_all_ported_tests` PASS
- [ ] **T7.4** **全部** `scripts/verify_*.sh` 全绿（含 C6 · 核端口 · 未驱动 · 双形态）

## P8 · 判别力自证（变异 · MUST 实测转红）

- [ ] **T8.1** **M1** 分流判据**取反**（写成「D-TCM 窗口 ⇒ D-L1」）⇒ ⇒ **AC4**（核级挂死）· **AC3**
- [ ] **T8.2** **M2** MMIO **不走旁路**（接进 D-L1）⇒ ⇒ **AC2**（MMIO 读挂死）
- [ ] **T8.3** **M3** 两条兄弟边**对调** ⇒ ⇒ **AC3**（`(2,6)` ↔ `(6,2)`）
- [ ] **T8.4** **M4** 两条边**接同一源** ⇒ ⇒ **AC3**（两值相同）
- [ ] **T8.5** **M5** `windows[1]` 改成低位 `{0, 4KB}` ⇒ ⇒ 组合变异（与 M1 同时）
- [ ] **T8.6** **M6** GPIO 的 `gpio_in` 未驱动 ⇒ ⇒ **AC6**
- [ ] **T8.7** 每个变异 MUST **先确认仍能编译** + **重建后**复跑（还原源码 ≠ 复原）

## P9 · 收口

- [ ] **T9.1** `openspec validate --all` ⇒ ⇒ 绿
- [ ] **T9.2** `scripts/verify_openspec_delta.sh` ⇒ ⇒ 绿（**实际门**）
- [ ] **T9.3** 过 **critic**（`review_profile: build`）—— ⚠️ 触及**公开面** + **核端口面** ⇒ MUST 审
- [ ] **T9.4** 母 improvement 批次表 / 进度行同步（S4a 已闭 ⇒ 下一批 = S4b）
- [ ] **T9.5** 归档（`openspec archive`）
