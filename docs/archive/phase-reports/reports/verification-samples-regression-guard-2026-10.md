# 验证记录 — `samples-regression-guard-2026-10`

> 对应 change：`openspec/changes/samples-regression-guard-2026-10`
> 记录日期：2026-10-22
> 执行环境：Linux x86_64 / g++-14 / OpenSpec CLI **1.4.1**

---

## 1. Task 0 基线清单

### 1.1 覆盖面

| 项 | 命令 | 基线值 |
|---|---|---|
| sample 源文件 | `ls samples/*.cpp \| wc -l` | **40** |
| 已注册 target | `sed -n '/set(CH_SAMPLE_TARGETS/,/^)/p' samples/CMakeLists.txt \| sed -n 's/^    \([A-Za-z0-9_]*\)$/\1/p' \| wc -l` | **24** |
| 未注册 | 差集 | **16** |
| ctest 覆盖 | `grep -c 'samples/' tests/CMakeLists.txt` | **1**（注释行 ⇒ 有效 0） |
| examples 脚本覆盖 | `grep -c 'samples/' run_all_ported_tests.sh` | **0** |
| CI 里的 examples 运行覆盖 | `grep -rn "run_all_ported" .github/workflows/` | **0 命中**（CI 只有编译覆盖） |

### 1.2 已构建 sample 的退出码（落地前基线）

命令：

```bash
for s in build/samples/*; do [ -x "$s" ] && [ -f "$s" ] || continue; \
    timeout 60 "$s" >/dev/null 2>&1 || echo "FAIL: $s (exit=$?)"; done
```

| sample | exit | 说明 |
|---|---|---|
| `nested_bundle_demo` | **139** | SIGSEGV —— 根因在 bundle 框架（基类构造期调虚函数），归 `bundle-ecosystem-hardening-2026-10` ⇒ **显式豁免** |
| `switch_demo` | **139** | SIGSEGV —— 缺 `ctx_swap` ⇒ **本 change 修复** |
| 其余 22 个 | 0 | 通过 |

⚠️ 退出码语义：`139` = SIGSEGV，`124` = `timeout` 超时（两者在护栏汇总里区分）。

> 注：`samples/fifo_bundle.cpp` 的 `empty=0` / exit=1 已在
> `bundle-master-slave-io-2026-10` 修复 ⇒ 落地前基线已是 exit=0。

### 1.3 16 个未注册 sample 的逐个定性

判定命令：`g++ -std=c++23 -fsyntax-only`（用 `samples/CMakeFiles/counter.dir/flags.make`
的 `CXX_FLAGS` + `CXX_INCLUDES`）。

| sample | 免改可编译？ | 根因 | 处置 |
|---|---|---|---|
| `dag_compare` | ✅ **是** | — | **补进 `CH_SAMPLE_TARGETS`** |
| `tlm_bundle_demo` | ✅ **是** | — | **补进 `CH_SAMPLE_TARGETS`**（`deserialize<>` 路径的 sample 级消费方，`:121`） |
| `advanced_pod_bundle_serialization` | ❌ | `:55` `part1/part2` 不再是类型名 | 禁用清单 |
| `large_pod_bundle_example` | ❌ | 同上 | 禁用清单 |
| `conditional_demo` | ❌ | `:8` `chlib` 不是命名空间名 ⇒ 连带 `if_then` 未声明 | 禁用清单 |
| `spinalhdl_component_function_area_example` | ❌ | `:14` 基类名不认识 + `ch_sint` 未声明 | 禁用清单 |
| `spinalhdl_stream_example` | ❌ | `:2` `#include <chlib/fragment.h>` 文件不存在 | 禁用清单 |
| `stream_arbiter_example` | ❌ | `0xAB_d` 字面量后缀不被 `lit_dec` 接受 | 禁用清单 |
| `stream_demux_example` | ❌ | 同上 | 禁用清单 |
| `stream_flow_basic_example` | ❌ | 同上 | 禁用清单 |
| `stream_fork_example` | ❌ | 同上 | 禁用清单 |
| `stream_join_example` | ❌ | 同上 | 禁用清单 |
| `stream_fifo_example` | ❌ | `:28` `stream_fifo<...>` 无匹配重载 | 禁用清单 |
| `stream_component_example` | ❌ | `:37` 同上 + `:69` `context::get_default_clk` 改名 | 禁用清单 |
| `stream_handshake_example` | ❌ | `:34` `get_default_clk` / `get_default_rst` 改名 | 禁用清单 |
| `if_demo` | ❌ | `:39` 字面量宽度不匹配 + `:59` `ch_in<ch_bool>` 不能绑到 `const ch_bool&` | 禁用清单 |

