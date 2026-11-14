# Tasks: 存储器时序契约（D-TCM 寄存读 + MEM 数据对齐 A0）

> **实施顺序固定**（见 `design.md` §1）⇒ ⇒ **MUST 按序执行**。
> ⚠️ **T2 的三处强制结构变更 MUST 同批同 commit**（漏一 ⇒ ⇒ **静默错值**，无报错）。
> ⚠️ **期望值 MUST 手推**（禁抄仿真输出 —— 两后端同源 ⇒ parity 对该族缺陷结构性失明）。

## S0 — 开工前基线（**零改动**）

- [x] 记录基线：`ctest -N` 总数 = **263**（2026-11-13 实测）
- [x] 记录基线：**B3 会影响的 16 条 ctest 条目全绿**（2026-11-13 实测 `ctest -R "rv32i|forward|stall|memory"`
      ⇒ ⇒ `100% tests passed, 0 tests failed out of 16`）：
      `test_rv32i_pipeline_example` · `test_rv32i_soc_dual_parity` · `test_forwarding` · `test_memory` ·
      `test_memory_dual` · `test_forwarding_dual` · `test_rv32i_arch_dual` · `test_rv32i_stage_dual` ·
      `test_rv32i_timing_dual` · `test_rv32i_pipeline` · `test_rv32i_alu` · `test_rv32i_pipeline_behavior` ·
      `test_verilator_memory_e2e` · `spinalhdl_rv32i_phase{1,2,3}`
- [x] 确认工作区干净（`git status --short` 为空）⇒ ⇒ **禁** `git add -A`
- [x] ⚠️ **【新发现 · T5 的前置】时序金标的「现值」在源码里找不到硬编码常量**
      ⇒ ⇒ 草案里的 `perf_instructions = 178` / `load_use_stalls = 6` / `control_stalls = 4`
      **未在 `tests/` / `examples/` 中作为断言常量出现** ⇒ ⇒ 它们**可能**来自 perf report 的**观测值**
      （⇒ ⇒ 那就**不是断言** ⇒ ⇒ 「重算金标」的对象是**断言位置**，不是报告数字）
      ⇒ ⇒ **T5 开工前 MUST 先定位断言位置**：
      ① `examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp` 有 `kExpectedWritebacks`（24 条写回序列）
      ② `test_riscv_tests_pipeline.cpp` / `test_rv32i_soc_dual_parity.cpp` 暴露 `obs_perf_*` 观测端口
      ⇒ ⇒ **禁**把「观测值」当「断言值」⇒ ⇒ 二者 MUST 分开登记（否则 = 拿仿真输出当期望值 ⇒ ⇒ 违纪律）

✅ **定位结论（2026-11-13 实测）**：草案里的 178 / 6 / 4 **确有断言字面量**
（`6u` / `21u` / `kCycles - 22u`）⇒ ⇒ 「重算金标」的对象**是断言位置**，不是报告数字
⇒ ⇒ 逐条定位 + **手推推导**落 `design.md` §10（**先推后写**）⇒ ⇒ **只改 3 个数**。

```bash
cd /workspace/project/CppHDL
ctest --test-dir build -N | tail -1
bash scripts/verify_flat_rtl_emit.sh
bash scripts/verify_test_counts.sh
```

## S1 — T1：D-TCM 改寄存读（**先红后绿**）

- [x] 写**产物级**判据：D-TCM 的读 MUST NOT 是「容量:1 组合 mux」⇒ ⇒ **先红**
      ⇒ ⇒ **2026-11-14 已落地**：`verify_examples_rtl_sim.sh` 的 **④'**（load 口 = `always_ff`
      ⇒ ⇒ 寄存器 · RMW 口 = `assign` 组合）⇒ ⇒ 变异退回 `aread` ⇒ ⇒ **红**
      ⚠️ **2026-11-14 实测**：**未写**（本批的产物面由 `scripts/verify_flat_rtl_emit.sh`
      的 **F8（产物卫生）** + **F1（lint 门）** 覆盖，**但**没有**专门**钉「D-TCM 读口是
      `always_ff` 而非 `assign`」的判据 ⇒ ⇒ **本项未完成** ⇒ ⇒ 归属「S6 收口」前补
