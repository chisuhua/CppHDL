# 任务清单：`hierarchical-corpus-regression-2026-11`

> 判据来源：proposal.md §Acceptance · capability delta `hierarchical-corpus-regression`
> （**ADDED ×3**）· improvement 草稿 v2
> `.rddf/improvements/hierarchical-corpus-regression-2026-11.md`
> ⚠️ 本 change **不**新增公开 API 面（⇒ ⇒ 用 P8 已有的 `ch::hier_eq`）⇒ ⇒ ⇒ 但**触及**
> 护栏 + 测试基础设施 ⇒ ⇒ 归档前**仍走 critic**。

## T1 共享 fixture（`tests/hier_corpus_fixture.h` · 新增）

- [x] 1.1 新增 `tests/hier_corpus_fixture.h` ⇒ ⇒ 提供 T0 的**通用**判据
      （★ 用 **P8 的公共 API** ⇒ ⇒ **零**重复实现）
- [x] 1.2 `ensure_dir`（⚠️ `emit` **不建** `work_dir` ⇒ ⇒ MUST 由调用方建）
- [x] 1.3 `modules_of(signal_map)` ⇒ ⇒ module 清单取自**发射器自己的**真相
      ⇒ ⇒ **MUST NOT** 自研文本解析器
- [x] 1.4 `ports_self_consistent(ctx, ports)` ⇒ ⇒ 每个 input 端口 ↔ ctx 的 `type_input`
      节点；clock/reset 齐备 ⇒ ⇒ ⚠️ clock/reset 是**独立**节点类型 ⇒ ⇒ MUST 接受它们
- [x] 1.5 `check_product_level<Dut>(tag, work_root)` ⇒ ⇒ 6 项：发射成功 · ≥2 module ·
      端口集自洽 · 确定性 · ≠ 扁平 · lint（★ verilator 不可用 ⇒ ⇒ 显式 skip）

## T2 既有语料文件内的新增用例（★ DUT 的类**在各自文件内** ⇒ ⇒ 只能留在那里）

- [x] 2.1 `tests/test_rv32i_arch_dual.cpp`：**3** 个 T0 用例（`Rv32iPcDut` · `ClintDut` ·
      `CsrBankDut`）
- [x] 2.2 `tests/chlib/test_pipeline_dual.cpp`：**2** 个 T0 用例（`StageDut` · `ChainDut`）
- [x] 2.3 `tests/test_module.cpp`：**3** 个 T0 用例（`ModuleValueTransferTop` ·
      `ModuleChainTop` · `ModuleIoDirectionTop`）+ **2** 个 T1 用例（C2 · C1'）
- [x] 2.4 `examples/riscv-mini/tests/test_cache_smoke.cpp`：**1** 个 T0 用例
      （`ICachePrefetch`）+ **1** 个 T1 用例（C3）
- [x] 2.5 `tests/test_rv32i_stage_dual.cpp`：**1** 个**拒绝边界**用例
      （`ImmConsistencyDut` ⇒ ⇒ MUST 被拒 + 原因点名 `implicit-port synthesis`）
- [x] 2.6 ⚠️ **MUST NOT** 改既有 DUT 的**类定义** ⇒ ⇒ **零**改既有
      `DUAL_BACKEND_TEST_PAIR` 调用 ⇒ ⇒ ⇒ parity 的两个锚点**不变**

## T3 登记表 + 覆盖栅栏

- [x] 3.1 新增 `tests/hier_corpus_registry.md` ⇒ ⇒ 逐 DUT 登记：覆盖层 · 结构类 ·
      **判别力上限** · 已知边界 · 豁免表
- [x] 3.2 新增 `scripts/verify_hier_corpus_coverage.sh` ⇒ ⇒ **三向**双向核对
      （① 登记 → 用例 ② 用例 → 登记 ③ **★ 语料 → 登记**）
- [x] 3.3 分析器 MUST 扫 **`include/`**（★ `ICachePrefetch` 是**库**类 ⇒ ⇒ 只扫
      `tests/` + `examples/` 会**漏判**⇒ ⇒ ⇒ 第 ③ 个方向就**有洞**）
