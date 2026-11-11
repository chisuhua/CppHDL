# 验收报告：B0 批六项（`rv32i-dead-component-consolidation` ⑧ + C5/C6/C7/C8 + `port-naming-ratchet` ⑥ + `node-name-prefix-consistency` F-A）

- **验收对象**：提交 `67926cf` + `bd21b45`（合起来 = B0 的完整改动）
- **验收角色**：verifier（**只验不改实现**）
- **待验收树**：`bd21b45`（工作区另有 architect 域的 4 个文档改动 + 1 份审稿记录 ⇒ **纯文档** ⇒ 零代码面 ⇒ 不影响本报告结论）
- **判据来源**（按 verifier 产出契约的优先级）：
  ① 用户本次明确给出的验收条件 → **无**（用户给的是「执行下一步」⇒ ⇒ 无显式验收条件）
  ② `openspec/changes/<name>/proposal.md` 的 `## 验收标准` 段 → **不适用**（B0 不是 openspec change）
  ③ 本次变更自己声明了什么 → **`.rddf/improvements/rv32i-dead-component-consolidation-2026-11.md` 的 `## Acceptance` 段**（逐条）⇒ ⇒ **本报告的主判据**
  ④ ADR 的 Verification 章 → 本批**无**（⑧/C5/C6/C7/C8 无 ADR；⑥/F-A 的契约化 change 另见 §6）
- **分流**：`rdd-verifier` 的 canonical preconditions **不满足**
  （① `grep -lE '^## (验收标准|Acceptance Criteria)$'` = **空**（两个 change 的标题带「（AC）」后缀）·
  ② 无 `.rddf/state/iteration.json` ⇒ P3 archive gate 未触发）⇒ ⇒ **走本地验收 1–5 步**

---

## 1. 验证顺序（早失败早停，逐条有工具证据）

| 步 | 内容 | 结果 | 证据 |
|---|---|---|---|
| **1** | 编译 / 静态检查 | ✅ | `cmake --build build -j$(nproc)` ⇒ **0 error / 0 warning** 输出 |
| **2** | 单元测试 | ✅ | `ctest --output-on-failure` ⇒ **100% tests passed, 0 tests failed out of 257**（base 220 · chlib 1 · perf 11 · spinalhdl-ported 25 · verilator 1 · **verilator_e2e 76**）· 总耗时 1614.99 s |
| **3** | 集成 / 端到端（**真跑**） | ✅ | `./run_all_ported_tests.sh` ⇒ **PASS 28 / FAIL 0 / STALE 0 / MISSING 0**（C2 数量守恒：跟踪 28 == 实际执行 28） |
| **4** | 边界与错误路径 | ✅ | 见 §3 的「边界覆盖」列 |
| **5** | 遗留清扫复核 | ✅ | 见 §5 |
| **+** | **护栏复跑（9 条）** | ✅ | `verify_test_counts` · `verify_dual_backend_parity` · `verify_public_api_visibility` · `verify_roadmap_fragments` · `verify_arch_ledger_residuals` · `verify_hierarchical_rtl` · `verify_hierarchical_port_naming` · `verify_example_assertion_quality` · `verify_example_test_wiring` ⇒ **全部 PASS** |

---

## 2. 六项逐条验收（主判据 = 草案 `## Acceptance` 段）

### 2.1 ⑥ 关键字表补全（`port-naming-ratchet` N3）

| 判据 | 判定 | 证据 |
|---|---|---|
| 关键字表 MUST 覆盖 `bit`/`time`/`event`/`table` | ✅ | `include/codegen_naming.h` 表 **246 词**（实测解析）⇒ 4 词全部在内 |
| MUST 是 IEEE 1800-2017 保留字全集（**非凭记忆**） | ✅ | 文件头注释记录**实测来源**：逐词用本仓 `build/verilator-install/bin/verilator` 判「`logic <词>;` 是否报语法错误」（对照词 `zzznotakeyword`/`foo`/`my_port` 均通过 ⇒ 探针有判别力）· 排除 `endcasex`/`endcasez`（实测非保留字） |
| 零产物影响（**实测**） | ✅ | 71 个 `.v` 产物抽取 **8585** 个标识符 ⇒ 与关键字**完全相等** = **0** |
| 单元判据（**禁**用产物 diff） | ✅ | `tests/test_verilog_name_sanitize.cpp`（6 用例 / **145** 断言）· 注册于 `tests/CMakeLists.txt:1081` |
| 反向判据（MUST NOT 为绿而误加词表） | ✅ | 判据 4「非保留字 MUST NOT 被转义」（含 `endcasex`/`endcasez`/`global`）· 判据 6「幂等」（防 `bit_r` 被当关键字 ⇒ 名漂移） |

