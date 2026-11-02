# 提案：路线 B（H 批）—— `priority_selector` / `round_robin_selector` 进双后端 parity

<!-- touched-components: priority_selector, round_robin_selector -->

- **依据**：`.rddf/plans/dual-backend-full-coverage-2026-11.md` §4 批次表 + §4 的「可转性 4 问」
- **规格影响**：**无 spec 变更** ⇒ 归档可用 `--skip-specs`
- **规模**：1 个新测试文件（2 个 `DUAL_BACKEND_TEST_PAIR` / 34 断言每后端）+ 1 条 CMake 注册
  + 计数锚点 + 命名诚实性登记表 **+2 条**
- **关联**：`objective-test-coverage-100pct` · capability `dual-backend-test-coverage` ·
  capability `chlib-stream-arbitration`（命名诚实性）

## Why

`include/chlib/selector_arbiter.h` 的 `priority_selector` 与 `round_robin_selector` 是
**纯组合**的请求选择器 ⇒ ⇒ 它们生成的 Verilog（长 `select` 链 + `bit_select` mux）
与 interpreter 是否逐周期一致，此前**从未验证过**。

⇒ 且 **`round_robin_selector` 有生产消费者**：`include/chlib/stream.h` 的
`stream_arbiter_round_robin` 调它 ⇒ ⇒ 它的语义正确性**直接决定 stream 仲裁器的正确性**
⇒ ⇒ 值得单独钉。

### ⚠️ 同文件的 `round_robin_arbiter` **刻意不做**

`UpdatePriority`（`selector_arbiter.h:94-115`）用**普通 C++ `if (result.valid)`** 判断
「本轮是否授予了某个请求」⇒ ⇒ `result.valid` 是**非 const 组合节点** ⇒ ⇒
`ch_bool::operator uint64_t()`（`src/lnode/bool.cpp:43-49`）**只对 const 节点返回真值**
⇒ ⇒ **C++ `if` 在建图期恒假** ⇒ ⇒ `next_priority` **永不更新** ⇒ ⇒ `ptr_reg` 恒 0
⇒ ⇒ **`round_robin_arbiter` 恒从位置 0 扫描（= 不轮转）**。

⇒ **独立探针实测**：`next_ptr` 连续 3 拍恒 **0**（期望每拍 +1）。
⇒ ⇒ 这是「**条件量在建图期被求值**」缺陷家族的又一实例（`_if` / `uart_tx` FSM /
cache wrapper / `fixed_point_divider` / `add_with_carry`）⇒ ⇒
**独立立项**：`.rddf/improvements/selector-round-robin-not-rotating-2026-11.md`。

⇒ ⇒ **本批刻意不做**它的 dual parity：带**已知缺陷**的组件做 parity 会把缺陷**盖章**
（两后端一致地错 ⇒ parity **看不见**）⇒ ⇒ **先修缺陷、再做覆盖**。

## 可转性 4 问

| # | 问题 | 回答 |
|---|---|---|
| ① | 语义是否已被别的 dual 文件覆盖？ | **否** —— 两个符号在全部 54 个 dual 文件里**各出现 0 次** |
| ② | 断言形态有判别力吗？ | **有** —— 变异 **6/6 全抓**（见下） |
| ③ | 与后端无关吗？ | **否** —— 真跑仿真 + 读顶层端口，正是要对比两后端 |
| ④ | 形态值得新写吗？ | **是** —— 既有测试**不可转换**（见下）⇒ 只能新写 |

### ⚠️ 既有测试**不可转换**

`tests/chlib/test_selector_priority.cpp`（4 用例）与
`tests/chlib/test_selector_round_robin.cpp`（2 用例）**全是裸 context**
（`ch_device=0`、无顶层端口）⇒ ⇒ 命中 **R6 排除项** ⇒ ⇒ **结构上不可转换**
⇒ ⇒ 必须新写（自带 wrapper DUT + 手推期望）。
⇒ ⚠️ 两个原文件 **MUST NOT 修改**。

## 期望值（**手推**，禁抄仿真输出）

模型（`/tmp/opencode/gen_selector_table.py`，独立 Python 实现 + 自检）：

```
priority_selector<N>(request):
    grant = 0 ; valid = false
    for i in 0..N-1:                       ← 低位 → 高位
        grant = (bit_i(request) and not valid) ? (1 << i) : grant
        valid = bit_i(request) or valid
    ⇒ grant = **最低**置位胜；valid = (request != 0)

round_robin_selector<N>(request, last_grant):   ← last_grant 是 one-hot
    has_last  = last_grant != 0
    last_idx  = onehot_to_binary(last_grant) if has_last else 0
    start_pos = (last_idx + 1) % N
    grant = 0 ; valid = false
    for i in 0..N-1:
        pos = (start_pos + i) % N          ← **环绕**扫描
        grant = (bit_pos(request) and not valid) ? (1 << pos) : grant
        valid = bit_pos(request) or valid
```

⚠️ **位宽自洽性**：`compute_idx_width(4)` = `bit_width(3)` = **2** ⇒ ⇒ 「2 位加减」在数值上
**恒等于 mod 4** ⇒ ⇒ `(start_pos + i) % 4` 的环绕语义**无歧义**（无论加法被扩到 2 位
还是 3 位，mod 4 的结果相同）。

## 🔴 判别力要害：**单位置位时两种语义给出同一个 grant**

「低位优先」**只有多位同时置位时才可观测** ⇒ ⇒ 激励 MUST 含多位向量：

