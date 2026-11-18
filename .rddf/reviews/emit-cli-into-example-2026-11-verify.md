# 验收报告 —— `emit-cli-into-example-2026-11`

- **验收者**：verifier（`source: user` · session `ses_e123fdf50ffekNVuofg777ESL3`）
- **日期**：2026-11-18
- **HEAD**：`ddb0bbb`
- **已用轮次**：**1 / 3**
- **判据来源**：`openspec/changes/emit-cli-into-example-2026-11/proposal.md` 的 `## 验收标准`（12 条 → 实为 AC1–AC14）+ 交付 commit `55f3f5a`
- **结论**：**ac-fail** —— 12 条通过 · **2 条不通过**（AC4 · AC9）· 1 条附注（AC5 的落盘证据与 AC4 同源缺失）

---

## 一、逐条判定 + 证据

| AC | 判定 | 取证（命令 ⇒ 输出摘要） |
|---|---|---|
| **AC1** | ✅ | `#COLUMNS` 含 `emitter_args`（13 列）；`--self-test` 投毒17（把 axi4 行 `emitter_args` 置 `-`）⇒ `✅ 自检 投毒17（emitter_args 置 - ⇒ 默认模式收到未知参数）：转红 ✓（③ 发射失败）` |
| **AC2** | ✅ | `emitter` / `emitter_args` 的列号由 `colidx()` 从 `#COLUMNS` 推导（`scripts/verify_examples_rtl_sim.sh:106-146`，无位置绑定）；`--self-test`「必绿：中间插列（值非空）」⇒ `✅ 仍绿 ✓` |
| **AC3** | ✅ | 12 条数据行字段数 == 13 == `#COLUMNS` 词数（0 条不符）；具名 fail 文案在 `:164`：`::error::清单行字段数 ${#F[@]} ≠ #COLUMNS 词数 ${NCOLS:-0}` |
| **AC4** | ❌ | **护栏不写 `argv.txt` / `argv.norm`**：`grep -n argv scripts/verify_examples_rtl_sim.sh` 仅命中 3 处 python `sys.argv`（无证据文件）；实测 `RTL_SIM_WORK=<tmp>` 跑完整轮后，`<tmp>/<target>/` 只有 `emit.log` / `run.log` / `manifest.json` / `top.v` / `obj` / `regen`，**无 argv 文件**。发射调用在 `:465`：`"$emitter_bin" $eargs --out "$work" --emit-hygiene >"$work/emit.log" 2>&1`（命令只进日志，未落 argv/norm） |
| **AC5** | ⚠️ **通过（附注）** | ① **两不同 `--out` 逐字节相同**：`top.v` / `manifest.json` 各 `cmp` 相同，且产物内 `grep -c <临时目录>` = **0**（无路径泄露）② **与 change 2 基线逐字节相同**：`tb_axi4_lite_example --emit-rtl --out … --emit-hygiene --emit-tb-verif …` ⇒ `top.v` = `20e0f88e34d97e55…`、`manifest.json` = `cf7b40f6c49ba178…`，与 `tasks.md:102` 记录的基线 sha **逐字相同** ③ **flag token 序列**：基线（`emit_rtl_axi4_lite --out … --emit-hygiene --emit-tb-verif …`）与新（`tb_axi4_lite_example --emit-rtl --out … --emit-hygiene --emit-tb-verif …`）剥 `argv[0]` + 模式标记后**逐 token 相同**——⚠️ 该比较由我按 tsv 的 `emitter_args=--emit-rtl` 手工推导，**落盘证据不存在**（与 AC4 同一缺陷） |
| **AC6** | ✅ | `emit_rtl_axi4_lite` 在 `examples/**` / `examples/CMakeLists.txt` / `tests/CMakeLists.txt` grep = **0**；`build/examples/axi4/emit_rtl_axi4_lite` 不存在；`examples/axi4/emit_rtl.cpp` 不存在。**投毒复跑**：放 dummy 到 `build/examples/axi4/emit_rtl_axi4_lite` ⇒ `run_all_ported_tests.sh` 报 `STALE-ARTIFACT  emit_rtl_axi4_lite`（且未把它报成 `UNTRACKED`）⇒ 清理后归零 |
| **AC7** | ✅ | `verify_examples_rtl_sim.sh` PASS：**③**（发射 + 产物四件套）与 **⑥'**（新鲜度逐字节重现）均绿 |
| **AC8** | ✅ | **⑥''**（期望源锚点 ↔ 验证源 scenario，7 个，双向 1:1）绿 |
| **AC9** | ❌ | ① `rtl_emitters()` **零改动** ✓（`git diff --stat 55f3f5a^..55f3f5a -- scripts/verify_examples_rtl_sim_coverage.sh` ⇒ 空）② 但 **PASS 计数 27 ≠ AC 的 28**：`./run_all_ported_tests.sh` ⇒ `PASS: 27 FAIL: 1`，27/27 执行全 PASS，`FAIL: 1` 来自 C1 的既有 `UNTRACKED emit_rtl_rv32i_soc_fw_init`。差 1 = change 3 的 `axi4_full_example` 退役（AC27 批准），**不是**「同一 target 兼 tracked + emitter」产生的 ±1 |
| **AC10** | ✅ | `verify_examples_rtl_sim_coverage.sh` 零改动（同上）+ 实跑 PASS：`守恒 27 == 10 ∪ 17`（原 28 == 10 ∪ 18，差 1 同上） |
| **AC11** | ✅ | 仓根默认模式 ⇒ `exit=0`，stdout `RESULT: checks=113 fails=0` |
| **AC12** | ✅ | `mktemp -d` 空目录内：`--bogus` ⇒ `exit=2`（stderr `FATAL: 未知参数（默认模式只接受无参）`）；`--emit-rtl`（缺 `--out`）⇒ `exit=2`（`FATAL: --emit-rtl 缺 --out <dir>（拒绝回落到默认 rtl_out/）`）；目录内 **无 `rtl_out/`**；识别按选项语法（`rest[i]=="--out"` + 值非空且首字符非 `-`） |
| **AC13** | ✅ | `examples/**` 旧名 `emit_rtl_axi4_lite` grep = **0**；`examples/axi4/AGENTS.md` FILES 表 + RUN 段已含 `--emit-rtl`（`:12,13,35-36`）；`examples/axi4/CMakeLists.txt` 活注释旧名归零（`:29-31`/`:41` 无匹配）；`docs/CHANGELOG.md` 有专条（该条正文按定义会提到被删的旧名） |
| **AC14** | ✅ | `git diff --stat 55f3f5a^..55f3f5a -- examples/axi4/emit_rtl_soc.cpp` ⇒ **空**（零 diff）；该文件其后由 change `example-file-boundary` 删除（§4.3.3） |

