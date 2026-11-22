# 审稿记录 —— ADR-077（两级顶层与 SoC 组合的四条形态裁决）

- **ADR**: `docs/adr/ADR-077-two-level-tops-and-soc-composition.md`
- **日期**: 2026-11-23
- **顺序**: **oracle → critic**（architect 路径，顺序不可颠倒）
- **轮次**: oracle **1** 轮 · critic **3** 轮（第 1 轮 + 1 次复审 + **1 次用户批准的预算外窄复审**）
- **当前状态**: **Accepted**（2026-11-23 · 三轮全闭 · 第 3 轮 `mergeable: yes`）
- **用户裁决**: 2026-11-23 选 **(a)** = 批准第 3 轮窄复审（而非 (b) 机械自证闭合）

---

## 1. oracle 第 1 轮（2026-11-23）

- **审查域**: 其他审查（项目 ADR）
- **判定**: **APPROVE**（无 BLOCKER）
- **findings 与处置**（**全部已处置**）

| # | 级别 | finding | 处置 |
|---|---|---|---|
| 1 | SHOULD_FIX | V5 算术不自洽（写「34（4 in + 28 out）」但 4+28=32）· 且子 module 端口表**无现成判据载体**（`manifest.port_table[]` 只覆盖顶层） | ✅ V5 改为「34 = 框架 2 + 核自有 32（4 in + 28 out）」+ 写明**判据载体 MUST 新建**（子 module 端口表提取 + 判别力自证 · §4.5 ③） |
| 2 | SHOULD_FIX | R4 的缓解引用失实：`verify_no_duplicate_rv32i_headers.sh` 扫描面 = `include/cpu/**` vs `examples/riscv-mini/src/**`，**不覆盖 `examples/riscv-soc/src/`** | ✅ 改为「**把扫描面扩到 `examples/riscv-soc/src/`**（S1 的 AC11 承接）⇒ ⚠️ 未扩前该风险**无护栏**」 |
| 3 | SHOULD_FIX | V6 无判据主语（"出现在表里且与 TB 逐字一致"靠自觉） | ✅ V6 补主语 = **一条机械判据**（§4.5 ④） |
| 4 | NIT | D3「唯一手段」失实（另有 `--firmware-init` 烘焙路径） | ✅ 改为「唯一**保持产品固件无关**的运行期手段」 |
| 5 | NIT | V12(a) 变异位点未指明（改 C++ 侧会编译失败 ⇒ 假绿） | ✅ 改为「变异施加在**观测集**」 |
| 6 | 可选 | §4 应明写"判别力弱于 axi4"的唯一性，避免后世误读 | ⚠️ 该条在中途被**取代**（见 §2 BLOCKER：S1 必须满足 ≥2，不再"弱于"） |
| 7 | 可选 | `rtl-hierarchical-output` 的「8 module」将来加作用域限定 | ✅ 登记为**第三次装配时的修订点**（§4 末尾注记 + §4.5 ⑤，不动 spec） |

**oracle 独立核实为成立的论断**（摘要）：D1 的 ADR-046 dated addendum 路径合法（ADR-046 原文作用域本就只写 `examples/riscv-mini/src/`）；判据分域有出处（「8」= `rtl-hierarchical-output:727` · 「IP+1」= `rtl-soc-output:39-40`）；D2 与既有「模块类型名非契约」裁决自洽；V12(b) 会真红（断连线 ⇒ 后代悬空 `ch_in` ⇒ ADR-072 D1-f fail-closed）。

---

## 2. critic 第 1 轮（2026-11-23）

- **profile**: `arch`
- **判定**: **REJECT · mergeable: no · scope_exceeded: false**（1 BLOCKER + 1 SHOULD_FIX + 1 NIT）

| # | 级别 | finding | 处置 |
|---|---|---|---|
| 1 | **BLOCKER** | V4/AC4 用 `min_varying_observations ≥ 1`，而**签批 spec** `rtl-soc-output` 明文要求 SoC 产物观测集 MUST 含 **≥2 个**随激励变化的量（`:26-27` / `:186-188` / `:202`，粘滞排除 `:190`）⇒ S1 交付物在签批 spec 下**不可通过**，且 ADR 用"结构上限"承认冲突而非走修订流程 | ✅ **走修复路径 (b)（不动 spec）**：D1 把根自有输出定为**两个计数器**（`store_count` + `writeback_count`，同属 `SocTraceCounter`）· `min_varying == 2` · 激励 ≥10 拍且值不同 · 粘滞不计入（V1/V4/V12(a)/R3 同步） |
| 2 | SHOULD_FIX | D4 的「同一产品建两行 ⇒ 守恒等式"算两次" ⇒ MUST NOT」**机制错误**：仓内已有同一设计占两桶的先例（`tb_rv32i_soc`=target + `riscv-mini-rv32i-soc-hier`=hier），守恒**逐桶** | ✅ §2.4 与 §3 选项 3 改写：单行 `soc` 的理由 = 「`hier` 桶语义 = **非默认**的另一种发射形态；SoC 的层次化是**默认** ⇒ 双行零信息增量」 |
| 3 | NIT | `dut` 行的收敛叙事与 tsv 表头口径略有出入 | ✅ 表格注记改为「以登记面**实际口径**为准；spec 修订时顺手对齐」 |

