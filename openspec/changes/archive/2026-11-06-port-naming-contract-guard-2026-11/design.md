# Design: 端口命名契约的静态护栏（`port-naming-contract-guard-2026-11`）

> 本文是 **builder 域**的定稿设计（`design.md` 无 skill owns ⇒ 由执行该 change 的
> agent 撰写）。**推导与证据**在 `proposal.md` 附录 A；**可勾选判据**在 `tasks.md`。
> ⚠️ 本 change **零运行时影响**（纯脚本 + 测试产物生成器扩展），**不改发射器**。

## 1. 设计目标与形态

**目标**：给「顶层端口不得带框架合成前缀」这条**已在主 spec** 的契约，加一个
**独立可跑**的静态护栏，且护栏本身**不可能恒绿**。

**形态约束（台账 §11.2）**：MUST 复用结构化提取（`verilator --json-only`）⇒
**MUST NOT** 自研 Verilog 文本解析器。

**编号**：护栏判据用 `N0..N3`（`N` = naming）⇒ ⇒ **不与**
`scripts/verify_hierarchical_rtl.sh` 的 `R1..R8` 撞名（否则日志里「R2 红了」指哪个？）。

## 2. 判据设计

### 2.1 N0 —— fail-closed（七类，各可区分）

| 类别 | 触发 | 错误前缀 |
|---|---|---|
| 缺 verilator | `command -v verilator` 失败（且不在 `build/verilator-install/bin/`） | `N0-MISSING-VERILATOR` |
| 缺产物生成器 | `build/tests/hier_rtl_products` 不可执行 | `N0-MISSING-GENERATOR` |
| 缺提取器 | `scripts/lib/verilog_tree_extract.py` 不存在 | `N0-MISSING-EXTRACTOR` |
| 缺 python3 | `command -v python3` 失败 | `N0-MISSING-PYTHON` |
| 生成器失败 | rc≠0 | `N0-GENERATOR-FAILED` |
| 产物数不符 | `.v` 数 ≠ 期望（3 主 + 2 fixture） | `N0-PRODUCT-COUNT` |
| 无 `.tree.json` | verilator `--json-only` 未产出 | `N0-NO-TREE-JSON` |

⇒ ⇒ 全部以**非 0** 退出；**MUST NOT** 报成「判据通过」。

### 2.2 N1 —— 根 module 不得有合成端口

- **谓词**：端口名匹配正则 **`^__(if|x|up)_[0-9]+$`**（**不是**「以 `__if_` 开头」
  ⇒ 用户可合法自取该前缀的名字 ⇒ 「开头」会假阳性）。
- **范围**：**只判 module 的一级端口声明** ⇒ ⇒ 由 `--module-ports`（结构化）供给。
  ⚠️⚠️ **实施期订正**：本节原写「只判 `isPrimaryIO` 的端口声明」—— **事实错误**：
  verilator 的 `isPrimaryIO` 语义 = **顶层 primary IO** ⇒ ⇒ **非顶层 module 的端口
  `isPrimaryIO` 是 `None`** ⇒ ⇒ 用 `--ports` 判「非根 module 有没有合成端口」会
  **恒绿**（假绿）。实测证据：`synth_x_child` 的合成端口 `__x_0` 的
  `isPrimaryIO=None` ⇒ ⇒ `--module-ports` 即为此而新增（见 §3）。
- **根的识别**：见 N3。
- ⚠️ **三条假阳性禁令**（写进脚本头注释 + 用例）：
  1. 根**体内**合法的 `__up_<n>` **wire**（`__up_` fixture 实测存在；`direction == 'NONE'`）
  2. 根**实例端口映射**里合法的 `.__if_0(...)` / `.__x_0(...)` / `.__up_0(...)`
  3. 用户自取的前缀名

### 2.3 N2a / N2b —— 存在量词 + 覆盖矩阵

- **N2a**：整个夹具集里，**非根** module 端口上至少出现 **1** 个匹配 N1 正则的端口。
  ⇒ 防「一条都判不到 ⇒ N1 恒绿」。
- **N2b**：`{__if_, __x_, __up_}` **每个** MUST 至少有一个**真实产物**覆盖；
  缺失 ⇒ 红并**点名缺哪个前缀**。
  ⇒ ⇒ **今天只有 `__if_`** ⇒ 故本 change **补两个 fixture**（见 §4）。

⚠️ **量词必须是「对语料」而非「逐 module」** —— 否则 `three_layers.v` /
`bubble.v`（本就没有合成端口）会被误报。

### 2.4 N3 —— 结构判根

