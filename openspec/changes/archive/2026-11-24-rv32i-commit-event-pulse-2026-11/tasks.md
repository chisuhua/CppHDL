# tasks · 提交事件端口脉冲化

> **Change**：`rv32i-commit-event-pulse-2026-11`
> **纪律**：TDD 5 步（**先见红**）· 每步小迭代 · 每步跑验证 · 提交**单独成段**
> **零行为变化**（观测面语义订正）⇒ ⇒ **核端口面 34 不变** ⇒ ⇒ C6 快照**不动**
> ⇒ ⇒ **零新增 test target** ⇒ ⇒ `CTEST_TOTAL` / `DUAL_BACKEND_PARITY_*` /
> `VERILATOR_E2E_TOTAL` **锚点零变更**

---

## ⚠️ 订正块（2026-11-24 · 实施期实测 · **本块优先于下文任何相冲突的表述**）

> **被推翻的初稿任务**（保留供审稿复核）：
> - ~~**T0.1** 新建 `examples/riscv-mini/tests/test_commit_event_pulse.cpp`（新 target）~~
>   ⇒ ⇒ **订正**：判据载体改为**既有** `test_rv32i_soc_dual_parity.cpp`（**追加**一个
>   `DUAL_BACKEND_TEST_PAIR`）⇒ ⇒ **零锚点变更** ⇒ ⇒ 不必同步 `CTEST_TOTAL`。
> - ~~**T0.1** 的固件 = `addi x1,x0,42` · `lw x2,0(x0)` · `addi x3,x0,1` · `jal x0,0`~~
>   ⇒ ⇒ **订正**：该固件在旧实现下 `wb_high = 3` ⇒ **零灌水** ⇒ **恒绿**
>   ⇒ ⇒ 固件 MUST 改为「**3 条 `rd≠0` 的写回类指令 + 1 条冷 `lw`**」（design §1.6）。
> - ~~**T0.2** 断言「停顿期间逐拍为 0」~~ ⇒ ⇒ **订正**：`mem_stall` **不是顶层端口**
>   ⇒ ⇒ 核外**不可观测** ⇒ ⇒ 改为可观测形式（事件次数 / 每 rd 次数 / 相邻重复）。
> - ~~**T1.1** `rv32i_pipeline.h:693` ⇒ `io().data_write_en <<= exmem_is_store & !mem_stall;`~~
>   ⇒ ⇒ **订正**：**成组合环** ⇒ ⇒ 实测 `FATAL: Cycle detected in IR graph during
>   topological sort` ⇒ ⇒ **不可实施** ⇒ ⇒ store 事件改在**核顶层总线侧**派生。
> - ~~**T3.2**（M2 变异）~~ ⇒ ⇒ **撤除**（恒绿判据必须撤掉）⇒ ⇒ 归 S4a。

---

## P0 · 判据先行（先见红）

- [x] **T0.1** 判据载体 = **既有** `examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp`
  ⇒ ⇒ **追加** `DUAL_BACKEND_TEST_PAIR(Rv32iCoreTop, "Rv32iSoc - CommitEventPulse", …)`
  ⇒ ⇒ **零新增 target** ⇒ ⇒ **锚点零变更**
- [x] **T0.2** 固件 = `addi x2,x0,7` · `addi x3,x0,9` · `addi x4,x0,11`
  · `lw x5,0x100(x0)`（**冷行** ⇒ D-L1 miss ⇒ 实测 **8 拍**反压）· `jal x0,0`
  ⇒ ⇒ 编码**手推**（禁抄仿真输出）⇒ ⇒ 编码器 × 手算常量**交叉校验**（防自证）
- [x] **T0.3** 断言 **AC1**：**写回事件次数 == 4**（x2 / x3 / x4 / x5；`jal x0,0` 的
  `rd=x0` ⇒ 不写回）⇒ ⇒ ⚠️ **旧实现 = 12**（实测）⇒ ⇒ **必红**
- [x] **T0.4** 断言 **AC2**：**每个 rd 恰好一次**（x2/x3/x4/x5 各 1）
  ⇒ ⇒ ⚠️ **旧实现下 x3 = 9**（实测）⇒ ⇒ **必红**
- [x] **T0.5** 断言 **AC3**：**相邻重复 == 0**（相邻两拍都高 **且 `(rd,data)` 相同**）
  ⇒ ⇒ ⚠️ **旧实现 = 8**（实测）⇒ ⇒ **必红**
