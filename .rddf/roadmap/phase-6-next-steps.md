# Phase 6 后续任务清单 + 实施路径（planner 域 · 2026-11-09）

> **触发**：用户口令「然后切 planner 对后续还有哪些任务内容都梳理一遍，再给出后续任务内容和
> 实施路径」。
> **上游**：`docs/architecture/2026-11-09-phase-6-midterm-review.md`（architect 中期复盘 ·
> 三项实测发现 + ADR-066 **v1.4** / ADR-060 **v1.21** 事实订正 · oracle `PASS-WITH-NOTES` +
> critic `APPROVE_WITH_CONDITIONS`（**0 BLOCKER**））。
> **窗口唯一真相** = `.rddf/roadmap/phases/phase-6.md`（**任务状态以 phase-6 为准**；本文件是
> **任务内容 + 实施路径**的载体，**不复制** phase-6 的状态格）。
> **本文件不新增任何架构义务** —— 所有判据均映射**既有** AC1–AC10 / phase-6 出口判据。

---

## 1. 后续任务**总览**（**按可执行性排序** · 不按编号）

⚠️ **「可并行」的**真实边界**（metis 订正 · 只覆盖 `src/**` **写点** ⇒ ⇒ **收口阶段不并行**）：
三条轨共享三个**单点资源** ⇒ ⇒ ① `openspec/specs/rtl-hierarchical-output/spec.md`
（T1 与 T3 **都改**这一个文件）② 根 `AGENTS.md` 的 **4 个计数锚点**（各 **2 处** ⇒ ⇒ 文本必冲突）
③ `.rddf/checks/public-api-*`（T4 与 T1 都动 `EXPECTED_REMOVAL_ROWS` / `EXPECTED_PROBES`）
⇒ ⇒ ★ **锚点编辑权 = 最后合入者** ⇒ ⇒ 合入前 MUST **全量重建并重算**（否则两条轨各自 +K ⇒ ⇒
**错的最终值**）⇒ ⇒ ⚠️ **护栏清单**（T1 的 S6″）MUST 含 `verify_dual_backend_parity.sh`。
⇒ ⇒ ★ **建议实施序列**（避免上述冲突）：**T1 先行**（它动 spec + 锚点 + 登记表）⇒ ⇒ T3 / T4 在
T1 **归档后**接手 ⇒ ⇒ ⇒ T5（P6-0 只读侦察）**可全程并行**。

| 轨 | 任务 | 归属 | 前置 | 阻塞关系 |
|---|---|---|---|---|
| **T1** | **P5 收尾**（change `mem-port-arg-cross-layer-2026-11` · AC1–AC10） | builder | — | ★ **主线**（唯一在途 change） |
| **T2** | **P5 文档订正的同步**（phase-6 P5 行 + change 文档） | planner | — | ⚠️ **部分完成**（见 §4 的「本轮**未做完**」清单） |
| **T3** | **P7 builder 步**（三档 `PortPolicy` 语义分化） | builder | — | 与 T1 **可并行**（写点函数级不重叠） |
| **T4** | **P8 builder 步**（删 `set_ab_verification` stub + golden 覆盖下限） | builder | — | 与 T1 / T3 **可并行**（写点不重叠） |
| **T5** | **P6 侦察 + 立项**（RV32I ISA 缺口：CSR / `ECALL`·`EBREAK` / `FENCE`） | planner → builder | — | 与 T1–T4 **可并行**（P6-0 是**只读侦察**） |
| **T6** | **P10 的 ADR 判据订正**（ADR-052 / ADR-063 / ADR-067 的 CLI 版本判据） | architect | 独立 | ✅ **2026-11-10 已做**（**ADR-052 v2 / ADR-063 v2 / ADR-067 v2** + **ADR-066 v1.5**）⇒ ⇒ 剩余待办 = **improvement 草案同步**（planner 域） |
| **T7** | **P11**（ADR-036 逻辑算子物理移除） | builder | — | 🔒 **日历锁**（最早 **2027-04-17**）⇒ 本窗口**不做** |
| **T8** | **P9 遗留**（真 FIFO 写缓冲 / 真预取 / `miss` 期间 `pc` 稳定性核对） | 独立立项 | — | 明确**不在**本窗口 ⇒ ⇒ 需**新 improvement** |

---

## 2. T1 = P5 收尾（**主线** · 详细实施路径）

