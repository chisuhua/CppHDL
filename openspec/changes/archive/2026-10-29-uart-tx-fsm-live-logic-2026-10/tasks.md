# Tasks: uart-tx-fsm-live-logic-2026-10

## 0. 准备（**承重约束核实**）
- [x] 0.1 `reg->next` 的多次赋值语义 —— 实测 `regimpl::set_next` = `set_src(0, next)`
      （`include/ast/ast_nodes.h:47-54`）⇒ **last-wins（覆盖）**
      ⇒ ⇒ **每个寄存器 MUST 只有一个写入点** ⇒ 各状态候选值 MUST 用 `select` 组合
      ⚠️ **连带发现**：`tests/test_verilator_complex_e2e.cpp` 的 SPI fixture
      **自身也有**这个问题（`shift->next` 在 IDLE `:134` + TRANSFER `:153` 两处
      ⇒ TRANSFER 覆盖 IDLE ⇒ 「IDLE 载入 data_in」永不生效）⇒ ⇒ 上一 commit
      称它为「正确形态范例」**不准确** ⇒ 已订正（design.md §5）
- [x] 0.2 `select` 的可用性 —— 实测 `include/core/operators.h:147`
      `select(const Cond&, const T&, const U&)` ⇒ 硬件 mux（条件**不**经过 C++
      bool 上下文）⇒ 安全
- [x] 0.3 `ch_bool` 的 `&` / `|` 可用 —— 实测 `include/core/operators_logic.h:104/109`
      （泛型 LHS/RHS）⇒ ADR-036 已把 `&&`/`||` 迁移为 `&`/`|` ✓
- [x] 0.4 `sm.is_in(S)` 的可用性 —— 读 `state_reg`（构造时即存在）⇒ 可在
      `build()` **之后**用于组合逻辑 ✓（实测：生成的 Verilog 里 `eq_eq_4..7`
      **有消费者** ⇒ 证明确实被用上了）
- [x] 0.5 `transition_when(cond, target)` 是**唯一**正确的条件转移 API
      （`include/chlib/state_machine.h:228-232`）
- [x] 0.6 ⚠️ **`port<ch_bool>` 无 `ch_bool` 转换** —— 实测（探针
      `/tmp/opencode/probe_conv.cpp`）：`ch_bool(port)` ❌ / `ch_uint<1>(port)` ❌ /
      `ch_reg<ch_bool>` 中转 ✅ ⇒ ⇒ `frame_start` MUST 经寄存器中转
      ⇒ 代价：**条件晚 1 拍**（UART 不在意；自检以 `busy` 上升沿为锚点 ⇒ 无关）

## 1. 实现（`examples/spinalhdl-ported/uart/uart_tx.cpp`）
- [x] 1.1 `on_active` 里**只**写 `transition_when`（**不**碰任何 `reg->next`）
- [x] 1.2 `sm.build()` **之后**写 3 个寄存器的**唯一** next 写入点
      （`baud_counter` / `bit_counter` / `shift_reg`，各一条 `select` 组合表达式）
- [x] 1.3 `sm.build()` **之后**写 2 个输出的**唯一**驱动点
      （`uart_tx` / `busy`，按 `is_in()` 多路选择 ⇒ **只驱动一次**）
- [x] 1.4 数据位改发 `(shift_reg >> 1) & 1`（= `bit1..bit8` ⇒ 完整 8 位）
      ⇒ 顺带修正原实现的 **off-by-one**（少发 `data[7]`；对 `data[7]=0` 碰巧正确）
- [x] 1.5 **未**用三元 `? :`（同坑形态 ③）
- [x] 1.6 编译 0 error / 0 新增 warning（`logger.h:194` 是**既有噪声**：
      强制重编**未改动**的 `test_module` 同样出现 3 处）

## 2. 自检断言（**判别力是承重件**）
- [x] 2.1 以 `busy` 上升沿 = 帧起点；每 **434** 拍采样一次 `uart_tx`
- [x] 2.2 期望序列（**手推**）：`[0, 1,0,1,0,0,1,0,1, 1]`
      ⇒ 数据 MUST 用 **`0xA5`**（`10100101` ⇒ `bit7=1` ⇒ LSB-first = `1,0,1,0,0,1,0,1`）
      ⇒ ⚠️ **不可**用 `0x55`（`bit7=0` ⇒ 与起始位相同 ⇒ 抓不到 off-by-one）
      ⇒ **已被 M1 变异实证**（用 `0xA5` 时 5 个位错位 ⇒ 转红）
- [x] 2.3 不匹配 ⇒ `return 1`（⇒ `run_all_ported_tests.sh` 只查退出码 ⇒ 可被抓到）
      ⇒ ⚠️ **实测**：加断言后**第一次运行就是红的**（采样漏了 rel=0 的起始位
      ⇒ 只采到 9 位 ⇒ `[FAIL] 采样位数 9 ≠ 期望 10`）⇒ ⇒ 这正是它应有的行为
      （此前它对任何行为退化都**恒 PASS**）⇒ 已修（起始位在 `t0` 那拍立即采样）
- [x] 2.4 帧结束后 MUST 回到 IDLE（`busy=0` 且 `uart_tx=1`）⇒ 断言已加

## 3. 变异验证（**每条先确认编译成功**，否则跑旧二进制 ⇒ 假绿）
- [x] 3.0 ⚠️ **注入器 fail-closed** —— 变异用**行号**注入
      （`/tmp/opencode/mutate.py`）：行号越界 / 目标子串不匹配 ⇒ `MUTATE_FAIL`
      且 **exit 2** ⇒ 杜绝「注入失败却跑旧代码 ⇒ 假绿」
      ⚠️ **留痕**：首次尝试 M2 时用字符串替换**失败**（旧串不匹配）⇒ 跑的是
      M1 的残留 ⇒ **已修正为行号注入**
