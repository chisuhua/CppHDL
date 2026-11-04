# Proposal: 层次化 RTL 输出 · 第 6 段 —— 层次发射（`hierarchical-rtl-emission-2026-11`）

## Why

「输出**层次化** Verilog」当前是**框架零支持**：唯一 RTL 出口
`toVerilog(filename, context*)`（`include/codegen_verilog.h:36`）固定输出**一个**
`module top`（`src/codegen_verilog.cpp:301, 409`），writer 内**零** `children()` 遍历、
**零**实例化语句发射。

⇒ ⇒ 现状下想要层次化 RTL，只能「每层手写 wrapper + 手写顶层例化」⇒ ⇒
**手工量随层次深度线性增长**，且每处手工连线都是一次可错的机会。

## What Changes

1. **module 主体**：每个成层组件产出一个 `module`（名字 = `<module_type_name>_<序号>`，
   序号按**实例路径**排序 ⇒ ⇒ 确定性）。
2. **实例化语句**：父 module 内对子 module 例化，端口映射走**实例端口映射**
   ⇒ ⇒ **零跨 module `assign`**。
3. **隐式端口（按方向）**：ADR-060 §2.1 的方向矩阵逐行实现（含第三轴 pass-through）。
4. **悬空冒泡**：ADR-060 §2.2（名字与扁平**逐字相同** + 注释 + `CHWARN` +
   `strict` ⇒ `CHFATAL_EXCEPTION`）。
5. **clock / reset 贯通**：根 module **无条件**发；非根按 `has_seq`；
   ⚠️ **clock 与 reset MUST 分开判**（mem 写端口的 `always_ff` **无 reset 分支**
   ⇒ 纯 RAM 写 module MUST **只发 clock**）。
6. **MUST NOT 复用 module 定义**：同类型多实例**各发一份**；同名不同类型**响亮拒绝**。
7. **接口白名单**（ADR-062 §2.6）：成层单元接口 MUST 限于 `ch_uint<N>` / `ch_bool`
   ⇒ ⇒ 违反 ⇒ `CHFATAL_EXCEPTION` + 点名端口。
8. **跨模块 `ch_mem`** ⇒ **响亮拒绝**（判定口径 = **inline 折叠后**的 module 归属）。
9. **独立公开入口** `toVerilogHierarchical(...)`（既有 `toVerilog()` 路径**逐字节不变**）。
10. **命名权威 = ctx 级全局表**：MUST NOT 发明 per-module 局部命名 ⇒ ⇒ 顶层端口名
    MUST 与扁平产物**逐字相同**。

## 不做（Non-Goals）

- **不**做等价性验证 ⇒ ⇒ 那是 T3-⑤。
- **不**改 `toVerilog()` 的既有路径（**新入口** · ADR-058 §2.3）。
- **不**复用 module 定义（同类型多实例各发一份）。
- **不**用「按端口数 / 节点数自动推断」成层。

## Acceptance

> ⚠️ 判据两条纪律：① 「逐字节一致」MUST 是**可机械判定**的（`cmp` 退出码）；
> ② 每条判据 MUST 有**判别性对照**（mutation ⇒ 必须转红）。

- [ ] **端口覆盖 = `⊇` + 五类来源**（⚠️ **MUST NOT** 写成「等于」· ADR-062 §5）：
      **成层且非根** module 的端口集合 MUST **⊇** 自身 `ch_in` ∪ `ch_out`
      ⇒ 每个端口可追溯到五类之一（自身 / inline 后代 `ch_out`（**仅根**）/ 隐式 / 冒泡 /
      clock·reset）
- [ ] **RTL 顶层 module 端口表 MUST 等于扁平端口表**（**逐字逐序**：
      `clock → reset → 无 driver 的 input → 全部 ch_out`）
- [ ] **输入集合 MUST 相等**（名字 + 宽度逐项配对）；**输出集合 MUST 等于精确期望集**
- [ ] **零跨 module `assign`**；实例端口**宽度一致**；实例端口名 ∈ 目标 module 端口集
- [ ] **inline 不产生隐式端口** ⇒ **mutation**：在 inline 路径强制合成隐式端口 ⇒ 必转红
- [ ] **clock / reset 分开判**（两条失败模式各一判据：漏写端口 ⇒ lint 报错；
      一元 `has_seq` ⇒ 纯 RAM 写 module 得到**永不使用的** reset 端口 ⇒ 转红）
- [ ] 四方向矩阵**逐行**有判据；第三轴 pass-through 有判据
- [ ] 悬空冒泡五判据（逐字同名 + 注释 + `CHWARN` + `strict` 抛 + 顶层可驱动）
      ⇒ ⇒ ⚠️ **成层组件自己的悬空 `ch_in` 也 MUST 冒泡**（不只 inline 组件的）
      ⇒ ⇒ 否则层次化产物里它只在子 module 上 ⇒ ⇒ **顶层无法驱动** ⇒ ⇒ 与扁平行为分叉
- [ ] `owner == nullptr` 归顶层 + `CHWARN`（clock/reset **不告警**）
- [ ] 已 build 未 `add_child` ⇒ 抛 + `add_child` 修法 + **负向对照**
- [ ] 自驱动 `ch_in` ⇒ 抛；子 module 内 MUST NOT `assign <ch_in> = ...`
- [ ] 父 module 为子 `ch_out` 声明连接网（接 output 的实参 MUST 是已声明 `logic`；
      接 input 可表达式 ⇒ MUST NOT 强制造网）
