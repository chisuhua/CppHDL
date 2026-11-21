# Proposal: `.verif` 原语集扩展 —— 历史/序列判据（`verif-history-primitives`）

- **状态**: **立项（2026-11-21 · builder 受用户显式指令）** · 未开工
- **架构依据**: `docs/adr/ADR-076-verif-history-primitives.md`（**新独立 ADR** —— 扩展 ADR-074 D2 的封闭原语集）⇒ ⇒ ⚠️ **不改 ADR-074 的任何决策**（其 §7 头部已自禁「继续追加」⇒ ⇒ MUST 拆独立 ADR）
- **上游先例**: ADR-074 §7 dated addendum（`WAIT` 增补电平形参 = 同型「集合增补」）
- **用户裁决**（2026-11-21）：在「8 个已登记 target 全迁」的前提下，用户明确选择「**先扩展 `.verif` 原语集**」⇒ ⇒ 本 change = 该裁决的**前半段**（框架）；后半段（8 个 example 迁移）= 独立 change `spinalhdl-ported-verif-migration-2026-11`
- **推导与实现细则**: `design.md`（同目录）

---

## Why

`.verif` 的封闭原语集（ADR-074 D2）只能表达「**当前采样点**的观测」⇒ ⇒ **表达不了「对过去若干个采样点的关系」的观测** ⇒ ⇒ 三类 **metamorphic（相对判据）**在 `.verif` 里**不可表达**：

| 判据形态 | 实例 | 缺什么 |
|---|---|---|
| 自滞后恒等 | `counter_simple`「周期 256」 | 历史 + 「与 N 拍前那份逐项比」 |
| 相邻差 vs 他端口 | `counter`「Δ == enable」 | 历史 + 相邻差 + 与另一端口同拍值比 |
| 窗口计数 | `pwm`「256 拍内高电平数 == duty」 | 历史 + 窗口内等于某值的项数 |

⇒ ⇒ 降级成绝对判据 = **判据面实质收窄**（仓内对 metamorphic 的定位是「不依赖任何手推绝对期望值」）⇒ ⇒ 本 change 扩原语集，**保住判据面**。

⚠️ **为什么不加「万能原语」**：封闭集是 fail-closed 的前提 ⇒ ⇒ 每条新原语 MUST 有**单一、无歧义、两端逐字同构**的语义 ⇒ ⇒ 只加 **4 条**（`RECORD` + 3 条 `EXPECT_SEQ_*`），每条**都有实际消费者**（Zero-Debt：禁死代码 ⇒ ⇒ 不加无人用的 `EXPECT_SEQ_EQ`）。

---

## What Changes

### ① 新原语（4 条 · 语义见 `design.md` §2）

| 原语 | 语义 | 产生断言 |
|---|---|---|
| `RECORD <port>` | 把端口当前值（沿后结算后）追加到该端口的历史序列 | 0 |
| `EXPECT_SEQ_SELF <port> <n> <lag>` | 最近 n 项与再往前 lag 项**逐项恒等** | 1 |
| `EXPECT_SEQ_STEP <portV> <portE> <n>` | `portV` 的**相邻差**与 `portE` 的**同位项**逐项恒等（mod 2^64） | 1 |
| `EXPECT_SEQ_COUNT <port> <value> <n> <count>` | 最近 n 项中 == value 的项数 == count | 1 |

### ② 扫描期静态良构性（fail-closed · **两端同源**）

参数个数 · 取值域（`n≥1` / `lag≥1` / `count≤n`）· **32 位上溢** · 历史深度上限（1024）· **历史充足性**（到该语句为止的累计 `RECORD` 次数 ≥ 所需；`EXPECT_SEQ_STEP` **两个端口各判**）· **端口位宽 > 64 ⇒ fail-closed** ⇒ ⇒ **「历史不足」「深度超限」「宽端口截断」三类分叉在扫描期就被消灭** ⇒ ⇒ 两端读同一份 IR ⇒ ⇒ 不存在运行期分歧。

