# ADR-075: 产品面顶层端口具名化 —— 「端口名可解析到 C++ 成员名」对产品顶层的强制化

- **Status**: **Accepted**（2026-11-17 · architect role（build agent）· 双审已闭合：oracle 第 1 轮 REJECT（本 ADR 命中 1B）⇒ 全闭 · critic 第 1 轮 REJECT（本 ADR 命中 1B）⇒ 全闭 · **复审 APPROVE_WITH_CONDITIONS · 0 BLOCKER** ⇒ 关联 change 已立项 + AC 映射已回填 ⇒ 详见 `.rddf/reviews/ADR-075-port-naming-prerequisite.md`）
- **Date**: 2026-11-17
- **Author**: architect role（build agent）
- **Parent**: **ADR-072**（顶层端口面收敛 · D1 · D2）· **ADR-046**（D10 SoC 端口冻结 · RV32I 侧）· **ADR-058 / ADR-061**（层次化输出）
- **关联 change**: **`rtl-verification-single-source-2026-11`**（`openspec/changes/rtl-verification-single-source-2026-11/` · **已立项** · planner 2026-11-17）⇒ ⇒ **AC 映射** = 该 change 的 **AC2 / AC17 / AC19 / AC20**（双向对照见 improvement §3）
- **蓝图**: `docs/architecture/2026-11-17-rtl-verification-single-source.md` §4.2
- **审稿记录**: `.rddf/reviews/ADR-075-port-naming-prerequisite.md`
- **超长自检**: 正文 **< 250 行** ⇒ 无需拆分
- **实施序**: **本 ADR 的 E1 先于 ADR-074 的 D1**（单向）

---

## 1. 背景与问题

### 1.1 诉求来源

**ADR-074**（验证单一真相源）要求判据能被机械渲染成 `.sv` TB ⇒ ⇒ TB 的端口连线 MUST 能从 manifest **机械**得到 ⇒ ⇒ 需要一条无人工介入的「C++ 成员名 ↔ 产物端口名」映射。

### 1.2 实测事实（2026-11-17 · 可复算）

| # | 事实 | 证据 |
|---|---|---|
| ① | 🔴 **框架无反射** ⇒ 未具名端口的 C++ 成员名**拿不到** ⇒ `manifest.cpp_member` 记为 `unnamed_input` 占位 ⇒ 消费方 MUST 把该值理解为「未知」 | `include/rtl_emit/manifest.h:110-125` |
| ② | ✅ **N2 棘轮护栏已把「端口名可解析到 C++ 成员名」立为 MUST**（P1 判「标识符 == 显式名字串」）⇒ ⇒ **本条已是既有规范，非新造** | `scripts/verify_port_naming.sh:30-32` |
| ③ | ⚠️ **N2 是棘轮**（存量登记豁免 ⇒ 债只许减不许增）⇒ ⇒ 存量可继续未具名；**豁免表数据行 = 1447**，护栏锚点 `EXPECTED_EXEMPT_PORTS=1447` | `scripts/verify_port_naming.sh:20-24, 50` · `grep -vc '^#' .rddf/checks/port-naming-exempt.tsv` = **1447** |
| ④ | 🔴 **首批两个 AXI4 example 的「产品顶层」共 32 个端口未具名** —— `axi4_lite_example.cpp` **20**（`AxiLiteTop`，单一 `__io`）+ `axi_soc_example.cpp` **12**（`AxiSocTop` 的 `__io` 在 `:161-181`） | 逐块计数（脚本：`__io` 块 41..72 = **23** · 161..181 = **12**）· `.rddf/checks/soc-rtl-expectations.md:28-50`（顶层 = 框架 2 + 根自有 12 = 14） |
| ⑤ | ⚠️ **`axi_soc_example.cpp` 另有 23 个内层端口**（`AxiLiteTestMaster` 的 `__io`，`:41-72`）⇒ ⇒ 它们是**层次化产物里子 module 的端口**，**不是产品顶层** ⇒ ⇒ **不在本 ADR 范围** | 同上 + ADR-073 D1（成层后 4 个 IP 各有自己的 module） |
| ⑥ | ✅ **具名先例存在且是产品面** —— RV32I 顶层 **32** 个具名端口 ⇒ 产物端口名 = `top_<成员名>` | `examples/riscv-mini/src/rv32i_pipeline_top.h:62-73` · `docs/developer_guide/patterns/PORT-NAMING-GUIDE.md` |
| ⑦ | ✅ 具名构造约束已定（`port` 具名构造是 `explicit` ⇒ 拷贝初始化 `= "name"` 编译不过；名字串 MUST 与标识符逐字相同） | `include/core/AGENTS.md`（端口具名条）· `PORT-NAMING-GUIDE.md:32` |
| ⑧ | 🔴 **`port_table[]` 元素只有 `{name, dir, width, role}` —— 无 `cpp_member`**；`cpp_member` 只在全量 `ports[]` 里 | `include/rtl_emit/manifest.h:74-83` · `:51` |
| ⑨ | ⚠️ **「产物端口名与 C++ 成员名字面相同」在当前扁平发射器下不可达** —— route (c) 已被探针证伪 + base name 撞 | `docs/architecture/2026-11-02-hierarchical-rtl-output.md:302` |
| ⑩ | ⚠️ **产物根 module 名可配置**（默认 `top`）⇒ ⇒ 「`top_` + 成员名」的字符串拼接**不是契约** | `include/hierarchy_verilog.h:33`（`top_module_name = "top"`）· `include/device.h`（`"top"`） |

