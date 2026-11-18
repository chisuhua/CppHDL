# tasks — `emit-cli-into-example-2026-11`

> ⚠️ **设计不在本文件**：决策面 = ADR-074 + 蓝图 §4.3。
> ⚠️ **顺序硬约束**：**T-D1（采基线）MUST 先于 T-D2（删旧 target）**；
> **T-D5（STALE-ARTIFACT 投毒）MUST 在 T-D3（tsv 换 emitter）之后** —— 否则 dummy 会被
> `rtl_emitters()` 认领（C1 直接 `continue`），永远不报 `STALE-ARTIFACT`。

## 批次 A — 参数化调用契约（AC1 · AC2 · AC3 · AC4）

| # | 任务 | 落点 | 判据 | 状态 |
|---|---|---|---|---|
| A1 | 清单增 `emitter_args` 列（13 列，`#COLUMNS` 同步）⇒ **无参写 `-`** | tsv | AC1 | ☐ |
| A2 | 护栏按**列名**取 `emitter` + `emitter_args`；在 **2 处**调用点组装（③ `:449` · ⑥' `:571`），跳过 `-`；**并写两份证据**：原始命令行 → `<work>/argv.txt`、归一化（剥离模式标记）→ `<work>/argv.norm` | `scripts/verify_examples_rtl_sim.sh` | AC4 | ☐ |
| A3 | `run_all_ported_tests.sh` 的 `rtl_emitters()` **零改动**确认 ⇒ 并在 tsv 的 note 列登记「emitter 集合与 tracked 集合**不再互斥**」 | 同上 + tsv | AC9 | ☐ |
| A4 | `verify_examples_rtl_sim_coverage.sh` **零改动**确认（列数/列号本就从 `#COLUMNS` 推导） | 同上 | AC10 | ☐ |
| A5 | **字段数守恒判据**：每行字段数 != `#COLUMNS` 词数 ⇒ **具名 fail**（行号 + 期望字段数 + 提示「空单元 ⇒ 无参写 `-`」）⇒ 把 `set -u` 的无名 abort 变成可归因的红 | `scripts/verify_examples_rtl_sim.sh` | AC3 | ☐ |
| A6 | **④″ 保持原状**（`--check-products` 仍硬编码）⇒ 已登记残留；**并在该处写注释说明「本调用故意不读 `emitter_args`」**（否则将来 `kind=soc` 行需要额外参数时会被静默忽略） | `:653` | — | ☐ |

## 批次 B — self-test fixture 与投毒（AC1 · AC2 · AC3 的耐久载体）

| # | 任务 | 落点 | 判据 | 状态 |
|---|---|---|---|---|
| B1 | fixture 的 `#COLUMNS` + 3 行数据变 **13 列**（否则 `--self-test` 基线直接 exit 2） | 同上 | — | ☐ |
| B2 | **fixture 的 axi4 行改名**：`emitter` → `tb_axi4_lite_example`、`emitter_args` → `--emit-rtl`（否则 D2 删掉 target 后基线必红） | 同上 | — | ☐ |
| B3 | **重写投毒 12 的 sed**：现用固定 8 字段 regex（`\t` 计数），插入 `emitter_args` 后**不匹配** ⇒ 必须改为按列名/`awk -F'\t' -v c=…` 构造 | 同上 | — | ☐ |
| B4 | 新增投毒 `empty_emitter_args`（**钉在 axi4 行**：参数改 `-` ⇒ ③ 必红） | 同上 | AC1 | ☐ |
| B5 | 新增**必绿**用例 `insert_column`（中间插一列 + 同步 `#COLUMNS`，**值非空**）⇒ 需给 `run_case` 加 `expect_green` 分支（现有契约只有「必红」） | 同上 | AC2 | ☐ |

## 批次 C — example 的发射模式（AC11 · AC12）

| # | 任务 | 落点 | 判据 | 状态 |
|---|---|---|---|---|
| C1 | `main()` 支持发射模式：**位置无关**剥离该标记 ⇒ 其余 argv 透传 `run_emit_cli` | `examples/axi4/tb_axi4_lite_example.cpp` | AC11 | ☐ |
| C2 | **factory 复用**：根组件名 MUST 仍是 `"top"`（与原薄壳逐字一致）⇒ **两条模式建不同的根**（验证模式用 `ch_device`） | 同上 | AC5 | ☐ |
| C3 | **fail-closed 两跳**：默认模式收到未知参数 ⇒ exit 2；`--emit-rtl` 缺 `--out` ⇒ exit 2 | 同上 | AC12 | ☐ |
| C4 | 默认模式在**仓库根**运行 ⇒ exit 0 且 stdout 含 `RESULT: checks=113 fails=0` | 同上 | AC11 | ☐ |
| C5 | 两条 fail-closed 都在 `mktemp -d` 新建**空**目录下判 ⇒ 且该目录 MUST NOT 出现 `rtl_out/`；`--out` 缺失识别 MUST **按选项语法**（`--out` + 值） | shell | AC12 | ☐ |

