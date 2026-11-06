# 任务清单：`port-naming-contract-guard-2026-11`

> 判据来源：proposal.md §Acceptance（摘要）· capability delta
> `hierarchical-port-naming-guard`（**ADDED ×4**）· improvement 草稿 v3
> `.rddf/improvements/port-naming-contract-guard-2026-11.md` §3（24 条）
> ⚠️ **零运行时影响**（纯脚本 + 产物生成器扩展）⇒ ⇒ **不改发射器**、**不改 `include/`**。
>
> **实施状态（2026-11-06）**：全部完成 ⇒ 归档前**唯一待办** = §6.4 的
> 「归档后手工替换 `## Purpose` 骨架」。

## T1 结构化提取器扩展（`--modules` + `--module-ports`）

- [x] 1.1 `scripts/lib/verilog_tree_extract.py` 加 `extract_modules(tree)`
      ⇒ ⇒ 输出**全部声明**的 module 名（按声明序）
      ⇒ ⇒ ⚠️ **零端口 module 也在此可见**（`extract_ports` 只返回有 `isPrimaryIO`
      的 module ⇒ 有盲区 ⇒ ⇒ N3 的声明集 MUST 用它）
      ⇒ ⇒ **已落**（`extract_modules()`）
- [x] 1.2 加 `--modules` CLI（TSV / `--json` 两种输出，与既有 `--ports` 同款）
      ⇒ ⇒ **默认行为逐字节不变**（不带 flag 时输出与改前一致）
      ⇒ ⇒ **已落**（`main()` 新增 `--modules` 分支）
- [x] 1.3 加 `extract_module_ports(tree)` + `--module-ports` CLI
      ⇒ ⇒ ⚠️ **实施期新增**（原计划用 `--ports` ⇒ **事实错误**，见下方「实施期发现」）
      ⇒ ⇒ 判据 = **一级 `stmtsp`** 里 `type=='VAR'` 且 `direction ∈ {INPUT,OUTPUT}`
      ⇒ ⇒ **实测**：5 个产物上根 module 均正确取到 4 个端口（含 `default_clock` /
      `default_reset` / `unnamed_input` / `unnamed_output`）· 非根 module 的合成端口
      `__x_0` / `__up_0` 全部抓到（4 处）· 体内 wire（`direction='NONE'`）**未**被抓
      ⇒ ⇒ 根的**实例映射** `.__x_0(...)` **未**被抓（CELL 的 `pinsp` 不在一级 `stmtsp`）
- [x] 1.4 **既有 `--ports` 逐字未改** ⇒ ⇒ `verify_hierarchical_rtl.sh` 的 R8 不受影响
      ⇒ ⇒ **已验**（`verify_hierarchical_rtl.sh` 主路径 + `--self-test` 全绿）

## T2 产物生成器扩展（2 个真实 fixture）

- [x] 2.1 `tests/hier_rtl_products.cpp` 加 `--prefix-fixtures <dir>` 子命令
      ⇒ ⇒ 产出到**独立目录**（**MUST NOT** 写进 `verify_hierarchical_rtl.sh` 的
      共享产物目录 —— 那里强制**恰好 3 个** `.v`，R6/R8 对未知产物名 fail-closed）
      ⇒ ⇒ **已落**（`emit_prefix_fixtures()` + `main()` 的子命令分支）
      ⇒ ⇒ ⚠️ **只产出 2 个 fixture**，**不**产出主产物 ⇒ ⇒ 共享目录的「恰好 3 个」不变
- [x] 2.2 fixture `synth_x.v`：搬 `InnerRow2` / `OuterRow2`
      （`tests/test_hierarchy_verilog.cpp:228-256`）⇒ 父层读**子成员的内部寄存器**
      ⇒ ⇒ 子侧合成 **output** `__x_0`
      ⇒ ⇒ **已落**（`SynthXChild` / `SynthXTop`）⇒ ⇒ **实测**产物含
      `output [7:0] __x_0`（子 module 头）+ `assign __x_0 = top_child__reg;`
      + 根的实例映射 `.__x_0(top_unnamed_output)`
