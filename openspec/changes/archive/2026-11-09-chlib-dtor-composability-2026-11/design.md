# Design: `chlib` 析构异常规格 vs 类型可组合性（ADR-067 落地）

> ⚠️ **本文件承载推导 / 逐条核算 / 变异清单**（ADR-067 的「推导与证据不进正文」纪律）
> ⇒ ⇒ 决策本身在 `docs/adr/ADR-067-dtor-exception-spec-composability.md`。

## 1. API 形态（ADR-067 决策 ② 的落地形态）

```cpp
namespace chlib {

/// 析构期检出策略（ADR-067 决策 ②）
enum class dtor_detection {
    automatic,     ///< 析构期自动检出（抛**可捕获**异常）—— 默认（逐字保留 ADR-050 D5）
    explicit_only  ///< 析构 noexcept（⇒ 可作按值成员 / 基类）⇒ 显式检查 + 终止兜底
};

template <typename StateEnum, size_t N,
          dtor_detection Detection = dtor_detection::automatic>
class ch_state_machine { ... };

}
```

### 1.1 为什么用**枚举**而不是 `bool`

`bool DetectUnresolved = false` 在调用点是**不可读的**（`false` 到底是「不检出」还是
「不自动检出」？⇒ 可组合形态**仍然有**检出，只是时机不同）⇒ ⇒ 用具名枚举
⇒ ⇒ 读点自解释：`ch_state_machine<S, 3, chlib::dtor_detection::explicit_only>`。

⚠️ **枚举名的诚实性**：`explicit_only` 表达「**只有**显式路径检出」⇒ ⇒ 准确
（终止兜底是**兜底**，不是「检出路径」）。

### 1.2 异常规格的推导（**逐条**）

派生类的隐式析构异常规格由**子对象**推导 ⇒ ⇒ 「成员的析构是 potentially-throwing」
⇒ ⇒ 派生类析构 `noexcept(false)` ⇒ ⇒ override 一个 `noexcept` **虚**析构 ⇒ 硬错误。

⇒ ⇒ 故「可组合」⇔ 「析构 `noexcept`」⇔ 「析构期不可抛」⇒ ⇒ 三者**同生共死**。

| 形态 | 析构 | 派生类持它 ⇒ 派生类析构 | 可组合 | 析构期检出 |
|---|---|---|---|---|
| `automatic` | `noexcept(false)` | `noexcept(false)` ⇒ **override `noexcept` 虚析构 ⇒ 编译错误** | ❌ | **可捕获**（抛） |
| `explicit_only` | `noexcept` | `noexcept` ⇒ ✅ | ✅ | **不可捕获**（`std::terminate`） |

⇒ ⇒ **默认形态逐字不变** ⇒ ⇒ 既有 8 处实例化点的**类型身份**不变
（`std::is_same_v<ch_state_machine<S,N>, ch_state_machine<S,N,dtor_detection::automatic>>`）。

### 1.3 终止兜底的实现要点

```cpp
~ch_state_machine() noexcept(kDetectAtDtor ? false : true) {
    if (!has_unresolved_contributions()) return;
    if (std::uncaught_exceptions() > 0 ||
        ch::detail::in_static_destruction()) return;      // ← 展开期 / 静态析构期
    if constexpr (kDetectAtDtor) {
        CHFATAL_EXCEPTION("... never resolved ...");        // ← 可捕获（逐字保留 ADR-050 D5）
    } else {
        CHERROR("... never resolved ... => terminating ...");
        std::terminate();
    }
}
```

⚠️ **两道门与既有实现逐字一致**（`state_machine.h` 原实现 + `if_stmt.h`）：
`std::uncaught_exceptions() == 0` ⇒ 不抛；`!in_static_destruction()` ⇒ 不抛
⇒ ⇒ **可组合形态**照抄 ⇒ ⇒ 不新增「双异常 ⇒ terminate」路径。

⚠️ `CHERROR` 与 `CHFATAL_EXCEPTION` **只打日志 / 才抛** ⇒ ⇒ 终止路径 MUST 用
`CHERROR`（日志）+ `std::terminate()`（**不可捕获**）⇒ ⇒ **绝不静默**。

