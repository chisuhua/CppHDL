# Capability: chlib-state-machine-semantics

## ADDED Requirements

### Requirement: `ch_state_machine` MUST 提供两种析构形态（默认可捕获检出 / 可组合形态）

`ch_state_machine` MUST 提供两种析构形态（第三模板实参 `dtor_detection`）：

| 形态 | 析构 | 检出 | 可作按值成员 / 基类 |
|---|---|---|---|
| `dtor_detection::automatic`（**默认**） | `noexcept(false)` | 析构期**自动**检出 ⇒ 抛**可捕获**异常 | ❌ |
| `dtor_detection::explicit_only` | `noexcept` | ① **显式** `check_unresolved()` ⇒ 抛**可捕获**异常 ② 析构期**终止兜底**（`std::terminate`） | ✅ |

⇒ ⇒ **默认形态逐字保留** ADR-050 **D5**（「`contrib` 了但从未 `resolve`」MUST 在**析构期**
**可捕获**检出）⇒ ⇒ 既有实例化点 MUST **零改动**（默认实参 ⇒ 类型身份不变）。

⇒ ⇒ **可组合形态**的检出 MUST 是 **fail-closed** 的：析构期若确有未 resolve 的贡献
⇒ MUST 输出诊断并 `std::terminate()` ⇒ ⇒ **绝不静默**。

⚠️ **检出非幂等**：`check_unresolved()` 是**只读**检查 ⇒ `catch` 后**不 resolve**
⇒ ⇒ 析构期兜底**照常终止** ⇒ ⇒ MUST NOT 引入「已检出 ⇒ 豁免」的标志位
（那会把检出变成「一次性提醒」⇒ ⇒ 退回静默）⇒ ⇒ `catch` 之后**继续 `resolve`**
⇒ ⇒ 兜底自然不触发（**推荐的恢复路径**）。

⚠️ **展开期 / 静态析构期**：两者 MUST **不抛 / 不终止**
（⇒ 不新增「异常展开中二次 terminate」路径）。

⚠️ **唯一权威** = `docs/adr/ADR-067-dtor-exception-spec-composability.md`
（决策 ② · **Accepted**）。

#### Scenario: 默认形态的析构 MUST 抛可捕获异常

- **WHEN** 默认形态（`dtor_detection::automatic`）的 `ch_state_machine` 存在
      **未 resolve** 的贡献，且在**非展开期**、**非静态析构期**被销毁
- **THEN** 析构 MUST 抛出**可捕获**异常（`std::runtime_error`）
- **AND** 该形态的析构异常规格 MUST 为 **`noexcept(false)`**
      （⇒ ⇒ 类型 MUST NOT 被当作「有虚析构的类」的按值成员或基类）

#### Scenario: 可组合形态 MUST 可作按值成员

- **WHEN** 用 `dtor_detection::explicit_only` 实例化 `ch_state_machine`
- **THEN** 该类型的析构异常规格 MUST 为 **`noexcept`**
- **AND** 一个**有虚析构**的类 MUST 能**按值**持有它
      （⇒ 其隐式析构异常规格 MUST 仍为 `noexcept` ⇒ ⇒ 编译 MUST 通过）

#### Scenario: 可组合形态的显式检查 MUST 可捕获

- **WHEN** 可组合形态存在未 resolve 的贡献，且调用 `check_unresolved()`
- **THEN** MUST 抛出**可捕获**异常（`std::runtime_error`）
- **AND** 若贡献已被 `resolve_*` 消解 ⇒ `check_unresolved()` MUST **不抛**

#### Scenario: 可组合形态的析构兜底 MUST 终止（fail-closed）

- **WHEN** 可组合形态存在未 resolve 的贡献，且在**非展开期**、**非静态析构期**被销毁
- **THEN** MUST 输出诊断并 `std::terminate()`
- **AND** MUST NOT 静默丢弃该未 resolve 的贡献

#### Scenario: 展开期 MUST NOT 终止

- **WHEN** 可组合形态存在未 resolve 的贡献，且在**异常展开期**被销毁
- **THEN** MUST **不**终止（⇒ 不新增「双异常 ⇒ terminate」路径）
- **AND** 正在传播的异常 MUST 照常传播