### 2.2 ⑧ SLTU 修缺陷（`rv32i-dead-component-consolidation` AC①）

| 判据 | 判定 | 证据 |
|---|---|---|
| MUST 自带判据：`execute_alu(0, 0xFFFFFFFF, SLTU) == 1` | ✅ | `tests/test_rv32i_alu.cpp`（4 用例 / **45** 断言）· 注册于 `tests/CMakeLists.txt:1194` |
| 实现 MUST 与仓内权威同口径 | ✅ | `rv32i_alu.h:103` = `(io().operand_a < io().operand_b)` ⇒ **逐字等于** `ex_stage.h:189` 的 `(io().rs1_data < alu_second_operand)` |
| 两后端**无符号** | ✅ | interpreter 侧 `bv_lt_truncate` → `bv_lt<is_signed=false>`（`include/bv/bitvector.h:915`）· Verilog 侧发射 `<` 作用于 `logic [31:0]`（`src/codegen_verilog.cpp:215`）⇒ ⇒ **两侧同口径** |
| 期望值 MUST **手推**（禁抄仿真输出） | ✅ | 判据 1 的 6 组期望值全部在测试里**手写注释推导**（`0-0xFFFFFFFF=0x00000001 ⇒ bit31=0`） |
| 反向判据（SLT MUST 仍**有符号**） | ✅ | 判据 2：同一组操作数上 SLT=1 而 SLTU=0（`0x80000000` vs `0`）⇒ ⇒ 防「把 SLT 也改成无符号」 |
| 边界覆盖 | ✅ | `0`/`1`/`0x7FFFFFFF`/`0x80000000`/`0xFFFFFFFF` 两两组合（判据 3 · 25 组）· 相等（`0x12345678`）⇒ 0 · 其余 ALU 操作回归（判据 4 · 9 项） |
| 符号常量（MUST NOT 用字面量 `3`） | ✅ | 测试用 `riscv::ALU_SLTU` 等符号常量（注释明确「禁字面量 3 —— 那是 funct3 编码」） |

### 2.3 C5 + C6 同批退役

| 判据 | 判定 | 证据 |
|---|---|---|
| 三个头**物理删除** | ✅ | `rv32i_forwarding.h` · `hazard_detection_unit.h` · `rv32i_hazard_complete.h` ⇒ 文件**不存在**（实测 `-e` 判否）· 提交 `67926cf` |
| 全仓 grep **零引用** | ✅ | 全仓 `riscv::ForwardingUnit` / `riscv::HazardDetectionUnit` / `riscv::HazardUnitComplete` ⇒ 仅存于**退役登记**（`AGENTS.md` 锚点注 · `docs/CHANGELOG.md` · `include/cpu/AGENTS.md` 退役登记 · `examples/riscv-mini/AGENTS.md` · 历史归档 change）⇒ ⇒ **零活引用** |
| 测试用例**逐条**处置并登记 | ✅ | 6 个 SECTION（`tests/test_rv32i_pipeline.cpp`：ForwardingUnit ×4 + HazardDetectionUnit ×2）· 5 个 dual pair（`_behavior.cpp`：×3 + ×2）· 2 个测试文件整删（`test_hazard_complete.cpp` / `test_hazard_complete_dual.cpp`）· `test_phase4_integration.cpp` 的 DUT 换成 `riscv::HazardUnit`（**保留**「多组件可同时实例化」覆盖）· `tests/CMakeLists.txt` 的两处注册块清理 ⇒ ⇒ 逐条均有登记注释 |
| `public-api-surface.md`「已移除」节 + `docs/CHANGELOG.md` 一行 + 护栏全绿 | ✅ | 移除表 **3 行**（`:81-83`，含「物理删除」+ 迁移指引）· CHANGELOG 的 `### Removed` 段 **3 条** · `verify_public_api_visibility.sh` **PASS**（探针 24→**30** 条，移除表 15→**18** 行，R5/R6 双向绑定） |
| **逐个**有裁决记录 | ✅ | `public-api-surface.md` 三行各含「依据」列（C5 / C6 / C6）· `include/cpu/AGENTS.md` 的「退役登记」段 |
| 缩面须**显式批准**（metis finding 9） | ✅ | 用户 2026-11-11 批准（对话记录）⇒ ⇒ 四个锚点**全部**标注「这是显式批准的缩面」：`CTEST_TOTAL` 257（增删相抵）· `VERILATOR_E2E_TOTAL` 77→**76** · `DUAL_BACKEND_PARITY_TOTAL` 66→**65** · `DUAL_BACKEND_PARITY_PAIRS` 290→**284** ⇒ ⇒ 双护栏实跑 PASS |