- [x] 2.3 fixture `synth_up.v`：搬 `UpLeaf` / `UpMid` / `UpTop`
      （`:847-887`）⇒ 顶层读**孙层**的内部寄存器 ⇒ ⇒ 每级合成 **output** `__up_0`
      ⇒ ⇒ **已落**（`SynthUpLeaf` / `SynthUpMid` / `SynthUpTop`）⇒ ⇒ **实测**产物含
      `synth_up_leaf` 与 `synth_up_mid` **各一个** `output [7:0] __up_0`
      + 根的实例映射 `.__up_0(top_unnamed_output)`
- [x] 2.4 **G3-5 合规**：两个 fixture 的合成端口 MUST 只出现在**非根** module
      ⇒ ⇒ **实测**：两个 fixture 的根 module（`top`）的端口表 = 4 个且**无**合成前缀
      ⇒ ⇒ 产物**成功产出**（若有合成端口落在根上 ⇒ 发射器直接 `ok=false`）
- [x] 2.5 **默认路径逐字节不变**：`hier_rtl_products <dir>` 的 3 个产物
      **逐字节相同**（改前 vs 改后 `cmp` 0）
      ⇒ ⇒ **已验**（对照脚本：备份 → `git checkout HEAD --` → 重建 → 生成 → `cmp`
      → 从**未变异**备份还原 → `sha256` 比对 → 重建 → 复验）
      ⇒ ⇒ **实测**：`bubble.v` / `row1_implicit.v` / `three_layers.v` **3/3 逐字节相同**
      ⇒ ⇒ `DIFF_COUNT = 0`
- [x] 2.6 **生成器自建输出目录**（幂等）⇒ ⇒ 缺目录时发射器原本会报
      `cannot open '... .tmp'`（错误信息指向**路径**而非根因）⇒ ⇒ 已修

## T3 护栏脚本（`scripts/verify_hierarchical_port_naming.sh`）

- [x] 3.1 **N0 fail-closed 七类**（各可区分的文本 + 非 0 退出码 **2**）：
      缺 verilator / 缺产物生成器 / 缺提取器 / 缺 python3 / 生成器 rc≠0 /
      产物数≠期望 / 无 `.tree.json`
      ⇒ ⇒ **已落**（错误前缀 `N0-MISSING-VERILATOR` · `N0-MISSING-GENERATOR` ·
      `N0-MISSING-EXTRACTOR` · `N0-MISSING-PYTHON` · `N0-GENERATOR-FAILED` ·
      `N0-PRODUCT-COUNT` · `N0-NO-TREE-JSON`）
      ⇒ ⇒ **实测**：缺提取器 ⇒ 退出码 **2** + 文本点名（见 §T4 的 N0 投毒）
- [x] 3.2 **N1**：根 module 的端口名 MUST NOT 匹配 **`^__(if|x|up)_[0-9]+$`**
      （⚠️ **精确正则**，不是「以 `__if_` 开头」）
      ⇒ ⇒ **已落**（`awk` 对 `--module-ports` 的 TSV 匹配）
      ⇒ ⇒ ⚠️ **判根失败时 N1 MUST NOT 静默跳过**（否则「找不到根」= N1 恒绿 = 假绿）
      ⇒ ⇒ 已加：判根失败 ⇒ N1 明确报「无法判定」并计入失败
- [x] 3.3 **N2a**：整个夹具集里，非根 module 端口上 MUST 至少出现 **1** 个匹配端口
      ⇒ ⇒ **已落** ⇒ ⇒ **实测**：语料含 **4** 个（`__x_0` ×1 + `__up_0` ×2 + `__if_0` ×1）
- [x] 3.4 **N2b 覆盖矩阵**：`{__if_, __x_, __up_}` **每个** MUST 至少有一个
      **真实产物**覆盖；缺失 ⇒ 红并**点名缺哪个前缀**
      ⇒ ⇒ **已落** ⇒ ⇒ **实测**：三前缀**全覆盖**（`__if_` 来自 `row1_implicit.v` ·
      `__x_` 来自 `synth_x.v` · `__up_` 来自 `synth_up.v`）
- [x] 3.5 **N3 结构判根**：`declared − instantiated` 恰好 1；
      `declared` 来源 = `--modules`；**MUST NOT** 硬编码 `top` 或文件名
      ⇒ ⇒ **已落**（`comm -23`）⇒ ⇒ **实测**：5 个产物的根**全部**判对（`top`）
