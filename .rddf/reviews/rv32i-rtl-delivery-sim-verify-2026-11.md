# 验收报告：RV32I 核的 RTL 交付 + Verilator 仿真环境

- **任务**：用户 2026-11-17 诉求「给出完整的 RV32I 核的 RTL 设计和 Verilator 仿真环境」
- **实施角色**：builder（角色内建 verifier 环节）
- **已用轮次**：**1 / 3**
- **判据来源**：用户本次诉求（无 openspec change ⇒ 按 quick 路径：本次声明什么就验什么）
- **日期**：2026-11-17

## 增量定位

上一份报告（`hier-rtl-product-surface-verify-2026-11.md`）已闭「层次化进产品面」。
本报告只验**增量**：① 仿真环境（一条命令）② 交付文档 ③ 一处事实订正。

## 逐条判定

| # | 判据 | 判定 | 工具证据 |
|---|---|---|---|
| AC1 | **一条命令**跑通完整仿真流程（生成 → lint → 编译 → 运行） | ✅ | `bash scripts/rv32i_rtl_sim_env.sh` ⇒ `✅ rv32i_rtl_sim_env: PASS`；四步各留证据（`emit.log` / `lint.log` / `compile.log` / `run.log`） |
| AC2 | **两种形态**产物都生成 | ✅ | flat = **1** module（`top`）· hier = **8** module（`IfStage IdStage ExStage MemStage WbStage HazardUnit ForwardingMux_32_ top`） |
| AC3 | 产物顶层端口表 == manifest `port_table[]` | ✅ | 脚本内断言 **34 == 34**（两形态均过）；`manifest.json` `port_table[]` = 34 · `ports[]` = 213 · 诊断 0 |
| AC4 | lint 干净 | ✅ | `verilator --lint-only --top-module top` ⇒ **退出码 0** · **零 `%Error`**（两形态） |
| AC5 | 独立 TB 编译 + 跑通 | ✅ | `verilator --binary --timing` ⇒ 编译 rc=0；运行 ⇒ **`RESULT: checks=24 fails=0`**（**两形态同一组判据** ⇒ 「层次化 ≡ 扁平」在产品路径上得到证据） |
| AC6 | 交付文档（设计与环境）完整且引用不悬空 | ✅ | `docs/developer_guide/rv32i-rtl-delivery-and-sim.md`（270 行）· 自检脚本确认 **19 个引用文件全部存在**（唯一「未找到」是产物名 `manifest.json` ⇒ 非仓内路径）· 陷阱表 **8** 条 · 固件 **3** 条路径 |
| AC7 | **零扰动**（注释订正不改产物） | ✅ | `rv32i_pipeline_top.h` 注释订正前后，扁平/层次化 `top.v` + `manifest.json` 的 SHA-1 ⇒ **逐字节相同** |
| AC8 | 边界与错误路径 | ✅ | `--form bogus` ⇒ **exit 2**（用法错误 fail-closed）· `--form hier` ⇒ 只跑层次化 PASS · `--out` ⇒ 换目录生效 · 脚本缺 verilator / 缺发射器 ⇒ exit 2 · lint 非零 / 编译失败 / 断言失败 ⇒ exit 1（代码路径 + 实际 run） |
| AC9 | 全量测试不回归 | ✅ | `ctest -LE perf` ⇒ **255/255 passed**（含 `verilator_e2e` **77** 项）· 标签摘要后**无 FAILED 段** |
| AC10 | 护栏回归 | ✅ | `verify_examples_rtl_sim`（PASS 12 项）· `verify_flat_rtl_emit` · `verify_rv32i_fw_init_rtl` · `verify_dtcm_read_timing` · `verify_public_api_visibility` ⇒ 全 **exit 0** |

## 事实订正（随增量同批）

`examples/riscv-mini/src/rv32i_pipeline_top.h` 的头注释原写「固件 MUST 无数据冒险：lib 版
`HazardUnit` 不接数据前推」⇒ ⇒ **与实际装配矛盾**（ADR-047 D2 的前推 mux =
`chlib::ForwardingMux<32>`，装在**流水线装配层** `rv32i_pipeline.h::build_forwarding_paths()`
⇒ ⇒ 层次化产物里可见 `ForwardingMux_32_` module）⇒ ⇒ 订正为「前推已接入」并指向
ADR-047 §5.5 + ADR-069（**不复制**其判据）。

⚠️ 这是**事实订正 · 无行为变更**（AC7 已证明产物逐字节不变）⇒ ⇒ 不属 architect 域。

## 遗留清扫（复核）

| 项 | 结果 |
|---|---|
| TODO / FIXME / XXX / HACK / 调试日志 | **零**（新增两文件全扫） |
| 注释掉的代码块 | **零** |
| 被 skip 的测试 | **零** |
| 未替换的 mock / 临时 fallback | **零** |
| 临时文件 | **零**（交付产物落在**未跟踪**的 `build/` ⇒ 不进仓） |

## 未纳入本次（如实登记）

| 项 | 原因 |
|---|---|
| 仿真环境进 CI | **本次不做**：与 `verify_examples_rtl_sim.sh` 重复，且会让 CI 墙钟 +N（已在文档 §5 如实标注「未进 CI」） |
| 层次化 + 固件初值的组合变体（`emit_rtl_fw_init` 的层次化版） | 「路线 A」与 `--hierarchical` 正交 ⇒ ⇒ 需要时按同一模式加一个即可（文档 §3.4 已给模板） |
| `rv32i-fw-dump --format vh` 用于本 SoC | **不可行**（mem 在 DUT 内部 ⇒ TB 不可达）⇒ ⇒ 文档 §3.4 已点明，避免用户走错路 |
| 交付产物签入仓 | **不签入**：单份 `top.v` = 206 KB / 3067 行 ⇒ ⇒ 签入会让 diff 不可读；产物由脚本**可复现**生成 |

## 结论

**PASS**（10/10 判据）。RV32I 核的 RTL 交付（扁平 + 层次化）与 Verilator 仿真环境就位，
一条命令可复现，两形态跑同一组判据，文档引用零悬空。