**落地后**：40 源文件 = **26 已注册** + **14 已禁用**（无遗漏 ⇒ C1 绿）。

---

## 2. 护栏行为验证（变异测试）

护栏：`scripts/run_all_samples.sh`。每项变异 = 改坏一处 ⇒ 期望护栏**失败**。

| # | 变异 | 期望 | 实测 |
|---|---|---|---|
| M1 | 把一个**已通过**的 sample 塞进 `KNOWN_FAILING` | 报 **stale** 并失败 | ⚠️ **首次未通过**（见下）⇒ 修复后通过：`STALE EXEMPTION (已转绿)` + exit=1 |
| M2 | 把 `counter` 从 `CH_SAMPLE_TARGETS` 移除且不在禁用清单 | C1 注册一致性失败 | ✅ `UNREGISTERED counter` + exit=1 |
| M3 | 删掉 `build/samples/counter` 二进制 | C3 缺失二进制失败 | ✅ `MISSING BINARY` + exit=1 |
| M4 | 让一个未豁免的 sample 返回 3 | 失败并点名 | ✅ `FAILED (exit code: 3)` + exit=1 |
| M5 | `CH_SAMPLE_TARGETS` 里加一个不存在的 target | C2 数量守恒失败 | ✅ `MISMATCH: 已注册 27，实际执行 26` + exit=1 |

### ⚠️ M1 暴露的护栏自身缺陷（已修）

首次跑 M1 时：护栏**打印了** `STALE EXEMPTION (已转绿)`、`FAIL: 1`、`STALE: 1`，
但**退出码仍是 0** ⇒ 护栏形同虚设。

根因：STALE 分支只累加了 `FAIL` 计数，**没有**把 sample 名加进 `FAIL_NAMES`
⇒ 末尾的 `if [ ${#FAIL_NAMES[@]} -gt 0 ]` 判不到。

⇒ 修复：STALE 分支同时 `FAIL_NAMES+=("$name (stale exemption)")`。

> 这一条正是本 change 的主题（**「零覆盖 ⇒ 静默腐坏」**）在护栏自身的复刻：
> 一个「看起来在工作」的失败路径。已由 M1 变异测试钉住。

---

## 3. 修复验证

| 项 | 命令 | 结果 |
|---|---|---|
| `switch_demo` segfault | `./build/samples/switch_demo; echo $?` | **exit=0**（补 `ch::core::context` + `ctx_swap`） |
| 护栏全量 | `bash scripts/run_all_samples.sh; echo $?` | **exit=0**（PASS 25 / FAIL 0 / STALE 0 / MISSING 0） |
| 豁免项仍被执行 | 护栏输出 | `nested_bundle_demo  EXEMPTED (SEGFAULT (exit 139))` —— **崩溃可见**，只是不计入失败数 |
| C1 | 护栏输出 | `OK：40 个 sample 源文件全部有归属（已注册 26 / 已禁用 14）` |
| C2 | 护栏输出 | `OK：已注册 26 == 实际执行 26` |

---

## 4. 遗留（转交 follow-up）

- `nested_bundle_demo` 的豁免**必须**在 `bundle-ecosystem-hardening-2026-10`
  archive 时**摘除**（失效条件已写进护栏脚本的 `KNOWN_FAILING` 条目）。
- 14 个禁用 sample 的**编译修复**不在本 change 范围（Non-Goal 5）⇒ 独立 follow-up。
- `examples/` 的**运行**覆盖不在 CI 里（Non-Goal 4）⇒ 独立 follow-up。
