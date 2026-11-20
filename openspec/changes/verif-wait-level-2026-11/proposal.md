# Proposal: `.verif` 原语集扩展 —— `WAIT` 增补电平形参（D2′ 实施）（`verif-wait-level`）

- **状态**: **立项（2026-11-20 · planner）** · 未开工
- **架构依据**: `docs/adr/ADR-074-rtl-verification-single-source.md` **§7 dated addendum**（D2′/D3′）——
  该节已由 oracle 第 1 轮 APPROVE + critic 第 1 轮 REJECT（3 BLOCKER）→ 全闭 → **复审 APPROVE**
  ⇒ ⇒ **决策面已定** ⇒ ⇒ 本 change 只**实施**它（**不改任何决策**）
- **用户追认**（2026-11-20）: 主题 A 拆**两个** change ⇒ ⇒ 本 change = **第 1 个**（原语扩展），
  第 2 个（SoC 迁 `.verif`）另立
- **前置**: `phase3a-rtl-product-2026-11`（**已归档 2026-11-20**）

---

## Why

蓝图 §7 **R13**（SoC 的 C++ 侧激励是手写过渡态）的闭口前提 = **`.verif` 原语集能表达 UART 8N1
帧级判据** ⇒ ⇒ 而 ADR-074 **D2** 的原语集**封闭** ⇒ ⇒ 「等目标电平的跃迁沿」这类构造**不可
表达**（现有 `WAIT` 只等**非零** ⇒ 对空闲高的 `uart_tx` 第一拍就返回 ⇒ 锚不住帧起点）。

⇒ ⇒ ADR-074 §7.2 已把解法定为 **D2′ = `WAIT` 增补电平形参**（集合增补，**不动** D1/D2 的
「生成期求值 ⇒ 产物只有字面量」不变量）⇒ ⇒ **本 change 实施 D2′**，并补上 §7.6 要求的
**D8 证据义务**（运行期时序判别性）。

⇒ ⇒ **为什么不是「一个 change 做完」**：SoC 迁 `.verif`（第 2 个 change）依赖本 change 的原语
⇒ ⇒ 且护栏 ⑥⁗ 只在 `.verif` 已存在时判红 ⇒ ⇒ 两者**耦合不成立**（可独立回滚）⇒ ⇒ 拆两个。

---

## What Changes

### ① `WAIT` 增补电平形参（ADR-074 §7.2 的 D2′）

**语法**：`WAIT <port> [<level>] <max>` ⇒ `level` **缺省 = 非零** ⇒ ⇒ **现有
`WAIT <port> <max>` 语义逐字不变** ⇒ ⇒ 现有 `.verif`（Lite 的 113 条）的 checked-in `.sv`
**产物逐字节不变**。

**四条语义**（§7.2 已钉死，实施 MUST 逐条对齐）：采样窗 = 沿后结算窗（与 `EXPECT` 同锚）·
位宽比较（按 N 位）· 缺省 level = 非零 · 命中后 MUST 至少推进 1 拍。
**fail-closed**：`level` MUST 是 `high` / `low` 关键字（数字非法 ⇒ 扫描器非零退出）。

⚠️ **`high` 的定义（本 change 钉死 · ADR §7.2 需 architect dated 补注）**：
**`high` ≡ 缺省 ≡ 「端口值 ≠ 0」**（**非零**，**不是**「N 位全 1」）⇒ ⇒ 二者**完全等价**
⇒ ⇒ 无歧义 ⇒ ⇒ 且**不需要端口宽度**（⚠️ 解释器的 `Resolver` **不返回 width** ⇒ 「全 1」
判定在解释器侧**缺输入** ⇒ ⇒ 选「全 1」会把实现逼进死路）。
⇒ ⇒ 后果（**必须写进文档**）：`WAIT p high max` 对 **N>1** 端口**不是**「等全 1」⇒ ⇒
唯一现役消费者 `uart_tx` 是 **1-bit** ⇒ ⇒ 当前无行为差异 ⇒ ⇒ 但 API 语义**以此为准**。
⇒ ⇒ ⚠️ **「位宽比较」那行的准确含义 = 「按端口自身位宽取值比较，不做符号/宽度扩展」**
（**不是**「与全 1 比较」）⇒ ⇒ architect 补注时 MUST 写清这一点。

### ② **三处同步**（缺一即分叉）

| 处 | 落点 | 改什么 |
|---|---|---|
| 扫描 | `src/rtl_emit/verif_scan.cpp` | `WAIT` 的参数个数与 `level` 关键字校验 |
| IR / AST | `include/rtl_emit/verif.h` | `WAIT` 的 `level` 字段（缺省 = 非零） |
| 解释 | `src/rtl_emit/verif_run.cpp` | 按 `level` 比较（**位宽比较**）⇒ 命中后 ≥1 拍 |
| 渲染 | `src/rtl_emit/verif_render.cpp` | 按 `level` 比较 ⇒ **与解释器逐字同语义** |