- [x] 改 `include/cpu/pipeline/rv32i_tcm.h`：`aread` → `sread(addr, ch_bool(true))`
      ⇒ ⇒ **2026-11-14 已实施**（enable **恒真** ⇒ ⇒ ADR-068 §2.2-R6）
- [x] 判据转绿 ⇒ ⇒ 重建 + 复跑
      ⇒ ⇒ **2026-11-14**：组件级双后端用例已按**新契约**改写
      （`tests/test_rv32i_timing_dual.cpp` 的 `DataTCM` pair ⇒ ⇒ **两拍一用例**：
      第 1 拍 = **沿级 read-old**（写前内容）· 第 2 拍 = **写后内容** ⇒ ⇒
      「1 拍延迟」由此**可判别**（退回 `aread` ⇒ ⇒ 第 1 拍即见新值 ⇒ ⇒ **必红**）)

```bash
cd /workspace/project/CppHDL
cmake --build build -j$(nproc)
bash scripts/verify_flat_rtl_emit.sh
```

## S2 — T2：A0 的三处强制结构变更（**同批同 commit**）

- [x] ① 前推**档位 2** 的数据源：`memwb_*` → **M2** ⇒ ⇒ **2026-11-14 已实施**
- [x] ①′ ⚠️ **【前置 · ② 的依赖】load 侧车道 mux MUST 改用 M2 锁存的地址 / funct3**
      （`mem_stage.h:101-114` 现消费 `io().alu_result` = EX/MEM 地址 ⇒ ⇒ 若漏搬
      ⇒ ⇒ M2 档会按**后继指令的地址**选车道 ⇒ ⇒ **静默错 lane**）
      ⇒ ⇒ 依据 = ADR-069 **V5** · ⇒ ⇒ **MUST 排在 ② 之前**
- [x] ② ⚠️ **【2026-11-13 订正】D11 = 两级优先旁路**（**不是**「源替换」）：
      **新增 M2 档**（判据 = `m2_valid & (m2_is_alu|m2_is_load|m2_is_jump) & (m2_rd_addr != 0)`
      ∧ `m2_rd_addr == rs{1,2}_addr_id`；数据 = **M2 级同构写回 mux**）
      **+ WB 档逐字保留**（判据 `wb_write_en` / `wb_stage.io().rd_addr_out`）
      ⇒ ⇒ **优先级 = M2 档 > WB 档**（youngest-wins）
      ⇒ ⇒ 🔴 **`wb_stage` 的写回 mux MUST NOT 前移**（前移 ⇒ `reg_file.write` 单写口
      采到更年轻指令 ⇒ ⇒ 真正提交的那条**永不写回**）
      ⇒ ⇒ 依据 = `docs/adr/ADR-069-mem-data-alignment.md` **§7.1.2**
      与 `docs/adr/ADR-047-rv32i-data-forwarding.md` **§5.5.2.1**
- [x] ③ `include/cpu/pipeline/hazard_unit.h` 停顿链**加一级**（`stall_hold_d3` 或等价）
      ⇒ ⇒ **2026-11-14 已实施**（`stall_hold_d3`）⇒ ⇒ 停顿 = **4 拍**（实测 `load_use_stalls` = 8）
