# Discovery：`hierarchical-rtl-equivalence-2026-11` · 顶层名约束的前提不可复现

- 日期：2026-11-06
- 角色：builder（实施期发现）
- 分类：`design_drift`（需要改**已签批 ADR 的约束条款**才能继续 —— ADR-058 v2.13 §2.3 / §4 风险 7 / §5 判据）
- 状态：**待 architect 裁决**（本文件即交接凭据；`rddf feedback add` 不可用 —— 本 change 未经 `rddf-planner`
  立项，`.rddf/improvements/` 下无对应 improvement 文件）

## 1. 结论（一句话）

ADR-058 v2.13 的「**顶层 module 名 ≠ `top` ⇒ 编译期失败** ⇒ 入口 MUST fail-early 拒绝」**前提不成立**：
编译期失败的真因是**输入文件名**（Verilator 的 prefix 从文件名派生），而本实现的新入口**把产物复制成
`<work_dir>/top.v`** ⇒ 根 module 叫 `my_top`、`foo`、任何名字都**照常工作**（端到端实测正确）。
⇒ ⇒ 按现文实现 fail-early 会**拒绝本可工作的产物**，且该判据**没有任何失败模式可作判别力**。

## 2. 证据（全部可复算）

### ① 根 module 名 ≠ `top` 也能仿真（端到端）

spike（临时 TEST_CASE，已删除）：`BeTop`（2 层 · 叶子含 `ch_reg`）以
`HierarchyEmitOptions{top_module_name = "my_top"}` 发射 ⇒ 产物首行 `module my_top`；
`backend_port_set(emitted, "my_top")` → `try_initialize_with_verilog(path, ports, dm)`；
`sim.set_backend(...)` → `din = 0x2A` → `tick()` ×2 → **`dout == 0x2A`**（5 断言全过）。

```
# 复算（简化成纯 Verilog 版，等价于产物形态）
mkdir -p /tmp/vt && cd /tmp/vt
cat > top.v <<'EOF'
module my_top(input wire clk, output wire [7:0] y);
  sub u(.a(clk), .y(y));
endmodule
module sub(input wire a, output wire [7:0] y);
  assign y = a ? 8'd5 : 8'd0;
endmodule
EOF
verilator --cc top.v            # exit 0
grep -n 'VL_IN8\|VL_OUT8' obj_dir/Vtop.h   # 31: VL_IN8(&clk,...)  32: VL_OUT8(&y,7,0)
```

⇒ ⇒ Verilator **自行选取**「未被实例化的那个 module」为顶层（root 在文件里排第一 / 排最后都实测选中
`my_top`）· 生成的类名 / 头文件名 = **`V` + 输入文件名主干** = `Vtop` ⇒ 与 module 名**无关**。

### ② 真正的约束 = **输入文件名**（不是 module 名）

```
mkdir -p /tmp/vt2 && cd /tmp/vt2
printf 'module top(input wire clk, output wire [7:0] y);\n  assign y = clk ? 8%sd5 : 8%sd0;\nendmodule\n' "'" "'" > hier.v
verilator --cc hier.v           # exit 0
ls obj_dir | grep '\.h$'        # Vhier.h  ← 没有 Vtop.h
```

⇒ ⇒ `sim_main.cpp` 里硬编码 `#include "Vtop.h"` 会**找不到头文件** ⇒ 编译失败。
**这与根 module 叫什么完全无关**，只与 `verilator` 的第一个输入文件名有关。
依据（源码）：`third_party/verilator/src/V3Options.cpp:1152-1160` —— prefix 依次取
`--prefix` → `--top-module` → **首个输入文件名**的 `filenameNonDirExt`。

⇒ ⇒ 本实现的新入口**已天然满足**该不变量：把外部产物复制到 `<work_dir>/top.v` 再编译
（`src/core/verilator_backend.cpp::try_initialize_with_verilog` 的第 ② 步）。

### ③ 多顶层是**响亮**失败（不是静默错值）

