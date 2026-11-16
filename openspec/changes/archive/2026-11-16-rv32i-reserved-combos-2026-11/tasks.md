# Tasks — rv32i-reserved-combos-2026-11

- [x] 1 `Rv32iDecoder`：保留组合判定 + 新输出 `illegal_reason`（**最后一个 `io()` 赋值** ⇒ 保编号）
- [x] 2 `id_stage.h`：改为**转发**库组件的 reason（删 `known_opcode`/`is_system`/
      `illegal_known_unimpl`/`illegal_unknown` 四个局部量 ⇒ 零死代码）
- [x] 3 判据：SoC 层「保留组合 ⇒ reason 3 + 冻结」场景（含**负向对照**：`FENCE` = 0）
- [x] 4 ⚠️ 夹具缺陷修正：差分表的 SUB 编码 `0x803100B3` → **`0x403100B3`**（`funct7` 0x40 → 0x20）
- [x] 5 独立 Verilog TB：decoder 端口**位移 +1** + 新增 1 根连接（按**产物实测**对齐）
- [x] 6 **变异自证**：去掉 `LOAD` funct3 检查 ⇒ 转红 ⇒ 还原 + **重建** ⇒ 复绿
- [x] 7 ADR-071 **v1.2**：supersede §2.6 D6 + 新增 **§2.6.1 判定表**（含 U/J 型无 funct3 字段）
- [x] 8 文档：`docs/CHANGELOG.md` · `include/cpu/AGENTS.md` · improvement · 路线图 §7
- [x] 9 验收：`ctest -LE perf` 全绿 · `verify_examples_rtl_sim.sh` 绿 · `verify_*.sh` 全 PASS