- [x] 3.6 **N3 的四种情形文本可区分**：单 module（零 CELL）= **绿** ·
      常规 = 绿 · 根被实例化 = 红 · 多根（>1）= 红
      ⇒ ⇒ **已落** ⇒ ⇒ **实测**：多根分支由投毒 C 触发（**转红**）
- [x] 3.7 **不误报**：根体内 `__up_0` wire **不**判红；根实例映射
      `.__if_0(...)` / `.__x_0(...)` / `.__up_0(...)` **不**判红
      ⇒ ⇒ **已落**（判据只取一级端口声明）⇒ ⇒ **实测**：5 个产物主路径**全绿**
      ⇒ ⇒ 而两个 fixture 的产物**确实**含根体内 `__up_0` wire 与根实例映射
      `.__x_0(...)` / `.__up_0(...)` ⇒ ⇒ 仍绿 ⇒ ⇒ 禁令① ② **实测有效**
- [x] 3.8 **不误报**：`three_layers.v` / `bubble.v`（本就没有合成端口）**绿**
      ⇒ ⇒ **实测**：主路径绿（它们对 N2a/N2b **不贡献**任何合成端口）
- [x] 3.9 **退出码可区分**：0 = 通过 · 1 = 判据失败 · 2 = 前置失败（N0）
      ⇒ ⇒ **已落** ⇒ ⇒ **实测**：N0 投毒 ⇒ 2；N1/N3 投毒 ⇒ 1
- [x] 3.10 **MUST NOT** 自研 Verilog 文本解析器 ⇒ ⇒ 端口/声明集/实例化集**全部**
      来自 `scripts/lib/verilog_tree_extract.py`
      ⇒ ⇒ **已落**（脚本只对 TSV 做 `awk` 筛选，不碰 `.v` 文本）
      ⇒ ⇒ ⚠️ **self-test 的投毒**确实要改 `.v` 文本 ⇒ 但那是**投毒**（构造坏输入），
      **不是**判据路径 ⇒ ⇒ 判据路径全程零文本解析
- [x] 3.11 自己的 `mktemp -d` + `trap rm -rf` ⇒ **MUST NOT** 共享
      `verify_hierarchical_rtl.sh` 的 `$WORK` ⇒ ⇒ **已落**

## T4 `--self-test`（逐条投毒 ⇒ 必红 ⇒ 还原复绿）

- [x] 4.1 每条投毒前先 `expect_green`（干净基线）⇒ ⇒ **已落** ⇒ ⇒ **实测**基线绿
- [x] 4.2 投毒 A：把一个**根端口**改名成 `__if_9` ⇒ N1 转红
      ⇒ ⇒ **已落** ⇒ ⇒ **实测转红** ✓
- [x] 4.3 投毒 B：把一个**根端口**改名成 `__up_9` ⇒ N1 转红（补全区）
      ⇒ ⇒ **已落** ⇒ ⇒ **实测转红** ✓
- [x] 4.4 投毒 C：造一个**多根**产物 ⇒ N3 转红（`>1 根`）
      ⇒ ⇒ **已落**（追加未被实例化的 `orphan_root` module）⇒ ⇒ **实测转红** ✓
- [x] 4.5 投毒 D：造一个**改名根**（`module my_dut`）⇒ 硬编码 `top` 的实现 MUST 转红
      ⇒ ⇒ **已落** ⇒ ⇒ **实测仍绿**（结构判根 ⇒ 不依赖名字）✓
      ⇒ ⇒ ⚠️ 这条是**反向对照**（证明「不依赖名字」）；**硬编码会失效**是推论
      ⇒ ⇒ 为补上「跟着改名后的根走」的正面证据 ⇒ ⇒ **新增投毒 E**
- [x] 4.6 **投毒 E（实施期新增）**：根改名 **且** 插 `__if_9` ⇒ N1 MUST **转红**
      ⇒ ⇒ 证明「护栏跟着改名后的根走」⇒ ⇒ **排掉**「改名后因找不到根而恒绿」这条假绿
      ⇒ ⇒ **实测转红** ✓
