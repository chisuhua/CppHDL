# 交接快照：objective-test-coverage-100pct（路线 A/B 推进）

- **时间**：2026-11-02（builder，第 2 次交接）
- **objective**：`.rddf/roadmap/objectives/objective-test-coverage-100pct.md`
- **plan**：`.rddf/plans/dual-backend-full-coverage-2026-11.md`
- **用途**：下一次会话**直接从这里继续**，不需要重新盘点。

---

## 1. 当前计数（实测，全部已对齐 AGENTS.md 锚点）

| 量 | 值 | 命令 |
|---|---|---|
| **parity 源文件数** | **49**（目标 60，差 **11**） | `bash scripts/verify_dual_backend_parity.sh` |
| parity macro 调用数（PAIRS） | **243** | 同上 |
| `CTEST_TOTAL` | **226** | `cd build && ctest -N \| tail -1` |
| `VERILATOR_E2E_TOTAL` | **58** | `ctest -N -L verilator_e2e \| tail -1` |
| non-perf | **215** | `ctest -N -LE perf \| tail -1` |
| `tests/test_*.cpp` | **129** | `ls tests/test_*.cpp \| wc -l` |
| `tests/chlib/test_*.cpp` | **41** | `ls tests/chlib/test_*.cpp \| wc -l` |
| 全量测试 | **215/215 绿**（**全量重建后**） | `cmake --build build -j$(nproc) && cd build && ctest -LE perf -j4` |

⚠️ 工作区里有一批**并发会话**的文件（`hierarchical-rtl-output`：`docs/adr/ADR-058..062`、
`docs/usage_guide/05-component-design.md`、`HIERARCHICAL-RTL-AUTHORING-GUIDE.md`、
`openspec/changes/hierarchical-rtl-output-2026-11/` 等）
⇒ ⇒ **提交时 MUST 按路径显式 `git add`，MUST NOT 用 `git add -A`**。

## 2. 已完成批次（累计 13 个 commit，全部未 push）

第 1 会话（`0cd957a` … `2fe6b69`，11 个）：路线 B 第 6 批 · `sread` 禁读拍修复与归档 ·
路线 A D 批第 1 个（`test_mem_connection_dual`，4 pair）。

第 2 会话（本会话）：路线 A **D 批第 2 个** = `test_mem_comb_read_dual`
（**2 pair / 14 断言**：`ch_mem` 写口 × `aread` 交互）+ 归档 + 台账 47 → **49**.

parity 轨迹：**37 → 49**。

## 3. 🔴 本会话最重要的产出：一次**「可转性」否定**（下次必须复用这个判据）

交接文档第 1 版判 `test_mem_comb_read.cpp`「6 可转」，**逐个读断言实现后实际只有 2 个**
有增量。⇒ ⇒ **后续每一批的第一件事都必须是「这条用例转 dual 后是否真有增量」**，
判据三条（本次实测得出）：

| 判据 | 怎么查 | 结论 |
|---|---|---|
| **① 是否已被别的 dual 文件覆盖？** | 按**语义 + 判别形态**（不是按文件名）在 `tests/**/*dual*.cpp` 里搜 | `test_mem_comb_read` 的 aread/sread 时序对 **已被** `test_mem_read_timing_dual.cpp` 覆盖，连**初值/期望值**都一样 ⇒ 重复转换 = 零增量 |
| **② 断言形态是否对目标维度有判别力？** | 见下方 **T2** | `:203` / `:247`（输入地址 + `tick()` 后读）**对 0/1-cycle 零判别力** |
| **③ 是否与后端无关？** | 读的是**生成的 Verilog 文本** / 纯编译期 | `:368` / `:439` 是 codegen 文本断言 ⇒ 转 dual 零增量 |

⇒ ⇒ **后果**：`objective-test-coverage-100pct` §2 的「路线 A 剩余 **12**，逐个复核
全部成立」里**至少有 2 个是重复计数** ⇒ ⇒ 我已在 objective 台账里登记
「算术不自洽（49 + 12 + 1~3 = 62~64 vs 上限 58~60）⇒ 需 planner 重算上限」
（**只交数字 + 证据，未改判据**）。

⇒ ⇒ ⚠️ **下一批（`test_mem_timing.cpp`，「7 可转」）MUST 先做同样的三问** ——
它大概率也有重复（`test_mem_read_timing_dual` / `test_mem_sread_disable_dual` /
本批的 `test_mem_comb_read_dual` 已覆盖了 mem 读侧的大部分语义面）。

## 4. 下一步（明确的第一个动作）

### D 批剩 1：`tests/test_mem_timing.cpp`

**第一个动作不是写代码**，而是：把该文件每个 TEST_CASE 与
`test_mem_read_timing_dual.cpp` / `test_mem_sread_disable_dual.cpp` /
`test_mem_comb_read_dual.cpp` / `test_mem_connection_dual.cpp` 的**语义面**逐条对照
⇒ 把「重复 / 无判别力 / 与后端无关」三类先剔掉 ⇒ 只转剩下的。
⚠️ 已知：该文件的 `sread holds its value...` 已被订正为 **clears**（`sread` 禁读拍清零）。

### 之后