⚠️ **静态计数器生命周期**（oracle 闭合项 1 · **核心承诺的成立前提**）：计数器**一份**（**不是** per-scenario 局部）· **跨 scenario 不清空** · 按**展开后**的程序顺序累计 · key = **端口标识符字符串** ⇒ ⇒ **运行期历史长度恒等于静态计数**。

### ③ 四处同步（缺一即分叉）

| 处 | 落点 | 改什么 |
|---|---|---|
| IR | `include/rtl_emit/verif.h` | 4 个 kind + 5 个新字段（**不复用** `value`/`count` ⇒ 同 ADR-074 §7.2 的语义翻转坑） |
| 扫描 | `src/rtl_emit/verif_scan.cpp` | 新原语的解析 + 静态良构性 + `check_id` 分配 + `verif_check_count` 计入 |
| 解释 | `src/rtl_emit/verif_run.cpp` | 历史缓冲 + 4 条语义 |
| 渲染 | `src/rtl_emit/verif_render.cpp` | 历史数组声明 + `RECORD` 写入 + 3 条序列断言的渲染 |

⚠️ **失败形态**（实施期自检）：只改扫描 ⇒ 渲染器缺分支（若有 `default` 则**静默不发射** ⇒ 更危险）· 只改解释器 ⇒ `check_id` 集合不等但**值**不同抓不住 ⇒ ⇒ 四处 MUST 同批改；且解释器遇未知 kind 需 `default: error`（fail-closed）。

### ④ D8 证据义务（两执行后端同结论）

新测试源 `tests/test_verif_history_primitives.cpp`（标签 `verilator_e2e`）⇒ ⇒ 同一份 `.verif` 在 **IR 解释器**与**渲染 TB（真 verilator 编译运行）**各跑一次 ⇒ ⇒ 逐条 `(check_id, PASS/FAIL)` **对**比对相等 ⇒ ⇒ ⚠️ **不是**静态 id 集合比对（对序列敏感原语**结构性失明**）⇒ ⇒ 每条构例另带**手推期望**（禁抄仿真输出）⇒ ⇒ **变异自证**投在夹具的 `.verif` 输入上（不改实现代码）。

---

## Capabilities

**无 spec 变更** ⇒ `.openspec.yaml` 声明 `skip_specs: true` ⇒ 归档用 `--skip-specs`。⇒ ⇒ 既有 `examples-rtl-sim-coverage` 的 requirement（「C++ 侧 MUST 与生成式 TB 共享同一验证源」「期望源锚点 ↔ 验证源 scenario 双向 1:1」「断言条数三方等式」等）已完整覆盖本 change 的义务 ⇒ ⇒ 不写 delta。

---

## Impact

| 面 | 变化 |
|---|---|
| 库 | `include/rtl_emit/verif.h` · `src/rtl_emit/verif_{scan,run,render}.cpp` |
| 测试 | 新增 `tests/test_verif_history_primitives.cpp`（⇒ `CTEST_TOTAL` / `VERILATOR_E2E_TOTAL` / `tests/test_*.cpp` 计数**会变** ⇒ 按实测回填） |
| 既有产物 | **逐字节不变**（新原语不改动既有原语的渲染文本 ⇒ ⇒ Lite 的 checked-in `.sv` 与 `min_checks=113` 等式照常成立） |
| 清单 | **不动**（本 change 不迁任何 example ⇒ `interp_checks` / `min_checks` 全不变） |

---

## Out of Scope

| 项 | 理由 |
|---|---|
| **8 个 example 迁 `.verif`** | 独立 change（`spinalhdl-ported-verif-migration-2026-11`）⇒ ⇒ 依赖本 change 的原语 |
| **加「万能原语」/ 表达式化参数** | ADR-074 §7.4 的 C/D 行已否决（不动「生成期求值」不变量）⇒ ⇒ 若实施期发现不动该不变量就表达不了 ⇒ ⇒ **STOP** ⇒ 另立 ADR |
| **加 `EXPECT_SEQ_EQ`（两端口历史逐项恒等）** | **无消费者** ⇒ ⇒ fifo / uart_rx 的「逐项恒等」比较对象是**字面量序列** ⇒ ⇒ 绝对判据即可 ⇒ ⇒ Zero-Debt 禁死代码 |
| **改既有原语的语义** | ⇒ ⇒ 既有 checked-in 产物逐字节不变（AC） |

