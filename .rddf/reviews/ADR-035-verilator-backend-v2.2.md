# ADR-035 v2.2 审稿记录 —— R9 决策行修订（phase-6 **P8** 的 architect 步）

> **范围**：phase-6 **P8** 的 architect 步 = 把 ADR-035 §5 风险登记的 **R9 决策行**处置掉 ——
> 该行的缓解措施写「显式 `set_ab_verification(true)` 才启用」⇒ ⇒ 它把一个**即将不存在**的 API
> 呈现为**现行决策** ⇒ ⇒ 归档后 ADR-035 会继续呈现一条**不存在的 API** 的启用契约。
> **产物**：`docs/adr/ADR-035-verilator-backend.md` **v2.2 补丁**（末尾新章 · §5 R9 行改记 ·
> §「Phase 进度」与 §7 验证标准两处加注 · 元数据「修正审计」指针）。
> **下游实施** = phase-6 **P8**（builder）· **本轮**未实施任何一步。

## 1. oracle 第 1 轮（9 条 · 逐条应答）

| # | finding | 严重度 | 处置 |
|---|---|---|---|
| 1 | **事实错误**：v2.2 的「范围」段与根 `AGENTS.md:319` 都称「该块受 `#if __has_include` 保护 ⇒ `CH_JIT_ENABLE=OFF` 构建不含该符号」⇒ ⇒ 实测 `include/jit/jit_compiler.h` 是**源树物理头** ⇒ `__has_include` **恒真** ⇒ ⇒ 该 API 在**两种构建中都存在** | **高** | ✅ **采纳** ⇒ ADR-035「范围」段加**事实订正**专段（`__has_include` 对源树头恒真 ⇒ 删除**无条件** ⇒ 两种构建 MUST 各自编译通过）；⇒ ⇒ 并把「根 `AGENTS.md` 那句是**事实错误**、实施时 MUST 一并订正」写成 builder 的**范围义务**（**不是**纯删） |
| 2 | 「只删 4 个符号」清单**漏了两个局部量**：`ab_result_a`（今天起就是零使用的死声明）与 `run_ab_verify`（仅服务那段 WARN）⇒ ⇒ 删 WARN 块后即成死变量 ⇒ Zero-Debt 门会在 P8 评审卡壳 | 高 | ✅ **采纳** ⇒ 「只删」条目补这两个局部量 + 写明 Zero-Debt 理由（删 WARN 块后**必须**同删） |
| 3 | **「golden 已覆盖 runtime A/B 核心价值」过强**（golden = 测试期 · 固定向量 · 5/39 JitOp；runtime A/B = 运行期 · 任意输入 · 全部被触发的 JitOp）⇒ ⇒ 且漏了更硬的论据：**现状 stub 是零保护** ⇒ ⇒ 删除**不降低安全水位** | 高 | ✅ **采纳** ⇒ 选项表 A 的理由**重写**为「**现状 stub = 零保护**（只置位 + 每 tick 一条 `CHWARN`，从未比对）⇒ 删除**不降低安全水位**」+「golden 提供**同型**校验于测试期」+「JIT **默认 ON** ⇒ ctest 中 ≥`JIT_MIN_NODES` 的用例**本就在 JIT 路径上**以**手推期望值**断言 ⇒ runtime A/B 的增量价值 = 仅覆盖『未断言内部节点的运行期场景』」⇒ ⇒ **删掉**原「已覆盖核心价值」的过强表述 |
| 4 | **残余风险**未登记（删除后运行期**无** interpreter-vs-JIT 双跑 ⇒ 未被 golden 覆盖的 JitOp 误编译只在断言未达处暴露）⇒ ⇒ 且根 `AGENTS.md` KNOWN LIMITATIONS 那条**不要纯删**，要**替换** | 高 | ✅ **采纳** ⇒ v2.2 新增 §「残余风险（如实登记 · 不掩盖）」表（含缓解 ①②③）+ 明确指示 builder 把 AGENTS.md 那条**替换**为残余风险（**不是**纯删 ⇒ ⇒ 否则「运行期没有双跑校验」这件事**无人知道**） |
| 5 | 「已裁定（⏳ 待实施）」是否混淆「风险消解」与「决策做出」⇒ ⇒ 更诚实的形态？ | 中 | ✅ **采纳（采纳 oracle 的答复）** ⇒ 本例中风险的触发条件**就是载体本身** ⇒ ⇒ 「决策做出」与「风险消解」**同刻**发生 ⇒ ⇒ 无「决策已做但风险悬置」的窗口 ⇒ ⇒ 真正的风险是**误读**（「不适用」可被读成「缓解已实现」）⇒ ⇒ §5 表下加**关闭惯例句**（v2.2）：关闭方式 = **载体移除裁定**（**不是**缓解实现）· MUST 以「已裁定 / 已关闭 · 日期 · 指向补丁章」标注 · **MUST NOT 删行**（删行 = 丢审计轨迹）⇒ ⇒ 一次性改记**升格为可复用约定** |
| 6 | 「默认关闭的开关长期无人打开」—— 仓内**有**谁打开过吗？⇒ ⇒ 若零 ⇒ 该论据是**强**证据还是**空话**？ | 中 | ✅ **采纳** ⇒ 实测**仓内 0 个调用点**（`.cpp` 里唯一出现 = 定义本身；`ADR-005:313` 是注释里的示例）⇒ ⇒ 选项表 B 的理由补该证据 **+ 标注它是「仓内」证据**（下游可见性由 P14 登记承担 ⇒ ⇒ 不越界声称） |
| 7 | 判据① 补「**锚点同步**」指令（P14 登记会触发 `EXPECTED_REMOVAL_ROWS` / `EXPECTED_PROBES` 必 bump）⇒ ⇒ 并给 2 条探针的精确 tsv 行 | 中 | ✅ **采纳** ⇒ 判据 ① 补「锚点 MUST 同步 bump：`EXPECTED_REMOVAL_ROWS` **15 → 16** · `EXPECTED_PROBES` **24 → 26**」+ 建议的 2 条探针（`set_ab_verification` · `ab_verification_` · `kind=removed`）⇒ ⇒ 并注明「2 条即闭合 3 个符号」 |
| 8 | 审稿记录指针指向的文件**当前不存在** ⇒ ⇒ MUST 在本 architect 步收口时落盘 | 中 | ✅ **采纳** ⇒ 本文件即 `.rddf/reviews/ADR-035-verilator-backend-v2.2.md` ⇒ ⇒ 与 ADR-035 v2.2 章节的指针**互相咬合** |
| 9 | 判据④（`CH_JIT_ENABLE=OFF` 构建）成本过高 ⇒ ⇒ CI 当前无 OFF matrix leg ⇒ ⇒ 建议**降级为 lib-only** | 低 | ✅ **采纳** ⇒ 判据 ④ 改为「主构建全量 + OFF 构建**降级为库目标**（`--target cpphdl`）」⇒ ⇒ 并写明降级理由（库目标编译**足以**暴露成员 / 引用不匹配 ⇒ 全量 OFF 构建**另立 work item**）⇒ ⇒ ★ 与 oracle #1 的事实订正**合并**（两种构建**都含**该 API ⇒ ⇒ OFF 构建**必须**能过） |