| 批 | 文件 | 备注 |
|---|---|---|
| E（3） | `test_bundle_connection`(20) · `test_bundle_master_slave`(9) · `chlib/test_cache_complete`(14) | cache 可能触发 ADR-041 的 Async golden-value 分类 |
| F（6） | `chlib/test_onehot` · `test_operation_results` · `test_rv32i_pipeline` · `test_module_advanced` · `chlib/test_forwarding` · `chlib/test_hazard_complete` | 多 DUT；CI 墙钟风险最大 ⇒ **先实测再决定拆分** |
| B（剩 1–3） | 路线 B 剩余新文件 | |

## 5. 陷阱清单（**T2 本会话被实测强化，务必先读**）

### 🔴 T1：`ctest` **不会**自动重新构建

改了 `src/**` / `include/**` 后只重建被点名的 target 就跑 ctest ⇒ **假绿**（第 1 会话真实踩过）。
⇒ ⇒ 跑 ctest 前 MUST `cmake --build build -j$(nproc)`（已写入 AGENTS.md COMMANDS 段）。
⚠️ 本会话补充：**变异测试**只需重建被观测的 target（`libcpphdl.a` 是**静态库** ⇒
替换归档不影响已在跑的其他进程）⇒ 但**最终验收**仍须全量重建。

### 🔴 T2：靠「`tick()` 后读」**无法**区分 0-cycle 与 1-cycle

本会话把机制彻底查清（不是猜）：
- 地址是**输入端口**时，TB 在 `tick()` **之前**就写入 `data_map_` ⇒ 沿上采样到的是
  **新**地址 ⇒ 「0-cycle 读新地址」与「1-cycle 沿上采样新地址」**观测值相同**；
- 「tick 之间观测」也救不了：`get_port_value` 在两后端**都只读 `data_map_`**
  （`include/simulator.h:113-140`），而 `set_input_value` 不触发组合求值 ⇒ 两侧都读到
  **上一拍**的值。

⇒ ⇒ **唯一可判别的形态 = 地址由「同一条沿提交的 `ch_reg`」驱动**（沿前值 ≠ 沿后值）。
⇒ ⇒ 看到「时序」类用例先问：**它有没有同沿提交的寄存器？** 没有 ⇒ 这条断言**不可能**
判别时序 ⇒ 如实登记为「数据通路断言」，MUST NOT 当作时序证据（更不要为它造一个
「非时序 pair」来充数 —— 本会话初稿干过，变异实测**没有任何变异能抓它**，已删除）。

### 🔴 T3：`DUAL_BACKEND_TEST_PAIR` 的 body 参数里**花括号内的逗号**会被当参数分隔

用例表 MUST 放**文件作用域**（只有圆括号保护）。

### 既有陷阱（仍有效）

- **子组件 MUST 用 `ch::ch_module<T>`**：裸 `PipelineStage stage(this, "x")` 的
  `describe()` **不会被框架调用** ⇒ `ch_reg` 从未创建 ⇒ **SIGSEGV**。
- **端口连线 MUST 用 `<<=`**（`operator=` 是 deleted；`io().x = value` 对输出端口是
  **重绑定**，容易写出「看起来连上了」的假连接）。
- **`ch_in<ch_bool>` 不能绑 `const ch_bool&`** ⇒ 用 `port<ch_uint<1>> == ch_uint<1>(1_d)`。
- **变异 MUST 先确认仍可编译**（否则跑旧二进制 = 假绿）；**还原用 `cp`**（刷新 mtime）。
- **注入器 MUST fail-closed**：模式必须**唯一命中**，否则 `MUTATE_FAIL` + 非零退出
  （`/tmp/opencode/mutate_mem_comb.py` 可作模板）。
- **无效变异要如实登记**（第 1 会话登记了 2 条：`<=`→`=`、pipeline 的 rst）。
- **N2′ 判据**：断言数(with verilator) == **2 ×** 断言数(interpreter-only)
  （`./build/tests/<t> "*\[interpreter\]"` 可直接取单后端数）。
- **AGENTS.md 的 hunk 级隔离**：并发会话也在改它 ⇒ 先用 `git status` 确认它是否 dirty；
  若 clean 则直接编辑并在提交前**逐 hunk 复核** `git diff --cached AGENTS.md`
  （本会话就是 clean ⇒ 直接编辑 + 提交前复核）。

## 6. 关键文件索引

| 路径 | 说明 |
|---|---|
| `tests/dual_backend_fixture.h` | `DUAL_BACKEND_TEST_PAIR` 宏 + R1..R6 |
| `tests/test_mem_comb_read_dual.cpp` | **本会话新增**（mem 写口 × aread，2 pair） |
| `tests/test_mem_connection_dual.cpp` | 第 1 会话新增（4 连接形态） |
| `tests/test_mem_read_timing_dual.cpp` | ⚠️ **已覆盖 aread/sread 时序**（别重复转换） |
| `tests/test_mem_sread_disable_dual.cpp` | 禁读拍清零 |
| `tests/chlib/test_memory_dual.cpp` | `chlib::memory` RAM 族写拍 = 0 |
| `.rddf/roadmap/objectives/objective-test-coverage-100pct.md` | §2 判据 + 台账（现状 **49** + 不自洽登记） |
| `.rddf/plans/dual-backend-full-coverage-2026-11.md` | 批次清单 + §5 墙钟账本 |
| `/tmp/opencode/mutate_mem_comb.py` | fail-closed 变异注入器模板（**会话级，不在仓内**） |