---

## 3. critic 第 2 轮（复审 · 只验闭合）

- **本轮提交**: 上轮 BLOCKER 闭合证据 + 本轮新增 diff + `review_profile: 继承第 1 轮：arch`
- **判定**: **REJECT · mergeable: no**（**上轮 BLOCKER 已闭合** ✔ · **新增 1 个 BLOCKER**）

| # | 级别 | finding | 处置 |
|---|---|---|---|
| 1 | **BLOCKER（新）** | V1 已改为「根自有 6 ⇒ 产品端口 8」，但 **V3 仍写「顶层端口数 == 7」** ⇒ 同一决策两处数字不可同时满足（`check_products` CHECK 2 比对 `manifest.port_table.size()` = 含框架 2 ⇒ 必为 8） | ✅ **已改**（V3 = **8**）⇒ 机械复核见 §4 |
| 2 | SHOULD_FIX | §4 混入审稿轮次叙述（"被 critic 第 1 轮判 BLOCKER"）⇒ 载体应是本报告 | ✅ **已删**（决策正文只留结论） |
| 3 | 确认 | **上轮 BLOCKER 闭合**：D1 `:63-73` 引 spec 原文 · V4 `== 2` + ≥10 拍 · V12(a) 变异 = 移除 `writeback_count` ⇒ 变化量 1 < 2 ⇒ 必红 · R3 同步 | — |

---

## 4. 闭合证据（机械复核）与**待裁决**

```bash
# 全篇数字一致性：root __io = 6 · 产品端口/CHECK 2 = 8 · 核 module = 34（框架 2 + 自有 32）
grep -nE "root .*__io|产品端口|顶层端口数|== \*\*[0-9]+\*\*" \
  docs/adr/ADR-077-two-level-tops-and-soc-composition.md
# ⇒ 命中：V1 `__io` = 6 · V3 顶层端口数 = 8 · V5 = 34；无 5/7 残留
```

- **第 1 个 BLOCKER**：经 critic 第 2 轮**确认闭合** ✔
- **第 2 个 BLOCKER（陈旧数字）**：已修复，并**经第 3 轮窄复审确认闭合**（见 §4.1）

---

## 4.1 critic 第 3 轮（用户批准的预算外窄复审 · 只验第 2 轮 BLOCKER 闭合）

- **触发**: 用户 2026-11-23 选 **(a)**（批准窄复审，而非 (b) 机械自证）
- **本轮提交**: 第 2 轮 BLOCKER 的闭合证据（V3 两行 diff）+ 「自第 2 轮以来仅两处 diff」的范围声明 + `review_profile: 继承第 2 轮：arch`
- **判定**: **APPROVE · mergeable: yes · scope_exceeded: false**
- **findings**:
  - **BLOCKER 无** —— V3（L209）「顶层端口数 == **8**（`manifest.port_table.size()` · 含框架 2）」与 V1（L207）「root `__io` == 6 ⇒ 产品端口 == 8（框架 2 + 根自有 6）」**数字与推导链一致**（6 + 2 = 8）；全篇无 `== 5` / `== 7` 残留；§4 ① 的「AC3（7 → **8**）」是给 planner 的处置指令，方向与 V1/V3 一致，不构成文档内矛盾。
  - **SHOULD_FIX 无** —— 第 2 轮「审稿轮次叙述混入决策正文」已删（正文只留结论）。
  - **NITS 无新**（窄复审范围 = 上述两处 diff）。
  - ⚠️ **审查者自陈的范围限制**：critic 环境 `shell` 权限被拒 ⇒ 无法独立确认「仅两处 diff」，只能验证给定 diff 在**当前文件**的落点状态。⇒ 由本报告 §4 的 grep 机械复核补足。

⇒ **ADR-077 三轮全闭，转 `Accepted`（2026-11-23）**。

---

## 5. 与 change 的衔接

| 项 | 归属 | 状态 |
|---|---|---|
| **ADR-046 §D1 的 dated addendum** | architect | ✅ **已落地**（2026-11-23 · D1 第 1 类条目改写为「装配层 · 两个层级」+ 行尾追记指向 ADR-077，无决策变更；落在 §审查配对的 ADR「描述性措辞」豁免的可数条件内） |
| proposal 的 AC3（7→**8**）· AC4（`≥1`→**==2** + ≥10 拍）· 组件名 `SocStoreCounter`→**`SocTraceCounter`** | **planner**（architect 写集不含 proposal） | 🔴 **未同步**（critic 复审已点名：proposal `:146-148` 仍写 7 / ≥1 / 旧名）⇒ ⇒ **权威数字以 ADR-077 的 V1/V2/V4 为准**，proposal 未同步前 S1 的 AC 与 ADR 不一致 |
| `verify_no_duplicate_rv32i_headers.sh` 扫描面扩到 `examples/riscv-soc/src/` | builder | 待 S1（AC11） |
| 子 module 端口表提取（V5 载体）· 登记面↔TB 期望集比对（V6 主语） | builder | 待 S1（AC6 / AC11） |