### 1.1 替代方案（oracle 评估 · 本轮记录）

| 方案 | 结论 | 理由 |
|---|---|---|
| **G / C：保留 API + 进「已弃用（尚未移除）」表** | ❌ **未选**（已写入 v2.2 选项表 **C** 行） | ① 更省事（R9 行**不用**改）⇒ ⇒ 但该表既有两条都是**语义正确、仅不鼓励**的兼容面 ⇒ ⇒ 而 `set_ab_verification` 是**从未工作的 no-op** ⇒ ⇒ 按 ADR-050 D2 先例（`is_built` / `tick_state` 这类「**说谎的 API**」= **移除 + 登记**，不是弃用）；② Zero-Debt「No dead code」—— 给死代码贴弃用标签**仍是**死代码；③ phase-6 **P8** 行已裁定 A 并记录 B 的拒绝理由 ⇒ ⇒ 此刻改 C = 重开已定路由 ⇒ ⇒ **不选**，但**留档**（= 取舍的完整形态） |
| **F：保留 API 并改成真实现** | ❌ **未选** | = B（已在选项表 B 记录拒绝理由）⇒ ⇒ **不重启** |

## 2. critic 第 1 轮（`arch` 档）⇒ **REJECT**（1 BLOCKER + 2 SHOULD_FIX + 1 NIT）

