# tasks — `axi4-verif-driven-cpp-2026-11`

> ⚠️ **设计不在本文件**：决策面 = ADR-074 / ADR-075。
> ⚠️ **提交粒度**：批次 A–D（驱动改造）与 E（改名）**分成两个 commit** ⇒ 回滚粒度与价值粒度对齐。
> ⚠️ **变异纪律**：任何「改源 ⇒ 验红 ⇒ 还原」的动作 MUST 走「**还原后全量重建**」
> （还原源码 ≠ 复原 —— 二进制也是状态）。

## 批次 A — 构建接线与签名（前置）

| # | 任务 | 落点 | 判据 | 状态 |
|---|---|---|---|---|
| A1 | 新增 `target_compile_definitions(axi4_lite_example PRIVATE CPPHDL_SOURCE_DIR="${PROJECT_SOURCE_DIR}")`（与 `tests/CMakeLists.txt:1138` 同形） | `examples/axi4/CMakeLists.txt` | AC13 | ☐ |
| A2 | `main` 签名改 `int main(int argc, char** argv)`（argv 暂不用 ⇒ 后继 change 不再改签名） | `examples/axi4/axi4_lite_example.cpp` | — | ☐ |

## 批次 B — 驱动改造（AC2 · AC4 · AC5 · AC6 · AC12）

| # | 任务 | 落点 | 判据 | 状态 |
|---|---|---|---|---|
| B1 | `main()` 改为 `parse_verif()` → `build_rtl_manifest()` → `run_verif()`；路径字面量**只有** `/examples/axi4/tb_axi4_lite_example.verif` 与 `CPPHDL_SOURCE_DIR` 拼接（**恰好 1 次**） | 同上 | AC2 | ☐ |
| B2 | **fail-closed**：打不开 / 解析失败 / `manifest.diagnostics` 非空 ⇒ `return 1` + stderr，且**不打印** `RESULT:`（也不得以其它形态写出 `checks=/fails=`） | 同上 | AC11 | ☐ |
| B3 | **自洽退出码**：`res.ok && res.checks == verif_check_count(prog) && res.fails == 0` 才 `return 0` | 同上 | AC1 · AC12 | ☐ |
| B4 | 打印 `RESULT: checks=<N> fails=<M>`（**十进制** · 单行 · 无 ANSI；**禁** `std::hex` 残留） | 同上 | AC12 | ☐ |
| B5 | 删 `Axi4TimingChecker` / `VcdRecorder` / 手写激励 / 6 行 `set_input_value` / `toVerilog(...)` ⇒ **零残留** | 同上 | AC4 · AC5 · AC6 | ☐ |
| B6 | **失败明细进 stderr**：把 `res.failure_lines` 逐条打印（`CHECK <id> FAIL …`）⇒ spec 的 scenario 要求「结论行 MUST 报出失败的 `check_id`」 | 同上 | AC1 | ☐ |

## 批次 C — 判别力与 CWD（AC1 · AC11 · AC13）

| # | 任务 | 判据 | 状态 |
|---|---|---|---|
| C0 | **失败路径可执行判据**：把 `.verif` **复制到 `mktemp -d`** 的临时副本并把一条 `EXPECT` 改成必失败值 ⇒ 以该副本为路径**重建** ⇒ exit ≠ 0 且 stderr 含 `check_id`、stdout 无 `RESULT:` ⇒ **丢弃副本**（不提交、不改仓内 `.verif` ⇒ 与 AC7 不冲突） | AC1 | ☐ |
| C1 | **fail-closed 投毒**：把路径改成不存在的文件 ⇒ 改源 ⇒ **重建** ⇒ 验红 ⇒ 还原 ⇒ **全量重建** ⇒ 复绿 | AC11 | ☐ |
| C2 | 四 CWD 实测：仓根 / `build/` / `build/tests/` / `mktemp -d` 新建空目录 ⇒ 都 exit 0 且 `RESULT:` 行相同 | AC13 | ☐ |
| C3 | 空目录运行后该目录仍为空（零落盘） | AC6 | ☐ |

## 批次 D — 条数等式与时长（AC3 · AC14）

| # | 任务 | 判据 | 状态 |
|---|---|---|---|
| D1 | 既有护栏 **⑥**（`.md` `VERIF-CHECKS` ↔ 运行期 `checks` ↔ `min_checks`）仍绿 ⇒ ⇒ **⑥''** 是键集合判据，**不**承担数值 | AC3 | ☐ |
| D2 | 实测运行时长 < 30s | AC14 | ☐ |

## 批次 E — 改名与引用面（AC7 · AC8）

