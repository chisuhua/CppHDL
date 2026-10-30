# Design: `multi_if` 的输出可赋值性（E1）与条件可接受性（E3）

## 1. E1 的修法与**关键机制**（ADL）

`include/core/io.h:112-115`：

```cpp
template <typename U> void operator=(const U &value) {
    lnode<U> src_lnode = get_lnode(value);   // ← **非限定**调用
    ...
}
```

⇒ ⇒ `get_lnode(value)` 是**非限定**调用 ⇒ ⇒ 候选集 =
「定义点的普通查找」+「**实例化点的 ADL**」⇒ ⇒ `multi_if` 在 `chlib`
⇒ ⇒ **ADL 会找到 `chlib::get_lnode`** ⇒ ⇒ **`core/` 零改动** ✔

⚠️⚠️ **踩过的坑**：把 `get_lnode(multi_if)` 写进 `namespace ch::core` ⇒ ⇒
**ADL 找不到**（关联命名空间是 `chlib`）⇒ ⇒ 修复**失效** ⇒ ⇒ 必须写在
**`namespace chlib`** 内 ⇒ ⇒ 而 `ch_width_impl` 的特化**必须**写在
**`namespace ch::core`** ⇒ ⇒ 两者**命名空间不同** ⇒ ⇒ 所以都放在
`if.h` 的**末尾**（`} // namespace chlib` **之后**）⇒ ⇒ 两个独立的
namespace 块。

⚠️ **形参 MUST 是具体类型**（`const multi_if<T> &`）⇒ ⇒ 它是**类型专用**入口
⇒ ⇒ **不**违反 ADR-053（「MUST NOT 有**无约束的通用**入口」）⇒ ⇒ ADR-053 的
源码级扫描器**不**会报它（扫描器判「通用」= 形参是**单个标识符**）。

⚠️ **为什么需要 `ch_width_impl` 特化**：`lnode<T>` 的 `ch_width_impl`
（`include/core/lnode.h:38`）用 `ch_width_v<T>` ⇒ ⇒ `multi_if<T>` 必须有宽度
⇒ ⇒ 结果类型宽度 = `ch_width_v<T>`（与 `multi_if` 的结果类型一致）。

⚠️ **语义**：`get_lnode(multi_if)` MUST 触发 `multi_if` 的**合成**
（`operator T()`）⇒ ⇒ 返回的节点就是**合成后的值** ⇒ ⇒ 与
`ch_uint<8> v = if_then(...); io().out = v;` **逐位等价**（运行期断言）。

## 2. E3 的修法

条件形参 `const ch_bool &` ⇒ ⇒ `ch_in<ch_bool>` **不能**绑 ⇒ ⇒ 改为
`const C &condition` + 经 `to_operand` 归一化：

```cpp
ch_bool cond(to_operand(condition).impl());
```

⚠️ `to_operand` 的 port 分支返回 `lnode<value_type>(value.impl())`
⇒ ⇒ 对 `ch_in<ch_bool>` 得到 `lnode<ch_bool>` ⇒ ⇒ `.impl()` 即其节点 ✔

⚠️ **归一化不新建节点**：`to_operand` 对硬件类型走 `get_lnode`（取现有节点）
⇒ ⇒ 零额外节点 ⇒ ⇒ 既有行为**逐位不变** ✔

⚠️ **实测**：顶层 `ch_in<ch_bool>` 在 `describe()` 内作条件**可用**
（`test_if_stmt.cpp` 的既有 DUT 就这么用 ⇒ ⇒ 该用法**已被验证**）。

## 3. 判据设计

| 判据 | 内容 | 防什么 |
|---|---|---|
| **R0** | 两个 fixture MUST **存在且非空**（**fail-closed**） | ⚠️ 同 `verify_conditional_assign_integrity.sh`：文件不存在 ⇒ 编译器报「No such file」⇒ ⇒ 被 R1 误判为「编译失败」（**假绿**）⇒ ⇒ 且 R2 也判「对照正确」⇒ ⇒ **两条判据同时假绿** |
| **R1** | `pos_output_assignability.cpp` MUST **编译通过**（**GCC + Clang 各一次**） | E1 / E3 回退 |
| **R2** | `neg_control.cpp` MUST **编译失败**且错误指向本文件 | R1 恒真（无判别力） |
| **R3** | 源码级：① `get_lnode(const multi_if<T> &)` ② `ch_width_impl<::chlib::multi_if<T>, void>` ③ `if_then(const C &, const U &)` ④ `ch_bool cond(to_operand(condition).impl());` **且** 条件 MUST **不**退回 `const ch_bool &` | 「只实现了一条」/ 「改回旧形」 |

⚠️ **R1 与 R3 不可互相替代**（实测）：把 fixture 里的「直接赋 `ch_out`」改成
「先转 `ch_uint` 再赋」⇒ ⇒ **编译仍通过** ⇒ ⇒ R1 **抓不到** ⇒ ⇒ 必须靠 R3。

## 4. 变异

| # | 注入 | 结果 |
|---|---|---|
| **M1** | 删掉 `get_lnode(const multi_if<T>&)`（改名） | ✅ 护栏 **R1 + R3 转红**（exit 1） |
| **M2** | 条件改回 `const ch_bool &` | ✅ 护栏 **R1 + R3 转红**（exit 1） |

⚠️ 两条变异的**预期结果都是编译失败** ⇒ ⇒ 「先确认编译成功」这条纪律
**不适用**于**编译期**判据 ⇒ ⇒ 如实登记。

## 5. 改动面

| 文件 | 改什么 |
|---|---|
| `include/chlib/if.h` | `if_then` 的条件模板化 + 末尾的 `get_lnode` / `ch_width_impl` 特化 |
| `tests/chlib/test_multi_if_acceptance.cpp` | 1 个 DUT + 1 个用例（**既有** target ⇒ 计数锚点**不变**） |
| `tests/fixtures/multi_if_output/` | `pos_output_assignability.cpp`（4 种组合）· `neg_control.cpp`（对照） |
| `scripts/verify_multi_if_output_assignability.sh` | 新护栏（R0–R3 + `--self-test`）+ CI 两处 |

## 6. 非目标

- **不**改 `multi_if` 的宽度规则（ADR-052 已落地）；
- **不**改 `core/`（**零**改动）；
- **不**处理「块外裸直写覆盖条件合成」（N7 ⇒ 独立 change）。
