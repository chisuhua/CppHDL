# Tasks: S4b —— N×M Lite 互联 + 5 个外设

> **Change**：`rv32i-soc-peripherals-interconnect-2026-11`
> **前置**：S4a 已归档 · IP 冻结批已闭
> **纪律**：TDD 5 步（先见红）· 期望值手推 · 每步跑验证 · 提交单独成段
> **决策门**：D-1 / D-3 **只阻塞依赖它们的阶段** —— D-1 阻塞 P2 起（地址表），
> D-3 阻塞 T3.2（SoC 物理引脚）；**P1（新 IP）不依赖两者，可立即开工**。

## P0 · 决策门（按阶段阻塞）

- [x] **T0.1** D-1 = 追加 `kTimer` 段（`0x8010_7000`）**已确认**（2026-11-27 · 用户「继续 P2」）
  ⇒ ⇒ **T2 已解禁**（`rv32i_mem_map.h` 已改）
- [x] **T0.2** D-3 = 外设物理引脚**本批不上根端口** **已确认**（2026-11-27 · 用户裁决）
  ⇒ ⇒ **T3.2 已解禁并完成**
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

## P3 · SoC 装配（✅ 已闭）

- [x] **T3.1** `SocCrossbarMap`（**8 窗口**，逐段派生自库层表）+ **8 条 `static_assert`**
      （逐段 base/size == 库层表 ⇒ 手写基址字面量 ⇒ **编译失败**）+ 7 条 `target` 意图断言
      （CLINT/DMA → `kErr`；GPIO/UART/TIMER/SPI/I2C/PWM → s0..s5）