- [x] 4.7 每条投毒后 MUST 断言 `verilator --json-only` **rc==0** **且**
      `.tree.json` **存在** ⇒ 否则是**无效变异**（MUST 如实登记）
      ⇒ ⇒ **已落** ⇒ ⇒ **实测**：5 条投毒全部通过该断言（**无**无效变异）
- [x] 4.8 投毒**还原后** MUST 复绿（逐字节比对 + 重建）
      ⇒ ⇒ **已落** ⇒ ⇒ **实测**：还原后复绿 ✓
- [x] 4.9 **N0 fail-closed 的 self-test**：缺提取器 ⇒ 退出码 **2** + 文本点名
      ⇒ ⇒ **已落** ⇒ ⇒ **实测** ✓

## T5 零回归

- [x] 5.1 `verify_hierarchical_rtl.sh` 主路径 + `--self-test` 仍绿
      ⇒ ⇒ **实测**：主路径 PASS（R1–R8）· `--self-test` PASS
- [x] 5.2 `gen_products` 仍见**恰好 3 个** `.v` ⇒ ⇒ **实测**（新护栏的 N0 校验：
      主产物数 = 3 · fixture 数 = 2）
- [x] 5.3 R6 / R8 的 `EXPECTED` 表**不出现未知产物名** ⇒ ⇒ **实测**（R6/R8 全绿）
- [x] 5.4 `ctest -LE perf` 全绿 · samples · examples
      ⇒ ⇒ **实测**：`ctest -LE perf` **240/240**（`100% tests passed, 0 tests failed
      out of 240` · 455.67 s）· samples **39/39** · examples **28/28**
- [x] 5.5 **计数锚点全部不变**（`CTEST_TOTAL` 251 / `VERILATOR_E2E_TOTAL` 76 /
      parity 66 / 289 / `EXAMPLES_TEST_SOURCES` 17）
      ⇒ ⇒ **实测**：`verify_test_counts.sh` PASS（251 = 251 · 76 = 76）·
      `verify_dual_backend_parity.sh` **66 / 289**（未变）
      ⇒ ⇒ 本 change **不新增 ctest 条目**（护栏是脚本，非 ctest 用例）
- [x] 5.6 `verify_hierarchical_p8.sh` 仍绿（**零** `include/` 改动 ⇒ R4 不受影响）
      ⇒ ⇒ **实测** PASS
- [x] 5.7 其余护栏全绿：`verify_hierarchical_tree_extract.sh` · `verify_openspec_delta.sh`
      · `verify_roadmap_fragments.sh` · `verify_header_self_contained.sh` ·
      `verify_agents_size.sh` ⇒ ⇒ **实测** 全 PASS

## T6 登记面

- [x] 6.1 **CI 挂载两处**：主路径 + `--self-test`，挂在 `pr-verilator-fast` 的
      `if: matrix.shard == 1` 下（与 `verify_hierarchical_rtl.sh` 同款）
      ⇒ ⇒ **已落**（`.github/workflows/ci.yml`）⇒ ⇒ **YAML 语法已校验**
- [x] 6.2 根 `AGENTS.md` COMMANDS 段增行（与 `verify_hierarchical_rtl.sh` 并列，
      注明「需要 verilator · Ubuntu-only CI」）
      ⇒ ⇒ **已落** ⇒ ⇒ `verify_agents_size.sh` PASS（51104 B ≤ 72 KiB）
- [x] 6.3 roadmap `feat-hierarchical-rtl-output` 的残留 5 行打勾 + 归档名
      ⇒ ⇒ **已落**（归档名 `2026-11-06-port-naming-contract-guard-2026-11` · 含
      实测证据 + ⚠️ **如实登记**「G3-5 已硬拒绝 ⇒ N1 在现有 3 个产物上结构性恒绿 ⇒
      护栏的真实价值 = 回归栅栏」）
- [x] 6.4 **归档后手工替换 `## Purpose` 骨架**
      ⇒ ⇒ ⚠️ **无需动作**：delta 里**已带** `## Purpose` 段 ⇒ ⇒ 归档时 CLI 直接带入
      ⇒ ⇒ **实测**主 spec 无 `TBD`、无 `created by archiving change <name>` 残留
      （grep 0 命中）⇒ ⇒ **未**违反 delta 规则 1