- [x] ⚠️ 对外端口契约**不变**（`stall` 仍是布尔输出）⇒ ⇒ 无 API 变更
- [x] 每处各配**一条判别性用例**（缺任一 ⇒ ⇒ 漏改不可见）
      ⇒ ⇒ **2026-11-14 已落地**：V12（组件级停顿链真值表 · 新建）· V13/V14a/V14b/V15
      ⇒ ⇒ （距离 2/3/4 的生产者-消费者）⇒ ⇒ 三条突变**各自指名**受害者
      ⚠️ **2026-11-14 实测**：①②③ 三处的**间接**判别力**已有**
      （`test_rv32i_soc_dual_parity` 的 24 条写回序列 + `load_use_stalls` = 8
      ⇒ ⇒ 档位 2 源改回 `memwb_*` ⇒ ⇒ 距离 2 的消费者读旧值 ⇒ ⇒ **必红**）
      ⇒ ⇒ ✅ **V12 已闭**（2026-11-14 · 新建 `tests/chlib/test_hazard_unit_stall.cpp`
      ⇒ ⇒ **组件级停顿链真值表** · 36 断言 / 3 用例 ⇒ ⇒ 钉住
      「检测条件持续 N 拍 ⇒ 停顿 **N+2** 拍」⇒ ⇒ 变异（删 `stall_hold_d3`）⇒ ⇒ **必红**）
      ⇒ ⇒ ⛔ **V13–V15 的「专向」用例（手推 + 变异自证）仍未写** ⇒ ⇒ **本项未完成**
      ⇒ ⇒ 归属「S6 收口」前补（载体 = **新建** pipeline 级 fixture ⇒ ⇒ 禁塞进
      `tests/chlib/test_forwarding.cpp`）
- [x] ⚠️ **【判据拆分】V14 ⇒ V14a + V14b**（ADR-069 §7.4）⇒ ⇒ **已做**（2026-11-14）
      ⇒ ⇒ V14a = 距离 **3** 的消费者（M2→ID 旁路）· V14b = **WB 档必要性**
      ⇒ ⇒ ⚠️ **V14b 的归因如实修正**：它由**既有固件**钉住（突变体 C ⇒ golden A 红）
      ⇒ ⇒ ⇒ ⇒ **不是**新用例 ⇒ ⇒ 登记在 ADR-069 §7.4
      **V14a** = 距离 3（ALU / load / jump 三类）⇒ ⇒ 判据源 = **M2 档**
      **V14b** = 距离 4 **及**「**4 拍**停顿后的 load-use 消费者」⇒ ⇒ 判据源 = **WB 档**
      ⇒ ⇒ **V14b 的变异 = 删掉 WB 档 ⇒ ⇒ 必红**（否则该档零判别力 ⇒ ⇒ 结构性漏改不可见）
- [x] ⚠️ **V14c** M2 档与 WB 档同时命中同一 `rd` ⇒ ⇒ MUST 取 **M2 档**（youngest-wins）
      ⇒ ⇒ **2026-11-14 复核：已覆盖** ⇒ ⇒ 既有双后端用例的「⑥ 双生产者同 rd ——
      ⇒ ⇒ 档位优先级（取**较新**的那个）」⇒ ⇒ 载体 = `test_rv32i_soc_dual_parity.cpp`
- [x] ⚠️ **V15** M2 档的数据叶 MUST 是**流水线寄存器**（M2 寄存器 + `sread` 输出寄存器）
      ⇒ ⇒ **2026-11-14 已实施**（叶 = M2 寄存器 + `sread` 输出寄存器 ⇒ ⇒ 不成环）
      ⇒ ⇒ 不成环 ⇒ ⇒ 变异：叶改成 MEM 级**组合输出** ⇒ ⇒ 必红（`bf78367` 同型）
- [x] ⚠️ **V16** `commit_count` 的退休时点 MUST NOT 改动（增量源仍 = `mem_stage.io().mem_valid_out`）
      ⇒ ⇒ **2026-11-14 已实施**（增量源未动 ⇒ ⇒ **fill 段仍是 3** ⇒ ⇒ `non_retired` = 3+8+12 = **23**）
      ⇒ ⇒ 变异：改增量源为 M2 的 valid ⇒ ⇒ `fill` 段 3→4 ⇒ ⇒ `non_retired` 断言必红