### 1.3 问题

「端口名可解析到 C++ 成员名」这条 MUST 的**适用范围**该多大？
- 现状（N2 棘轮）：**只对新增端口** ⇒ 产品顶层（32 个）不受约束 ⇒ **ADR-074 落不了地**。
- 全量 1447 条：**不可行**（改名会动全部产物 · 仓内已评估「不能一次改完」）。

⇒ ⇒ 需要**有界的中间态**：**产品顶层必须具名**，其余维持棘轮。

---

## 2. 决策

### 2.1 **E1** = 首批两个 AXI4 example 的 **产品顶层** MUST 具名（**32** 个）

| 文件 | 顶层 `__io` | 端口数 | 处置 |
|---|---|---|---|
| `examples/axi4/axi4_lite_example.cpp` | `AxiLiteTop` | **20** | **MUST 具名** |
| `examples/axi4/axi_soc_example.cpp` | `AxiSocTop`（`:161-181`） | **12** | **MUST 具名** |
| 合计 | — | **32** | — |

- 形态：`ch_in<ch_uint<32>> awaddr{"awaddr"};`（**按端口实际类型与宽度**；`ch_in<ch_bool> start{"start"};` 为 1 位形态）⇒ ⇒ 直接初始化 + 名字串是 `explicit` 构造的必然形态；名字串 MUST 与标识符**逐字相同**（事实 ⑦）。
- ⚠️ **不在范围**（v2 明确）：`axi_soc_example.cpp:41-72` 的 **23** 个**内层**端口（`AxiLiteTestMaster`）⇒ ⇒ 改它们 = 改**层次化产物中子 module 的端口名** ⇒ ⇒ 另一类产品可见变更 ⇒ ⇒ **本次不做**（若要扩，MUST 单独立项并登记子 module 端口名变更）。

### 2.2 **E2** = 承诺边界（**MUST 精确**）

| 承诺 | 不承诺 |
|---|---|
| ✅ `manifest.cpp_member` **可机械解析** ⇒ ⇒ `port_table[].name ↔ ports[].cpp_member` 可建 **1:1 唯一映射**（事实 ⑧ 两表都在 manifest 里） | ❌ 「产物端口名 == C++ 成员名字面相同」⇒ 事实 ⑨：**不可达**；且根 module 名可配置（事实 ⑩）⇒ ⇒ **禁**任何 `top_` 字符串拼接 |

### 2.3 **E3** = 映射载体 = **两表 join + 1:1 唯一性 fail-closed**（**不新增 schema**）