---

## 用户可见变更

| 项 | 前 | 后 |
|---|---|---|
| `.verif` 的原语集 | 9 条（ADR-074 D2） | **13 条**（+`RECORD` / `EXPECT_SEQ_SELF` / `EXPECT_SEQ_STEP` / `EXPECT_SEQ_COUNT`） |
| 历史序列 | **不存在** | 每端口一条（`RECORD` 追加 · 跨 scenario 累积 · 上限 1024 · 超限在**扫描期**即报错） |
| 历史不足 | — | **扫描期**非零退出（消息点名端口与所需/已有）⇒ ⇒ 运行期不出现「历史不足」分叉 |
| **宽端口（>64 位）** | 端口值被读成**低 64 位**（两端「大概都截断」） | 被 `RECORD` / 被序列断言引用的宽端口 ⇒ **fail-closed**（两端同源拒绝） |
| **sanitize 撞名** | — | 两个标识符 sanitize 成同一名字 ⇒ **fail-closed**（不静默共用一个历史数组） |
| `SET`/`RESET` 后紧邻 `RECORD` | — | **扫描期**非零退出（与 `EXPECT` 同纪律）⇒ ⇒ `EXPECT_SEQ_*` **不受**该纪律（只读历史） |
| **同一物理端口用两个标识符 `RECORD`** | — | 得到**两条独立历史** ⇒ ⇒ 文档 MUST 写明「一个物理端口只用一个标识符」 |
| 既有 `.verif` 与 checked-in `.sv` | — | **逐字节不变**（禁止重构渲染器，不是「结果等价」） |

---

## 验收标准