### 2.1 起点状态（**已实测**）

| 项 | 状态 |
|---|---|
| S2 判别程序（三桶 + 归属比对） | ✅ **已提交**（`364e213`）· build 通过 · `ctest -R hier` **4/4 绿** |
| `(ii)` 放行 | ✅ **实测生效**（产物含父侧 `logic` 声明 + 子 output `.port(net)` + mem 端口读该网 ⇒ **命名恒等**） |
| ⚠️ **AC1–AC10** | ❌ **零验证** ⇒ change **未完成**、**未归档** |

### 2.2 实施路径（**每步一个可判据**）

| 步 | 内容 | 判据（机械） | 依赖 |
|---|---|---|---|
| **S-1** | **提交本轮 planner/architect 工件**（ADR-066 v1.4 / ADR-060 v1.21 / 审稿记录 §10 / 中期复盘 / 本文件 / change 文档订正）⇒ ⇒ 并把该 **commit 记为基线** ⇒ ⇒ ⚠️ 否则 S6″ 的「零回归」**无法界定改前改后** | `git log --oneline -1` 记录基线 commit | — |
| **S0** | **基线**：`ctest -LE perf` 全绿 + 记录 **4 个**锚点（`CTEST_TOTAL` / `VERILATOR_E2E_TOTAL` / `DUAL_BACKEND_PARITY_TOTAL` / `DUAL_BACKEND_PARITY_PAIRS`）⇒ ⇒ ⚠️ metis：锚点 MUST 是 **4 个**（不是 2 个）且 MUST 写**期望 delta**（`k=0` 处也 MUST 显式写 `+0`）⇒ ⇒ 否则「全绿」检不出**静默缩面** | 4 个锚点 + `verify_test_counts.sh` + `verify_dual_backend_parity.sh` | S-1 |
| **S1** | **V20 探针**（字面 (i) 现状登记）⇒ ⚠️ **预期值 = 已响亮拒绝**（⚠️ 已订正 · ADR-066 v1.4 §2.7）⇒ ★ 交付物 = **(i)/(ii) 归因分开** | 探针输出**显式 PASS/FAIL** + 台账行 20 + 登记表归因描述 | S0 |
| **S2′** | **代码注释订正**（`src/hierarchy_analysis.cpp:277-279` 的独立扫描注释仍写「字面 (i) 不产生跨 module 边 ⇒ 闸门不可达」⇒ ⇒ **与 v1.4 矛盾** ⇒ ⇒ MUST 改）⇒ ⇒ ⚠️ 判据 MUST **两面**（否则「删块也能绿」） | 负：`grep -c "不产生任何跨 module 边" src/hierarchy_analysis.cpp` == **0** · **正**：`grep -c "含 mem 本体" src/hierarchy_analysis.cpp` ≥ **1** | — |
| **S3′** | **AC4 声明断言**（产物内 mem 实参引用的名字 MUST 在父 module 有 `logic` 声明 ⇒ 非 implicit wire）⇒ ⇒ ⚠️ metis：**产物级断言 MUST 有载体** ⇒ ⇒ **载体** = `tests/test_hierarchy_verilog.cpp` 新增 CASE（`toVerilogHierarchical` 写临时目录 + 读回文本）⇒ ⇒ **或**扩 `tests/hier_rtl_products.cpp` + 在 `scripts/verify_hierarchical_rtl.sh` 加 **R9′** | 产物级断言（**载体已指名**） | S1 |
| **S3″** | **AC3 值判据**（读 `addr` 与写 `wdata`/`en` **两侧** · **精确期望值** · **手推**）⇒ ⚠️ 「非 0」只是下界 | 值判据 | S3′ |
| **S4** | **AC5 `en` 槽 fail-closed**（读 `:1294-1297` + 写 `:1368-1371` ⇒ **两处共用 helper** ⇒ 免漂移） | 诊断断言（`enable != nullptr` 且无名 ⇒ 报错 ⇒ **MUST NOT** 回退 `1'b1`） | S3″ |
| **S4′** | **AC6 归因订正**（被拒诊断 MUST NOT 含「把 mem 与使用者放同层」—— 当 mem 本体**已在**同层时）⇒ ⚠️ 反向：(i) 支**仍合法** ⇒ ⇒ **MUST NOT** 一刀切删除 | 诊断文本断言（**语义要点**，非整串） | S4 |
| **S5** | **AC7 变异**（不登记落点网 ⇒ 值判据转红）⇒ ⚠️ MUST **先确认仍能编译**（否则跑旧二进制 ⇒ 假绿）⇒ ⇒ **还原后 MUST 重建** | 变异转红 + 还原复绿 | S4′ |
| **S5′** | **AC8 边界清单**（孙组件仍拒 / 父层自身节点不误报 / 读写不同侧 / 多个 `ch_mem` / `aread`+`sread` 两类 / lint 绿 ≠ 值对）⇒ ⇒ ⚠️ metis **补两条**：① **新 DUT MUST 登记**进 `tests/hier_corpus_registry.md` + 新增结构类（root 持 `ch_mem` + mem 端口实参跨层）⇒ ⇒ 否则 `verify_hier_corpus_coverage.sh` 三向核对**转红**；② **决策 ⑤ 共网用例**（同一子 `ch_out` **既**直通父 output **又**作 `(ii)` 实参 ⇒ ⇒ 断言产物里**只有一根** `logic` 网） | 各有用例 + 语料登记 | S5 |
| **S6** | **AC9 spec delta**（capability `rtl-hierarchical-output` · **禁** `--skip-specs`）+ **归档** | `verify_openspec_delta.sh` 绿 | S5′ |
| **S6′** | **AC10 台账行 20 闭口**（`已裁决 → 已收口` · `归属 → —` · 闭口证据 = archive 路径（含 `proposal.md`）· 登记表归因描述同步） | `verify_arch_ledger_residuals.sh` 绿 | S6 |
| **S6″** | **零回归 + 锚点**：`ctest -LE perf` 全绿 · `verify_test_counts.sh` 绿 · `verify_hierarchical_rtl.sh`（含 `--self-test`）绿 ⇒ ⇒ ⚠️ metis **补**：**4 个锚点 + 期望 delta**（`CTEST_TOTAL 255 + K` · `VERILATOR_E2E_TOTAL 77 + k1` · `DUAL_BACKEND_PARITY_TOTAL 66 + k2` · `DUAL_BACKEND_PARITY_PAIRS 289 + k3`，`k=0` 也 MUST 显式写）· **护栏清单** MUST 含 `verify_hierarchical_port_naming.sh` · `verify_hier_corpus_coverage.sh` · `verify_dual_backend_parity.sh` | 全部绿 + delta 对齐 | S6′ |

