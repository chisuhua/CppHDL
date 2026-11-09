<!-- touched-components: ch_state_machine -->

# Proposal: `chlib` 析构异常规格 vs 类型可组合性（ADR-067 落地）

> **权威决策**：`docs/adr/ADR-067-dtor-exception-spec-composability.md`（**Accepted** · 用户拍板）
> ⇒ ⇒ 本 proposal **MUST NOT** 重述或改写 ADR-067 的决策内容或理由。
>
> **improvement 来源**：`.rddf/improvements/chlib-dtor-noexcept-composability-2026-11.md`
> （7 段 · **approved** · 已过 `pre_create_brainstorm_check.sh` HARD-GATE）· 审稿记录
> `.rddf/reviews/ADR-067-dtor-composability.md`（oracle 6 条 + critic 9 条 **逐条应答**）
>
> **上游 issue**：`https://github.com/chisuhua/CppHDL/issues/28`（下游 ChipForge 的编译破坏反馈）
>
> **本 change 有 spec 变更** ⇒ ⇒ `specs/chlib-state-machine-semantics/spec.md`（**ADDED**）
> 与 `specs/chlib-conditional-semantics/spec.md`（**ADDED**）存在 ⇒ ⇒ 归档时
> **MUST NOT** 用 `--skip-specs`。

## Why

ADR-050 **D5** 让 `~ch_state_machine()` 成为 `noexcept(false)`（为了让「`contrib` 了但
从未 `resolve`」在**析构期可捕获**检出）⇒ ⇒ **析构非 `noexcept` 的类型** ⇒ ⇒
**不能作为「有虚析构的类」的按值成员**（派生类的隐式析构被推成 `noexcept(false)`
⇒ override 一个 `noexcept` 虚析构 ⇒ **硬编译错误**）。

⇒ ⇒ 该错误**只在下游**出现 ⇒ ⇒ 上游测试**恒绿** ⇒ ⇒ 零信号：

- **实测**：用**上游自己的** `ch::Component`（`include/component.h:62` 的
  `virtual ~Component();`，**隐式** noexcept）作基类 + 按值持有 FSM ⇒ **同样的编译错误**
  ⇒ ⇒ 「把 describe() 里的**局部** FSM 提为**组件成员**」这个**标准重构**在上游也不可用；
- **下游实证**：ChipForge 的 `FsmBase : PluginBase`（`virtual ~PluginBase() = default;`
  ⇒ **隐式** noexcept）⇒ 编译失败 ⇒ 上游 issue #28。

⚠️ **触发面比 issue 描述的更宽**：基类**不必显式**写 `noexcept` ⇒ 真实条件 =
「**基类链上有虚析构**」+「派生类**按值**持有」。

⇒ ⇒ C++17 下「析构期**可捕获**检出」与「可作按值成员」在**同一析构函数**上**互斥**
⇒ ⇒ ADR-067 裁定：**默认形态逐字不变** + **显式**提供「可组合形态」。

## What Changes

1. **`include/chlib/state_machine.h`**（`core/` **零改动** ⇒ 与 ADR-050 **D8** 同型边界）：
   - 新增 `enum class dtor_detection { automatic, explicit_only }`；
   - `ch_state_machine` 新增**第三模板实参** `Detection`（默认 `automatic` ⇒ **类型身份不变**）；
   - 析构按形态分派：`automatic` ⇒ `noexcept(false)` + 抛**可捕获**异常（**逐字保留 ADR-050 D5**）；
     `explicit_only` ⇒ `noexcept` + **终止兜底**（fail-closed · **绝不静默**）；
   - 新增 `check_unresolved()`（**只读**检查 ⇒ 抛**可捕获**异常 ⇒ 可组合形态的**可捕获**检出路径）；
   - 头注释新增「③ 析构形态与可组合性」段（形态表 + **选择指引** + **使用契约** + **拷贝警告**）。
2. **`include/chlib/if_stmt.h`**：头注释新增**使用契约**（决策 ③/④）⇒ ⇒ 条件块的
   「不可组合」在**两处**都有明文。
3. **契约同步**：2 份 capability 的 **ADDED** requirements（见 `specs/`）。
4. **测试**：`tests/test_chlib_composability_api.cpp`（**新** · 仓外视角 · 真编译 + 真链接 + 真运行）
   ⇒ ① 编译期 4 条 `static_assert` ② 运行期 3 条 ③ **子进程**观测终止兜底（SIGABRT）。
