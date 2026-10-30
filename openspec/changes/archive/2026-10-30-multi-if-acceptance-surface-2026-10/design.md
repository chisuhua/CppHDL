# Design: multi-if-acceptance-surface-2026-10

> 实施 ADR-052。**决策在 ADR**，本文只承载**实施形态**（签名 / 判据代码 / 护栏机制 /
> fixture 清单 / 变异清单）。逐字证据见 `docs/architecture/2026-10-30-n4-n6-evidence.md`。

## 1. 实施形态（`include/chlib/if.h`）

### 1.1 为什么必须改签名（硬前置）

现 `elif` / `else_` 的形参是 `const T &`（`if.h:43,51`）⇒ ⇒ 实参在**进入函数体前**
就已完成到 `T` 的转换 ⇒ 宽分支已被 **`bits<N-1,0>` 截断** ⇒ 函数体内**无法**
恢复原始宽度 ⇒ 无法判定「更宽」。

⇒ ⇒ 判据 MUST 在**转换之前**拦截 ⇒ ⇒ `ctor` / `elif` / `else_` MUST 改成模板 `<typename U>`
+ `if constexpr` 分派。

### 1.2 三条判据（ADR-052 D1 / D2 / D4）

```cpp
// ① port 检测（与 to_operand 同款，operators.h:60-64）
template <typename T>
concept is_port_like = requires(T t) {
    typename T::value_type; typename T::direction;
};

// ② 分支值的「有效硬件类型」：port ⇒ 归一化到 value_type
template <typename U> struct branch_vt { using type = std::remove_cvref_t<U>; };
template <typename U> requires is_port_like<std::remove_cvref_t<U>>
struct branch_vt<U> { using type = typename std::remove_cvref_t<U>::value_type; };
template <typename U> using branch_vt_t = typename branch_vt<U>::type;

// ③ 归一化构造（ch_bool 端口无 operator ch_bool() ⇒ 经 lnode 兜底，证据 §6.5）
template <typename VT, typename U> VT normalize_branch(const U &v) {
    if constexpr (std::is_constructible_v<VT, const U &>) return VT(v);
    else if constexpr (requires { typename U::value_type; })
        return VT(lnode<typename U::value_type>(v).impl());
    else return VT(v);
}
```

`ctor` / `elif` / `else_` 的分派（以 `elif` 为例）：

```cpp
template <typename U>
multi_if &elif(const ch_bool &cond, const U &value) {
    using UU = std::remove_cvref_t<U>;
    if (has_else_) throw std::runtime_error("Cannot add elif after else");
    if constexpr (HardwareType<UU> || is_port_like<UU>) {
        using VT = branch_vt_t<UU>;
        static_assert(ch_width_v<VT> <= ch_width_v<T>,
            "multi_if: branch wider than result width; use "
            "if_then<ch_uint<W>>(...) to declare a wider result explicitly");
        branches_.push_back({cond, normalize_branch<T>(value)});   // 窄者 zext
    } else if constexpr (std::is_integral_v<UU>) {
        // 裸整数字面量：**豁免**类型宽度判据（ch_width_v<int>=32 是占位语义）
        // ⇒ 建图期值域检查（值 MUST 可容纳于 width(T)）
        check_int_literal_fits<T>(value, "multi_if");
        branches_.push_back({cond, T(value)});
    } else {
        static_assert(!sizeof(UU *),
            "multi_if: branch must be a hardware type or an integral literal; "
            "use if_then<ch_uint<N>>(cond, value) and cast the branch explicitly");
    }
    return *this;
}
```

⚠️ **`ch_literal_impl<V,W>` 走第一条分支**（它是 `HardwareType`）⇒ 自动获得**静态**
`ch_width_v<W> <= ch_width_v<T>` 判据（ADR-052 D2 的 sized 语义）✔

### 1.3 单分支拒绝（ADR-052 D3）

`operator T()` 入口处：

```cpp
operator T() const {
    if (branches_.size() < 2) {
        CHFATAL_EXCEPTION(
            "multi_if: a single branch cannot be a conditional expression — its "
            "condition can never participate (no mux is built); add .else_(v) or "
            "another .elif(...).");
    }
    ...
}
```

⚠️ **MUST** 用 `CHFATAL_EXCEPTION`（**不是** `CHERROR` / `CHFATAL` —— 实测那几个
**只打日志、不抛异常** ⇒ 是假安全网，ADR-050 实施期已登记）。
⚠️ 顺带删除 `branches_.empty()` 的「Empty if block」分支 —— `ctor` 保证 ≥1 ⇒ 不可达
（Zero-Debt）。

## 2. 护栏机制（`scripts/verify_multi_if_width.sh`）

