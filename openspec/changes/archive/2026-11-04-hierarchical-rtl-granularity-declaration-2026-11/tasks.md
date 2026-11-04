# 任务清单：`hierarchical-rtl-granularity-declaration-2026-11`

> 判据来源：proposal.md §Acceptance · capability delta（ownership ADDED ×1 · hierarchical ADDED ×2）。
> ⚠️ **前置**：**T3-①a**（类级宏与编译期可见名）· **T3-①b**（组件树正确 ⇒ 装配点可判定）。

## T1 两个新 API 名（`include/module.h`）

- [x] 1.1 `ch_layer<T>`：显式成层；**T 已声明成层 ⇒ 继承其类级名**（单参形态）；
      **T 未声明 ⇒ 实例处 MUST 给名**（缺名**没有可用重载** ⇒ 编译期错误）
      ⚠️ 用 `requires detail::has_class_layer_name<T>` 做**编译期**分派
- [x] 1.2 `ch_inline<T>`：显式 inline（覆盖类级声明）
- [x] 1.3 两者 **复用** `ch_module` 的机制（薄封装 + 受保护 ctor）⇒ ⇒ 零破坏
      （实现形态：`ch_module` 加**受保护**的第二个 ctor 接收 `InstanceDecl`；
      实例级 setter 是 **private** + `friend ch_module` ⇒ 公开面只有两个封装）
- [x] 1.4 `ch_module` 公开 ctor 签名**零变更**（`static_assert(is_constructible<ch_module<T>, string>)`）

## T2 运行期判定（`include/component.h`）

- [x] 2.1 `Component::is_layer()`：`Granularity` > 实例级覆盖 > 类级宏（默认 inline）
- [x] 2.2 `Granularity` 枚举 + `granularity()` / `set_granularity()` + `GranularityScope`（RAII）
- [x] 2.3 `PortPolicy{SynthesizeWarn, SynthesizeFatal, Reject}` ·
      `UndrivenPolicy{Bubble, Reject}`（默认 `SynthesizeWarn` / `Bubble`，ADR-060 §2.4）

## T3 四档来源（`include/module.h` + `include/component.h`）

- [x] 3.1 ② 继承类级名 ⇒ 判据含**负向**：`ch_layer<Layered>` 的**两参**形态 MUST 编译不过
- [x] 3.2 ③ 实例处给名 ⇒ 缺名 MUST 编译不过（`static_assert(!is_constructible<...>)`）
- [x] 3.3 ④ `ForceLayer` 回退 `typeid` + `CHWARN`（⚠️ **未 sanitize** —— sanitize 归发射侧；
      每组件只告警一次）

## T4 判据

- [x] 4.1 三组合判据：`ch_module`(类级) / `ch_inline`(覆盖) / `ch_module`(未声明⇒inline) /
      `ch_layer<Helper>`(提升) / `ch_layer<Layered>`(继承)
- [x] 4.2 判据：`make_unique` + `add_child` + `build(ctx)`（无 active parent）⇒ 仍按类级声明成层
      + 登记进组件树 + `parent()` 非空
- [x] 4.3 四档逐档可判定；「缺名 ⇒ 编译失败」用 **`static_assert` + `requires`** 承载
      （⇒ 无需额外 fixture 脚本：判据在测试二进制里 ⇒ 无法静默腐坏）
- [x] 4.4 `ForceInline` ⇒ 全部 inline（含类级声明）；`ForceLayer` ⇒ 全部成层（含未声明）
- [x] 4.5 **零回归**：`ctest -LE perf` **235/235** · samples 39/39 · examples 28/28 ·
      计数锚点同步（245 → 246 / non-perf 234 → 235 / `tests/test_*.cpp` 137 → 138）

## 验证命令

```bash
cd /workspace/project/CppHDL
cmake --build build -j"$(nproc)"
ctest -LE perf --output-on-failure
bash scripts/run_all_samples.sh
./run_all_ported_tests.sh
openspec validate hierarchical-rtl-granularity-declaration-2026-11 --type change --strict
bash scripts/verify_openspec_delta.sh
```
