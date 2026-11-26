# 审稿报告 · ADR-048 **dated addendum v1.1**（取指级 PC 重定向的第二路 · P6-3）

> **对象**：`docs/adr/ADR-048-rv32i-fetch-skid-buffer.md` §8（dated addendum v1.1 · 2026-11-26）
> ＋ `docs/adr/ADR-071-fence-and-illegal-opcode-semantics.md` 的 **v1.3** 标注
> **关联 change**：`openspec/changes/rv32i-trap-entry-2026-11/`（P6-3 · trap 入口接线）
> **母 improvement**：`.rddf/improvements/rv32i-privileged-csr-2026-11.md`
> **审查顺序**：**oracle → critic**（不可颠倒）· **critic 轮次 = 1/2**（本 addendum）
> **最终判定**：**APPROVE_WITH_CONDITIONS**（critic · 0 BLOCKER · 2 SHOULD_FIX · 2 NIT）
> ⇒ ⇒ **0 BLOCKER ⇒ 无需复审** ⇒ ⇒ SHOULD_FIX / NIT 逐条处置后**落档**

---

## 1. oracle（对抗式审查 · 「这个决策在目标场景会怎么坏」）

**判定**：**REJECT**（1 BLOCKER + 3 中高 + 3 低/可选）⇒ ⇒ **修订后**由 critic 判通过。