### 2.3 实施纪律（**指针**，不复制）

⚠️ 变异纪律（还原 ≠ 复原 · 先编译确认 · 还原后重建 · 无效变异如实登记）= 根 `AGENTS.md`
**ANTI-PATTERNS** 的「变异（mutation）纪律」4 条 ⇒ ⇒ **不在此复制**。
⚠️ 本轮**一次性**结论（(i) 的现状 = 已响亮拒绝）= **ADR-066 v1.4** ⇒ ⇒ 不在此复制。
⚠️ (i) 既有覆盖的定位 = `tests/test_hierarchy_analysis.cpp:666` ⇒ ⇒ 不在此复制（但见 §2.2 的判别力订正）。

## 3. T3 / T4 / T5 = 其余三条轨（**可并行**）

### 3.1 T3 = P7 builder 步（三档 `PortPolicy` 语义分化）

**architect 步已闭**（ADR-060 **v1.18**）⇒ ⇒ 剩余 ②③④⑤：

| 子步 | 内容 | 判据 |
|---|---|---|
| ② | **同一可合成样本**上三档**两两可区分** ⇒ ⇒ 样本**四约束**：矩阵内可合成 · **N ≥ 2** 条可合成边 · **含端口 + 网** · **无悬空 `ch_in`** | 元组 `(产物是否存在, 诊断列出的边数, 产物注释是否存在)`：`Warn = (true, N, 有)` · `Fatal = (false, N, —)` · `Reject = (false, 1, —)` · **N ≥ 2** ⇒ ⇒ ⚠️ metis **补两条（否则不可判）**：① **提取通道** = `Warn` 的 N 从 **重定向 `std::cerr`** 数 `CHWARN` 行（logger 无 sink）· `Fatal`/`Reject` 的 N 从 `result.error` 解析（`(\d+) cross-module edge` / `\n  [` 计数）；② ⚠️ **Warn 侧的「产物是否存在」今天恒 `false`**（`:952-991` 对所有 policy 都 `return result` ⇒ Warn ≡ Fatal ⇒ **这正是 P7 的病灶**）⇒ ⇒ **施序 = 先修实现再判** ⇒ ⇒ 改前改后都是 `(false,·,·)` ⇒ ⇒ 三档不可能区分 |
| ③ | **行为点** = 失败路径（policy 分支）+ **成功路径**（`synthesized_ports` 声明/连接发射点 ⇒ CHWARN + 产物注释）⇒ ⇒ ⚠️ metis **纠偏**：`port_policy` **只在 `HierarchyEmitOptions`**（`include/hierarchy_verilog.h:28`）可见 ⇒ ⇒ codegen 拿不到 ⇒ ⇒ **MUST NOT** 给 `ModuleEmitter` 加 `HierarchyEmitOptions` 依赖（⇒ ⇒ 会与 P5 的 AC5 改动**同文件同区域冲突**）⇒ ⇒ 正确做法 = **策略读取留在 `hierarchy_verilog.cpp`**（`:1262` 附近）· 注释文本随 `ModuleView` 传给 codegen（codegen **只打印**） | 成功路径**每条被合成的边一条 CHWARN**（现状**零**） |
| ④ | 文档/注释义务落地 + **台账行 15 闭口**（`已知 → 已收口` · `归属 → —` · `闭口证据` = archive 路径 · `复核条件` 清空） | 登记表绿 |
| ⑤ | **计数分别判**（`// CppHDL-synth:` 的 `implicit port` / `net` **分别**计数 ⇒ ⇒ 1 边 ⇒ k 端口 + 1 网 ⇒ 「计数一致」**不可满足**） | 两个计数分别判 |

