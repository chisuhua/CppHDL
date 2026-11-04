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
- [ ] 悬空冒泡四判据（逐字同名 + 注释 + `CHWARN` + `strict` 抛 + 顶层可驱动）
- [ ] `owner == nullptr` 归顶层 + `CHWARN`（clock/reset **不告警**）
- [ ] 已 build 未 `add_child` ⇒ 抛 + `add_child` 修法 + **负向对照**
- [ ] 自驱动 `ch_in` ⇒ 抛；子 module 内 MUST NOT `assign <ch_in> = ...`
- [ ] 父 module 为子 `ch_out` 声明连接网（接 output 的实参 MUST 是已声明 `logic`；
      接 input 可表达式 ⇒ MUST NOT 强制造网）
- [ ] **MUST NOT 复用** module；同名不同类型**响亮拒绝**
- [ ] **接口白名单**违反 ⇒ `CHFATAL_EXCEPTION` + 点名端口
- [ ] 三层设计产出 **3 个 module + 2 条实例化语句**；结构同构（**独立提取**，
      MUST NOT 复用分析层的 `ModuleDef`）
- [ ] 错误模型：校验先于 I/O · 临时目录 → rename · 校验 MUST NOT 落在 `catch(...)` 内 ·
      静态析构期返回码兜底
- [ ] **零回归**：`ctest -LE perf` / samples / examples 全绿 · 计数锚点同步 ·
      新增护栏 `scripts/verify_hierarchical_rtl.sh` 挂 CI **两处**

## Capabilities

| capability | delta |
|---|---|
| `rtl-hierarchical-output` | **ADDED ×7**（R2 成层组件形成独立 module · R5 隐式端口按方向 · R6 悬空子输入冒泡 · R10 时序模块 clock/reset · R11 端口覆盖 + 可追溯 · R12 体内不引用非自己/非祖先端口节点 · R14 接口类型白名单） |

⇒ 归档**禁** `--skip-specs`。
