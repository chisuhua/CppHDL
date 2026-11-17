## 2026-11-17 — **顶层端口面收敛 + 冒泡机制退役**（ADR-072 D1-f / D1-h）

**问题**：manifest 声称的顶层端口表与产物**不自洽**（`is_top_io: true` = 32，产物顶层 = 130
⇒ ⇒ 98 个顶层端口是后代信号）；且「悬空后代输入冒泡成顶层 input」把**漏接线**变成看起来
合法的新顶层端口（编号随 eval-list 序漂移、无语义）。

**改了什么**（`rtl-productization-surface-2026-11` **T15 + T6**）
- **后代 `ch_out` 不再上任何 module 的端口表**（含根）⇒ 一律体内 `logic`
- **悬空后代输入 MUST 被响亮拒绝**：`ok = false` + `error` 点名（端口名 + module + 层级），
  `files` 为空；**根自己**的悬空 `ch_in` 不受影响（它就是顶层 `input`）
- 物理删除：`UndrivenPolicy::Bubble`（只剩 `Reject`）· `HierarchyPortOrigin::Bubbled/InlineFolded` ·
  `PortProvenanceOrigin::Bubbled/InlineFolded` · `BubbledInput` / `bubbled_inputs` ·
  `ensure_bubbled_port` · `dangling` 向量（公开面登记见 `.rddf/checks/public-api-surface.md`）
- 用户可见：**RV32I 分层产物顶层 34 端口**（= `default_clock` / `default_reset` + 4 `ch_in` +
  28 `ch_out`，与 `AGENTS.md` 手推锚一致）；ctx-only 遗留扁平产物**逐字节不变**（130）
- 护栏同步：`flat_rtl_emit_check.py` 的 F3 改为**只消费** manifest 的 `port_table[]`（逐字逐序）·
  `verify_hierarchical_rtl.sh` R6/R8 期望集重算（冒泡靶子换成「同层两兄弟」正向设计）

## 2026-11-17 — **manifest schema v1 → v2**（新增产品端口表 `port_table[]`）

**背景**：v1 只有 `is_top_io`（语义 = 根组件自有 IO）⇒ ⇒ 消费方**无法**据此复原产物端口面。

**改了什么**（`rtl-productization-surface-2026-11` **T4**）
- manifest 新增**顶层数组** `port_table: [{name, dir, width, role}]`（**按产物顺序**）
- `schema` → `rtl-emit-manifest-v2`；`ports[]` **仍是全量**（10 字段不变）
- **端口面的唯一权威 = `port_table[]`**；`is_top_io` 退为**消费方筛选信号**
- 产物卫生头里的元数据同步为 `schema=rtl-emit-manifest-v2`

## 2026-11-17 — **新增带根扁平发射入口**（`toVerilog(path, Component&, EmitOptions)`）

**背景**：RTL 生成能力的**产品面**要求「**manifest 声称的端口表 == 产物的端口表**」。
既有扁平入口只收 `ctx` ⇒ ⇒ 拿不到「根组件」⇒ ⇒ 端口面无法收敛到**根组件自有 `__io`**
（`VerilatorBackend` 同理拿不到 root ⇒ ⇒ 它继续走遗留路径）。

**改了什么**（`rtl-productization-surface-2026-11` **T2**）
- 新增 `ch::toVerilog(const std::string &, Component &, const EmitOptions &)` —— **带根**产品入口
- 新增 `ch::make_flat_module_view(Component &, bool)` —— 端口选择 = `default_clock` →
  `default_reset` → **根组件自有**（`node->get_parent() == &root`）的无 driver `ch_in` / 全部 `ch_out`
- `verilogwriter` 新增带根构造

⚠️ **只增不改**：既有 `toVerilog(path, ctx)` / `toVerilog(path, ctx, options)` 的**签名与产物
逐字节不变**（49 处调用点零改动）⇒ ⇒ 下游既有代码**零动作**；需要产品端口面时改用带根重载。

**实测（手推对照）**：`root{a,q} ⊃ leaf{din,dout}` ⇒ ⇒ ctx-only = **5** 端口
（含后代 `top_leaf_dout`）· 带根 = **4** 端口（`default_clock` / `default_reset` / `top_a` /
`top_q`）⇒ ⇒ 后代 `ch_out` 降为体内 `logic`；`verilator --lint-only --top-module top` **exit 0**。

