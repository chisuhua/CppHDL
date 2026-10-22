# Change: samples-regression-guard-2026-10 — samples 纳入自动化回归防护

## Why

`samples/` 下的 **40 个源文件**目前**零自动化覆盖**：

- `grep -c 'samples/' tests/CMakeLists.txt` → **1**（唯一一处是注释行 ⇒ 0 个 sample 被 ctest 覆盖）
- `grep -c 'samples/' run_all_ported_tests.sh` → **0**

⇒ **sample 坏了没有任何自动化会知道**，只能靠人手工跑。

这不是理论风险 —— 本次审计在 samples 里**实测发现 2 个既有 segfault**
（且都**不在** ctest / CI 里）：

| sample | 现象 | 根因 | 基线也崩？ |
|---|---|---|---|
| `samples/fifo_bundle.cpp` | `empty=0` / exit=1 | bundle 字段被重绑定成零字面量总线的位切片 ⇒ 只读字段恒 0 | ✅（已由 `bundle-master-slave-io-2026-10` 修复） |
| `samples/nested_bundle_demo.cpp` | **segfault** | `bundle_base(const std::string&)` 在**基类构造期**调虚函数 `set_name_prefix()` ⇒ 对**尚未构造**的嵌套 bundle 成员调虚函数 = UB | ✅（将由 `bundle-ecosystem-hardening-2026-10` 修复） |
| `samples/switch_demo.cpp` | **segfault** | `make_uint<4>(0)` 在**无 `ctx_swap`** 的情况下创建节点 ⇒ 节点创建失败 ⇒ nullptr 解引用 | ✅（本 change 修复） |

⇒ **`samples/fifo_bundle` 的失败从落地起存在了数月才被审计发现** —— 这就是
「零覆盖 ⇒ 静默腐坏」的直接代价。

> ⚠️ **本提案经 2026-10-22 的双路评审修订**：修正了 3 处**事实错误**
> （CI 现状 / 覆盖面 / `switch_demo` 根因位置），并补上了「护栏自身也会静默
> 缩面」这一**与它要治的病同型**的缺口。

### ⚠️ 评审修正的 3 处事实

1. **「`examples/` 是有覆盖的」只在本地成立** —— `.github/workflows/` 下**只有**
   `ci.yml`，`grep run_all_ported` **0 命中** ⇒ examples 在 CI 里只有**编译**覆盖
   （`ci.yml` 的 Build 步），`run_all_ported_tests.sh` 覆盖 28 个 main()-bearing
   example 是**纯本地**行为 ⇒ 本 change 的 CI job **没有「与之并列」的既有 job 可挂**。
2. **覆盖面是 24/40，不是 40/40** —— `samples/CMakeLists.txt` 只注册了 **24** 个
   target；**16** 个 `.cpp` **从未被编译**（见下表）⇒ 「遍历 `build/samples/`」的护栏
   **看不见**它们 ⇒ 「全部 sample MUST 有自动化护栏」在**源文件层面不可验证**。
3. **`switch_demo` 的根因位置** —— 首个失败点是 `samples/switch_demo.cpp:10` 的
   `make_uint<4>(0)`（不是 proposal 原写的 `make_uint<8>(0)`），且该文件共有
   **9 个** node 创建点、**同一个无 context 的 main 作用域** ⇒ 补 `ctx_swap` 是
   全局修复（不是单点修复）。

### 16 个「从未被编译」的 sample（`samples/*.cpp` 有 40，`CH_SAMPLE_TARGETS` 只有 24）

| sample | 状态 |
|---|---|
| `advanced_pod_bundle_serialization` | 未注册 |
| `conditional_demo` | 未注册 |
| `dag_compare` | 未注册 |
| `if_demo` | 未注册（在 CMakeLists 里被注释） |
| `large_pod_bundle_example` | 未注册 |
| `spinalhdl_component_function_area_example` | 未注册 |
| `spinalhdl_stream_example` | 未注册 |
| `stream_arbiter_example` | 未注册（在 CMakeLists 里被注释） |
| `stream_component_example` | 未注册 |
| `stream_demux_example` | 未注册（在 CMakeLists 里被注释） |
| `stream_fifo_example` | 未注册（在 CMakeLists 里被注释） |
| `stream_flow_basic_example` | 未注册（在 CMakeLists 里被注释） |
| `stream_fork_example` | 未注册（在 CMakeLists 里被注释） |
| `stream_handshake_example` | 未注册（在 CMakeLists 里被注释） |
| `stream_join_example` | 未注册（在 CMakeLists 里被注释） |
| `tlm_bundle_demo` | 未注册（在 CMakeLists 里被注释）—— ⚠️ 它是 `deserialize<>` 路径的 **sample 级消费方**（`samples/tlm_bundle_demo.cpp:121`）⇒ 该路径的 sample 级覆盖**当前无效** |