- [x] 3.4 分析器 MUST 接受**带命名空间限定**的 DUT 名（★ `chlib::ICacheAsync`）
- [x] 3.5 `--self-test` 投毒：① 登记表**删一行** ⇒ 红 ② 登记表**加一个不存在的 DUT** ⇒ 红
      ⇒ ⇒ 还原复绿 ⇒ ⇒ **MUST 非零退出**
- [x] 3.6 **CI 接线**（★ 护栏是**纯静态** ⇒ ⇒ 与分片无关 ⇒ ⇒ 每个分片都跑）

## T4 判别力自证（变异 · 每条先确认仍能编译）

- [x] 4.1 变异 ① **去掉 T0 的「≥2 module」检查** ⇒ ⇒ **恒绿** ⇒ ⇒ **无效变异**
      （★ 检测器级 ⇒ ⇒ 测试断言的就是 fixture 的**自报** ⇒ ⇒ 自指限制 ⇒ ⇒ 见 §C）
- [x] 4.2 变异 ② **去掉 T0 的「≠ 扁平产物」检查** ⇒ ⇒ **恒绿** ⇒ ⇒ **无效变异**
- [x] 4.3 变异 ③ **去掉 T0 的「确定性」检查** ⇒ ⇒ **恒绿** ⇒ ⇒ **无效变异**
- [x] 4.4 变异 ④ **去掉 T0 的「端口集自洽」检查** ⇒ ⇒ **恒绿** ⇒ ⇒ **无效变异**
      （★ critic focus 2(b) 判定它是 T0 六项里**最弱**的一项 ⇒ ⇒ 两端都来自
      发射器/ctx 的自报 ⇒ ⇒ 价值是**纯防御性**的）
- [x] 4.5 变异 ⑤ **把 T1 的激励改成恒定** ⇒ ⇒ `compare` **拒绝**
      （前缀 `no-varying-signal`）⇒ ⇒ ⇒ **系统级**变异 ⇒ ⇒ 对应用例**转红**
- [x] 4.6 ★ **系统级**变异：破坏 `Granularity::ForceLayer` 的语义
      （`src/component.cpp` 的 `is_layer()`）⇒ ⇒ 层次化产物不再分层 ⇒ ⇒
      ⇒ ⇒ **对应用例转红** ⇒ ⇒ ⇒ **这是「发射器 → fixture → 断言」整条链有判别力的证据**
- [x] 4.7 护栏级：去掉第 ③ 个方向 ⇒ ⇒ **恒绿** ⇒ ⇒ **无效变异**
      （★ 它是 `self-test` 投毒③ 的**反向对照** ⇒ ⇒ ⇒ 证明「③ 方向」是抓住
      「新增未登记的复合 DUT」的**唯一**机制）
- [x] 4.6 每条变异**还原后** MUST **逐字节比对 + 重建 + 复绿**
- [x] 4.7 ⚠️ **变异脚本 MUST 检查 python 退出码**（★ 否则断言失败会**静默跳过**变异 ⇒ ⇒
      误判「无判别力」⇒ ⇒ 本仓 P6 已踩过）

## T5 零回归 · 锚点 · 文档

- [x] 5.1 `ctest -LE perf` 全绿 · samples 全绿 · examples 全绿
- [x] 5.2 计数锚点：★ **parity 的两个锚点 MUST 不变**（66 / 289）⇒ ⇒ ⇒
      `CTEST_TOTAL` **不变**（★ 新用例进**既有** target ⇒ ⇒ **不**产生新 ctest 条目 ⇒ ⇒
      ⇒ **MUST NOT** 新增 target ⇒ ⇒ ⇒ 否则锚点漂移 + CI 时间）
- [x] 5.3 ⚠️ **新用例**带 `verilator_e2e` 标签 ⇒ ⇒ ⇒ T1 跑**真 verilator**
      ⇒ ⇒ ⇒ MUST 落在**已有**的 `verilator_e2e` target 里（⇒ ⇒ `test_rv32i_stage_dual`
      / `test_module` / `riscv_mini_cache_smoke` **已是** `verilator_e2e` ⇒ ⇒ ⇒ **零**锚点变动）
