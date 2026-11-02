# 提案：路线 B 第 3 批 —— `sync_fifo` 本体的**空/满门控**进双后端 parity

- **依据**：`.rddf/plans/dual-backend-full-coverage-2026-11.md` §4 **H 批**（路线 B）
- **规格影响**：**无 spec 变更**（`.openspec.yaml` 显式声明 `skip_specs: true`）
- **规模**：1 个新测试文件 + 1 条 CMake 注册 + 计数锚点
- **关联**：`objective-test-coverage-100pct` · capability `dual-backend-test-coverage`

## Why

`chlib::sync_fifo` 是**所有** FIFO 型组件的本体（`stream_fifo` /
`StreamBuilder::queue` / 两个 cache 的队列都建在它上面），而它的
**空/满门控（自保护）** 在 **verilator codegen 路径上从未被验证过**。

⚠️ **这个缺陷类是仓内真实发生过的**（ADR-045 / change
`chlib-stream-semantics-2026-10`）：

| 缺陷 | 后果 |
|---|---|
| 写口用**裸 `wren`** ⇒ 满 FIFO 再写 | 写指针回绕 ⇒ **覆写已写入的数据**（静默损坏） |
| 读指针用**裸 `rden`** ⇒ 空 FIFO 空读 | 读指针**每拍自由运行** ⇒ 与写指针失步 ⇒ 之后写入的元素**永远读不出来**（`count` 归零但读到错槽） |

⇒ 这两条**只在 interpreter 上被钉过**（`tests/chlib/test_fifo.cpp` 的
`[fifo][sync_fifo][regression]` 组）⇒ ⇒ **codegen 侧零覆盖**。
而「指针门控」正是**寄存器 + 组合条件**的混合体 ⇒ 两条实现路径最容易分叉的地方。

⚠️ **为什么不能复用既有 dual 文件**：`tests/test_stream_fifo_dual.cpp` 覆盖的是
`stream_fifo`（**wrapper**，读使能由外部驱动）⇒ ⇒ **不是** `sync_fifo` 本体
⇒ ⇒ 本体的 `will_write` / `will_read` 门控**从未**在 verilator 下跑过。

## 为什么是【新增】而不是转换

`tests/chlib/test_fifo.cpp` 是**裸 context + 直接调 `sync_fifo()`** 的形态
（无 `ch_device` / 无顶层端口）⇒ ⇒ `DUAL_BACKEND_TEST_PAIR` **不适用**
（macro 需要 `ch_device<DutType>`）⇒ ⇒ 必须**新建**带端口的 DUT 包一层。
⇒ 原文件 **MUST NOT 修改**。

## 被**判定为不适用**的候选（如实登记，不硬转）

| 候选 | 为什么不转 |
|---|---|
| `tests/test_stream_operators.cpp`（15 条断言） | **全部**是**结构性**断言（`impl() != nullptr` · `num_srcs()` · `src(0) == …`），且其中带仿真的一条是**裸 context**（不是 `ch_device`）⇒ ⇒ 既无 DUT 也无运行期语义 ⇒ ⇒ dual 化只会制造**零判别力**的假覆盖 |

## What Changes

| 文件 | 内容 |
|---|---|
| `tests/chlib/test_sync_fifo_dual.cpp`（**新**） | **2** 个 pair：① 写 4 再按序读（顺序 + 数据完整性）② **满写 MUST NOT 覆盖** + **空读 MUST NOT 移动指针**（自保护） |
| `tests/CMakeLists.txt` | 1 条 `add_dual_backend_test` |
| 计数锚点 | `CTEST_TOTAL` 217 → **218** · `VERILATOR_E2E_TOTAL` 49 → **50** · `DUAL_BACKEND_PARITY_TOTAL` 40 → **41** · `PAIRS` 217 → **219** · non-perf 206 → **207** · `tests/chlib/test_*.cpp` 35 → **36** |

## 期望值（**手推**，禁抄仿真输出）

语义前提（逐条来自 `include/chlib/fifo.h` 的实现）：

1. `will_write = wren & !full(count)` · `will_read = rden & !empty(count)`
   ⇒ 门控在**写口**与**读指针**上（这正是本文件要钉的东西）；
2. `count` / `write_ptr` / `read_ptr` 都是 `ch_reg` ⇒ **同沿**推进；
3. `q_async` 是 **0-cycle 组合**读（`aread`）⇒ 呈现**当前** `read_ptr` 指向的槽
   ⇒ 沿后读到的是「沿后指针」指向的元素；
4. `memory.write` 在沿上写入（受 `will_write` 门控）。

⇒ ⇒ **读数 MUST 用「`rden=0` 采样」两步法** —— `q_async` 在 pop 沿**之后**就指向
**下一个**队首（AGENTS.md 记的 FIFO 协议）⇒ 「要看的元素」必须在 **pop 之前**采样。

深度 = `1 << ADDR_WIDTH` = **4**（`sync_fifo<8, 2>`）。

## Verification（前瞻 · 可勾选）

- [ ] **N2′**：断言数(with verilator) == **2 ×** 断言数(interpreter-only) ⇒ `88/44`
- [ ] 期望值**全部手推**（文件头逐条写明）
- [ ] 原文件 `tests/chlib/test_fifo.cpp` **MUST NOT 修改**
- [ ] 变异证明：M1 写口去掉 `!is_full` 门控 ⇒ 红 · M2 读指针去掉 `!is_empty` 门控 ⇒ 红
- [ ] `ctest -LE perf` 零回归 · 既有 `test_fifo` / `test_stream_fifo_dual` 不回归
- [ ] 计数锚点同步且两个护栏双绿
