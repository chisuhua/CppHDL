# Proposal: 端口命名契约的静态护栏（`port-naming-contract-guard-2026-11`）

## Why

### 契约已在案，护栏不在

主 spec `openspec/specs/rtl-hierarchical-output/spec.md` 已明文约束顶层端口命名
（Purpose `:10-11` + 「悬空子输入 MUST 向上冒泡为顶层输入」Scenario `:186-187`）：

> **AND** 层次化侧该端口名 MUST **与扁平产物逐字相同**（= 该输入节点的 ctx 级全局名）
> ⇒ ⇒ MUST NOT 加任何框架前缀

但**没有任何静态护栏**看守它 ⇒ ⇒ 台账 §7 残留 5 的原话：「无 ⇒ 独立 change 加
`scripts/verify_*.sh`」。

### 唯一现成的结构化判据只覆盖 3 个固定产物

`scripts/verify_hierarchical_rtl.sh` 的 **R8**（顶层端口表 == 手推期望集）是唯一的
结构化端口判据，但它只对 `bubble.v` / `row1_implicit.v` / `three_layers.v` 三个**固定**
产物生效 ⇒ ⇒ **新设计形态零覆盖**（这才是残留 5 的实质，不是「少一条检查」）。

### ⚠️ 如实登记：现状比「没有护栏」更接近「已守住」

| 事实 | 出处 |
|---|---|
| 发射器已有 **G3-5 硬拒绝**：根 module 若有合成端口 ⇒ `ok=false` | `src/hierarchy_verilog.cpp:1113-1120` |
| `__up_` **不会**出现在根 module 的**端口表**：它只生成在「目标祖先的直接子 → 驱动端 module」链上，根拿到的是**体内 wire**（`logic [7:0] __up_0;`） | `src/hierarchy_verilog.cpp:516,555-569` · `tests/test_hierarchy_verilog.cpp:1637-1645` |

⇒ ⇒ 因此本护栏的 R1 在**现有 3 个产物上结构性恒绿** ⇒ ⇒ 它的真实价值 =
**对 G3-5 被移除 / 被绕过、以及未来新产物形态的回归栅栏**（**不是**「关闭了一个正在
发生的缺口」）。⇒ `__up_` 纳入判据 = **契约补全（defense in depth）**，当前**零判别力**
⇒ ⇒ 本 change MUST 如实这样登记，**MUST NOT** 把它算成「补了一个洞」。

## What Changes

1. **新增护栏脚本** `scripts/verify_hierarchical_port_naming.sh`（+ `--self-test`）
   ⇒ 四条判据（编号用 `N0..N3`，避免与 `verify_hierarchical_rtl.sh` 的 R1–R8 撞名）：

   | 判据 | 内容 |
   |---|---|
   | **N0** | **fail-closed**：verilator / 产物生成器 / 提取器 / `python3` 缺失、生成器 rc≠0、产物数≠3、无 `.tree.json` ⇒ 各自报**可区分**的错误并以非 0 退出（MUST NOT 报成「判据通过」） |
   | **N1** | **根** module 中 `isPrimaryIO` 的端口名 MUST NOT 匹配精确正则 **`^__(if\|x\|up)_[0-9]+$`** |
   | **N2a** | **全局存在量词**：整个夹具集里，非根 module 端口上 MUST 至少出现 **1** 个匹配该正则的端口 |
   | **N2b** | **前缀覆盖矩阵**：`{__if_, __x_, __up_}` **每个** MUST 至少有一个**真实产物** fixture 使其出现在**非根** module 端口表；缺失 ⇒ 红并点名缺哪个 |
   | **N3** | **判根** = `declared − instantiated` **恰好 1**；`declared` 来源写死；三种失败（0 根 / >1 根 / 根 ∈ instantiated）**文本可区分** |

2. **给 `scripts/lib/verilog_tree_extract.py` 加两个模式**（⇒ ⇒ 两个护栏共用一份结构化
   提取实现 ⇒ ⇒ **MUST NOT** 在 bash 里 awk 端口表，那等于自研解析器）：
   - `--modules`：输出**全部声明**的 module 名（含零端口 module ⇒ 判根的输入）
   - `--module-ports`：输出**每个 module 自己**的端口声明
     （一级 `VAR` + `direction ∈ {INPUT,OUTPUT}`）

   ⚠️⚠️ **实施期订正**：原计划用既有的 `--ports`（`isPrimaryIO`）判端口 —— **事实错误**：
   verilator 的 `isPrimaryIO` 语义 = **顶层 primary IO** ⇒ ⇒ **非顶层 module 的端口
   `isPrimaryIO` 是 `None`** ⇒ ⇒ 用它判「非根 module 有没有合成端口」会**恒绿**
   （假绿）。实测：`synth_x_child` 的合成端口 `__x_0` 的 `isPrimaryIO=None`
   ⇒ ⇒ `--module-ports` 即为此而新增。⚠️ 既有 `--ports` **逐字未改** ⇒ ⇒
   `verify_hierarchical_rtl.sh` 的 R8 不受影响。