| # | finding | 处置 |
|---|---|---|
| **B1** | **判据② 与本文档的保留义务自相矛盾**：判据② 的 grep 范围含 `docs/` 且只允许 **1 处**命中（「本 ADR 的 R9 行即该 1 处」）⇒ ⇒ 但 ① §5 规定 **R9 行 MUST NOT 删行** ② **§v2.2 整章必须保留**（元数据指向它）⇒ ⇒ 该章内 `set_ab_verification` / `ab_verification_` 出现 **≥6 处**（选项表 / 范围 / 判据① / 判据② 自身）且多数**不带**「已移除」⇒ ⇒ **P8 正确实施后字面 grep 仍 ≫1** ⇒ ⇒ 判据**恒红** ⇒ ⇒ 实施方无法判断通过 | ✅ **采纳并修** ⇒ 拆成**两条可 grep** 判据：**(a) 代码面** `include/ src/ AGENTS.md` 命中 == **0**；**(b) docs/ 面** 排除**本 ADR**（R9 行 + v2.2 章 = **审计轨迹** ⇒ 不计入）⇒ ⇒ 其余每条命中 MUST 在**同一段**含「已移除」⇒ 判据 = 过滤后**剩余 == 0** |
| S1 | 「关闭惯例」以**单条 ADR 自我升格为仓级可复用约定** ⇒ **越权**（仓内多张 ADR 各有风险表 ⇒ 单张无约束力、也无可见性） | ✅ **采纳** ⇒ 收窄为 **本 ADR 内**的局部约定 + 明写「仓级化的通用约定 = **独立 work item**（落仓级治理文档）⇒ 本轮**不**做」+ 保留写在此的理由（让**本表**的关闭写法可追溯） |
| S2 | **判据③ 无机械判据**（`scripts/verify_test_counts.sh` **无** golden/JitOp 锚点 ⇒ 「今天就已满足」⇒ 不可执行）⇒ 且「新增护栏」只写在审稿记录 §4 ⇒ **归档后 ADR 与判据脱钩** | ✅ **采纳** ⇒ 判据③ 补**机械核法**（分子 `grep -oE "JitOp::[A-Z_0-9]+" tests/jit/test_jit_golden.cpp \| sort -u \| wc -l` ≥ 5；分母 `grep -cE '^  [A-Z_0-9]+,' include/jit/jit_ir.h` == 39）+ 明写「**builder MUST 新增护栏锚点**并让本判据以该锚点为准（否则『防回退』是空判据）」 |
| N1 | AGENTS.md 的**替换**义务只给了内容、未给**逐字文本** | ✅ **采纳** ⇒ v2.2 的「文档同步」条补**建议逐字文本**（供 builder 直接采用，含事实订正句） |

### 2.1 critic 第 2 轮（**复审** · 只验 B1 闭合）

⇒ ⇒ ❌ **REJECT** —— **B1 `CLOSED`**，但本轮 diff **引入 2 个新 BLOCKER**（均为一行的判据形态问题）：

| # | 新 BLOCKER | 处置 |
|---|---|---|
| **N-B1** | 判据③ 的**分母正则字面必红**：`grep -cE '^  [A-Z_0-9]+,' include/jit/jit_ir.h` 只数**带逗号**的项 = **38**，而末项 `NOP` **无尾逗号** ⇒ ⇒ 期望 `39` **永不成立** | ✅ **已修** ⇒ 正则改 `'^  [A-Z_0-9]+,?$'` ⇒ ⇒ **实测 38 → 39** ✔；并在 ADR 内注明「旧正则数到 38 ⇒ 判据会恒红」 |
| **N-B2** | ②(a) 要求 `AGENTS.md` **零命中**，而 N1 的**建议逐字文本**必含 `set_ab_verification` ⇒ ⇒ 两处指令**不可同时满足**（「替换 ≠ 纯删」的既定决策与新判据冲突） | ✅ **已修** ⇒ ②(a) 收窄为 **`include/ src/` == 0**（`AGENTS.md` **移出**零命中面 + 明写理由）；`AGENTS.md` 并入 **(b) 文档面**（排除本 ADR 审计轨迹 ⇒ 其余命中 MUST 同段含「**已移除**」或「**已删除**」） |

