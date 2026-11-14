# 验收报告 — `firmware-load-chain` L2（路线 A）+ L6

- 日期：2026-11-14
- 角色：builder（自验；verifier 独立复核仍建议在本 change 收口前做一轮）
- 方案：**用户裁决 = 路线 A**（固件经 `ch_mem::init_data` ⇒ 产物自带 `initial` ⇒ TB 零装载）
- 变更面：`include/cpu/pipeline/rv32i_tcm.h` · `examples/riscv-mini/src/rv32i_pipeline_top.h` ·
  `examples/riscv-mini/src/rv32i_fw_demo.h`（新）· `examples/riscv-mini/emit_rtl_fw_init.cpp`（新）·
  `examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp` · `scripts/verify_rv32i_fw_init_rtl.sh`（新）·
  `.github/workflows/ci.yml` · `.rddf/checks/guard-reachability.md` · 变更文档（proposal/design/tasks/spec delta）

## 1. 判据与结果

| # | 判据（来源） | 证据 | 结果 |
|---|---|---|---|
| AC-1 | `.text` 与 `.data` **都能装载**（spec delta · 路线 A） | `InstrTCM` / `DataTCM` 接 `init_words`；`Rv32iPipelineTop` 透传 | ✅ |
| AC-2 | 产物 MUST 自带 `initial` 且**逐字**等于初值镜像 | `scripts/verify_rv32i_fw_init_rtl.sh` **F3**（`I-TCM[0..3]` + `D-TCM[64]=0x5a`，**手推值**） | ✅ |
| AC-3 | 基线产物 **零** init 赋值（反向 ⇒ 证明初值只来自 `init_data`） | 同上 **F4** | ✅ |
| AC-4 | **零端口变更**（带初值 vs 基线产物顶层端口表逐字相同） | 同上 **F2**（114 端口，diff 为空） | ✅ |
| AC-5 | 装载后**走真实读通路**读回 | `Rv32iSoc - FirmwareInitDataLoadedFromConstruction`：**零 `prog_*` 写** · 判据取自 **WB 写回**（`dbg_wb_rd_addr`/`dbg_wb_write_data`） | ✅ |
| AC-6 | 双后端一致（verilator 侧 = 真 Verilog 的 `initial` 生效） | 同用例 ①：40 拍 × 27 观测 **无掩码**逐拍全等 | ✅ |
| AC-7 | 确定性（两次发射逐字节相同） | `verify_rv32i_fw_init_rtl.sh` **F5** | ✅ |
| AC-8 | 默认空镜像 ⇒ **零行为变化** | 既有 253 个非 perf 用例全绿；基线产物 init 赋值 0 条（F4） | ✅ |

## 2. 变异自证（判别力）

| 变异 | 方式 | 结果 |
|---|---|---|
| 丢掉 `.data` | `dtcm{"dtcm", itcm_init_}`（把 `.data` 换成指令镜像）⇒ **重建** | 🔴 红：`x1 = 0`（期望 `0x5A`）⇒ 还原 + **重建**后复绿 ✅ |
| 值投毒 | 产物里 `D-TCM[64] = 32'h5b`（一位差） | 🔴 红（`--self-test` 里的第三投毒） |
| 正反互换 | 基线产物喂「期望有初值」/ 带初值产物喂「期望无初值」 | 🔴 两条各自红（`--self-test`） |

## 3. 遗留清扫

| 项 | 处置 |
|---|---|
| 临时调试日志 / 注释掉的代码 / 被 skip 的测试 | 无 |
| 半截实现 | 无（`init_words` 全链路接通：TCM → SoC → 发射器 → 产物 → 判据） |
| 未替换的 mock | 无 |
| `examples/riscv-mini/AGENTS.md` STRUCTURE | 已同步（`src/` 2 个头 + 2 个发射入口） |
| `include/cpu/AGENTS.md` 自述行数 | 已同步 6582 → **6606**（护栏 C5 复核绿） |
| 运行期装载口（原 L2 设计） | **已显式移出**本 change（proposal Out of Scope + design 文首记录两条硬约束）⇒ 交用户决定是否独立立项 |

## 4. 复跑命令

```bash
cmake --build build -j$(nproc)
ctest --test-dir build -LE perf                    # 253/253
bash scripts/verify_rv32i_fw_init_rtl.sh           # F0–F5
bash scripts/verify_rv32i_fw_init_rtl.sh --self-test
bash scripts/verify_guard_reachability.sh
bash scripts/verify_no_duplicate_rv32i_headers.sh
```

实测：`ctest -LE perf` **253/253** · 全部 `scripts/verify_*.sh` **FAIL=0** · F 护栏含自检全绿。
