# Proposal: 层次化 RTL 输出 · 第 3 段 —— 层级粒度声明（`hierarchical-rtl-granularity-declaration-2026-11`）

## Why

层次化输出里「哪些组件成为独立 Verilog module」**必须由用户显式决定**：
默认把所有组件都成层 ⇒ ⇒ 产物是**例化清单的噪声**（小组合子全部成层）；
默认全部 inline ⇒ ⇒ 需要成层的 IP 单元无法单独复用 / 交付 / 综合
⇒ ⇒ ⇒ **默认 inline，成层须显式声明**（ADR-061）。

⚠️ **不能用启发式替用户决定**（「按端口数 / 节点数自动推断」）⇒ ⇒ 不可预测 ⇒ ⇒
用户无法预测产物 ⇒ ⇒ 平台替用户猜 = 本仓反复吃过的亏。

⚠️ **粒度声明与装配点 MUST 解耦**（下游 N2）：下游 ChipForge 的桥接 plugin 在
`setup()` 里 `make_unique<Unit>()` + `build(ctx)` ⇒ ⇒ **没有 active parent**
⇒ ⇒ 成层声明**不**得以「有父组件」为前提。

## What Changes

1. **两个新 API 名**（**不动 `ch_module` 签名**）：`ch_layer<T>`（显式成层）·
   `ch_inline<T>`（显式 inline）。
   ⚠️ **为什么不用 tag 参数**：`ch_module` 的构造参数**转发给组件构造函数**
   ⇒ ⇒ tag 参数会要求**每个**组件的 ctor 都接受它 ⇒ ⇒ 破坏现有组件。
2. **`Component::is_layer()`**：运行期读取成层判定。
3. **模块类型名的四档来源**（ADR-059 §2.2.1）**②③④ 三档**在此落地：
   - ② `ch_layer<T>` · `T` **已**声明成层 ⇒ MUST **继承** `T` 的类级名字（MUST NOT 另给）
   - ③ `ch_layer<T>` · `T` **未**声明成层（helper 被提升）⇒ MUST **在实例处**给名
     （缺名 MUST 是**编译期**错误 ⇒ 需类级宏暴露**编译期可见**的名字 ⇒ 由 ①a 提供）
   - ④ `Granularity::ForceLayer`（用户未声明）⇒ 允许回退 `sanitize(typeid)` **+ `CHWARN`**
4. **`Granularity` 枚举**：`RespectDeclarations` / `ForceInline` / `ForceLayer`
   （③ 也含 `PortPolicy` / `UndrivenPolicy` 两个线性枚举与**默认值**，见 §10.6-C-㉓）。
5. **无 active parent 处的装配点**：成层判定与装配点**解耦**（ADR-061 §2.3）。

## 不做（Non-Goals）

- **不**实现 emitter（分析层 / 发射 / 等价性）⇒ ⇒ 那是 T3-②/③/④/⑤。
- **不**改归属真源 ⇒ ⇒ 那是 **T3-①b**。
- **不**定义类级宏 `CH_HIERARCHY_LAYER("name")` 本身 ⇒ ⇒ 那是 **T3-①a**（①a 落地第 ① 档）。
- **不**用「实例名派生成模块类型名」⇒ ⇒ 会破坏 ADR-060 §2.3 的「同类型 ⇒ 同一
  `<module_type_name>` 组」⇒ ⇒ 序号（按实例路径排序）失去分组依据。

## Acceptance

- [ ] 类级 / 实例级**三组合**的成层结果 MUST 与声明一致
- [ ] **无 active parent** 处装配的组件 MUST 仍能按声明成层
- [ ] `ch_module` 签名**零变更**
- [ ] **模块类型名的四档来源**可判定：① 类级（**①a** 落地）· ② 继承 · ③ 实例处给名
      （缺名**编译期**不可表达）· ④ `ForceLayer` 回退 + **`CHWARN`**
      ⇒ **判别性对照**：把 ③ 的缺名改成运行期抛 ⇒ **编译期 fixture 必转红**
- [ ] ② MUST NOT 另给名字（否则同类型分组失效）
- [ ] `Granularity::ForceInline` ⇒ 全部组件 inline；`ForceLayer` ⇒ 全部成层

## Capabilities

| capability | delta |
|---|---|
| `component-ownership-and-naming` | **ADDED ×1**（R4 模块类型名的四档来源） |
| `rtl-hierarchical-output` | **ADDED ×2**（R1 层级粒度由显式声明决定 · R8 成层判定与装配点解耦） |

⇒ 归档**禁** `--skip-specs`。
