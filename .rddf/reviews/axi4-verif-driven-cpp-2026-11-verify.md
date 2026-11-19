# 验收报告 —— `axi4-verif-driven-cpp-2026-11`

- **验收者**：verifier（`source: user` · session `ses_e123fdf50ffekNVuofg777ESL3`）
- **日期**：2026-11-18
- **HEAD**：`ddb0bbb`（工作区干净）
- **已用轮次**：**1 / 3**（本 change 首次独立验收）
- **判据来源**：① `openspec/changes/axi4-verif-driven-cpp-2026-11/proposal.md` 的 `## 验收标准`（16 条 → 实为 17 条 AC1–AC17）；② 用户本次「逐条核对」指令；③ 交付声明（commit `71d3b46`）。
- **结论**：**ac-fail** —— 15 条通过 · **2 条不通过**（AC10 · AC16）· 另有 2 条结构性记账缺口（见 §四）

---

## 一、逐条判定 + 证据

| AC | 判定 | 取证（命令 ⇒ 输出摘要） |
|---|---|---|
| **AC1** | ✅ | 投毒：临时树的 `.verif` 把第一条 `EXPECT awready 1` 改成 `0`，用 `-DCPPHDL_SOURCE_DIR=<临时树>` 单独编译 tb 后运行 ⇒ `exit=1`；stderr `CHECK bringup_and_low_active_reset#0 FAIL (awready got 1 want 0)` |
| **AC2** | ✅ | `grep -c '"/examples/axi4/tb_axi4_lite_example\.verif"' examples/axi4/tb_axi4_lite_example.cpp` ⇒ **1**；tsv `tb` 列 = `examples/axi4/tb_axi4_lite_example.sv` ⇒ 换成 `.verif` 后该路径存在 |
| **AC3** | ✅ | `bash scripts/verify_examples_rtl_sim.sh` ⇒ PASS；**⑥**（`.md` `VERIF-CHECKS` ↔ 运行期 `checks` ↔ `min_checks` 三方等式）绿 |
| **AC4** | ✅ | `grep -c 'Axi4TimingChecker\|VcdRecorder\|toVerilog\|axi4_lite_waveform' <该文件>` ⇒ **0** |
| **AC5** | ✅ | `grep -c 'set_input_value\|get_port_value\|\.io()\.' <该文件>` ⇒ **0** |
| **AC6** | ✅ | `mktemp -d` 空目录内运行 ⇒ exit 0，运行后 `ls -A` **条目数 = 0**（零落盘） |
| **AC7** | ✅ | 护栏 **⑥'**「生成物新鲜度：逐字节重现 ✓」 |
| **AC8** | ✅ | ① `verify_examples_rtl_sim_coverage.sh` ⇒ PASS（守恒 **27 == 10 ∪ 17**）② `ctest -N` 含 `Test #260: spinalhdl_axi4_lite` ③ `install(TARGETS tb_axi4_lite_example …)`（`examples/axi4/CMakeLists.txt:27`）④ tsv `source` 列 12 行全部存在于磁盘（**0** 缺失） |
| **AC9** | ✅ | 护栏 **C3**（`tests/**` 不得反向 `#include examples/**`）绿（`verify_no_duplicate_rv32i_headers.sh` PASS）。AC 自身已声明 C3 对 `spinalhdl_*` 空跑、判别力由 AC17 承担 ⇒ AC17 已验 |
| **AC10** | ❌ | `./run_all_ported_tests.sh` ⇒ **exit=1**；`PASS: 27 FAIL: 1`，其中 27/27 执行全 PASS，FAIL 来自 **C1**：`UNTRACKED  emit_rtl_rv32i_soc_fw_init`（既不在 `CH_TRACKED_EXAMPLE_TARGETS` 也不在 `disabled_examples.txt`）。且 PASS 计数 **27 ≠ AC 的 28**（差 1 = change 3 的 `axi4_full_example` 退役，见 AC27）。**该 UNTRACKED 为既有缺口**：`git show 55f3f5a:examples/riscv-mini/CMakeLists.txt` 已有该 target（4 处），引入 commit = `a2f5075` ⇒ 非本 change 引入 |
| **AC11** | ✅ | 删掉临时树的 `.verif` 后运行 ⇒ `exit=1`；stderr `FATAL: 打不开验证源：<path>`；stdout **无** `RESULT:` 且 `grep -cE 'checks=\|fails='` ⇒ **0** |
| **AC12** | ✅ | 成功路径 stdout 末行 `RESULT: checks=113 fails=0`（十进制 · 单行 · `grep -cP '\x1b'` ⇒ 0）；失败路径 `RESULT: checks=113 fails=1` + `exit=1`（自洽：`checks` 恒 == 静态展开数 113，`fails>0` ⇒ 非 0 退出） |
| **AC13** | ✅ | 四个 CWD（仓根 / `build/` / `build/tests/` / `mktemp -d` 空目录）各跑一次 ⇒ 全部 `exit=0` 且 `RESULT: checks=113 fails=0` **逐字相同** |
| **AC14** | ✅ | 实测 **190 ms**（≪ 30 s）；`tests/CMakeLists.txt:1432` 的 `add_spinalhdl_ported_test` 固定 `TIMEOUT 30` |
| **AC15** | ✅ | `cmake -B build-nojit -DCH_JIT_ENABLE=OFF -DBUILD_VERILATOR=OFF` + `cmake --build build-nojit --target tb_axi4_lite_example` ⇒ 构建成功；空目录运行 ⇒ `exit=0`、`RESULT: checks=113 fails=0`、stderr **0 行**（记录：本条**不**声称 CI 保证） |
| **AC16** | ❌ | 旧名归零不成立：`examples/axi4/axi4_lite_example_top.h:3` 仍写 `（从 examples/axi4/axi4_lite_example.cpp 抽出）`。作用域含 `examples/`，**不在** AC 白名单（self-test fixture 行 / `.rddf/checks/**` / `openspec/**` / `docs/adr` / `docs/architecture` / `docs/reports` / `docs/CHANGELOG.md` / `docs/archive`）。该行由 `c1af967` 引入（当时 `.cpp` 确叫原名），E1 改名后成**悬空引用**，E6 的同步面清单未含此头 |
| **AC17** | ✅ | ① 护栏 **⑨**（C++ 侧条数 == tsv `min_checks`）存在且绿：`✅ ⑨ 解释器条数相等（checks=113 fails=0）` ② `--self-test` 投毒15（解释器条数被篡改 ⇒ `⑨ 解释器条数不等` 转红）+ 投毒16（`source` 列指向不存在路径 ⇒ `F1b source 列路径不存在` 转红）③ `--self-test` **整轮 PASS**（17 投毒各自转红 + 1 必绿仍绿 + 还原复绿），未用 `|| true` 吞失败 ④ ⑨ 作用域由 note 约定 `interp-entry` 表达（`scripts/verify_examples_rtl_sim.sh:774`） |