⇒ `samples/CMakeLists.txt` 里现有的**注释掉的条目没有任何禁用理由记录** ⇒
「为什么它没被编译」这件事本身已经失传。

## What Changes

### D1: 新增 `scripts/run_all_samples.sh` —— samples 的回归护栏

- 遍历 `build/samples/` 下对应的**已构建二进制**，**逐个执行**并收集退出码
- **任一 sample 非零退出 ⇒ 脚本失败**（含 segfault / 超时）
- 输出汇总（PASS / FAIL 名单），便于 CI 定位
- ⚠️ MUST 设**超时**（`timeout 60`）—— 已有 sample 可能挂死（历史上
  `tests/test_axi_spi.cpp` 就有 5 处无界 `while (ready == 0) tick();`）
- ⚠️ MUST **区分**「本 change 引入的失败」与「既有失败」⇒ 首次落地时
  MUST 先跑一遍全量并把**既有失败**显式列出，修复后再要求全绿

### D2: 护栏 MUST 对「覆盖集合」本身负责 —— 不许静默缩面（评审新增）

⚠️ **这是本 change 最容易重蹈覆辙的一处**：若护栏只盯「跑起来的 sample 是否失败」，
那么**一个 sample 编译失败 ⇒ 二进制不存在 ⇒ 从 `build/samples/` 消失 ⇒ 护栏静默
缩面** —— 这正是 `fifo_bundle` 模式的**同型复刻**（失败被"消失"而不是被报告）。

⇒ 护栏 MUST 有**集合守恒**断言：

1. **注册一致性**：每个 `samples/*.cpp` MUST 要么出现在 `samples/CMakeLists.txt` 的
   `CH_SAMPLE_TARGETS` 里，要么出现在**带理由**的禁用清单
   （**`samples/disabled_samples.txt`**，每行 `sample 名 <TAB> 理由 <TAB> 归位条件`）
   ⇒ 现状下 16 个未注册 sample MUST 被显式登记（带理由），
   **MUST NOT** 处于「既没注册、也没记录」的中间态。
   ⚠️ **处置只有两种**（本 change **不承担**「让它们恢复可编译」的修复工作 —— 见
   Non-Goal 5）：① **未经修改即可编译** ⇒ 直接补进 `CH_SAMPLE_TARGETS`；
   ② 否则 ⇒ 带理由登记进禁用清单（注明「何时回来」的归位条件）。
2. **数量守恒**：脚本实际执行的 sample 数 MUST 等于「**已注册数**」；
   不等 ⇒ **失败**（不是 SKIP）。
   ⚠️ **豁免项也计入「已注册数」** —— 被豁免的 sample **仍然被执行**
   （作为 stale 探针）⇒ 豁免**只影响失败计数**，**不影响**覆盖集合。
3. **缺失二进制 = 失败**：`run_all_ported_tests.sh` 的 `SKIPPED (binary not found)`
   语义对**回归护栏**太弱 ⇒ 本脚本 MUST 判**失败**。

### D3: 修 `samples/switch_demo.cpp` 的既有 segfault

根因：`make_uint<4>(0)`（`samples/switch_demo.cpp:10`）在**没有 `ctx_swap`** 的情况下
创建节点（`node_builder` 报「No active context for literal creation」并返回
nullptr）⇒ 节点为 nullptr ⇒ 后续解引用崩溃。

修法：按 `samples/AGENTS.md` 的约定补 `ch::core::context` + `ch::core::ctx_swap`
（与 `tests/test_fifo_example.cpp` 等一致）。⚠️ 该文件共 **9 行 / 17 处**
`make_uint` 调用，全部在同一无 context 的 main 作用域 ⇒ 补 `ctx_swap` 是
**全局**修复，MUST NOT 只补第一个点。

### D4: 挂进 CI

`.github/workflows/ci.yml` **新增一个 job** 跑 `scripts/run_all_samples.sh`。