| # | 级别 | finding | 处置（**逐条**） |
|---|---|---|---|
| **F1** | **BLOCKER** | ⚠️ **DA3 漏了第 4 个 kill 站点 ⇒ 活锁**：`if_id_valid_reg->next` 是**两级 select** —— `if_id_update_en = 0` 时走 else 分支，由**内层**条件（`io().flush \| flush_d1 \| !primed`）决定「清零 vs 保持」⇒ ⇒ 只门 `if_id_update_en` ⇒ ⇒ `valid` **永不清零** ⇒ ⇒ 触发指令留在 IF/ID ⇒ ⇒ 下一拍 `illegal_reason` 仍非零 ⇒ ⇒ trap **每拍重触发** ⇒ ⇒ PC 每拍重定向 `mtvec`（幂等）⇒ ⇒ `if_id_update_en` 永 0 ⇒ ⇒ **`mtvec` 处的指令永不进 ID = 活锁**。⚠️ 该活锁**不会被**「`mepc`/`mcause`/不退休/PC→`mtvec`」类判据抓到 | ✅ **已修**：**DA3 改为「四个 MUST 站点」**（① `if_id_update_en` ② `skid_valid` 清空 ③ `skid` 填充排除 ④ ⚠️ `if_id_valid_reg` 的内层清零条件）+ 活锁链**逐拍写进 ADR** + **V3 改为「不装载**且 `valid` 清零**」**（直接观测 `valid`）⇒ ⇒ 判据网能抓到 |
| **F2** | 中 | ⚠️ **trap 拍的 ID/EX 气泡来源未登记**：「触发指令不进 EX」实际由 `illegal_active ⇒ stall`（`rv32i_pipeline.h` 驱动 `id_stage.io().stall` 的那一项）经 `idex_bubble` 承担 ⇒ ⇒ 若以「trap 已取代冻结」为由删掉该 OR 项 ⇒ ⇒ 触发指令以 `valid=1` 装进 ID/EX ⇒ ⇒ 进 EX 并退休 | ✅ **已修**：**DA4 追加承重登记**（该 OR 项在 trap 机制下**仍是承重件** ⇒ MUST NOT 移除） |
| **F3** | 高 | ⚠️ **对 ADR-071 / `rv32i-isa-semantics` 的 supersede 未登记**：ADR-071 D3 明写「MUST NOT 陷阱到 `mtvec`」·「冻结是终态」；母 improvement「接缝兑现 #1」要求接线 change 以 **MODIFIED delta** 修订该冻结 Requirement；而 proposal AC10 写「既有 Requirement 不动」⇒ ⇒ 主 spec 将与实现漂移 | ✅ **已修**：**§8.7 同步义务 S1–S4**（① ADR-071 v1.3 dated 标注 ② change MUST 带 `rv32i-isa-semantics` 的 **MODIFIED delta** ③ 根 `AGENTS.md` 红线同步改写 ④ capability「不覆盖」清单的逐项归属）⇒ ⇒ **ADR-071 的 v1.3 标注本轮已落档**（含 D3 与 **D3-b** 两处） |
| **F4** | 中 | ⚠️ **触发集在 change 各载体间两两矛盾**：`design.md` 与 delta 头写「`reason != 0` 全触发」⇒ ⇒ `ECALL` 也会 trap ⇒ ⇒ 与 proposal O1 + delta 末条 Scenario（「`funct3=0` MUST 仍冻结」）**打架** | ✅ **已修**：**DA5 把触发集钉死为 `illegal_reason ∈ {2,3}`**（未知 opcode + 保留组合）· `reason = 1`（ECALL/EBREAK/MRET/WFI）**维持冻结**（归 P6-4/P6-5）· `illegal_active`（`!= 0`）**继续只服务 stall** ⇒ ⇒ 并由 **critic 的 SHOULD_FIX-1** 追加 **§8.7 S5**（change 侧四处载体 + delta Requirement 头的订正清单） |
| **F5** | 低/可选 | ⚠️ **优先级「trap > branch」在流水线级不可观测**（DA5 的 `!flush` 掩码保证互斥）⇒ ⇒ 变异「优先级写成 branch > trap」**恒绿**（违反本 ADR 自己的 D9.4 教义） | ✅ **已修**：**V1'**（组件级）＋ DA1 注明该优先级当前是**防御性**的（为 P6-6 的中断来源预备） |
| **F6** | 低/可选 | `trap_redirect` 与 `trap_valid` 的关系未定义 | ✅ **已修**：**DA2 补定义**（`trap_redirect ≡ trap_flush`；P6-5 引入 `mret` 时 override 集 MUST 同步扩） |
| **F7** | 低/可选 | DA3 说「重新表述」但**没给内容** | ✅ **已修**：**DA3 内补三行论证**（`skid_valid = 1` ⇒ 停顿窗口 ⇒ ID 冻结在消费者 C ⇒ C 首拍 = 停顿首拍 ⇒ C 非法 ⇒ trap 已触发 ⇒ 填充被排除 ⇒ 矛盾 ⇒ 不可达） |
| **F8** | 低/可选 | 同步面：① tasks T3 未提「重写 `if_stage.h` 的『flush 唯一来源』注释块」② Batch 4-a 的驱动 helper 只驱动 7 个输入 ⇒ ⇒ +2 端口后 MUST 扩展（`trap_flush = 0`），否则 V2 的「契约未被破坏」建立在**未驱动端口**上 | ✅ **已修**：**§8.4 代价 ⑤⑥**（并按**复核中的独立发现**加强：helper **实际只驱动 4 个输入**，**连 `instr_ready` 也没驱动** ⇒ ⇒ `if_id_valid_reg` 恒 0 ⇒ ⇒ **既有 Batch 4-a 的组件级判据可能在保真度不足下通过** ⇒ ⇒ MUST 显式驱动 `instr_ready = 1`）＋ **§8.7 S5** 把该义务指向 **tasks T5 新增组件级用例行**（修正 critic 指出的「指向了不含该内容的 T3」） |

⚠️ **oracle 的「已排除的替代方案」三条**（结论与本 ADR §8.3 的选项表一致 ⇒ 无需改）：
「DA3 三站点已够」被排除（两级 select 是**结构事实**）· 「B（合并 flush）除 kill 域外可接受」被排除
（**更致命**的是 DA5 的 `!flush` 掩码在单信号下**不可表达** ⇒ 成分必须可分 ⇒ 合并自始不成立）·
「trap∥skid 可达 ⇒ DA3 清空是承重而非防御」被排除（新论证下仍不可达 ⇒ 仍是**防御性契约**）。

---

## 2. critic（`arch` 档 · 「与既有 ADR / 契约是否冲突」）

**判定**：**APPROVE_WITH_CONDITIONS** · `mergeable: yes` · `scope_exceeded: false` · **0 BLOCKER**