| # | 任务 | 落点 | 判据 | 状态 |
|---|---|---|---|---|
| E1 | `git mv` `axi4_lite_example.cpp` → `tb_axi4_lite_example.cpp` | `examples/axi4/` | AC8 | ☐ |
| E2 | `add_executable` / `target_link_libraries` / `target_include_directories` / `set_target_properties` / **`install(TARGETS …)`** 逐条改名 | `examples/axi4/CMakeLists.txt` | AC8 | ☐ |
| E3 | **`add_spinalhdl_ported_test` 的第二参数**改名（CTest 名 `spinalhdl_axi4_lite` **不变**） | `tests/CMakeLists.txt:1455` | AC8 | ☐ |
| E4 | `CH_TRACKED_EXAMPLE_TARGETS` 改名 | `examples/CMakeLists.txt` | AC8 | ☐ |
| E5 | tsv 第 53 行 `target` / `source` 两列同步（`emitter` 列**不动** ⇒ 属后继 change） | tsv | AC8 | ☐ |
| E6 | **引用面全同步**：`include/axi4/AGENTS.md` · `examples/axi4/AGENTS.md` · **根 `AGENTS.md:83`** · `run_all_ported_tests.sh:100-105`（点名 `.vcd`）· `examples/axi4/emit_rtl.cpp:6-7` · `tests/test_axi_lite_verilator_e2e.cpp:8,21,73-75,216` · `docs/CHANGELOG.md` | 同上 | AC16 | ☐ |
| E7 | 生成式 TB **逐字节不变**（⑥' 新鲜度绿） | 护栏 | AC7 | ☐ |

## 批次 F — 护栏升级与验收（AC9 · AC17）

| # | 任务 | 落点 | 判据 | 状态 |
|---|---|---|---|---|
| F1 | 新增 **⑨（C++ 侧条数相等）**：遍历 tsv 里「**登记了解释器入口**」的行 ⇒ 按 `target` 列在 `$BUILD_DIR/examples/**` 找可执行文件（**找不到 ⇒ fail-closed 违规**）⇒ 运行 ⇒ 断言 stdout 含 `RESULT: checks=<min_checks> fails=0` | `scripts/verify_examples_rtl_sim.sh` | AC17 | ☐ |
| F1c | **在清单里表达「有解释器入口」**（**用 note 约定，不新增列** —— 新增列会与后继 change 的「12 → 13 列」前提冲突）⇒ ⇒ ⑨ 只对登记了入口的行生效（**不得**假设「凡 `generated=yes` 必有 C++ 侧」） | tsv + 护栏 | AC17 | ☐ |
| F1a | **self-test fixture 同步**：fixture 里 axi4 行的 `target` 改为 `tb_axi4_lite_example`（否则 ⑨ 找不到二进制） | 同上 | AC17 | ☐ |
| F1b | **新增判据 + 投毒**：tsv 的 `source` 列路径 MUST 存在于磁盘（该列目前**读了不判**）⇒ 投毒「改成不存在的路径 ⇒ 必红」 | 同上 | AC8 · AC17 | ☐ |
| F2 | `verify_example_test_wiring.sh` 的 C3 仍绿（**注**：C3 对 `spinalhdl_*` 空跑 ⇒ 本 example 的判别力由 ⑨ 承担；该洞已登记为独立遗留） | 护栏 | AC9 | ☐ |
| F3 | `./run_all_ported_tests.sh` 绿（28 tracked · PASS 计数仍 28） | 护栏 | AC10 | ☐ |
| F4 | 旧名归零（精确模式 + 作用域 + 白名单，见 AC16） | grep | AC16 | ☐ |
| F5 | `--self-test` **整轮绿**（含 F1/F1b 的两条投毒）⇒ **禁止** `\|\| true` | 护栏 | AC17 | ☐ |

## 批次 G — 构建矩阵（AC15）

| # | 任务 | 判据 | 状态 |
|---|---|---|---|
| G1 | **本地** `cmake -B build-nojit -DCH_JIT_ENABLE=OFF` ⇒ 构建该 target ⇒ 运行 ⇒ exit 0（记录命令 + 输出；**不**声称由 CI 保证） | AC15 | ☐ |

## 复核命令

```bash
cmake --build build -j$(nproc)

# 条数等式（⑥）+ 新鲜度（⑥'）+ 覆盖守恒
bash scripts/verify_examples_rtl_sim.sh
bash scripts/verify_examples_rtl_sim_coverage.sh

# 判别力（⑨ + source 列 + 投毒）；**不要**加 `|| true`
bash scripts/verify_examples_rtl_sim.sh --self-test

# 运行护栏
./run_all_ported_tests.sh

# CWD 无关（四 CWD；空目录用 mktemp）
for d in . build build/tests; do ( cd "$d" && /workspace/project/CppHDL/build/examples/axi4/tb_axi4_lite_example | tail -1 ); done
t=$(mktemp -d); ( cd "$t" && /workspace/project/CppHDL/build/examples/axi4/tb_axi4_lite_example | tail -1; find . -type f | wc -l ); rmdir "$t"
```

