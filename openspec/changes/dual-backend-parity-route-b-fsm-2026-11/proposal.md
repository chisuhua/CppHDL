# 提案：路线 B 第 1 批 —— `ch_state_machine` 的**运行期转移**进双后端 parity

- **依据**：`.rddf/plans/dual-backend-full-coverage-2026-11.md` §4 **H 批**（路线 B）
  + objective `objective-test-coverage-100pct` §2 重述后的「诚实上限」（含
  **新增有意义的 dual 文件数**）
- **规格影响**：**无 spec 变更**（`.openspec.yaml` 显式声明 `skip_specs: true`）
- **规模**：1 个新测试文件 + 1 条 CMake 注册 + 计数锚点
- **关联**：`objective-test-coverage-100pct` · capability `dual-backend-test-coverage`

## Why

`ch_state_machine` 是仓内**唯一**的 FSM DSL，而它的**运行期转移**在
**verilator codegen 路径上从未被验证过**。

⚠️ **这不是理论风险 —— 同一个缺陷类已在仓内两次真实发生，且两次都被弱断言掩盖**：

| 事故 | 症状 | 为什么当时没被发现 |
|---|---|---|
| `examples/spinalhdl-ported/uart/uart_tx.cpp` | 生成的 Verilog 里状态寄存器 `q <= q`（**自环**）；`is_in(...)` 的比较节点**零消费者** ⇒ FSM **从未工作过** | 该 example **没有**任何行为自检（只查退出码） |
| `tests/test_verilator_complex_e2e.cpp` 的 `SpiFsmFixture` | 状态卡死在 `TRANSFER`、`shift` 恒 0（`STOP` 不可达） | 断言**只有 parity** ⇒ 两后端由**同一份** fixture 生成 ⇒ 缺陷**同构** ⇒ parity 结构性失明 |

⇒ ⇒ **⇒ 必须有一条「状态序列」层面的判据，且期望值手推**（不是 parity 自证）。

## 为什么**不是**转换 `tests/test_state_machine.cpp`（如实说明）

那个文件 8 个用例里 **6 个**是**编译期 / 构图期**判据（被移除的符号不存在 ·
`set_entry` 越界抛异常 · `contrib` 重复抛异常 · `compute_state_bits` 纯函数）
⇒ ⇒ 对两后端是**同一段 C++ 代码** ⇒ ⇒ **dual 化零增量价值**。
剩下的 2 个运行期用例（`StateMachineDut` / `DefaultTailDut`）只覆盖
**「复位后停在 entry」**与**「`resolve_or` 默认尾部」** ⇒ ⇒ **整个转移路径零覆盖**。

⇒ ⇒ 因此本 change 是**新增**（路线 B），**不是**转换：
原文件 **MUST NOT 修改**。

## What Changes

| 文件 | 内容 |
|---|---|
| `tests/test_state_machine_dual.cpp`（**新**） | 2 个 `DUAL_BACKEND_TEST_PAIR`：<br>① 无条件链 A→B→C（判别「状态寄存器是不是自环」）<br>② 条件转移（电平敏感 · 计数器产生互斥条件） |
| `tests/CMakeLists.txt` | 1 条 `add_dual_backend_test` |
| 计数锚点 | `CTEST_TOTAL` 214 → **215** · `VERILATOR_E2E_TOTAL` 46 → **47** · `DUAL_BACKEND_PARITY_TOTAL` 37 → **38** · `PAIRS` 210 → **212** · non-perf 203 → **204** · `tests/test_*.cpp` 125 → **126** |

## 期望值的推导依据（**手推**，禁抄仿真输出）

语义前提（由**两条已知良好的 interpreter 断言**反推、并与 RTL 寄存器语义一致）：

1. 状态寄存器在**时钟沿**更新 ⇒ 沿**之后**读到的是**新**状态；
2. 组合逻辑读到的是**沿之前**的状态；`cyc` 与状态寄存器**同沿**推进；
3. `transition_when` **电平敏感**（条件为真的那一拍就转移）；
4. 转移的 `select` 树「后注册者优先」，但各 `is_in(S)` **两两互斥** ⇒ 顺序不影响结果。

互证证据（既有 interpreter-only 用例）：

- `StateMachineDut`（**无**转移）tick1 后 = **0**（A）—— 枚举 0 == 复位值；
- `DefaultTailDut`（A→B **无条件**）tick1 读到 `val`=**7**（A 的贡献）、tick2 读到
  **99**（B 未被覆盖 ⇒ 默认尾部）⇒ ⇒ 证明「沿前状态驱动组合逻辑、沿后状态是新值」。

逐拍金标见文件头注释（DUT 1 三拍 / DUT 2 六拍，**逐项**手推）。

## Verification（前瞻 · 可勾选）

- [ ] **N2′ 判据**：断言数(with verilator) == **2 ×** 断言数(interpreter-only)
- [ ] 期望值**全部手推**（文件头逐条写明推导 + 互证来源）
- [ ] 原文件 `tests/test_state_machine.cpp` **MUST NOT 修改**
- [ ] 变异证明判别力：把 `transition_when` 的注册删掉 ⇒ 状态停在 A ⇒ **红**
- [ ] `ctest -LE perf` 零回归
- [ ] 计数锚点同步且 `verify_test_counts.sh` / `verify_dual_backend_parity.sh` 双绿