- [x] 6.5 `openspec validate hierarchical-port-naming-guard --type spec --strict` 通过
      ⇒ ⇒ **实测**：`Specification 'hierarchical-port-naming-guard' is valid`
      ⇒ ⇒ 全局 `openspec validate --specs --strict`：**17 passed → 18 passed**
      （failed 仍 **22** ⇒ 余量为存量 CLI 1.14.x 漂移 ⇒ ⇒ **无新增 warning**）
      ⇒ ⇒ 4 条 Requirement 正文 223 / 238 / 247 / 210 字符 ⇒ 全 ≤ 500

## 审查（critic · `build` 档 · 归档前增量审）

**第 1 轮**：✅ **APPROVE** · `mergeable: yes` · `scope_exceeded: false`
⇒ **0 BLOCKER** · **0 SHOULD_FIX** · **3 NIT**（**全部已采纳并修**）。

| NIT | 处置 |
|---|---|
| 脚本头注释「`top_module_name` 可配」⇒ 误示脚本内有可配置项 | ✅ 改为「根名**无需配置**（结构判定使配置不必要 ⇒ 且**不硬编码** `top` / 文件名）」 |
| `collect_data` 三条 `python3` 管道**未检查退出码** ⇒ 失败分类从 N0（rc=2）漂到判据失败（rc=1） | ✅ 三条各自检查退出码 ⇒ 失败即报「N0 提取失败」并返回非 0 |
| delta R3「根名可配置」措辞 ⇒ 实为「结构判定使配置不必要」 | ✅ delta 正文改为「MUST NOT 要求提供根名配置项」；Scenario 判据不变 |

⇒ ⇒ 修后复跑：主路径 PASS · `--self-test` PASS（6 条投毒仍全符合期望）·
`openspec validate --type change --strict` valid（4 条 Requirement 正文 223/238/247/210
字符 ⇒ 全 ≤ 500）。

⚠️ **登记**：critic 指出的「`output reg` 仍 `direction=OUTPUT`」是**推断未实测**
⇒ 但 CppHDL 发射器**只发 `input`/`output`** ⇒ 该形态在本仓**不可达** ⇒ 不是判别力缺口
⇒ ⇒ 记 backlog（若将来发射器支持 `output reg`，MUST 补一条投毒）。

完整审稿记录 = `.rddf/reviews/port-naming-contract-guard-2026-11-critic.md`。

---

## 实施期发现（如实登记 · 4 条）

### ① ★「只判 `isPrimaryIO` 的端口声明」是**事实错误**（原计划 ⇒ 实施期推翻）

**原计划**（proposal 草稿 / improvement §2.2）：「判据只判 `isPrimaryIO` 的端口声明」。

**实测**：`verilator` 的 `isPrimaryIO` 语义 = **顶层 primary IO** ⇒ ⇒
**非顶层 module 的端口 `isPrimaryIO` 是 `None`**：

| 节点 | `isPrimaryIO` | `direction` | 是不是端口声明 |
|---|---|---|---|
| `top.default_clock` | `True` | `INPUT` | ✅ 顶层端口 |
| `synth_x_child.default_clock` | **`None`** | `INPUT` | ✅ **端口**（`isPrimaryIO` 抓不到） |
| `synth_x_child.__x_0` | **`None`** | `OUTPUT` | ✅ **合成端口**（`isPrimaryIO` 抓不到） |

⇒ ⇒ **后果**：用 `--ports` 判「非根 module 有没有合成端口」⇒ **N2a / N2b 恒绿**
⇒ ⇒ 那正是本护栏**要防的假绿** ⇒ ⇒ 判据基础错误，**必须在实现期拦住**。

**修正**：改用 `--module-ports`（一级 `stmtsp` + `type=='VAR'` +
`direction ∈ {INPUT,OUTPUT}`）⇒ ⇒ 精确等于 module 头的 `input`/`output` 声明行
⇒ ⇒ 体内 wire（`direction=='NONE'`）与实例映射（`CELL.pinsp`）天然不在其中。
⇒ ⇒ 已加进 `scripts/lib/verilog_tree_extract.py`（既有 `--ports` **逐字未改**）。

### ② 投毒脚本 bug：投毒打到「第一个 module」而非**根** ⇒ N1 恒绿

