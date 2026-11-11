# 审稿记录：RV32I 微架构主题文档回填 + 三份 ADR 的 2026-11-11 更新注

- **审稿对象**：`docs/architecture/rv32i-microarchitecture.md`（**主题文档**，非 ADR）
  + `docs/adr/ADR-038-forwarding-namespace-orphan.md` · `docs/adr/ADR-046-rv32i-canonical-home.md`
  + `docs/adr/ADR-047-rv32i-data-forwarding.md`（**仅加更新注 / 事实订正**，**未改任何裁决**）
- **触发**：B0 批 C5 + C6 同批退役（`riscv::ForwardingUnit` / `riscv::HazardDetectionUnit` /
  `riscv::HazardUnitComplete` 物理删除）⇒ ⇒ 主题文档结构树仍列三个已删头 ⇒ ⇒ **architect 域回填**
- **审稿顺序**：oracle（**唯一** reviewer —— 按角色矩阵「只同步主题文档 / 改措辞 → 只 oracle」）
- **档位**：`review_profile: arch`
- **预算**：oracle **1 轮**（已用 1）· critic **0 轮**（**未触发**：未新建 ADR · 未反转既有决策 ·
  未改任何裁决正文 ⇒ ⇒ 按角色矩阵**不需** critic）

---

## 1. oracle 第 1 轮

- **判定**：`治理面判定: PASS-WITH-NOTES` · **0 BLOCKER** · 9 条建议（4 条 = 本次回填**自身的事实错误**
  ⇒ ⇒ 必须修；3 条 = ADR 同步；2 条 = 派单其它域）

### 1.1 逐条处置（**全部采纳**）

| # | 级别 | finding | 处置 |
|---|---|---|---|
| ① | **本次错误** | **ADR-038 归因错 + 结论错**：§2.3 的标题是「Don't merge `forwarding.h` and `riscv::HazardUnit`」⇒ ⇒ 合并对象 = `chlib::ForwardingUnit` + `riscv::HazardUnit` —— **两者都还在** ⇒ ⇒ §2.3 / §5 **未失效** ⇒ ⇒ 真正被消解的是 **ADR-046 D6**（「两个 `ForwardingUnit` 并存，靠 namespace 区分」）⇒ ⇒ 且原行**自相矛盾**（既说「裁决失效」又说「先读它」） | **改** ⇒ ⇒ 残留表该行改写为：消解对象 = **ADR-046 D6**；**明确写**「ADR-038 §2.3 / §5 的裁决本身仍有效（合并对象两者都还在）」；失效的**只是** ADR-038 的 **2026-10-27 更新注**（其前提「ADR-047 不动 `riscv::ForwardingUnit`」已不成立） |
| ② | **本次遗漏** | `:11` 「最后同步：2026-10-27」未 bump | **改** ⇒ ⇒ 改为「2026-11-11（随 B0 批 C5+C6 退役回填）」 |
| ③ | **本次遗漏** | 「唯一的数据 mux 实现」过强：同头内 `chlib::forward_select<>`（`include/chlib/forwarding.h:353-376`）是**同一逻辑的函数式副本**（该头 `:343-345` 自陈「同一份逻辑的两处实现」）⇒ ⇒ 而 `rv32i-dead-component-consolidation-2026-11.md:275` 已把「同一语义 MUST NOT 有两份实现」判为**永远为红** | **改** ⇒ ⇒ 全文改为「**流水线侧唯一的数据 mux 组件**」+ 加注函数式副本的存在（`include/chlib/forwarding.h:353-376`）⇒ ⇒ **不说**「唯一实现」 |
| ④ | **本次遗漏** | `chlib::ForwardingUnit` 组件类**零实例化**（全仓 `ForwardingUnit<` 唯一命中 = `include/chlib/forwarding.h:59` 的**文档注释**；`tests/chlib/test_forwarding.cpp` 用的是**函数式** `forward_detect<>` / `forward_select<>`）⇒ ⇒ 原行标「已消解」**不完整** | **改** ⇒ ⇒ 残留表**新增一行**独立登记（仓内零消费者 ⇒ 按 C6 三级准则**待裁决** ⇒ 归属 = 独立立项 / backlog）⇒ ⇒ **不并入「已消解」行**（否则 = 把残留藏进已消解项，违反「如实登记 / 零技术债」纪律） |
| ⑤ | 建议（ADR 同步） | ADR-038 §2.3 / §5 仍有效 ⇒ 需加 2026-11-11 更新注，**不得**判死 | **采纳** ⇒ ⇒ ADR-038 加更新注（含 `chlib::ForwardingUnit` 零实例化的残留登记） |
| ⑥ | 建议（ADR 同步） | ADR-046 D6 的「两者并存」已与磁盘矛盾 ⇒ 同文件 `:362` 已有同型注记范式 | **采纳** ⇒ ⇒ ADR-046 D6 加 2026-11-11 更新注 |
| ⑦ | 建议（ADR 同步） | ADR-046:367 的 forward-reference「合并（D6）—— ADR-038 §2.3 已留」：合并对象之一已退役 ⇒ 该 forward-reference 失效，但 §2.3 本身仍 open | **采纳** ⇒ ⇒ 改写为「合并对象之一已不存在 ⇒ forward-reference 失效 ⇒ §2.3 本身仍 open ⇒ 剩余待办 = `chlib::ForwardingUnit` 零实例化的处置」 |
| ⑧ | 建议（ADR 同步） | ADR-047 三处 stale：`:489-491`（列已删组件的合并）· `:501`（「它仍有消费者（`hazard_detection_unit.h` + 两个根测试）」—— 两个消费者**已随 C5/C6 删除**）· `:562-563` + `:579`（引用**已不存在**的路径） | **采纳** ⇒ ⇒ 四处各加 2026-11-11 更新注（`:562-563` 与 `:579` 明写「保留作**决策史 / 历史爆炸半径快照**，**不**代表当前磁盘状态」） |
| ⑨ | 存量 stale（同文件内） | `:41` cache「6 个头」→ 实测 **8**（且「wrapper」写成单数）· `:133` CLINT `mtime` 不自增 → **已于 2026-10-30 修复**（ADR-049 · change 已归档）· `:134` 两个 cache wrapper 分支不发射 → **已于 2026-10-28 修复**（change 已归档）· `:165` ISA 缺口列 `LB·LH·LBU·LHU` → **已覆盖**（`rv32i-isa-semantics/spec.md:200-240` + 判别性固件 `Rv32iSoc - LoadByteLaneDiscriminating`）· `:34` `rv32i_pipeline_regs.h`「490 行，**最大**」→ `rv32i_pipeline.h` **655 行**更大 | **采纳** ⇒ ⇒ 5 处全部订正（逐条实测，不采信描述） |

