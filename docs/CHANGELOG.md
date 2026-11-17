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