| # | 判据 | 方式 |
|---|---|---|
| AC1 | 4 条新原语可解析且 IR 字段正确；`RECORD` 不计入断言数；3 条 `EXPECT_SEQ_*` 各计 **1** 条 ⇒ ⇒ `verif_check_count` 随之变化 | 单测 |
| AC2 | 扫描器负向：参数个数错 / `n=0` / `lag=0` / `count>n` / **32 位上溢** / **历史不足**（含 `EXPECT_SEQ_STEP` 的**两个端口各判**）/ **深度超限**（**跨 scenario** 形态）⇒ **非零退出**且消息**点名**对应项 | 单测 |
| AC3 | 四处同步 ⇒ ⇒ 同一份 `.verif` 在**两端**给出**同一结论** | 运行期比对 |
| AC4 | **运行期** `(check_id, PASS/FAIL)` 逐条比对相等（⚠️ **不是**静态 id 集合比对）⇒ ⇒ 载体 = ctest（新测试文件 · `verilator_e2e` 标签）⇒ ⇒ 锚点 `VERILATOR_E2E_TOTAL` + `CTEST_TOTAL` 会变 | 新夹具 |
| AC5 | 每条新原语 ≥2 条构例（PASS + FAIL）+ **手推期望**（禁抄仿真输出） | 新夹具 |
| AC6 | **变异自证**：变异投在夹具 `.verif` 输入上 ⇒ ⇒ **点名清单**（`lag` 256→255 · `count` ±1 · `STEP` 两端口对调 · `TICK+RECORD` → `SET+RECORD` ⇒ 扫描期红 · `COUNT` 的 `value` 1→0）⇒ ⇒ 各自必红（两端同时）⇒ ⇒ ⚠️ **无效变异如实登记**（原候选「`STEP` 的 `n` 40→39」**恒绿** ⇒ 因该原语只产 1 条 check 且 `n` 减 1 只是窗口**严格子集** ⇒ ⇒ **不计入**判别力条数） | 新夹具 |
| AC7 | 既有 checked-in `.sv` **产物逐字节不变**（`min_checks=113` 等式仍成立）⇒ ⇒ 双证：⑥' 新鲜度 + `top.v` 的 sha256 | 护栏 + sha256 |
| AC8 | `verify_examples_rtl_sim.sh` **PASS** · `--self-test` **PASS** · `ctest` **全绿** · 锚点按实测回填 | shell |
| AC9 | **解释器遇未知 `VerifOpKind` ⇒ `default: error`**（fail-closed）⇒ ⇒ 不静默跳过 | 代码 + 单测 |
| AC10 | **X 态差异登记**：`EXPECT_SEQ_SELF`/`STEP` 的「X ⇒ FAIL」+ **`EXPECT_SEQ_COUNT` 的「X 项不计数 ⇒ 计数偏少 ⇒ FAIL」**（具体形态）⇒ ⇒ 两端对 X 不对称 ⇒ ⇒ **登记**（与既有 `EXPECT` 的 X 差异同型）⇒ ⇒ 本 change 不修 | 文档 |
| AC11 | **`RECORD` 的采样窗钉死** = 「沿后结算后」⇒ ⇒ 与 `EXPECT` 同窗 ⇒ ⇒ 构例：驱动后未结算就 `RECORD` ⇒ **扫描期**报错（⇒ 证明该纪律**有判别力**，不是摆设） | 单测 + 构例 |
| AC12 | **历史跨 scenario 不清空** ⇒ ⇒ 构例：scenario A 记录 · scenario B 比较 ⇒ PASS（证明不隔离）⇒ ⇒ 文档写明 | 构例 + 文档 |
| AC13 | **`EXPECT_SEQ_*` 不受 `SETTLE` 纪律约束**（只读历史）⇒ ⇒ 构例：`SET` 后**直接** `EXPECT_SEQ_COUNT` ⇒ 可解析且结论符合手推 | 单测 + 构例 |
| AC14 | **历史深度上限 = 1024** 且**扫描期**判定 ⇒ ⇒ 构例：单 scenario `REPEAT(1025)` ⇒ 扫描期非零退出 **且** **跨 scenario** 形态（600 + 425）⇒ 扫描期非零退出（⇒ 证明计数器**不是** per-scenario 重置） | 单测 |
| AC15 | **被 `RECORD` / 被序列断言引用的端口 width > 64 ⇒ ⇒ fail-closed**（渲染期已知 width · 解释器从**同一 manifest** 查 width ⇒ ⇒ 两端同源一致）⇒ ⇒ 负向构例 | 单测 |
| AC16 | **静态计数器生命周期钉死**：**一份** · 跨 scenario 不清空 · 按展开后程序顺序累计 · key = 标识符字符串 ⇒ ⇒ 「运行期历史长度 == 静态计数」⇒ ⇒ 超限与不足**都不可达** | 代码 + AC14 构例 |
| AC17 | **`EXPECT_SEQ_STEP` 的「同位」语义钉死** = 「`portE` 各自最近 n 项」+ **使用前提**（两端口 MUST 在**同一采样点集合**上 `RECORD` ⇒ 交错记录且次数相等）⇒ ⇒ 文档 MUST 警告 LV ≠ LE 时的语义含义 | 文档 + 构例 |
| AC18 | **sanitize MUST 单射** ⇒ ⇒ 撞名 ⇒ fail-closed ⇒ ⇒ 负向构例 | 单测 |
| AC19 | **「同一物理端口 MUST 只用一个标识符 `RECORD`」**（否则两条独立历史）⇒ ⇒ 文档写明 | 文档 |
| AC20 | **D5 锚点义务扩展**：序列类断言的推导层（`.md`）MUST 含 `lag` / `count` / `value` 的由来 ⇒ ⇒ **迁移 change MUST 落地** ⇒ ⇒ 本 change 登记范式 | 文档（范式） |
| AC21 | **复位尾缝继承登记**：场景首条 `RECORD` 继承 wait-level 已登记的 1 拍接缝 ⇒ ⇒ 夹具 MUST 沿用 `SET` + `TICK` 前缀范式 ⇒ ⇒ 文档登记 | 文档 + 构例 |