- `port_table[]`（产品端口表，按产物顺序）· `ports[]`（全量，含 `cpp_member`）⇒ ⇒ **join 键 = `name`**（两者都是 Verilog 名，`manifest.h:107`）。
- 判据：每个 `port_table[]` 元素 MUST 命中**恰好一个** `ports[]` 元素且其 `cpp_member != "unnamed_input"`；缺失 / 不唯一 / 占位 ⇒ ⇒ **生成器非零退出**。
- ⚠️ **不选**「给 `port_table[]` 加 `cpp_member`」⇒ ⇒ 那是 **manifest schema v2 变更** ⇒ ⇒ 要动 capability `rtl-emit-portability` + ADR-072 R-1（把 4 字段写死）⇒ ⇒ 成本高于 join。

### 2.4 **E4** = 范围**有界**：全仓其余 **1415** 条**本次不做**

- 1447（事实 ③）− 32（E1）= **1415**。
- 维持 N2 棘轮（**债只许减不许增**）⇒ ⇒ 全仓端口名逐步可自解释，**不承诺本次归零**；剩余 MUST 视为**既有债**（豁免表即台账）。

### 2.5 **E5** = 豁免表**棘轮同步**（32 行移除 + 基线更新，**同批**）

- `.rddf/checks/port-naming-exempt.tsv` 移除这 **32** 行；`scripts/verify_port_naming.sh:50` 的 `EXPECTED_EXEMPT_PORTS` **1447 → 1415**。
- ⚠️ 护栏**反向判据**（登记表里有、树里没有 ⇒ 红）⇒ ⇒ 移除与基线更新 **MUST 同批**（否则必然红）。

### 2.6 **E6** = **产品可见变更** ⇒ MUST 记 `docs/CHANGELOG.md`

- `top_unnamed_input_N` ⇒ `top_<成员名>`（**仅这 32 个**；子 module 与其余 example 端口名**不变**）。
- 同批 MUST 同步：手写 TB 连线（若仍在手写期）· `.rddf/checks/*.md` 的端口映射表 · manifest 快照（护栏 ⑧ 逐字逐序）。

---

## 3. 选项对比表

| # | 选项 | 结论 | 理由 |
|---|---|---|---|
| **①** | 维持现状（N2 棘轮，存量不改） | ❌ 拒 | ADR-074 落不了地 ⇒ 手写映射表与事实 ④ 的漂移面保留 |
| **②** | **首批两个 example 的产品顶层具名（32 条）** + 其余维持棘轮 | ✅ **采用** | 有界（32）· 回到既有规范（事实 ②⑥）· 直通 ADR-074 的硬前置 · 不触子 module 端口面 |
| **③** | 连内层 `AxiLiteTestMaster` 的 23 条一起改（= 原 v1 的「55」） | ❌ 拒（**v2 改判**） | 改的是**层次化产物子 module 的端口名** ⇒ 另一类产品可见变更 ⇒ 与「最小改动」冲突；且与 ADR-074 的身份链（只消费 `port_table[]`）**无关** |
| **④** | 全仓 1447 条一次改完 | ❌ 拒 | 改名会动全部产物与既有 TB ⇒ 影响面不可控 |
| **⑤** | 给框架加反射 | ❌ 拒 | C++ 无反射 ⇒ 需改 `port`/`__io` 的语言层设计 ⇒ 影响全部组件；事实 ⑦ 已给出等价且更低成本的手段 |
| **⑥** | 保持未具名 + 生成器侧维护映射表 | ❌ 拒 | 映射表仍是**手写的第二载体** ⇒ 事实 ④ 的漂移面原样保留 |
| **⑦** | 只具名 SoC 顶层，AXI4-Lite 暂不动 | ❌ 拒 | `tb_axi4_lite_example.sv` 正是**已漂**的那份（事实 ④）⇒ 漏它 = 漂移面保留 |
| **⑧** | 给 `port_table[]` 加 `cpp_member`（schema 变更） | ❌ 拒（**已考虑**） | 需改 capability + ADR-072 R-1；join 已达同等效果且零 schema 变更 |

---

