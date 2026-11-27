# Tasks: S4b —— N×M Lite 互联 + 5 个外设

> **Change**：`rv32i-soc-peripherals-interconnect-2026-11`
> **前置**：S4a 已归档 · IP 冻结批已闭
> **纪律**：TDD 5 步（先见红）· 期望值手推 · 每步跑验证 · 提交单独成段
> **决策门**：D-1 / D-3 **只阻塞依赖它们的阶段** —— D-1 阻塞 P2 起（地址表），
> D-3 阻塞 T3.2（SoC 物理引脚）；**P1（新 IP）不依赖两者，可立即开工**。

## P0 · 决策门（按阶段阻塞）

- [x] **T0.1** D-1 = 追加 `kTimer` 段（`0x8010_7000`）**已确认**（2026-11-27 · 用户「继续 P2」）
  ⇒ ⇒ **T2 已解禁**（`rv32i_mem_map.h` 已改）
- [ ] **T0.2** D-3 = 外设物理引脚**本批不上根端口** **已确认**
  ⇒ 未确认 ⇒ **T3.2 STOP**（装配前必须定）；T1 / T2 不受影响
- [ ] **T0.3** 确认 `AxiLiteMatrix2x2` 在 `examples/axi4` 仍被使用 ⇒ **不删**
- [ ] **T0.4** 记录基线：核级 24 判据 · SoC 10 判据 · `tb_riscv_soc` 的 `checks=10` ·
      根端口 10 · 层次化 module 12

## P1 · 新 IP（TDD · 先写失败用例 · **数据面用 chlib stream**）

- [x] **T1.0** 接线纪律前置：确认 3 条（照抄 `examples/stream/stream_mux_top.h`）
      - 被组合子消费的 stream MUST 先 `as_master()`
      - `input_streams[i].ready` MUST 用 `<<=`（禁 `operator=`）
      - 消费端覆盖输出 ready MUST 直接 `<<=` 到 `result.output_stream.ready`
- [x] **T1.1** 🔴 先写 `tests/test_axi_lite_crossbar.cpp` 的**失败**用例（首版 `M=3`；
      落地时按端口面固定 2×6 改为 6 个 slave，见 T1.2a）：
      - 路由完备：每个 (master, slave) 对 1 笔写 + 1 笔读
      - 锁存：AW / AR 握手后改地址 ⇒ W/B / R 仍去原 slave
      - 在途门控：同 master 第二笔 AW 不被接受、且不丢（**同目标 + 另一个空闲目标**）
      - 仲裁：两 master 同时请求 ⇒ **AW 与 AR 各自**轮转（连续 4 轮，顺序符合手推）
      - DECERR：未映射 + `ERR` 窗口 ⇒ `bresp/rresp = 0b11`、不挂死
      - W 跟随写路由：AW 与 W 相隔 1 拍
      - 同 slave 串行：第二个 `awready=0` 且 slave 看不到第二次 `awvalid`
- [x] **T1.2a** 边界适配器：19 根端口 ↔ 5 条 `ch_stream`（**唯一**手写 valid/ready 的地方）
      ⇒ 端口面固定 2×6、全部**具名**（`verify_port_naming.sh` 0 新增债）
- [x] **T1.2b** AW / AR 平面：`stream_mux<2>`（grant 选主）→ `stream_demux<7>`（地址选从 ·
      **第 6 路 = ERR**）⇒ ERR 路 `ready=1`（**禁**用「select 越界」表达 ERR）
- [x] **T1.2c** W 平面：每 master `stream_demux<7>`（按锁存路由）→ 每 slave `stream_mux<2>`
      （按 owner）⇒ ERR 路由**就地消费**（ready=1）。
      ⚠️ 两级是必需的：ready 节点只能有一个驱动者（`<<=` last-wins）
- [x] **T1.2d** B / R 平面：每 slave `stream_demux<2>`（按 owner 反查）→ 每 master
      `stream_mux<7>`（按锁存路由，末档 = ERR）；**本地 DECERR B/R 流**保持到 ready
- [x] **T1.2e** 仲裁：`chlib::round_robin_selector` + `last_grant`（握手后推进）+
      `chlib::onehot_to_binary`；owner 由 `active ∧ route` **组合派生**（不存寄存器）
- [x] **T1.3** 跑 T1.1 ⇒ 全绿（20 TEST_CASE / 508 断言 · interpreter + verilator）
- [x] **T1.4** 🔴 **变异自证**（每条实测转红；先确认仍能编译）：
      M1 ERR 窗口→s0（ERR 用例红）· M2 B 平面用读 owner（fanout/arb 红）·
      M3 去 AW 在途门控（在途用例红）· M4a/M4b 去 AW/AR 指针更新（仲裁红）·
      M5 rvalid 恒 0（全部读用例红）· M6 窗口 size 非 2 的幂（编译失败）·
      M7 去 slave 侧忙门控（串行用例红）
- [x] **T1.5** 窗口非法配置的编译期判据：`size` 非 2 的幂 / `base` 未对齐 / `target` 越界
      ⇒ `static_assert(windows_are_valid())`（M6 实测触发）
