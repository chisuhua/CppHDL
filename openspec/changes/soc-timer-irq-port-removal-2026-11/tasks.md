# Tasks: 删掉 SoC 顶层零消费的 `timer_irq` 占位端口（台账 id=27）

## 1. 设计层

- [x] 1.1 `examples/axi4/axi_soc_example_top.h` 删 `ch_in<ch_bool> timer_irq{"timer_irq"};`
      （连同「零消费」注释块）⇒ 根自有 `__io` 12 → 11（7 in + 4 out）

## 2. 验证面（三处 + 一次重生成）

- [x] 2.1 `examples/axi4/tb_axi_soc_example.verif` 删 `SET timer_irq 0`
- [x] 2.2 `examples/axi4/tb_axi_soc_example.cpp`：
      `make_inputs()` 删 `top.timer_irq` 项（根 `ch_in` 全集 8 → 7）·
      `make_stimulus()` 每行删最后一列 + 删 `no_irq` 常量 + 同步顺序注释 ·
      `kExpectedTopPorts` 14 → 13（注释同步「框架 2 + 根自有 11（7 in + 4 out）」）
- [x] 2.3 **重生成** `examples/axi4/tb_axi_soc_example.sv`（由 `.verif` 渲染 ⇒
      **禁**手改生成物；护栏 ⑥' 逐字节比对 ⇒ 重生成物 MUST 与仓内文件逐字节相同）

## 3. 期望源与清单

- [x] 3.1 `.rddf/checks/soc-rtl-expectations.md`：§1 端口表删行 10 + 重编号 11–14 → 10–13 +
      「14 个端口 MUST 全部连接」→「13 个」+ §1 标题「（14 个 = 框架 2 + 根自有 12）」→「13 个 = 框架 2 + 根自有 11」·
      §5 输入常量删 `timer_irq` 行 · §5b 场景出处表删 `timer_irq` · §5c 盲区段删死端口那条
- [x] 3.2 `.rddf/checks/examples-rtl-sim-targets.tsv`：SoC 行 note「顶层端口 14 / 根自有 12」→ 13 / 11

## 4. 残留登记两侧同批转「已收口」（**归档后**回填）

> ⚠️ **顺序**：闭口证据指向**归档后**的 change 路径 ⇒ ⇒ 归档前该路径**不存在**
> ⇒ ⇒ 护栏 `verify_arch_ledger_residuals.sh` 的「证据存在」判据会红 ⇒ ⇒
> ⇒ 故本组 MUST 在**归档之后**回填（与归档同一 commit）。

- [ ] 4.1 `.rddf/checks/arch-residual-registry.md` id=27：状态 `已知` → `已收口` ·
      日期 2026-11-21 · 后缀 · 闭口证据（归档 change `proposal.md` + `examples/axi4/axi_soc_example_top.h`）· 归属 `—`
- [ ] 4.2 `docs/architecture/2026-11-02-hierarchical-rtl-output.md` §7 行 27：状态格 MUST 以
      **登记表生成的规范句**开头（`✅ 已收口（2026-11-21 · <后缀>）`）
- [ ] 4.3 `bash scripts/verify_arch_ledger_residuals.sh` ⇒ PASS

## 5. 验证

- [x] 5.1 `bash scripts/verify_examples_rtl_sim.sh` ⇒ PASS（⑥'/⑥''/⑥‴/⑧/⑧''/⑨ 全绿）
- [x] 5.2 `cmake --build build --target all-examples` + `./run_all_ported_tests.sh` ⇒ 全绿
- [x] 5.3 `ctest -L base` ⇒ 无回归
- [x] 5.4 `openspec validate soc-timer-irq-port-removal-2026-11 --strict` + `scripts/verify_openspec_delta.sh` ⇒ PASS

## 6. 归档

- [ ] 6.1 `openspec archive soc-timer-irq-port-removal-2026-11 --yes --skip-specs`
      （**带** `.openspec.yaml` 的 `skip_specs: true` 声明 ⇒ 本 change 无 spec 变更）
- [ ] 6.2 归档后回填 §4 的两侧（证据 = 归档路径）⇒ ⇒ 护栏绿 ⇒ ⇒ commit
