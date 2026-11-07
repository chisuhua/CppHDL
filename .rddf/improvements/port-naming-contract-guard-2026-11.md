---
name: port-naming-contract-guard-2026-11
priority: P2
source: 台账 §7 残留 5 + roadmap `feat-hierarchical-rtl-output` §③ 后续立项清单（残留 5 行）
phase: phase-5
category: guard
type: chore
created_at: 2026-11-06
evidence: probe:docs/architecture/2026-11-02-hierarchical-rtl-output.md#7
---

# Improvement: 端口命名契约的静态护栏（残留 5）

- **Date drafted**: 2026-11-06（v3 —— v2 吸收 metis 第 1 轮 gap 分析；v3 落下用户拍板 B1）
- **Drafted by**: build-agent（planner 角色）
- **Status**: 📝 草稿 v3（**设计已拍板** ⇒ 待 `rdd-builder` P0 走 5 选项批准门）
- **权威登记**：台账 §7 残留 **5**
- **规范所有者**：命名规则本身归 **`openspec/specs/rtl-hierarchical-output/spec.md`**
  （Purpose `:10-11` + 「悬空子输入 MUST 向上冒泡」Scenario `:186-187`）
  ⇒ ⇒ 本 change 只补**护栏机制**，**MUST NOT** 复述命名规则成第二份真相。

## 1. Why (背景与动机)

### 1.1 真正要补的是**覆盖空洞**，不是「加一条检查」

`scripts/verify_hierarchical_rtl.sh` 的 **R8** 是唯一的结构化端口判据，但它只对
**3 个固定产物**做手推期望集 ⇒ ⇒ **新设计形态零覆盖**（台账 §7 残留 5 的实质）。

### 1.2 ⚠️ 现状比 v1 描述的**更接近已守住**（metis 核实，必须如实登记）

| 事实 | 出处 |
|---|---|
| 发射器已有 **G3-5 硬拒绝**：根若有合成端口 ⇒ `ok=false` | `src/hierarchy_verilog.cpp:1113-1120` |
| `__up_` **不会**出现在根 module 的**端口表**：它只生成在「目标祖先的直接子 → 驱动端 module」链上；根拿到的是**体内 wire**（`logic [7:0] __up_0;`） | `src/hierarchy_verilog.cpp:516,555-569` · `tests/test_hierarchy_verilog.cpp:2222-2237` |

⇒ ⇒ **v1 的「若只守两种前缀，`__up_` 泄漏到顶层时无护栏」是错的** —— 今天它**无路径**泄漏。
- `__up_` 纳入判据 = **契约补全（defense in depth）**，**当前零判别力** ⇒ MUST 如实标注，
  **MUST NOT** 计成「关闭了一个真实缺口」。
- **R1 在现有 3 个产物上结构性恒绿**（G3-5 守住了根）⇒ 其真实价值 = **对 G3-5 被移除 /
  被绕过、以及未来新产物形态的回归栅栏**。

### 1.3 语料的实际合成端口覆盖（决定 R2 怎么写）

| 产物 | 合成端口 | 说明 |
|---|---|---|
| `three_layers.v` | **0** | 全部连接是端口↔端口（`tests/hier_rtl_products.cpp:47-63`） |
| `row1_implicit.v` | **1 个 `__if_0`** | `tests/hier_rtl_products.cpp:101` |
| `bubble.v` | **0** | 悬空输入走**冒泡**（origin=`Bubbled`，名字 = ctx 全局名） |

⇒ ⇒ **`__x_` / `__up_` 在整个产物集里零覆盖** ⇒ 覆盖矩阵（§2.2 的 N2b）今天**会红** —— 这正是它该暴露的账。

### ⚠️ 但两个缺失前缀**都有现成的触发形态**（决定 B1 可行）

| 前缀 | 既有先例 | 形态 |
|---|---|---|
| `__up_` | `tests/test_hierarchy_verilog.cpp:1637-1645` | 三层链：leaf 的 `output [7:0] __up_0` + `assign __up_0 = top_mid_leaf__reg;`，mid 的 `.__up_0(__up_0)`，top 的 `.__up_0(top_unnamed_output)` |
| `__x_` | `tests/test_hierarchy_verilog.cpp:1873` | 子 module 有一个**合成 output 端口** `__x_0`（父层读子内部节点） |

⇒ ⇒ **B1 不是从零构造**：两个前缀都有已验证的 DUT 形态可搬（这两个用例当前断言的是
**产物文本**，本 change 只需把同形态搬进 `hier_rtl_products` 类产物生成器）。

