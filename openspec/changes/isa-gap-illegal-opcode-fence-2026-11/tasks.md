# Tasks — isa-gap-illegal-opcode-fence-2026-11

- [x] G3：装载器把程序尾部填成 NOP（`load_firmware` · 独立 Verilog TB 的装载任务）
- [x] G1：ID 级产出 `illegal_reason`（`valid` 门控 · 纯 `select`/`&`/`|`/`!`）
- [x] G2①：SoC 顶层引出 `dbg_illegal_reason`（2 bit）
- [x] G2②：冻结（`if_stage.stall` / `id_stage.stall` 均 OR 上 `illegal_reason != 0`）
- [x] G4：`OP_FENCE = 0x0F` 入已知集合（行为 = NOP）
- [x] 判别性用例（3 场景 × 双后端 · 期望值手推 · `FENCE` 作负向对照）
- [x] 独立 Verilog TB 端口表补 3 根 + 装载器按 G3 填 NOP
- [x] 文档：`include/cpu/AGENTS.md`（行数 + G1/G4 说明）· `docs/CHANGELOG.md` ·
      `.rddf/checks/examples-rtl-expectations.md`（端口 126 → 129）
- [x] 验收：`ctest -LE perf` 254/254 · 全部 `scripts/verify_*.sh` PASS

## 遗留（**不在**本 change）

- `funct3` / `funct7` 非法组合检测（只覆盖 opcode）
- `G5`（`ExceptionUnit` 的 `mcause` 写缺陷）
- CSR / `ECALL` / `EBREAK` / `MRET` / 中断 / trap 入口（独立立项）