```
# /tmp/vt3/top.v = 两个互不实例化的 module
verilator --cc top.v
# %Warning-MULTITOP: top.v:4:8: Multiple top level modules
# %Error: Exiting due to 1 warning(s)
```

⇒ ⇒ 「产物里有多个未实例化 module」这一真实风险由 Verilator **自己**报错（MULTITOP 默认 fatal），
**不需要**入口自建校验。

### ④ 补充：组合设计的根 module **也**无条件带 clock / reset

`src/hierarchy_verilog.cpp:935`：`SeqNeeds needs = m.is_root ? SeqNeeds{true, true} : seq_needs(m);`
⇒ ⇒ E1 的「集合 ⊇ {clock, reset}」对**所有**产物都成立（spike：纯组合 DUT 的派生集合 =
`din`/`dout`/`default_clock`/`default_reset`，clocks=1 resets=1）⇒ 该条**无缺口**，不必放宽。

## 3. 影响面（若采纳「按文件名订正」）

| 位置 | 现文 | 需改为 |
|---|---|---|
| ADR-058 §2.3 「顶层 module 名的约束」段 | 顶层 module 名 ≠ `top` ⇒ 编译期失败 ⇒ 入口 MUST 显式校验 + 致命拒绝 + 点名 | **编译输入文件名** MUST 是 `top.v`（class / 头文件名 = `V` + 文件名主干）；入口 MUST 以 `top.v` 作编译输入（当前实现 = 复制）· **MUST NOT** 落到编译期才发现 |
| ADR-058 §4 风险 7 | 「`top_module_name = "my_top"` ⇒ 该产物**无法**被 backend 仿真」 | **假** —— 实测可仿真 ⇒ 删除该风险行，或改为「产物文件主干名 ≠ `top` 时 backend 无法直接编译（本实现靠复制规避）」 |
| ADR-058 §5 「效果判据（顶层名 · v2.13）」 | 顶层 module 名 ≠ `top` ⇒ 入口致命拒绝 + 点名 | 改为：入口 MUST 把外部产物复制为 `<work_dir>/top.v` 后编译 ⇒ **变异**：改成直接用调用方路径（`hier.v`）⇒ 编译失败 ⇒ 转红 |
| `changes/.../tasks.md` 1.7 | 顶层名 fail-early（`root_module != "top"` ⇒ 拒绝 + 点名 + 不留文件） | 同上（按文件名） |
| `changes/.../design.md:28` | 「E3 致命失败语义 + 顶层名 fail-early」 | 同上 |
| roadmap B4 行 / P0-1 的工期理由 | 含「顶层名 fail-early」工作量 | 按订正后的判据调整（工作量小幅下调：**不**需要 module 名解析） |

⚠️ **ADR v2.13 的变更记录行不改**（历史记录）；订正以**新版本行**（v2.16）+ 条款正文体现。

## 4. 建议处置（给 architect 的两个选项）

- **选项 A（推荐 · 按实测订正）**：约束改成「编译输入文件名 MUST 是 `top.v`」，并把这件的承担方式写死为
  「入口复制」。判据可变异（直接用原路径 ⇒ `Vhier.h` 缺 ⇒ 编译失败 ⇒ 转红），且**不减少能力**
  （`top_module_name` 任意值都能被 backend 消费 —— 与 capability `rtl-hierarchical-output` 的
  Scenario「顶层 module 名可配置」**相容**）。
- **选项 B（保留保守契约）**：仍要求根 module 名 == `top`。代价：① 拒绝 spike 证明**可用**的产物；
  ② 该 fail-early 判据**无失败模式**支撑（只能断言「我们拒绝了」，无法证明「不拒绝会坏」）；
  ③ 与「`top_module_name` 可配置」在语义上**冲突**（发射器允许、backend 拒绝）。

## 5. 已用轮次

- 本 change 的 discovery 记录：**1**（本节）。未见其他 discovery 记录。
