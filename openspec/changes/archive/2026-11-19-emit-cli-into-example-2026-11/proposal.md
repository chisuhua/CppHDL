# Proposal: 发射入口并入 example（`--emit-rtl` 模式）+ 清单 `emitter_args` 列（`emit-cli-into-example`）

- **状态**: **立项（2026-11-18 · planner）** · 未开工 · **metis 第 1/2 轮已处置**
- **前置**: `rtl-verification-single-source-2026-11` —— **代码部分已提交、change 未归档**；
  其 `tasks.md` 的「本次不做」把「删 `emit_rtl.cpp`」**递延给本 change**（与蓝图 M5 一致）
  ⇒ ⇒ 本 change 是这次删除的**唯一**执行者（避免「按 C2 再删一次」或「按 M5 判越权」）
- **硬依赖**: **`axi4-verif-driven-cpp-2026-11` MUST 先落地**（改名）⇒ ⇒ 见「🚪 门 G-1」
- **上游决策**: ADR-074（Accepted）· 蓝图 `docs/architecture/2026-11-17-rtl-verification-single-source.md` §4.3

---

## Why

库层 `run_emit_cli()` 已是**完整**的发射 CLI（参数解析 / 层次化 / manifest / 诊断 fail-closed /
`--emit-tb-verif`）⇒ ⇒ 但**每个**要发射 RTL 的 example 仍要一个**纯样板**薄壳：

| 文件 | 行数 |
|---|---:|
| `examples/axi4/emit_rtl.cpp` | **32** |
| `examples/riscv-mini/emit_rtl.cpp` | 32 |
| `examples/riscv-mini/emit_rtl_fw_init.cpp` | 38 |
| `examples/riscv-mini/emit_rtl_hier.cpp` | 48 |

⇒ ⇒ 每新增一个要发射 RTL 的 example 就要**再复制一遍**「收集 argv + 注册设计 + 调库层」
⇒ ⇒ 正是用户 2026-11-17 诉求「希望项目代码不要过于碎片化」指向的**最后一组重复载体**。

⇒ ⇒ 且护栏自己也有**产品专属硬编码**：`scripts/verify_examples_rtl_sim.sh:653` 对 `kind=soc`
的行硬写 `--check-products` ⇒ ⇒ 那是「调用参数」混进了护栏代码 ⇒ ⇒ `emitter_args` 列正是它的载体
（本 change **只**把「发射器调用」参数化 ⇒ ④″ 的迁移见「本次不做」）。

### 路线取舍（**metis 要求书面交代**）

| 方案 | 新增契约 | 判据可表达性 | 结论 |
|---|---|---|---|
| A. 「出现 `--out` 即发射模式」 | **0**（不加列、不加标记） | ❌ 无法表达「同一二进制**两种模式**」的登记 ⇒ 清单只能写 target 名 ⇒ 「默认模式必须仍是验证模式」这条判据无处落 | 否 |
| **B. `--emit-rtl` 标记 + `emitter_args` 列** | 1 列 + 1 标记 | ✅ 清单能表达「target + 参数」⇒ 护栏可按行调用 | **选 B** |

⇒ ⇒ 选 B 的理由是**判据可表达性**，不是「更漂亮」；代价（1 列 + 1 标记）已在本 change 内付清。

⇒ ⇒ **终态诚实说明**：本 change 只收敛 **1 个** target（`emit_rtl_axi4_lite`）。
清单里其余 11 个 emitter（`examples/spinalhdl-ported/**` 与 RV32I 侧的 3 个）**仍是薄壳**
⇒ ⇒ 它们被 `scripts/rv32i_rtl_sim_env.sh` 与 CI/文档多处引用 ⇒ ⇒ 影响面不同源 ⇒ ⇒ **独立立项**
（本 change 只提供**机制**）。

## What Changes

