# Tasks: RV32I D-L1 的 AXI4 full 主口 + 真 line（S3）

> 三个阶段各自可独立验证；每阶段结束都要编译 + 跑相关测试。

## T0 基线

- [ ] **T0.1** 记录核侧三份产物 sha256（**S3 会改产物** —— 与 S2 不同，这次端口/逻辑都变）
- [ ] **T0.2** 记录 `tb_rv32i_soc` 的 `RESULT: checks=24 fails=0`
- [ ] **T0.3** 记录既有 5 个 cache 测试的通过状态（`test_cache_sync` / `test_cache_simple`
  / `test_cache_complete` / `test_cache_pipeline_integration` / `test_phase4_integration`）

## T1 `DCacheSync` 重写（AXI full + 真 line）

- [ ] **T1.1** AXI 侧端口改 full 5 通道（AR/R/AW/W/B）· 移除 `axi_addr`/`axi_wdata`/
  `axi_we`/`axi_valid`/`axi_ready`/`axi_rdata`/`axi_resp_valid`
- [ ] **T1.2** 读 miss：burst fill 状态机（`ar_len=3` / `ar_size=2` / `ar_burst=INCR`）
  + `beat_idx` + `r_last` + `fill_target_data` 锁存
- [ ] **T1.3** 写：write-through 单 beat（`aw_len=0`）+ `w_strb`（byte lane）
- [ ] **T1.4** **修「假 line」**：读按 `addr[3:2]` 选 lane · fill 写满 4 lane · 写按 lane 更新
- [ ] **T1.5** `ready` 语义：hit ⇒ 1 拍；miss ⇒ fill 完成那拍；写 ⇒ 握手完成
- [ ] **T1.6** 组件级测试：AC1（4 lane 写读回）· AC2（offset 选 lane）· AC3（hit 1 拍）·
  AC4（miss 反压）· AC5（burst 参数手推）· AC8（双后端）

## T2 `TcmAxiSlave`（新）

- [ ] **T2.1** `include/axi4/tcm_axi_slave.h`：AR/AW burst → 逐 beat 访问 `ch_mem`
- [ ] **T2.2** 字节地址 → 字地址（`>> 2`）· `w_strb` → `strbe`
- [ ] **T2.3** 组件级测试：AC6（`ARLEN=3` 读 4 字 / `AWLEN=3` 写 4 字 · 手推地址与值）
- [ ] **T2.4** 双后端覆盖

## T3 核接线（TCM 旁路）

- [ ] **T3.1** `examples/riscv-mini/src/rv32i_core_top.h`：`pipeline ↔ DCacheSync ↔ TcmAxiSlave ↔ DataTCM`
- [ ] **T3.2** `pipeline.data_ready ← cache.ready`（S2 通路生效）
- [ ] **T3.3** `prog_*` 仍直写 I-TCM（**不动**）
- [ ] **T3.4** 核级：AC7（24 判据 · dual parity 逐拍等价 · `prog_*` 一致）
- [ ] **T3.5** 产物重新生成 + lint（`verify_hierarchical_rtl` / `verify_examples_rtl_sim`）

## T4 收口

- [ ] **T4.1** 既有 5 个 cache 测试的激励改 4 beat（T1 的契约变更所致）
- [ ] **T4.2** 登记面同步：`CTEST_TOTAL` / `tests/test_*.cpp` / `include/cpu/AGENTS.md` 行数 /
  `port-naming-registry`（新端口）· `public-api-surface`（`DCacheSync` 的 AXI 口**改名/移除**）
- [ ] **T4.3** `docs/CHANGELOG.md` + 验收报告 `.rddf/reviews/rv32i-dcache-burst-2026-11-verify.md`
- [ ] **T4.4** 母 improvement 回填 S3
- [ ] **T4.5** `d_cache_async.h` 的同样缺陷**登记为已知边界**（不修 · async 未接入生产）

## 已登记的范围外项

- 母 improvement 的 **S3 含「TCM 归属 / 成层决策」** ⇒ 本 change **不做**
  （撞已签批 `rtl-hierarchical-output` 的成层集合 7 / 交付粒度 8 module ⇒ 需单独裁）
- `d_cache_write_buffer.h` 的「无真缓冲」债务 ⇒ 独立立项
- 外部总线暴露（G-8）⇒ 未裁
