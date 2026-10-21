# Change: samples-regression-guard-2026-10 — samples 纳入自动化回归防护

## Why

`samples/` 下的 **40 个程序**目前**零自动化覆盖**：

- `grep -c 'samples/' tests/CMakeLists.txt` → **0**（唯一一处是注释）
- `grep -c 'samples/' run_all_ported_tests.sh` → **0**

⇒ **sample 坏了没有任何自动化会知道**，只能靠人手工跑。

这不是理论风险 —— 本次审计在 40 个 sample 里**实测发现 2 个既有 segfault**
（且都**不在** ctest / CI 里）：

| sample | 现象 | 根因 | 基线也崩？ |
|---|---|---|---|
| `samples/fifo_bundle.cpp` | `empty=0` / exit=1 | bundle 字段被重绑定成零字面量总线的位切片 ⇒ 只读字段恒 0 | ✅（已由 `bundle-master-slave-io-2026-10` 修复） |
| `samples/nested_bundle_demo.cpp` | **segfault** | `bundle_base(const std::string&)` 在**基类构造期**调虚函数 `set_name_prefix()` ⇒ 对**尚未构造**的嵌套 bundle 成员调虚函数 = UB | ✅（将由 `bundle-ecosystem-hardening-2026-10` 修复） |
| `samples/switch_demo.cpp` | **segfault** | `make_uint<8>(0)` 在**无 `ctx_swap`** 的情况下创建节点 ⇒ 节点创建失败 ⇒ nullptr 解引用 | ✅（本 change 修复） |

⇒ **`samples/fifo_bundle` 的失败从落地起存在了数月才被审计发现** —— 这就是
「零覆盖 ⇒ 静默腐坏」的直接代价。

**注意 `examples/` 是有覆盖的**：`run_all_ported_tests.sh` 覆盖 28 个 main()-bearing
example ⇒ **只有 `samples/` 这 40 个是裸奔的**。

## What Changes

### D1: 新增 `scripts/run_all_samples.sh` —— samples 的回归护栏

- 遍历 `samples/*.cpp` 对应的已构建二进制，**逐个执行**并收集退出码
- **任一 sample 非零退出 ⇒ 脚本失败**（含 segfault / 超时）
- 输出汇总（PASS / FAIL 名单），便于 CI 定位
- ⚠️ MUST 设**超时**（`timeout 60`）—— 已有 sample 可能挂死（历史上
  `tests/test_axi_spi.cpp` 就有 5 处无界 `while (ready == 0) tick();`）
- ⚠️ MUST **区分**「本 change 引入的失败」与「既有失败」⇒ 首次落地时
  MUST 先跑一遍全量并把**既有失败**显式列出（当前：`switch_demo`），
  修复后再要求全绿

### D2: 修 `samples/switch_demo.cpp` 的既有 segfault

根因：`make_uint<8>(0)` 在**没有 `ctx_swap`** 的情况下创建节点
（`node_builder` 报「No active context for literal creation」）⇒ 节点为
nullptr ⇒ 后续解引用崩溃。

修法：按 `samples/AGENTS.md` 的约定补 `ch::core::context` + `ch::core::ctx_swap`
（与 `tests/test_fifo_example.cpp` 等一致）。

### D3: 挂进 CI

`.github/workflows/ci.yml` 增加一个 job 跑 `scripts/run_all_samples.sh`
（与既有的 `run_all_ported_tests.sh` job 并列）。

### D4: 文档同步

- 根 `AGENTS.md`：
  - COMMANDS 段加 `bash scripts/run_all_samples.sh`
  - 「已消除的过时信息」新增本 change 条目
  - NOTES 的测试计数段说明 samples 的覆盖方式（**不计入 CTEST_TOTAL**
    —— 它们不是 ctest target）
- `samples/AGENTS.md`：加「每个 sample MUST 以 0 退出；由
  `scripts/run_all_samples.sh` 守护」

## Impact

| 项 | 影响 |
|---|---|
| `scripts/run_all_samples.sh` | **新增**（40 个 sample 的回归护栏） |
| `samples/switch_demo.cpp` | 补 context（修既有 segfault） |
| `.github/workflows/ci.yml` | 新增 1 个 job |
| 根 `AGENTS.md` / `samples/AGENTS.md` | 文档同步 |
| 计数 | ⚠️ **CTEST_TOTAL 不变**（samples 不是 ctest target；它们走 shell 脚本）⇒ `scripts/verify_test_counts.sh` 无需改 |
| 风险 | 🟢 **低**。① 纯新增护栏 + 修一个 sample 的 context 缺失；② 但**首次落地会暴露既有失败** ⇒ MUST 先跑全量、逐个定性（既有失败要么修、要么显式豁免并记录，**MUST NOT** 静默跳过） |

## Non-Goals

1. **不**把 sample 改写成 ctest target（sample 的形态是「带 stdout 输出的演示」，
   与 Catch2 用例不同；用 shell 脚本守护更贴合其定位）
2. **不**修 `samples/nested_bundle_demo.cpp` 的 segfault —— 根因在 bundle 框架，
   归 `bundle-ecosystem-hardening-2026-10`
3. **不**给 sample 加行为断言（sample 的职责是「演示 + 自检」；
   行为断言属于 `tests/`）
4. **不**改 `examples/` 的覆盖方式（已有 `run_all_ported_tests.sh`）

## 附录 A：审计的复现命令

```bash
# 全量跑 40 个 sample，逐个记录退出码
for s in build/samples/*; do [ -x "$s" ] && [ -f "$s" ] || continue; \
    timeout 60 "$s" >/dev/null 2>&1 || echo "FAIL: $s (exit=$?)"; done
# ⇒ 实测：nested_bundle_demo (139=SEGV) / switch_demo (139=SEGV)

# 覆盖面
ls samples/*.cpp | wc -l                          # 40
grep -c 'samples/' tests/CMakeLists.txt           # 0
grep -c 'samples/' run_all_ported_tests.sh        # 0
grep -c 'examples/' run_all_ported_tests.sh       # 28（examples 是有覆盖的）
```