## 批次 D — 基线与删薄壳（AC5 · AC6）

| # | 任务 | 落点 | 判据 | 状态 |
|---|---|---|---|---|
| D1 | **采基线（MUST 在删除之前）**：`emit_rtl_axi4_lite --out /tmp/opencode/axi4_base --emit-hygiene --emit-tb-verif examples/axi4/tb_axi4_lite_example.verif` ⇒ 记 `flag token 序列`（**不含 `argv[0]`**）+ sha256(`top.v`) + sha256(`manifest.json`) | note | AC5 | ☐ |
| D2 | 删 `examples/axi4/emit_rtl.cpp` + CMake target `emit_rtl_axi4_lite` ⇒ **并同步该文件里的活注释**（`:29-31`「每个 example 一个独立 target」· `:41`「不与 `emit_rtl_axi4_lite` 合流」⇒ 已成假陈述） | `examples/axi4/` + CMakeLists | AC6 · AC13 | ☐ |
| D3 | 清单该行：`emitter` → `tb_axi4_lite_example`、`emitter_args` → `--emit-rtl` | tsv | AC5 | ☐ |
| D4 | **对照**：新入口产出的 `top.v` + `manifest.json` 与 D1 基线**逐字节相同** ⇒ ⇒ 且**flag token 序列**（丢弃 `argv[0]` + 剥离模式标记后）与 D1 **逐 token 相同**（原始 argv 必然差「二进制名 + 一个模式标记」⇒ **不得**按原始比） | diff | AC5 | ☐ |
| D5 | **STALE-ARTIFACT 投毒（MUST 在 D3 之后）**：手工放 dummy 到 `build/examples/axi4/emit_rtl_axi4_lite` ⇒ 跑 `./run_all_ported_tests.sh` ⇒ 报 `STALE-ARTIFACT` 且不报 `UNTRACKED` | 投毒 | AC6 | ☐ |
| D6 | **路径无关性**：同一 flag 序列下两个不同 `--out` 路径 ⇒ 产物仍逐字节相同（防路径泄露到 manifest/产物） | diff | AC5 | ☐ |

## 批次 E — 文档与验收（AC7 · AC8 · AC9 · AC10 · AC13）

| # | 任务 | 落点 | 判据 | 状态 |
|---|---|---|---|---|
| E1 | `examples/axi4/AGENTS.md`：FILES 表删 `emit_rtl.cpp` 行；RUN 段命令改为 `tb_axi4_lite_example --emit-rtl …` | 该文件 | AC13 | ☐ |
| E2 | `docs/CHANGELOG.md` 一行 | 同上 | AC13 | ☐ |
| E3 | `examples/axi4/CMakeLists.txt` 的活注释已随 D2 同步（`:29-31` / `:41`） | CMakeLists | AC13 | ☐ |
| E4 | 旧名 `emit_rtl_axi4_lite` 归零（白名单：`docs/archive/**` · `openspec/changes/archive/**` · `docs/architecture/**` 历史记述 · **self-test fixture 行**） | grep | AC13 | ☐ |
| E5 | `bash scripts/verify_examples_rtl_sim.sh` + `--self-test` 绿（**不加 `\|\| true`**） | 护栏 | AC7 · AC8 | ☐ |
| E6 | `bash scripts/verify_examples_rtl_sim_coverage.sh` 绿（守恒） | 护栏 | AC10 | ☐ |
| E7 | `./run_all_ported_tests.sh` 绿（28 tracked · PASS 计数仍 28 · C1 无 UNTRACKED） | 护栏 | AC9 | ☐ |
| E8 | **零 diff 判据**：`git diff --stat <base>..HEAD -- examples/axi4/emit_rtl_soc.cpp` 为空（⇒ 硬顺序「本 change 先于 3.2」的机械依据） | shell | AC14 | ☐ |

## 复核命令

