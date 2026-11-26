# 验收报告 · `rv32i-trap-entry-2026-11`（RV32I trap 入口接线 · P6-3）

> **判据来源**（按优先级）：
> 1. `openspec/changes/rv32i-trap-entry-2026-11/proposal.md` 的 `## 验收标准`（**AC1..AC12**）
> 2. `design.md`（§1–§8 实施期订正）+ `tasks.md`（T0–T6）
> 3. capability delta（`rv32i-privilege` **ADDED ×1** + `rv32i-isa-semantics` **MODIFIED ×2**）
> 4. `docs/adr/ADR-048-rv32i-fetch-skid-buffer.md` **§8**（DA1–DA5 · dated addendum v1.1）
>    的 Verification 章（**V1–V9** · 经 §8 的 AC 映射）
>
> **判定纪律**：每条判据 MUST 有**工具证据**；期望值**手推**（禁抄仿真输出 ——
> 解释器与 codegen 同源 ⇒ 双后端一致地错 ⇒ parity 对该族缺陷结构性失明）。
> **已用轮次**：1（首次验收 · 无返工轮）。

---

## 1. 逐条判定

### AC1 · 非法指令 ⇒ `mepc` = 该指令 PC · `mcause` = 2 ✅ PASS

- **证据**：`examples/riscv-mini/tests/test_rv32i_trap_entry.cpp` 的
  `TEST_CASE("TRAP - 未知 opcode 陷入：mepc / mcause 写对且 PC 重定向到 mtvec")`
  ⇒ 手推：触发指令在 **PC 复位值** `0x8000_0000` ⇒ `mepc` = `0x8000_0000` · `mcause` = 2
  ⇒ 处理器（别名落点 word `0x040`）用 `csrrs` 读回 ⇒ 写回序列**恰好**
  `x1 = 0x8000_0000` · `x2 = 0x2`。
- **工具**：`./build/examples/riscv-mini/test_rv32i_trap_entry` ⇒ **6 用例 / 522 断言全过**（exit 0）。
- **判别力**：**M3**（`current_pc` 恒 0）· **M4**（`exception_cause` 恒 0）· **M2**（不重定向）
  ⇒ 该用例**转红** ✔

### AC2 · 触发指令**不退休** + IF/ID 被杀 ✅ PASS

- **证据**：同一用例断言 —— 触发拍（第一条 `reason == 2` 的拍）ID 级 `valid == 1` 且
  退休计数 == **0**（触发指令是第一条 ⇒ **它从未退休**）；其后**连续两拍** ID 级 `valid == 0`。
- **判别力**：**M7**（kill 站点 ④ 去掉 `trap_flush`）⇒ `valid` 永不清零 ⇒ 触发指令留在 ID
  ⇒ **活锁**（handler 永不执行 ⇒ 写回序列变空）⇒ **转红** ✔

### AC3 · PC 重定向到 `mtvec` 的 base ✅ PASS

- **证据**：`mtvec` 复位值 = `0x2000_0100` ⇒ 核顶层折成 **12 位字索引**
  ⇒ `(0x2000_0100 >> 2) & 0xFFF` = **`0x040`** ⇒ 处理器放在 word `0x040`
  ⇒ 5 个用例的写回序列**全部**经该落点 ⇒ 「重定向真的发生」有直接证据。
- **判别力**：**M2**（`if_stage.trap_flush` 恒 0）⇒ 落点永不执行 ⇒ 写回序列塌成
  「本路径的指令」⇒ 3 个用例转红 ✔

### AC4 · 在途**更老**指令照常退休（trap 不杀 EX/MEM/WB）✅ PASS

- **证据**：`TEST_CASE("TRAP - 在途更老的指令照常退休（trap 不杀 EX/MEM/WB）")`
  ⇒ `addi x3, x0, 0x11` → 非法字 ⇒ 写回序列 = `x3 = 0x11` · `x1 = mepc = 0x8000_0004`
  · `x2 = 2`。
- **判别力**：**M6**（`exmem_en` 再门控 `!trap_trigger` ⇒ trap 杀整条流水线）⇒ `x3` 无写回
  ⇒ **转红** ✔

### AC5 · 控制流冲刷**优先于** trap ✅ PASS

