# tasks: RV32I 流水线 MUST 接数据前推（ADR-046 §D5 的落地裁决）

- **change**: `rv32i-forwarding-decision-2026-10`
- **架构依据**: `docs/adr/ADR-047-rv32i-data-forwarding.md`（Accepted，2 轮 critic 审稿
  ⇒ 第 1 轮 APPROVE_WITH_CONDITIONS、第 2 轮 APPROVE；**2026-10-27 修订**
  ⇒ 新增 **D11**、改写 **D4 / D9**、更正**跳转归因**、修正 **D7**）
- **5 段草案**: `.rddf/improvements/rv32i-forwarding-decision-2026-10.md`
- **规格**: 5 个 capability（新建 `rv32i-data-forwarding` + 4 个 MODIFIED）

---

## 阶段拆分（ADR-047 §5.2）与**提交边界**的说明

ADR-047 §5.2 把实施拆成 C1 / C2 两阶段，各阶段**可独立验收**。
⚠️ 但本仓的**编译单元是耦合**的：删 `HazardUnit` 的 5 个预留端口（C2）
⇒ 7 个实例化点**同时**编译失败 ⇒ 「端口删除」与「实例化点更新」**必须同批**；
同理，前推判定加 `& !ex_is_load` 门控（D6）⇒ 改变独立实例化点的行为
⇒ 「门控」与「补 `ex_is_load` 驱动」**必须同批**。

⇒ **本 change 拆成 2 个 commit**（而非 2 个可独立构建的阶段）：
- **commit A** = C1 + C2 的**代码与测试**（一次落地 ⇒ 中间态始终可编译可测）
- **commit B** = 文档 / 规格 / 簿记同步

⇒ 验收判据与 ADR-047 §5.2 的 C1 / C2 判据**一一对应**（见下）。

---

## C1：前推接线 + D11 旁路 + 判别性证据

- [x] **1.1 `include/chlib/forwarding.h` —— mux 端口面（D4 修订）**
  - [x] `ForwardingMux` 新增 2 个**输入端口** `is_jump` / `jump_result`
  - [x] 档位 `1` 的数据 = `select(is_jump, jump_result, ex_mem_alu_result)`
        （**在 mux 组件内部**完成 ⇒ 可组件级单测）
  - [x] **同步**函数式接口 `forward_select<>`（同一份逻辑的**第二实现**
        ⇒ 不同步 ⇒ 两个 API 行为分叉）⇒ 加 2 个参数
  - [x] 档位**编号** MUST NOT 改变（`0`/`1`/`2`）⇒ 不违反
        `rv32i-canonical-home` 的「逐档等价」（该条约束**编号**，不是端口面）
  - [x] verify: `tests/chlib/test_forwarding.cpp` 的 6 处调用同步编译通过

- [x] **1.2 `include/cpu/pipeline/hazard_unit.h` —— D6 门控 + 假注释**
  - [x] 前推判定加 `& !ex_is_load`（`forward_ex_to_a` / `forward_ex_to_b`）
        ⇒ 档位 1 **保守地**不覆盖 load（load-use 停顿**行为不变**）
  - [x] 修 `:83-86` 的**假注释**（「前推数据输出（**经过前推MUX 后的数据**）」——
        今天 `:198-199` 是**直通** ⇒ 该注释**今天就是假的**）
  - [x] 修 `:141-146` 的「预留输入」注释（端口在 C2 删 ⇒ 注释同步删）

- [x] **1.3 `include/cpu/pipeline/rv32i_pipeline.h` —— 前推接线 + D11 旁路**
  - [x] 加 `ch::ch_module<chlib::ForwardingMux<32>>` 实例
  - [x] 加 `fwd_a_d1` / `fwd_b_d1` 两个 `ch_reg`（**档位 select 打一拍**，D5）
  - [x] 接线 3 个数据源：档位 0 = `id_stage.io().rs1_data`（ID/EX 锁存值）/
        档位 1 = `exmem_alu_result` + `is_jump`=`exmem_is_jump` +
        `jump_result`=`exmem_link_value` / 档位 2 = `wb_stage.io().write_data`
  - [x] `ex_stage.io().rs1_data` / `rs2_data` **改接** mux 输出 ⇒ **单一驱动**
        （D8：删 `:200` / `:201` 的旧驱动）
  - [x] **D11：寄存器堆读点的 WB→ID 组合旁路**（rs1 / rs2 各一条）
        ⇒ 数据叶是 `memwb_*` 寄存器 ⇒ 不成环；与 D7 正交；`x0` 无需特判
  - [x] **保留** `:484` / `:485` 的 `rs1_data_raw` / `rs2_data_raw` 驱动
        （D7 修订：正确性优先于整洁）⇒ 但 MUST 加**如实注释**

