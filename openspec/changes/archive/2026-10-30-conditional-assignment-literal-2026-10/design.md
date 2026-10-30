# Design: `conditional_assign` 接受裸 literal

## 1. 根因（实测）

`include/chlib/if_stmt.h` 的 `conditional_assignment::assign()`：

```cpp
if (!blk) {                       // 不在任何条件块内 ⇒ 直写
    if (reg_) {
        (*reg_)->next = value;    // ✅ 这条路 OK（内部走 to_operand）
    } else {
        (*out_) <<= value;        // ❌ 这条路对 literal 编译不过
    }
    return;
}
```

⇒ `(*out_) <<= value` ⇒ `operator<<=` ⇒ `get_lnode(driver)`
⇒ ⚠️ `ch_literal_impl` **不是** `HardwareType`（它**无** `impl()`）
⇒ ⇒ **没有** `get_lnode` 重载 ⇒ ⇒ **编译错误**。

⚠️⚠️ **关键放大机制**：`if` / `else` **两个分支都是模板代码** ⇒ ⇒ **两条路
都会被实例化** ⇒ ⇒ 即使运行期 `reg_` 非空（走不到 else）⇒ ⇒ else 那行
**仍会编译** ⇒ ⇒ 一旦有 literal 实参 ⇒ ⇒ **整个调用点编译失败**。

⇒ ⇒ **「能不能编译」与「走哪条分支」无关** ⇒ ⇒ 这是**编译期**缺陷。

## 2. 修法

```cpp
(*out_) <<= ch_uint<ch_width_v<T>>(to_operand(value).impl());
```

⇒ `to_operand`（`include/core/operators.h:60`）对**四类**都有分支：
hardware（`get_lnode`）· port（`value_type` 归一化）· `ArithmeticLiteral`
· **`CHLiteral`**（→ `lnode<ch_uint<W>>`）⇒ ⇒ 全部可用 ⇒ ⇒ **零** `core/` 改动。

⚠️ **为什么不用「给 `ch_literal_impl` 加 `get_lnode` 重载」**：那会给框架**新增**
一条「literal → lnode」路径 ⇒ ⇒ 与 `to_operand` 的既有路径**可能分叉**
（宽度、节点构造方式都不同）⇒ ⇒ 归一化到 `to_operand` 更安全。

## 3. 判据设计

| 判据 | 内容 | 防什么 |
|---|---|---|
| **R0** | 两个 fixture MUST **存在且非空**（**fail-closed**） | ⚠️ **实测踩中的护栏盲区**：文件不存在 ⇒ 编译器报「No such file」⇒ ⇒ 被 R1 误判为「编译失败」⇒ ⇒ **假绿**；且 R2 也判「对照正确」⇒ ⇒ **两条判据同时假绿** |
| **R1** | `pos_literal.cpp` MUST **编译通过**（**GCC + Clang 各一次**） | 接受面回退 |
| **R2** | `neg_control.cpp` MUST **编译失败**且错误指向本文件 | R1 恒真（无判别力） |
| **R3** | 源码级：**两条**路径都经 `to_operand` 归一化 | 「只修一半」被掩盖（因 else 分支总被实例化 ⇒ 只修 `ch_reg` 路径时 `ch_out` 路径的编译错误**仍然出现** ⇒ 但若把 `ch_out` 路径的 literal 用例删掉 ⇒ 就看不出来） |

## 4. 变异

| # | 注入 | 结果 |
|---|---|---|
| **M1** | 撤掉 `to_operand` 归一化（恢复 `(*out_) <<= value;`） | ✅ 护栏 **R1 + R3 转红** + **编译失败**（2 errors）⇒ 证明修复点确实是它。⚠️ **预期结果就是编译失败** ⇒ 「先确认编译成功」这条纪律**不适用**于编译期判据 ⇒ 如实登记 |
| **M2** | 吞掉块外直写（`(*reg_)->next = value;` ⇒ `(void)value;`） | ✅ **编译成功**（0 error）⇒ 运行期用例**转红**（`0x5A` ⇒ `0`）⇒ 运行期判别力成立 |
| **M3** | 删除 `neg_control.cpp` | ✅ **R0 转红**（fail-closed 生效）⇒ 证明 R0 不是摆设 |

## 5. 改动面

| 文件 | 改什么 |
|---|---|
| `include/chlib/if_stmt.h` | **1 行** + 注释（else 分支归一化）· 头注释示例加注 |
| `tests/chlib/test_if_stmt.cpp` | 2 个 DUT + 2 个用例（**既有** target ⇒ 计数锚点**不变**） |
| `tests/fixtures/conditional_assign_literal/` | `pos_literal.cpp`（4 种组合）· `neg_control.cpp`（对照） |
| `scripts/verify_conditional_assign_integrity.sh` | 新护栏（R0–R3 + `--self-test`） |

## 6. 非目标

- **不**处理「块外对**同一 target** 的**裸**直写会覆盖条件合成」⇒ **独立** change
  （**预防性**护栏 ⇒ 零受害者 ⇒ 优先级低于本项）；
- **不**给 `ch_literal_impl` 加 `get_lnode` 重载；
- **不**改 `core/`（**零**改动）。