⚠️ **`msg` 变量陷阱**（根 `AGENTS.md`）：`CHERROR` / `CHFATAL` 宏内部声明了
`std::string msg` ⇒ 实参里 MUST NOT 出现名为 `msg` 的变量 ⇒ 本实现无此变量 ✓。

### 1.4 `check_unresolved()` 的语义

**只读**（不消解任何状态）⇒ ⇒ **检出非幂等** ⇒ ⇒ `catch` 后不 resolve ⇒
析构兜底照常终止 ⇒ ⇒ MUST NOT 引入「已检出 ⇒ 豁免」标志位（ADR-067 决策 ② 语义 4）。

⇒ ⇒ 两形态**都**提供该方法（单一实现 ⇒ 无 `if constexpr` 的 API 分叉）
⇒ ⇒ 默认形态下它是「提前检出」（析构期仍会抛 ⇒ 行为不变）。

## 2. 判据设计（ADR-067 决策 ⑤）

### 2.1 三面判据（缺一面 ⇒ 「绿」无判别力）

| 面 | 内容 | 为什么必须 |
|---|---|---|
| **① 编译期** | 4 条 `static_assert` | 「可组合」是**类型级**属性 ⇒ 只能由类型系统判 |
| **② 运行期** | 显式检查可捕获 / resolve 后不抛 / 展开期不终止 | 「可捕获」与「展开期不终止」是**行为** ⇒ 静态断言判不了 |
| **③ 子进程** | 析构期兜底 ⇒ SIGABRT | `std::terminate` **同进程观测不到** ⇒ 必须子进程 |

### 2.2 编译期 4 条断言（**逐条**）

```cpp
static_assert(std::is_same_v<SmDefault, chlib::ch_state_machine<TestState,3,chlib::dtor_detection::automatic>>);
//  ↑ 「默认形态零变更」在**类型层**可判（默认实参 ⇒ 同一类型）

static_assert(!std::is_nothrow_destructible_v<SmDefault>);      // 默认形态仍 noexcept(false)（ADR-050 D5）
static_assert(std::is_nothrow_destructible_v<SmComposable>);    // 可组合形态 noexcept
static_assert(std::is_nothrow_destructible_v<std::optional<SmDefault>>);  // ⚠️ 见 §2.3
```

### 2.3 ⚠️ `std::optional` 的**落差**（⚠️ 取反会写成恒假断言）

实测（`/tmp/opencode/issue28/optcheck.cpp`）：libstdc++ 把 `T` 放进 union 且 union 析构
是**用户提供 + noexcept** ⇒ ⇒ `~optional()` 推导为 **`noexcept`**
⇒ ⇒ `is_nothrow_destructible_v<std::optional<SmDefault>>` **为真**。

⇒ ⇒ 故「静默降级」MUST 钉成**合取**（「`Sm` 假 ∧ `optional<Sm>` 真」）：

```cpp
static_assert(!std::is_nothrow_destructible_v<SmDefault> &&
              std::is_nothrow_destructible_v<std::optional<SmDefault>>);
```

⚠️ ⚠️ **写成 `!is_nothrow_destructible_v<std::optional<SmDefault>>` 是恒假断言**
⇒ ⇒ 判据**永不可达** ⇒ ⇒ 这是 ADR-067 审稿第 1 轮 BLOCKER 的原形 ⇒ ⇒
**禁**在别处重犯。

### 2.4 ★ 与下游**同形**的 DUT

```cpp
struct ComposableDut : ch::Component {   // ★ include/component.h:62 的 virtual ~Component();
    SmComposable sm_;                     //   ⇒ 隐式 noexcept ⇒ 与下游 PluginBase 同形
    void describe() override {}
};
static_assert(std::is_nothrow_destructible_v<ComposableDut>);
```

⇒ ⇒ 这一条**直接**复现下游的形状 ⇒ ⇒ 它绿 ⇒ ⇒ 「下游能编译」当场成立。

### 2.5 子进程判据的两个**坑**（实测）

