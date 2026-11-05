# 设计：等价性验证（`hierarchical-rtl-equivalence-2026-11`）

> 决策在 ADR-058 §2.2 / §5（效果判据）· ADR-061 §5；本文件承载**验证策略**。
## 6. 验证策略

| 层次 | 判据 |
|---|---|
| **最强 · 零成本** | **默认（全 inline）** 产物与扁平产物**逐字节一致** ⇒ **允许差异 = 空集**（`cmp` 退出码 0） |
| **语义（承重）** | 映射表声明的观测集合 `O`（**非空** + **至少一个随激励变化**）上 `flat[O] ≡ hier[O]` **逐拍全等**；**顶层端口集合 MUST 按 ADR-058 §2.2 ③ 的两条精确判据**：① **输入端口集合 MUST 相等**（名字 + 宽度逐项配对）② **输出端口集合 MUST 等于「精确期望集」**（flat 输出集 − {`ch_out` 的**最近成层祖先 ≠ 根**} 的全部节点）。⚠️ **MUST NOT** 写成 `⊆`（丢端口正是子集方向 ⇒ 漏发一半也能过） |
| **判别力自证** | 故意破坏层次化（删连线 / 改隐式端口方向）⇒ 等价性测试 MUST **转红** ⇒ 还原复绿 |
| **结构** | 实例端口名 ∈ 目标 module 端口集合；宽度一致；产物通过 **ADR-060 §2.5 的 lint 门**（唯一权威：**不加 `-Wall`** · 捕获退出码 · stderr 无 `%Error` · `lint_off` 白名单 · **禁 `-Wno-fatal`**） |
| **粒度** | 类级 × 实例级三组合的成层结果与声明一致；`ForceInline` vs `ForceLayer` 逐拍等价 ⇒ ⚠️ **仅用于不含跨模块 `mem` 读的小 DUT**（见 §6.2 的矩阵裁决） |
| **归属** | 归属 ↔ 语义分流回归（`type_input` 分类 + `io.h` 四处守卫触发面**逐项一致**） |
| **确定性** | 同一设计两次生成**字节相同** |
| **零回归** | `ctest -LE perf` · `run_all_samples.sh` · `run_all_ported_tests.sh` 全绿 · 计数锚点同步 |

⚠️ **等价性测试的载体**：**backend 只增不改**的 `initialize_with_verilog(...)`
⇒ ⇒ 不另起「自建多文件 harness」。

⚠️⚠️ **但这个「只增不改入口」远不止加一个函数**（oracle R13）：
① `build_port_access_table` 从 **ctx 全量**推端口表，按 **node id** 取 `data_map_`、
按 **Vtop 字段名**取端口（`src/core/verilator_backend.cpp:594-660`）⇒ 层次化下子组件输出
**不再是顶层端口** ⇒ 生成的 `case` 引用不存在的字段；
② `emit_sim_main_postlude` 同样从 `ctx->get_eval_list()` 全量生成（`:193-231`）；
③ `invoke_verilator` 编译**单文件** `top.v`（`:490-497`），缓存 key 只哈希**一份** source。
⇒ ⇒ 新入口 MUST 接收**显式端口集合**（`node_id` + 顶层端口名 + 宽度 + 方向）
⇒ ⇒ ⇒ **工作量重估：~4 天 → ~7–9 天**（含 **E1** 显式端口集合 + **E2** 物理隔离双 work_dir +
**E3** 致命失败语义 + 顶层名 fail-early；⚠️ **不含**多文件编译 —— 产物形态是**单文件多 module**，
ADR-058 v2.10 已封死多文件模式）。
> ⚠️ **2026-11-06 实施期发现**：「顶层名 fail-early」的**前提不可复现** —— 实测根 module 名 ≠ `top`
> 也能被新入口正常仿真（值判据 0x2A 通过），编译期失败的真因是**输入文件名**（Verilator prefix =
> 首个输入文件名主干 ⇒ 入口复制成 `top.v` 已天然满足）⇒ ⇒ 该项**待 architect 裁决**，
> 本 change 暂不实现（证据与两个选项：`.rddf/reviews/hierarchical-rtl-equivalence-2026-11-discovery.md`）。

### 6.2 三条硬约束（ADR-058 v2.14 §2.3 E1–E3）与 DUT × 粒度矩阵

