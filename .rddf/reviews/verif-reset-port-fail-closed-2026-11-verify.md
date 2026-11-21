# 验收 —— `.verif` 的 `RESET` 指向框架复位端口 ⇒ 两端同源 fail-closed

- **change**: `openspec/changes/verif-reset-port-fail-closed-2026-11/`
- **裁决**: **PASS**（0 失败项 · 已用轮次 **1**）
- **取证日**: 2026-11-21 · 全量重建后的 `build/`

---

## 1 判据来源（按优先级）

| # | 来源 | 内容 |
|---|---|---|
| 1 | **用户硬约束**（本会话） | 「**不得降级判据面**」＋「检查遗留 / 遗漏 / 剩余 / 债务并继续推进」 |
| 2 | `proposal.md` 的 **AC1–AC5** | 见下 §2 逐条取证 |
| 3 | 审查者结论（critic · `build` 档） | `APPROVE_WITH_CONDITIONS` · **0 BLOCKER** · 1 SHOULD_FIX（已修）· 2 NIT |
| 4 | 同型先例 | ADR-076 的三类扫描/运行期 fail-closed（宽端口 / sanitize 撞名 / 历史不足） |

---

## 2 逐条判定 + 证据

### AC1 —— 两端都 fail-closed 且**消息逐字相同**

```
$ build/tests/test_verif_wait_level          # 用例「verif-reset-port: `RESET default_reset` 两端同源 fail-closed」
All tests passed (118 assertions in 7 test cases)
```

用例内部做三件事：① 解释器 `run_verif` 返回 `ok=false` 且消息含 `default_reset`；
② 渲染器 `render_tb` 返回 `false`；③ **`rerr == res.error`**（逐字相等）。
**③ 判定**：✅ 满足。

### AC2 —— 判别力（两条变异各自转红 · 实测）

| 变异 | 实测 |
|---|---|
| **A**：移除 `verif_run.cpp` 的解释器侧调用 | ✅ 转红（`tests/test_verif_wait_level.cpp:355` · `assertions: 115 \| 114 passed \| 1 failed`）|
| **B**：移除 `verif_render.cpp` 的渲染器侧调用 | ✅ 转红（`...:364` · `assertions: 117 \| 116 passed \| 1 failed`）|
| 两侧还原 | ✅ 复绿（`118 assertions in 7 test cases`）|

⇒ ⇒ 每条变异都**先确认仍能编译**（`grep -c " error"` = 0），再跑测试 ⇒ ⇒ 不是「旧二进制假红」。
**③ 判定**：✅ 满足。

### AC3 —— 零破坏（既有 `.verif` 零处使用 `RESET`）

```
$ grep -rn "^RESET\|^ *RESET " examples/ --include=*.verif
（空）
```

⇒ ⇒ 12 处 `RESET` 命中**全部是注释行**（「原语封闭集：… / RESET」）⇒ ⇒ 无产物漂移
⇒ `verify_examples_rtl_sim.sh` **PASS（17 项）**（含 ⑥' 新鲜度：生成物与验证源一致）。
**③ 判定**：✅ 满足。

### AC4 —— 端口 ident → role 的优先级与 `Resolver::resolve` 同源

- `verif_internal.h::role_of()` 逐条镜像三条路径（① 根自有成员名 → ② 产物端口名 →
  ③ **成员名唯一**的后代兜底）。
- **批判性证据**（critic 的 SHOULD_FIX 即此条）：原实现只查 `is_top_io` 条目 ⇒ ⇒
  漏掉路径 ③ ⇒ ⇒ 「解释器接受并空跑 / 渲染器拒绝」的残洞 ⇒ ⇒ **已修**；
  并把渲染侧的检查**前移到端口解析之前** ⇒ ⇒ 后代 ident 的情形两端也**同消息**。
**③ 判定**：✅ 满足（SHOULD_FIX 已闭合）。

### AC5 —— 文档同步

| 落点 | 内容 |
|---|---|
| `docs/usage_guide/04-testbench-guide.md` | 「`RESET` 的已知边界」段（含**仍开放**的去断言竞态 B0 ②）＋ 原语表行改为精确语义 |
| `docs/CHANGELOG.md` | 一条（实测发现 / 硬化 / 判据载体 / **未闭合的相邻项**） |
| `.rddf/checks/examples-rtl-expectations.md` | phase3a 节新增「⛔ 为什么该 TB MUST 保持手写」· SoC 节新增「✅ ADR-076 后复核」 |
| `.rddf/checks/examples-rtl-sim-targets.tsv` | 两行 `generated=no` 的 note 改成**结论性**表述 + **收窄后**的复核条件（`NF=14` 守恒） |

**③ 判定**：✅ 满足。

---

## 3 回归面（全绿 · 全量重建后）

```
cmake --build build -j$(nproc)            → 0 error
ctest -LE perf                            → 262/262 PASS（rc=0）
./run_all_ported_tests.sh                 → PASS: 27 · FAIL: 0 · STALE: 0 · MISSING: 0
bash scripts/verify_*.sh（**全部 52 个**）  → 52 PASS · 0 FAIL
```

⚠️ `STALE-ARTIFACT: 16` = 已删 target 的**构建残留二进制**（脚本语义：清 `build/` 即消失，
**非归属问题**）⇒ ⇒ 不是失败项。

---

## 4 审查者结论与处置（critic · `build` 档）

- **判定**：`APPROVE_WITH_CONDITIONS` · `mergeable: yes` · `scope_exceeded: false` · **0 BLOCKER**
- **SHOULD_FIX（1）**：`role_of()` 漏掉 Resolver 的后代兜底 ⇒ ⇒ **已修**（见 AC4）。
- **NIT（2）**：
  - 「无时钟」检查排在 `RESET` 检查之后 ⇒ 「两者同时成立」时两端**消息**不同
    （判定一致）⇒ ⇒ **已在代码里加注释**限定「逐字相同」的适用范围（`verif_render.cpp`）。
  - 检查触发时 prelude 已部分写入 `ostream`（半成品）⇒ 与既有「无时钟」检查**同型**
    （既有模式）⇒ ⇒ 调用方按退出码门控 ⇒ 无假绿 ⇒ **记录，不改**。

---

## 5 遗留清扫（本次范围内）

| 项 | 结果 |
|---|---|
| TODO / FIXME / 半截实现 | **0**（`src/rtl_emit/` + `tests/test_verif_wait_level.cpp`） |
| 临时调试输出 | **0**（探针在 `/tmp/opencode/gap_probe/`，**未进仓**） |
| 探针 target / 临时 CMake 条目 | **无**（探针是仓外独立 `g++` 编译，未动 CMake） |
| 公开面登记义务 | **不触发**（共享函数在**内部**头 `src/rtl_emit/verif_internal.h`） |
| 被 skip 的测试 | **无新增** |

**已知局限（登记，不修）**：

1. **通用去断言竞态**（数据端口也受影响）：渲染侧 `repeat(n) @(posedge clk); name = <deassert>;`
   **无 `#1`** ⇒ 与解释器差 1 拍 ⇒ 登记 = 归档 change 的 **B0 ②**（**待 architect 裁决**）。
2. `RESET` 指向**框架复位端口** ⇒ 已 fail-closed ⇒ 「运行期中框架复位」**不可表达**
   ⇒ 需要框架侧新能力（`FRAME_RESET`）⇒ 归 architect。

---

## 6 已用轮次

**1**（首次验收即通过；无打回 ⇒ 剩余额度 2）。
