# 任务清单：`hierarchical-rtl-ownership-fix-2026-11`

> 判据来源：proposal.md §Acceptance · capability delta（ownership ADDED ×5）。
> ⚠️ **前置**：**T3-①a**（纯命名）—— 本 change 的 `ch_module` parent 修正依赖
> 正确的 `name()` / `local_name()`。

## T1 作用域栈（`include/component.h` + `src/component.cpp`）

- [ ] 1.1 `static thread_local std::vector<Component*> scope_` + `node_owner()`
      （栈顶优先；空则回落 `current_`）
- [ ] 1.2 `Component::Component()` **压入** CONSTRUCT 作用域、**构造函数出口弹出**（配对）
- [ ] 1.3 `build_internal()` 压入 / 弹出 BUILD 作用域
- [ ] 1.4 `create_child` 路径 MUST **不进入**任何作用域
- [ ] 1.5 作用域**配平**断言（用 `CHFATAL_EXCEPTION`）+ RAII 回退（构造抛异常时）

## T2 `ch_module`（`include/module.h` + `src/component.cpp`）

- [ ] 2.1 parent 取「最内层作用域的组件」（`node_owner()`），MUST NOT 取祖父
- [ ] 2.2 build context 取**环境 context**（D2′）；env ctx 为空时 MUST **NOT** 硬失败
- [ ] 2.3 无 active parent ⇒ `CHFATAL_EXCEPTION`（D8，MUST NOT 只打日志）
- [ ] 2.4 `build(ctx)` 的 ctx MUST == 首次绑定（D2″ 基准两支）

## T3 组件树（`src/component.cpp`）

- [ ] 3.1 `add_child` MUST 补绑 `child->parent_`
- [ ] 3.2 归属判定 MUST **只向下走 `children()`**（MUST NOT 用 parent 链）
- [ ] 3.3 `context::init()` 建的 clock/reset MUST `owner == nullptr`（D10）

## T4 判据

- [ ] 4.1 Leaf `ch_reg` 成员 `get_parent()` == Leaf
- [ ] 4.2 成员 `ch_module` 挂直接父 + 可 `io()`
- [ ] 4.3 **顶层 `ch_device<Pipeline>` 可实例化且不 SIGSEGV**
- [ ] 4.4 作用域配平（含「`create_child` 不 build」用例 + 构造抛异常 RAII 回退）
- [ ] 4.5 **归属 ↔ 语义分流**回归（`type_input` 分类逐项一致）
- [ ] 4.6 `io.h` 4 处守卫判据 = **「改前改后均无触发」**
- [ ] 4.7 跨 ctx 两支 + env ctx 为空不硬失败
- [ ] 4.8 组件树登记双向完整 + 成员子组件不被误杀
- [ ] 4.9 **零回归**：`ctest -LE perf` / samples / examples 全绿 · 计数锚点同步

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