- **证据**：`TEST_CASE("TRAP - flush 优先：错误路径上的非法字 MUST NOT 触发 trap")`
  ⇒ `beq x0, x0, +8` 的 fall-through 槽放非法字 ⇒ 写回序列 = `x1 = 0` · `x2 = 0`
  （`mepc` / `mcause` 均保持复位值 ⇒ **没 trap**）。
- **判别力**：**M1**（`trap_trigger` 去掉 `!flush`）⇒ `mepc` 被**错误路径**的 PC 写坏
  ⇒ **转红** ✔

### AC6 · `trap_flush_d1` 多杀一拍 + skid 清空 ✅ PASS

- **证据**：
  1. 核级：AC1 用例断言的「其后**连续两拍** ID 级 `valid == 0`」= ① 清零 + ② `trap_flush_d1`；
  2. 组件级：`tests/test_rv32i_pipeline_behavior.cpp` 的 **Batch 5-e**（trap 拍不装载 +
     下一拍**仍不装载**）与 **Batch 5-f**（trap 清空**非空** skid **且**不填充）。
- **判别力**：**M8**（kill 站点 ① 去掉 `trap_flush_d1`）⇒ Batch 5-e **转红** ·
  **M9**（kill 站点 ③）与 **M9b**（kill 站点 ②）⇒ Batch 5-f **各自转红** ✔
- ⚠️ **如实登记（M8 的边界）**：M8 在**核级不红**。理由 = `trap_flush_d1` 漏杀只让
  **pc↔指令的配对**错一个字；`mepc` 仍正确、handler 仍执行 ⇒ 核级观测面（写回序列 +
  `dbg_id_*`）**看不出来**（本核没有「读当前 PC」的 CSR）⇒ 判别力归**组件级**
  （ADR-048 §8 **D9.4** 的定位）。详见 `design.md` §8.3。

### AC7 · `CsrBank` 单写口仲裁 + `mepc`/`mcause` 成对 ✅ PASS

- **证据**：`TEST_CASE("TRAP - 单写口仲裁：trap 上下文优先于 CSR 指令的写")`
  ⇒ `addi` → `csrrw x0, mscratch, x1` → 非法字 ⇒ `csrrw` 在 **EX 级**的那一拍 ⇔ 非法字在
  **ID 级** ⇒ **同拍冲突** ⇒ 手推：写口让给 trap ⇒ `mscratch` 保持**复位值 0** ·
  `mepc` = `0x8000_0008`；实测写回序列 = `x1 = 0x2A` · `x3 = 0x0` · `x4 = 0x8000_0008`。
- 成对性：AC1 用例的 `mepc` / `mcause` 是**同一次**异常的一对（`mcause` 晚 1 拍写入 ⇒
  与 `mepc` 同源）⇒ `x1` = 触发 PC 且 `x2` = 2 同时成立。
- **判别力**：**M12**（让 CSR 指令优先）⇒ 该用例**转红** ✔

### AC8 · 零端口面变更 ✅ PASS

- **证据**：
  - `bash scripts/verify_riscv_soc_core_ports.sh` ⇒ **PASS**
    （V1 端口表 == AGENTS.md 的 **53** 行逐字逐序 · V2 = **53** · V3 核自有 **51** 与 C6 快照逐字一致）；
  - `bash scripts/verify_no_duplicate_rv32i_headers.sh` ⇒ **PASS**（**C6**：51 端口集合不变）；
  - `test_rv32i_hier_rtl`（全量回归内的 ctest 条目 #27）⇒ 断言 `module_count == 8`
    （`ExceptionUnit` 用 `ch_module` inline ⇒ **不新增层**）⇒ **PASS**（见 §5）。
- **机制**：`ExceptionUnit` 的 8 个输出**全部被消费**（含 `mret_pc` / `mret_valid` /
  `trap_entry_valid` 的恒等门）⇒ 无 ch_out 冒泡；9 个输入**全部驱动** ⇒ 无悬空输入。

### AC9 · 每条新判据附一条变异（且变异 MUST 仍能编译）✅ PASS

- 见 §4 的变异表：**13 条**（M1–M12 + M9b），**全部 BUILD-OK**，**12 条转红**；
  M8 只在**组件级**转红（已如实登记）。