- [x] **T1.6** 复用审计（人工 + grep）：crossbar 内无手写的
      `xxx_ready = ...valid...` 反压转发；数据面经 `stream_mux` / `stream_demux`
      ⇒ 手写 valid/ready 仅两处：边界适配、在途/忙门控

## P2 · 库层地址表（D-1 · ✅ 已闭）

- [x] **T2.1** `rv32i_mem_map.h` 追加 `kTimer`（`kMmioBase + 7 × kMmioSegmentBytes`）
      + `kMmioBytes` 派生（7 → **8** 段）+ 2 条 `static_assert`（连续性 / 覆盖全部 8 段）
      ⇒ ⇒ **既有 7 段基址零移动**
- [x] **T2.2** `examples/riscv-mini/tests/test_riscv_tests_soc.cpp`（ctest target
      `test_address_decoder`）补 TIMER 判据：首字节只命中自己 · 窗口末字节 `0x8010_7FFF`
      命中 TIMER（`local = 0xFFF`）· `local_addr` 减**自己**的基址（`base+0x40 ⇒ 0x40`）·
      第一个外字节 `0x8010_8000` 报错 ⇒ ⇒ 期望值**手推**
      ⇒ ⇒ **变异自证 N1**（`in_timer` 恒 0 ⇒ 3 处红）/ **N2**（`timer_local` 减错基址 ⇒ 2 处红）
- [x] **T2.3** 公开面登记：`public-api-surface.md`（行为变更 +1 行 · 新增符号 +1 行）
      + `docs/CHANGELOG.md`（1 条）+ `include/cpu/AGENTS.md` 的自述计数（7601 → **7624**）
      ⇒ ⇒ `AddressDecoder` 的 `timer_cs` **具名** ⇒ 端口命名棘轮**零新增债**

## P3 · SoC 装配

- [ ] **T3.1** `SocCrossbarMap`（8 窗口，派生）+ 8 条 `static_assert`（逐段 == 库层表）
- [ ] **T3.2** `AxiLiteMatrix2x2` → `AxiLiteCrossbar<2, 6, SocCrossbarMap>`
      + 6 个外设接线 + m1 tie 0 + 物理输入 tie 常量
- [ ] **T3.3** `--check-products` 绿：module 数 = 12 + 6 = **18** · 根端口 **10** ·
      未驱动输入 **0** · lint 白名单（物理输出无消费者）

## P4 · 判据（TDD）

- [ ] **T4.1** 🔴 先在 `tb_riscv_soc` 里加 **5 条失败回环判据**（UART/TIMER/SPI/I2C/PWM）
      ⇒ 固件写序列 + 读回 + 存 D-TCM ⇒ TB 断言（期望值手推，见 design §4.1）
- [ ] **T4.2** 让固件通过：`examples/riscv-soc/tb_riscv_soc.cpp` 的程序段按 A10–A14 扩展
      ⇒ `checks = 10 + 5 = 15`（**等式**，不是下界）
- [ ] **T4.3** A8（CLINT 段 DECERR）**语义不变** + 新增「`kTimer` 段**不** DECERR」判据（D-1 的判别点）
- [ ] **T4.4** 期望台账：`.rddf/checks/soc-rtl-expectations.md#riscv-soc` 增 5 条
      （地址 / 写值 / 读值 / 来源）
- [ ] **T4.5** tsv：`tb_riscv_soc` 行的 `min_checks` / `scenarios` / `spec_checks` 重算

## P5 · 回归与收口

- [ ] **T5.1** 全量回归：`ctest` 全绿（**先全量重建**）· `verify_examples_rtl_sim.sh` ·
      `verify_examples_rtl_sim_coverage.sh` · `verify_hierarchical_rtl.sh` · `rv32i_rtl_sim_env`
- [ ] **T5.2** 核级 24 判据 / 双后端 parity **零变化**（对照 T0.4 基线）
- [ ] **T5.3** 计数锚点：`CTEST_TOTAL`（若新增 target）/ `EXAMPLES_TEST_SOURCES`（不变则记录原因）
- [ ] **T5.4** 母 improvement 回填：`rv32i-soc-platform-2026-11` 的 `进度:` 追加 S4b 完成记录
- [ ] **T5.5** `verifier` 独立验收（AC1–AC9 逐条工具证据 + 遗留清扫）

## P6 · 归档

- [ ] **T6.1** `openspec validate rv32i-soc-peripherals-interconnect-2026-11 --strict`
- [ ] **T6.2** `scripts/verify_openspec_delta.sh`（本 change 有 ADDED ×1）
- [ ] **T6.3** `openspec archive … --yes`

## 硬约束

1. **T1 必须先于 T3**（IP 语义先独立判据，再进 SoC）。
2. **T2 必须先于 T3.1**（窗口派生要读得到 `kTimer`）。
3. **T4.1 必须先于 T4.2**（先见红）。
4. ✅ **D-1 已确认**（2026-11-27）⇒ T2 已闭 · ⚠️ **D-3 未确认 ⇒ T3.2 STOP**（T1/T2 已闭）。