## 4. 后果 / 代价

### 4.1 正面

| # | 后果 |
|---|---|
| P1 | **身份链闭合**（DSL 标识符 = `cpp_member` ⇒ join ⇒ 产物端口名）⇒ ADR-074 可行 |
| P2 | 32 个产品端口名可自解释（产物名**由 `port_table[].name` 读取**，预期形态 `top_<成员名>`，取代 `top_unnamed_input_2`）|
| P3 | 手写映射表可退役（事实 ④ 的**当前已漂**状态被封口） |
| P4 | 回到既有规范（事实 ②⑥）⇒ 非新造机制 |

### 4.2 负面 / 代价

| # | 代价 | 量级 |
|---|---|---|
| N1 | 32 个端口产物名变更 ⇒ 连带 TB 连线 / 期望源 / manifest 快照同步 | 1 批 |
| N2 | 豁免表移除 + 基线 `1447 → 1415` MUST 同批 | 1 条 |
| N3 | 用户可见变更 ⇒ CHANGELOG 义务 | 1 条 |
| N4 | 全仓 1415 条债仍在 ⇒ 多数 example 产物端口名**仍不可自解释** | 既有债 |
| N5 | **子 module 端口名仍不可自解释**（`AxiLiteTestMaster` 的 23 条） | 已登记为**已知边界** ⇒ 与 ADR-074 身份链无关 |

### 4.3 风险

| # | 风险 | 缓解 |
|---|---|---|
| R1 | 改名 ⇒ 护栏 ⑧（逐字逐序）红 | 同批重生成产物 + manifest |
| R2 | 改名 ⇒ 旧 TB 悬空 ⇒ 误红被判成实现缺陷（同事实 ④ 的事故型） | 同批改 TB；ADR-074 生成器落地后该面消失 |
| R3 | 误把「字面相同」写进 AC ⇒ 永不可满足 | E2 明确不承诺 + 事实 ⑩ 禁字符串拼接 |

---

## 5. Verification（可勾选验收判据 · **前瞻**）

- [ ] **AC1** 32 个产品端口在 `ports[]` 里 `cpp_member != "unnamed_input"`（逐条）
- [ ] **AC2** `port_table[]` ↔ `ports[]` join **1:1 唯一**（缺失 / 不唯一 / 占位 ⇒ 非零退出）
- [ ] **AC3** 产物端口名**从 `port_table[].name` 读取**（**禁** `top_` 字符串拼接）且与期望表一致
- [ ] **AC4** 豁免表减少 **32** 行 且 `bash scripts/verify_port_naming.sh` 绿（基线 `1447 → 1415`）
- [ ] **AC5** 护栏 ⑧（manifest ↔ 产物 · 逐字逐序）绿
- [ ] **AC6** `.rddf/checks/*.md` 的端口映射表与新端口名一致（无 `top_unnamed_*` 残留）
- [ ] **AC7** `docs/CHANGELOG.md` 有用户可见变更条目
- [ ] **AC8** **RV32I 产品面零变化**（`rv32i_pipeline_top.h` 的 32 根逐条不变）
- [ ] **AC9** 全仓豁免表**只减不增**（diff 只含移除）
- [ ] **AC10** 子 module 端口名**未变**（`AxiLiteTestMaster` 的 23 条仍为原形态）⇒ E1 的范围边界成立

---

## 6. 边界与不变量（**不是决策**）

- **不改**：ADR-046 D10（RV32I 端口冻结 · 事实 ⑥ 的 32 根）· ADR-072 D1（顶层端口面 = 根自有 `__io` 的口径）· N2 护栏的**判据本身**（只同步基线）。
- **实施序**：本 ADR 的 **E1 先于 ADR-074 的 D1**（单向；不是互依赖）。
- **不覆盖**：`docs/developer_guide/patterns/PORT-NAMING-GUIDE.md` 与 `include/core/AGENTS.md` 的正文改写（不在 architect 写集 ⇒ 交 planner/builder）。
- **归档前**：`Proposed` → `Accepted` + 关联 change 已立项并回填 AC 映射。