```bash
cmake --build build -j$(nproc)
bash scripts/verify_examples_rtl_sim.sh
bash scripts/verify_examples_rtl_sim.sh --self-test
bash scripts/verify_examples_rtl_sim_coverage.sh
./run_all_ported_tests.sh

# 默认模式（AC11）
./build/examples/axi4/tb_axi4_lite_example | tail -1

# fail-closed（AC12）；在空目录下判，避免 rtl_out/ 旧残留干扰
t=$(mktemp -d)
( cd "$t" && /workspace/project/CppHDL/build/examples/axi4/tb_axi4_lite_example --bogus; echo "exit=$?"; ls )
( cd "$t" && /workspace/project/CppHDL/build/examples/axi4/tb_axi4_lite_example --emit-rtl; echo "exit=$?"; ls )
rmdir "$t"
```

## ⚠️ 未做 / 本次不做

| 项 | 原因 |
|---|---|
| `emit_rtl_soc.cpp` 的合并 / checker 下沉 | owner = `example-file-boundary-2026-11` 的 **3.2**（受 architect 门约束）；**硬顺序**：本 change 先于 3.2 |
| ④″ 的 `--check-products` 迁进 `emitter_args` | 会让该参数在 ③ 也生效（同一 checker 跑两遍 + 失败归因混淆）⇒ 保留原状（已登记残留） |
| RV32I 侧 3 个样板薄壳 | 影响面不同源（`scripts/rv32i_rtl_sim_env.sh` + CI/文档）⇒ 独立立项 |
| 改 `split_row` 解析原语 | 牵动 `F[]` 全部消费点 ⇒ 独立重构（本 change 用 `-` 哨兵 + 字段数守恒判据） |
| 库层 `run_emit_cli` 语义 | 零变化 |


---

## 实施实况（2026-11-18 · builder）

| 批次 | 状态 | 取证摘要 |
|---|---|---|
| A（清单 + 护栏按列名） | ✅ | tsv 12 → **13 列**（`emitter_args` · 无参 `-`）· 护栏 ③/⑥' 两处按列名取参 · 字段数守恒具名 fail · ④″ 加「故意不读 `emitter_args`」注释 |
| B（self-test） | ✅ | fixture 13 列 · axi4 行改 `tb_axi4_lite_example` + `--emit-rtl` · 投毒12 改按列名 awk · **投毒17**（args 置 `-` ⇒ ③ 必红）· **必绿用例**（插列 ⇒ 仍绿）+ `run_case_green` |
| C（tb 双模式） | ✅ | `--emit-rtl` 位置无关剥离 · 默认模式无参才跑验证 · factory 复用根名 `"top"` |
| D（删文件 + 对照） | ✅ | 基线 sha256（`top.v` 20e0f88e… · `manifest.json` cf7b40f6…）⇒ 新入口**逐字节相同** · 删 `emit_rtl.cpp` + target · tsv 改行 · **STALE-ARTIFACT 判据实测转红**（旧二进制残留 ⇒ 报 `STALE-ARTIFACT emit_rtl_axi4_lite`）⇒ 清理后归零 |
| E（文档 + 护栏） | ✅ | `examples/axi4/AGENTS.md`（FILES / RUN）· `docs/CHANGELOG.md` · CMakeLists 注 · 旧名归零 · `verify_examples_rtl_sim.sh` **PASS** · `--self-test` **PASS（17 投毒 + 1 必绿）** · `coverage` **PASS** · `run_all_ported_tests.sh`：`STALE-ARTIFACT 0`（仅剩既有 `UNTRACKED emit_rtl_rv32i_soc_fw_init`，change `axi4-verif-driven-cpp` 已登记） |

### AC 取证

| AC | 证据 |
|---|---|
| AC1（空 args 必红） | 投毒17：`emitter_args` = `-` ⇒ ③ 发射失败 ✓ |
| AC11（默认模式在仓根） | `RESULT: checks=113 fails=0` · exit 0 ✓ |
| AC12（fail-closed 两条） | 未知参数 ⇒ exit **2** · `--emit-rtl` 缺 `--out` ⇒ exit **2** · CWD 零落盘 ✓ |
| AC13（文档同步） | AGENTS.md + CHANGELOG + 旧名归零 ✓ |
| AC14（零 diff `emit_rtl_soc.cpp`） | 未触碰该文件 ✓ |
| AC5（产物等价） | sha256 三者逐字节相同 ✓ |