| # | 变更 | 依据 |
|---|---|---|
| **C1** | **清单增 `emitter_args` 列**（12 → 13 列）⇒ ⇒ `emitter` = **target 名（单 token）**；`emitter_args` = **空格分隔的参数 token 序列**，**无参 MUST 写 `-` 哨兵**（见下「为什么不能用空单元」） | 蓝图 §4.3 |
| **C2** | **护栏按列名取参数**并在 **2 处**调用点使用：③ 发射（`:449`）· ⑥' 重新生成 TB（`:571`）⇒ ⇒ 形态 `<target> <args…> --out … --emit-hygiene …`（**参数位置 MUST 无关** ⇒ 由调用方**剥离式**组装）⇒ ⇒ **④″（`:653`，SoC `--check-products`）故意不读本列**（迁移会让该 flag 在 ③ 也生效）⇒ ⇒ 该决定 MUST 写进护栏注释 | C1 |
| **C3** | **`tb_axi4_lite_example` 的 `main()` 支持 `--emit-rtl`**：剥离该标记（**位置无关**）⇒ 其余 argv 透传 `run_emit_cli`；默认模式 MUST 仍是验证模式 | 蓝图 §4.3 |
| **C4** | **fail-closed 两条**：① 默认模式收到**任何**未知参数 ⇒ 退出码 2；② `--emit-rtl` 缺 `--out` ⇒ 退出码 2（**MUST NOT** 落到库层默认 `rtl_out/` ⇒ 否则写进 CWD） | metis P1-7/8 |
| **C5** | **删 `examples/axi4/emit_rtl.cpp`** + target `emit_rtl_axi4_lite`；清单该行 `emitter` → `tb_axi4_lite_example`、`emitter_args` → `--emit-rtl` | C3 |
| **C6** | **产物等价性**：新入口的 `top.v` + `manifest.json` 与原入口**逐字节相同**（基线在删除**之前**采集） | 合并不得改产物 |
| **C7** | **self-test fixture 同步**：`verify_examples_rtl_sim.sh` 的 fixture `#COLUMNS` 与 3 行数据变 13 列；新增 **2 条投毒**（`empty_emitter_args` 必红 · `insert_column` 必绿） | AC1/AC2 的耐久载体 |
| **C8** | **文档同步**：`examples/axi4/AGENTS.md`（FILES 表 + RUN 段命令）· `docs/CHANGELOG.md` | PHASE COMPLETION GATE |

### 为什么不能用「空单元」（**本 change 最硬的实现约束**）

`scripts/verify_examples_rtl_sim.sh:157` 是 `split_row() { IFS=$'\t' read -r -a F <<<"$1"; }`
⇒ ⇒ **TAB 是 IFS 空白字符 ⇒ 连续 TAB 被折叠、空字段不保留**
⇒ ⇒ `emitter_args` 若留空 ⇒ 该行后续字段**整体左移**（`note` 读越界、`status` 读到 `tb`）
⇒ ⇒ 11 个 `generated=no` 行会**齐红**（`note` 长度判据 `:341`）。

⇒ ⇒ ⇒ **MUST 用 `-` 哨兵表示「无参」**，且护栏 MUST NOT 把 `-` 传给发射器。
⇒ ⇒ （替代方案「改解析原语为 `awk -F'\t'`」会牵动 `F[]` 的全部消费点 ⇒ ⇒ 本 change 不采用；
若将来要做，属独立重构。）

---

## 验收标准

**12 条**

