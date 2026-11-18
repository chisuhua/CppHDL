# 第二轮复核 —— `axi4-verif-driven-cpp-2026-11`

- **验收者**：verifier（`source: user` · session `ses_e123fdf50ffekNVuofg777ESL3`）
- **日期**：2026-11-18 · **HEAD**：`67c5b24`（工作区干净）
- **已用轮次**：**2 / 3**
- **复核范围（不是全量重审）**：第 1 轮判 ❌ 的 2 条（AC10 · AC16）+ 因 AC 措辞订正而**判定基准变化**的 1 条（AC8 的守恒数字）+ 本轮新增 diff 是否引入新 BLOCKER。
- **结论**：**AC10 ✅ · AC16 ✅ · AC8 ✅ ⇒ 17/17 条全部成立**（本 change 可归档，但归档顺序受 D6-1 约束——见 §四）

## 一、逐条判定 + 证据

| AC | 第 1 轮 | 本轮 | 证据（本轮实跑） |
|---|---|---|---|
| **AC10** | ❌（`run_all_ported_tests.sh` exit=1；PASS 27 ≠ 28） | ✅ | 订正后口径 = 「**27** tracked · PASS 计数 **27** · 前提 = 无 `UNTRACKED`」。实跑：`exit=0` · `OK：全部已构建的 example 程序都有归属（被跟踪 27）` · `跟踪 27 == 实际执行 27` · `PASS: 27 FAIL: 0 STALE: 0 MISSING: 0 STALE-ARTIFACT: 0`。前提由 `f389c3c`（登记 `emit_rtl_rv32i_soc_fw_init`）达成 |
| **AC16** | ❌（`examples/axi4/axi4_lite_example_top.h:3` 仍写旧名） | ✅ | 本批 `549163f` 改注释。复跑：`grep -rnE '(^\|[^A-Za-z0-9_])axi4_lite_example\.cpp' include examples scripts tests docs AGENTS.md`（排除 AC16 白名单 `docs/{adr,architecture,reports,archive}/` 与 `docs/CHANGELOG.md`）⇒ **零命中** |
| **AC8** | ✅（附注：28 == 10 ∪ 18 已过时） | ✅ | 订正后口径 = `27 == 10 ∪ 17`。实跑 `verify_examples_rtl_sim_coverage.sh` ⇒ `PASS（守恒 27 == 10 ∪ 17 · 落选登记完备 · 列纯度）` |

> **其余 14 条**（AC1–AC7 · AC9 · AC11–AC15 · AC17）第 1 轮已判 ✅ 且本轮 diff 未触及它们的判据面 ⇒ 不重审（AC17 的 ⑨ 与两条投毒在 `ac48bef` 后仍绿：`--self-test` 19 投毒 + 2 必绿整轮 PASS）。

## 二、新增 diff 是否引入新 BLOCKER

本 change 的增量 = `docs/CHANGELOG.md`（两段登记）· `examples/axi4/axi4_lite_example_top.h`（一行注释）· `examples/disabled_examples.txt`（一行登记）· 本文件与两份 proposal/tasks · `scripts/verify_examples_rtl_sim.sh`。

| 检查 | 结果 |
|---|---|
| 是否触及实现代码（`src/**` / `include/**`） | **零** ⇒ 无行为回归面 |
| `run_all_ported_tests.sh` 的归属判定是否被「登记」削弱 | 无：C1 只认「tracked ∪ 已登记」，登记行带理由与归位条件；被登记目标的**行为覆盖**另有 `scripts/verify_rv32i_fw_init_rtl.sh`（本轮 PASS） |
| 是否有新的跳过/放行 | 无新增 `SKIP` / `|| true` |

## 三、遗留清扫（本 change 范围内）

| 项 | 结果 |
|---|---|
| `tasks.md` 账实 | **30/30 ✅**，每行带一条证据（`3f9fc1f`） |
| TODO / 半截实现 / 调试日志 | 本轮新增文件零命中（`grep -c 'TODO\|FIXME\|TEMP DEBUG'` = 0） |
| 被 skip 的测试 | 无新增 |
| 未替换的 mock / 临时 fallback | 无 |

## 四、归档前提（**不属本 change 的 AC**，但阻塞归档）

- **D6-1**（用户 2026-11-18 裁决）：本批**只归档 change 3**；本 change 与 change 2 **挂起**至上游 `rtl-verification-single-source-2026-11` 验收 + 归档。
- 阻塞事实：本 change 的 **G-1** 前置 `test -f openspec/specs/examples-rtl-sim-coverage/spec.md` **当前不满足**；delta 正文引用的「既有 requirement」（7 条）只存在于未归档的上游 delta。
- ⇒ ⇒ **本 change 的 AC 已全闭，但「可归档」= 待上游归档后再判。**

## 五、复现命令

```bash
./run_all_ported_tests.sh; echo "exit=$?"                       # AC10 ⇒ 0，PASS 27 / FAIL 0
grep -rnE '(^|[^A-Za-z0-9_])axi4_lite_example\.cpp' include examples scripts tests docs AGENTS.md \
  | grep -vE '^docs/(adr|architecture|reports|archive)/|^docs/CHANGELOG\.md'   # AC16 ⇒ 空
bash scripts/verify_examples_rtl_sim_coverage.sh                # AC8 ⇒ 27 == 10 ∪ 17
bash scripts/verify_examples_rtl_sim.sh --self-test             # AC17 ⇒ 19 投毒 + 2 必绿
```