- [x] 3.1 M1 恢复 off-by-one（数据位发 `shift_reg & 1`）⇒ ✅ **转红**（5 个位错）
- [x] 3.2 M2 数据位序反（发 `bit8`）⇒ ✅ **转红**（第 3/6/8 位错）
- [x] 3.3 M3 起始位改成 `1` ⇒ ✅ **转红**（第 0 位错）
- [x] 3.4 M4 停止位改成 `0` ⇒ ✅ **转红**（第 9 位错 + 帧后未回 IDLE）
- [x] 3.5 M5 波特分频改 `433` ⇒ ✅ **转红**（第 1/2/3 位错）
- [x] 3.6 M6 DATA 右移改 2 位 ⇒ ✅ **转红**（第 2/3/6 位错）
      ⇒ ⇒ **7/7 全被抓**（含审稿后补的 M7），每条编译错误数 = **0**
- [x] 3.7 M7（审稿新增）去掉 DATA→STOP 的 `& baud_done` 门控 ⇒ ✅ **转红**
      （`busy` 高电平 3473 ≠ 3906）⇒ ⇒ **7/7 全被抓**

## 3.5 对抗审稿（第 1 轮 = **REJECT** ⇒ 返工闭合）
- [x] 3.5.1 **BLOCKER**：DATA→STOP 的转移条件缺 `baud_done` 门控 ⇒ `transition_when`
      的条件是**电平敏感**（每拍求值、为真即走）⇒ `bit_last` 一变 7 就持续为真
      ⇒ **第 8 个数据位只发 1 拍**（而非 434 拍）⇒ 停止位提前 433 拍
      ⇒ ✅ 已修：`transition_when(bit_last & baud_done, STOP)`（= 纪律 ④）
- [x] 3.5.2 ⚠️ **BLOCKER 的检出依赖新断言** ⇒ 补「**`busy` 高电平总拍数 == 3906**」
      （手推 = 9 位 × 434）⇒ ⚠️ **采样网格对「位段长度」零判别力**（每位样本落在
      位段**末尾** ⇒ 位段被截短时采样值仍可能正确）⇒ 实测 M7 变异的 10 个采样值
      **全对** ⇒ ⇒ 必须有拍数断言
- [x] 3.5.3 ⚠️ **根因是流程缺陷**：草案 §2.2 的两条契约（「每位持续 434 拍」
      「busy 期间为 1」）在实现时**被静默丢弃**（未落进断言表 + 无丢弃记录）
      ⇒ ⇒ 已在 design.md §4.3 **诚实登记**，并把其中可执行的一条落成断言
- [x] 3.5.4 **SHOULD_FIX #1**：纪律清单不完备 ⇒ ✅ 补**纪律 ④**（`transition_when`
      的**电平敏感性**）+ 纪律 ① 的**完整枚举**（`switch` / `while`-`for` 行程条件 /
      early `return` / 传给 `bool` 形参）+ 纪律 ① 的**边界**（条件 MUST 是硬件节点，
      C++ bool 语境会退化为常量）
- [x] 3.5.5 **SHOULD_FIX #2**：`uart_tx.cpp` 里「最低位补 1（为停止位准备）」的
      注释**不准确**（停止位来自输出 `select` 的常量分支，不经过 `shift_reg`）
      ⇒ ✅ 已改
- [x] 3.5.6 **NIT**：roadmap 新增行的路径（已归档）与状态双陈旧 ⇒ ✅ 已改
- [x] 3.5.7 **第 7 条变异 M7**（去掉 `& baud_done` 门控）⇒ ✅ 转红
      （`busy` 高电平 **3473 ≠ 3906**）⇒ 证明新断言有判别力
- [x] 3.5.8 ⚠️ **数值订正**：critic 推演给出「错误版 3493 拍」，**实测 3473**
      ⇒ 以实测为准（结论经实测确认 ✓）

## 4. 验收
- [x] 4.1 生成的 Verilog：状态寄存器**无自环**
      （`reg_3 <= mux_select_11[2:0]`）；`is_in` 比较节点**有消费者**
      （`eq_eq_4` 11 次 / `eq_eq_5` 6 / `eq_eq_6` 5 / `eq_eq_7` 5，原为**零消费者**）；
      `uart_tx`/`busy` 各**单条** assign（接 `mux_select_1`/`_2`）
- [x] 4.2 `uart_tx` **不再是常量**（实测有 0/1 两种值 + 完整帧序列）
- [x] 4.3 `./run_all_ported_tests.sh` ⇒ **28/28**
- [x] 4.4 `bash scripts/run_all_samples.sh` ⇒ **35/35**
- [x] 4.5 `ctest -LE perf` ⇒ **191/191**
- [x] 4.6 **计数锚点不变**：`CTEST_TOTAL` 202 / `EXAMPLES_TEST_SOURCES` 17 /
      `ctest -L verilator_e2e` 38 / non-perf 191
- [x] 4.7 9 个 `scripts/verify_*.sh` + 3 个 `--self-test` 全 exit 0

## 5. 文档与归档
- [x] 5.1 根 `AGENTS.md` NOTES 记录（并**订正**上一 commit 把
      `test_verilator_complex_e2e.cpp` 称为「正确形态范例」的说法 ——
      它**自身**也有 last-wins 多驱动问题 ⇒ 该文件的 SPI 语义也是错的）
- [x] 5.2 `openspec archive uart-tx-fsm-live-logic-2026-10 --yes --skip-specs`
