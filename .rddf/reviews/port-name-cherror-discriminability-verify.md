# 验收 —— 端口重名 `CHERROR` 的**触发**判别力补齐（N3 补遗）

- **载体**: `tests/test_port_name_parity.cpp`（新增 2 个用例 · **同一 target** ⇒ 锚点不变）
- **裁决**: **PASS**（0 失败项 · 已用轮次 **1**）
- **取证日**: 2026-11-21 · 全量重建后的 `build/`
- **性质**: **判据补齐**（纯测试增量 · **零产品代码改动** · **零公开面改动**）

---

## 1 判据来源（按优先级）

| # | 来源 | 内容 |
|---|---|---|
| 1 | **用户口径**（本会话） | 「做下一项 B1 内容」⇒ B1 = 剩余任务清单里的「N3 补遗 · 端口重名 `CHERROR` 判别力」 |
| 2 | `.rddf/roadmap/phase-6-rtl-productization.md` §4 · N3 补遗行 | 立项内容 = **正向**（构造重名 ⇒ 断言 `CHERROR` 触发 **且** 两名不同）∧ **反向**（不重名 ⇒ 断言**无** `CHERROR`）+ **判别力自证**（删 `CHERROR` ⇒ 正向必红）|
| 3 | 仓内既有纪律 | 「判别性护栏 MUST **同时**有正反两面」（根 `AGENTS.md` ANTI-PATTERNS）|

---

## 2 逐条判定 + 证据

### AC1 —— 正向：构造重名 ⇒ `CHERROR` 触发 **且** 两名不同

```
$ ./build/tests/test_port_name_parity
All tests passed (40 assertions in 7 test cases)
```

用例「N3 补遗: 显式端口名冲突 ⇒ CHERROR 被点名（且两个名字仍唯一）」的断言集：

| 断言 | 内容 |
|---|---|
| `err.find("端口命名冲突") != npos` | **触发**本身 |
| `err.find("top_dup_prog_addr") != npos` | 消息**点名 base**（否则「点名报告」退化成「报了个错」）|
| `err.find("top.dup.prog_addr") != npos` | 消息**点名节点** |
| `names[0] == "top_dup_prog_addr"` ∧ `names[1] == "top_dup_prog_addr_1"` ∧ `names[0] != names[1]` | 报错**不**等于丢名（点名报告 ≠ 拒绝整份设计）|

**③ 判定**：✅ 满足。

### AC2 —— 反向：不重名 ⇒ **无** `CHERROR`

用例「N3 补遗: 无冲突 ⇒ 不触发 CHERROR（反向判据）」：

- `err.find("端口命名冲突") == npos` ⇒ ⚠️ **不是**断言「stderr 为空」（其它级别也走 stderr）⇒
  ⇒ **精确且不受噪声影响**；
- 对照：`it->second == "top_prog_addr"`（**无** `_1` 后缀 ⇒ ⇒ 差别来自冲突本身）。

**③ 判定**：✅ 满足。

### AC3 —— 判别力自证（**两条**变异 · 各自只打自己那一格）

| 变异 | 构造 | 实测 | 判定 |
|---|---|---|---|
| **M1** | 删 `CHERROR`（保留 `(void)base; (void)node->name();` ⇒ **变量仍被使用** ⇒ 编译干净）| ✅ **仅正向红**（`tests/test_port_name_parity.cpp:393`）· `assertions: 32 \| 31 passed \| 1 failed` · 其余 **6** 用例绿 | ✅ |
| **M2** | 报错**无条件**化，但**保留** `reserved.insert(base)` 的占位副作用 ⇒ ⇒ 名字行为**零变化** | ✅ **仅反向红**（`:435`）· `assertions: 38 \| 37 passed \| 1 failed` · 其余 **6** 用例绿 | ✅ |
| 还原 | `cp` 回原始文件 | ✅ `git diff --stat include/codegen_naming.h` = **空**（逐字还原）⇒ **重建**后复绿（40 断言 / 7 用例）| ✅ |

⚠️ **M2 的第一次实现不纯**（用 `if (true)` 顶掉了 `reserved.insert` ⇒ ⇒ 连**占位副作用**一起改
⇒ ⇒ `Ratchet 3` 跟着红 ⇒ ⇒ 3 个用例红 ⇒ ⇒ 证据不纯）⇒ ⇒ **已重做为干净变异** ⇒ ⇒
如实登记在 `.rddf/roadmap/phase-6-rtl-productization.md` §4 的 dated 注记里。