### AC10 · spec delta（禁 `--skip-specs`）✅ PASS

- **证据**：`openspec/changes/rv32i-trap-entry-2026-11/specs/` 下
  `rv32i-privilege/spec.md`（**ADDED ×1**）+ `rv32i-isa-semantics/spec.md`（**MODIFIED ×2**，
  既有内容一条不删）。
- `openspec validate rv32i-trap-entry-2026-11` ⇒ **valid**（零 WARNING）；
  `bash scripts/verify_openspec_delta.sh` ⇒ **4/4 绿**（R1–R4）。

### AC11 · 全量回归零失败 + 全部 `verify_*.sh` PASS

- 见 §5（`ctest -LE perf` 的实测结果与守卫清单）。

### AC12 · `illegal_reason = 1` ⇒ **仍冻结**（不 trap）✅ PASS

- **证据**：`TEST_CASE("TRAP - reason=1（ECALL）仍冻结：不 trap、不重定向")`
  ⇒ **零写回**（别名落点从未执行）· 逐拍退休计数恒 **0** · `reason` 恒 **1**（从不出现 2 / 3）。
- **判别力**：**M5**（触发集放宽到 `!= 0`）⇒ ECALL 也会 trap ⇒ **转红** ✔

### 追加 · `illegal_reason = 3`（ISA 保留组合）⇒ **陷入** ✅ PASS

- **证据**：`TEST_CASE("TRAP - reason=3（保留组合）⇒ 陷入（mepc = 触发指令的 PC）")`
  ⇒ 写回序列 = `x1 = 0x8000_0000` · `x2 = 2`。
- **判别力**：**M11**（未知 opcode 的 reason 2 → 1）⇒ 触发集失去一半 ⇒ 2 个用例转红 ✔

---

## 2. 遗留清扫

| 项 | 结论 |
|---|---|
| TODO / FIXME / 半截实现（本次改动面） | 无 ⇒ `grep -nE "TODO\|FIXME\|XXX\|HACK"` 在 `include/cpu/pipeline/{rv32i_pipeline.h,stages/if_stage.h}` + 两个测试文件上**零命中** |
| 注释掉的代码 | 无（`if_stage.h` 的注释块是**取代记录**，不是注释掉的实现） |
| 临时调试日志 / `printf` | 无（曾用 `TEST_CASE("DEBUG trace")` 做逐拍探针 ⇒ **已删**；`grep -n "printf"` 只命中测试框架的 `snprintf` 格式化辅助） |
| 被 `SKIP` 的测试 | 无新增 |
| 未替换的 mock / 临时 fallback | 无（核级 DUT 用**真** I-TCM `sread`；组件级用**真** `IfStage`） |
| 死代码 | 无：新增的 `trap_pc_selected` / `csr_instr_addr` / `csr_instr_write_en` / `exc_write_en` 全部被消费（`ExceptionUnit` 的 8 个 `ch_out` 亦全部被消费 ⇒ 无冒泡） |
| 计数锚点 | `CTEST_TOTAL` 283→284 · `EXAMPLES_TEST_SOURCES` 25→26 · `DUAL_BACKEND_PARITY_PAIRS` 291→297 · `include/cpu/AGENTS.md` 7384→7507 ⇒ **全部同批同步并由守卫复核** |

---

## 3. 已知边界（如实登记 · 不静默留尾）

1. **`trap_flush_d1` 漏杀在核级不可观测**（AC6 · M8）⇒ 判别力归组件级 Batch 5-e
   （`design.md` §8.3）。
2. **DA3 的 ②③ 两站点互相掩护** ⇒ 判据排程必须让 skid 在 trap 拍为空（`design.md` §8.2）。
3. **CSR 指令的写被抑制后不补偿**（`CsrBank` 单写口无队列）⇒ 设计选择，已在
   `proposal.md` S7 / capability delta 的 Scenario 里写死。
4. **`mtvec` 的取值语义（D-1）/ `mcause` 编码表（D-2）** 仍待用户 / architect 裁决
   ⇒ 本项只把 `mtvec` 的**当前值**当输入（`0x2000_0100`）· `mcause` 只写 **2**。

---

## 4. 变异登记（AC9 的证据）