⚠️ **平台约束**：CI 矩阵含 `macos-14`（`ci.yml:87`）⇒ GNU `timeout` 在 macOS 上
**默认不存在**（需 `gtimeout`）⇒ 新 job MUST 挂在 **Linux** runner
（`ubuntu-22.04`）上；脚本本身 MUST 做 `command -v timeout || gtimeout` 回退，
以便本地 macOS 也能跑。

⚠️ **job 级预算**：最坏 24 × 60s ≈ **24 min** ⇒ MUST 设 `timeout-minutes`
（参考 `pr-verilator-fast` 的 45）。

### D5: 豁免清单 MUST 是数据驱动的、可失效的、且**不静默**（评审新增）

既有失败的 sample MUST 被**显式豁免**，但豁免 MUST NOT 变成「永久免检」：

- 豁免 MUST 是**显式列表**（sample 名 + 根因 + 关联 change/缺陷单 + **失效条件**），
  对齐 `scripts/verify_rst_n_wiring.sh` 的既有约定（豁免**必须**在 CI 日志里
  `::notice` 点名 ⇒ **豁免不是静默的**）
- ⚠️ **豁免项仍然被执行**（作为 stale 探针）⇒ 覆盖集合的守恒公式**不因豁免而减少**
  ⇒ 豁免**只影响失败计数**（被豁免项的失败不计入失败数）
- ⚠️ **stale 检测**：若某豁免项**已经转绿** ⇒ 脚本 MUST 报 **stale** 并**失败**
  （防止豁免永久残留）
- ⚠️ **失效条件 MUST 可判定**：本 change 落地时 `nested_bundle_demo` 的豁免，
  其失效条件 = `bundle-ecosystem-hardening-2026-10` archive ⇒ 该 change 的
  Task 7 MUST 负责**摘除**这条豁免并全量重跑

### D6: 文档同步

- 根 `AGENTS.md`：
  - COMMANDS 段加 `bash scripts/run_all_samples.sh`
  - 「已消除的过时信息」新增本 change 条目
  - NOTES 的测试计数段说明 samples 的覆盖方式（**不计入 CTEST_TOTAL**
    —— 它们不是 ctest target）
- `samples/AGENTS.md`：加「每个 sample MUST 以 0 退出；由
  `scripts/run_all_samples.sh` 守护」+「新增 sample MUST 同时注册进
  `CH_SAMPLE_TARGETS`」

### D7: 新 capability 的 `## Purpose` MUST 在 archive 后手工写入

⚠️ **实测结论**（OpenSpec CLI 1.4.1，沙箱归档验证）：archive 流程对**新建**的
capability **一律生成**骨架：

```
## Purpose
TBD - created by archiving change <change-name>. Update Purpose after archive.
```

⇒ delta 里的 `## Purpose` **不会被合并**（与既有能力不同：既有能力的 Purpose 在
archive 时被**保留**）⇒ 该骨架行里带着 **change 名** ⇒ 主 spec 会残留 change 名。

⇒ **裁决**：本 change 的 delta MUST 写好 `## Purpose`（作为**手工写入的源文本**），
且 archive **之后** MUST 用它**替换**骨架行（仓库先例：`axi4-full-slave-burst` 的
Purpose 即「archive 后已补写」）。

> ℹ️ 这一步 MUST 落在**归档后**的收尾任务里，而不是 archive 之前 —— archive 是
> CLI 行为，MUST NOT 靠「预先安排」绕过。

## Impact

| 项 | 影响 |
|---|---|
| `scripts/run_all_samples.sh` | **新增**（samples 的回归护栏） |
| `samples/disabled_samples.txt` | **新增**（带理由的禁用清单：未注册 sample 的登记处） |
| `samples/CMakeLists.txt` | 16 个未注册 sample 逐个处置（补注册 / 登记进禁用清单）（D2） |
| `samples/switch_demo.cpp` | 补 context（修既有 segfault） |
| `.github/workflows/ci.yml` | 新增 1 个 **Linux-only** job |
| `docs/reports/verification-samples-regression-guard-2026-10.md` | **新增**（验证记录：基线清单 + 变异结果） |
| 根 `AGENTS.md` / `samples/AGENTS.md` | 文档同步 |
| `openspec/specs/sample-regression-guard/spec.md`（归档后新建） | 新 capability；⚠️ **实测（CLI 1.4.1 沙箱归档）**：archive 对**新** capability **一律生成** `## Purpose` = `TBD - created by archiving change <name>. Update Purpose after archive.` ⇒ delta 里的 `## Purpose` **不会**被合并 ⇒ MUST 在 archive **之后**手工写入真实 Purpose（仓库先例：`axi4-full-slave-burst` 的 Purpose 即「archive 后已补写」）⇒ 手工写入前，主 spec 会残留 change 名 |
| 计数 | ⚠️ **CTEST_TOTAL 不变**（samples 不是 ctest target；它们走 shell 脚本）⇒ `scripts/verify_test_counts.sh` 无需改 |
| 风险 | 🟢 **低**。① 纯新增护栏 + 修一个 sample 的 context 缺失；② 但**首次落地会暴露既有失败** ⇒ MUST 先跑全量、逐个定性（既有失败要么修、要么显式豁免，**MUST NOT** 静默跳过） |

