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
   - `module_type_name()`（**模块类型名**，运行期由类级字面量或 `typeid` 回落解析）；
   - `name()` 语义**不变**（仍是**全路径**）⇒ ⇒ **单一命名真源**：`name()` = 全路径、
     `local_name()` = 局部。
   ⚠️ **构造函数入参语义收紧（实施期定稿 · 见 design.md §2.1.1）**：构造函数的 `name`
   参数恒为**局部名**，**全路径由构造函数自己算** ⇒ ⇒ `name()` 对**所有**组件恒为全路径
   ⇒ ⇒ 派生组件的 ctor 签名**零变更**。
3. **合并宏** `CH_HIERARCHY_LAYER("name")`：**声明 + 命名一体** ⇒ ⇒
   「成层却忘了命名」这个组合**编译期即不可表示**（ADR-059 §2.2.1 第 ① 档）。
4. **删除 `Component::hierarchical_name()`**：它的唯一消费者在 (1) 里被替换 ⇒ ⇒
   留着 = 两个真源 = 前缀重复会**再回来** ⇒ ⇒ ZERO-DEBT「无死代码」。
5. **`build_hierarchical_name()` 从 `include/module.h` 移到 `include/component.h`**：
   全路径拼接的**唯一**真源 MUST 与「计算它的那一处」同文件 ⇒ ⇒ `module.h` 经
   `#include "component.h"` 继续对外可见（签名与语义不变）⇒ ⇒ D3 的判据仍然成立。

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
- [ ] **命名权威 = ctx 级全局表**（契约载体在 **T3-④**）：层次化 MUST NOT 发明
      per-module 局部命名 ⇒ ⇒ 顶层端口名 MUST 与扁平产物**逐字相同**
- [ ] **合并宏**：`CH_HIERARCHY_LAYER("name")` 一次给出「成层 + 模块类型名」
      ⇒ ⇒ 缺名**编译期不可表达**（MUST NOT 推迟到发射时的运行期硬错）
- [ ] **单一真源**：`Component::hierarchical_name()` MUST NOT 仍存在（否则两个真源 ⇒ 前缀重复复发）
- [ ] **零语义变更**：仿真结果**逐位不变**；`ctest -LE perf` / `run_all_samples.sh` /
      `run_all_ported_tests.sh` 全绿；计数锚点同步
      ⚠️ **唯一的语义面变化（已登记）**：`create_child` 子组件的 `name()` 由**局部名**
      变为**全路径**（`"child1"` → `"top.child1"`）⇒ ⇒ 这是 R3「`name()` 恒为全路径」
      的直接结果 ⇒ ⇒ 既有断言 `tests/test_multithread.cpp:255-256` 钉的是**不一致的旧
      语义** ⇒ ⇒ 按**正确语义**更新 + **补 `local_name()` 断言**（旧行为未消失，
      只是挪到了 `local_name()`）
- [ ] **文本变化面逐条登记**（信号名会变 ⇒ 至少覆盖 ADR-059 §5 列出的 4 处实测依赖 +
      本仓 trace 输出）

## Capabilities

| capability | delta |
|---|---|
| `component-ownership-and-naming` | **ADDED ×2**（R2 组件实例路径 MUST 无重复前缀 · R3 两个命名维度） |
| `rtl-hierarchical-output` | **无**（命名权威是 emitter 级属性 ⇒ 归 ④） |

⇒ 归档**禁** `--skip-specs`。

⚠️ **为什么 `rtl-hierarchical-output` 的 delta 为空**：「命名权威 MUST 是 ctx 级全局表」
与「层次化 MUST 有独立的公开入口」都是 **emitter 级**属性（顶层端口名 / 新入口）
⇒ ⇒ 在 ①a（**零语义变更**、无 emitter）**不可观测** ⇒ ⇒ 放进 ①a 会让该 change
**无法自证自己的判据** ⇒ ⇒ 两条均归 **T3-④**（emission）。
⇒ ⇒ 与「模块类型名四档」归 ①c 是**同型**处理。

## Impact

| 面 | 影响 |
|---|---|
| 公开 API | `Component` **新增**两个只读访问器（`local_name()` / `module_type_name()`）· **删除** `hierarchical_name()` · **新增**宏 `CH_HIERARCHY_LAYER` · `build_hierarchical_name()` **换文件**（`module.h` → `component.h`，对外可见性不变） |
| 生成产物 | 信号名**可能变化**（去掉重复前缀）⇒ Verilator 缓存 key 失效 ⇒ 一次冷编译 |
| 语义 | **零变更**（只改名字） |
| 回归面 | 命名断言 + 既有全量测试；**不需要**双后端 parity / 产物逐字节比对 |
| 新增交付物 | `tests/test_component_naming.cpp`（**1 个** ctest 条目）· `tests/fixtures/component_naming/`（3 个编译期 fixture）· `scripts/verify_component_naming.sh`（R0 fail-closed · R1 正向 · R2/R3 编译期反向 · R4 源码级 · `--self-test` 中和自证）+ CI **两处** |
