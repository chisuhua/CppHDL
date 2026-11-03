# Tasks：路线 C 第 1 批 —— RV32I 微架构组件进双后端 parity

## C0 —— 可转性 4 问 + 路线盘点

- [x] ① 语义是否已被别的 dual 文件覆盖？ ⇒ **否**（三个符号在 58 个 macro dual 文件里各 0 次；
      唯一既有覆盖是 **SoC 级** hand-crafted dual ⇒ 判别形态不同 ⇒ 逐案裁决通过）
- [x] ② 断言形态有判别力吗？ ⇒ 变异 12/13 抓（+ 1 条判别力边界）
- [x] ③ 与后端无关吗？ ⇒ **否**（真跑仿真 + 读顶层端口）
- [x] ④ 形态值得新写吗？ ⇒ **零 DUT 复制**（三个组件本身带顶层端口 ⇒ 直接当 DUT）

## C1 —— 新文件

- [x] `tests/test_rv32i_stage_dual.cpp`
      ⇒ pair 1 `riscv::MemStage`（24 组：load 车道/扩展 + store 字节使能）
      ⇒ pair 2 `riscv::ExStage`（28 组：ALU 族 + 第二操作数选择 + 分支/跳转重定向）
      ⇒ pair 3 `riscv::Rv32iDecoder`（17 组：R/I/S/B/U/J 五种格式）
- [x] `tests/CMakeLists.txt`：1 条 `add_dual_backend_test`

## C2 —— 判据（N2′）与判别力

- [x] **N2′**：**920 == 2 × 460**
- [x] 变异 **12/13 抓**（每条先确认仍可编译；还原用 `cp` 后重建）
- [x] 期望值**手推**（独立 Python 模型 + **规范编码器** ⇒ 一次通过）
- [x] ⚠️ 如实登记 **4 条**「判别力边界 / 无效变异」+ **2 处我自己的表错**（第 4 次命中
      「两后端一致 ⇒ 同一处推导错」）
- [x] **M11 = 把缺陷修好** ⇒ 转红 ⇒ 证明本批的 B 型钉点有判别力

## C3 —— 缺陷登记（本批的副产品）

- [x] 🔴🔴 `Rv32iDecoder`（**及生产路径 `id_stage.h`**）的 **B 型立即数 bit 11 misplaced**
      ⇒ `.rddf/improvements/rv32i-b-type-immediate-bit11-misplaced-2026-11.md`
- [x] 🔴 `ex_stage.h` 的 `BRANCH_*` 常量块与实现相反 ⇒
      `.rddf/improvements/rv32i-branch-type-constants-contradict-impl-2026-11.md`
- [x] ⚠️ **本批刻意不修**这 2 个缺陷（行为变更 ⇒ 需 ADR ⇒ 独立 change）

## C4 —— 计数与验收

- [x] 实测（全量重建后）：`ctest -N` **237** · `-L verilator_e2e` **69** · `-LE perf` **226** ·
      `verify_dual_backend_parity.sh` **60 / 268** · `tests/test_*.cpp` **131**
- [x] AGENTS.md 锚点同步（4 anchor + 3 NOTES 计数 + NOTES 条目）
- [x] 全量重建后 `ctest -LE perf` **226/226** 全绿
- [x] 全部 `scripts/verify_*.sh` exit 0（含命名诚实性自检）
- [x] ⭐ **parity 60 ⇒ 阈值达成** ⇒ objective 台账 + feature fragment 的验收勾选已同步
- [x] 归档用 `--skip-specs`（本 change 显式声明无 spec 变更）