- [x] **T0.6** 断言 **AC4**：`x5` 的写回数据 == 0（冷读 D-TCM ⇒ 零初始化）
  ⇒ ⇒ 证明「脉冲化没有改变『哪条指令被写回、写回什么』」
- [x] **T0.7** ⚪ **store 事件次数**的断言 ⇒ ⇒ **不写**（今日**恒绿** ⇒ 无效判据）
  ⇒ ⇒ **如实登记**（proposal **AC7** / design **R4**）⇒ ⇒ 判别力归 **S4a**

## P1 · 实现（**2 处**）

- [x] **T1.1** `include/cpu/pipeline/rv32i_pipeline.h`（`build_debug_outputs`）
  ⇒ ⇒ `auto mem_stall = !io().data_ready;`
  ⇒ ⇒ `io().dbg_wb_write_en <<= wb_stage.io().write_en & !mem_stall;`（附口径注释）
  ⇒ ⇒ ⚠️ **本端口零内部消费者** ⇒ ⇒ 门控**不成环** ✓
- [x] **T1.2** `examples/riscv-mini/src/rv32i_core_top.h`
  ⇒ ⇒ `auto data_ready_calc = (!pipeline.io().data_req) | dcache.io().ready;`
  ⇒ ⇒ `pipeline.io().data_ready <<= data_ready_calc;`
  ⇒ ⇒ `io().obs_data_write_en <<= pipeline.io().data_write_en & data_ready_calc;`
  （附口径注释：**功能信号零改动** ⇒ ⇒ 组合环不存在 ✓）
- [x] **T1.3** ⚠️ **禁改**：`dbg_exmem_is_store` / `dbg_id_is_store`（**级属性**端口 ⇒ 电平正确）
  ⇒ ⇒ `reg_file.write` 的使能口径**不动**（幂等 ⇒ 零可观测影响 ⇒ 见 design §4 R1）
  ⇒ ⇒ `io().data_write_en`（**功能**信号）**不动** ⇒ ⇒ 附「为何不可门控」的注释（组合环）

## P2 · 验证（先窄后宽）

- [x] **T2.1** **全量重建**（`cmake --build build -j$(nproc)`）⇒ ⇒ ⚠️ **禁**只重建被点名的 target
  （旧二进制 ⇒ 假绿）
- [x] **T2.2** 新用例 **全绿**（双后端）⇒ ⇒ AC1 / AC2 / AC3 / AC4 过
- [x] **T2.3** 核级 `tb_rv32i_soc` ⇒ ⇒ `RESULT: checks=24 fails=0`（**AC5** · 零变化）
- [x] **T2.4** `test_rv32i_soc_dual_parity` ⇒ ⇒ **写回序列 golden 逐项不变**
  ⇒ ⇒ 「折连续相同」workaround **保留**（脉冲化后**幂等**）
- [x] **T2.5** `ctest` 全绿（**275** · 锚点零变更）· `run_all_ported_tests` PASS
- [x] **T2.6** 全部 `scripts/verify_*.sh` 全绿（**含 C6 快照** —— 端口面零变更 ⇒ C6 不动）

## P3 · 判别力自证（变异 · MUST 实测转红）

- [x] **T3.1** **M1** = 去掉 `dbg_wb_write_en` 的 `& !mem_stall`
  ⇒ ⇒ ✅ **实测转红**：`wb_events = 12` · `x3` 写回 9 次 · 相邻重复 8
  ⇒ ⇒ **重建后**复跑（还原源码 ≠ 复原）
- [x] **T3.2** ~~**M2** = 去掉 `data_write_en` 的 `& !mem_stall`~~ ⇒ ⇒ **撤除**
  （① 该写法**成组合环** ⇒ 仿真 `FATAL`；② 今日 store 事件**恒绿** ⇒ 无效判据）
  ⇒ ⇒ **如实登记**（design R4）⇒ ⇒ 判别力归 **S4a**
- [x] **T3.3** **M3** = 两根事件端口**对调**（核顶层）
  ⇒ ⇒ ✅ **实测转红**：`wb_events = 0`
- [x] **T3.4** 每个变异 MUST **先确认仍能编译**（编译失败 ⇒ 跑的是旧二进制 ⇒ 假绿）
- [x] **T3.5** 全部还原 ⇒ ⇒ **全量重建** ⇒ ⇒ 复跑 ⇒ ⇒ 全绿 ⇒ ⇒ 记录进期望台账

## P4 · 登记面（文档同步 · 与代码同批）

