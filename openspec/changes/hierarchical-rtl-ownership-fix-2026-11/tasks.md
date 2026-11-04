# 任务清单：`hierarchical-rtl-ownership-fix-2026-11`

> 判据来源：proposal.md §Acceptance · capability delta（ownership ADDED ×5）。
> ⚠️ **前置**：**T3-①a**（纯命名）—— 本 change 的 `ch_module` parent 修正依赖
> 正确的 `name()` / `local_name()`。

## T1 作用域栈（`include/component.h` + `src/component.cpp`）

- [x] 1.1 `static thread_local std::vector<Component*> scope_` + `node_owner()`
      （栈顶优先；空则回落 `current_`；D10 抑制期返回 `nullptr`）
- [x] 1.2 `Component::Component()` **压入** CONSTRUCT 作用域；弹出由 `ConstructionFrame`
      截断完成（⚠️ 基类**无法**感知「派生类成员造完」⇒ 见 design.md §4 偏离 1）
- [x] 1.3 `build_internal()` 压入 / 弹出 BUILD 作用域（`CHSCOPE_EXIT` ⇒ 异常路径也弹出）
- [x] 1.4 `create_child` 路径 MUST **不进入**任何作用域（只用 `ConstructionFrame` 截断子组件自己的帧）
- [x] 1.5 作用域配平：`~Component` 的 `erase_scope_frame()` 兜底（构造抛异常时基类析构仍会跑）
      + `scope_depth()` 判据（`REQUIRE(scope_depth() == 0)`）

## T2 `ch_module`（`include/module.h` + `src/component.cpp`）

- [x] 2.1 parent 取 `Component::node_owner()`（MUST NOT 取祖父）
- [x] 2.2 build context 取 `ch::core::ctx_curr_`；env ctx 为空 ⇒ `parent->defer_build(...)`
      ⇒ 宿主 `build(ctx)` 时补齐（MUST NOT 硬失败）
- [x] 2.3 无 active parent ⇒ `CHFATAL_EXCEPTION`（错误信息列出三种合法投喂方式）
- [x] 2.4 `build(ctx)` 的 ctx MUST == 首次绑定（分支 ① 构造期 env ctx / 分支 ② 首次 build ctx）
      ⚠️ 守卫 MUST 在 `built_` 早退**之前**（否则重复 build 时永不生效）

## T3 组件树（`src/component.cpp`）

- [x] 3.1 `add_child` 补绑 `child->parent_` **且** 宿主 `build()` 里再补一次
      （`rebind_children_parents()` —— 成员形态时宿主尚未被 `shared_ptr` 接管 ⇒ 见 design.md §4 偏离 2）
- [ ] 3.2 归属判定 MUST **只向下走 `children()`**（MUST NOT 用 parent 链）
      ⚠️ **本 change 不实现「归属判定」本身**（那是 T3-② 分析层）；本 change 只保证
      「`children()` 与 `parent()` **双向完整**」这条前提 ⇒ 判据在 4.8
- [x] 3.3 `context::init()` 建的 clock/reset MUST `owner == nullptr`（D10 · `core::NodeOwnershipSuppression`）

## T4 判据

- [x] 4.1 Leaf `ch_reg` 成员 `get_parent()` == Leaf（且节点名 `top.mid.leaf._reg`）
- [x] 4.2 成员 `ch_module` 挂直接父 + 可 `io()`
- [x] 4.3 `ch::ch_device<riscv::Rv32iPipeline>` 可实例化且不 SIGSEGV（7 个成员 `ch_module`
      全部挂直接父 + 各拥有节点）⇒ `examples/removed_examples.txt` 的归位条件**解除**
- [x] 4.4 作用域配平（含「`create_child` 不 build」用例 + 构造抛异常路径）
- [x] 4.5 **归属 ↔ 语义分流**回归：`type_input` 无「新增无归属节点」（`simulator_init.cpp:95` 分流不变）
- [x] 4.6 `io.h` 4 处守卫：判据 = **条件成立**（`CHREQUIRE` 只打日志 ⇒ 不能用「触发面 diff」）
- [x] 4.7 跨 ctx 两支 + env ctx 为空不硬失败（推迟 build）
- [x] 4.8 组件树登记双向完整（`children()` 含之 **且** `parent().get()` 非空）
- [x] 4.9 **零回归**：`ctest -LE perf` **234/234** · samples 39/39 · examples 28/28 ·
      计数锚点同步（244 → 245 / non-perf 233 → 234 / `tests/test_*.cpp` 136 → 137）

## 验证命令

```bash
cd /workspace/project/CppHDL
cmake --build build -j"$(nproc)"
ctest -LE perf --output-on-failure
bash scripts/run_all_samples.sh
./run_all_ported_tests.sh
openspec validate hierarchical-rtl-ownership-fix-2026-11 --type change --strict
bash scripts/verify_openspec_delta.sh
```
