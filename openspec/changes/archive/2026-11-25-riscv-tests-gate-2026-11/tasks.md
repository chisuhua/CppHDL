# tasks · 官方 riscv-tests 套件门禁

> **Change**：`riscv-tests-gate-2026-11`
> **纪律**：TDD 5 步（**先见红**）· 每步小迭代 · 每步跑验证 · 提交**单独成段**
> ⇒ ⇒ **零端口面变更**（53 端口契约表 + C6 快照**不动**）
> ⇒ ⇒ **零公开 API 变更**
> ⇒ ⇒ **新增 CTest 条目 5 个**（`test_rv32i_stall_integrity` + 官方套件 4 组）
> ⇒ ⇒ `CTEST_TOTAL` **277 → 282** · `EXAMPLES_TEST_SOURCES` **22 → 24**
> （锚点**同批同步**）

---

## T0 · 缺陷定位（**先见红**）

- [x] **T0.1** 用上游 riscv-tests 编译出 40 个 `rv32ui-p-*` ELF（上游 `env/p` 布局）
      ⇒ ⇒ 跑在核上 ⇒ ⇒ **33 PASS / 7 FAIL**（`lb/lbu/lh/lhu/lw/sb/sh`）
- [x] **T0.2** 根因分离：**链接布局**（`.data` 落在 I-TCM）vs **反压期操作数污染**
      ⇒ ⇒ 用**新布局**重建 ⇒ ⇒ 7 个失败**减半** ⇒ ⇒ 剩余部分指向 **W2**
- [x] **T0.3** W2 的**逐拍轨迹**取证（`rv32ui-p-lb` c=40..49）⇒ ⇒ **三处数值互证**
      ⇒ ⇒ 根因闭合到 `fwd_a_d1` / `fwd_b_d1` 未随反压冻结

## T1 · 核心修复（`include/cpu/pipeline/rv32i_pipeline.h`）

- [x] **T1.1** 反压期间**保持** `fwd_a_d1` / `fwd_b_d1`
      （`select(mem_stall_fwd, fwd_a_d1, hazard.io().forward_a)`）
- [x] **T1.2** 注释写明：机制 · **禁用**「清零档位」· **禁用**「冻结 ID 读拍旁路」
      · **解冻沿无缝隙**
- [x] **T1.3** **零结构变更** ⇒ ⇒ ADR-069 D11 的「两级 + d1」**不动**

## T2 · 缺陷判据（`examples/riscv-mini/tests/test_rv32i_stall_integrity.cpp`）

- [x] **T2.1** DUT = `ch_device<Rv32iCoreTop, itcm, dtcm>`（核级）· 冻结由
      **D-L1 冷未命中自然产生** ⇒ ⇒ **不写实现时序进判据**
- [x] **T2.2** **A1**（M1 · 后继别名型）⇒ ⇒ 判别点 = 被冻结指令的寄存器值
- [x] **T2.3** **A2**（M2 · 决策回退型）⇒ ⇒ 判别点 = `x6` 读到 43（陈旧回退给 1）
      ⇒ ⇒ 同时杀掉「清零档位」这种错法
- [x] **T2.4** **A3**（控制面）⇒ ⇒ 形态 = `beq x5,x0`（`x5` 由**距离 2** 的生产者提供）
      ⇒ ⇒ 判别点 = 分支方向被翻转 ⇒ ⇒ `x8` 被写（`0x22`）
      ⇒ ⇒ ⚠️ 前三版（`beq x0,x0` / `beq x14,x0` / `beq x14,x14`）**恒绿** ⇒ ⇒
      **已作废并登记原因**（design §5）
- [x] **T2.5** **判别力自证**：① 只回退 **A 口**（`fwd_a_d1`）⇒ A1 / A3 红；
      ② 只回退 **B 口**（`fwd_b_d1`）⇒ **A2 红**（`0 == 42`）；
      ③ 两处都回退 ⇒ **三个全红**（实测）⇒ ⇒ 还原后复绿
      ⇒ ⇒ ⚠️ **A/B 两口 MUST 分别有判别力**：A1 的冻结指令只消费 `rs1`
      ⇒ ⇒ A2 的判别操作数 MUST 放在 **`rs2`**（`add x6, x0, x5`）

## T3 · R1：官方镜像重建 + 入库

- [x] **T3.1** `env/riscv_test.h` —— 只替换 **4 个** harness 宏 ⇒ ⇒ **测试体一字未改**
      ⇒ ⇒ 依据 = 上游 issue #368
- [x] **T3.2** `env/link.ld` —— **双 TCM 布局**（`.text` @ `0x8000_0000` ·
      `.tohost` @ `0x8001_0000` · `.data` / `.bss` 随后）
- [x] **T3.3** `env/build.sh` —— 从上游重建 40 份 + 与入库镜像 `diff -r` **逐字节比对**
      ⇒ ⇒ **重建产物与入库镜像一致**（实测）
- [x] **T3.4** `images/rv32ui-p-*/{itcm.mem,dtcm.mem}` —— **40 份**入库
- [x] **T3.5** `PROVENANCE.md` + `LICENSE.riscv-tests` ⇒ ⇒ 来源 / 改造 / **排除项** /
      格式 / 复现方法 / 许可

## T4 · R2：核级门禁

- [x] **T4.1** `tests/test_rv32i_official_suite.cpp` —— DUT = 核 IP 顶层
      ⇒ ⇒ 判定口径 = **上游 env 的协议**（`tohost == 1` ⇒ PASS）
- [x] **T4.2** 观察面 = **既有**根端口的 store 提交事件 ⇒ ⇒ **零新增端口**
- [x] **T4.3** **4 个独立二进制**（`-DRV32I_OFFICIAL_GROUP=1..4`）
      ⇒ ⇒ 单组实测 **95 / 97 / 106 / 122 s**（< 300 s 上限）
      ⇒ ⇒ **护栏 C3 按「ctest 条目名 = 二进制名」运行** ⇒ ⇒ MUST 独立二进制
- [x] **T4.4** **40 / 40 全过**（实测）

## T5 · 收口

- [x] **T5.1** 锚点同步（`CTEST_TOTAL` 277 → 282 · `EXAMPLES_TEST_SOURCES` 22 → 24）
- [x] **T5.2** 全量回归：`ctest`（282）+ 51 护栏 + `run_all_ported_tests.sh`
- [x] **T5.3** 文档同步：`examples/riscv-mini/AGENTS.md`（STRUCTURE / RUN ⑥ / WHERE TO LOOK ·
      **「riscv-mini 完成」的定义** = 官方套件全过）· `RUNNING-ELF.md` §8.6（R1/R2 落地 + 资产表）·
      `include/cpu/AGENTS.md`（load 字节道段：仓外二进制依赖**已闭合**）·
      根 `AGENTS.md`（**仅**计数锚点，见 T5.1）
- [x] **T5.4** spec delta：`rv32i-data-forwarding`（MODIFIED）+ 新 capability
      `rv32i-official-test-suite`（ADDED）
- [x] **T5.5** critic 审稿（`build` 档）⇒ ⇒ BLOCKER 全闭
- [x] **T5.6** `openspec archive` ⇒ ⇒ 主 spec 合并 ⇒ ⇒ `## Purpose` **手工**写入