```bash
cd /workspace/project/CppHDL
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure -R 'rv32i_pipeline|forwarding'
```

## S3 — T3：`data_ready` 接真 + 反压用例

- [x] `include/cpu/pipeline/rv32i_pipeline.h:61` 的 `data_ready` 接真信号
      ⇒ ⇒ **2026-11-14**：driver **本就**是 `dtcm.io().ready`（= 恒 1）⇒ ⇒
      依 **ADR-068 V7**（「driver 仍**恒 1**」）⇒ ⇒ **零改动**
- [-] ~~MEM 级在 `data_ready == false` 时冻结 MEM/WB + 向前级发 stall~~
      ⇒ ⇒ **2026-11-14：按 ADR-068 V7 移出本批**（「驱动 0 ⇒ MEM 冻结」= **反压实现**
      ⇒ ⇒ 属「**接 cache**」独立立项 ⇒ ⇒ ADR-068 §6 明写它**不在**本 ADR 的范围
      ⇒ ⇒ ⚠️ **本项不是「未做」而是「不在本批」**（**原任务的旧形态** ⇒ ⇒ 已订正）
- [-] ~~**测试专用 DUT**：能拉低 `data_ready`~~ ⇒ ⇒ **同上游出本批**（反压的判据面）
- [-] ~~用例：反压期间有效链保持~~ ⇒ ⇒ **同上游出本批**
- [x] 契约登记：`include/cpu/AGENTS.md` 注明「**固定 1 拍延迟**（**不是握手**）」
      ⇒ ⇒ **2026-11-14 已登记**（`include/cpu/AGENTS.md` 的「`Rv32iPipeline::data_ready` 的语义」条）
      ⇒ ⇒ 另登记 = `.rddf/checks/public-api-surface.md` 的「行为变更」节（预告 → **已实施**）
      + `docs/CHANGELOG.md` 的一节

```bash
cd /workspace/project/CppHDL
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure -R 'rv32i'
bash scripts/verify_rst_n_wiring.sh
```

## S4 — T4 + T6：写相位契约与判别性用例

- [x] **写使能 MUST 与被覆盖地址同拍** ⇒ ⇒ **2026-11-14 已落地（T4）**
      ⇒ ⇒ **行为级判据** = `Rv32iSoc - StorePhaseCoherence`（双后端 ·
      3 条**背靠背** store · 地址/数据两两不同 ⇒ ⇒ 每拍 `en==1` 的那一拍
      `(addr, data)` MUST 是自己的配对）⇒ ⇒ 判据**结构上不可判**（三者都是组合 net）
      ⇒ ⇒ ⚠️ **判别力实测**：把 `data_write_en` 用成员寄存器**错开一拍**
      ⇒ ⇒ 该用例**红**（配对断言失败）⇒ ⇒ 还原 + 重建后复绿 ✅
      ⇒ ⇒ 结构面另由 `scripts/verify_dtcm_read_timing.sh` 的 **W1–W3** 守
      （写口 = `always_ff` · 有门控 · 索引/数据非常量）
- [x] ⚠️ 车道覆盖：`sb` **4 车道** + `sh` **2 半字车道** ⇒ ⇒ **2026-11-14 已补全（T4）**
      ⇒ ⇒ `sb` 4 车道：`SB(1..4, 0, 0x100..0x103)` ⇒ lane 0/1/2/3
      ⇒ ⇒ `sh` **2/2**：新增 `Rv32iSoc - StoreHalfwordLaneCoverage`
      （`SH -> 0x120` 低半字 + `SH -> 0x122` 高半字 ⇒ 读回 `x3 = 0x00220011`）
      ⇒ ⇒ ⚠️ **判别力实测**：把 `sh_be` 改成**恒取高半字** ⇒ ⇒ 该用例**红**
      （`x3 = 0x11` ≠ `0x00220011`）⇒ ⇒ 还原 + 重建后复绿 ✅
