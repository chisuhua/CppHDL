# Proposal: 层次化 RTL 输出 · 第 1 段 —— 纯命名（`hierarchical-rtl-naming-fix-2026-11`）

## Why

### 缺陷（实测 · 可复算）

`build_hierarchical_name()`（`include/module.h:19`）用 `parent->hierarchical_name()` 拼子路径，
而 `Component::hierarchical_name()`（`include/component.h:61`）本身**又**把父路径拼到
`name_` 前面 ⇒ ⇒ 而 `name_` 对 `ch_module` 子组件**已经是全路径**（`ch_module` ctor 传的是
算好的 `child_path_name`）⇒ ⇒ **父路径被拼两次**。

⇒ 深度 3 的实例路径实测形如 `top.top.m.lf`（ADR-059 §1.1 的第三条）。

⚠️ **`hierarchical_name()` 的唯一消费者就是 `module.h:27`** ⇒ ⇒ 这是一个
**自伤式**缺陷：两个函数互相喂全路径 ⇒ ⇒ 谁也拿不到正确的局部名。

### 为什么现在修（不是「顺手」）

① **它是硬前置的起点**：ADR-059 §5 要求「深度 ≥3 的实例路径 MUST 无重复前缀」，
而这条判据本身要靠**命名**表达 ⇒ ⇒ 命名不修 ⇒ 后续归属/粒度/emitter 全部建在错的名字上；
② **ADR-059 D4** 明确要求新增 `local_name()` / `module_type_name()` **两个互不替代的命名维度**，
并把「模块类型名的来源」定为**四档**（§2.2.1）⇒ ⇒ 其中**第 ① 档（类级宏）**由本 change 落地；
③ **ADR-061 决策①** 的合并宏 `CH_HIERARCHY_LAYER("name")`（**声明 + 命名一体**）需要
「类级可编译期可见的名字」⇒ ⇒ 那是本 change 的产物。

## What Changes

1. **D3**：`build_hierarchical_name()` 改用 `parent->name()`（**不是** `hierarchical_name()`）
   ⇒ ⇒ 深度 ≥3 不再重复前缀。
2. **D4（类级部分）**：`Component` 新增两个命名维度 ——
   - `local_name_`（实例**局部**名）+ `local_name()` 访问器；
   - `module_type_name_`（**模块类型名**）+ `module_type_name()` 访问器；
   - `name()` 语义**不变**（仍是**全路径**）⇒ ⇒ **单一命名真源**：`name()` = 全路径、
     `local_name()` = 局部。
3. **合并宏** `CH_HIERARCHY_LAYER("name")`：**声明 + 命名一体** ⇒ ⇒
   「成层却忘了命名」这个组合**编译期即不可表示**（ADR-059 §2.2.1 第 ① 档）。
4. **删除 `Component::hierarchical_name()`**：它的唯一消费者在 (1) 里被替换 ⇒ ⇒
   留着 = 两个真源 = 前缀重复会**再回来** ⇒ ⇒ ZERO-DEBT「无死代码」。

## 不做（Non-Goals）

- **不做**「全仓普查所有头里的非 inline 自由函数」⇒ ⇒ 那是 `header-odr-inline-fix-2026-11`
  （已归档）的**链接期**判据，本 change 不重复。
- **不改**归属（作用域栈 / `node_owner()` / `ch_module` parent）⇒ ⇒ 那是 **T3-①b**。
- **不改**粒度语义（`ch_layer` / `ch_inline` / `is_layer()` / 无 parent 装配点）⇒ ⇒ 那是 **T3-①c**。
  ⇒ ⇒ 本 change 只**定义**类级宏与名字存储；**四档来源的 ②③④ 分派**归 ①c
  （档 ②③ 需要 `ch_layer<T>`，本 change 尚不可表达 ⇒ ⇒ 放进 ①a 会让该 change
  **无法自证自己的判据**）。
- **不改** `toVerilog()` 的**语义**（端口筛选 / 算子映射 / `always_ff` / mem 一律不动）
  ⇒ ⇒ 只改信号名 ⇒ ⇒ 扁平产物**语义等价、文本有变**（ADR-059 D5）。
- **不**引入 per-module 局部命名（= 命名权威仍是 ctx 级全局表）。

## Acceptance

- [ ] **深度 ≥3 的实例路径无重复前缀**：三层组件树（Top → Mid → Leaf）⇒ Leaf 路径 MUST 形如
      `top.m.lf`，MUST NOT 形如 `top.top.m.lf`
- [ ] **`local_name()` 返回局部名**（不含父路径）；**`module_type_name()` 返回类型名**
      ⇒ ⇒ **同一类型的多实例 MUST 返回相同值**（⇒ module 名由 `<module_type_name>_<序号>` 派生，
      序号 = 该组内实例**按实例路径排序**的次序 ⇒ ⇒ 确定性）
- [ ] **命名权威 = ctx 级全局表**：层次化 MUST NOT 发明 per-module 局部命名
      ⇒ ⇒ 顶层端口名 MUST 与扁平产物**逐字相同**；MUST NOT 带框架前缀
- [ ] **合并宏**：`CH_HIERARCHY_LAYER("name")` 一次给出「成层 + 模块类型名」
      ⇒ ⇒ 缺名**编译期不可表达**（MUST NOT 推迟到发射时的运行期硬错）
- [ ] **单一真源**：`Component::hierarchical_name()` MUST NOT 仍存在（否则两个真源 ⇒ 前缀重复复发）
- [ ] **零语义变更**：仿真结果**逐位不变**；`ctest -LE perf` / `run_all_samples.sh` /
      `run_all_ported_tests.sh` 全绿；计数锚点同步
- [ ] **文本变化面逐条登记**（信号名会变 ⇒ 至少覆盖 ADR-059 §5 列出的 4 处实测依赖 +
      本仓 trace 输出）

## Capabilities

| capability | delta |
|---|---|
| `component-ownership-and-naming` | **ADDED ×2**（R2 组件实例路径 MUST 无重复前缀 · R3 两个命名维度） |
| `rtl-hierarchical-output` | **ADDED ×1**（R3 命名权威 MUST 是 ctx 级全局表） |

⇒ 归档**禁** `--skip-specs`。

## Impact

| 面 | 影响 |
|---|---|
| 公开 API | `Component` **新增**两个只读访问器（`local_name()` / `module_type_name()`）· **删除** `hierarchical_name()` · **新增**宏 `CH_HIERARCHY_LAYER` |
| 生成产物 | 信号名**可能变化**（去掉重复前缀）⇒ Verilator 缓存 key 失效 ⇒ 一次冷编译 |
| 语义 | **零变更**（只改名字） |
| 回归面 | 命名断言 + 既有全量测试；**不需要**双后端 parity / 产物逐字节比对 |
