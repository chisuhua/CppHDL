# CppHDL Roadmap

## 元信息
- **版本**: 1
- **创建时间**: 2026-10-19
- **最后更新**: 2026-10-27
- **当前阶段**: phase-5
- **当前 Sprint**: sprint-2026-10

## Phase Skeleton
| Phase | Theme | Status | Started | Done |
|-------|-------|--------|---------|------|
| phase-1 | 核心抽象与算子基础设施 | archived | 2026-05 | 2026-05 |
| phase-2 | 高级特性与 JIT 编译器 | archived | 2026-05 | 2026-06 |
| phase-3 | Verilator 后端集成与 Verilog codegen | archived | 2026-06 | 2026-09 |
| phase-4 | dual-backend 测试完整化 + cache 子系统重建 | archived | 2026-10 | 2026-10 |
| phase-5 | riscv-mini 收敛 + SoC 双路 parity + RV32I ISA 语义边界 + 平台矩阵 | active | 2026-10 | |
<!-- AUTO-INDEX -->

## Fragment Index (auto-generated)

### Phases
- `phase-1` — 核心抽象与算子基础设施
- `phase-2` — 高级特性与 JIT 编译器
- `phase-3` — Verilator 后端集成
- `phase-4` — dual-backend 测试完整化
- `phase-5` — riscv-mini 单源化收敛 · RV32I 双路 parity · ISA 语义边界 · RTL 缺陷收口

### Features
- `feat-cache-rewrite` — cache 子系统从 C-style 数组重写为 ch_mem（ADR-039）+ Sync/Async 双变体（ADR-041）+ stub 物理退役（ADR-037） (refs: phase-4, phase-5)
- `feat-dual-backend-coverage` — 单元测试统一跑 interpreter + verilator 双后端（5 阶段推进） (refs: phase-4, phase-5)
- `feat-hdl-logical-deprecation` — ADR-036 逻辑算子 deprecation 标记 → 全量迁移 → 移除（removal gate） (refs: phase-4, phase-5)
- `feat-platform-matrix` — CI 平台矩阵扩展（macOS-14 已启用 → Windows-2022） (refs: phase-3, phase-5)
- `feat-riscv-mini-convergence` — 消除 riscv-mini 技术债 + 把 include/cpu 与 examples/riscv-mini 收敛成单一 RV32I 实现 (refs: phase-5)
- `feat-riscv-mini-rtl-debt` — riscv-mini 实测挖出的两个 RTL 缺陷收口（cache wrapper 分支从不发射 · CLINT mtime 不自增） (refs: phase-5)
- `feat-rv32i-integration` — RV32I 5-stage 流水线 + riscv-mini SOC 端到端集成（双路 parity + JIT 回落断言） (refs: phase-5)
- `feat-rv32i-isa-completeness` — RV32I 语义边界收口（SB/SH 写字节道 · LB/LH 读字节道 · JAL/JALR 重定向 · SRA）+ 流水线健壮性（skid buffer · 取指双装） (refs: phase-5)

<!-- AUTO-SPRINT-START -->
## Current Sprint: sprint-2026-10