⇒ ⇒ ✅ **已由用户 2026-11-06 拍板为 B1**：本 change **补 fixture**，使
`__if_` / `__x_` / `__up_` **各有一个真实产物用例** ⇒ N2b 真的绿（不靠 self-test 自证）。

### 1.4 反面代价

命名契约一旦漂移，破坏的是「顶层端口名与扁平产物逐字相同」⇒ 用户按名字连线的代码会
**静默**连到未驱动网络（名字变了，编译通过）。

## 2. What Changes (变更范围)

### 2.1 新增护栏脚本

`scripts/verify_hierarchical_port_naming.sh`（+ `--self-test`）

⚠️ **判据编号用 `N0..N3`**，**不用** `R0..R3`（与 `verify_hierarchical_rtl.sh` 的 R1–R8
在日志/文档里撞名 ⇒ 「R2 红了」指哪个？）。

**硬约束**：MUST 复用 `verilator --json-only` + `scripts/lib/verilog_tree_extract.py`
⇒ ⇒ **MUST NOT** 自研 Verilog 文本解析器。

### 2.2 四条判据（全部重写 —— v1 的 R1/R2/R3 有错）

| 判据 | 内容 | 为什么 |
|---|---|---|
| **N0** | fail-closed，七类口径**分别**归类并给错误前缀：<br>① verilator 缺 ② 生成器缺 ③ 提取器缺 ④ `python3` 缺 ⑤ 生成器 rc≠0 ⑥ 产物数 ≠ 3 ⑦ 无 `.tree.json` | 本仓已踩过「fixture 缺失被当成编译失败」⇒ 报错口径错会让人误判根因 |
| **N1** | **根** module 中 `isPrimaryIO` 的端口名 MUST NOT 匹配精确正则 **`^__(if\|x\|up)_[0-9]+$`** | v1 的「以 `__if_` 开头」**太宽**：用户名字可 legit 以该前缀开头；且合成名形态恒为 `prefix + 十进制序号`（`make_synth_name`，`src/hierarchy_verilog.cpp:210-217`） |
| **N2a** | **全局存在量词**：整个夹具/产物集里，非根 module 端口上 MUST 至少出现 **1** 个匹配该正则的端口 | 防「一条都判不到」⇒ N1 恒绿 |
| **N2b** | **前缀覆盖矩阵**：`{__if_, __x_, __up_}` **每个** MUST 至少有一个**真实产物** fixture 使其出现在**非根** module 端口表；缺失 ⇒ 红并**点名缺哪个** | ⚠️ **今天只有 `__if_`** ⇒ ⇒ ✅ 已拍板 **B1**：本 change **补 fixture**（形态有先例，见 §1.3）⇒ 判据完整、不靠 self-test 自证 |
| **N3** | **判根** = `declared − instantiated` **恰好 1**；声明集来源见下；三种失败**文本可区分** | v1 的「未被任何 parent 实例化」**字面极易读反** —— `extract()` 第一列是**容器** module、第三列是**被实例化**的 module ⇒ 正确谓词是 `root ∈ {第一列} ∧ root ∉ {第三列}` |

#### N3 的三条实现要求

1. **声明集来源**（MUST 写死，二选一）：
   - **(建议)** 给 `verilog_tree_extract.py` 加 `--modules`（两个护栏共用、零自研解析）
   - 退路：`--ports` 的 module 名 ∪ `parents` ∪ `targets`，并**登记盲区**（零端口 module 在 `--ports` 里不可见）
2. **三种失败文本可区分**：
   - `0 根`（⇒ 若 `declared` 恰 1 ⇒ 视作**单 module 产物，合法绿**）
   - `>1 根`（⇒ 「多棵树不在本判据覆盖内（C2 拼接产物）」）
   - `根 ∈ instantiated`（⇒ 自实例化 / 重复例化）
3. **MUST NOT 硬编码** `top` **或**任何文件名（`top_module_name` 可配）

#### 脚本头 MUST 写三条**假阳性禁令**（否则文本扫描必误报）

- 根**体内**合法的 `__up_<n>` **wire**
- 根**实例端口映射**里合法的 `.__if_0(...)` / `.__x_0(...)`
- 用户自取的前缀名
⇒ ⇒ 只判 `isPrimaryIO` 的**端口声明**，任何 `grep` 文本扫描都不行。

### 2.3 夹具所有权（B1 已把选项**收敛为唯一一个**）

`verify_hierarchical_rtl.sh` 强制**恰好 3 个** `.v`（`:94-98`），且 R6/R8 对未知产物名 fail-closed
⇒ ⇒ **MUST NOT** 往那个目录加第 4 个。