---

## 二、失败项与路由建议

| AC | 失败形态 | 分类 | 路由 |
|---|---|---|---|
| **AC16** | 一个悬空旧路径注释（`examples/axi4/axi4_lite_example_top.h:3`） | `implementation_gap` | **回 builder**：改一行注释（`examples/axi4/axi4_lite_example.cpp` → `tb_axi4_lite_example.cpp`），零风险 |
| **AC10** | ① `run_all_ported_tests.sh` exit=1（既有 `UNTRACKED emit_rtl_rv32i_soc_fw_init`）② PASS 计数 27 ≠ AC 字面的 28 | ① 既有缺口（非本 change 引入）② **AC 数字过时**（28 − 1 = change 3 AC27 批准退役） | ① **独立立项**（用户既已认定建议立项）：把该 target 补进 `CH_TRACKED_EXAMPLE_TARGETS` 或 `disabled_examples.txt` ② **planner** 把 AC10 的「仍 28」改为「27（= 28 − change 3 AC27 的显式退役）」 |

> ⚠️ **AC10 / change2 AC9 的同一冲突**：两条 AC 都写「PASS 计数仍 28」，与 change 3 的 AC27（`CH_TRACKED_EXAMPLE_TARGETS == 27`）**直接互斥**。当前树按 change 3 的裁决（27）落地 ⇒ 需 planner 一次性订正 change 1 / change 2 的这两条措辞，否则**每个后续 verifier 都会重复命中**。