3. **给产物生成器加独立子命令**，产出 `__x_` / `__up_` 两个**新 fixture** 到**另一个目录**
   ⇒ ⇒ 形态取自既有先例（`tests/test_hierarchy_verilog.cpp:1637-1645` 的 `__up_0` 三层链 ·
   `:1873` 的 `__x_0` 合成 output）⇒ ⇒ **MUST NOT** 往 `verify_hierarchical_rtl.sh` 的共享
   产物目录加第 4 个 `.v`（该脚本强制恰好 3 个，R6/R8 对未知产物名 fail-closed）。

## 不做（Non-Goals）

- ✗ **不改发射器逻辑**（只加护栏）· ✗ **不动** `verify_hierarchical_rtl.sh` 的 R1–R8。
- ✗ **不复述命名规则**成第二份真相 —— 规范所有者仍是 `rtl-hierarchical-output`。
- ✗ **不修**主 spec 的**已知相邻漂移**（Purpose 仍写「R1–R7」而脚本已 R1–R8；
  R8 硬编码 `$1=="top"`）⇒ ⇒ 只登记，不在本 change 顺手改（改动最小化）。
- ✗ **不**用**手写** `.v` fixture 或**文本替换**充当 `__x_` / `__up_` 的覆盖
  ⇒ ⇒ 那两者都**不证明发射器行为**（等于 self-test 自证）。
- ✗ 不引入新依赖（verilator + python3 + 既有提取器已足够）。

## Acceptance

> 完整判据清单（24 条）见 `tasks.md` §3；此处是**摘要**。

- [ ] **N1** 精确正则（不是「以 `__if_` 开头」）+ 只判 `isPrimaryIO` 端口声明
- [ ] **N2a / N2b** 两面性：既有「根不得有合成端口」，也有「语料里确有被判对象」+
      **三前缀覆盖矩阵**
- [ ] **N3** 由结构判根（`declared − instantiated`），**MUST NOT** 硬编码 `top`/文件名
- [ ] **N0** fail-closed 的每一类都**可区分**（移走提取器 ⇒ 非 0 退出 + 明说缺什么）
- [ ] **不误报**：根体内 `__up_0` wire、根实例映射 `.__if_0(...)` **不**判红；
      `three_layers.v` / `bubble.v`（本就没有合成端口）**绿**
- [ ] `--self-test` 逐条投毒必红 ⇒ 还原复绿（投毒后 MUST 确认产物仍能被 `--json-only` 解析）
- [ ] **零回归**：`verify_hierarchical_rtl.sh` 主路径 + `--self-test` 仍绿；
      `gen_products` 仍见**恰好 3 个** `.v`；`ctest -LE perf` 全绿；**计数锚点全部不变**
- [ ] **登记面**：CI 挂载**两处**（主路径 + self-test）· 根 `AGENTS.md` COMMANDS 增行 ·
      roadmap 残留 5 打勾 · **归档后手工替换 `## Purpose` 骨架**（openspec 生成的骨架含
      change 名 ⇒ 不替换即违反 delta 规则 1）

## Capabilities

| capability | delta |
|---|---|
| `hierarchical-port-naming-guard` | **ADDED ×4**（护栏存在性 · 两面性 · 结构判根 · 判据面与 fail-closed） |

⇒ 归档**禁** `--skip-specs`。

⇒ ⇒ ⚠️ **capability 正文只写「护栏机制义务」**，命名规则**引用**
`rtl-hierarchical-output` 为规范所有者（MUST NOT 复述 —— 本仓纪律：两份必然漂移）。

---

## 附录 A：实测证据（`verilator --json-only` 的结构化提取）

### A.1 三个现有产物的合成端口实际分布

| 产物 | 合成端口 | 依据 |
|---|---|---|
| `three_layers.v` | **0** | 全部连接是端口↔端口（`tests/hier_rtl_products.cpp:47-63`） |
| `row1_implicit.v` | **1 个 `__if_0`** | `tests/hier_rtl_products.cpp:101` |
| `bubble.v` | **0** | 悬空输入走**冒泡**（origin=`Bubbled`，名字 = ctx 全局名） |