| 步骤 | 内容 |
|---|---|
| 1 | 对每个 fixture 以 `-fsyntax-only` 编译（**GCC 与 Clang 各一次**） |
| 2 | 「拒绝」fixture：断言**编译失败** + stderr 含**必需关键词**（宽度数值 / `if_then<ch_uint<` / 指引文本） |
| 3 | **中性化对照**：对每个「拒绝」fixture，用一个 `-D` 宏把对应 `static_assert` 中性化 ⇒ 断言该 fixture **编译通过** ⇒ 证明「拒」来自该断言 |
| 4 | 「正向」fixture：断言**编译通过** |
| 5 | **fail-closed**：fixture 缺失 / 意外通过 / 缺必需关键词 ⇒ `exit 1` |

⚠️ **禁止**以「本地编过一次确实报错」作验收证据（不可复算）。护栏挂 CI **两处**
（主路径 + `--self-test`）。

## 3. Fixture 清单

**拒绝（每条配中性化对照）**

| id | 形态 | 期望 |
|---|---|---|
| `R_A` | `if_then(f, a-b).elif(t, a+b)`（T=8，分支 9 位） | 编译失败 + 消息含 `8` / `9` / `if_then<ch_uint<` |
| `R_E` | `if_then<ch_uint<1>>(c, a)`（a 8 位） | 同上 |
| `R_D` | `if_then(c, 1)`（T 推导为 `int`） | ctor 拒绝 + 指引 `if_then<ch_uint<N>>` |
| `R_lit` | `if_then(c, ch_uint<8>(x)).elif(c2, 0x1FF_h)` | 编译期拒绝（sized 字面量 W=9 > 8） |
| `R_neg` | `if_then(c, a).else_(-1)` | 拒绝（负数字面量） |

**运行期（建图期拒绝）**

| id | 形态 | 期望 |
|---|---|---|
| `T_B` | 单分支 `if_then(c, x)` 转换 | `REQUIRE_THROWS_AS` |
| `T_range` | `elif(c, 256)` 进 `T=8` | `REQUIRE_THROWS_AS` |

**正向（防过度实现，ADR-052 D6 六种）**

| id | 形态 | 期望 |
|---|---|---|
| `P_wide` | `if_then<ch_uint<16>>(c, ch_uint<8>(x))` + 全 8 位分支 | 通过 + 零扩展 |
| `P_else` | `if_then(c, a).else_(b)` | 通过（值正确） |
| `P_multi_no_else` | `if_then(c,a).elif(c2,b)` | 通过（尾部 = 末分支值） |
| `P_explicit` | 每分支显式 `ch_uint<8>(...)`（`conditional_demo` 形态） | 通过 |
| `P_intlit` | `elif(c, 1)` | 通过（值域检查放行） |
| `P_port` | `if_then(c, io().a)`（`ch_in<ch_uint<8>>`） | 通过（归一化） |
| `P_port_bool` | `if_then(c, io().flag)`（`ch_in<ch_bool>`） | 通过（经 `lnode<ch_bool>` 兜底） |
| `P_bool1` | `T=ch_bool` / `T=ch_uint<1>`（1 位分支） | 通过 |

## 4. 变异清单（每条**先确认编译成功**，否则跑旧二进制 ⇒ 假绿）

| # | 注入 | 期望 |
|---|---|---|
| **M1** | 宽度 `static_assert` 的条件放宽（`<=` → 恒真） | `R_A` / `R_E` 意外通过 ⇒ 护栏红 |
| **M2** | `zext` 换成 `sext` | `P_wide` 的运行期金标：`0x0A5` → `0x1A5` ⇒ 红 |
| **M3** | 去掉整数字面量的**值域检查** | `T_range` 不再抛 ⇒ 红 |
| **M4** | 删掉 `size() < 2` 检查 | `T_B` 不再抛 ⇒ 红 |
| **M5** | 去掉 port 归一化（退回 `T` = `port<…>`） | `P_port` 退回模板深渊 ⇒ 红 |
| **M6** | 「更宽 ⇒ 拒绝」误写成「不等 ⇒ 拒绝」 | `P_wide`（16 位 T + 8 位分支）失败 ⇒ 红 |

⚠️ **如实登记无判别力的变异**：「zext 换恒等存储」**不红**（窄值存宽 `T` 天然无损）
⇒ 该维度只能靠编译期结构证据（Verdi 节点审查 / `static_assert`）。

## 5. 交付顺序

1. `if.h` 签名模板化 + 三条判据（保持既有行为，先过全量绿）；
2. 加 `static_assert` / `CHFATAL_EXCEPTION`（此时既有 `conditional_demo` 应仍绿）；
3. 新增护栏脚本 + fixture；
4. 变异验证（6 条）；
5. 契约合并（归档）。

---

## 6. 实施期实测记录（2026-10-30）

### 6.1 🔴 实施期抓到的**真缺陷**：显式模板实参绑错了参数