---

## 三、遗留清扫（本次范围内）

| 项 | 结果 |
|---|---|
| 临时文件 | 全部落在 `/tmp/opencode/vfy/**`（投毒二进制、临时 .verif、四 CWD 目录、`build-nojit/`），**未触碰仓内 `.verif`**（投毒用临时副本，`verif.bak` 还原后复测 exit=0） |
| 被 skip 的测试 | `test_product_check` 无 SKIP；`test_axi_lite_verilator_e2e` 的 `SKIP("verilator not on PATH")` 为既有**环境门**，本机 verilator 可用（实测 Passed），非静默跳过 |
| 注释掉的代码 / 调试日志 | 仓内 `git status` 干净；`build-nojit/` 是本次验收新增的**构建目录**（未跟踪，重启后可删） |
| 未替换的 mock | 无 |
| 临时 fallback | 无 |

---

## 四、结构性记账缺口（不构成 AC 失败，但影响「变更可闭环」）

| 缺口 | 证据 | 影响 |
|---|---|---|
| **tasks.md 全未回填** | `openspec/changes/axi4-verif-driven-cpp-2026-11/tasks.md`：任务行 **29** · ✅ **0** · ☐ **29** | 本 change 的交付物实际存在（AC 15/17 通过），但任务板显示「一件未做」⇒ 归档前的账实不符 |
| **缺 `.rddf/state/iteration.json`** | `ls .rddf/state/` 无该文件（只有 `builder/*.json` 等） | 本地验收的「`tasks_done == tasks_total > 0`」门**不可评估**；本报告改以 tasks.md 状态为准 |
| 未归档 | `openspec/changes/archive/` 无同名副本 | 正常（三份 change 均待验收后归档） |

---

## 五、复现命令清单

```bash
# AC1 / AC11 投毒（不改仓内 .verif）——临时树 + 独立编译
mkdir -p /tmp/vfy/poison/examples/axi4 && cp examples/axi4/tb_axi4_lite_example.verif /tmp/vfy/poison/examples/axi4/
g++ -std=c++23 -O2 -DNDEBUG -I include -I include/core -I include/ast -I include/abstract \
    -I include/utils -I external/inipp -I examples/axi4 \
    -DCPPHDL_SOURCE_DIR='"/tmp/vfy/poison"' \
    examples/axi4/tb_axi4_lite_example.cpp -o /tmp/vfy/tb build/libcpphdl.a -lLLVM-18
sed -i 's/^EXPECT awready 1$/EXPECT awready 0/' /tmp/vfy/poison/examples/axi4/tb_axi4_lite_example.verif
/tmp/vfy/tb; echo "exit=$?"            # ⇒ 1，stderr 含 bringup_and_low_active_reset#0

# AC6 / AC13（四 CWD）
for d in . build build/tests "$(mktemp -d)"; do (cd "$d" && "$OLDPWD/build/examples/axi4/tb_axi4_lite_example" | tail -1); done

# AC12
./build/examples/axi4/tb_axi4_lite_example | tail -1 | grep -cP '\x1b'   # ⇒ 0

# AC15
cmake -B build-nojit -DCH_JIT_ENABLE=OFF -DBUILD_VERILATOR=OFF && \
  cmake --build build-nojit --target tb_axi4_lite_example -j"$(nproc)" && \
  (cd "$(mktemp -d)" && «本机»/build-nojit/examples/axi4/tb_axi4_lite_example | tail -1)

# AC16
grep -rnE 'axi4_lite_example\.cpp|(^|[^A-Za-z0-9_])axi4_lite_example([^A-Za-z0-9_]|$)' \
  examples scripts tests docs/*.md AGENTS.md | grep -v tb_axi4_lite_example

# AC17
bash scripts/verify_examples_rtl_sim.sh && bash scripts/verify_examples_rtl_sim.sh --self-test

# AC10
./run_all_ported_tests.sh; echo "exit=$?"
```