⇒ ⇒ **`__x_` / `__up_` 在整个产物集里零覆盖** ⇒ 这是 N2b 存在的理由
（也是它今天**会红**的原因 ⇒ 故本 change 补 fixture）。

### A.2 `__x_` / `__up_` 的既有触发形态（⇒ B1 补 fixture 的可行性依据）

| 前缀 | 既有先例 | 形态 |
|---|---|---|
| `__up_` | `tests/test_hierarchy_verilog.cpp:1637-1645` | 三层链：leaf 的 `output [7:0] __up_0` + `assign __up_0 = top_mid_leaf__reg;`，mid 的 `.__up_0(__up_0)`，top 的 `.__up_0(top_unnamed_output)` |
| `__x_` | `tests/test_hierarchy_verilog.cpp:1873` | 子 module 有一个**合成 output 端口** `__x_0`（父层读子内部节点） |

⇒ ⇒ 两个 fixture **不是从零构造** —— 形态可直接搬进产物生成器的独立子命令。

### A.3 判根的正确谓词（`extract()` 的列语义）

`scripts/lib/verilog_tree_extract.py::extract()` 产出 `(parent, instance, module)`：

- 第一列 `parent` = **容器** module（它**包含**实例）
- 第三列 `module` = **被实例化**的 module

⇒ ⇒ 判根 = `root ∈ {第一列} ∧ root ∉ {第三列}`（= `parents − targets`，
`scripts/verify_hierarchical_rtl.sh:320-324` 就是这么算的）。

⚠️ **四个反例必须被区分**（否则「不是 `top` 就红」会变成假判据）：

1. **零 CELL 产物**（单 module / 全 inline）⇒ `rows == []` ⇒ 此时根应是**唯一声明的
   module** ⇒ 合法**绿**
2. **多棵独立树**（未来 C2 拼接产物）⇒ `>1 根` ⇒ 文本须可区分
3. **孤儿 module**（有子、从未被实例化）⇒ 会被误算成第二个根
4. **自实例化 / 同 module 多处例化** ⇒ 根进第三列 ⇒ `roots == ∅`

⇒ ⇒ 故 N3 的声明集来源 MUST 写死（`--modules` 输出全部声明，而非只看 `--ports`
的「有端口的 module」⇒ 后者对**零端口 module** 有盲区）。

### A.4 三条**假阳性禁令**（任何文本扫描都会踩）

1. 根**体内**合法的 `__up_<n>` **wire**
2. 根**实例端口映射**里合法的 `.__if_0(...)` / `.__x_0(...)`
3. 用户自取的前缀名（合成名的确切形态恒为 `prefix + 十进制序号`，
   `src/hierarchy_verilog.cpp:210-217`）

⇒ ⇒ 判据 MUST 只看 module 的**一级端口声明** ⇒ ⇒ **MUST NOT** 自研文本解析器。

### A.5 ⚠️ **实测**：`isPrimaryIO` **不能**用来判「非根 module 的端口」

verilator 的 `isPrimaryIO` 语义 = **顶层 primary IO** ⇒ ⇒ 非顶层 module 的端口
`isPrimaryIO` 是 `None`。实测（`synth_x.v` 的 `.tree.json`）：

| 节点 | `type` | `isPrimaryIO` | `direction` | 是不是端口声明 |
|---|---|---|---|---|
| `top.default_clock` | `VAR` | `True` | `INPUT` | ✅ 顶层端口 |
| `synth_x_child.default_clock` | `VAR` | **`None`** | `INPUT` | ✅ **端口**（但 `isPrimaryIO` 抓不到） |
| `synth_x_child.__x_0` | `VAR` | **`None`** | `OUTPUT` | ✅ **合成端口**（`isPrimaryIO` 抓不到） |
| `synth_x_child.top_child__reg` | `VAR` | `None` | `NONE` | ❌ 体内 wire |
| `top` 的 CELL `child` 的 `pinsp` 里的 `__x_0` | `PIN` | — | — | ❌ **实例映射**（合法） |

⇒ ⇒ ⇒ 判据 MUST 用「**一级 `stmtsp` + `type=='VAR'` + `direction ∈ {INPUT,OUTPUT}`**」
⇒ ⇒ 它精确等于 module 头的 `input`/`output` 声明行
（Verilog 里 `wire` 无方向 ⇒ `direction` 恒 `'NONE'` ⇒ 体内线网天然被排除）；
⇒ ⇒ 且只看**一级** ⇒ ⇒ CELL 的 `pinsp`（实例映射）天然不在其中。