⇒ ⇒ ⚠️ **评审预算**：critic 已用 **2/2** ⇒ ⇒ 第 3 轮**经用户明确批准**（用户口令「批准 ADR-035 的第 3 轮复审」）。

### 2.2 critic 第 3 轮（**复审** · 只验 2 个新 BLOCKER 闭合）

⇒ ⇒ ✅ **APPROVE**（`mergeable: yes` · `scope_exceeded: false`）· **N-B1 `CLOSED`** · **N-B2 `CLOSED`** · **无新 BLOCKER**
⇒ ⇒ 复核依据（critic 独立复跑，与本记录逐字一致）：
- **N-B1**：新正则 `grep -cE '^  [A-Z_0-9]+,?$' include/jit/jit_ir.h` = **39** · 旧正则 = **38** ·
  分子 `grep -oE "JitOp::[A-Z_0-9]+" tests/jit/test_jit_golden.cpp | sort -u | wc -l` = **5**
  ⇒ ⇒ 判据字面**可执行**，「判据会恒红」的警示注释**保留在位**；
- **N-B2**：② 已拆为 (a) 代码面（`include/ src/` == 0 · **`AGENTS.md` 不在此列** + 理由）与
  (b) 文档面（`AGENTS.md` + `docs/` · 排除本 ADR 审计轨迹 · 其余命中 MUST 同段含「已移除 / 已删除」）
  ⇒ ⇒ 交叉核对：v2.2 建议的 AGENTS.md 替换文本**本身含「已删除」** ⇒ 满足 (b) 过滤器
  ⇒ ⇒ 两条判据与「替换 ≠ 纯删」的既定决策**可同时满足** ⇒ ⇒ 矛盾**消除**。
⇒ ⇒ ⇒ ★ **本 architect 步至此 0 BLOCKER 闭合**（critic 总轮次 = 3 · 其中 2 轮为复审）。

## 3. 判据自检（**当前实测**）

```bash
grep -c '已裁定（2026-11-09 · v2.2 · ⏳ 待实施）' docs/adr/ADR-035-verilator-backend.md   # 1（R9 行）
grep -c '残余风险' docs/adr/ADR-035-verilator-backend.md                                 # ≥1（oracle #4 闭）
grep -c '锚点 MUST 同步 bump' docs/adr/ADR-035-verilator-backend.md                      # 1（oracle #7 闭）
grep -c '__has_include' docs/adr/ADR-035-verilator-backend.md                             # ≥1（事实订正段 · oracle #1 闭）
```

## 4. 遗留（**如实登记**）

| 项 | 归属 | 状态 |
|---|---|---|
| 删 `set_ab_verification` / `is_ab_verification` / `ab_verification_` + WARN 块 + 两个局部量 | **P8（builder）** | ⏳ 待做 |
| P14 移除登记（登记表 +1 行 · tsv +2 探针 · 锚点 15→16 / 24→26 · CHANGELOG 一行） | **P8（builder）** | ⏳ 待做 |
| 根 `AGENTS.md` KNOWN LIMITATIONS 那条：**替换**为残余风险 + 订正「`CH_JIT_ENABLE=OFF` 时符号不存在」的**事实错误** | **P8（builder）** | ⏳ 待做（**不是**纯删） |
| `docs/usage_guide/10-verilator-backend.md:141` + `docs/simulation/JIT_ROADMAP.md:216` + `docs/adr/ADR-005-dual-mode-simulation.md:313` 的 A/B 表述 | **P8（builder）** | ⏳ 待做（MUST NOT 留「可用但占位」表述） |
| golden 覆盖**下限 5 / 39** 的守护（新增 JitOp MUST 附 golden 或登记理由） | **P8（builder）** | ⏳ 待做（防回退 · 机械核法已实测可行） |
| openspec change **立项**（AC 映射回填 ADR-035 v2.2 的 `backlog:` 指针） | **planner** | ⏳ 待办（`backlog: phase-6 P8`） |