### 1.2 oracle 的**事实核实**（全部独立复核通过）

| 待核事实 | 结论 | 复核证据 |
|---|---|---|
| `include/cpu/pipeline/` = 10 + `stages/` 5 = 15 | ✅ 成立 | `ls` 实测 |
| 三个头已物理删除 | ✅ 成立 | glob → 无匹配；`git log` 见 `67926cf` / `bd21b45` |
| `riscv::HazardUnit` 是权威 hazard 组件且已接入真实流水线 | ✅ 成立 | `rv32i_pipeline.h:34` include · `:124` `ch_module<HazardUnit>` · 端口驱动 `:495-540` · 输出消费 `:251-252` / `:484-485` / `:603` / `:612` / `:638` |
| `chlib::ForwardingUnit` 组件类零实例化 | ✅ 成立 | 全仓 `ForwardingUnit<` 唯一命中 = `include/chlib/forwarding.h:59` 文档注释 |
| ADR-038 §2.3 的合并对象 | ✅ **oracle 正确 · 本次回填写错** | §2.3 标题「Don't merge `forwarding.h` and `riscv::HazardUnit`」；§5「Merge `chlib::ForwardingUnit` + `riscv::HazardUnit`」⇒ ⇒ **两者都还在** |
| cache 头文件数 = 8 | ✅ 成立 | `ls include/cpu/cache/*.h \| wc -l` = 8 |
| CLINT `mtime` / cache wrapper 分支已修 | ✅ 成立 | `feat-riscv-mini-rtl-debt.md:17-19`（两 change 均**已归档**） |
| `LB`/`LBU`/`LH`/`LHU` 已覆盖 | ✅ 成立 | `rv32i-isa-semantics/spec.md:200-240` + 固件 `test_rv32i_soc_dual_parity.cpp:1688` `Rv32iSoc - LoadByteLaneDiscriminating` |
| `rv32i_pipeline.h` 比 `rv32i_pipeline_regs.h` 大 | ✅ 成立 | `wc -l` = **655** vs **490** |

### 1.3 oracle 的**已排除替代方案**（本轮采纳其判断）