初版实现把判据写成 `checked_value<T>(value, where)` ⇒ ⇒ **显式实参 `<T>` 绑定到
**函数**模板参数 `U`**（不是类模板参数 `T`）⇒ ⇒ `U` 被强制成 `T` ⇒ ⇒ 形参变成
`const T&` ⇒ ⇒ **更宽的分支被隐式窄化**（静默截断！）⇒ ⇒ **宽度判据根本没跑到**
⇒ ⇒ **D1 形同虚设**。

⚠️ **它是被 fixture 抓出来的**：三个「拒绝」fixture 当时**竟然编译通过**
⇒ ⇒ 若只写运行期用例，这个缺陷会**完全隐形**（运行期行为与「静默丢位」完全一样）。

⇒ ⇒ **教训**：判据的实现形态本身就是判据的一部分 ⇒ ⇒ 必须有**编译期** fixture
（运行期用例**看不见**「编译期该拒却没拒」）。

⇒ ⇒ 已在 `if.h` 的 `checked_value` 上加**防再犯注释**。

### 6.2 `ch_literal_impl` **不是** `HardwareType` ⇒ 需要**第三档**派发

`ch_literal_impl<V,W>` **无 `impl()`** ⇒ ⇒ `HardwareType<ch_literal_impl>` = false
⇒ ⇒ 若只按「硬件 / 整数字面量」两档分派，它会落进「非法类型」⇒ ⇒ 报错信息**误导**
⇒ ⇒ 补上 **CHLiteral** 档（`ch::core::CHLiteral`，`literal.h:166-178`）。

⚠️ 附带发现：`ch_uint<N>(const ch_literal_impl<V,W>&)` 自带
`static_assert(W <= N, "Literal width must not exceed target uint width")`
（`include/lnode/uint.tpp:38`）⇒ ⇒ sized-literal 分支有**两个**独立拒绝源
⇒ ⇒ 护栏的「中性化对照」必须**同时**处理两处（否则 R2 误报）。

### 6.3 护栏的中性化机制的**两个坑**（都实测踩中）

1. **替换串必须保留原消息串与分号**：把 `static_assert(cond, "msg");` 整体替换成
   `static_assert(true, "");` ⇒ ⇒ 原来的 `"msg"` + `);` 留在后面 ⇒ **语法被破坏**
   ⇒ ⇒ 后续判据**全部误报**。
   ⇒ ⇒ 修法：`if.h` 的两处**只换条件部分**（消息保留），`uint.tpp` 那处才整体替换。
2. **备份/还原必须覆盖**所有被改的文件（`if.h` + `uint.tpp`）⇒ ⇒ 还原失败会让
   后续判据在**被污染的实现**上跑。

⇒ ⇒ 中性化机制已写进 `scripts/verify_multi_if_width.sh` 的注释。

### 6.4 测试侧的**两个框架事实**（踩中）

1. **`__io` 宏只声明存储**（`include/core/io.h:312-319`）⇒ ⇒ MUST 显式
   `new (io_storage_) io_type;`（在 `create_ports()` 里）⇒ ⇒ 否则 `io()` 指向
   **未构造**的存储 ⇒ 端口节点为 **null** ⇒ ⇒ `operator<<=` 报
   `Invalid port connection: receiver node is null`（`io.h:451`）。
2. **顶层 `ch_in` 在 `describe()` 内**节点为 **null**（未驱动）⇒ ⇒ 不能当值源
   ⇒ ⇒ 零扩展用例改用**编译期常量条件**的模板化 DUT（ADR-052 Non-Goal #4）。

### 6.5 变异结果（每条**先确认编译成功**）

| # | 注入 | 实测结果 |
|---|---|---|
| **M1** | 宽度 `static_assert` 放宽（`<=` ⇒ 恒真） | ✅ 护栏 **R1 转红**（拒绝 fixture 意外通过） |
| **M2** | `zext` ⇒ `sext` | ✅ 运行期金标 `0x0A5` ⇒ `0x1A5` ⇒ **转红** |
| **M3** | 去掉整数字面量**值域检查** | ✅ `T_range` 不再抛 ⇒ **转红** |
| **M4** | 删掉 `branches_.size() < 2` 检查 | ✅ `T_B` 不再抛 ⇒ **转红** |
| **M5** | 去掉 port 归一化 | ✅ 护栏 **R3 转红**（`P_port` 退回模板深渊） |
| **M6** | 「更宽 ⇒ 拒绝」误写成「不等 ⇒ 拒绝」 | ✅ **正向 fixture 编译失败**（过度实现被抓住） |

⚠️ **M6 的预期结果就是编译失败** ⇒ ⇒ 变异 harness 必须为它**单独**走
「编译 fixture」路径（而不是先构建测试 target）—— 否则 harness 会因
「编译失败 ⇒ 绝不跑旧二进制」而**中止**（这是**正确**的 fail-closed，但会
掩盖 M6 的真实判据）。

⚠️ **如实登记无判别力的变异**：「zext 换**恒等存储**」**不红**（窄值存宽 `T`
天然无损）⇒ ⇒ 该维度只能靠编译期结构证据。