⚠️ **坑 1**：`/proc/self/exe` **不能**直接写进命令行 ⇒ ⇒ 那是 **shell** 的 exe
（`/proc/self/exe` 由**执行 exec 的那个进程**解析 ⇒ shell 把自己再跑一遍 ⇒ 子进程退出码 0
⇒ ⇒ 断言**恒绿** ⇒ ⇒ **假绿陷阱**）⇒ ⇒ MUST 在**父进程**里 `readlink` 解析出绝对路径。

⚠️ **坑 2**：`std::system` 返回**原始 wait status**（glibc）⇒ ⇒ **两种形态**都要认：
① 子进程被**信号**直接杀死 ⇒ `status & 0x7f` == 信号编号；
② **shell** 把信号死亡折成退出码 `128 + 信号` ⇒ `(status >> 8)` == `128 + 信号`
（★ 实测 `sh -c` 会 exec 掉自己 ⇒ 子进程被信号杀死时 shell 往往以**退出码 134** 正常退出
⇒ ⇒ 只判 ① 会**恒红**）。

## 3. 静态护栏（`scripts/verify_chlib_dtor_composability.sh`）

| 规则 | 内容 | 判别力（自检） |
|---|---|---|
| **R1** | fixture MUST 编译 | **M1**：把 `noexcept(kDetectAtDtor ? false : true)` 翻成 `noexcept(false)` ⇒ **必红**（实测：3 处红，含真实的「looser exception specification」） |
| **R2** | 头 MUST 暴露 `dtor_detection`（两个枚举值）+ `check_unresolved()` | **M3**：改名 `check_unresolved` ⇒ 必红 |
| **R3** | **两份**头注释 MUST 含契约措辞 | **M2**：从 `if_stmt.h` 抹掉 token ⇒ 必红 |
| **R4** | capability spec（**主 spec 或** pending delta）MUST 含契约措辞 | （随 spec 合并自动转绿） |

⚠️ **R4 为何取并集**：本 change 的 spec delta 在 `openspec/changes/<name>/specs/`
⇒ ⇒ 合并进主 spec **之前**，主 spec 里**没有**该契约 ⇒ ⇒ 只查主 spec ⇒ **CI 恒红**
⇒ ⇒ ⇒ 取「主 spec ∨ pending delta」⇒ ⇒ 合并后 delta 归档 ⇒ 主 spec 命中 ⇒ ⇒ 恒绿
⇒ ⇒ 而主 spec 若**回退**且无 pending delta ⇒ 红 ✓。

## 4. 变异清单（**每条都实测**）

| # | 变异 | 预期 | 实测 |
|---|---|---|---|
| **M1** | `noexcept(kDetectAtDtor ? false : true)` → `noexcept(false)` | fixture **必红** | ✅ 3 处红（2 × `static_assert` + 1 × 真实编译错误） |
| **M2** | `if_stmt.h` 抹掉契约 token | R3 必红 | ✅（`--self-test` 内置） |
| **M3** | `check_unresolved` 改名 | R2 必红 | ✅（`--self-test` 内置） |

## 5. 未决 / 明确不做

- **条件块的可组合形态**（ADR-067 决策 ③）⇒ ⇒ **可行但当前不做** ⇒ ⇒ 独立立项入口已登记；
- **删拷贝**（ADR-067 决策 ⑨）⇒ ⇒ 独立后续项（理由 = 「浅拷贝节点句柄 = 语义地雷」）；
- **elaboration 期扫描**（决策 ⑦）⇒ ⇒ 独立立项；
- **恢复 `is_built()`**（决策 ⑧）⇒ ⇒ 归姊妹 improvement `public-api-remove-visibility-2026-11`；
- ⚠️ **本仓有**两个 `chlib` 命名空间**（`::chlib` 与 `ch::chlib`）⇒ ⇒ 聚合头 `chlib.h` +
  `chlib::` 限定 ⇒ **歧义编译错误** ⇒ ⇒ 已登记为**独立立项**项
  （`include/bundle/stream_bundle_member_inlines.h` 头注释）⇒ ⇒ 本 change **不动**命名空间布局
  ⇒ ⇒ fixture 因此**采用下游口径的 include 集**（`chlib/state_machine.h` + `component.h` + `core/context.h`）。