- **声明集** `declared` = `verilog_tree_extract.py --modules` 输出的**全部** module 名
  （⚠️ **不是** `--ports` 的 module 名集合 ⇒ 后者对**零端口 module** 有盲区）。
- **被实例化集** `instantiated` = `extract()` 第三列的集合。
- **根** = `declared − instantiated` ⇒ MUST 恰好 **1** 个。
  ⚠️ `extract()` 第一列是**容器** module（它**包含**实例），第三列才是**被实例化**的
  module ⇒ ⇒ 「不在第一列」是**反的**（R6 就用 `parents − targets` 算，
  `scripts/verify_hierarchical_rtl.sh:320-324`）。
- **四种情形**（文本必须可区分）：

  | 情形 | 判定 | 语义 |
  |---|---|---|
  | `declared == 1` 且 `instantiated == ∅` | **绿** | 单 module 产物（零 CELL）⇒ 合法 |
  | `declared − instantiated == 1` | **绿** | 常规多 module 产物 |
  | `declared − instantiated == 0` | **红** | 根被实例化（自实例化 / 重复例化） |
  | `declared − instantiated > 1` | **红** | 多棵独立树（C2 拼接场景）⇒ 文本点名 |

  ⚠️ 「孤儿 module」（有子、从未被实例化）会被算成第二个根 ⇒ ⇒ 该情形归入
  `>1 根`（**文本点名**「可能是孤儿 module」）。

### 2.5 正则与「只判端口声明」的落地

- 正则在**脚本内**用 `awk`（对 `--module-ports` 的 TSV 逐行匹配）⇒ ⇒ **不是**对
  `.v` 文本做 grep ⇒ ⇒ 天然满足「只判 module 的一级端口声明」。
- ⚠️ **MUST NOT** 在 bash 里 awk/grep `.v` 文本来找 module 声明 ⇒ 那等于自研解析器。

## 3. 提取器扩展（`scripts/lib/verilog_tree_extract.py`）

新增**两个**模式：

| 模式 | 函数 | 用途 |
|---|---|---|
| `--modules` | `extract_modules(tree)` | **全部声明**的 module 名（按声明序）⇒ ⇒ **零端口 module 也在此可见**（`extract_ports` 只返回有 `isPrimaryIO` 的 module ⇒ 有盲区） |
| `--module-ports` | `extract_module_ports(tree)` | **每个 module 自己**的端口声明（`type=='VAR'` 且 `direction ∈ {INPUT,OUTPUT}`，**只看一级 `stmtsp`**）⇒ ⇒ 精确对应 module 头的 `input`/`output` 声明行 ⇒ ⇒ 体内 wire（`direction=='NONE'`）与实例映射（`CELL.pinsp`）**天然不在其中** |

⚠️ **`--module-ports` MUST NOT 用 `extract_ports()`（`isPrimaryIO`）** ——
理由见 §2.2 的实施期订正（`isPrimaryIO` 只标顶层 ⇒ 非根 module 的端口会被漏掉
⇒ ⇒ N2a / N2b 会恒绿）。

```python
def extract_modules(tree: dict) -> list[str]:
    """产出全部**声明**的 module 名（按声明序）。

    ⚠️ 与 `extract_ports` 的差别：**零端口 module 也在此可见**
    （`extract_ports` 只返回有 `isPrimaryIO` 的 module ⇒ 有盲区）。
    """
```

⇒ ⇒ **两个护栏共用一份结构化实现** ⇒ ⇒ 护栏脚本**不**自己解析 `.tree.json`。
⇒ ⇒ ⚠️ **既有 `extract_ports()`（`--ports`）逐字未改** ⇒ ⇒ `verify_hierarchical_rtl.sh`
的 R8（顶层端口表）**不受影响**。

## 4. 产物生成器扩展（`tests/hier_rtl_products.cpp`）

新增**独立子命令**：

```
hier_rtl_products <out_dir> --prefix-fixtures <fixture_dir>
```

⇒ 产出 **2 个** fixture 到**另一个目录**（**MUST NOT** 写进 `verify_hierarchical_rtl.sh`
的共享产物目录 —— 那里强制**恰好 3 个** `.v`，且 R6/R8 对未知产物名 fail-closed）。

| fixture | DUT（搬自既有测试） | 触发的合成端口 |
|---|---|---|
| `synth_x.v` | `InnerRow2` / `OuterRow2`（`tests/test_hierarchy_verilog.cpp:228-256`）⇒ 父层读**子成员的内部寄存器** | 子侧 **output** `__x_0` |
| `synth_up.v` | `UpLeaf` / `UpMid` / `UpTop`（`:847-887`）⇒ 顶层读**孙层**的内部寄存器 | 每级 **output** `__up_0` |

