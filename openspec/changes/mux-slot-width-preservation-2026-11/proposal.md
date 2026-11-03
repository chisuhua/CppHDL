# 提案：`mux` 槽位宽度 MUST 保持（修 `instr_mux` 的拷贝赋值 + 补判据）

- **关联 capability**：`dual-backend-test-coverage`（**ADDED ×1**）⇒ 归档**禁** `--skip-specs`
- **行为变更**：**有**（interpreter 侧 `mux` 的输出宽度语义 ⇒ 向 Verilog 对齐）
- **立项来源**：`.rddf/improvements/cross-dut-bit-select-out-of-range-2026-11.md`

## Why

### 现象

`tests/chlib/test_stream_arbiter_dual.cpp` **完整运行**时 stderr 出现 **6 条**：

```
[instr_op_bit_sel] Error: Bit index out of range!   （index=1 或 2，operand_width=1，src1_width=32）
```

⇒ ⇒ 单独跑任一个 DUT ⇒ **0 条** ⇒ ⇒ 曾登记为「**跨 DUT 状态污染**（根因未定位）」。

### 根因（2026-11-03 实测定位）

⇒ ⇒ **不是**跨 DUT 污染。真因在 `include/ast/instr_mux.h`：

```cpp
if (!cond_->is_zero()) *dst_ = *true_val_;    // ← 拷贝赋值 ⇒ bitvector::operator= **RESIZE**
else                   *dst_ = *false_val_;
```

⇒ ⇒ `mux` 节点宽度 = `max(真分支宽, 假分支宽)`（`build_mux`）⇒ ⇒
**较窄的分支被选中**时整个槽位被**压窄** ⇒ ⇒ 下游按声明宽度取位 ⇒ ⇒ **越界**。

### 为什么「只在多 DUT 时」+「只在 interpreter 侧」

| 观察 | 解释 |
|---|---|
| 只在 interpreter | Verilog 的 `assign d = c ? t : f;` **天然保持声明宽度** ⇒ ⇒ **两后端分歧** |
| 只在多 DUT | node id 是**进程级全局唯一**（`g_next_node_id`）⇒ ⇒ id 分布变 ⇒ ⇒
  拓扑序 tie-break 变 ⇒ ⇒ `mux` 与消费它的 `bit_select` **谁先求值**变 ⇒ ⇒ 时好时坏 |
| 断言未受影响 | 越界分支写 0，而**窄分支的高位本来就是 0** ⇒ ⇒ **值逐位等价** ⇒ ⇒ 良性但产生噪声 |

⚠️ **定位手段**（全部实测，非推断）：① 复现矩阵 ⇒ 确认触发条件；
② 插桩打印 `node_id` / 宽度 ⇒ 发现 `src1_width=32`（**编译期索引**变体）；
③ 打印节点身份 ⇒ 发现**图里是正确的 3 位**（`src0=(367,'top.mux_select',w3)`）；
④ 检测「槽位宽度被 eval 改变」⇒ ⇒ `[DBG-WIDTH-CHANGE] node 367 'top.mux_select' 3 -> 1`。

## What Changes

| 文件 | 改动 |
|---|---|
| `include/ast/instr_mux.h` | `*dst_ = *分支` ⇒ `dst_->assign_truncate(*分支)`（**2 处**） |
| `tests/test_mux_width_dual.cpp`（**新**） | **1** 个 `DUAL_BACKEND_TEST_PAIR` / **42 == 2 × 21** 断言 |
| `tests/CMakeLists.txt` | 注册 + `set_tests_properties(... FAIL_REGULAR_EXPRESSION "Bit index out of range")` |
| `openspec/specs/dual-backend-test-coverage/spec.md` | ADDED ×1（1 Requirement + 3 Scenario） |

## 为什么这是「修缺陷」而不是「改语义」

⇒ `node->size()` 是**权威**（`build_mux` 算出的 `max(分支宽)`）⇒ ⇒
`eval` **不该改变**它。修后 `mux` 输出 = 选中分支的值**零扩展**到声明宽度
⇒ ⇒ **与 Verilog 的 `assign` 逐位一致** ⇒ ⇒ 消除两后端分歧。

⇒ ⇒ 同一惯例在仓内**已有三处**：`instr_output::eval`（其注释早已把同一个坑写明）·
`instr_proxy::eval` · `instr_reg::eval`（+ JIT 的 `STORE_DATA` mask）
⇒ ⇒ `instr_mux` 是**漏网的第四处**。

## Verification

- [x] 越界日志 **6 → 0**（`test_stream_arbiter_dual`）
- [x] 全量重建 **0 error** · `ctest -LE perf` **231/231** 全绿（零回归）
- [x] N2′：`42 == 2 × 21`
- [x] 变异：注入 M1（改回拷贝赋值）⇒ ⇒ `FAIL_REGULAR_EXPRESSION` 判红；还原 ⇒ 绿
- [x] ⚠️ **如实登记**：M1 在**值层面不可判别**（压窄只丢已为 0 的高位）
      ⇒ ⇒ 唯一可观测量 = 越界日志 ⇒ ⇒ 判据 MUST 含 `FAIL_REGULAR_EXPRESSION`