| # | 级别 | finding | 处置 |
|---|---|---|---|
| — | **BLOCKERS** | **未发现**（三类逐条查过：① 与已签批 ADR 冲突 —— 取代流程**已走**且范围**精确**；② 决策缺备选/理由/回滚 —— 选项表 4 行 + 后果表 + dated addendum 机制本身构成回滚路径；③ 文档内自相矛盾 / 未决写成已决 —— DA1–DA5 自洽且与实现现状吻合） | — |
| **SHOULD_FIX-1** | SHOULD_FIX（最接近 BLOCKER） | ⚠️ **DA5/DA1/DA3 的「传播与接线」不完整**：① §8.7 漏了 change 侧**四处仍写 `illegal_active`**（`proposal.md` S2 · `design.md` §2.1 · `tasks.md` T4.3 · 母 improvement 正确性条款）＋ **delta 的 Requirement 头把 `funct3 = 0` 写进触发集**（与自身末条 Scenario 直接打架 ⇒ ⇒ 照此归档会把**自相矛盾**的 Requirement 冻进主 spec）；② **V9 无 AC 落点**（change 的 AC1–AC11 没有覆盖「`reason = 1` 仍冻结」⇒ ⇒ DA5 的**负向判据在验收网里是空的**）；③ **组件级判据无任务家**（V1'/V4 必须落组件级，但 tasks T5.1 只建流水线级测试文件；§8.4 ⑤ 指向的 T3 不含该内容；T2.1 还引用「D1–D4 四条」） | ✅ **已修**：**§8.7 新增 S5**（逐条列出：四处触发集载体 + delta Requirement 头 + T2.1 的 DA1–DA5 + T5 组件级用例任务 + 补「`reason = 1` 仍冻结」的 AC）· **§8 头部 AC 映射行订正**（V5–V8 → AC1–AC8；V1/V1'/V2/V3/V4 = 组件级 + D9.4 义务；V9 的落点由 S5 补齐）· **§8.4 ⑤ 的指向**改指 **T5 新增组件级用例行** |
| **SHOULD_FIX-2** | SHOULD_FIX | ⚠️ **ADR-071 的 D3-b 未点名**：v1.3 标注覆盖了 D3 的「冻结是终态」，但 **D3-b 的优先级公式**（`pc_update_en = !rst & !(stall & !(flush & branch_valid))`）被 §8 DA2 扩展（override 集加 `trap_redirect`）、**D3-b ③**（「正确路径上冻结仍是终态」）对 `reason ∈ {2,3}` 同样失效 ⇒ ⇒ 只读 D3-b 的读者**不会**知道自己看到的是旧形态 | ✅ **已修**：ADR-071 §2.3 的 v1.3 就地标注**追加一句**（D3-b 公式被 DA2 扩展 · D3-b ③ 对 `reason ∈ {2,3}` 失效） |
| **NIT-1** | NIT | DA1 表 ② 行写「trap / `mret`」而无「`mret` 归 P6-5」标记 ⇒ ⇒ 单读该表会误以为 `mret` 本次接线 | ✅ **已修**（DA1 表 ② 行加「**`mret` 归 P6-5**；本项 `mret_valid` **恒 0**」） |
| **NIT-2** | NIT | §8.4 ① 承认 `IfStage` module 端口表变（13 → 15）但**全仓无任何护栏钉模块级端口表**（R8′ 只钉顶层 53 根）⇒ ⇒ 免得后人误以为有机械判据 | ✅ **已修**（§8.4 ① 追加「⚠️ **模块级端口表变化当前无机械判据** ⇒ 靠本行登记」） |

⚠️ **critic 的两条「验证建议」**：
① 落档时**一并创建**本报告文件（§8 头部引用了它，**当时不存在**）⇒ ⇒ **本文件即该报告** ✔
② builder 完成 S5 后跑 `grep -n "illegal_active" openspec/changes/rv32i-trap-entry-2026-11/{proposal,design,tasks}.md`
（应只剩「stall 专用」语境的命中）+ `openspec validate rv32i-trap-entry-2026-11 --strict` ⇒ ⇒ **归 builder**（见 §4）
③ V1' 的可实施性（`ch_device<IfStage>` 可同时驱动 `trap_flush = 1 ∧ branch_valid = 1`）是**推断，未验证**
⇒ ⇒ 实施时 MUST 先确认该用例**编译通过**再谈判别力（变异纪律 ②）⇒ ⇒ **归 builder**

---

## 3. 落档状态（**Step 4 收口**）

