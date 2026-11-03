# Tasks：chlib 仲裁器族的两处「建图期求值」缺陷修复（ADR-065 实施）

## T1 —— A-3：`stream_arbiter_lock`（`include/chlib/stream_arbiter.h`）

- [x] 选择逻辑由**普通 C++ `if/else`** 改成**硬件 `select` 链**
      （`select(locked_now, locked_idx, select(has_valid, prio_sel, locked_idx))`）
- [x] 锁存更新走 `locked_channel->next`（**MUST NOT** 对 `ch_reg` 的 C++ 赋值）
- [x] **新增 `locked` 寄存器**（`ch_reg<ch_bool>`）：授予时置位、`output.fire` 时释放
      ⇒ 头注释承诺的「stays selected until the transaction completes」**首次兑现**
- [x] `out.valid` 改成**选中通道的 valid**（`bit_select(valid_vector, selected_idx)`），
      **MUST NOT** 是「任一通道 valid」
- [x] 读 `ch_reg` MUST 用**隐式转换赋值**（`ch_bool locked_now = locked;`）
      ⇒ **MUST NOT** 写 `ch_bool(locked)`（实测：那条路径让锁存恒读 0）

## T2 —— A-4：`round_robin_arbiter`（`include/chlib/selector_arbiter.h`）

- [x] `UpdatePriority::process` 删掉外层 `if (result.valid)`（建图期恒假 ⇒
      整个 if 体含递归**从不执行**）⇒ select 链**无条件**执行
- [x] `if constexpr` **MUST 保留**（编译期展开，合法且必需）
- [x] 「被授予位」判定改用 `bit_select(result.grant, pos)`
      ⇒ **MUST NOT** 用 `grant == (ch_uint<N>(1_d) << pos)`
      （后者生成 N+(N-1) 位节点 ⇒ N=64 ⇒ 127 位 ⇒ interpreter 的 `eq` 在
      「64 位 vs 127 位」且**移位量 ≥ 2** 时**判错** ⇒ 64 位下 `next_ptr` 恒 0）

## T3 —— 既有测试更新（ADR-065 D5）

- [x] `tests/chlib/test_selector_arbiter.cpp`：**8 处期望值转红**
      ⇒ 逐条核对 ⇒ **全部**是「钉住了缺陷」（指针恒 0 的旧语义）
      ⇒ 按**正确语义 + 手推读模型**更新，并补 `next_ptr` 断言
      （84 → **120** 断言）
      ⇒ ⚠️ 读模型：`tick()` = 沿前组合 → 寄存器提交 → **沿后**组合
      ⇒ ⇒ `get_value` 在 `tick()` 后读到的是**指针已前进之后**的组合值
- [x] `tests/test_stream_arbiter.cpp`：2 个 lock 用例补**行为断言**
      （`selected` + `payload` + `ready`；10 → **17** 断言）

## T4 —— 变异验证（ADR-065 §5.3；每条**先确认编译成功** ⇒ 否则跑的是旧二进制 ⇒ 假绿）

| 变异 | 实测结果 |
|---|---|
| M1 A-3 选择逻辑恢复 C++ `if/else` | 🔴 **4 failed** |
| M2 A-3 `selected` 忽略 `locked`（恒取仲裁结果） | 🔴 **6 failed** |
| M3 A-3 `locked_channel->next` 不推进 | 🔴 **4 failed** |
| M4 A-3 `out.valid` 改回「任一通道 valid」 | 🔴 **2 failed** |
| M5 A-4 恢复原始 `if (result.valid) { ... }` | 🔴 **6 failed** |
| M6 A-4 `next_pos` 不前进（`= pos`） | 🔴 **6 failed** |
| M6b A-4 迭代顺序反转（链序） | ⚪ **恒绿 = 等价变异** ⇒ 如实登记 |

⚠️ **M6b 为何是等价变异**：`select` 链里**只有被授予位**的 cond 为真（`grant` 是
one-hot）⇒ ⇒ **链序不影响结果** ⇒ ⇒ 「链序反转」**不是**判别点
（⇒ ⇒ 想判别链序，MUST 用**多个 cond 同时为真**的形态，而本组件不存在这种形态）。

### ⚠️ 本轮实测的两条变异纪律教训

1. **还原「原始缺陷形态」时 MUST 用原始写法** —— M5 首版写成
   `if (!result.valid) return;`，而 `!` 是**硬件算子**（产出节点）⇒
   `if (节点)` 在**建图期恒假** ⇒ ⇒ **永远不 return** ⇒ 该变异**恒绿**（等价）
   ⇒ ⇒ 改回原始的 `if (result.valid) { ... }`（正向守卫 + 整体包裹）才转红
   ⇒ ⇒ **「看起来等价」的守卫改写会制造假变异**；
2. 变异注入器的 `text=True` 会因 Verilator 的**非 UTF-8 输出**抛
   `UnicodeDecodeError` ⇒ MUST 用 `errors="replace"`（否则脚本自身崩，
   变异状态不明，**不得**据此声称「抓到」或「没抓到」）。

## T5 —— 判据（D6）

- [x] `tests/chlib/test_stream_arbiter_lock_dual.cpp`（**新**，
      **7** 个 `DUAL_BACKEND_TEST_PAIR` / **184 == 2 × 92** 断言）
      ⇒ lock：选择 + 数据不串 · 锁存保持 · 释放 · valid 门控
      ⇒ rr：全 1 轮转 · 偏请求无饿死 · 无请求保持
- [x] N2′ 成立（interpreter 半边 **92** 断言 × 2 = **184**）

## T6 —— 契约与验收

- [x] capability `chlib-stream-arbitration` **ADDED ×1**（1 Requirement + 5 Scenario）
- [x] `verify_openspec_delta.sh` 4 条全绿 · `openspec validate --strict` 通过
- [x] 全量重建 0 error · `ctest -LE perf` **229/229** 全绿
- [x] 计数锚点同步（parity 源文件数 62 → **63**）
- [x] 命名诚实性登记表 **+2**（`stream_arbiter_lock` / `round_robin_arbiter`）
- [x] 归档**未**用 `--skip-specs`

## 不做（独立立项）

- [ ] **框架层缺陷**：interpreter 的 `eq` 在「64 位 vs 127 位」且**移位量 ≥ 2** 时
      **判错**（独立最小复现：`a=0x4`、`shl=0x4` ⇒ `eq` 返回 0）
      ⇒ 立项 `.rddf/improvements/interpreter-eq-wide-operand-miscompare-2026-11.md`
      ⇒ 本 change **只绕开**（改用 `bit_select`），**不**修框架
- [ ] `priority_encoder` 注释「优先处理低位」与实现（高位优先）相反 ⇒ 独立议题
