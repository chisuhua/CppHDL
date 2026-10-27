# Tasks: rv32i-load-byte-lane-2026-10

> TDD 5 步（红 → 绿 → 精化 → 变异 → 回归）。**无实现改动** ⇒ 单 commit 即可。
> **有** spec 变更 ⇒ 归档 **禁** `--skip-specs`。

## 0. 准备

- [x] 0.1 读码确认 LB/LH/LBU/LHU **已实现**（`mem_stage.h:82-163`）⇒ 本 change 只**验证**
- [x] 0.2 手推 12 个期望值（design.md §2）⇒ 期望值**不来自**仿真输出
- [x] 0.3 确认「每字节都不同」的字（`0xA1B2C3D4`）与「非零 offset」的必要性
- [x] 0.4 确认 `DataTCM` 的写是**读-改-写**（① 的 spec 条款）⇒ 4 次 `SW` 得整字

## 1. 红 / 绿

- [ ] 1.1 新增 `skid` 风格的独立固件 `load_byte_lane_firmware()`（**既有固件不动**）
- [ ] 1.2 新增独立 TEST_CASE（逐拍双后端全等 + 12 项写回序列逐项比对）
- [ ] 1.3 既有固件的 golden **零变化**（既有 TEST_CASE 仍绿）
- [ ] 1.4 `ctest -R test_rv32i_soc_dual_parity` 绿

## 2. 精化（判别力自检）

- [ ] 2.1 每条期望带**反向期望**（`X_old != X_new`）并留痕
- [ ] 2.2 确认 12 项里**每一项**都含「非零 offset 或非 lane0」（⇒ 无「碰巧正确」项）

## 3. 变异验证（每条**先确认编译成功**）

- [ ] 3.1 **M1** byte 选择恒取 lane0 ⇒ 转红
- [ ] 3.2 **M2** 符号扩展写成零扩展 ⇒ 转红
- [ ] 3.3 **M3** half 选择取反 ⇒ 转红
- [ ] 3.4 **M4** `funct3` 分派漏 LBU/LHU ⇒ 转红
- [ ] 3.5 全部还原 ⇒ **重建** ⇒ 复跑全绿（build 目录不得留变异二进制）

## 4. 回归与工程

- [ ] 4.1 `ctest -LE perf` 零回归（191/191）+ `ctest -L perf` 全绿
- [ ] 4.2 `run_all_ported_tests.sh` 28/28 + `run_all_samples.sh` 35/35
- [ ] 4.3 8 个 `scripts/verify_*.sh` 全 exit 0
- [ ] 4.4 计数**不变**：202 / 17 / 37
- [ ] 4.5 `openspec validate --specs --strict` 不因本 change **新增**红

## 5. 文档与簿记

- [ ] 5.1 根 `AGENTS.md`：§KNOWN LIMITATIONS 的「ISA 覆盖缺口」收窄 + §NOTES 记录
- [ ] 5.2 `feat-rv32i-isa-completeness` 的验收项勾选 + `roadmap.md`
- [ ] 5.3 主 spec 的 `## Purpose` 归档后核对（delta 的 Purpose 不会被合并 ⇒ 手工）

## 6. 审稿与归档

- [ ] 6.1 对抗审稿（critic，`review_profile: plan, build`）⇒ ≤ 2 轮
- [ ] 6.2 审稿处置（BLOCKER ⇒ 返工；SF / NIT ⇒ 同批闭合留痕）
- [ ] 6.3 `openspec archive rv32i-load-byte-lane-2026-10 --yes`（**禁** `--skip-specs`）
