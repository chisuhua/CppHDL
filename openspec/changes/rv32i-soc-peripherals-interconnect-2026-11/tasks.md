# Tasks: S4b —— N×M Lite 互联 + 5 个外设

> **Change**：`rv32i-soc-peripherals-interconnect-2026-11`
> **前置**：S4a 已归档 · IP 冻结批已闭
> **纪律**：TDD 5 步（先见红）· 期望值手推 · 每步跑验证 · 提交单独成段
> **决策门**：D-1 / D-3 **只阻塞依赖它们的阶段** —— D-1 阻塞 P2 起（地址表），
> D-3 阻塞 T3.2（SoC 物理引脚）；**P1（新 IP）不依赖两者，可立即开工**。

## P0 · 决策门（按阶段阻塞）

- [ ] **T0.1** D-1 = 追加 `kTimer` 段（`0x8010_7000`）**已确认**
  ⇒ 未确认 ⇒ **T2.1 起 STOP**（不得改 `rv32i_mem_map.h`）；**T1 不受影响**
- [ ] **T0.2** D-3 = 外设物理引脚**本批不上根端口** **已确认**
  ⇒ 未确认 ⇒ **T3.2 STOP**（装配前必须定）；T1 / T2 不受影响
- [ ] **T0.3** 确认 `AxiLiteMatrix2x2` 在 `examples/axi4` 仍被使用 ⇒ **不删**
- [ ] **T0.4** 记录基线：核级 24 判据 · SoC 10 判据 · `tb_riscv_soc` 的 `checks=10` ·
      根端口 10 · 层次化 module 12

## P1 · 新 IP（TDD · 先写失败用例 · **数据面用 chlib stream**）

- [ ] **T1.0** 接线纪律前置：确认 3 条（照抄 `examples/stream/stream_mux_top.h`）
      - 被组合子消费的 stream MUST 先 `as_master()`
      - `input_streams[i].ready` MUST 用 `<<=`（禁 `operator=`）
      - 消费端覆盖输出 ready MUST 直接 `<<=` 到 `result.output_stream.ready`
- [ ] **T1.1** 🔴 先写 `tests/test_axi_lite_crossbar.cpp` 的**失败**用例：
      - 路由完备：`N=2, M=3` 的 fixture map，每对 1 笔写 + 1 笔读
      - 锁存：AW 握手后改 `awaddr` ⇒ W/B 仍去原 slave
      - 在途门控：同 master 第二笔 AW 不被接受、且不丢
      - 仲裁：两 master 同时请求 ⇒ 轮转（连续 4 轮，顺序符合手推）
      - DECERR：未映射 + `ERR` 窗口 ⇒ `bresp/rresp = 0b11`、不挂死
      - W 跟随写路由：AW 与 W 相隔 1 拍
- [ ] **T1.2a** 边界适配器：19 根端口 ↔ 5 条 `ch_stream`（**唯一**手写 valid/ready 的地方）
- [ ] **T1.2b** AW / AR 平面：`stream_mux<N>`（grant 选主）→ `stream_demux<M+1>`（地址选从 ·
      **第 M 路 = ERR**）⇒ ERR 路 `ready=1`（**禁**用「select 越界」表达 ERR）
- [ ] **T1.2c** W 平面：`ERR` 路由**就地消费**（ready=1），其余按 `wr_owner[j]` 经
      `stream_mux<N>` 送对应 slave
- [ ] **T1.2d** B / R 平面：按 `wr_owner[j]` / `rd_owner[j]` 用 `stream_demux<N>` 反查 master；
      **本地 DECERR B/R 流**（`resp=0b11` · `rdata=0`）保持到 ready
- [ ] **T1.2e** 仲裁：`chlib::round_robin_selector` + `last_grant_oh`（握手后推进）+
      `grant_oh`（保持到握手）；索引用 `chlib::onehot_to_binary`
- [ ] **T1.3** 跑 T1.1 ⇒ 全绿；再跑 T1.4 的反向判据
- [ ] **T1.4** 🔴 **变异自证**（每条必须实测转红，且**先确认仍能编译**）：
      M1 target 改 `ERR` · M2 读写 owner 串线 · M3 去在途门控 · M4 仲裁固定优先 ·
      M5 只回 `bvalid` 不回 `rvalid`
      ⇒ 记录「变异 → 哪条用例红」
- [ ] **T1.5** 窗口非法配置的编译期判据：`size` 非 2 的幂 / `base` 未对齐 / `target` 越界
      ⇒ 用 `static_assert` 断言
- [ ] **T1.6** 复用审计（人工 + grep）：crossbar 内 **MUST NOT** 出现手写的
      `xxx_ready = ...valid...` 反压转发；数据面 MUST 经 `stream_mux` / `stream_demux`
      ⇒ 记录 grep 判据与例外（边界适配器除外）

## P2 · 库层地址表（D-1）

- [ ] **T2.1** `rv32i_mem_map.h` 追加 `kTimer` + `kMmioBytes` 派生（8 段）
      ⇒ `static_assert` 连续性 + 与 TCM 不重叠
- [ ] **T2.2** `tests/test_address_decoder.cpp` 补 `kTimer` 片选 / `local_addr` 判据
      （手推：`kTimer.base+0x40 ⇒ 0x40`；`0x8010_7FFF` 命中；`0x8010_8000` 出窗）
- [ ] **T2.3** 公开面登记：`public-api-surface.md` + `docs/CHANGELOG.md`（**1 行**）

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
4. **D-1 未确认 ⇒ T2.1 起 STOP**（T1 可做）· **D-3 未确认 ⇒ T3.2 STOP**（T1/T2 可做）。