### 2.4 C7 交接登记（诚实化）

| 判据 | 判定 | 证据 |
|---|---|---|
| 每个**保留但不接入**的组件 MUST 明写「⚠️ 非流水线路径」 | ✅ | `include/cpu/AGENTS.md:84-90` 共 **7 行**：`riscv::BranchPredictor` · `BranchPredictorV2` · `BranchDetector` · `BranchConditionUnit` · `ControlHazardUnit` · `BranchPredictorState` · `Rv32iPc` ⇒ ⇒ 逐个核实**生产引用数 = 0**（口径写进表下） |
| 分支预测器 MUST 明写「未接入 ⇒ 每个 taken 分支付 3 拍冲刷」 | ✅ | 表内明写 + 引 `rv32i_pipeline.h:599`（「`JAL`/`JALR` —— 它们同样让流水线空 3 拍」）⇒ ⇒ **事实已核实**（该行确实存在且表述一致） |
| 命名空间 MUST 正确 | ✅ | 全部 `riscv::`（实测逐个查 `namespace riscv` 声明）⇒ ⇒ **无** `chlib::` 误写 |
| 根 `AGENTS.md` 的 KNOWN LIMITATIONS MUST **NOT** 声称流水线由这些组件构成 | ✅ | `AGENTS.md` 里对这三个名字的**唯一**提及在 `:465` / `:486` 的**缩面说明**里 ⇒ ⇒ **不构成**「流水线由其构成」的声称 |

### 2.5 C8 `mem_is_load` tie-off

| 判据 | 判定 | 证据 |
|---|---|---|
| 装配点 MUST 显式 tie-off | ✅ | `include/cpu/pipeline/rv32i_pipeline.h:530` = `hazard.io().mem_is_load <<= ch_bool(false);` |
| **组件公开面零变更**（`mem_is_load` 端口 MUST 保留） | ✅ | `include/cpu/pipeline/hazard_unit.h` **未被 B0 改动**（`git diff` = 空 · 最后修改者是更早的 `0c48078`）⇒ ⇒ `__io` 逐字相同 · `ch_in<ch_bool> mem_is_load;`（`:56`）仍在 |
| **产物判据**：顶层端口表 MUST **不再**出现 `owner = …hazard_unit` 的 input ⇒ ⇒ **顶层 input 由 8 → 7** | ✅ | **独立探针实测**（生成 RV32I SoC 的层次化 Verilog 并解析顶层 module 端口表）：<br>**修复后**：`TOP_INPUTS = 7`（`default_clock` · `default_reset` · `top_unnamed_input` · `_1` · `_2` · `_3` · `_4`）⇒ ⇒ `HAZARD_UNIT_UNNAMED_INPUT_IN_TOP_PORTS = 0`<br>**产物内**：`top_pipeline_hazard_unit_unnamed_input_4` 降级为**内部 `logic`**（`.v:458`）⇒ ⇒ 不在端口表内 |
| **行为等价**：golden 全绿（退休数 / `load_use_stalls` / `control_stalls` / store 全不变） | ✅ | ① `test_rv32i_soc_dual_parity` 属 `verilator_e2e` 标签 ⇒ **76/76 PASS**；② **golden 断言文件未被 B0 改动**（`git diff 67926cf~1 bd21b45 -- examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp` = **空**）⇒ ⇒ 期望值一字未改而全部通过 ⇒ ⇒ **行为零变化** |
| **4 态证据**：独立于 Verilator 的可判别证据 | ✅ | 上面的静态产物判据（端口表里没有它）**已足够**（草案原文如此）⇒ ⇒ **可判别**（M4 变异证明它会转） |
| **禁** `--skip-specs` 类误解（补 driver ≠ 改契约） | ✅ | 契约 `rtl-hierarchical-output/spec.md:309-316` 说的是「**无 driver 的** input」⇒ ⇒ 补 driver = 修缺陷 ⇒ ⇒ 与契约**相容**；主 spec 该段**未被触碰**（`git log` 显示最后修改者为更早的 `aed393b`） |