> 全部经 `cmake --build --target <target>` 验证 **BUILD-OK**（**编译失败 ⇒ 跑的是旧二进制 ⇒ 假绿**）。

| # | 变异（还原错误实现） | 判据 | 结果 |
|---|---|---|---|
| **M1** | `trap_trigger` 去掉 `& !hazard.io().flush` | AC5 | ✅ 红（AC5 用例） |
| **M2** | `if_stage.io().trap_flush <<= ch_bool(false)` | AC3 / AC1 | ✅ 红（3 用例） |
| **M3** | `exception.io().current_pc <<= ch_uint<32>(0)` | AC1（`mepc`） | ✅ 红（3 用例） |
| **M4** | `exception.io().exception_cause <<= 0` | AC1（`mcause`） | ✅ 红（3 用例） |
| **M5** | 触发集 → `illegal_reason != 0` | AC12 | ✅ 红（AC12 用例） |
| **M6** | `exmem_en` 再门控 `& !trap_trigger`（trap 杀整条流水线） | AC4 | ✅ 红（AC4 用例） |
| **M7** | `if_stage` kill 站点 ④ 去掉 `trap_flush` | AC2 / AC6（活锁） | ✅ 红（核级 3 用例 + Batch 5-d） |
| **M8** | `if_id_update_en` 去掉 `trap_flush_d1` | AC6 | ✅ 红（**仅** Batch 5-e · 核级不红 ⇒ §3 已登记） |
| **M9** | `skid_fill_en` 去掉 `trap_flush \| trap_flush_d1` | AC6（DA3 ③） | ✅ 红（Batch 5-f） |
| **M9b** | `skid_valid` 的清空去掉 `trap_flush \| trap_flush_d1` | AC6（DA3 ②） | ✅ 红（Batch 5-f） |
| **M10** | `next_pc_target` 变成 `branch > trap` | AC3（优先级） | ✅ 红（Batch 5-a） |
| **M11** | 未知 opcode 的 `reason` 2 → 1 | 触发集 | ✅ 红（2 用例） |
| **M12** | 写口仲裁改「CSR 指令优先」 | AC7 | ✅ 红（AC7 用例） |

⚠️ **DA3 的 ②③ 站点互相掩护** ⇒ M9 / M9b 的可观测性依赖 Batch 5-f 的**特定排程**
（skid 在 trap 拍为空）⇒ 该排程已写进用例注释（`design.md` §8.2）。

---

## 5. 全量回归 + 守卫

### 5.1 全量回归（AC11）

- **命令**：全量重建（`cmake --build build -j$(nproc)`，0 error）⇒ `ctest --test-dir build -LE perf -j$(nproc)`
- **结果**：**`100% tests passed, 0 tests failed out of 273`**（`Total Test time (real) = 714.76 sec`）
- 关键条目：`test_rv32i_trap_entry`（#12）· `test_rv32i_csr_instructions`（#11）·
  `test_rv32i_pipeline_behavior`（#235）· `test_rv32i_hier_rtl`（#27 · 断言
  **`module_count == 8`** ⇒ AC8 的层次化面证据）· `test_rv32i_soc_dual_parity`（#26 ·
  含本轮按语义变更订正的 3 条既有判据）· `test_rv32i_official_suite_g1..g4`
  （官方 `rv32ui-p-*` 40 个 ⇒ 不受非法指令语义变更影响）

> ⚠️ **首次回归红过 3 项**（`test_pipeline_wrapper` · `test_component_ownership` ·
> `test_rv32i_soc_dual_parity`）⇒ ⇒ **全部**是本项语义变更的**合法影响**
> （见 §6 的爆炸半径表）⇒ ⇒ 订正后重跑 **273/273 绿**。
> ⚠️ **教训（已记入 `design.md` §8.4）**：「冻结 → 陷入」的爆炸半径 MUST 在实施前
> 用「跑一遍全量」发现，而不是靠推理（我第一版手推 `retire` 就漏看了 `kTailFillWords`）。

### 5.2 守卫（`scripts/verify_*.sh`）

