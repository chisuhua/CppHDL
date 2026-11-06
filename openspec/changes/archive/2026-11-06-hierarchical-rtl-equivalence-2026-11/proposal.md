# Proposal: 层次化 RTL 输出 · 第 7 段 —— 等价性验证（`hierarchical-rtl-equivalence-2026-11`）

## Why

本仓目前**完全没有**「两份 Verilog 逐拍对比」的设施：`DUAL_BACKEND_TEST_PAIR` 比的是
「C++ 模拟器 vs 扁平 Verilog」，**不是** Verilog A vs B
⇒ ⇒ 「层次化产物与扁平产物等价」这条**最关键**的承诺**零证据**。

⚠️ **不能用「parity」代替**：两后端由**同一份** C++ 生成 ⇒ ⇒ **同构缺陷**会被
parity **盖章**（两后端一致地错）⇒ ⇒ 只有「手推期望值 + 逐拍比对」能抓。

## What Changes

1. **backend 只增不改入口**：给 Verilator backend 加 `initialize_with_verilog(path, data_map)`
   ⇒ ⇒ 既有 `initialize(ctx, …)` 行为**逐字节不变**（ADR-058 §2.3）。
2. **映射表**：emitter 产出**四元组**（`node_id` → 设计级信号名 → 扁平位置 →
   层次化位置集合，含 `hier_top_port_name`）。
3. **≥5 个 DUT 的逐拍等价**：观测集合 `O`（**非空** + **至少一个随激励变化**）上
   `flat[O] ≡ hier[O]` 逐拍全等。
4. **确定性**：同一设计两次生成**字节相同**（限定「**同编译器 / 同 ABI 内**」）。

⚠️ **工作量重估**（oracle R13 实测）：这个「只增不改入口」**远不止加一个函数** ——
`build_port_access_table`（`src/core/verilator_backend.cpp:594-660`）从 ctx 全量推端口表、
按 **node id** 取 `data_map_`、按 **Vtop 字段名**取端口 ⇒ 层次化下子组件输出**不再是顶层端口**
⇒ 生成的 `case` 引用不存在的字段；`emit_sim_main_postlude`（`:193-231`）同样全量生成；
`invoke_verilator`（`:490-497`）编译**单文件**且缓存 key 只哈希**一份** source
⇒ ⇒ 新入口 MUST 接收**显式端口集合** ⇒ ⇒ ⇒ **~7–9 天**（不是「加一个函数」）。

## 不做（Non-Goals）

- **不**改既有 backend 入口的行为 ⇒ ⇒ 「只增不改」。
- **不**做「层次友好度诊断 API」（feature 的 **P6** · 待用户拍板）。
- **不**把等价性 harness 做成**公共 helper**（feature 的 **P8** · 待用户拍板）
  ⇒ ⇒ 默认**不做**（MUST NOT 静默塞进本 change）。

## Acceptance

- [ ] **默认（全 inline）** 的层次化产物与扁平产物**逐字节一致** ⇒
      **允许差异 = 空集**（`cmp` 退出码 **0**）
- [ ] **5 个 DUT 的清单 MUST 锁定** = 两层 / 三层+兄弟连线 / **bundle 成员跨层** /
      **含 `ch_mem`** / 真实最深设计 ⇒ ⇒ MUST NOT 用 5 个 trivial 两层组合凑数
- [ ] **观测集合 `O` 的下限**（⚠️ **口径已按实施后审查订正** ⇒ 与 delta 的
      Requirement ④/⑤ 逐字对齐）：
      - `O` MUST ⊆ **该次运行粒度下可见**的 `ch_out` 集（可见 = 映射表里
        `hier_top_port_name` **非空**）⇒ ⇒ 不可观测者 MUST NOT 被选入 `O`
      - **单粒度**比较 ⇒ `O` MUST 覆盖该粒度下**全部可见**的 `ch_out`
        （盲区 = 该粒度下不可见者 ⇒ MUST 以**对偶断言**机械登记）
      - **跨粒度**比较（同一设计两种粒度）⇒ `O` MUST 取**公共子集**
        ⇒ ⇒ 「可见但未观测」者 MUST 进**显式豁免清单**（机械载体 · MUST NOT 仅注释）
        ⇒ ⇒ 该次比较 MUST NOT 声称「全部可见输出已比对」
- [ ] **映射表覆盖被观测的全部信号**（MUST 是**四元组**）⇒ ⇒ 且被观测信号 MUST
      `node_id` ∈ 映射表 ∧ `hier_top_port_name` 非空
- [ ] **「至少一个随激励变化」的机械判据**：存在一个信号在**两组不同激励**下取值集合**不同**
- [ ] **ForceInline vs ForceLayer 逐拍全等** ⇒ 激励 MUST 是**多拍序列**且 DUT MUST
      **至少含一个寄存器**
- [ ] **同一 `node_id` 在两条路径的名字 MUST 逐字相同**（用映射表的 `node_id` 索引逐项比对；
      MUST NOT 只比「名字集合交集」）
- [ ] **判别力自证 ≥2 形态**（删一条连线 · 改一个隐式端口方向）⇒ 两红 ⇒ 还原复绿
- [ ] 确定性（同 ABI 内两次生成字节相同）；产物通过 **ADR-060 §2.5 的 lint 门**
      （⚠️ **不加 `-Wall`**、**禁 `-Wno-fatal`** · 退出码 + 无 `%Error` + `lint_off` 白名单）
- [ ] **既有 `initialize(ctx, …)` 行为逐字节不变**
- [ ] **零回归**：`ctest -LE perf` / samples / examples 全绿 · 计数锚点按新增用例**同步更新**
      （⚠️ 等价性用例带 `verilator_e2e` 标签 ⇒ `VERILATOR_E2E_TOTAL` 必变；parity 两锚点不变）

## Capabilities

| capability | delta |
|---|---|
| `rtl-hierarchical-output` | **ADDED ×2**（默认（全 inline）产物 MUST 与扁平产物逐字节一致 · 层次化产物 MUST 与扁平产物等价（可机械判定））+ **MODIFIED ×5**（`-Wall` · 「抛异常」×4 · `strict` ×2 的措辞与实现对齐，见 delta 文件） |

⇒ 归档**禁** `--skip-specs`。