## Non-Goals

1. **不**把 sample 改写成 ctest target（sample 的形态是「带 stdout 输出的演示」，
   与 Catch2 用例不同；用 shell 脚本守护更贴合其定位）
2. **不**修 `samples/nested_bundle_demo.cpp` 的 segfault —— 根因在 bundle 框架，
   归 `bundle-ecosystem-hardening-2026-10`（本 change 显式豁免它，失效条件 =
   那个 change archive）
3. **不**给 sample 加行为断言（sample 的职责是「演示 + 自检」；
   行为断言属于 `tests/`）
4. **不**改 `examples/` 的覆盖方式（已有 `run_all_ported_tests.sh`）
   —— ⚠️ 但「examples 的**运行**覆盖也不在 CI 里」这一缺口 MUST 被**记录**
   （独立 follow-up，本 change 不修）
5. **不**修 16 个未注册 sample 的**编译**（未知 bit-rot，风险独立）
   —— 本 change 只做**登记**：未经修改即可编译的 ⇒ 补注册；否则 ⇒ 带理由登记进
   `samples/disabled_samples.txt` ⇒ 它们的腐坏从「静默」变成「可见」

## 落地顺序（与 `bundle-ecosystem-hardening-2026-10` 的耦合）

⚠️ 两个 change **必须串行**（都会改根 `AGENTS.md`）：

1. **本 change 先**落地（护栏带 `nested_bundle_demo` 的**显式豁免**，
   失效条件 = 下一个 change archive）
2. **`bundle-ecosystem-hardening-2026-10` 后**落地 ⇒ archive 时 MUST 移除该豁免
   并全量重跑护栏

## 附录 A：审计的复现命令

```bash
# 全量跑已构建的 sample，逐个记录退出码
for s in build/samples/*; do [ -x "$s" ] && [ -f "$s" ] || continue; \
    timeout 60 "$s" >/dev/null 2>&1 || echo "FAIL: $s (exit=$?)"; done
# ⇒ 实测：nested_bundle_demo (139=SEGV) / switch_demo (139=SEGV)
#   ⚠️ 注意：exit 139 = SIGSEGV，124 = timeout（两者 MUST 在汇总里区分）

# 覆盖面（⚠️ 24 ≠ 40）
ls samples/*.cpp | wc -l                                     # 40
sed -n '/set(CH_SAMPLE_TARGETS/,/^)/p' samples/CMakeLists.txt | sed -n 's/^    \([A-Za-z0-9_]*\)$/\1/p' | wc -l   # 24
comm -23 <(ls samples/*.cpp | sed 's|samples/||;s|\.cpp||' | sort) \
         <(sed -n '/set(CH_SAMPLE_TARGETS/,/^)/p' samples/CMakeLists.txt | sed -n 's/^    \([A-Za-z0-9_]*\)$/\1/p' | sort)
# ⇒ 16 个未注册的 sample（见 Why 表）

# 覆盖面
ls samples/*.cpp | wc -l                          # 40
grep -c 'samples/' tests/CMakeLists.txt           # 1（唯一一处是注释行）
grep -c 'samples/' run_all_ported_tests.sh        # 0
grep -c 'examples/' run_all_ported_tests.sh       # 28（examples 的**本地**运行覆盖）

# ⚠️ CI 现状：examples 的运行覆盖**不在** CI 里
ls .github/workflows/                              # 只有 ci.yml
grep -rn "run_all_ported" .github/workflows/       # ⇒ 0 命中
grep -n "macos-14" .github/workflows/ci.yml        # ⇒ 矩阵含 macOS（timeout 不可用）

# T2
./build/samples/switch_demo                     # segfault（基线同样）
#   日志：「No active context for literal creation」⇒ 缺 ctx_swap
```