- [x] 5.4 文档：登记「覆盖了什么 / **没**覆盖什么 + 为什么」（★ 台账登记要求的
      「**未覆盖**是已知边界而非遗漏」）
- [x] 5.5 口径差**显式登记**（★ 台账措辞归 architect ⇒ ⇒ 本 change **MUST NOT** 改台账）
- [x] 5.6 `openspec validate hierarchical-corpus-regression-2026-11 --type change --strict` PASS

## 审查（critic · **1 轮** ⇒ `REJECT` ⇒ 1 BLOCKER 已闭）

- **轮次**：**1 / 2** · **结论**：`REJECT` · `mergeable: no` · `scope_exceeded: false`
- **统计**：**1 BLOCKER** · **3 SHOULD_FIX** · **3 NIT**
- **记录**：`.rddf/reviews/hierarchical-corpus-regression-2026-11-critic.md`

| # | 类型 | 内容 | 处置 |
|---|---|---|---|
| **B1** | BLOCKER | **豁免机制机械上不可用**，与 delta Requirement ① 的「覆盖**或**显式豁免」**直接矛盾** ⇒ ⇒ 未来任何需走豁免路线的复合 DUT ⇒ ⇒ CI 进入**无解的红** | ✅ **已闭合**：分析器改为**按 section 解析**（豁免 section 特殊 · 其余默认登记）⇒ ⇒ 新增方向 **③'**（豁免项 MUST 确实在语料里）⇒ ⇒ 自检新增**投毒④**（豁免路径可用 ⇒ 必须绿）与**投毒⑤**（豁免不存在的 DUT ⇒ 必须红）⇒ ⇒ ⇒ **已实测复现并验证修复**（★ 第 2 轮复审指出「修复后该形态绿」是**夸大** ⇒ ⇒ ⇒ 已实测证伪并按实测口径改写 ⇒ ⇒ ⇒ 证据用**投毒 ④ 的合成形态**） |
| SF1 | SHOULD_FIX | `tasks.md` 的 4.1–4.3（计划措辞「转红」）与 §C（实测「恒绿 ⇒ 无效变异」）**自相矛盾** | ✅ **已统一**：4.1–4.7 改写为**实测口径**（恒绿 = 无效变异 ⇒ 附理由）⇒ ⇒ 并新增 **4.6 系统级变异**（证明整条链有判别力）与 **4.7 护栏级反向对照** |
| SF2 | SHOULD_FIX | **T1 在 verilator 不可用时硬失败**（`REQUIRE(r.ok)`）⇒ ⇒ 与同 change 内 T0 的 `lint_skipped` 分支**不自洽** ⇒ ⇒ 且与 `DUAL_BACKEND_TEST_PAIR` 的 **WARN-skip** 约定不一致 ⇒ ⇒ macOS/Windows 的 `-DBUILD_VERILATOR=OFF` 本地开发会**新增硬失败** | ✅ **已修**：**3 个** T1 用例加 `ch_test::tool_available("verilator")` 守卫 ⇒ ⇒ 不可用 ⇒ **WARN + return**（★ 同 `DUAL_BACKEND_TEST_PAIR` 的约定） |
| SF3 | SHOULD_FIX | lint 口径是 `verify_hierarchical_rtl.sh` R1 的**复制**⇒ ⇒ 两处将来会**漂移** | ✅ **已加交叉引用注释**（★ 权威 = R1；抽共享 helper 记为后续项） |
| SF4 | SHOULD_FIX | 「带子组件」判定口径**可靠但仍是启发式** ⇒ ⇒ 已知边界未登记 | ✅ **已登记**（护栏头注释）：**误报方向**（列 0 `};` 截断 ⇒ 吵不漏）· **漏判方向**（`describe()` 定义在类体外 / 宏生成 ⇒ 真洞）⇒ ⇒ ⇒ 「召回率 100%」**不可**被误信 |
| N1 | NIT | 去重的 `break` ⇒ 两个同名 call-site 只挂第一个 ⇒ ⇒ 第二个从 ③ 的视野里**消失** | ✅ **已修**：收集**全部**同名 call-site |
| N2 | NIT | 「证明**整条**链」措辞**强于**证据（仅 3 个用例转红） | ✅ **已改**为「**发射器 → fixture → 断言**整条链有判别力」并在 §C 标注证据边界 |
| N3 | NIT | 消费方计数不一致（fixture「第二个」vs registry「第三个」） | ✅ **已统一**（三个消费方在三个不同文件 ⇒ ⇒ 三方证据） |