### 2.6 F-A 前缀口径对齐（`node-name-prefix-consistency` P1）

| 判据 | 判定 | 证据 |
|---|---|---|
| `include/component.h` 的 `\|\| parent_path == "unnamed"` MUST 删 | ✅ | `include/component.h:293` = `if (parent_path.empty()) {` |
| MUST 与 `src/core/lnodeimpl.cpp:30-35` **同口径** | ✅ | `lnodeimpl.cpp:33` = `name_ = parent_->name() + "." + name;`（**不剥离**）⇒ ⇒ 组件侧也不再剥离 ⇒ ⇒ 同口径 |
| 公开面登记（行为变更，非符号移除）+ **代价明写** | ✅ | `public-api-surface.md` 新增「**行为变更（非符号移除）**」节 **3 条**（含 F-A / 代价 / C8）· CHANGELOG 的 `### Changed` 段 · ⚠️ **代价已明写**：「空名与用户真名 `"unnamed"` 在 `name()` 上**永久不可区分**」⇒ ⇒ 且注明「彻底解决须**另立 item**」 |
| 边界：空名根 / 真名 `"unnamed"` / 非空父名 | ✅ | `tests/test_component_naming.cpp` 新增 **4 用例 / 27 断言**（含**反向**判据：非空父路径 MUST NOT 受影响） |

### 2.7 不回归（★5 纪律）

| 判据 | 判定 | 证据 |
|---|---|---|
| ctest 全绿 | ✅ | **257 / 257**（`ctest --output-on-failure`） |
| 判据 MUST 引用 `CTEST_TOTAL` 锚点，**禁**写死「257」 | ✅ | `scripts/verify_test_counts.sh` 实测：锚点 `CTEST_TOTAL: 257` == `ctest -N` 的 257 ⇒ ⇒ **一致**（退役 −2 与新增 +2 相抵）⇒ ⇒ 护栏 PASS |
| 缩面须 reviewer **显式签字** | ✅ | 用户 2026-11-11 批准（对话记录）· 四个锚点均带「显式批准的缩面」标注 · `docs/CHANGELOG.md` 有「⚠️ **这是显式批准的覆盖缩面**（用户 2026-11-11 批准）」段 |

---

## 3. 判别力复核（**变异** —— 「还原缺陷形态 ⇒ 判据 MUST 转红 ⇒ 还原修复 ⇒ 复绿」）

> ⚠️ **方法学修正（如实登记）**：第一轮变异脚本（v1）的 M2/M3 用**手打字符串**做变异 ⇒ ⇒
> bash 在 heredoc 里解释了反引号 / `||` ⇒ ⇒ 变异串**可能**不是**逐字原始写法** ⇒ ⇒
> 违反「变异 MUST 用**原始写法**」纪律 ⇒ ⇒ **作废 v1 的 M2/M3**，重跑 **v2**：
> **变异行直接从 `git show 67926cf~1:<file>` 取**（原始源码 = 权威）⇒ ⇒ 零手打。

| 变异 | 变异来源 | 转红证据 | 还原复绿 | 判定 |
|---|---|---|---|---|
| **M1′** ⑥ 关键字表 → 原始 31 词表 | `git show 67926cf~1:include/codegen_naming.h` 的整张表 | **3 failed** | 145 断言 / 6 用例 PASS | ✅ 判别力成立 |
| **M2′** ⑧ SLTU → 原始 `bits<31, 31>(sub_result);  // 简化实现` | `git show …:rv32i_alu.h` **逐字行** | **3 failed** | 45 断言 / 4 用例 PASS | ✅ 判别力成立 |
| **M3′** F-A → 原始 `if (parent_path.empty() \|\| parent_path == "unnamed") {` | `git show …:component.h` **逐字行** | **1 failed** | 27 断言 / 4 用例 PASS | ✅ 判别力成立 |
| **M4** C8 → 移除 tie-off | 删 `rv32i_pipeline.h:530` 一行 | **顶层 input 7 → 8** 且 `top_pipeline_hazard_unit_unnamed_input_4` **回到顶层端口表** | 顶层 input **回到 7** 且该端口**再次不在**端口表 | ✅ 判别力成立 |