- [x] **1.4 `include/cpu/pipeline/stages/ex_stage.h`**
  - [x] 修 `:200-202` 的**过时注释**（「`JALR` 的 `rs1` ⇒ **无数据前推**
        （ADR-046 D5 悬置）」⇒ 已裁决「接」）

- [x] **1.5 组件级验收（2 条**固件级不可达**的变异 MUST 走组件级）**
  - [x] 「档位 1 忘接 `link_value`」⇒ **组件级单测**
        （直接驱动 mux 的 `is_jump=1` + `jump_result` + 档位 `1`
        ⇒ 断言 mux 输出 == `jump_result`）⇒ 放 `tests/chlib/test_forwarding.cpp`
  - [x] 「前推判定漏 `& !ex_is_load`」⇒ **组件级真值表**
        （独立驱动 `ex_is_load=1` + `ex_reg_write=1` + rd 匹配
        ⇒ 断言档位 **≠** `1`）⇒ 放 `examples/riscv-mini/tests/test_rv32i_pipeline.cpp`

- [x] **1.6 判别性固件（7 类判别性 + 1 类负向护栏）**
  - [x] ⚠️ **实施期更正**：**没有**新建测试文件 —— 判别性固件加在**既有的**
        `examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp` 里
        （**独立的一套固件** + **独立的 TEST_CASE**）
        ⇒ 复用该文件已有的观测 / 加载 / 双后端 harness（**不重复**）
        ⇒ 且既有 108 字 SoC 固件的 golden **零变化**
        ⇒ 好处：`CTEST_TOTAL` / `EXAMPLES_TEST_SOURCES` / NOTES 算术链**零改动**
  - [x] ① 相邻 ALU→ALU（无 NOP）⇒ 档位 1
  - [x] ② 距离 2 的 load→use ⇒ 档位 2
  - [x] ③ 距离 3 的 load→use ⇒ **D11 的读点旁路**
  - [x] ④ `JALR` 的 `rs1` 紧邻被写 ⇒ 档位 1
  - [x] ⑤ 分支条件用紧邻新值 ⇒ 档位 1
  - [x] ⑥ store 的**写数据（rs2）**紧邻被写 ⇒ mux 的 **b 路**
  - [x] ⑦ 双生产者同 rd ⇒ 档位优先级
  - [x] ⑧ `x0` 负向护栏（**不是**「未接前推」的判别性 ⇒ 是「去掉 `rd_nz` 门控」
        的判别性 ⇒ MUST 如实标注）
  - [x] **每个**用例 MUST 附带**反向期望**（`X_old != X_new`）
        ⇒ 否则该用例 MUST 被视为**无效**并重写
  - [x] 所有期望值 MUST **手工推导** + **代码注释留痕**
        ⇒ MUST NOT 从仿真输出**抄值**（循环论证）
  - [x] 双后端（interpreter + verilator）**逐拍全等** + **显式端口清单**

- [x] **1.7 负向对照 MUST 固化为常驻测试**
  - [x] `bf78367` 形态（数据源取自 EX/MEM 级**组合输出**）⇒ 拓扑排序**仍然** `CHFATAL`
        （否则「不成环」可能只是**环检测本身失效**）

- [x] **1.8 变异验证（每条都先确认编译成功）**
  - [x] 去掉打拍 ⇒ 红（档位错一级）—— **固件级**
  - [x] 交换档位 1 / 2 的数据源 ⇒ 红 —— **固件级**
  - [x] 前推判定漏 `& !ex_is_load` ⇒ 红 —— **组件级**（1.5）
  - [x] 档位 1 忘接 `link_value` ⇒ 红 —— **组件级**（1.5）
  - [x] 去掉 D11 的读点旁路 ⇒ 红 —— **固件级**（距离 3 用例）
  - [ ] ⚠️ 编译失败的变异 MUST NOT 计入「已抓到」（跑的是**旧二进制** ⇒ 假绿）