| 选项 | 是否可用 | 理由 |
|---|---|---|
| **(a) 给产物生成器加独立子命令**（如 `hier_rtl_products --prefix-fixtures <dir>`）写**另一个目录** | ✅ **采用** | 产物由**发射器真实生成** ⇒ 才证明「发射器会生成 `__x_` / `__up_`」（B1 的全部价值） |
| (b) 新脚本自带**手写** `.v` fixture | ❌ **不可用** | 手写的 `.v` **不证明**发射器行为 ⇒ 与 B1 的意图直接冲突（等于 self-test 自证） |
| (c) 对复制品做 `module top` → `module my_dut` 的**文本替换** | ⚠️ **仅限 N3 的改名用例** | 改名产物**不是** B1 的需求（B1 要三个前缀的真实覆盖） |

⇒ ⇒ ⚠️ **B1 与 (b)/(c) 互斥** —— 这是用户拍板 B1 的直接推论，实施者 MUST NOT 图省事选手写 fixture。

### 2.4 `--self-test` 义务（断言化）

每条投毒 MUST：① 先 `expect_green`（干净基线）② 投毒后断言 `--json-only` **rc==0**
**且** `.tree.json` **存在**（否则是无效变异，MUST 如实登记）③ 还原复绿。

至少 **4** 条投毒：
- 顶层 `__if_`（N1）
- 顶层 `__up_`（N1 的补全区）
- 多根 / 零 CELL（N3 的两条分支）
- `top_module_name="my_dut"` 的产物（N3 的硬编码反例）

### 2.5 明确不做

- ✗ 不改发射器逻辑 · ✗ 不动 `verify_hierarchical_rtl.sh` 的 R1–R8
- ✗ 不修主 spec 的**已知相邻漂移**（Purpose 仍写「R1–R7」（`:20-21`）、R8 硬编码 `$1=="top"`（`:493`））
  ⇒ ⇒ 只在 §5 登记，避免下一个人以为新护栏是唯一结构判根的脚本

## 3. Acceptance (验收判据 —— MUST 可勾选)

### 3.1 判据本身

- [ ] N1 / N2a / N2b / N3 各有**独立**用例
- [ ] **N3 用非 `top` 的根名验证**：`top_module_name = "my_dut"` 的产物 ⇒ 判对
      （硬编码 `top` 的实现会在此失效）
- [ ] **N3 的零 CELL 分支**：单 module 产物（`rows == []`）⇒ **合法绿**
      （⇒ 这条同时证明 N3 不是「只要不是 `top` 就红」的假判据）
- [ ] **N3 的多根分支**：构造两棵独立树 ⇒ 红 + 文本**可区分**于「0 根」
- [ ] **N2b 绿**：`__if_` / `__x_` / `__up_` **各有一个真实产物**贡献覆盖
      ⇒ ⇒ 三个 fixture 的 DUT 形态 MUST 取自 §1.3 的两个既有先例
      （`tests/test_hierarchy_verilog.cpp:1637-1645` 的 `__up_` 三层链 · `:1873` 的 `__x_0`）
      ⇒ ⇒ ⚠️ **MUST NOT** 只靠 self-test 投毒充数（那是自证）
- [ ] **N2b 的负向对照**：临时移除某一个 fixture ⇒ 判据**转红**并点名缺哪个前缀

### 3.2 不误报

- [ ] `three_layers.v` / `bubble.v` **绿**（它们**本来就没有**合成端口 ⇒ 这是**不误报**判据）
- [ ] `row1_implicit.v` **绿** + 贡献 `__if_` 覆盖
- [ ] 根体内 `__up_0` wire ⇒ **不**判红
- [ ] 根实例映射 `.__if_0(...)` ⇒ **不**判红

### 3.3 fail-closed 与自检

- [ ] **N0 七类口径**逐类可触发：最简做法 = 临时移走提取器 / verilator / 生成器，各断言退出码
      非 0 **且**错误文本含对应类别（MUST NOT 报成「判据通过」或笼统「失败」）
- [ ] **`--self-test`** 4 条投毒全红 ⇒ 还原复绿；每条投毒前确认产物仍能被 `--json-only` 解析

### 3.4 零回归（具体化）