⚠️ **失败形态**（实施期自检）：只改扫描 ⇒ 渲染器缺分支（若有 `default` 则**静默不发射** ⇒ 更危险）·
只改解释器 ⇒ `check_id` 集合不等但**值**不同抓不住 ⇒ ⇒ 三处 MUST 同批改；且解释器遇未知 kind
需 `default: error`（fail-closed）。

### ③ **D8 证据义务升级为运行期比对**（§7.6 的 AC-D8′-1/2/3）

⚠️ **现状是「静态 `check_id` 集合比对」**（`tests/test_verif_axi4_lite.cpp` 附近 · 只比
check_id 集合 · **不跑 Verilator · 不比对值**）⇒ ⇒ 对**时序敏感**的新原语**结构性失明**
（16 拍位宽会吸收 1 拍偏移 ⇒ 两端各自全绿）。

⇒ ⇒ 本 change MUST 提供：**同一份 `.verif` 在 IR 解释器与渲染 TB 各实跑一次 ⇒ 逐条
`(check_id, PASS/FAIL)` 对比对相等**（运行期，非静态）⇒ ⇒ 外加 **4 条时序判别性构例**：
首拍即成立 / 目标在窗口最后一拍才成立 / 窗口内从未成立（⇒ FAIL 且点名 `check_id`）/
**变异 1 拍必红**。

⚠️ **变异机制（metis P1-6 · 裁决）**：变异**投在夹具的 `.verif` 输入上**（改 `WAIT` 的 `max`
⇒ ⇒ 边界构例必红）⇒ ⇒ **不动实现代码** ⇒ ⇒ 可自动化、可重跑 ⇒ ⇒ 优于「改解释器一行」
（那会与「还原源码 ≠ 复原（二进制也是状态）」同型 ⇒ ⇒ 需重建 ⇒ ⇒ 成本高且易留污染）。

### ④ 库层 `run_verif` helper 下沉（规则/值分离）

Lite 侧的「默认模式 `run_verif` 七步序列」目前**内联在 `tb_axi4_lite_example.cpp`** ⇒ ⇒ 下一个
`.verif` 驱动的 example（SoC）会**再抄一份** ⇒ ⇒ 下沉为库层 helper（`include/rtl_emit/verif.h`
或新头）⇒ ⇒ **同一份 helper 驱动 Lite 与 SoC**（= ADR-074 AC16/AC20 的同一性判据形态）。

⚠️ **helper MUST NOT 返回 `RESULT:` 结论行**（metis slop 风险）⇒ ⇒ 结论行由**各 example 自己**打印（它是**值**侧的载体：条数 / 失败项）⇒ ⇒ helper 只做**规则**（解析 → 建图 → 跑 → 收集失败行）⇒ ⇒ 否则「库层结论行」与「example 协议行」成**双权威**。

---

## Capabilities

**无 spec 变更** ⇒ `.openspec.yaml` 声明 `skip_specs: true` ⇒ 归档用 `--skip-specs`。

⇒ ⇒ 既有 `examples-rtl-sim-coverage` 的 requirement（「C++ 侧 MUST 与生成式 TB 共享同一验证源」
「清单列号 MUST 从机器可读表头推导」…）**已完整覆盖**本 change 的义务（扩原语**不新增**
requirement；判据本体在 ADR-074 §7）⇒ ⇒ **不写 delta**。

---

## Impact

| 面 | 变化 |
|---|---|
| 库 | `include/rtl_emit/verif.h` · `src/rtl_emit/verif_{scan,run,render}.cpp` · （helper 新增） |
| example | `examples/axi4/tb_axi4_lite_example.cpp`（七步序列改调库层 helper · **产物逐字节不变**） |
| 测试 | 新增「运行期同结论比对」的夹具 + 4 条时序构例 + 1 条变异自证 |
| 清单 | **不动**（本 change 不迁任何 example ⇒ `interp_checks` / `min_checks` 全不变） |
| 锚点 | `CTEST_TOTAL` / `EXAMPLES_TEST_SOURCES` / `tests/test_*.cpp` **会变**（新增测试源）⇒ 按实测回填 |

---

## Out of Scope

| 项 | 理由 |
|---|---|
| **SoC 迁 `.verif`** | 第 2 个 change（用户已拍板拆两个）⇒ ⇒ 依赖本 change 的原语 |
| **放宽表达式文法**（变量 / 条件 / 外部引用） | ADR-074 §7.4 的 C/D 行已否决（不动不变量 ⇒ 否决理由 = 成本/动机，非逻辑必然）⇒ ⇒ 若实施期发现**不动不变量就表达不了** ⇒ ⇒ **STOP** ⇒ 另立 ADR |
| **`.verif` 的其他原语扩展** | 本 change 只做 `WAIT` 的电平形参（最小集）⇒ ⇒ 下一个 SPI/I2C 需求再裁 |

