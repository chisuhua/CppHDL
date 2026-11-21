# Proposal: `.verif` 的 `RESET` 指向框架复位端口 ⇒ **两端同源 fail-closed**

- **状态**: **实施完成（2026-11-21）** · 待归档
- **性质**: **缺陷硬化**（把一个「看起来能用、实际是静默空操作」的构造改成显式拒绝）
- **架构依据**: `docs/adr/ADR-074-rtl-verification-single-source.md`（`.verif` 单一真相源 ·
  D1 = 同一份 IR 派生两个后端 ⇒ **两端结论 MUST 一致** · D2 = 封闭原语集）
- **同型先例**: `docs/adr/ADR-076-verif-history-primitives.md` 的三类**扫描/运行期 fail-closed**
  （宽端口 > 64 位 · sanitize 撞名 · 历史不足）

---

## Why

`.verif` 的 `RESET <port> <high|low> [cycles]` 在文档里读作「复位某个端口」，但**实测**证明：
把它指向**框架复位端口**（`default_reset` · manifest `role == "reset"`）时，**两个执行后端
都不会真的复位寄存器** ——

| 后端 | 机制 | 结果 |
|---|---|---|
| **解释器** | `build_register` 给寄存器的 `rst_val` 是 **nullptr**（`include/core/node_builder.h`）⇒ 复位期间 `instr_reg::eval`（`src/ast/instr_reg.cpp`）**只 return、不赋值** | 寄存器**保持原值** |
| **渲染 TB** | `RESET` 的**释放动作**与**最后一个**时钟沿**同拍**（`repeat(n) @(posedge clk); name = <deassert>;` **无 `#1`**）⇒ 该沿走 `else` 分支 | 刚写下的复位值被**同拍覆盖** |

⇒ ⇒ 两端**一致地**什么都不做（**不是**「两端结论相反」）⇒ ⇒ 但同样危险：
作者会以为复位发生了 ⇒ 可能把期望值改成「不复位的值」⇒ ⇒ **假绿**。

**实证**：gpio **真实产品 DUT** + **真 verilator**，`RESET default_reset high N`
（N = 1 / 2 / 4 / 5）⇒ ⇒ 两端都不复位（`gpio_out` 仍为复位前的 90）⇒ ⇒ 两端一致 ✓。

## What Changes

- **新增内部共享判定**：`src/rtl_emit/verif_internal.h::verif_reset_port_violation(program, manifest)`
  ⇒ ⇒ `RESET` 的目标端口 role == `"reset"` ⇒ 返回错误消息（点名端口 + 原因）。
  ⇒ ⇒ **端口标识符 → role 的优先级 MUST 镜像解释器的 `Resolver::resolve`** 三条路径
  （① 根自有成员名 → ② 产物端口名 → ③ **成员名唯一**的后代兜底）⇒ ⇒ 否则会出现
  「解析器认为端口存在、但判定查不到 role」⇒ ⇒ **漏判** ⇒ ⇒ 一端接受、另一端拒绝。
- **两端各一处调用**（**同一个函数** ⇒ ⇒ 错误消息**逐字相同**）：
  - 解释器 `src/rtl_emit/verif_run.cpp::run_verif`（在 `Resolver::build` 之后、
    **任何 sim 变更之前**）；
  - 渲染器 `src/rtl_emit/verif_render.cpp::render_tb`（**排在端口解析之前** ⇒ ⇒
    「后代 reset 成员」这类 ident 也两端同消息）。
- **判据载体**：`tests/test_verif_wait_level.cpp` 新增用例「`RESET default_reset`
  两端同源 fail-closed」（解释器拒绝 + 渲染器拒绝 + **消息逐字相等**）
  ⇒ ⇒ **两条变异自证**（移除解释器侧 / 移除渲染器侧 ⇒ 各自转红 ⇒ 还原复绿）。
- **文档**：`docs/usage_guide/04-testbench-guide.md`（`RESET` 的已知边界 + 仍开放的
  **去断言竞态** B0 ②）· `docs/CHANGELOG.md`（一条）· `.rddf/checks/examples-rtl-expectations.md`
  （phase3a 节「⛔ 为什么该 TB MUST 保持手写」+ SoC 节「✅ ADR-076 后复核」）·
  `.rddf/checks/examples-rtl-sim-targets.tsv`（两行 `generated=no` 的 note 改成结论性表述
  + 收窄后的复核条件）。

## Acceptance Criteria

- [x] **AC1** `RESET default_reset …` ⇒ **解释器**与**渲染器**都 fail-closed，且错误消息**逐字相同**
- [x] **AC2** 判据有判别力：移除任一侧的检查 ⇒ 用例转红（两条变异各自实测）
- [x] **AC3** **零破坏**：既有 `.verif` **零处**使用 `RESET`（实测 grep）⇒ 无产物漂移
      ⇒ `verify_examples_rtl_sim.sh` PASS（含 ⑥' 新鲜度）
- [x] **AC4** 端口标识符 → role 的优先级与 `Resolver::resolve` **同源**（三条路径逐条镜像）
- [x] **AC5** 文档同步（usage guide / CHANGELOG / 期望源登记 / 清单 note）

## 用户可见变更

| 项 | 前 | 后 |
|---|---|---|
| `.verif` 的 `RESET` 指向**框架复位端口** | **静默空操作**（两端都不复位 ⇒ 看起来能用）⇒ 作者可能据此写出假绿的期望值 | **两端同源 fail-closed**（错误消息点名端口并说明原因） |
| `RESET` 在**数据端口**上的用法（如 `RESET rst_n low 2`） | 可用 | **不变**（仍可用）⇒ ⇒ 本次只拒绝**框架复位端口** |
| 既有 `.verif` 产物 | — | **零变化**（仓内零处使用 `RESET`）⇒ ⑥' 新鲜度仍成立 |
| 通用**去断言竞态**（去断言与最后一个沿同拍 ⇒ 与解释器差 1 拍） | 开放（登记在归档 change 的 **B0 ②**） | **仍开放**（**待 architect 裁决**）⇒ 本次只在文档写明，未改渲染文本 |