5. **静态护栏**：`scripts/verify_chlib_dtor_composability.sh`（R1 fixture 编译 / R2 API 面 /
   R3 两份头注释的契约 / R4 capability 契约）+ `--self-test`（M1/M2/M3 **原地投毒** ⇒ 必红）。
6. **CI 接线**：护栏 + `--self-test` 两步挂进 `build-and-test` 的 guard 段。

## Impact

- **产品代码**：`include/chlib/state_machine.h`（主要）· `include/chlib/if_stmt.h`（**仅注释**）
  ⇒ ⇒ `core/` **零改动** ⇒ ⇒ `ch_reg` 单 next 源 / `ch_out` 单次驱动契约**零变化**；
- **契约**：2 份 capability **ADDED**（`chlib-conditional-semantics` 的既有
  「析构 MUST `noexcept(false)`」requirement **零改动** ⇒ ⇒ **无 MODIFIED delta**）；
- **测试/CI**：新增 1 个 ctest target ⇒ `CTEST_TOTAL` **253 → 254**；
- **上游用户**：**默认形态零改动** ⇒ ⇒ 既有 8 处实例化点逐字不变；
- **下游用户**：可改用「可组合形态」⇒ ⇒ **按值持有可行** ⇒ ⇒ 检出走
  「显式 `check_unresolved()`（可捕获）+ 析构期终止兜底」；
- **回滚**：**纯增量** ⇒ ⇒ 还原本 change 即可（无数据迁移 / 无序列化变更）。

## 验收标准（= improvement 的 Acceptance + ADR-067 §5；逐条见 `tasks.md`）

- [ ] **默认形态零变更**：既有 `tests/test_state_machine.cpp` 逐字不变且绿；
- [ ] **可组合形态可编译**：`static_assert(std::is_nothrow_destructible_v<Dut>)`（`Dut : ch::Component` + 按值持有）绿；
- [ ] **默认形态仍不可组合**：`static_assert(!std::is_nothrow_destructible_v<SmDefault>)` 绿；
- [ ] **静默降级被钉死（合取形态）**：`!is_nothrow_destructible_v<SmDefault>` **且**
      `is_nothrow_destructible_v<std::optional<SmDefault>>` 同时绿；
- [ ] **变异自检**：把 `noexcept` 翻回 `noexcept(false)` ⇒ fixture **必红** ⇒ 还原复绿；
- [ ] **终止兜底可测**（子进程 SIGABRT）/ **展开期不终止** / **显式检查可捕获**；
- [ ] **使用契约已固化**：2 份头注释 + 2 份 capability spec 含精确措辞 ⇒ `grep` 可判；
- [ ] **capability delta 合入主 spec**：`verify_openspec_delta.sh` + `openspec validate --specs --strict` 绿；
- [ ] **计数锚点同步**：`CTEST_TOTAL` **254** ⇒ `verify_test_counts.sh` 绿。

## 用户可见变更

（本段按 `.rddf/checks/public-api-surface.md` 的载体机制填写 —— 每个归档 change
MUST 一行以上，**下游可发现性**是唯一目的；护栏 = `scripts/verify_public_api_visibility.sh` R1。）

- **新增（不破坏）**：`chlib::dtor_detection`（`automatic` / `explicit_only`）与
  `chlib::ch_state_machine` 的**第三模板实参** `Detection`（默认 `automatic`
  ⇒ 既有代码**零改动**；类型身份不变）。下游若需按值持有 FSM，改用
  `chlib::ch_state_machine<S, N, chlib::dtor_detection::explicit_only>`。
- **新增（不破坏）**：`chlib::ch_state_machine::check_unresolved()`
  （只读检查 ⇒ 抛**可捕获**异常；检出**非幂等** ⇒ `catch` 后请继续 `resolve_*`）。
- **行为变化（仅 `explicit_only`）**：该形态的析构为 `noexcept` ⇒ 未 resolve 的贡献
  在析构期**终止**（fail-closed，`std::terminate`）而不是抛异常。
- **契约（不破坏）**：默认形态的析构**仍**为 `noexcept(false)`（ADR-050 D5 逐字保留）
  ⇒ ⇒ 「析构非 `noexcept` 的类型」MUST NOT 作为「有虚析构的类」的按值成员或基类
  （ADR-067 决策 ④ —— 该契约同时适用于 `chlib::conditional_block`）。