---

## 用户可见变更

| 项 | 前 | 后 |
|---|---|---|
| `.verif` 的 `WAIT` 语法 | `WAIT <port> <max>`（只等**非零**） | `WAIT <port> [<level>] <max>`（`level` ∈ {`high`,`low`} · **缺省 = 非零** ⇒ 旧写法**语义逐字不变**） |
| `WAIT` 的「等非零」对空闲高信号 | 第一拍就返回 ⇒ **锚不住帧起点** | `WAIT p low <max>` 等到**目标电平**成立 |
| 扫描器对非法 `level` | — | **非零退出** + 消息**点名 `level`**（`HIGH` / `1` 等都拒绝） |
| Lite 的 checked-in `.sv` 产物 | sha256 = X | **逐字节不变**（= 禁止重构渲染器，不是「结果等价」） |

## 验收标准

| # | 判据 | 方式 |
|---|---|---|
| AC1 | `WAIT <port> <level> <max>` 可解析且 `level` ∈ {`high`,`low`}；`level` **缺省**时语义与改动前**逐字相同** | 单测 |
| AC2 | 扫描器负向：`level` 为数字 / 缺 `max` / 多余实参 ⇒ **非零退出**（fail-closed） | 单测 |
| AC3 | **三处同步**：扫描 / AST / 解释 / 渲染 ⇒ ⇒ 同一份 `.verif` 在**两端**给出**同一结论** | 运行期比对 |
| AC4 | **运行期** `(check_id, PASS/FAIL)` 逐条比对相等（⚠️ **不是**静态 id 集合比对）—— **载体 = ctest**（新测试文件 · 打 `verilator_e2e` 标签 ⇒ 复用仓内已有的 verilator 能力）⇒ ⇒ 锚点 **`VERILATOR_E2E_TOTAL` + `CTEST_TOTAL` 会变**；⚠️ **`EXAMPLES_TEST_SOURCES` 不变**（夹具放 `tests/**`，**不放** `examples/**` ⇒ 否则 examples 侧测试源计数会变） | 新夹具 |
| AC5 | **4 条时序判别性构例**：首拍即成立 / 窗口最后一拍才成立 / 从未成立（⇒ FAIL 且点名 `check_id`）/ **变异 1 拍 ⇒ 至少一条构例转红** | 单测 + 变异 |
| AC6 | Lite 的 checked-in `.sv` **产物逐字节不变**（`min_checks=113` 等式仍成立） | 护栏 ⑥' + sha256 |
| AC7 | 库层 `run_verif` helper 被 **Lite 与（未来的）SoC** 共用（同一份实现，非两份拷贝） | 代码 + grep |
| AC8 | `verify_examples_rtl_sim.sh` **PASS** · `--self-test` **PASS** · `ctest` **全绿** · 锚点按实测回填 | shell |
| AC9 | **缺省 level 的值级断言**：1-bit 信号第 3 拍起高 ⇒ ⇒ `WAIT` 在**第 3 拍**命中（**不是**第 4 拍）⇒ ⇒ 钉死「命中拍」的语义 | 单测 |
| AC10 | **N>1 端口的 high/low 构例** ⇒ ⇒ 钉死「`high` = 非零」在多位端口上的行为（**不是**全 1） | 单测 |
| AC11 | **边界构例两条都断**：命中**恰在第 `max` 拍** ⇒ **PASS**；命中在第 `max+1` 拍 ⇒ **FAIL** 且点名 `check_id` | 单测 |
| AC12 | 扫描器负向的错误消息 MUST **点名 `level`**（不是「非法数字」）⇒ ⇒ `HIGH` / `High` / `1` 三种写法都报 level | 单测 |
| AC13 | **「首拍即成立」构例的期望 = 「命中后仍推进 ≥1 拍」**（D3′ 的「至少推进 1 拍」条款）⇒ ⇒ **不是**「立即返回」⇒ ⇒ ⚠️ 「入口即成立 ⇒ MUST 用 `TICK(n)` 禁 `WAIT`」是**规范义务**（由人遵守），**不是** `WAIT` 的行为 ⇒ ⇒ 两者**不冲突** | 单测 |
| AC14 | ⚠️ **已知差异登记**：渲染侧 `!==` 对 **X** 恒真 ⇒ ⇒ `high` 把 X 当高（fail-open）· `low` 永不命中（fail-closed）⇒ ⇒ 两端对 X 的处理**不对称** ⇒ ⇒ MUST 登记（与既有 `EXPECT` 的 X 差异**同型**）⇒ ⇒ 不在本 change 修 | 文档 |
| AC15 | **默认路径的渲染文本 MUST NOT 改** ⇒ ⇒ 双证：⑥' 新鲜度 + `top.v` 的 sha256 与改动前**逐字相同** ⇒ ⇒ ⚠️ 「禁止重构渲染器」（不是「结果等价」） | sha256 + 护栏 |