- [ ] **MUST NOT 复用** module；同名不同类型**响亮拒绝**
- [ ] **接口白名单**违反 ⇒ `CHFATAL_EXCEPTION` + 点名端口
- [ ] 三层设计产出 **3 个 module + 2 条实例化语句**；结构同构（**独立提取**，
      MUST NOT 复用分析层的 `ModuleDef`）
- [ ] 错误模型：校验先于 I/O · 临时**文件** → rename（失败路径 MUST NOT 触碰既有同名文件）·
      校验 MUST NOT 落在 `catch(...)` 内 · 静态析构期返回码兜底
- [ ] **产物形态 = 单文件多 module**（ADR-058 v2.9）⇒ ⇒ 选项结构 MUST NOT 出现
      `out_dir` / `one_file_per_module`（Zero-Debt ⇒ 不留未实现字段）
- [ ] **策略枚举单一权威**：`ch::PortPolicy` / `ch::UndrivenPolicy`（ADR-060 §2.4）
      ⇒ ⇒ MUST NOT 存在第二份副本（如 `HierarchyPortPolicy`）
- [ ] **`Component::set_granularity()` 已删除**（ADR-061 v1.6.3）⇒ ⇒ 粒度的**唯一入口** = RAII
      `Component::GranularityScope` ⇒ ⇒ 选项结构 MUST NOT 有 `granularity` 字段
- [ ] **粒度的判别性判据**（ADR-061 v1.6.3）：scope **只**包构造、发射在 scope 之外
      ⇒ ⇒ 产物 MUST = `RespectDeclarations` 形态（**不是** `ForceInline` 形态）
      ⇒ ⇒ ⚠️ 用例 MUST **含类级/实例级声明**（否则两形态恒等 ⇒ 判据退化为恒真）
- [ ] **粒度的正向判据**（与上一条成对）：scope **同时覆盖构造与发射** + 粒度 = `ForceLayer`
      ⇒ ⇒ 产物 MUST 把每个组件都发射成独立 module（⇒ ⇒ 「作用域必须覆盖两段」的**正向**证据）
- [ ] **`SynthesizeFatal` 的语义**（ADR-060 §2.4.1）：先合成完再失败 · 不产出文件 ·
      诊断 MUST 含**全部**被合成的边与合成端口名 ⇒ ⇒ **「合成」MUST 是纯内存 phase ·
      先于任何文件 I/O** ⇒ ⇒ 失败时无临时文件、不触碰既有文件
- [ ] **分析层新增字段 MUST NOT 进 `to_text()`** ⇒ ⇒ T3-② 的 4 份 golden **逐字节不变**
      ⇒ ⇒ 归档 change 的判据不被削弱（新增信息由本 change 的测试覆盖）
- [ ] **护栏 R7**：层次化产物路径 MUST ≠ 既有扁平产物路径（ADR-058 v2.9 的「同名不同物」消解
      ⇒ ⇒ API 侧无自卫 ⇒ ⇒ 由护栏 + 用法指南承担）
      ⇒ ⇒ ⚠️ **适用面 = 仓内调用点**（对每个 `toVerilogHierarchical(` 的 path 实参 ⇒ ⇒ 断言其 ≠
      同目录任何 `toVerilog(` 实参）⇒ ⇒ MUST NOT 写成「扫描工作目录所有 `.v`」（⇒ 误报机器）
- [ ] **隐式端口的告警与注释**（ADR-060 §2.1 + §2.4.1）：**发生合成时** MUST 对**每一条**被合成的边
      打 `CHWARN`（点名该边 + 合成端口名）+ 产物内写注释 ⇒ ⇒ **未发生合成时产物 MUST NOT 含任何
      合成注释** ⇒ ⇒ 「全 inline ⇒ 与扁平逐字节一致」判据**仍成立**
- [ ] **端口清单**：产物 MUST 随附**端口清单**，覆盖**全部非顶层 module 的合成端口名**（名 + 方向 + 宽度）
      ⇒ ⇒ 载体 = `HierarchyEmitResult`（⇒ ⇒ 分析层 `to_text()` 与既有字段的顺序/命名**零改动**
      ⇒ ⇒ T3-② 的 4 份 golden + 87 条断言不受影响）
- [ ] **兄弟边专属用例**：子 A 的 `ch_out` 直连子 B 的 `ch_in` ⇒ ⇒ 父 module MUST 有**一根**连接网
      + **两处**实例化各连一次 + **零**隐式端口 ⇒ ⇒ 「input 侧实参可以是表达式 ⇒ 不造网」这条
      互斥形态 MUST 有判别性用例
- [ ] **零回归**：`ctest -LE perf` / samples / examples 全绿 · 计数锚点同步 ·
      新增护栏 `scripts/verify_hierarchical_rtl.sh` 挂 CI **两处**

## Capabilities

| capability | delta |
|---|---|
| `rtl-hierarchical-output` | **ADDED ×11**（成层组件形成独立 module · **命名权威 = ctx 级全局表** · 隐式端口按方向 · 悬空子输入冒泡 · 时序模块 clock/reset · 端口覆盖 + 可追溯 · 体内不引用非自己/非祖先端口节点 · **独立公开入口 + 既有路径逐字节不变** · 接口类型白名单 · **单文件多 module + 顶层名可配置 + 路径不得与扁平同名** · **端口策略单一权威（线性枚举）+ 各取值的失败语义** · **层级粒度由全局唯一权威决定 + 作用域覆盖构造与发射**）|

⇒ 归档**禁** `--skip-specs`。