| 守卫 | 结果 |
|---|---|
| `verify_riscv_soc_core_ports.sh` | ✅ PASS（V1 53 逐字逐序 · V2 = 53 · V3 C6 **51**） |
| `verify_no_duplicate_rv32i_headers.sh` | ✅ PASS（C5 行数 7507 · **C6** 端口集合 51 不变） |
| `verify_test_counts.sh` | ✅ `CTEST_TOTAL` 284 = `ctest -N` 284 · `VERILATOR_E2E_TOTAL` 81 = 81 |
| `verify_dual_backend_parity.sh` | ✅ `PARITY_COUNT` 67 · `PARITY_PAIRS` 297（双锚点一致） |
| `verify_openspec_delta.sh` | ✅ 4/4（R1–R4） |
| `verify_agents_size.sh` | ✅ 52100 B ≤ 72 KiB |
| `verify_roadmap_fragments.sh` | ✅ AUTO 块守恒 |
| `verify_docs_links.sh` | ✅ PASS（L1 47 篇 / 192 链 / 177 引用） |
| `verify_example_test_wiring.sh` | 见 §5.3 |
| `scripts/rv32i_rtl_sim_env.sh`（产物级端到端：生成 → lint → 编译 → 运行） | 见 §5.3 |

### 5.3 产物级 / examples 门

| 门 | 结果 |
|---|---|
| `scripts/rv32i_rtl_sim_env.sh`（生成 → lint → 编译 → 运行 · **两种形态**） | ✅ **PASS** ⇒ `[flat] RESULT: checks=24 fails=0` · `[hier] RESULT: checks=24 fails=0` |
| `verify_hierarchical_rtl.sh`（R1–R8 + R8′ + R9） | ✅ **PASS**（R8′ = RV32I 产品语料的顶层端口表 == 手推 **53** 根 · 独立于产物） |
| `verify_example_test_wiring.sh`（C1 归属守恒 · C2 数量守恒 · C3 逐条 exit 0） | ✅ **PASS**（C1/C2/C3/C3b 全绿 · 26 个 target 逐条 exit 0） |

> ⚠️ **`verify_example_test_wiring.sh` 的首次运行是「假红」**：我在**变异**期间跑它 ⇒
> 当时 `build/` 里的二进制是**变异版**（源码已还原但**没重建**）⇒ ⇒ 3 项 exit≠0。
> ⇒ ⇒ 这正是仓内铁律「**还原源码 ≠ 复原（二进制也是状态）**」的实例 ⇒ ⇒ 全量重建后复跑才有效。

---

## 6. ⚠️ **语义变更的爆炸半径**（受影响的**既有**判据 · 全部已订正）

P6-3 把 `illegal_reason ∈ {2,3}` 的处置从「**流水线级冻结**」改为「**陷入**」
⇒ ⇒ 三条**既有**判据的期望值因此过时（**判据变更，不是行为回归**）；

| # | 文件 / 用例 | 旧期望 | 新期望（+ 依据） |
|---|---|---|---|
| 1 | `examples/riscv-mini/tests/test_rv32i_csr_instructions.cpp` 的 `funct3=4（保留组合）⇒ reason 3` | `retired == 0`（冻结） | 只保留**译码级**判据 `reason == 3`；「陷入」的完整语义归 `test_rv32i_trap_entry.cpp`（P6-3 的归属文件）· 镜像 MUST 在别名落点 `0x040` 放**自环**（否则 NOP 跑出镜像 ⇒ 读到 0 ⇒ `reason = 2` 混入判据） |
| 2 | `examples/riscv-mini/tests/test_pipeline_wrapper.cpp` 的 `PC advances after reset release` | 只填 word 0 = NOP | **填 8 个 NOP**（`rst=1` 期间填 ⇒ PC 不动）⇒ 否则 PC 走到 word 1（未初始化 = **0** = 未知 opcode）⇒ **trap** ⇒ `instr_addr = 0x20000108`（实测）⇒ 测的就不再是「PC 推进」 |
| 3 | `examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp` 的 `UnknownOpcodeFreezes + FenceIsNop` | 末拍仍持有 `reason` / ID PC 停在探针 | 拆成 `freezes` 两类：`reason = 1`（ECALL）**仍冻结**（末拍断言逐字不变）· `reason ∈ {2,3}` ⇒ 断言「`reason` **出现过**」+「探针**永不退休**」+「最终 ID PC **不是**探针」；`retire(20)` 8 → **13**（陷入后落到别名 word `0x040` = 尾部 NOP ⇒ 照常退休 ⇒ 手推 8+5） |