⚠️ **变异纪律**：每条变异都**先确认仍能编译**（`grep -c " error|Error [0-9]"` = **0**）⇒ ⇒
跑的是**新二进制**；还原后**重建**再跑（还原源码 ≠ 复原 —— 二进制也是状态）。

**③ 判定**：✅ 满足（正反两面各有判别力）。

### AC4 —— 零破坏（产品代码零改动）

- 变更文件 = `tests/test_port_name_parity.cpp`（**仅**测试）⇒ ⇒ `include/codegen_naming.h` 最终
  `git diff` = **空** ⇒ ⇒ 命名行为**零变化**；
- `Ratchet 3`（原有用例）在两条变异下**始终绿**（M1 绿 / M2 绿）⇒ ⇒ 新用例**不**与既有判据冲突。

**③ 判定**：✅ 满足。

### AC5 —— 锚点不变

- 新增用例在**既有 target**（`add_catch_test(test_port_name_parity …)`）⇒ ⇒ **未新增 target**
  ⇒ ⇒ `CTEST_TOTAL`（273）/ `VERILATOR_E2E_TOTAL`（80）/ `DUAL_BACKEND_PARITY_TOTAL`（66）/
  `DUAL_BACKEND_PARITY_PAIRS`（286）/ `EXAMPLES_TEST_SOURCES`（20）**全部不动**
  ⇒ ⇒ `verify_test_counts.sh` **PASS**；
- 该 target **无** `verilator_e2e` 标签 ⇒ ⇒ 新用例**不**进 parity 口径
  ⇒ ⇒ `verify_dual_backend_parity.sh` **PASS**。

**③ 判定**：✅ 满足。

### AC6 —— 文档同步

| 落点 | 内容 |
|---|---|
| `docs/CHANGELOG.md` | 一条（缺口 / 2 个用例 / 观测手段 / 两条变异表 / **M2 不纯的如实登记**）|
| `.rddf/roadmap/phase-6-rtl-productization.md` §4 | N3 补遗行**追加 dated 注记**（**不改写**原登记 · 防重复开项）|

**③ 判定**：✅ 满足。

---

## 3 回归面（全绿 · 全量重建后）

```
cmake --build build -j$(nproc)            → 0 error
ctest -LE perf                            → rc=0（base 237 + chlib 1 + spinalhdl-ported 24 + verilator_e2e 80）
./run_all_ported_tests.sh                 → PASS: 27 · FAIL: 0 · STALE: 0 · MISSING: 0
bash scripts/verify_*.sh（**全部 52 个**）  → 52 PASS · 0 FAIL
```

⚠️ `STALE-ARTIFACT: 16` = 已删 target 的**构建残留二进制**（清 `build/` 即消失，**非归属问题**）。

---

## 4 遗留清扫（本次范围内）

| 项 | 结果 |
|---|---|
| TODO / FIXME / 半截实现 | **0**（`tests/test_port_name_parity.cpp`）|
| 临时调试输出 | **0**（stderr 捕获是**判据手段**，非调试残留；RAII 保证用完即还原）|
| 临时文件 | **0**（`tmpfile()` 由 C 运行时管理 ⇒ 出作用域即删除）|
| 被 skip 的测试 | **无新增** |
| 公开面登记义务 | **不触发**（未改任何公开符号；`verify_public_api_visibility.sh` PASS）|
| 探针 / 临时 CMake 条目 | **无**（复用既有 target）|

**已知局限（登记，不修）**：

1. **POSIX-only**（`dup2` / `tmpfile` / `<unistd.h>`）⇒ ⇒ CI 平台范围 = **仅 Ubuntu**
   （2026-11-03 用户裁决）⇒ ⇒ macOS/Windows **无 CI 证据**（已知且接受，与仓内既有
   POSIX 依赖同型）。
2. **stderr 捕获是进程级** ⇒ ⇒ 要求**同一 target 内串行**（ctest 每 target 独立进程 ⇒
   满足；`ctest -j` 不会并行同一 target 内的用例）。
3. `CHERROR` 的**消息文案**被断言（`端口命名冲突` / base / 节点名）⇒ ⇒ 文案改写会红 ⇒ ⇒
   这是**有意的**（文案是「点名报告」契约的一部分）⇒ ⇒ 改文案时 MUST 同步本用例。

---

## 5 已用轮次

**1**（首次验收即通过；无打回 ⇒ 剩余额度 2）。