---

## 二、失败项与路由建议

| AC | 失败形态 | 分类 | 路由 |
|---|---|---|---|
| **AC4** | 护栏未产出 `argv.txt` / `argv.norm` 两份证据 | `implementation_gap` | **回 builder**：在 ③ 的发射调用点前后落两份文件（`printf '%s\n' "$emitter_bin" $eargs --out "$work" --emit-hygiene` → `<work>/argv.txt`；剥 `argv[0]` + 模式标记后的 flag token → `<work>/argv.norm`），并在 `--self-test` 里补一条「`argv.norm` 含由 `emitter_args` 透传的 `-` ⇒ 必红」的反向判据（否则落盘仍无判别力） |
| **AC9** | PASS 计数 27 ≠ 28 | **AC 数字过时**（与 change 3 AC27 冲突）+ 既有 `UNTRACKED` 使脚本 exit=1 | **planner** 订正 AC9 措辞；`UNTRACKED` 缺口**独立立项**（与 change 1 的 AC10 同一笔） |

---

## 三、遗留清扫（本次范围内）

| 项 | 结果 |
|---|---|
| 投毒残留 | `build/examples/axi4/emit_rtl_axi4_lite` dummy **已删除**（AC6 复跑后立即清理，`ls` 复核为空） |
| `--self-test` 的仓内副作用 | 自检自身「还原后复绿 ✓」；本次复核 17 投毒全绿 |
| 临时目录 | `/tmp/opencode/vfy/{oA,oB,work,fc,m1,m2,m3}**`（未跟踪） |
| 仓内源文件 | 未改动（`git status --short` 空） |

---

## 四、结构性记账缺口

| 缺口 | 证据 |
|---|---|
| **tasks.md 全未回填** | 任务行 **30** · ✅ **0** · ☐ **30**（交付物已存在，任务板显示未开工） |
| AC5 / AC4 的证据同源缺失 | 「flag token 序列」无落盘载体 ⇒ AC5 的比较口径只能人工推导 |

---

## 五、复现命令清单

```bash
# AC1 / AC2 / AC6（投毒与必绿全在 self-test 里）
bash scripts/verify_examples_rtl_sim.sh --self-test        # 17 投毒 + 1 必绿 + 还原复绿

# AC3 / AC4（字段数与 argv 证据文件）
python3 - <<'PY'
cols=None
for l in open('.rddf/checks/examples-rtl-sim-targets.tsv'):
    if l.startswith('#COLUMNS:'): cols=l[len('#COLUMNS:'):].split(); continue
    if l.startswith('#') or not l.strip(): continue
    assert len(l.rstrip('\n').split('\t'))==len(cols), l
print("字段数守恒 OK · 列数 =", len(cols))
PY
W=$(mktemp -d); RTL_SIM_WORK="$W" bash scripts/verify_examples_rtl_sim.sh >/dev/null; find "$W" -name 'argv*'   # ⇒ 空（AC4 缺口）

# AC5（产物等价 + 路径无关）
A=$(mktemp -d); B=$(mktemp -d)
./build/examples/axi4/tb_axi4_lite_example --emit-rtl --out "$A" --emit-hygiene --emit-tb-verif examples/axi4/tb_axi4_lite_example.verif
./build/examples/axi4/tb_axi4_lite_example --emit-rtl --out "$B" --emit-hygiene --emit-tb-verif examples/axi4/tb_axi4_lite_example.verif
sha256sum "$A/top.v" "$A/manifest.json"      # ⇒ 20e0f88e… / cf7b40f6…（change 2 基线）
cmp "$A/top.v" "$B/top.v" && echo 路径无关

# AC6（STALE-ARTIFACT 投毒）
printf '#!/bin/sh\nexit 0\n' > build/examples/axi4/emit_rtl_axi4_lite && chmod +x build/examples/axi4/emit_rtl_axi4_lite
./run_all_ported_tests.sh | grep STALE-ARTIFACT       # ⇒ emit_rtl_axi4_lite
rm -f build/examples/axi4/emit_rtl_axi4_lite

# AC9 / AC10
./run_all_ported_tests.sh; echo "exit=$?"              # ⇒ PASS 27 / FAIL 1（既有 UNTRACKED）
bash scripts/verify_examples_rtl_sim_coverage.sh

# AC12
D=$(mktemp -d); (cd "$D" && "$OLDPWD/build/examples/axi4/tb_axi4_lite_example" --bogus; echo "exit=$?")   # ⇒ 2
(cd "$D" && "$OLDPWD/build/examples/axi4/tb_axi4_lite_example" --emit-rtl; echo "exit=$?")               # ⇒ 2
ls "$D" | grep -c rtl_out                                                                                # ⇒ 0
```