⚠️ **失败档用例 MUST 先删残留产物**（否则读到上次的文件 ⇒ 假绿）。

### 3.2 T4 = P8 builder 步（删 stub）

**architect 步已闭**（ADR-035 **v2.2** · 路线 **A = 删 stub**）⇒ ⇒ 剩余 ①②④：

| 子步 | 内容 | 判据 |
|---|---|---|
| ① | **stub 删除**：`set_ab_verification` / `is_ab_verification` / `ab_verification_` / 每 tick WARN + **两个局部量**（`ab_result_a` / `run_ab_verify`）⇒ ⇒ ⚠️ **公开 API 移除 MUST 走 P14 机制**（`public-api-surface.md` 移除章 + `public-api-removals.tsv` 探针行 + `docs/CHANGELOG.md` 一行） | `verify_public_api_visibility.sh` 绿 |
| ② | **golden 覆盖下限** ⇒ ⇒ ⚠️ metis：**分子/分母 MUST 派生**（不是硬编码 `5 / 39` ⇒ ⇒ 否则漂移无人发现）：分母 = `include/jit/jit_ir.h` 的 `JitOp` 枚举行数 · 分子 = `grep -o "JitOp::[A-Z_]*" tests/jit/test_jit_golden.cpp \| sort -u \| wc -l` ⇒ ⇒ 判据 = 分子 ≥ 锚点 **且** 分子 ⊆ 分母 ⇒ ⇒ ⚠️ 「一个 TEST_CASE ↔ 一个 JitOp」是**约定**（非事实）⇒ ⇒ MUST 写成**契约**；⇒ ⇒ `CALL_EXTERNAL` / `NOP` **永不该有** golden ⇒ ⇒ **100% 不可达** ⇒ ⇒ 新增 `JitOp` MUST 附 golden **或** 在 **新建** `.rddf/checks/jit-golden-coverage.md` 登记未覆盖理由（⚠️ 该文件**当前不存在** ⇒ ⇒ 悬空指针 ⇒ MUST 建）⇒ ⇒ **护栏** = 新建 `scripts/verify_jit_golden_coverage.sh` |
| ④ | **文案清理**：根 `AGENTS.md` KNOWN LIMITATIONS + `docs/usage_guide/10-verilator-backend.md:133` ⇒ ⇒ ⚠️ 根 `AGENTS.md:313` 的「OFF 时不存在」是**事实错误**（`__has_include` 对**源树物理头恒真**）⇒ ⇒ **替换**（不是纯删）⇒ ⇒ ADR-035 v2.2 已给**逐字建议文本** | 文档面 **0** 命中 |