**现象**：投毒 A（根插 `__if_9`）首轮**未生效**（判据仍绿）。
**根因**：产物里 module 的**声明顺序**里**根不一定在前**（实测 `synth_x.v` 的第一个
module 是 `synth_x_child`，根 `top` 在**最后**）⇒ ⇒ 投毒插到了**非根**上 ⇒ ⇒
N1（只查根）不红 ⇒ ⇒ 且 N2a 只要求「≥1」⇒ ⇒ 整体仍绿。
**修正**：投毒改为**按结构判出的根名**定位（`poison()` 接收根名参数）。
⇒ ⇒ 这条本身也是「**假绿的活样本**」：一个「看起来在投毒、其实没打到点上」的负向对照。

### ③ 投毒脚本 bug：投毒**污染** `ROOT_OF` ⇒ 后续投毒打错模块

**现象**：投毒 C（多根）之后，投毒 D 报「无法改写 `synth_up.v` · 根='orphan_root'」。
**根因**：`run_poison` 用**投毒后**的产物重算 `ROOT_OF` ⇒ ⇒ 下一条投毒用的是**坏根名**。
**修正**：`run_poison` 开头**先恢复基线数据**（重新 `collect_data` 基线产物）。

### ④ 生成器缺目录 ⇒ 错误信息指向**路径**而非根因

**现象**：`--prefix-fixtures` 指向不存在的目录 ⇒ 发射器报
`cannot open '<dir>/synth_up.v.tmp'` ⇒ ⇒ 诊断指向「路径」而非「目录不存在」。
**修正**：生成器**自建**输出目录（`std::filesystem::create_directories`，幂等）
⇒ ⇒ 默认路径与 fixture 路径**都**自建 ⇒ ⇒ 调用方不必记得先 `mkdir`。

---

## 判别力自证（投毒对照 · 每条先确认产物仍能被 verilator 解析 ⇒ **无**无效变异）

| 投毒 | 改动 | 期望 | 实测 |
|---|---|---|---|
| 基线 | 无 | 绿 | ✅ 5 产物 · N1/N2a/N2b/N3 全绿 |
| **A** `root_if` | 根 module 头插 `input [7:0] __if_9,` | 红 | ✅ **转红** |
| **B** `root_up` | 根 module 头插 `input [7:0] __up_9,` | 红 | ✅ **转红** |
| **C** `orphan` | 追加未被实例化的 `orphan_root` module | 红 | ✅ **转红**（多根） |
| **D** `rename_root` | 根改名 `module my_dut (` | **绿**（反向对照） | ✅ **仍绿** |
| **E** `rename_root_if` | 根改名 **且** 插 `__if_9` | 红 | ✅ **转红**（跟着改名后的根走） |
| **N0** | 提取器路径指向不存在的文件 | 退出码 **2** + 点名 | ✅ 退出码 2 + `N0-MISSING-EXTRACTOR` |
| 还原 | — | 绿 | ✅ 复绿 |

⇒ ⇒ **6 条判据/投毒全部具备判别力**（含 1 条反向对照 + 1 条 fail-closed 判据）。

## 验证命令

```bash
cd /workspace/project/CppHDL
set -euo pipefail

# ── 护栏本体 ──
bash scripts/verify_hierarchical_port_naming.sh              # 主路径（N0–N3）
bash scripts/verify_hierarchical_port_naming.sh --self-test   # 逐条投毒 ⇒ 必红

# ── 前置产物（护栏依赖）──
cmake --build build --target hier_rtl_products -j"$(nproc)"

# ── 零回归 ──
bash scripts/verify_hierarchical_rtl.sh                       # 主路径
bash scripts/verify_hierarchical_rtl.sh --self-test           # 9/9
bash scripts/verify_hierarchical_p8.sh                        # 公开面护栏
( cd build && ctest -LE perf --output-on-failure )            # 240/240
bash scripts/run_all_samples.sh                              # 39/39
./run_all_ported_tests.sh                                    # 28/28
bash scripts/verify_test_counts.sh                            # 计数锚点

# ── spec ──
openspec validate port-naming-contract-guard-2026-11 --type change --strict
bash scripts/verify_openspec_delta.sh
```