## ⚠️ 未做 / 本次不做

| 项 | 原因 |
|---|---|
| SoC example 的改名与 `.verif` 驱动 | UART 8N1 帧解码超出封闭原语集 ⇒ 独立立项 |
| C++ 侧忠实波形 | Verilator 侧的 `ifdef DUMP_VCD` 路径仍提供真波形 ⇒ 独立立项 |
| `tests/test_axi_lite_verilator_e2e.cpp` 的手写激励面 | 该文件自己有一套 ⇒ 本 change 不主张「期望只剩一份」 |
| **修 `verify_example_test_wiring.sh` 的 C3 空跑** | 影响全部 26 个 `add_spinalhdl_ported_test` 条目 ⇒ 需「CTest 名 → 二进制」映射机制 ⇒ 独立立项（本 change 用 ⑨ 覆盖本 example） |
| 其余 26 个 example 的 C++ 侧改造 | 本 change 只做 1 个 |
| 期望值改动 | AC7 ⇒ 必须独立 change + `.md` 登记 |

---

## 实施实况（2026-11-18 · builder）

| 批次 | 状态 | 取证摘要 |
|---|---|---|
| A（构建接线） | ✅ | `CPPHDL_SOURCE_DIR` 已注入；`main(int, char**)`；构建绿 |
| B（驱动改造） | ✅ | `parse_verif` → `build_rtl_manifest` → `run_verif`；零残留（`grep -c 'Axi4TimingChecker\|VcdRecorder\|toVerilog\|set_input_value'` == **0**）；**88 行**（原 219） |
| C（判别力 / CWD） | ✅ | 四 CWD 均 `RESULT: checks=113 fails=0` · 空目录零落盘 · 验证源缺失 ⇒ exit 1 + **无** RESULT 行 · 改一条 `EXPECT` ⇒ exit 1 + `fails=1` + stderr 具名 `CHECK bringup_and_low_active_reset#0 FAIL` ⇒ `.verif` 逐字还原 |
| D（条数 / 时长） | ✅ | 护栏 ⑥ 三方等式绿 · 实测 **0.10 s**（< 30 s） |
| E（改名） | ✅ | `git mv` + CMake（5 处 + install）+ `tests/CMakeLists.txt` 第二参数（CTest 名 `spinalhdl_axi4_lite` **不变**）+ `CH_TRACKED_EXAMPLE_TARGETS` + tsv `target`/`source` + 三份 AGENTS + e2e 注释 + `run_all_ported_tests.sh` 注释；旧名归零（唯一残留 = AC16 白名单的 `docs/CHANGELOG.md`） |
| F（护栏） | ✅ | 新增 **⑨**（note 列 `interp-entry` 约定 · 找不到二进制 ⇒ fail-closed）+ **F1b**（`source` 列路径须存在）⇒ `--self-test` **16 条投毒各自转红 · 基线绿 · 还原复绿** |
| G（构建矩阵） | ✅ | `-DCH_JIT_ENABLE=OFF` 构建 + 运行 ⇒ `RESULT: checks=113 fails=0` |

### ⚠️ 遗留（**既有缺口 · 非本 change 引入**）

`./run_all_ported_tests.sh` 的 **C1** 报 `UNTRACKED emit_rtl_rv32i_soc_fw_init` ⇒ ⇒
**AC10 的「C1 无 UNTRACKED」不成立**。证据：
· 该 emitter 在 `examples/riscv-mini/CMakeLists.txt` 声明、被 `scripts/verify_rv32i_fw_init_rtl.sh` 消费；
· 但**从未**登记进 `.rddf/checks/examples-rtl-sim-targets.tsv` 的 `emitter` 列
  （`git log -S "emit_rtl_rv32i_soc_fw_init" -- <tsv>` ⇒ **空**）；
· 本 change **未触碰** `examples/riscv-mini/**`（`git status` 无该目录）。

⇒ ⇒ 归属是 **RV32I firmware-load 链条**的登记缺口 ⇒ ⇒ **独立立项**（不在本 change 顺手补：登记一个产品单元需要 TB + 期望源 + `min_checks`）。
⇒ ⇒ 本 change 的 PASS 计数 **28** 与既有基线**一致**（`PASS: 28`），FAIL 仅此 1 项且与本 change 无关。