- **把 ADR-038 §2.3 判为「已废止」并删 §5 条目** ⇒ **不选**：合并对象**都还在** ⇒ ⇒ 删条目 = 抹掉一条仍有效的 future work，且会诱导后人以为 `chlib::ForwardingUnit` 可自由合并 / 删除 ⇒ ⇒ 正解是**加注**而非**判死**。
- **顺手把 `chlib::ForwardingUnit` 一并退役** ⇒ **不选**：符合 C6 三级准则，但退役须走公开面移除登记 + CHANGELOG + 护栏 ⇒ ⇒ 属**独立立项** ⇒ ⇒ 混进「事实回填」会让本次判定面失焦。
- **只改结构树、不动残留表** ⇒ **不选**：残留表是本文自称的「**如实登记**」面 ⇒ ⇒ 留着「角色错配」的比留着已删头的名字更误导。
- **把残留表行套 ledger 生成句模板** ⇒ **不选**：`verify_arch_ledger_residuals.sh` 只治理 `docs/architecture/2026-11-02-hierarchical-rtl-output.md` §7 与 `.rddf/checks/arch-residual-registry.md` ⇒ ⇒ **不覆盖本文件** ⇒ ⇒ 套模板反而与本文件既有风格不一致。
- **直接改写 ADR-038 / ADR-046 的正文条款** ⇒ **不选**：本仓惯例 =「裁决改 ⇒ 先改 ADR ⇒ 再同步本文」⇒ ⇒ 本次**裁决未改**（只是前置事实变了）⇒ ⇒ **更新注**是最小、可审计、零风险的形式。

---

## 2. 派单（本轮**不动** —— 跨域）

| 域 | 项 | 状态 |
|---|---|---|
| **builder / 组件域** | `include/cpu/AGENTS.md`：`:7` / `:19` 头数 **30 → 27**（30 − 3 退役）· `:48` NAMESPACE MAP `pipeline/*.h` **13 → 10** · STRUCTURE 各头行数刷新（实测 `rv32i_pipeline.h` **655** · `rv32i_branch_predict.h` 475 · `rv32i_pipeline_regs.h` 490 · `rv32i_regfile.h` 316 · `hazard_unit.h` 279 · `rv32i_alu.h` 193 · `rv32i_decoder.h` 163 · `rv32i_tcm.h` 163 · `branch_predictor_v2.h` 151 · `rv32i_pc.h` 47）⇒ ⇒ **不在 architect 域**（`include/cpu/AGENTS.md` 不在 `arch_paths`） | ⬜ 待派单 |
| **builder / 组件域** | `chlib::ForwardingUnit` 组件类**零实例化** ⇒ 按 C6 三级准则裁决（接入 / 退役 / 登记定位）⇒ ⇒ 退役须走 `public-api-surface.md` + `docs/CHANGELOG.md` + `verify_public_api_visibility.sh` | ⬜ 待独立立项 |
| **planner 域** | `.rddf/roadmap/**` 三处仍列 `riscv::ForwardingUnit`：`phase-5.md:112` · `objective-riscv-mini-convergence.md:25,101` · `feat-riscv-mini-convergence.md:17` ⇒ ⇒ 与「零消费者组件 MUST 在文档明写定位」的 capability 要求对齐 | ⬜ backlog（不阻断） |

---

## 3. 本轮的**护栏复跑**（改文档后）

```
verify_arch_ledger_residuals   PASS
verify_roadmap_fragments       PASS
verify_public_api_visibility   PASS
verify_test_counts             PASS
verify_dual_backend_parity     PASS
```

⚠️ **未跑**：ctest（本次**纯文档**改动 ⇒ 零代码面 ⇒ ⇒ 但 `.rddf/checks/public-api-removals.tsv`
的 `re` / `nofile` 探针面只扫 `*.h/*.hpp/*.tpp/*.inc` ⇒ ⇒ 散文提及**不会**误触红）。

---

## 4. 结论

- **oracle 判定**：`PASS-WITH-NOTES` ⇒ ⇒ **9 条建议全部采纳并落地** ⇒ ⇒ 无遗留 finding。
- **critic**：**未触发**（未新建 ADR · 未反转既有决策 · 未改裁决正文 ⇒ ⇒ 按角色矩阵「只同步主题
  文档 / 改措辞 → 只 oracle」）⇒ ⇒ **不是**「跳过」，是「不适用」。
- **本次回填自身曾有 1 处事实错误**（ADR-038 归因）⇒ ⇒ 已修正 ⇒ ⇒ **登记在案**（§1.1 ①）
  ⇒ ⇒ 教训：写「某裁决**失效**」之前，MUST **读该裁决的标题与合并对象**，不能凭关键词联想。