**纪律自检**：
- ① 每条变异 MUST **先确认仍能编译** ⇒ ⇒ 4/4 「✅ 编译成功」（编译失败 ⇒ 跑的是旧二进制 ⇒ 假绿）；
- ② 「还原源码」≠「复原」（二进制也是状态）⇒ ⇒ 每条变异**还原后 MUST 重建** ⇒ ⇒ 4/4 复绿 PASS；
- ③ 变异 MUST 用**原始写法** ⇒ ⇒ v2 的 M1′/M2′/M3′ **逐字取自 git 原始源码**（M4 = 删除一行 ⇒ 无写法歧义）；
- ④ **无效变异 MUST 如实登记** ⇒ ⇒ **无无效变异**（0 条）；
- ⑤ **变异后工作区状态**：全部还原后 `git status` 的 `include/**` 为**空** ⇒ ⇒ 四处实现**逐字复位**（实测：246 词 / `operand_a < operand_b` / `parent_path.empty()` / tie-off ×1）。

---

## 4. 与「两份 openspec change」的交叉（**G2**）

| change | AC 数 | 判定 | 证据 |
|---|---|---|---|
| `node-name-prefix-path-contract-2026-11` | **8 / 8** ✅ | 全部满足 | ① delta 与 `.rddf/checks/decision-request-port-naming-parity-2026-11.md` §2 **R1-b** 围栏**逐字一致**（脚本比对 = True）② `## ADDED Requirements` ✓ ③ `verify_openspec_delta.sh` **PASS** ④ `openspec validate --specs`（非 strict）**44 passed / 0 failed** ⑤ `component.h:293` 条件已删 + `lnodeimpl.cpp:33-35` 不剥离 ⇒ 同口径 ⑥ 既有 `component-ownership-and-naming/spec.md:62`（「其 Verilog 名 MUST 含该组件的路径段」）**未被触碰**（`git log` 最后修改者更早）⑦ 公开面登记已落 + **代价明写** ⑧ 本 change 的 delta **不含** `rtl-hierarchical-output`（grep = 0） |
| `port-naming-member-parity-contract-2026-11` | **7 / 7** ✅ | 全部满足 | ① delta 与 §2 **R1-a** 围栏**逐字一致**（脚本比对 = True）② `## ADDED Requirements` × 1 / `## MODIFIED Requirements` × **0** ③ `verify_openspec_delta.sh` **PASS** ④ `validate --specs` **44 / 0** ⑤ 该 change 的提交 `ea7de54` 的 `--stat` = **6 个文件全在 `openspec/**`** ⇒ ⇒ **不含** `src/**` 与 `include/**` ⑥ 主 spec `rtl-hierarchical-output/spec.md:309-316` 的既有 Scenario **未被触碰**（最后修改者 `aed393b` 早于 `ea7de54`）⑦ 本 change **不含** `component-ownership-and-naming` 那条 Requirement |

⇒ ⇒ **两份 change 的实施体均已在 B0 内完成**（`node-name-prefix-path-contract` 的实施体 = F-A；`port-naming-member-parity-contract` = **纯契约 change**，零代码面）⇒ ⇒ **AC 可勾** ⇒ ⇒ 归档可行。

---

## 5. 遗留清扫复核（Step 5）

**范围** = B0 两笔提交改动的全部 `.h` / `.cpp`（`git diff --name-only 67926cf~1 bd21b45`）。

| 项 | 结果 | 证据 |
|---|---|---|
| TODO / FIXME / XXX / HACK | ✅ **无新增** | 唯一命中 = `tests/chlib/test_phase4_integration.cpp:167` 的「添加实际性能测量（等待 `Rv32iPipelineWithCache` 重建）」⇒ ⇒ **存量**（与 B0 无关）⇒ 按「范围外历史遗留只记录、不趁机清理」**不碰** |
| 注释掉的代码块 / 临时调试日志 | ✅ **无** | 全范围扫 `// std::cout` / `printf("DEBUG` / 注释掉的 `if`/`for`/`return` ⇒ 0 命中 |
| 被 `SKIP()` 的测试 | ✅ **无新增** | `test_phase4_integration.cpp` 改前 **6** / 改后 **6**；`test_rv32i_pipeline.cpp` 改前 **3** / 改后 **3** ⇒ ⇒ **数量未变** ⇒ ⇒ 全部为**存量**（指向未实现的 `Rv32iPipelineWithCache` / `Rv32iCore`）⇒ **不碰**；且 `:162` 的 SKIP 措辞「`ForwardingMux (riscv)` is a non-existent class」**仍然属实**（该类从未存在；B0 退役的是 `ForwardingUnit`）⇒ ⇒ **不陈旧** |
| `#if 0` / 未替换的 mock | ✅ **无** | 0 命中 |
| 孤儿文件（`.cpp` 未注册 CMake / 头无人 include） | ✅ **无** | `tests/CMakeLists.txt` 的两处退役注册块已清理；新建的 2 个测试已注册（`:1081` / `:1194`）⇒ ⇒ `verify_test_counts.sh` **PASS** |