---

## 实施期实测（证据）

### A. 覆盖清单（**10 个复合 DUT** · 护栏的分析器**可复算**）

| 层 | DUT | 位置 |
|---|---|---|
| **T0 产物级**（9） | `Rv32iPcDut` · `ClintDut` · `CsrBankDut` · `StageDut` · `ChainDut` · `ModuleValueTransferTop` · `ModuleChainTop` · `ModuleIoDirectionTop` · `ICachePrefetch` | 5 个既有语料文件内 |
| **T0 拒绝边界**（1） | `ImmConsistencyDut` | `tests/test_rv32i_stage_dual.cpp` |
| **T1 行为级**（3 结构类） | C2 `ModuleChainTop` · C1' `ModuleIoDirectionTop` · C3 `ICachePrefetch` | 同上 |

⇒ ⇒ 护栏 `scripts/verify_hier_corpus_coverage.sh` 的分析器输出
「登记 **10** · 复合 DUT **10** · 用例 **10**」⇒ ⇒ 与清单**一致**。

### B. ⚠️ **实施期的实测订正**（planner 普查的结论**被实测推翻**）

⚠️ planner 普查说 `ImmConsistencyDut`（「双子组件各自带寄存器」）是 T1 的 C1 类代表
⇒ ⇒ **实测：它**不可**层次化** ⇒ ⇒ `io().equal <<= (dec.io().imm == ids.io().imm);`
⇒ ⇒ 根的 `equal` 由**两个子组件的内部线网**驱动 ⇒ ⇒ 那两条边**需要隐式端口合成**
（ADR-060 §2.1）⇒ ⇒ **发射器不支持** ⇒ ⇒ `ok=false`。

⇒ ⇒ **订正 ①**：它**不是** T0 候选 ⇒ ⇒ 而是「**拒绝边界**」的**真实样本** ⇒ ⇒ 用例把
**拒绝原因**钉住（错误串 MUST 含 `implicit-port synthesis`）⇒ ⇒ ⇒ 将来发射器长出该
能力 ⇒ ⇒ 用例会红 ⇒ ⇒ ⇒ 那是「能力边界前移」的**正确信号**。

⇒ ⇒ **订正 ②**：T1 的「双子组件**各自带寄存器**」这一结构类**在既有语料里没有可用样本**
⇒ ⇒ ⇒ 改用「双子组件（**组合逻辑**）」作代表（`ModuleIoDirectionTop`）⇒ ⇒ ⇒ **backlog**
（★ 不得用「合成一个 DUT」来凑 ⇒ ⇒ 那会回到 T3-005 的「专用 DUT」形态 ⇒ ⇒ 缺口原样留存）。

### C. 变异：**1 条系统级转红 + 4 条检测器级**如实登记为无效**

⚠️ **4 条「检测器级」变异恒绿 ⇒ ⇒ 这是「谁来测测试」的经典自指限制**：测试断言的就是
fixture 的**自报** ⇒ ⇒ ⇒ 把 fixture 的自报恒置为「好」⇒ ⇒ 测试**无从察觉**
⇒ ⇒ ⇒ 它们 MUST **如实登记为无效变异** ⇒ ⇒ ⇒ 而「判别力」只能由**被测对象**的变异证明。