- [ ] `verify_hierarchical_rtl.sh` 主路径 + `--self-test`（9/9）仍绿
- [ ] `gen_products` 仍看到**恰好 3 个** `.v`（`:94-98`）
- [ ] R6 / R8 的 `EXPECTED` 表**不出现未知产物名**（`:300-303` / `:531-534`）
- [ ] `ctest -LE perf` 全绿；**计数锚点全部不变**（`CTEST_TOTAL` / `VERILATOR_E2E_TOTAL` / parity 两锚点）
- [ ] 新脚本用自己的 `mktemp -d` + `trap rm -rf` ⇒ **MUST NOT** 共享 `verify_hierarchical_rtl.sh` 的 `$WORK`
- [ ] 新增的 `__x_` / `__up_` fixture 产物落在**独立目录** ⇒ `verify_hierarchical_rtl.sh` 的
      `gen_products` 仍只看到**恰好 3 个** `.v`
- [ ] 既有 `tests/test_hierarchy_verilog.cpp` 的 `__x_` / `__up_` 断言**仍绿**
      （⇒ 搬形态 MUST NOT 改动它们的既有行为）

### 3.5 登记面（v1 完全漏了）

- [ ] **CI 挂载两处**：主路径 + `--self-test`，挂在 `pr-verilator-fast` 的
      `if: matrix.shard == 1` 下（与 `verify_hierarchical_rtl.sh` 同款，`.github/workflows/ci.yml:1378-1401`）
- [ ] **根 `AGENTS.md` COMMANDS** 增一行（与 `verify_hierarchical_rtl.sh` 并列，
      注明「需要 verilator · Ubuntu-only CI」）
- [ ] **roadmap** `feat-hierarchical-rtl-output` 的残留 5 行打勾 + 归档名
- [ ] **归档后手工替换 `## Purpose` 骨架**（openspec 生成的是
      `TBD - created by archiving change <change-name>`，**含 change 名** ⇒ 不替换即违反
      delta 规则 1，见 `docs/developer_guide/openspec-specs-guide.md:76-87`）
- [ ] `openspec validate <capability> --type spec --strict` 通过（按仓库钉的 **1.4.1**）

## 4. Capabilities (契约影响)

**建议新增** capability `hierarchical-port-naming-guard`（待用户确认）。

⚠️ **理由订正**（v1 的理由错了一半）：

| v1 的说法 | metis 核实 |
|---|---|
| 「MODIFIED 需逐字重述全文」 | ✅ **成立** —— `docs/developer_guide/openspec-specs-guide.md:73` 明文；且 `scripts/verify_openspec_delta.sh:124-149` 只校验**标题**逐字一致 ⇒ 「只写改动点」会**静默丢正文** |
| 「正文变长 ⇒ 踩 >500 字符自伤 ⇒ 主 spec 从 0 warning 变多条」 | ❌ **不成立** —— >500 字符是 openspec CLI **1.14.x** 的 strict 规则，仓钉 **1.4.1**、**CI 不跑** `openspec validate`；且 `rtl-hierarchical-output` 已有多条远超 500 字符的 Requirement（如 `:171-195`）⇒ 不存在「0 → 多条」 |

⇒ ⇒ **结论（新增 capability）保留，真实理由改为**：
**① 不制造第二份规范真相；② 不动既有长正文。**

**契约正文 MUST 只写「护栏机制义务」**（命名规则引用 `rtl-hierarchical-output` 为规范所有者，
**MUST NOT** 复述）：

1. **护栏存在性** —— 命名契约 MUST 由静态护栏核验；护栏 MUST 满足 N0 fail-closed
2. **N1/N2 的两面性** —— 既要有「根不得有合成端口」（N1），也要有「语料里确有被判对象」
   （N2a + N2b 覆盖矩阵）⇒ ⇒ 缺任一面都可能是恒绿
3. **N3 的结构判根** —— MUST 由结构判定，MUST NOT 硬编码根名 / 文件名
4. **判据面** —— 只判 `isPrimaryIO` 端口声明（MUST NOT 文本扫描）
5. **规范所有权** —— 命名规则本身归 `rtl-hierarchical-output`（本条**不复述**规则）

## 5. Impact (影响面)