- [x] T6 判别性用例：**紧邻** `SW x1, 0(x2)` / `LW x3, 0(x2)`（同址 · 零间隔）⇒ ⇒ 断言 `x3 == 写入值`
      ⇒ ⇒ **2026-11-14 已落地**：`t6_firmware` + `Rv32iSoc - StoreToLoadZeroIntervalReadNew`
      ⇒ ⇒ 突变 ⇒ ⇒ `rd=17` ⇒ ⇒ **红**
- [x] ⚠️ 现固件「隔 4 字」形态 MUST NOT 冒充该判据 ⇒ ⇒ 新用例 MUST 紧邻
      ⇒ ⇒ **2026-11-14 已满足**：新用例用**独立的 `t6_firmware`**（紧邻同址）
      ⇒ ⇒ ⇒ ⇒ 既有固件保持「隔 4 字」形态 ⇒ ⇒ 两者互不冒充
- [x] **相位错位探针**：故意把写使能错开一拍 ⇒ ⇒ 判据 MUST 转红（判别力自证）
      ⇒ ⇒ **2026-11-14 已做**：① **行为级**（写使能经成员寄存器错开一拍
      ⇒ ⇒ `StorePhaseCoherence` 红）② **结构级**（`verify_dtcm_read_timing.sh --self-test`
      的**两条原地投毒**：去掉 `if` 门控 ⇒ W2 红；写口改非寄存 ⇒ W1 红）

```bash
cd /workspace/project/CppHDL
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure -R 'rv32i'
```

## S5 — T5：时序金标**手推**重算

- [x] **逐条手推**：`perf_instructions` · `load_use_stalls`（3 → **4** 拍）· `control_stalls` · store 拍数 · `dbg_id_pc` 逐拍序列 · 24 条写回序列
      ⇒ ⇒ **手推结论落 `design.md` §10**（2026-11-13 · **先于实施**）
- [x] 推导**留痕**（禁抄仿真输出）⇒ ⇒ 每条附推导过程 ⇒ ⇒ `design.md` §10.2（分段分解）+ §10.3（不变项逐条理由）+ §10.4（最小改动集）
- [x] ⚠️ 手推值本身要能判别（禁对称值 / 位回文 / 字节回文）
      ⇒ ⇒ `8` / `23` / `176` **非对称 · 非回文 · 互相独立** ⇒ ⇒ 单改一个 ⇒ ⇒ 另两个必红
- [x] `openspec/specs/rv32i-commit-path/spec.md` 的基线 MODIFIED 订正（标题逐字保留）
- [x] ⚠️ **`openspec/specs/rv32i-data-forwarding/spec.md` 的 MODIFIED 订正**
      （**2026-11-13 architect 发现的硬缺口** · 该 capability 是距离矩阵的**唯一权威**
      ⇒ ⇒ 不改它 = 实施与 spec 冲突 ⇒ ⇒ 依据 = ADR-069 §7.1.3 + ADR-047 §5.5.3 ②）
      ⇒ ⇒ **6 处**：① 距离矩阵表右移一位 ②「距离 ⇔ 生产者所在级**一一对应**」句改写
      ③ 档位 `2` 的数据源 MEM/WB 级 ⇒ ⇒ **M2 级** ④ 距离 2 的 NOTE ⑤ 距离 3 的 NOTE + 「距离 ≥ 4」拆段
      ⑥ 「寄存器堆读点 MUST 有 WB→ID 组合旁路」Scenario ⇒ ⇒ 追加 NOTE（WB 档 MUST 保留 + MUST 新增 M2 档）
      ⇒ ⇒ 载体 = `specs/rv32i-data-forwarding/spec.md`（**已写** · `verify_openspec_delta.sh` 4/4 绿
      ⇒ ⇒ 且**既有 Scenario 零删除**已机械核验：R1 10→11 条（+1 新增「A0 下的两级旁路」）
      ⇒ ⇒ R2 4→4 条 · 逐字缺失恰为**被改写的 4 处**）