| Project | Phase | Priority | Feedback | Proposal |
|---------|-------|----------|----------|----------|
| RV32I ISA ⑤ load 字节道固件验证 | phase-5 | P0 | none | ✅ **已归档**（2026-10-28）：`openspec/changes/archive/2026-10-28-rv32i-load-byte-lane-2026-10/`（⚠️ **原计划是「只验证」⇒ 实施期发现 `LB` 符号扩展是**错的** ⇒ 改为「**含 2 行实现修复**」；spec delta = `rv32i-isa-semantics` ADDED ×1 ⇒ 归档**禁** `--skip-specs` ⇒ 主 spec 合并 +1 条）⇒ **`feat-rv32i-isa-completeness` ⑤ 收口** |
| cache wrapper 活逻辑（T8 ①） | phase-5 | P2 | none | ✅ **已归档**（2026-10-28）：`openspec/changes/archive/2026-10-28-riscv-mini-cache-wrapper-live-logic-2026-10/`（⚠️ **既有断言确实转红** ⇒ 反转断言与修实现**同 commit**；spec delta = `cpu-cache-subsystem` MODIFIED ×2（**路径订正**）+ ADDED ×1 ⇒ 归档**禁** `--skip-specs`）⇒ **`feat-riscv-mini-rtl-debt` ① 收口** |
| T7 可 dual 上限盘点 | phase-5 | P1 | none | ✅ **已归档**（2026-10-28）：`openspec/changes/archive/2026-10-28-dual-backend-full-coverage-inventory-2026-10/`（**只读**；**172** 行盘点表）⇒ ⚠️ **决策门已触发**：**诚实**可达上限口径 A ∈ [**71**, **96**] / 口径 B ∈ [**62**, **87**] ⇒ **两种口径的乐观上限都 < 100** ⇒ ⇒ **判据在现有语料内不可达** ⇒ **MUST 由 planner** 修订 `objective-test-coverage-100pct` 的 Go 判据（**取舍集 4 个轴**见该 change 的 `design.md` §9.2.2；含 **3 处被证伪的前提**；⚠️ **口径 A 的技术上限其实无界** ⇒ 71/96 是「语义诚实」的上限）⇒ 已立项 **`.rddf/improvements/dual-backend-criterion-revision-2026-10.md`**；✅ **2026-10-30 planner 已裁决**：采纳**轴 ②（改度量单位）+ ①（阈值下调）**、**否决轴 ④**（改 capability 契约 ⇒ 独立立项）、**轴 ③ 保留为 Phase 5 正题** ⇒ 新判据 = 「**双后端 parity 源文件数 ≥ 60**」（现状 **29** / 诚实保守上限 **62** / margin 2）⇒ **capability 零 MODIFIED** ⇒ **无需 OpenSpec change** ⇒ 机械锚点 = 根 `AGENTS.md` 的 `DUAL_BACKEND_PARITY_TOTAL` + `scripts/verify_test_counts.sh` 的对应检查 |
| roadmap 簿记一致性护栏 | phase-5 | P0 | none | ✅ **已归档**（2026-10-28）：`openspec/changes/archive/2026-10-28-roadmap-fragments-consistency-guard-2026-10/`（新增 capability `roadmap-bookkeeping-consistency` ⇒ **未**用 `--skip-specs`）⇒ `scripts/verify_roadmap_fragments.sh` + CI 2 处挂载 + `--self-test` 13 fixture |
| perf baseline 重采集 | phase-5 | P0 | none | ✅ **已归档**（2026-10-28）：`openspec/changes/archive/2026-10-28-perf-baseline-refresh-2026-10/`（**3 次采样取逐行中位数** ⇒ `git_sha=0758fa0`；MODIFIED ×1 ⇒ **未**用 `--skip-specs`）⇒ `perf_regression` 恢复为「Compared 18 rows ⇒ No regressions」 |
| `uart_tx` FSM 真正工作 | phase-5 | P2 | none | ✅ **已归档**（2026-10-29）：`openspec/changes/archive/2026-10-29-uart-tx-fsm-live-logic-2026-10/`（**首次实现**一个从未工作的 FSM + 自检断言 ⇒ 该 example 从「恒绿」变「可红」；⚠️ **正确形态的 5 条纪律**：条件只用硬件原语 · 每寄存器**单** next 写入点 · 每输出**只**驱动一次 · **`transition_when` 条件电平敏感**（须与计数器合取）· 不把 parity 当正确性证据） |
| SPI fixture 真正工作 | phase-5 | P2 | none | ✅ **已归档**（2026-10-29）：`openspec/changes/archive/2026-10-29-spi-fsm-fixture-live-logic-2026-10/`（`test_verilator_complex_e2e.cpp` 的 SPI fixture **从未真正工作过**（`shift` 恒 `0x00` / `STOP` 永不可达 / `mosi` 恒 0 / `bit_count` 从不复位）且断言**只有 parity** ⇒ 两后端**同构** ⇒ 长期「假绿」⇒ 本 change 用**正确形态**首次实现它 + 把断言升级为**手推语义金标**；spec delta = `dual-backend-test-coverage` MODIFIED ×1（R9 增补第 **3** 条例外 ⇒ **补登记**该文件历史上的 drift）+ `verilator-e2e-coverage` MODIFIED ×1 ⇒ 归档**禁** `--skip-specs`；⚠️ **判别力踩坑 2 处**：① `0xA5` 是**回文** ⇒ 位序维度零判别力 ⇒ 改 `0x96`；② 单次传输 ⇒ 「装载时复位计数器」**不可观测** ⇒ 改 2 次传输；⇒ **变异 9/9 全被抓** + 「旧 fixture + 新断言 ⇒ 转红」） |
| FSM 语义契约 + 静态护栏 | phase-5 | P2 | none | ✅ **已归档**（2026-10-29）：`openspec/changes/archive/2026-10-29-chlib-fsm-semantics-2026-10/`（**新** capability `chlib-state-machine-semantics`（ADDED ×6）⇒ 归档**禁** `--skip-specs`；⚠️ 起因：**同一缺陷在仓内两次真实发生**（`uart_tx` 的 FSM · `test_verilator_complex_e2e.cpp` 的 SPI fixture）且两次都被「**只有 parity**」的断言掩盖 ⇒ 长期「假绿」⇒ ⇒ 处置 = **契约 + 框架文档 + 静态护栏**（⚠️ 只读盘点确认**当前仓内已无受害站点** ⇒ 不是存量 bug ⇒ 是「**框架的诱导性设计**」⇒ 框架头注释**自己的示例**就是坏写法）；⇒ 新护栏 `scripts/verify_fsm_next_wiring.sh`（分析器抽成 `scripts/lib/fsm_wiring.py` ⇒ 护栏与 fixture **共用同一份**）**12/12 fixture** + ⭐ **对真实历史缺陷有效**（换回旧 `describe()` ⇒ 报出 **5** 处 `reg-next-in-closure`）+ CI **2 处**挂载；⚠️ **待裁决（architect）**：`ch_reg` 的「多源 next」⇒ 需 ADR ⇒ 决策材料见该 change 的 `design.md` §5 ⇒ **本 change 是它的前置**） |
| FSM **状态动作语义**（ADR-050 落地） | phase-5 | P1 | none | ✅ **已归档**（2026-10-30）：`openspec/changes/archive/2026-10-30-chlib-fsm-action-semantics-2026-10/`（实施 `docs/adr/ADR-050-chlib-state-machine-semantics.md` ⇒ **2 轮对抗审稿**：REJECT → APPROVE_WITH_CONDITIONS（0 BLOCKER））；⚠️ **决策在 ADR，不在 change** ⇒ **公开 API 缩减**（15 符号 + `current_state()`→`entry_state()` 改名）+ **新增贡献 API**（`sm.contrib` / `sm.resolve_hold` / `sm.resolve_or` ⇒ 框架只合成**一条** select 链、**用户写一次**）⇒ **`core/` 与 `ch_reg`/`ch_out` 公开契约零变化** ⇒ capability `chlib-state-machine-semantics` **MODIFIED ×1**（第 ⑥ 条；第 ① 条零 MODIFIED）⇒ 归档**禁** `--skip-specs`；**两个 commit**（① 移除+硬化 ② 贡献 API+dogfood：`uart_tx.cpp` + `test_verilator_complex_e2e.cpp` ⇒ **既有金标逐项不变** ⇒ 逐位等价）；⚠️ **实施期实测 3 条**：`CHERROR`/`CHFATAL`/`CHREQUIRE` **只打日志** ⇒ 检出**必须**用 **`CHFATAL_EXCEPTION`**；`= delete` 在 SFINAE 下是**硬错误** ⇒ 编译期拒绝改用「**无可用重载**」形态（才可自动化断言）；析构函数默认 `noexcept` ⇒ 须 `noexcept(false)`；⇒ **变异 7/7 全被抓**（M4/M6 第一轮恒绿 ⇒ 定位到**两个真实缺口**：default 路径**零覆盖** ⇒ 补 `DefaultTailDut`；「未 resolve」在**析构**时抛 ⇒ **掩盖**被测检查 ⇒ 补 resolve 隔离）） |
| 输入端口驱动护栏（spike） | phase-5 | P1 | none | ✅ **已归档**（2026-10-28）：`openspec/changes/archive/2026-10-28-input-port-driving-guard-2026-10/`（**只出文档**；零代码改动）⇒ ⚠️ **决策门结论：通用状态型护栏关闭**（实测：① 运行期**无法识别端口身份** ⇒ 诊断不可行动；② **语义判定缺承重件**（「从未被驱动」≠「应该被驱动」⇒ 需「必需端口注册表」，规范层已明文要求）且 **per-TEST-CASE 是必需粒度**而平规则在该粒度必然误报 ⇒ **死结**；③ 驱动藏在 29 个 helper + 186 次宏里 ⇒ 需过程间分析）⇒ ⇒ **义务继续留在 `AGENTS.md §ANTI-PATTERNS`** ⇒ ⇒ 3 个后续已立项：`input-port-driving-event-diff-check-2026-10`（**事件驱动增量检查**，实测新增端口负载 0–2/commit）/ `input-port-required-registry-2026-10`（**必需端口注册表**，规范层已承认需要）/ `input-port-assignment-antipattern-fix-2026-10`（**10 处存量静默失效**，独立可做） |
| CLINT `mtime` 自增（T9 ②） | phase-5 | P2 | none | 🟡 **实施完成，待验收归档**（2026-10-30）：change `clint-mtime-autoincrement-2026-10`；✅ **实施**：`mtime` 从「只能由软件写」的**静态比较器** → **64 位自由运行计数器**（形态 B：单一 64 位 `ch_reg` + `bits_update<32>`，ADR-049 D6 推荐）；**编译期** `TIMER_DIVIDER`（默认 1）；**写优先**（写的那一拍取写值 · 另一半保持 · **本拍不递增**）；**分频相位 = 自复位以来的拍数** ⇒ 写拍**照常推进**分频器（写拍落递增边界 ⇒ 递增被丢弃但**相位不变**）；复位值 0（`mtimecmp` 保持全 1）⇒ 复位后不触发中断；64 位读**允许撕裂**（不新增端口）；`Clint` ⇒ `Clint<>`（**纯类型名**，2 处实例化点）⇒ ⇒ ⚠️ **实施期实测新发现第 3 条陷阱**：`bits_update<32>` 在 64 位 target 上**返回 64 位值** ⇒ 用 32 位接收 ⇒ **静默截断** ⇒ 「写高半」**完全失效**（探针实测：写 hi=7 ⇒ mtime 仍 0x0）⇒ ⇒ 已写入 capability 契约⇒ ⇒ ⚠️ **读时序的三个实测事实**：① `read_data` 是**组合**输出 ⇒ 其值在**时序更新之后**求值 ② **读的那一拍本身会让 `mtime` 递增** ⇒ ⇒ **任何** tick 都推进分频相位 ③ ⇒ ⇒ **「写-读同拍」读到精确的写值**（写的那一拍不递增）⇒ ⇒ **消除时序脆弱性**的关键手段⇒ ⇒ ✅ **判别性用例 8 个**（V1 递增 · V2 **跨 32 位边界** · V3 分频相位 · V4 相位不被写复位 · V5 写拍落递增边界 · V6 写优先三项 · V7 写后继续 · V8 复位后 mtip=0）⇒ ⇒ ✅ **变异 7/7 全被抓**（M1 C++ 三元 · M2 位段位置写错 · M3 去写优先 · M4 忽略分频 · M5 写复位分频器 · M6 删递增源 · M7 32 位接收）⚠️ **如实登记**：ADR-049 §5.4 的 M2（手写 `hi|lo` 漏移位）是**形态 A 专属** ⇒ 形态 B 下**该路径不存在** ⇒ 改用等效变异（位段位置写错）⇒ ⇒ 验收：`ctest -LE perf` 零回归 · `test_interrupt_flow` **18 用例 / 68 断言** 全绿
| `conditional_assign` 接受裸 literal`（N9，2026-10-30） | phase-5 | P1 | none | ✅ **已实施 + 已归档**（2026-10-30，`openspec/changes/archive/2026-10-30-conditional-assignment-literal-2026-10/`）：⚠️ **起因**：`if_stmt.h` 的**头注释示例**（`conditional_assign(count).next() = 0_d;`）**照抄就编译不过** ⇒ ⇒ **框架内的真实影响**：`test_if_stmt.cpp` 的**全部**既有 DUT 都写 `ch_uint<8>(7_d)`（**显式包裹**）⇒ ⇒ 那是**绕过**缺陷的写法 ⇒ ⇒ 缺陷**从未被触发**⇒ ⇒ **根因**：`assign()` 的 else 分支（**块外直写**）写 `(*out_) <<= value;` ⇒ 走 `operator<<=` ⇒ 用 `get_lnode(driver)` ⇒ ⚠️ `ch_literal_impl` **不是** `HardwareType`（**无** `impl()`）⇒ ⇒ **没有** `get_lnode` 重载 ⇒ ⇒ **放大机制**：`if`/`else` **都是模板代码** ⇒ ⇒ **两条路都被实例化** ⇒ ⇒ 「能不能编译」与「走哪条分支」**无关**⇒ ⇒ **修复**（**1 行**，**零** `core/` 改动）：else 分支经 **`to_operand`** 归一化⇒ ⇒ ✅ **两层判据**（编译期 fixture 4 种组合 + 对照 · 运行期 2 个用例）⇒ ⇒ ✅ **新护栏** `scripts/verify_conditional_assign_integrity.sh`（R0 fixture 存在性 · R1 接受面 · R2 判别力 · R3 两条路径都归一化）+ CI 两处⇒ ⇒ ⚠️ **护栏自身也踩了盲区**（已修）：初版无「fixture 必须存在」检查 ⇒ ⇒ fixture 缺失时编译器报「No such file」⇒ ⇒ 被误判为「编译失败」（**假绿**）⇒ ⇒ **两条判据同时假绿** ⇒ ⇒ 已加 **R0（fail-closed）**⇒ ⇒ ✅ **变异 3/3**（M1 撤归一化 ⇒ 护栏红；M2 吞掉块外直写 ⇒ 运行期红；M3 删 fixture ⇒ R0 红）⇒ ⇒ **验收**：`ctest -LE perf` **194/194** · samples **36/36** · examples **28/28** · 护栏全 exit 0 · **计数不变**⇒ ⇒ ⚠️ **N7（块外裸直写覆盖条件合成）另立 change**（**预防性**护栏 ⇒ 实测**零受害者**）
| 共享守卫贡献的优先级契约（ADR-051 D10 订正） | phase-5 | P0 | none | 🟡 **已立项**（2026-10-30）：change `chlib-guard-contribution-priority-2026-10`；⚠️ **起因**：ADR-051 D10 把 ADR-050 的 `contrib` 与 `_if` 的条件贡献统一到**同一个**共享组合器 ⇒ ⇒ **链构造方向**由「正向状态序」变为「反向迭代」 ⇒ ⇒ **贡献优先级被静默翻转**（高编号 state 优先 → 低编号 state 优先）⇒ ⇒ ⚠️ **运行期不可观测**（状态守卫**互斥** ⇒ 同一时刻至多一个为真）⇒ ⇒ **既有金标 100% 绿** ⇒ ⇒ ⇒ **「绿」完全掩盖了它** ⇒ ⇒ 三个后果：① **契约缺口**（`chlib-state-machine-semantics` 从未写过优先级 ⇒ 翻转**不违反任何契约** ⇒ 无处登记）② **文档缺口**（D10 的「纯重构」表述不成立）③ **覆盖缺口**（链序**零覆盖** ⇒ 运行期钉不住 ⇒ 须**结构级断言**）⇒ ⇒ 顺带收口 `chlib-conditional-semantics` 的**析构兜底**与**表达式形式接受面限制**两处**无契约载体**⇒ ⇒ 口径裁决（用户）：**保留新行为**（=「先匹配者胜」，与 `_if`/`multi_if` 一致）⇒ 归档**禁** `--skip-specs` |
| `multi_if` 的接受面五形态（ADR-052 落地） | phase-5 | P0 | none | ✅ **已归档**（2026-10-30，`openspec/changes/archive/2026-10-30-multi-if-acceptance-surface-2026-10/`，实现 commit `c540980`）：change `multi-if-acceptance-surface-2026-10`；⚠️ **起因**：原以为「分支宽度不一致 ⇒ 模板推导冲突 ⇒ 编译不过」⇒ ⇒ **实测推翻**：编译通过、零告警、**静默丢位** ⇒ 探针连带挖出**五种**「框架放行却无法正确执行」的形态—— **A** 首窄后宽（静默丢高位）· **B** **单分支**（**条件被完全忽略**）· **C** `T`=port（模板深渊）· **D** `T`=纯算术（接受但不可用）· **E** 显式 `T` 过窄（静默截断）⇒ ⇒ **B 是「条件被忽略」家族第三实例**⇒ ⇒ **决策**（总原则：要么语义正确、要么响亮拒绝）：D1 宽度 = `width(T)` 更宽 ⇒ 编译期拒绝 · D2 `ch_literal`(sized) 静态判 / 裸算术(unsized) 建图期值域判 · D3 **单分支 ⇒ 拒绝** · D4 **port 归一化**、算术拒绝 · D5 结果类型恒为 `T` · D6 六种写法 MUST 合法 ⇒ ⇒ ✅ **实施期抓到真缺陷**：初版 `checked_value<T>(...)` 把显式实参绑到**函数**模板参数 `U` ⇒ **更宽的分支被隐式窄化** ⇒ **宽度判据根本没跑到** ⇒ **D1 形同虚设** ⇒ 由**编译期** fixture 抓出（运行期用例**看不见**「该拒却没拒」）⇒ ⇒ 新增 `scripts/verify_multi_if_width.sh`（R1 拒绝 / **R2 中性化对照** / R3 正向 / R4 源码级）+ `tests/chlib/test_multi_if_acceptance.cpp`（运行期）⇒ ⇒ ✅ **验收**：`ctest -LE perf` **194/194** · samples **36/36** · examples **28/28** · 护栏全 exit 0 · **变异 6/6** 全被抓 · **计数**：`CTEST_TOTAL` 204 → **205** · non-perf 193 → **194** · `tests/chlib/test_*.cpp` 30 → **31**
| `multi_if` 的 E1 / E3（输出可赋值性 + 条件可接受性） | phase-5 | P1 | none | ✅ **已实施 + 已归档**（2026-10-30，`openspec/changes/archive/2026-10-30-multi-if-output-assignability-2026-10/`）：**E1** `multi_if` **不能**直接赋给 `ch_out` ⇒ **编译不过**（`core/io.h:115` 的 `get_lnode(multi_if)` 无匹配）⇒ 而 `select` **可用** ⇒ 框架内不一致；**E3** `if_then` 的条件**不能**是 `ch_in<ch_bool>` ⇒ **编译不过** ⇒ 而 `_if` **可用** ⇒ 框架内不一致⇒ ⇒ ⚠️ **两条限制此前已被写进 capability**（两个 Scenario 明写「MUST 编译不过」）⇒ ⇒ 本次**修掉** ⇒ ⇒ 两个 Scenario **原地订正**（⚠️ **标题 MUST NOT 改** —— CLI 按标题匹配 Scenario ⇒ 改标题 = 丢弃旧 + 新增 ⇒ **拒绝归档**）⇒ ⇒ ✅ **E1 修法与关键机制（ADL）**：`core/io.h` 的 `get_lnode(value)` 是**非限定**调用 ⇒ **实例化点 ADL** 会找到 `chlib::get_lnode` ⇒ ⇒ **`core/` 零改动**；⚠️ 踩坑：写进 `namespace ch::core` ⇒ ADL **找不到** ⇒ 修复失效 ⇒ ⇒ `get_lnode` MUST 在 `namespace chlib` · `ch_width_impl` 特化 MUST 在 `namespace ch::core` ⇒ ⇒ 都放 `if.h` **末尾**；⚠️ 形参 MUST 是**具体类型** ⇒ **类型专用**入口 ⇒ 不违反 ADR-053⇒ ⇒ ✅ **E3 修法**：条件形参模板化 + 经 **`to_operand`** 归一化（**不新建节点** ⇒ 既有行为逐位不变）⇒ ⇒ ✅ **两层判据**（编译期 4 种组合 + 对照 · 运行期**逐位等价**断言 + 零扩展金标）⇒ ⇒ ✅ **新护栏** `scripts/verify_multi_if_output_assignability.sh`（R0–R3）+ CI 两处⇒ ⇒ ⚠️ **R1 与 R3 不可互替**（投毒后编译仍通过 ⇒ R1 抓不到 ⇒ 必须 R3）⇒ ⇒ ✅ **变异 2/2**（M1 删 `get_lnode` ⇒ R1+R3 红；M2 条件改回 `const ch_bool&` ⇒ R1+R3 红）⇒ ⇒ **验收**：`ctest -LE perf` **194/194** · samples **36/36** · examples **28/28** · 护栏全 exit 0 · **计数不变**
| `get_lnode` 入口集合收敛（ADR-053 落地） | phase-5 | P1 | none | ✅ **已归档**（2026-10-30，`openspec/changes/archive/2026-10-30-get-lnode-entry-set-2026-10/`，实现 commit `8a0db15`）：change `get-lnode-entry-set-2026-10`；⚠️ **起因**：两个**签名完全相同**的 `get_lnode(const T&)`（`lnode.h:24-28` 有约束 / `bundle_base.h:551-553` **无**约束）⇒ ⇒ ② **可证**「对有 `impl()` 者**死**（C++20 约束更严者胜 ⇒ ① 恒胜）、对无 `impl()` 者**有害**」⇒ ⇒ **两条独立证据**：① 报错**指向框架内部**（`if_demo.cpp` 实测 `bundle_base.h:552: 'multi_if<…>' has no member named 'impl'`）② **SFINAE 毒化**（返回类型显式 ⇒ `requires` 不实例化函数体 ⇒ 对无 `impl()` 类型返回 **true** ⇒ 能力探测假阳性）⇒ ⇒ **决策**：物理删除 ⇒ **通用**入口 = 1，MUST NOT 有无约束的**通用**入口 ⇒ 可执行护栏 **两层**（① 编译期 `static_assert` · ② 源码级扫描 `scripts/lib/get_lnode_entry_set.py`）⇒ ⚠️ MUST NOT 用 `= delete`（实测两编译器均为**硬错误**）或函数体 `static_assert`（真毒化）⇒ ⇒ ✅ **实施期实测订正**（**决策未变**）：① 「入口集合 = 2」**不准确** —— 实测活声明是 **3** 个（`uint.h:197-200` 的 `get_lnode(const ch_uint<N>&)` **实测生效**）⇒ ⇒ 契约精确化为「**通用**入口 = 1」② **只靠编译期判据会漏报**（它只看得到 fixture include 到的头文件 ⇒ 缺陷真正的所在地 `bundle_base.h` 若没被 include ⇒ **漏**）⇒ ⇒ 必须配**源码级扫描**（本仓「护栏自身有盲区」病症的新实例）⇒ ⇒ **验收**：`ctest -LE perf` **193/193** · samples **36/36** · examples **28/28** · 护栏全 exit 0 · **变异 2/2**（M1 恢复无约束重载 ⇒ R4 转红；M2 去掉受约束入口的 `requires` ⇒ R1+R4 转红）⇒ **计数**：`CTEST_TOTAL` 203 → **204** · non-perf 192 → **193** · `tests/test_*.cpp` 117 → **118**
| RV32I ISA ④ 取指 skid buffer | phase-5 | P0 | none | ✅ **已归档**（2026-10-27）：`openspec/changes/archive/2026-10-27-rv32i-fetch-skid-buffer-2026-10/`（ADR-048；一个 change、两个 commit = C1 skid / C2 `primed`）⇒ `rv32i-commit-path` MODIFIED ×2 ⇒ **`feat-rv32i-isa-completeness` ④ 收口** |

> ✅ **sprint-2026-10 又归档一项**（2026-10-27）：`dual-backend-rv32i-integration-2026-10`（P1）——
> RV32I 子组件的 dual-backend 覆盖补齐 **Batch 2/3**（`IfIdPipelineReg` ×2 + `BranchPredictor` ×1
> ⇒ 该文件 13 → **16** dual pair / 26 → 32 用例）；**变异 3/3 全被抓**（第 1 轮 critic 抓到的
> BLOCKER = Batch 2-b 第 1 拍漏驱动 `flush` ⇒ 已修；M3' 实测**零判别力** ⇒ 如实记录
> 「未驱动端口没有通用变异判别力 ⇒ 靠端口驱动表 + 代码评审兜住」）；
> **无 spec 变更**（`.openspec.yaml` 显式声明 `skip_specs`）⇒
> `openspec/changes/archive/2026-10-27-dual-backend-rv32i-integration-2026-10/`；
> `CTEST_TOTAL` 202 / `EXAMPLES_TEST_SOURCES` 17 / `ctest -L verilator_e2e` 37 **均不变**。
> ⇒ **`objective-rv32i-e2e` 的 Go 判据 6/6 全部达成 ⇒ 转 `completed`**。

> ✅ **sprint-2026-10 又归档一项**（2026-10-27）：`rv32i-forwarding-decision-2026-10`（P0）——
> ADR-047 裁决「接」数据前推：mux = 既有 `chlib::ForwardingMux`（**零新增组件**）+ 档位 select
> 打一拍 + 档位 1 源选择收进组件（`is_jump`/`jump_result`）+ `HazardUnit` 删 5 个预留端口 +
> **D11 寄存器堆读点 WB→ID 旁路**（修掉「距离 3 读旧值」的既有缺陷 —— 两后端**同构地错** ⇒
> 双后端 parity 抓不到）。第 1 轮对抗审稿 **APPROVE_WITH_CONDITIONS / 无 BLOCKER** ⇒ 按预算
> 不发起第 2 轮 ⇒ `openspec/changes/archive/2026-10-27-rv32i-forwarding-decision-2026-10/`，
> **新 capability `rv32i-data-forwarding`**（5 条 Requirement，距离矩阵的唯一权威）+ 4 个 MODIFIED。
> 变异 **6/6** 全被抓到；`CTEST_TOTAL` **202 不变**。

> ✅ **sprint-2026-10 又归档一项**（2026-10-27）：`rv32i-jump-sra-2026-10`（P0）——
> JAL/JALR 重定向 + 返回地址写回 + SRA 真算术右移，**连带修复 3 个既有缺陷**
> （U 型立即数 / `alu_op` 对 I 型的 SUB 误判 / 错误路径的 JAL·JALR 劫持 PC）。
> 2 轮对抗审稿（第 1 轮 **REJECT** 1 BLOCKER ⇒ 整改 ⇒ 第 2 轮 APPROVE_WITH_CONDITIONS）
> ⇒ `openspec/changes/archive/2026-10-27-rv32i-jump-sra-2026-10/`，
> capability `rv32i-isa-semantics` **2 → 6** 条 Requirement。

> ✅ **sprint-2026-10 本轮归档**（2026-10-26）：`example-assertion-quality-2026-10`（P1，
> 新 capability `example-assertion-quality`）与 `rv32i-store-byte-enable-2026-10`（P0，
> 新 capability `rv32i-isa-semantics`）—— 两者各经 **2 轮对抗审稿**（第 1 轮
> APPROVE_WITH_CONDITIONS ⇒ 条件项同批整改；第 2 轮 **APPROVE**）⇒ 均已归档。
> ⚠️ 两者共同的**合并门** = CI 的 `macos-14` job 绿（本机 Linux 不可完成）⇒
> 该 job 红 ⇒ MUST revert 归档 commit。

### Unmapped (1)

> 一个 improvement 草案尚无 change 目录（已归属 feature / objective，待立项）：
> `clint-mtime-autoincrement-2026-10` → `feat-riscv-mini-rtl-debt` ②。
> ⚠️ **2026-10-30 订正**：原段为「Unmapped (2)」，其中
> `riscv-mini-cache-wrapper-live-logic-2026-10` → `feat-riscv-mini-rtl-debt` ①
> **已于 2026-10-28 实施并归档**
> （`openspec/changes/archive/2026-10-28-riscv-mini-cache-wrapper-live-logic-2026-10/`，
> 实现 commit `0e117bb`）⇒ ⇒ **不是**「尚无 change 目录」⇒ ⇒ 本段减为 1；
> 该 draft 的 Status 亦已同步订正 ⇒ ⇒ 另：「名字不一致 ⇒ 立项时 MUST 统一」
> 这条遗留项**已随归档消解**（实际 change 名 = `riscv-mini-cache-wrapper-live-logic-2026-10`）。
> ⚠️ **CLINT 的决策门已解除**（2026-10-30，architect 落 **ADR-049**
> `docs/adr/ADR-049-clint-mtime-semantics.md` ⇒ D1–D5 **已裁决** + capability
> 归属已定 = **新建 `cpu-clint-timer`**）⇒ ⇒ **可立项**（planner 建 change，
> 带 ADDED delta ⇒ 归档**禁** `--skip-specs`）⇒ ⇒ **实施期 MUST 携带 ADR-049
> §1.4 的 3 条实测结论**（进位 MUST 用硬件原语 / `bits_update<32>` /
> `test_interrupt_flow` 须改「每次采样前重写 `mtime`」）。
>
> ✅ **sprint-2026-10 再归档一项**（2026-10-27）：`rv32i-fetch-skid-buffer-2026-10`（P0）——
> **RV32I 取指 skid buffer + 填充期 `primed` 门控**（ADR-048，一个 change、两个 commit）：
> C1 消除「load-use 停顿丢弃消费者之后第一条指令」（⇒ 固件**不再**需要在停顿点后插 NOP）；
> C2 消除「取指填充期同一条指令被装两次」（退休数 **179 → 178**、`non_retired` **20 → 21**，
> fill 段 2 → 3 拍，停顿/冲刷段不变）⇒ 同时消掉 ADR-047 之后升级成的**正确性**地雷
> （自依赖首条 + 前推 ⇒ 双装非幂等）；
> **变异 6/6 全被抓**（其中「删掉 flush 清空 skid」**只有组件级**能抓 ⇒ 与 ADR-048 D5 一致）；
> ⚠️ 实测补充：停顿后还有一次**重复装载** ⇒ C1 的「救回 1 条」与它**相抵** ⇒ C1 单独零观测面扰动。
> ⇒ `feat-rv32i-isa-completeness` ④ 收口（ISA 语义边界除 CSR/ECALL/FENCE/LB·LH 外全部落地）。

> ✅ **sprint-2026-10 已完成**：`axi4-interconnect-routing-2026-10`（P0）
> 于 2026-10-26 归档（`openspec/changes/archive/2026-10-26-axi4-interconnect-routing-2026-10/`），
> 新 capability `axi-interconnect-routing`（6 条 Requirement）。
<!-- AUTO-SPRINT-END -->

### Objectives
- `objective-cache-stub-retired` — cache stub 物理删除（ADR-037 deadline 2027-04-17）— **completed**
- `objective-riscv-mini-convergence` — 消除 riscv-mini 技术债 + 把 include/cpu 与 examples/riscv-mini 收敛成单一 RV32I 实现 — **completed**
- `objective-rv32i-e2e` — RV32I 完整 e2e 在 interpreter + verilator 双路 trace parity 通过（+ JIT 回落行为显式断言）— **completed**（Go 判据 **6/6** 全部达成，2026-10-27：判据 5「`dual-backend-rv32i-integration` archived」由 Batch 2/3 归档达成；判据 6「ISA 语义边界」由 ①②③ 达成 —— 范围注解见该 objective §2/§9.2）
- `objective-test-coverage-100pct` — 单元测试 100% 双后端（interpreter + verilator）覆盖 — **active**（⭐ **2026-10-30 判据修订**：完成判据改为「**双后端 parity 源文件数 ≥ 60**」，取代旧的「`ctest -L verilator_e2e` ≥ 100」—— 后者把 9 个**单后端**契约锁定参考实现算成「双后端覆盖」，且诚实上限 [71, 96] ⇒ **现有语料内不可达**；现状 **29** / 上限 **62**）
- `jit-mem-read-port-support` — JIT 对 `ch_mem` 读端口的 lowering（真三路 parity 前提）— **backlog，未建档**（当前 JIT 回落是有意设计且已断言；建档前置 = 出现必须 JIT-native 跑 RV32I 的需求）
- ~~`roadmap-fragments-consistency-guard` — roadmap fragments 一致性自检脚本 + CI 挂载~~ —— ✅ **2026-10-28 已实施并归档**（见上表）。⚠️ 实施期的**根因订正**：初稿写「CLI flag 存在、脚本 payload 缺失」⇒ **实测两头都反了** —— payload **存在可用**（工具链 `rdd-workflow` 的 `roadmap_update_agent_md.sh`），缺的是 **CLI dispatch**（`rddf roadmap` 没暴露 `--update-agent-md`）⇒ 工具**不可达** ⇒ AUTO 块只能手工维护。⇒ 护栏因此采用**与生成器同源的渲染规则**（渲染期望块 + 逐字比对），并用工具做**一次性 oracle** 等价性验证（工具链 SHA 已记录 + 再验证触发条件已写明）

> Sprint 编号口径：以本文件为准（`sprint-2026-10`）。三个 objective 的 append-only 台账曾把
> C1/C2/C3 归档记为 `sprint-2026-11`，属编号漂移 ⇒ 已在 `objective-rv32i-e2e` §11 追加澄清行
> （台账不可改写，只追加）。