| 面 | 影响 |
|---|---|
| 运行时 | 零（纯脚本） |
| 产物 | 零（不改发射器） |
| 测试 | 无 ctest 变化 ⇒ **计数锚点全部不变** |
| 依赖 | `verilator --json-only` · `scripts/lib/verilog_tree_extract.py`（可能需加 `--modules`）· `build/tests/hier_rtl_products` |
| 风险 | ① **N2b 需补两个 fixture**（B1 已拍板）⇒ 形态有先例（§1.3）⇒ 工程量可控；⚠️ 但 `__up_` 的产物受 **G3-5** 约束（根不得有合成端口）⇒ 新 fixture MUST 保持「合成端口只出现在非根」⇒ 否则产物直接 `ok=false`<br>② 前缀集合是**代码事实**（`hierarchy_verilog.h:46`）⇒ 将来加第四种前缀会漏 ⇒ 契约里指向 `Synthesized` 注释为单一真相<br>③ 根名与子层名撞名（`top_module_name="top"` 且某 layer 也叫 `top`）⇒ 产物有重复 `module` 声明 ⇒ `--json-only` 失败 ⇒ MUST 给**可区分**口径（MUST NOT 混进 N0 或 N1 的失败）<br>④ **本 change 从纯 chore 抬升**：加 fixture 意味着可能触及 `tests/hier_rtl_products.cpp` 或新增生成器子命令 ⇒ 若 `hier_rtl_products` 的改动影响既有 3 个产物的**逐字节**，则 `verify_hierarchical_rtl.sh` 的 R4（确定性）/ R6 / R8 会连带受影响 ⇒ MUST 先证明「新增子命令不影响默认路径输出」 |
| **已知相邻漂移（只登记不修）** | 主 spec Purpose 仍写「R1–R7」（`:20-21`）而脚本已 R1–R8；R8 硬编码 `$1=="top"`（`:493`）⇒ 本 change 不改 |

## 6. metis 第 1 轮 gap 分析的处置（逐条）+ 已拍板

| # | metis finding | 处置 |
|---|---|---|
| 1 | **R2 会误报**（`three_layers` / `bubble` 本就没有合成端口）⇒ 与「3 产物 PASS」互斥 | ✅ **采纳** ⇒ R2 重写为 **N2a（全局存在量词）+ N2b（覆盖矩阵）**；删「每个非顶层 module 至少一处」 |
| 2 | **`__up_` 今天零判别力**（G3-5 硬拒绝 + 只出现在根体内 wire）；v1 的动机错 | ✅ **采纳** ⇒ §1.2 如实改写 + §6 B |
| 3 | **N3 判根的术语陷阱**（第一列是容器、第三列是被实例化）+ 4 个反例 | ✅ **采纳** ⇒ §2.2 N3 三条实现要求 |
| 4 | N1 谓词太宽（需精确正则 + 只判 `isPrimaryIO`） | ✅ **采纳** ⇒ §2.2 N1 + 三条假阳性禁令 |
| 5 | 「不改发射器」与「造改名产物」矛盾 | ✅ **采纳** ⇒ §2.3 夹具所有权三选一 |
| 6 | 不能往共享产物目录加第 4 个 `.v` | ✅ **采纳** ⇒ §2.3 + §3.4 |
| 7 | MODIFIED 的理由错一半 | ✅ **采纳** ⇒ §4 理由订正 |
| 8 | 新增 capability 的 Purpose 骨架含 change 名 | ✅ **采纳** ⇒ §3.5 |
| 9 | R 编号撞名 | ✅ **采纳** ⇒ 改 `N0..N3` |
| 10 | N0 口径要列全（7 类） | ✅ **采纳** ⇒ §2.2 N0 |
| 11 | self-test 断言化 | ✅ **采纳** ⇒ §2.4 |
| 12 | 登记面验收（CI 两处 / AGENTS / roadmap） | ✅ **采纳** ⇒ §3.5 |
| 13 | `--modules` 共享库演进（避免 bash awk 违反「不自研解析」） | ✅ **采纳**（作为建议方案 a） |
| 14 | 建议顺手修主 spec 的 R1–R7 / R8 硬编码漂移 | ❌ **不修** ⇒ 违反「改动最小化」+ 与残留 5 无关 ⇒ 只在 §5 登记 |

### 已拍板（2026-11-06）

**B. N2b（前缀覆盖矩阵）** ⇒ **B1**：本 change **补 fixture**，使 `__if_` / `__x_` / `__up_`
各有一个**真实产物**用例 ⇒ 判据完整。

⇒ ⇒ 直接推论（已写进正文）：
1. **夹具所有权收敛为 (a)** —— 手写 `.v` 与文本替换都**不证明发射器行为**，与 B1 意图冲突（§2.3）
2. **新增 fixture 不得影响既有 3 个产物** —— R4/R6/R8 会连带受影响（§5 风险 ④）
3. **新 fixture MUST 遵守 G3-5**（合成端口只在非根）（§5 风险 ①）

⇒ ⇒ **无遗留待定项**。下一步 = `rdd-builder` P0。

### B2 的风险（仅存档，未采用）

`__x_` / `__up_` 只靠 self-test 投毒覆盖 ⇒ self-test 是**我们自己写**的投毒
⇒ ⇒ 它对「发射器**真的**会生成这两个前缀」**零证据**（典型假绿）。
用户已选择不接受这个缺口。