| AC | 判据（**可机械判定**） | 载体 |
|---|---|---|
| AC1 | 清单 MUST 有 `emitter_args` 列；**负向对照**：把 axi4 行的参数改成 `-` ⇒ 护栏 MUST 必红（**红因是 fail-closed**：默认模式收到 `--out` ⇒ 未知参数 ⇒ exit 2 ⇒ 不产产物） | 投毒 |
| AC2 | `emitter` / `emitter_args` 两列 MUST 都从 `#COLUMNS` 推导；**负向对照**：中间插一列（**值非空**）⇒ 仍绿 | 投毒 |
| AC3 | **字段数守恒**：每行字段数 MUST == `#COLUMNS` 词数 ⇒ 不等 ⇒ **具名 fail**（「该行字段数 ≠ `#COLUMNS`（空单元？无参 MUST 写 `-`）」）⇒ ⇒ 把 `set -u` 的无名 abort 变成可归因的红 | 护栏 |
| AC4 | **实参级判据**：护栏写两份证据 —— 原始命令行 → `<work>/argv.txt`；**归一化命令行** → `<work>/argv.norm`，归一化 = **丢弃 `argv[0]`（发射器路径）** + 剥离模式标记，只留 **flag token 序列** ⇒ 原始 argv MUST 含 `--emit-rtl` ⇒ ⇒ ⚠️ **2026-11-18 订正（可判化）**：判据 = 「`argv.norm` MUST NOT 含**恰好等于 `-` 的 token**」（判据命令 `grep -qx -- '-' <work>/argv.norm` 期望**非零**）；原「两文件 MUST NOT 含由 `emitter_args` 透传的 `-`」**不可判**（「来源」在文件内容里不可观测，且 argv 必然含以 `-` 开头的 flag）⇒ 作废 | grep |
| AC5 | **产物逐字节等价**：基线在删除旧 target **之前**采集；比较口径 = **flag token 序列**（两侧均不含 `argv[0]`）逐 token 相同；且**两个不同 `--out`** 路径下产物仍逐字节相同（防路径泄露） | diff |
| AC6 | `emit_rtl_axi4_lite` target MUST NOT 再存在；**残留判定** = 在 **D3 之后**手工放 dummy 到 `build/examples/axi4/emit_rtl_axi4_lite` ⇒ 跑 `run_all_ported_tests.sh` ⇒ 报 `STALE-ARTIFACT` 且不报 `UNTRACKED` | 投毒 |
| AC7 | 护栏 ③ 与 ⑥' MUST 仍绿 | 护栏 |
| AC8 | ⑥'' 新鲜度 MUST 仍绿 | 护栏 |
| AC9 | `run_all_ported_tests.sh` 的 C1 归属判据 MUST 仍绿（**27** tracked · **PASS 计数 27** ⇒ 同一 target 兼 tracked + emitter 不得产生 ±1）⇒ `rtl_emitters()` **零改动** ⇒ ⇒ ⚠️ **2026-11-18 订正（用户裁决 D4）**：原「仍 28」与 change `example-file-boundary-2026-11` 的 AC27（退役 ⇒ 28 → 27）**互斥** ⇒ ⇒ 以 **27** 为准 | shell |
| AC10 | `verify_examples_rtl_sim_coverage.sh` 守恒 MUST 仍绿（**27 == 10 ∪ 17**；⚠️ **2026-11-18 订正**：原 28/18 与 change `example-file-boundary-2026-11` 的 AC27 退役冲突 ⇒ 以实测为准）⇒ **该脚本零改动** | shell |
| AC11 | 默认模式（无 `--emit-rtl`）在**仓库根**运行 ⇒ exit 0 且 stdout 含 `RESULT: checks=113 fails=0` | shell |
| AC12 | **fail-closed**：默认模式收到未知参数 ⇒ exit **2**；`--emit-rtl` 缺 `--out` ⇒ exit **2** ⇒ ⇒ 两条都在 **`mktemp -d` 新建空目录**下判，且该目录 MUST NOT 出现 `rtl_out/`；`--out` 缺失的识别 MUST **按选项语法**（`--out` + 值）而非裸 token 扫描 | shell |
| AC13 | 文档同步：`examples/axi4/AGENTS.md`（FILES 表 + RUN 段）· `docs/CHANGELOG.md` · **`examples/axi4/CMakeLists.txt:29-31` 与 `:41` 的活注释**（「每个 example 一个独立 target」/「不与 `emit_rtl_axi4_lite` 合流」已成假陈述）⇒ 旧名归零（白名单：`docs/archive/**` · `openspec/changes/archive/**` · `docs/architecture/**` 历史记述 · self-test fixture **行**） | grep |
| AC14 | **`examples/axi4/emit_rtl_soc.cpp` 零 diff**（`git diff --stat <base>..HEAD -- <该文件>` 为空）⇒ ⇒ 这是「本 change 先于 3.2」这条硬顺序的**机械依据** | diff |

### AC ↔ task 映射

| AC | task |
|---|---|
| AC1 · AC2 · AC3 · AC4 | **A1** · **A2** · **A5** · **B2** · **B3** |
| AC5 | **D1** · **D4** · **D6** |
| AC6 | **D5**（**MUST 在 D3 之后**） |
| AC7 · AC8 | **E5** |
| AC9 · AC10 | **A3** · **A4** · **E7** |
| AC11 · AC12 | **C4** · **C5** |
| AC13 | **E1**–**E4** |
| AC14 | **E8** |

---

## Capabilities

- **`examples-rtl-sim-coverage`** ⇒ ⇒ **ADDED** 一条 requirement（`emitter_args` 参数化调用 +
  `-` 哨兵 + 位置无关）

---

## 用户可见变更