| 项 | 状态 |
|---|---|
| **ADR-048 Status** | **仍为 Accepted**（dated addendum **不放宽**任何既有不变量 ⇒ ⇒ 不改 Status） |
| **取代流程** | §8 头部标注 + **D3 附带收益**就地标注 + **D5 防御性契约**就地标注（三处） |
| **ADR-071** | **v1.3** 修订行 + **D3** 就地标注 + **D3-b** 同步说明 + §4.2 代价 ② 修订 |
| **关联 change** | `openspec/changes/rv32i-trap-entry-2026-11/`（§8 头部与 §8.7 均已引用） |
| **审稿指针** | 本文件（`.rddf/reviews/ADR-048-pc-redirect-second-path-review-2026-11.md`） |
| **过程日志** | **不入正文**（oracle / critic 的逐条处置只留在本文件；ADR 内只留**结论 + 指针**） |
| **AC 映射** | §8 头部（V5–V8 → AC1–AC8；V1/V1'/V2/V3/V4 = 组件级；V9 由 S5 补齐） |

---

## 4. ⚠️ **builder 待办**（**architect 写集不含** `openspec/changes/**` ⇒ ⇒ 须由 builder 执行）

> 逐条对应 §8.7 的 **S1–S5**。⚠️ **S2–S5 未完成前不得归档该 change**（否则主 spec 会冻结进
> 与实现漂移 / 自相矛盾的条款）。

| # | 待办 | 落点 |
|---|---|---|
| **B1** | 触发集表达式订正为 `illegal_reason ∈ {2,3}`（四处载体：`proposal.md` S2 · `design.md` §2.1 · `tasks.md` T4.3 · 母 improvement 正确性条款） | change + improvement |
| **B2** | `rv32i-privilege` delta 的 **Requirement 头删去「`OP_SYSTEM` 的 `funct3 = 0`」**（与自身末条 Scenario 打架） | change 的 delta |
| **B3** | **新增** `rv32i-isa-semantics` 的 **MODIFIED delta**（两条 Requirement：「未知 opcode MUST 可观测，且 MUST NOT 被当作 NOP 退休」+「冻结与 `flush` 同拍时 MUST 以**重定向优先**」）⇒ ⇒ 既有内容**一条都不删**（只改写冻结 ⇒ 陷入那几处）⇒ ⇒ **禁** `--skip-specs` | change 的 delta |
| **B4** | `design.md` §4.2 **订正冲突窗口** = **拍 T 本身**（`ExceptionUnit` 的 `csr_write_en` 在 trap **同拍**就拉高 ⇒ ⇒ 与 EX 级那条 CSR 写指令**同拍冲突**）⇒ ⇒ 仲裁规则（`ExceptionUnit` 优先）是**承重**的，不是「理论上不会发生」 | change 的 design |
| **B5** | `design.md` §7.1 改为**指针**到 ADR-048 §8（权威在 ADR）⇒ ⇒ 删除与 ADR 重复的草案正文（避免两份漂移） | change 的 design |
| **B6** | `tasks.md`：T2.1 的「D1–D4 四条」⇒ **DA1–DA5**；**T5 补组件级用例任务**（V1'/V4 宿主 = `tests/test_rv32i_pipeline_behavior.cpp`，并把 `instr_ready = 1` / `trap_flush = 0` / `trap_target = 0` 显式驱动）；**补一条「`reason = 1` 仍冻结」的 AC**（V9 的落点）；T3 补「重写 `if_stage.h` 的『flush 唯一来源』注释块」 | change 的 tasks |
| **B7** | 根 `AGENTS.md`：RV32I 红线「进 ID 级即冻结」**改写**（`reason ∈ {2,3}` ⇒ 冻结 ⇒ **陷入**；`reason = 1` 冻结不变）+ 计数锚点 | 根 `AGENTS.md`（builder 写集） |
| **B8** | 验收：`bash scripts/verify_docs_links.sh` · `openspec validate --specs` · `ctest -LE perf` | builder |

---

## 5. 已用轮次

- **oracle**：1 轮（判定 REJECT ⇒ 5 条 finding 全部处置）
- **critic**：1 轮（判定 **APPROVE_WITH_CONDITIONS** · 0 BLOCKER ⇒ **无复审**）
- **总轮次**：**2**（同一 ADR 的 addendum：oracle 1 + critic 1；**BLOCKER 复审额度未动用**）
