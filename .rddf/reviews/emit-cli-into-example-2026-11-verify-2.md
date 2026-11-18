# 第二轮复核 —— `emit-cli-into-example-2026-11`

- **验收者**：verifier（`source: user` · session `ses_e123fdf50ffekNVuofg777ESL3`）
- **日期**：2026-11-18 · **HEAD**：`67c5b24`
- **已用轮次**：**2 / 3**
- **复核范围**：第 1 轮判 ❌ 的 2 条（AC4 · AC9）+ 因措辞订正而基准变化的 1 条（AC10）+ 新增 diff 是否引入新 BLOCKER。
- **结论**：**AC4 ✅ · AC9 ✅ · AC10 ✅ ⇒ 14/14 条全部成立**（归档顺序受 D6-1 约束）

## 一、逐条判定 + 证据

| AC | 第 1 轮 | 本轮 | 证据（本轮实跑） |
|---|---|---|---|
| **AC4** | ❌（护栏不写 `argv.txt` / `argv.norm`） | ✅ | `ac48bef` 新增 ③'：两文件落盘 + 三条断言。实跑（单行清单，axi4 行）：`✅ ③' argv 双证据（argv.txt 5 token · argv.norm 无孤立 '-'）`；`argv.txt` = 5 行且含 `--emit-rtl`；`argv.norm` = 3 行且 `grep -qx -- '-'` **不命中**。判别力：`--self-test` 的**投毒19**（`RTL_SIM_TAMPER=argv_norm_dash`，在 ③ **成功后**注入）⇒ 转红原因 = `③' argv.norm 含孤立` ⇒ 与投毒17（`emitter_args` 置 `-` ⇒ ③ 发射失败）**正交** |
| **AC9** | ❌（PASS 计数 27 ≠ 28） | ✅ | 订正后口径 = **27**。实跑：`PASS: 27 FAIL: 0` · `跟踪 27 == 实际执行 27` · 无 `UNTRACKED`；`rtl_emitters()` 仍零改动（`git diff --stat 55f3f5a^..55f3f5a -- run_all_ported_tests.sh` 空） |
| **AC10** | ✅（附注：28 == 10 ∪ 18 过时） | ✅ | 订正后口径 = `27 == 10 ∪ 17`；实跑 `verify_examples_rtl_sim_coverage.sh` ⇒ `PASS（守恒 27 == 10 ∪ 17…）`；该脚本零改动 |

> **其余 11 条**（AC1–AC3 · AC5–AC8 · AC11–AC14）第 1 轮已 ✅；本轮 diff 对它们的影响面：护栏脚本被改（③' 新增），已在 `ac48bef` 后复跑全套 ⇒ `verify_examples_rtl_sim.sh` PASS（12 项）+ `--self-test` PASS（19 投毒 + 2 必绿）⇒ AC7 / AC8 仍成立。

## 二、新增 diff 是否引入新 BLOCKER

| 检查 | 结果 |
|---|---|
| ③' 是否改变**发射行为** | 否：只落盘两文件 + 做断言；发射命令本身未改（`"$emitter_bin" $eargs --out "$work" --emit-hygiene`） |
| 新断言是否会**误红**正常行 | 全量 12 行生产轮 PASS；空 `eargs` 行（`-` 哨兵）无 token 可查 ⇒ 空循环 |
| 新断言是否会**恒绿** | 投毒19 证明会红；且断言位置在 ③ 成功之后 ⇒ 与投毒17 正交 |
| `$work` 残留旧文件会不会伪造结论 | 落盘前 `rm -f` 两文件 |

## 三、遗留清扫

| 项 | 结果 |
|---|---|
| `tasks.md` 账实 | **30/30 ✅**，每行带证据（`3f9fc1f`） |
| 投毒/Dummy 残留 | `build/examples/axi4/emit_rtl_axi4_lite` 的 dummy 已在第 1 轮清理；本轮无新增 dummy |
| TODO / 调试日志 | 本轮新增代码零命中 |
| 被 skip 的测试 | 无新增 |

## 四、归档前提（不属本 change 的 AC）

- **D6-1**：本 change **挂起**（同上游）。阻塞事实：**G-3** 要求「前置 change 先归档」⇒ 上游未归档；delta 引用的既有 requirement 只在未归档的上游 delta 里。
- ⇒ ⇒ AC 已全闭，「可归档」待上游。

## 五、复现命令

```bash
# AC4（单行清单，快）
T=$(mktemp -d); python3 - "$T/list.tsv" <<'PY'
import sys
lines=open('.rddf/checks/examples-rtl-sim-targets.tsv').read().split('\n')
out=[];cols=None
for l in lines:
    if l.startswith('#COLUMNS:'): cols=l[10:].split(); out.append(l); continue
    if l.startswith('# REGISTERED_TARGETS'): out.append('# REGISTERED_TARGETS: 1'); continue
    if l.startswith('# REGISTERED_SOC'): out.append('# REGISTERED_SOC: 0'); continue
    if l.startswith('# REGISTERED_HIER'): out.append('# REGISTERED_HIER: 0'); continue
    if l.startswith('#') or not l.strip(): continue
    f=l.split('\t')
    if f[cols.index('target')]=='tb_axi4_lite_example': out.append(l)
open(sys.argv[1],'w').write('\n'.join(out)+'\n')
PY
RTL_SIM_LIST="$T/list.tsv" RTL_SIM_WORK="$T/work" bash scripts/verify_examples_rtl_sim.sh
grep -qx -- '--emit-rtl' "$T/work/tb_axi4_lite_example/argv.txt" && echo "含 --emit-rtl ✓"
grep -qx -- '-' "$T/work/tb_axi4_lite_example/argv.norm" || echo "无孤立 - ✓"

bash scripts/verify_examples_rtl_sim.sh --self-test   # 投毒19 转红
./run_all_ported_tests.sh; echo "exit=$?"            # AC9 ⇒ 0 / PASS 27
bash scripts/verify_examples_rtl_sim_coverage.sh     # AC10 ⇒ 27 == 10 ∪ 17
```