⚠️ **子命令 MUST NOT 影响默认路径**（`hier_rtl_products <dir>` 的 3 个产物
**逐字节不变** ⇒ `verify_hierarchical_rtl.sh` 的 R4（确定性）/ R6 / R8 不受影响）。

⚠️ **G3-5 合规**：两个 fixture 的合成端口 MUST 只出现在**非根** module
（否则根有合成端口 ⇒ 发射器直接 `ok=false` ⇒ 产物根本生成不出来）。

## 5. 脚本结构（`scripts/verify_hierarchical_port_naming.sh`）

```
N0  fail-closed 七类
 ├─ 生成 3 主产物（共享语义，**独立目录**）
 ├─ 生成 2 fixture（独立目录）
 ├─ 对每个产物：verilator --json-only → .tree.json
 ├─ 提取：--ports（端口）+ --modules（声明集）+ extract（实例化集）
 ├─ N3 判根（4 种情形）
 ├─ N1 根端口正则
 ├─ N2a 全局存在量词
 └─ N2b 覆盖矩阵（逐前缀点名）
```

⇒ ⇒ **自己的 `mktemp -d` + `trap rm -rf`** ⇒ MUST NOT 共享
`verify_hierarchical_rtl.sh` 的 `$WORK`。

### 5.1 `--self-test`（逐条投毒 ⇒ 必红 ⇒ 还原复绿）

| # | 投毒 | 期望 |
|---|---|---|
| A | 把一个**根端口**改名成 `__if_9` | N1 转红 |
| B | 把一个**根端口**改名成 `__up_9` | N1 转红（补全区） |
| C | 造一个**多根**产物 | N3 转红（`>1 根`） |
| D | 造一个**改名根**（`module my_dut`）且脚本仍硬编码 `top` | N3 转红（判根失败） |

⚠️ 每条投毒 MUST 先 `expect_green`（干净基线）⇒ 投毒后 MUST 断言
`verilator --json-only` **rc==0** **且** `.tree.json` **存在** ⇒ 否则是**无效变异**
（MUST 如实登记）。

## 6. 逐文件改动清单

| 文件 | 改动 | 性质 |
|---|---|---|
| `scripts/lib/verilog_tree_extract.py` | + `extract_modules()` · + `--modules` CLI | **新增**（向后兼容：默认行为不变） |
| `tests/hier_rtl_products.cpp` | + 2 个 fixture DUT · + `--prefix-fixtures` 子命令 | **新增**（默认路径**逐字节不变**） |
| `scripts/verify_hierarchical_port_naming.sh` | **新文件** | 护栏本体 |
| `openspec/specs/hierarchical-port-naming-guard/spec.md` | 归档时新建（ADDED ×4） | spec |
| `AGENTS.md` | COMMANDS 段 + 1 行（护栏用法） | 文档 |
| `.rddf/roadmap/features/feat-hierarchical-rtl-output.md` | 残留 5 行打勾 + 归档名 | 记账 |
| `.github/workflows/ci.yml` | 挂载**两处**（主路径 + self-test） | CI |

⇒ ⇒ **零** `include/` / `src/` 改动 ⇒ ⇒ **不触及公共 API 面**
⇒ ⇒ `verify_hierarchical_p8.sh` 的 R4（窗口已钉死在 T3-005）**不受影响**。

## 7. 失败语义表（对外唯一载体 = 退出码 + stderr 文本）

| 退出码 | 含义 |
|---|---|
| 0 | 全部判据通过 |
| 1 | 判据**失败**（N1 / N2a / N2b / N3 命中） |
| 2 | **前置失败**（N0：缺工具 / 生成器失败 / 产物数不符 / 无 `.tree.json`） |

⇒ ⇒ 退出码 1 与 2 **可区分** ⇒ ⇒ CI 能区分「护栏发现问题」与「环境没配好」。

## 8. 明确不做（与 proposal 的 Non-Goals 一致）

- ✗ 不改发射器 · ✗ 不动 `verify_hierarchical_rtl.sh` 的 R1–R8
- ✗ 不复述命名规则（规范所有者 = `rtl-hierarchical-output`）
- ✗ 不修主 spec 的相邻漂移（R1–R7 措辞 / R8 硬编码 `top`）⇒ 只登记
- ✗ 不用手写 `.v` 或文本替换充当 `__x_` / `__up_` 覆盖
- ✗ 不引入新依赖