| 向量 | 期望 | 若语义错则 |
|---|---|---|
| `priority` · `request=0b1010` | `grant=0b0010`（最低置位胜） | 变成 `0b1000`（最高胜） |
| `priority` · `request=0b1111` | `grant=0b0001` | 变成 `0b1000` |
| `round_robin` · `request=0b1111, last=0b0001` | `grant=0b0010`（起点 1） | 起点恒 0 ⇒ `0b0001` |
| `round_robin` · `request=0b1001, last=0b0010` | `grant=0b1000`（扫 2,3,0,1） | 起点恒 0 ⇒ `0b0001` |
| `round_robin` · `request=0b0110, last=0b0100` | `grant=0b0010`（扫 3,0,1,2） | 起点恒 0 ⇒ `0b0010`（同值 ⇒ **该向量单独零判别力**） |

⇒ ⇒ 激励共 **7 + 10 = 17** 组，覆盖「无请求 / 单位置位 / 多位置位 / 起点推进 / 环绕扫描」。

## 判别力（变异证据；每条先确认仍可编译；还原用 `cp` 后重建）

| 变异 | 内容 | 转红 |
|---|---|---|
| M1 | priority 的「先到先得」改成「后到覆盖」（`!result.valid` → `result.valid`）⇒ 变成**最高**置位胜 | 两半边 |
| M2 | priority 的 `valid` 恒 0 | 两半边 |
| M3 | round_robin 的扫描起点少 1（`+ 1_d` → `+ 0_d`） | 两半边 |
| M4 | round_robin 的 `grant` 恒 0 | 两半边 |
| M5 | round_robin 的 `valid` 恒 1 | 两半边 |
| M6 | round_robin 的扫描方向反转（往回扫） | 两半边 |

⇒ **6/6 全抓**。

## 🔴 连带：命名诚实性登记表（B8 的 R3）

本批与 G 批**改动过** chlib 组件 ⇒ ⇒ 护栏 `scripts/verify_chlib_naming_honesty.sh` 的
**R3**（per-change 义务）要求「最近的 chlib change MUST 在 proposal 里用
`<!-- touched-components: ... -->` 声明它触及的组件，**且那些组件 MUST 已在登记表登记**」。

⇒ ⇒ 本 change 的 proposal 已带标记（`priority_selector` · `round_robin_selector`）
⇒ ⇒ 登记表 **+2 条**（判别性用例 = 本文件的 tag）。
⇒ ⇒ G 批的归档 proposal **补上**标记（3 个 pipe）⇒ ⇒ 登记表 **+3 条**。

### 🔴 顺带修掉护栏自检的一处**脆弱性**（`scripts/lib/chlib_naming_honesty.py`）

自检的「删掉一条登记」投毒**删的是登记表第一行** ⇒ ⇒ 而 R1 只查「登记 ⊆ 组件全集」、
R2 只查「判别性用例仍存在」⇒ ⇒ 删掉一条**与最近 change 无关**的登记时
**R1/R2 仍绿、R3 也不红** ⇒ ⇒ 投毒**没有效果** ⇒ ⇒ 自检会误报「判别力为零」
（**实测**：G 批归档后自检真的红了 ⇒ ⇒ 该脆弱性**真实存在**，不是理论风险）。

⇒ ⇒ **修法**：投毒**定位到「最近 change 触及」的那一行**（读 proposal 的
`touched-components` 标记）⇒ ⇒ 删掉它 ⇒ ⇒ R3（该组件未登记）**必红**
⇒ ⇒ 且**fail-closed**（找不到 change / 标记 / 对应行 ⇒ ⇒ 抛异常 ⇒ ⇒ 自检报
「投毒失败（fail-closed）」而不是「判别力为零」）⇒ ⇒ 自检 **3/3** 全绿。

## What Changes

| 文件 | 内容 |
|---|---|
| `tests/chlib/test_selector_arbiter_dual.cpp`（**新**） | 2 个 wrapper DUT + 2 pair（7 组 + 10 组向量 × 2 输出）= **34 断言/后端** |
| `tests/CMakeLists.txt` | 1 条 `add_dual_backend_test` |
| `AGENTS.md` | 4 个计数锚点 + 3 处 NOTES 计数 + 本批 NOTES 条目（含护栏变更登记） |
| `.rddf/checks/chlib-naming-honesty-registry.md` | **+5 条**（3 个 pipe + 2 个 selector），附判别性用例的 tag |
| `scripts/lib/chlib_naming_honesty.py` | 投毒定位改为「最近 change 触及的那一行」（fail-closed） |
| `openspec/changes/archive/2026-11-03-dual-backend-parity-route-b-stream-pipeline-2026-11/proposal.md` | 补 `touched-components` 标记（G 批） |
| 台账 | plan §5 墙钟 · objective 现状 57 → 58 与可达上限 · handoff §1/§4.6/§5 |

## Verification（前瞻 · 可勾选）

- [x] **N2′**：**68 == 2 × 34**
- [x] 变异 **6/6 全抓**（每条先确认仍可编译；还原用 `cp` 后重建）
- [x] 期望值由**独立 Python 模型**逐拍生成（含模型自检）⇒ **一次通过**
- [x] **原文件 `tests/chlib/test_selector_priority.cpp` / `test_selector_round_robin.cpp` MUST NOT 修改**
- [x] verilator 半边**真的跑了**（N2′ 的 2× 断言差是正向证据）
- [x] 全量重建后 `ctest -LE perf` **224/224** 零回归
- [x] 命名诚实性护栏 + **自检 3/3** 全绿
- [x] 计数锚点同步且两个计数护栏双绿
- [x] 归档用 `--skip-specs`（本 change 显式声明无 spec 变更）