| 变异 | 层级 | 实测 | 判定 |
|---|---|---|---|
| **S1** | ★ **系统级**：`Granularity::ForceLayer` 的语义被破坏（`src/component.cpp` 的 `is_layer()`）⇒ ⇒ 层次化产物不再分层 | ✅ **转红**（`121 assertions / 3 failed`） | ⇒ ⇒ ⇒ **证明「发射器 → fixture → 断言」整条链有判别力** |
| M1 | 检测器级：去掉「确定性」检查 | ❌ 恒绿 | ⇒ ⇒ **无效变异**（自指限制） |
| M2 | 检测器级：去掉「≠ 扁平产物」检查 | ❌ 恒绿 | ⇒ ⇒ **无效变异**（自指限制） |
| M3 | 检测器级：去掉「端口集自洽」检查 | ❌ 恒绿 | ⇒ ⇒ **无效变异**（自指限制） |
| M4 | 检测器级：lint 结果恒置为通过 | ❌ 恒绿 | ⇒ ⇒ **无效变异**（自指限制） |
| M5 | 护栏级：去掉第 ③ 个方向 | ❌ 恒绿 | ⇒ ⇒ **无效变异**（★ 它是 `self-test` 投毒③ 的**反向对照** ⇒ ⇒ ⇒ 证明「③ 方向」是抓住「新增未登记的复合 DUT」的**唯一**机制） |

### C-ter. **critic 第 2 轮复审**（只验 BLOCKER 闭合）⇒ **APPROVE_WITH_CONDITIONS**

- **结论**：`mergeable: yes` · `scope_exceeded: false` · **0 BLOCKER** · 1 SHOULD_FIX + 2 NIT
- **B1 闭合判定**：机械根因已被**三点**实质移除 ⇒ ⇒ ① 按 section 解析 · 豁免 section 行以
  **名字**键控 ② 方向 ③ 接受 exempt 名字键 ③ **投毒 ④** 对「未登记 + 无用例 + 已豁免」的
  **合成**复合 DUT **机械走通为绿** ⇒ ⇒ ⇒ **delta 的两条出路第二条现在真实存在**
- **SHOULD_FIX（已处置）**：记录**夸大了实测结论** ⇒ ⇒ ⇒ **已实测证伪**并按实测口径改写
- **NIT（已处置）**：③' 的键控不一致（`exempt` 按**简单名**键控 · ③' 却与 COMPOSITE 行**全行**
  比对）⇒ ⇒ ⇒ 若未来语料出现 `::` 限定的复合 DUT 调用 ⇒ ⇒ 合法豁免会被 ③' **误杀**
  ⇒ ⇒ ⇒ 当前语料**零**限定名调用 ⇒ ⇒ **已登记**为潜伏边
- **NIT（登记为后续硬化项）**：delta 的「豁免 MUST 附**具体**原因」**一半**仍无机器核验
  ⇒ ⇒ ⇒ **第 1 轮即存在**、未作为 BLOCKER 上报 ⇒ ⇒ ⇒ **不重开** ⇒ ⇒ ⇒ 登记为**后续硬化项**

### C-bis. ⚠️ **critic 第 1 轮的 BLOCKER 处置**（豁免机制）

⚠️ critic 指出：delta Requirement ① 承诺「覆盖 **或** 显式豁免」**两条出路**，但**豁免那一条
机械上不可用** ⇒ ⇒ 分析器把豁免表里的 **DUT 名**行当成**登记** ⇒ ⇒ 方向 ① 会要求它有用例
⇒ ⇒ ⇒ 只剩一条出路 ⇒ ⇒ ⇒ **契约会被冻成「无解的红」**。

⇒ ⇒ **实测复现**（critic 的判断成立）：把一个**复合** DUT 从「登记清单」**整行删掉** +
放进「豁免表」⇒ ⇒ **仍红**（方向 ② + ③ 双双触发）⇒ ⇒ ⇒ 豁免路径**确实**不可用。