| 项 | 前 | 后 |
|---|---|---|
| AXI4-Lite 的 RTL 发射入口 | `build/examples/axi4/emit_rtl_axi4_lite --out <dir>` | `build/examples/axi4/tb_axi4_lite_example --emit-rtl --out <dir>` |
| 产物 | `top.v` + `manifest.json` | **逐字节相同**（AC4） |
| 二进制数 | 多一个 `emit_rtl_axi4_lite` | **无** |
| 清单 | 12 列 | 13 列（`emitter_args`，无参写 `-`） |

---

## Out of Scope（明确不做）

| 项 | 原因 |
|---|---|
| **`emit_rtl_soc.cpp` 的合并 / checker 下沉** | 它持有 SoC 产品级 checker（4 项判据）⇒ ⇒ 归 `example-file-boundary-2026-11` 的 **3.2 批次**（**该批次受 architect 门约束**）⇒ ⇒ 见下「唯一收口登记」 |
| **④″ 的 `--check-products` 迁进 `emitter_args`** | 会让该参数在 ③（发射）也生效 ⇒ 同一 checker 跑两遍、且 ④″ 的失败归因与 ③ 混淆 ⇒ ⇒ **本 change 保留 ④″ 原状**（已登记残留） |
| **RV32I 侧 3 个样板薄壳** | 被 `scripts/rv32i_rtl_sim_env.sh` 与 CI/文档多处引用 ⇒ 影响面不同源 ⇒ 独立立项 |
| `--emit-rtl` 做成库层通用模式 | 它是**调用方**的模式标记 ⇒ 库层不感知 ⇒ 零新增公开面 |
| 改 `parse_cli_options` 语义 | 本 change 只加**调用方**剥离逻辑 |
| 改 `split_row` 解析原语 | 会牵动 `F[]` 全部消费点 ⇒ 独立重构（本 change 用 `-` 哨兵绕开） |

### 唯一收口登记（**消除「都归对方」**）

`emit_rtl_soc.cpp` 的 checker 归属 ⇒ ⇒ **owner = `example-file-boundary-2026-11` 的 3.2 批次**；
**前提** = 该 change 的 G-1（architect 裁决）闭。本 change **不**碰该文件（AC 层面：零 diff）。

⇒ ⇒ **单向顺序**：`emit_rtl_soc.cpp` 的「零改动」要求与 3.2 互斥 ⇒ ⇒ 若 3.2 先落，本 change 的
「零 diff」判据失效 ⇒ ⇒ ⇒ **硬顺序 = 本 change 先于 3.2**。

---

## 🚪 门（**开工前 MUST 闭**）

| 门 | 内容 | 闭法 |
|---|---|---|
| **G-1** | **改名前置**：本 change 指 `tb_axi4_lite_example` | 开工前置：`test -f examples/axi4/tb_axi4_lite_example.cpp` |
| **G-2** | **基线先采**：AC4 的基线 MUST 在**删除旧 target 之前**采集 | tasks T4（基线）→ T5（删除+对照），**顺序不可交换** |
| **G-3** | **归档顺序**：capability 由 `rtl-verification-single-source-2026-11` 首次 ADDED 且尚未归档 | 实施期：前置 change 先归档（独立 commit） |

---

## 风险与回滚

| 风险 | 处置 | 回滚 |
|---|---|---|
| `-` 哨兵被当参数传给发射器 ⇒ 库层「未知选项」exit 2 | AC3 直接判；护栏跳过 `-` | — |
| 空单元导致字段左移 ⇒ 11 行齐红 | 已用 `-` 哨兵规避（C1 的硬约束） | — |
| 根组件名漂移（`top` → `axi_top`）⇒ 产物不等价 | 实施约束：`--emit-rtl` 模式 MUST 复用原 factory（`AxiLiteTop(nullptr, "top")`）；**两条模式建不同的根**（验证模式用 `ch_device`） | 恢复 `emit_rtl.cpp` |
| 默认模式静默当作发射模式（漏写标记） | C4 的 fail-closed（未知参数 ⇒ exit 2） | — |
| 旧二进制残留判成 UNTRACKED | AC5 的投毒判据 + 该行为是**期望**（清构建目录即可） | — |

## 关联 change

- **前置**: `rtl-verification-single-source-2026-11` · `axi4-verif-driven-cpp-2026-11`（**硬依赖**）
- **后者**: `example-file-boundary-2026-11`（3.2 批次 · 受 architect 门约束）