### 3.3 T5 = P6 侦察 + 立项（RV32I ISA 缺口）

**P6-0 侦察**（只读 · 先于任何实现）：

| 内容 | 依据 |
|---|---|
| **复用面清单**（复用 / 新增）—— ⚠️ `CsrBank`（`include/cpu/riscv/rv32i_csr.h`）· `ExceptionUnit`（`rv32i_exception.h`）**已存在** ⇒ ⇒ 缺口 = **指令级集成**，**非**从零实现 | 实测 |
| **`FENCE` 裁决**（单 hart + in-order + 无写缓冲 ⇒ 可否实现为 NOP）⇒ ⇒ 落进新 capability 的 spec | phase-6 P6 出口 ① |

**立项后的判据**（phase-6 P6 出口 ②）：新 ctest target · **逐条手推期望值**（禁只抄仿真输出 ——
两后端同源 ⇒ parity 对该族缺陷**结构性失明**）· `CTEST_TOTAL` 锚点同步 ·
`OP_SYSTEM` 的**剥注释后**引用数从 0 变正（与 `verify_public_api_visibility.sh` 的 tsv 探针**同口径**
⇒ ⇒ 禁裸 `grep -c`）**且**命中落在 `rv32i_decoder.h` **与** `id_stage.h` / `ex_stage.h` / `mem_stage.h`
**任一**（⇒ ⇒ 排除「只多一行注释」）。

---

## 4. T2 = planner 域订正的**证据索引**（**不是**过程日志）

> **唯一的真相** = `git diff`（**未提交** ⇒ ⇒ 本节只给**可 grep 的落点**，不复制 diff 内容）。
> ⚠️ 本节**不**承担「已完成」的证明作用 ⇒ ⇒ **完成度**看 `openspec/changes/.../tasks.md` 的
> checkbox（**那才是** change 的权威）。

### 4.1 已订正的落点（**可 grep**）

| # | 落点（可 grep） | 订正内容 |
|---|---|---|
| ① | `grep -c "实测订正" .rddf/roadmap/phases/phase-6.md` | P5 行 4 处（判据 ② / ③⑤ / ⑥ + 「已裁定」） |
| ② | `grep -c "实测订正" openspec/changes/mem-port-arg-cross-layer-2026-11/proposal.md` | What Changes #2 / #3 / #6 |
| ③ | `grep -c "实测订正" openspec/changes/mem-port-arg-cross-layer-2026-11/design.md` | §1 现状表 + 「关键」段 + §2.1 注释 |
| ④ | `grep -c "实测订正\|metis 订正" openspec/changes/mem-port-arg-cross-layer-2026-11/tasks.md` | S1 / S2 / S2′ / S3 / S4 |
| ⑤ | `grep -c "v1.4" docs/adr/ADR-066-hierarchical-parent-side-landing-semantics.md` | ADR-066 **v1.4** 事实订正 |
| ⑥ | `grep -c "v1.21" docs/adr/ADR-060-hierarchical-port-policy-and-module-reuse.md` | ADR-060 **v1.21** 事实订正 |

### 4.2 ⚠️ 本轮**未做完**的（**如实登记**）

| # | 未做完的 | 归属 | 为什么没做完 |
|---|---|---|---|
| ① | `proposal.md` 头部「架构权威」仍写 ADR-066 **v1.2** / **v1.3** + ADR-060 **v1.17** / **v1.20** ⇒ ⇒ **未**指向 **v1.4** / **v1.21** | planner | ⚠️ **本轮漏项** ⇒ ⇒ MUST 补 |
| ② | `design.md` 头部「权威」同 ① | planner | ⚠️ **本轮漏项** ⇒ ⇒ MUST 补 |
| ③ | `phase-6.md` 的 **P4 行证据落点**仍写 v1.2 / v1.17（**历史事实**，**不是**错）⇒ ⇒ 但 **P4 的判据已由 v1.4 订正** ⇒ ⇒ 可加一行「**后续由 v1.4 订正**」 | planner | 判据留存，不是错 ⇒ ⇒ 可选 |
| ④ | `src/hierarchy_analysis.cpp:277-279` 的**错误注释**仍在（**builder 域** ⇒ ⇒ planner MUST NOT 改 `src/**`）⇒ ⇒ 归 T1 的 **S2′** | builder | 角色边界 |
| ⑤ | `openspec/specs/rtl-hierarchical-output/spec.md` 的 **P7 相关 Requirement**（`grep -n PortPolicy` 命中 `:131-138` / `:371`）与 T1 的 spec delta **冲突面**未评估 | planner | T3 未开工 ⇒ ⇒ 冲突面在 T3 立项时评估 |