⇒ ⇒ **修复**：① 分析器改为**按 section 解析**（只有「豁免」section 的行是豁免 ⇒ ⇒ 其余
默认**登记** ⇒ ⇒ 「放错 section」**不会静默消失**）② 新增方向 **③'**：豁免项 MUST **确实**
存在于语料的复合 DUT 集合里 ⇒ ⇒ ⇒ 「豁免表不是垃圾桶」。

⇒ ⇒ **自检新增 2 条投毒**：④ 合成语料 + 豁免它 ⇒ ⇒ **必须绿**（⇒ ⇒ 证明第二条出路可用）
⑤ 豁免一个**不存在**的 DUT ⇒ ⇒ **必须红**（⇒ ⇒ ③' 生效）。

⇒ ⇒ 还原后：`tests/hier_corpus_fixture.h` · `scripts/verify_hier_corpus_coverage.sh` ·
`src/component.cpp` **sha256 逐字节相同** + **重建成功** + **复绿**。

⚠️⚠️ **豁免的语义边界（★ 第 2 轮 critic 指出并已实测确认）**：
**豁免 = 「不覆盖」** ⇒ ⇒ ⇒ 被豁免的 DUT MUST **没有**语料回归用例 ⇒ ⇒ ⇒ 因为
「有用例却不在登记清单」**必然**触发方向 ②（⇒ ⇒ 红）⇒ ⇒ ⇒ 那**不是**豁免机制坏了
⇒ ⇒ ⇒ 而是「豁免」与「已覆盖」**语义矛盾**⇒ ⇒ ⇒ 红是**正确**的。
⇒ ⇒ ⇒ 故闭合证据是**投毒 ④ 的合成形态**（未登记 + 无用例 + 已豁免 ⇒ ⇒ 绿），
**不是**「删掉一个**有用例**的 DUT 的登记行」那个形态（⇒ ⇒ 仍红 ⇒ ⇒ 正确）。

### D. 护栏的 `--self-test`（**5 条投毒** + 反向对照）

| # | 投毒 | 抓它的方向 | 实测 |
|---|---|---|---|
| ① | 登记表**删一行** | ① + ② 联合 | ✅ 红 |
| ② | 登记表**加一个不存在的 DUT** | ① | ✅ 红 |
| ③ | ★ **新增一个未登记的复合 DUT**（合成语料文件，**不**污染仓库） | **只有 ③**（① 要「登记了但没用例」；② 要「有用例但没登记」⇒ ⇒ **都抓不到**） | ✅ 红 |
| ④ | ★ **豁免可用**（合成语料 + 豁免它 ⇒ ⇒ 未登记 + 无用例 + 已豁免） | **验证 delta 的第二条出路真实存在** | ✅ **绿**（⇒ ⇒ 路径可用） |
| ⑤ | **豁免一个不存在的 DUT** | ③'（豁免表不是垃圾桶） | ✅ 红 |
| — | 还原 | — | ✅ 复绿 |

### E. 零回归

- `ctest -LE perf` 全绿 · samples 全绿 · examples 全绿
- **计数锚点全部不变**：`CTEST_TOTAL` **253** · `VERILATOR_E2E_TOTAL` **77** ·
  parity **66 / 289** ⇒ ⇒ ⇒ 新用例进**既有** target ⇒ ⇒ **不**产生新 ctest 条目
  ⇒ ⇒ ⇒ **零**锚点变动（★ tasks.md 5.2 / 5.3 的口径）

---

## 验证命令

```bash
cd /workspace/project/CppHDL
set -euo pipefail
cmake --build build -j"$(nproc)"
( cd build && ctest -LE perf --output-on-failure )
bash scripts/run_all_samples.sh
./run_all_ported_tests.sh
bash scripts/verify_hier_corpus_coverage.sh
bash scripts/verify_hier_corpus_coverage.sh --self-test
bash scripts/verify_hierarchical_p8.sh
bash scripts/verify_hierarchical_p8.sh --self-test
bash scripts/verify_dual_backend_parity.sh
bash scripts/verify_test_counts.sh
openspec validate hierarchical-corpus-regression-2026-11 --type change --strict
bash scripts/verify_openspec_delta.sh
```