⇒ ⇒ **本次范围内遗留清扫 = 0 处未处理项**；存量 4 处（3 SKIP + 1 TODO）**如实登记**，范围外不清理。

---

## 6. 结论

### 6.1 判定

**B0 批六项 —— ✅ 验收通过。**

- 验证顺序 **1–5 步全绿**（编译 0 warning · ctest **257/257** · examples **28/28** 真跑 · 边界覆盖到位 · 遗留清扫 0 处未处理）
- 六项 AC **逐条满足**，每条**至少 1 条工具证据**（0 个 0-工具调用的「假阳性 pass」）
- **判别力 4/4 成立**（v2 变异逐字取自 git 原始源码；M4 双向实测 7↔8）
- **9 条护栏全绿**
- **缩面已显式批准**且四个锚点同步（`257` / `76` / `65` / `284`）

### 6.2 顺带收口（G2）

- 两份 openspec change 的 AC **8/8 + 7/7 全部满足** ⇒ ⇒ **可勾选 + 可归档**（归档动作属状态推进 ⇒ ⇒ **由 planner 角色执行**，不属验收）。
- ⚠️ **归档后主 spec 会新增两条 Requirement**：`component-ownership-and-naming`（组件路径 / 节点名前缀拼接口径）· `rtl-hierarchical-output`（成员名 / 端口名 parity 契约）⇒ ⇒ ⇒ **B1 的 N4「规范落点」由这条 change 承载** ⇒ ⇒ ⇒ **N4 的实施体 = 归档这两份 change**（不是另写一份）。

### 6.3 遗留与登记（交用户 / 交其它角色）

| # | 项 | 归属 |
|---|---|---|
| ① | `docs/architecture/rv32i-microarchitecture.md` + 三份 ADR 的 2026-11-11 更新注 —— **architect 域**，**已改未提交**（4 个文件 + 1 份新审稿记录 `.rddf/reviews/rv32i-microarchitecture-backfill-2026-11.md`） | architect ⇒ **待提交** |
| ② | `include/cpu/AGENTS.md` 的头数（30→**27**）· NAMESPACE MAP（13→**10**）· STRUCTURE 行数刷新 | **builder 域**（非 `arch_paths`）⇒ 派单 |
| ③ | `chlib::ForwardingUnit` 组件类零实例化 ⇒ 按 C6 三级准则裁决（接入 / 退役 / 登记定位） | **独立立项**（backlog） |
| ④ | `.rddf/roadmap/**` 三处仍列 `riscv::ForwardingUnit`（`phase-5.md:112` · `objective-riscv-mini-convergence.md:25,101` · `feat-riscv-mini-convergence.md:17`） | **planner 域** ⇒ backlog（不阻断） |
| ⑤ | 3 处存量 SKIP + 1 处存量 TODO（`Rv32iPipelineWithCache` / `Rv32iCore`） | **范围外** ⇒ 只登记，不清理 |

### 6.4 已用轮次

| reviewer | 预算 | 已用 | 说明 |
|---|---|---|---|
| **verifier（本地验收）** | 3 轮 | **1** | 本报告；失败项 0 ⇒ ⇒ 无需第 2 轮 |
| **critic** | 2 轮 | **0** | 本批**未触发**（纯执行改动 · 无公开 API 契约面变更 · 用户未要求）⇒ ⇒ **不是**跳过，是**不适用** |
| **oracle** | 1 轮 | **1** | 属 architect 域的文档回填（`rv32i-microarchitecture-backfill-2026-11.md`）⇒ ⇒ **不属本 change** |