- [x] **T4.1** `.rddf/checks/public-api-surface.md` ⇒ 「**行为变更（非符号移除）**」节登记两根
  端口的**语义订正**（端口名 / 方向 / 位宽**未变** ⇒ 不属「新增 / 移除公开符号」⇒ 登记为语义订正）
  ⇒ ⇒ 含**迁移指引**（「当电平用」的判据 MUST 改判「该拍发生了事件」）
- [x] **T4.2** `docs/CHANGELOG.md` ⇒ 一节（4 小节：改动 · **三处订正** · 判据载体 · 判别力自证）
- [x] **T4.3** `include/cpu/AGENTS.md` ⇒ 新增「流水线观测面：**事件端口** vs **级属性端口**」一节
  （含**组合环**的实测踩坑记录）
- [x] **T4.4** `examples/riscv-mini/AGENTS.md` ⇒ 核端口契约表加**语义脚注**
  （`top_core_obs_data_write_en` / `top_core_dbg_wb_write_en` 现为**事件脉冲** ⇒ **表本身不动**）
- [x] **T4.5** ⚠️ **改落点**（原写 `.rddf/checks/examples-rtl-expectations.md` ⇒ ⇒ **不适用**）：
  该台账是 **TB 口径**（一节 = 一个 `tb_*` 产物）⇒ ⇒ 本 change 的判据在**单元测试**侧
  ⇒ ⇒ **判别力证据**落在 ① 本 change 的 `design.md §5`（实测值）② `docs/CHANGELOG.md` ④
  测试文件内的注释（`test_rv32i_soc_dual_parity.cpp` 的用例头）
- [x] **T4.6** 母 improvement `rv32i-soc-platform-2026-11` ⇒ 批次表 S4a-0 行的判据**订正**
  + 进度行推进 + 文首 **dated 订正记录**
- [x] **T4.7** **增补**（实施期发现）：`.rddf/checks/rv32i-soc-product-baseline.md`
  ⇒ ⇒ 三份产品级产物指纹**更新**（`top.v` 全变 = 预期）+ **§2 订正块**
  （`rtl-emit-manifest-v2` 有 `port_table[]`(34/8) **与** `ports[]`(284/294) 两份清单
  ⇒ ⇒ **manifest 整体 sha 变 ≠ 端口面变** ⇒ ⇒ 判端口面 MUST 看 `port_table[]` +
  C6 / R8′ / `verify_riscv_soc_core_ports`）
- [x] **T4.8** **增补**：根 `AGENTS.md` 的 `DUAL_BACKEND_PARITY_PAIRS` 锚点
  **290 → 291**（新增 1 个双后端 pair ⇒ ⇒ 护栏 `verify_dual_backend_parity.sh` 要求**显式**更新）
  + `tests/hier_corpus_registry.md` 的**豁免表**加 `Rv32iCoreTop`（含具体原因 +
  逐拍 flat≡hier **未覆盖** 的如实登记）

## P5 · 收口

- [x] **T5.1** `openspec validate --all` ⇒ ⇒ 绿（**54/54**）
- [x] **T5.2** `scripts/verify_openspec_delta.sh` ⇒ ⇒ 绿（**实际门** · 4 条全绿）
- [x] **T5.3** 过 **critic**（`review_profile: build, plan`）—— ⚠️ 本 change 触及
  **公开观测面语义** + **已签批 capability 的 MODIFIED** ⇒ ⇒ MUST 审
  ⇒ ⇒ **轮次 1：APPROVE**（`mergeable: yes` · `scope_exceeded: false` · **0 BLOCKER**）
  ⇒ ⇒ **1 SHOULD_FIX + 1 NIT 已逐条处置**（跨产品消费者 `soc_trace_counter.h` 的
  dated 脚注 + 「恰好一次」论证措辞精确化 · **实现不变**）
  ⇒ ⇒ 审稿记录 = `.rddf/reviews/rv32i-commit-event-pulse-2026-11-arch-review.md`
  ⇒ ⇒ ⇒ **无 BLOCKER ⇒ 无需复审**（预算 2 轮，用 1 轮）
- [x] **T5.4** ✅ **已归档**（`openspec archive rv32i-commit-event-pulse-2026-11 --yes` ⇒ ⇒ `openspec/changes/archive/2026-11-24-rv32i-commit-event-pulse-2026-11/` ⇒ ⇒ spec delta 已并入 `openspec/specs/rv32i-commit-path/spec.md`（1 modified））⇒ ⇒ **S4a 的硬前置解除**