| # | 约束 | 本 change 的落地形态 | 判据（含负向对照） |
|---|---|---|---|
| **E1** | 显式端口集合 **⊇ {clock, reset}**，且 MUST 由 **emitter 产物派生**（映射表 ∪ `port_provenance`），**MUST NOT** 由 harness / backend 独立撰写 | 入口签名收集合；`build_port_access_table` / `emit_sim_main_postlude` 只走该集合 | 集合漏 `clock`/`reset` ⇒ 寄存器冻结 ⇒「随激励变化」判据**必红** |
| **E2** | 与既有 `initialize(ctx, …)` **物理隔离**，harness 用**两个独立 work_dir** | 共享 `verilator_work_dir_` 会互踩 `top.v` / `sim_main.cpp` / `obj_dir` | 共用 work_dir ⇒ 产物互踩（**不是**静默通过） |
| **E3** | 失败 MUST **致命**（返回 `VerilatorError`），**MUST NOT** 沿用 best-effort `CHWARN`；harness MUST 断言 backend 真初始化成功 | 编译 / `dlopen` 失败 ⇒ 用例**失败** | **模拟一次编译失败 ⇒ harness 必红**（**MUST NOT** SKIP） |

⚠️ **DUT × 粒度矩阵的裁决**（防实施者临场发明）：

| DUT | 粒度 | 为什么 |
|---|---|---|
| 两层 / 三层+兄弟连线 / bundle 成员跨层 | `ForceInline` **与** `ForceLayer` **都跑** | 无跨模块 `mem` ⇒ 两种极端粒度都合法 |
| **含 `ch_mem`** | **只跑 `RespectDeclarations`** | ForceLayer 会把 mem 持有者成层 ⇒ 父层读子 `ch_mem` 被**合法拒绝** ⇒ 该变体**无可执行对象** |
| **真实最深设计** | **只跑 `RespectDeclarations`**（设计里**显式声明** 2–3 个流水级成层） | 同上；且深设计的价值是「多层 + 隐式端口 + pass-through」的**真实**组合，不是粒度对照 |

⚠️ **深设计的选型约束**：裁剪版 RV32I 核（`ch_reg` + imem/dmem + ≥2 个**声明成层**的流水级），
**逐顶层端口核宽度 ≤ 64**（`>64` ⇒ backend 合法拒绝 ⇒ 判据空转）；不要求跑通完整固件，
只要求**寄存器状态可观测且随激励变化**。

### 6.1 三条判据的**可执行形态**（critic 第 1 轮 SF 收口）

| 判据 | 可执行定义 | 反「自证」措施 |
|---|---|---|
| **「逐字节一致」= 允许差异** **空集** | `cmp <flat.v> <hier_forceinline.v>` 退出码 **0**（⇒ 全 inline 只产 1 个文件，其内容 MUST 与 `toVerilog()` 相同） | 不写「注释不算」—— ⇒ 任何差异即红；若实现**必须**产 banner ⇒ 先改 ADR |
| **结构同构** | 「RTL module 树」**独立提取**：**`verilator --json-only`**（`.tree.json` + `.tree.meta.json`，`bin/verilator:446`）或 `--dump-tree` ⇒ ⚠️ **`--xml-only` 已作废**（仓内 verilator **5.048** 已删除，`Changes:228`）⇒ 提取器形态以 §6.2 的 spike 结论为准 | **MUST NOT** 复用分析层的 `ModuleDef`（⇒ 否则分析错则两边一起错，「树形错但计数对」永远测不出） |
| **映射表 + 观测集合** | 映射表由 emitter 产出（**四元组**：`node_id` → 名字 → 扁平位置 → 层次化位置集合，含 `hier_top_port_name`）；`O` 由测试**手写声明**（≥1 个随激励变化的信号） | `O` 里若含恒 0 信号 ⇒ 测试 MUST 有「该信号确实变化过」的断言 |
| **端口集合** | ① **输入集合 MUST 相等**（名字+宽度逐项配对）；② **输出集合 MUST 等于精确期望集**（flat 输出集 − {`ch_out` 的**最近成层祖先 ≠ 根**} 的全部节点，**含成层子树里被 inline 的后代**） | ⚠️ **不用 `⊆`**（丢端口正是子集方向 ⇒ 漏发一半也能过） |

---

### 风险（自 umbrella §7 分派）

| # | 风险 | 对策 |
|---|---|---|
| 4 | 「逐字节一致」被「这算注释差异」平凡满足 | **允许差异 = 空集**（`cmp` 退出码 0）⇒ 要放宽 MUST 先改 ADR |