- [x] **T3.2** `AxiLiteMatrix2x2` → `AxiLiteCrossbar<SocCrossbarMap>` + **6 个外设**接线
      （19 根 × 6）· m1（DMA 预留）tie 0 · 物理输入 tie 常量（D-3 = **本批不引**）
      ⚠️ **方向按 AXI4-Lite 角色**：主→从（awaddr/awprot/awvalid/wdata/wstrb/wvalid/
      **bready**/araddr/arprot/arvalid/**rready**）与从→主（**awready/wready/bresp/bvalid/
      arready/rdata/rresp/rvalid**）**相反** ⇒ ⇒ 本批踩过一次（接反 ⇒ 互连看不到从设备的
      响应 ⇒ MMIO 事务挂死 ⇒ A3/A4/A7/A9 全红）⇒ ⇒ 方向已写进代码注释
- [x] **T3.3** `--check-products` **PRODUCTS-CHECK: PASS**（module 集 **17** · 根端口 **10** ·
      未驱动输入 **0** · lint 干净 · 双形态等价 ok）⇒ ⇒ `tb_riscv_soc` **checks=10 fails=0**
      ⇒ ⇒ `verify_examples_rtl_sim.sh` 全绿（⑧' 层次化 module 数 17 · ④″ 产物级判据）

## P4 · 判据（TDD）（✅ 已闭）

- [x] **T4.1** `tb_riscv_soc` 加 **5 条失败回环判据**（UART/TIMER/SPI/I2C/PWM）
      ⇒ ⚠️ **实现形态与设计稿不同**（如实记录）：设计稿写的是「固件写序列 + 读回 +
      **存 D-TCM** ⇒ TB 断言」；实际用**独立 SoC 实例 + GPIO 回环**
      （写外设寄存器 → 读回 → 写 GPIO `DATA_OUT` ⇒ 判 `gpio_out`）。
      理由：SoC 级**观测不到 D-TCM**（后代 `ch_out` 不是顶层端口 · ADR-072 D1）⇒ ⇒
      设计稿那条路径在 SoC 级**不成立**（规划期缺陷）；`gpio_out` 是唯一能让任意读值
      回到根端口的现成通路。每段**独立实例** ⇒ 5 个读值互不覆盖
      （同一实例只能留最后一个 ⇒ 分段按拍观测会依赖精确拍数 ⇒ 脆）。
- [x] **T4.2** `checks = 10 + 5 + 1 = 16`（**等式**，不是下界）：C++ 侧 16 · `.sv` 侧 16 ·
      tsv 登记 `interp_checks=16` / `min_checks=16`
      ⇒ ⚠️ **+1 = A15（同批追加）**：实测发现 A10–A14 对「窗口错位到同偏移也可写的
      外设」**结构性失明**（把 PWM 窗口指向 GPIO ⇒ 写读都落到 `DATA_OUT` ⇒ 15 条全绿）
      ⇒ ⇒ 追加「PWM `PERIOD` 复位值 == 0xFF」的**地址特征**判据闭合该缺口
- [x] **T4.3** A8（CLINT 段 DECERR）语义不变（仍 `writeback_count == 3`）；
      **A11 = D-1 的判别点**：TIMER `LOAD(0x00)` 写 0x6A 再读回 —— 若 `kTimer` 段仍落
      `kErr` ⇒ 写 DECERR + 读回 0 ≠ 0x6A ⇒ **必红**
- [x] **T4.4** 期望台账 §3 增 A10–A15（地址 / 写值 / **外设复位值** / 来源）+
      §5 更新「单点错位残留」盲区（**A15 闭合 PWM 面**；UART/TIMER/SPI/I2C 面仍由
      P1 逐从口单测 + `SocCrossbarMap` 的断言承担）
- [x] **T4.5** tsv：`interp_checks` 10→16 · `min_checks` 10→16 · `scenarios` 6→12 ·
      `spec_checks` 5→11（A10–A15 全部 spec-derived）

## P5 · 回归与收口

- [x] **T5.1** 全量回归（2026-11-27 实测）：
      · 全量重建 `BUILD=0`（**0 error**）· `ctest` **285/285 passed**（2535 s，含 perf）
      · `verify_examples_rtl_sim.sh` **PASS**（18 项 · 等式 `checks=16`）
      · `verify_examples_rtl_sim_coverage.sh` **PASS**（29 == 17 ∪ 12）
      · `verify_hierarchical_rtl.sh` **PASS**（R1–R8 + R8′ + R9）
      · `rv32i_rtl_sim_env.sh` **flat 24/24 · hier 24/24 · fails=0**
- [x] **T5.2** 核级 24 判据 / 双后端 parity **零变化**：`rv32i_rtl_sim_env` 两形态各
      **24/24**；ctest 全绿（含 `verilator_e2e` 82 条）；parity 锚点未动
      （`DUAL_BACKEND_PARITY_TOTAL=68` / `PAIRS=307`）
- [x] **T5.3** 计数锚点：`CTEST_TOTAL` **285 不变**（P4 未新增 ctest target ——
      `tb_riscv_soc` 不注册为 test）· `EXAMPLES_TEST_SOURCES` **26 不变**
      （P4 只在已有 `tb_riscv_soc.cpp` 内加判据，未新增测试源文件）
- [x] **T5.4** 母 improvement 回填：`rv32i-soc-platform-2026-11` 的 `进度:` 追加
      S4b P4 完成记录（A10–A15 · checks 10→16 · 变异 3 条实测转红）
- [x] **T5.5** `verifier` 独立验收（AC1–AC9 逐条工具证据 + 遗留清扫）
      ⇒ ✅ **PASS — 9/9 AC 满足**（2026-11-27 · verifier 角色 · 第 1 轮 · 无失败项）
      ⇒ 报告 = `.rddf/reviews/rv32i-soc-peripherals-interconnect-2026-11-verify.md`
      ⇒ 两条非阻断建议：① AC9 的「ERR → s0」形态未在 verifier 侧实测（写集不含
      `examples/**`；「或」支路已由 `static_assert` 满足）② UART/TIMER/SPI/I2C 的
      「单点窗口错位」SoC 层无判别力（由 P1 逐从口单测承担 · 已三处登记）

## P6 · 归档

- [ ] **T6.1** `openspec validate rv32i-soc-peripherals-interconnect-2026-11 --strict`
- [ ] **T6.2** `scripts/verify_openspec_delta.sh`（本 change 有 ADDED ×1）
- [ ] **T6.3** `openspec archive … --yes`

## 硬约束

1. **T1 必须先于 T3**（IP 语义先独立判据，再进 SoC）。
2. **T2 必须先于 T3.1**（窗口派生要读得到 `kTimer`）。
3. **T4.1 必须先于 T4.2**（先见红）。
4. ✅ **D-1 / D-3 均已确认**（2026-11-27）⇒ T2 / T3 已闭 ⇒ ⇒ 剩余 = **P4（外设回环判据）**。