⚠️ **第 3 条的手推更正（实测抓出的推导错误）**：我第一版推 `retire` 恒 8（假设落点读到 0
⇒ 再陷入 ⇒ trap 循环）⇒ ⇒ **实测 13** ⇒ ⇒ 根因 = `load_firmware` 会补 **256** 个尾部 NOP
（`kTailFillWords`）⇒ ⇒ 落点 word `0x040` 是 **NOP**；`13 = 8 + 5`（c=15..19 各一条）。
⇒ ⇒ **已按实测订正注释与常量**（并保留「延迟模型 `retire(c) = c − 4` 的推广」作为推导依据）。

---

## 7. ⚠️ 归档合并的**收尾订正**（`openspec archive` 的 MODIFIED 是**追加**语义）

`openspec archive rv32i-trap-entry-2026-11 -y` 的实测结果：

```
Applying changes to openspec/specs/rv32i-isa-semantics/spec.md:  ~ 2 modified
Applying changes to openspec/specs/rv32i-privilege/spec.md:       + 1 added
Change 'rv32i-trap-entry-2026-11' archived as '2026-11-26-rv32i-trap-entry-2026-11'.
```

⚠️ **MODIFIED 的实际效果 = 把 delta 正文插在 Requirement 头之后、旧正文之前**
⇒ ⇒ **不删旧文** ⇒ ⇒ 合并后出现 **5 处残留**（全部已手工订正）：

| # | 残留 | 订正 |
|---|---|---|
| 1 | 旧的「**行为策略 = 冻结**」段仍与新「按 `illegal_reason` 分流」段**并存**（自相矛盾） | 删除旧段 |
| 2 | `#### Scenario: 冻结 —— 该指令永不退休` + 其 `WHEN` 成**孤立块**（无 `THEN`） | 删除（已由两条新 Scenario 取代） |
| 3 | `#### Scenario: flush 的残留不得触发冻结` 同样孤立 | 删除孤立头 |
| 4 | `#### Scenario: 正确路径的冻结仍是终态` 同样孤立 | 删除孤立头 |
| 5 | 两条 PC 判据公式**并存**（`!rst & !(stall & !(flush & branch_valid))` 与含 `trap_redirect` 的新式） | 删除旧式 |

**另有两处「因语义变更而过时的旧文」**（不属合并残留，属爆炸半径）：

| # | 位置 | 订正 |
|---|---|---|
| 6 | `rv32i-privilege` 的 Scenario「保留组合与 `funct3 = 0` MUST 仍然冻结」 | `funct3 = 4`（reason 3）⇒ **陷入**；`funct3 = 0`（reason 1）⇒ 仍冻结（标题同改） |
| 7 | `rv32i-privilege` 的「与冻结语义的接缝」段（说「本 capability **不改变**那条冻结 Requirement」） | 改为「**已由 MODIFIED delta 修订**（P6-3）」 |
| 8 | `rv32i-isa-semantics` 的「本版只覆盖 opcode」Scenario（`reason = 3` ⇒「MUST 冻结」） | `reason = 3` ⇒ **陷入** |

**`## Purpose` 手工更新（T6.4）**：`rv32i-privilege` 的 Purpose 由「两段契约」改为「**三段**」
（① trap 的触发与重定向 · ② 上下文写入机制 · ③ CSR 指令语义），并同步边界 / 接缝两段。

**复核**：`openspec validate --specs` ⇒ **54 passed / 0 failed** ·
`verify_openspec_purpose.sh` ⇒ **PASS** · 两份 spec 的 Scenario **零孤立块**
（自写检查器：`#### Scenario:` 块内 MUST 有 `- **THEN**`）。

⚠️ **已知的既有形状问题（**不**由本项引入 · 仅登记）**：`rv32i-privilege` 的
`#### Scenario: 6 种 funct3 的 csr_wdata 语义`（P6-2 落地）是**纯表格式** Scenario ⇒
无 `THEN` 行 ⇒ 同一个检查器会点它 ⇒ ⇒ **本次不修**（属 P6-2 的形状选择，须独立立项）。