## C2：端口清理 + 双驱动收敛 + 文档 / 规格 / 簿记

- [x] **2.1 `HazardUnit` 删 5 个预留输入端口**（D7）
      （`wb_reg_write` / `wb_rd_addr` / `ex_alu_result` / `mem_alu_result` /
      `wb_write_data`）—— ⚠️ `ex_alu_result` 是**环饵** ⇒ 删掉让 ADR **自执行**
- [x] **2.2 全部 7 个实例化点同步**（删驱动 + 补 `ex_is_load` 驱动）
  - [x] `examples/riscv-mini/tests/forwarding_dut_fixture.h`（删 5 处 +
        补 `ex_is_load` + mux 新端口显式驱动 + 头注释「**非装配镜像**」）
  - [x] `examples/riscv-mini/tests/test_pipeline_debug.cpp`（删 5 处 + 补驱动）
  - [x] `examples/riscv-mini/tests/test_wrapper_simple.cpp`（删 5 处 + 补驱动）
  - [x] `examples/riscv-mini/tests/test_pipeline_exec.cpp`（删 **40** 处 + 补驱动）
  - [x] `examples/riscv-mini/tests/test_rv32i_pipeline.cpp`（删 **20** 处 + 补驱动）
  - [x] `include/cpu/pipeline/rv32i_pipeline.h`（删 5 处驱动）
- [x] **2.3 文档同步**（ADR-047 D10）
  - [x] 根 `AGENTS.md §KNOWN LIMITATIONS`：**3 条**过时（无前推约束 / 距离 2 /
        `JALR` 的 `rs1`）+ **补写「距离 3」**
  - [x] `include/cpu/AGENTS.md`：「`HazardUnit` 的『预留输入』端口无内部消费者」条目
  - [x] `include/cpu/pipeline/stages/ex_stage.h:200-202`（1.4 已做）
  - [x] `include/cpu/pipeline/hazard_unit.h:83-86`（1.2 已做）
  - [x] `examples/riscv-mini/tests/forwarding_dut_fixture.h` 头注释
- [x] **2.4 簿记**
  - [ ] `CTEST_TOTAL`（202 → +1 新测试 target）
  - [ ] `EXAMPLES_TEST_SOURCES`（17 → +1）
  - [ ] 根 `NOTES` 的算术链
  - [ ] `scripts/verify_example_assertion_quality.sh` 重跑（钉住断言计数）
  - [ ] `scripts/verify_example_test_wiring.sh` 重跑（归属 + 数量守恒）
  - [ ] `scripts/verify_openspec_delta.sh` + `openspec validate --strict`
- [ ] **2.5 回归**
  - [x] `ctest -LE perf` 零回归
  - [ ] `ctest -L perf` 全绿（**执行中** —— 2026-10-27 起跑，耗时 ~20-35 min）
  - [x] `run_all_ported_tests.sh` 28/28
  - [x] `run_all_samples.sh` 35/35
  - [x] 7 个护栏脚本全 exit 0
  - [x] **既有 golden 零变化**（既有 SoC 固件的 24 条写回序列 / 停顿计数 /
        退休数 MUST **一个都不变** ⇒ 归因 = 零）
- [ ] **2.6 规格同步**（archive 时）
  - [ ] 5 个 capability 的 delta 合并进主 spec
  - [ ] ⚠️ `rv32i-data-forwarding` 的 `## Purpose` **手工补写**
        （delta 里的 `## Purpose` **不会被 CLI 合并**）

## 验收（对齐 ADR-047 §5.2 的 C1 / C2 判据）

- [ ] **C1** ✅ 双后端 parity + **7 类**判别性用例 + **2 条组件级**验收
      + **档位对齐的实测**（距离由实测钉住，MUST NOT 预先断言）
- [ ] **C2** ✅ 护栏 + spec delta + 簿记
- [ ] **对抗审稿**（critic，≤ 2 轮）+ `openspec archive`