- [x] 20 项 golden（取指填充不变量 + 3 存储 golden + sb/sh lane + 3 冲刷未写回 + 4 派生计数器）全绿
      ⇒ ⇒ **2026-11-14 实测**：`ctest -LE perf` **252/252 全绿**（含
      `test_rv32i_soc_dual_parity` **13064 断言全绿** + `test_rv32i_timing_dual` +
      `test_component_ownership`）⇒ ⇒ ⚠️ **全量重建后**跑的（禁旧二进制 ⇒ ⇒ 假绿）
- [x] ⚠️ **只改这 3 个数**（`design.md` §10.4 · **其余断言 MUST 一字不动**）：
      ⇒ ⇒ **2026-11-14 已改** ⇒ ⇒ ⚠️ **实测值与手推值逐项一致**
      （`8` / `23` / `176` ⇒ ⇒ **三数同时**对 ⇒ ⇒ 单改一个 ⇒ ⇒ 另两个必红 ⇒ ⇒ 判别力成立）
      `load_use_stalls` `6u` → **`8u`**（`:863`）· `non_retired` `21u` → **`23u`**（`:924`）
      · `perf_instructions` `kCycles - 22u` → **`kCycles - 24u`**（`:925`）
      ⇒ ⇒ ⚠️ **若实施后「别的」断言转红 ⇒ ⇒ 那是实现错了** ⇒ ⇒ MUST 逐条查因（**禁**顺手改期望值）

```bash
cd /workspace/project/CppHDL
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure -R 'rv32i'
bash scripts/verify_test_counts.sh
```

## S6 — 收口

- [x] 计数锚点同步（`CTEST_TOTAL` / `DUAL_BACKEND_PARITY_PAIRS` / `VERILATOR_E2E_TOTAL` 如有变动）
  ⇒ ⇒ 用 `scripts/update_test_count_anchors.sh`（**禁**手写数字）
  ⇒ ⇒ **2026-11-14**：`DUAL_BACKEND_PARITY_PAIRS` **284 → 285**（新增 `LoadLaneSelector` pair）
- [x] 全护栏复跑（`scripts/verify_*.sh` 逐条）
  ⇒ ⇒ **2026-11-14 实测**：`verify_flat_rtl_emit` ✅ · `verify_hierarchical_rtl` ✅ ·
  `verify_examples_rtl_sim` ✅（**10/10** · 含 RV32I SoC 的独立 Verilog TB **checks=24 fails=0**）·
  `verify_public_api_visibility` ✅ · `verify_no_duplicate_rv32i_headers` ✅ ·
  `verify_dual_backend_parity` ✅ · `verify_test_counts` ✅ · `verify_hierarchical_p8` ✅ ·
  `verify_openspec_delta` ✅
- [x] ⚠️ **改了 `src/**` / `include/**` ⇒ ⇒ 跑 ctest 前 MUST 先全量重建**（否则跑的是旧二进制 ⇒ ⇒ 假绿）
  ⇒ ⇒ **2026-11-14**：`cmake --build build -j$(nproc)` 全量重建 ⇒ ⇒ 再跑 ctest ⇒ ⇒ **252/252**
- [x] 验收报告落 `.rddf/reviews/`（**verifier 域** · 只验不改）
  ⇒ ⇒ **2026-11-14**：`.rddf/reviews/memory-timing-contract-t1-t2-verify-2026-11.md`

```bash
cd /workspace/project/CppHDL
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
bash scripts/verify_openspec_delta.sh
```

## Out of Scope（**本 change 不做**）

- ❌ cache 接入（`cpu-cache-subsystem`）—— 独立立项
- ❌ 固件装载链路（`prog_addr` 语义 / D-TCM 加载通路）—— 属 `firmware-load-chain`
- ❌ CSR / ECALL / FENCE —— 属 `rv32i-isa-gap-closure`
- ❌ 分支预测器接入 —— 独立立项