## 5. ⚠️ **须用户裁决 / 新预算**的事项

| # | 事项 | 说明 |
|---|---|---|
| ① | **T6（P10 的 ADR 判据订正）需要新的 architect 评审预算** | ADR-052 / ADR-063 / ADR-067 仍写「`openspec validate --specs --strict` 绿（**CLI 须 1.4.1**）」⇒ ⇒ 实测 CLI = **1.14.3** · 仓级 **红** ⇒ ⇒ 那是 **Accepted ADR 的判据** ⇒ ⇒ MUST 走 **ADR 修订流程**（版本 bump + 修订行 + **oracle → critic** 双审）⇒ ⇒ 本轮**未**做（预算已用于 v1.4 / v1.21） |
| ② | **T8（P9 遗留）需新 improvement（+ 可能需 architect 裁定）** | 真 FIFO 写缓冲 · 真预取 · `miss` 期间 `pc` 稳定性核对 ⇒ ⇒ **明确不在**本窗口 ⇒ ⇒ 需 planner 另起 improvement（走 `add-improve` / HARD-GATE）⇒ ⇒ ⚠️ 若新实现需要**新的架构取舍**（如 wrapper `ready` 背压契约 / 预取语义）⇒ ⇒ **另需 architect 裁定** ⇒ ⇒ **未**在本轮起草（避免与 T1 并行制造文档债） |
| ③ | **T5 的 `FENCE` 裁决 + 新 capability 立项需 architect 预算** | ⚠️ metis **边界纠偏**：`FENCE` 的**语义裁决**（单 hart + in-order + 无写缓冲 ⇒ 可否为 NOP）= **ISA 语义** ⇒ ⇒ **planner 无权拍板** ⇒ ⇒ MUST 走 **architect 裁定** ⇒ ⇒ **新的 architect 评审预算**（ADR-066 v1.4 / ADR-060 v1.21 已用完本轮预算）⇒ ⇒ P6-0 **只做侦察**（复用面清单 + 候选方案），**不**落裁决 |
| ④ | **T1 的走向：planner 直接定 =「继续」** | ⚠️ metis **纠偏**：把决定权推回用户 = **自设阻塞门**（用户还得再开一轮才能开工）⇒ ⇒ planner **直接定**：**继续**（S2 已实测正确 ⇒ ⇒ 回退重做会**丢弃**已完成的工作且**无正确性收益**）⇒ ⇒ 「继续」的**代价** = 补齐 AC（不补 ⇒ 归档 = 假绿）⇒ ⇒ **代价已由 §2.2 的判据清单显式化** ⇒ ⇒ 用户只需在 §5 的 ①②③ 上拍板 |

---

## 6. 判据自检（**可复跑** · 只留**不会随无关改动转红**的）

```bash
# ① 订正已就位（计数）
grep -c "实测订正" .rddf/roadmap/phases/phase-6.md
grep -c "实测订正" openspec/changes/mem-port-arg-cross-layer-2026-11/{proposal.md,design.md}
# ② change 文档的 delta 自检
bash scripts/verify_openspec_delta.sh
# ③ roadmap 护栏
bash scripts/verify_roadmap_fragments.sh
```

⚠️ **删除的自检**（metis）：原「grep `不可达|静默丢端口`」⇒ ⇒ 该判据**会随无关改动转红**
（命中面含 `src/**` ⇒ ⇒ 超出 planner 域）⇒ ⇒ **不是**判据 ⇒ ⇒ 已删。
⚠️ **本文件的**位置保护（metis）：本文件**故意**在 `.rddf/roadmap/` **根**（**不**在 `phases/`）
⇒ 因为 `scripts/verify_roadmap_fragments.sh` 的 `parse_front_matter` 对「不以 `---` 开头」
**直接 raise** ⇒ ⇒ 任何人把它移进 `phases/` ⇒ ⇒ 护栏**立刻 fail-closed** ⇒ ⇒
⚠️ **MUST NOT** 移入 `.rddf/roadmap/phases/`。
