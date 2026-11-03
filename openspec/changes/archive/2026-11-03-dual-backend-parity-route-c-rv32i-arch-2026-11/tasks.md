# Tasks：路线 C 第 3 批（RV32I 体系结构层三组件进双后端 parity）

## T1 —— 4 问尽调（只读）

- [x] ① 重复：`Rv32iPc` / `Clint` / `CsrBank` 在全部 63 个 dual 文件里**各出现 0 次**
- [x] ② 判别力：变异 **9/9 全抓**（T4）
- [x] ③ 与后端无关：**否** —— 真跑仿真 + 读顶层端口，正是要对比两后端
- [x] ④ 值得新写：既有测试（`test_csr_unit` / `test_interrupt_flow`）都是 **SoC 级**
      集成用例 ⇒ 组件级**无**独立双后端判据 ⇒ 必须新写
- [x] **无已知未修缺陷**（`clint.h` 的 ADR-049 三条陷阱均已规避；`bits_update<32>`
      的截断已被 `cpu-clint-timer` 写成契约）⇒ ⇒ 符合「带缺陷不做 dual parity」的纪律

## T2 —— DUT 与拍表

- [x] 三个 wrapper DUT（`Rv32iPcDut` / `ClintDut` / `CsrBankDut`）
      ⇒ ⚠️ **每个输入端口都显式驱动**（含 `Rv32iPc` 的**死端口** `clk`）
- [x] 三张拍表由**独立 Python 模型**逐拍生成（禁抄仿真输出）

## T3 —— 判据

- [x] `tests/test_rv32i_arch_dual.cpp`（**新**，**4** 个 `DUAL_BACKEND_TEST_PAIR` /
      **366 == 2 × 183** 断言）
      ⇒ pair 1 `Rv32iPc` 顺序/跳转/复位/32 位回绕
      ⇒ pair 2 `Clint` mtime 自由运行 + 写优先 + mtimecmp + mtip
      ⇒ pair 3 `CsrBank` 复位值 + 读写 + 只读寄存器 + mip 粘住
      ⇒ pair 4 `CsrBank` mstatus 字段 + 中断请求链

## T4 —— 变异验证（9 条，全抓）

- [x] M1–M9 全抓（表见 proposal.md）
- [x] **M8 首轮恒绿 = 激励的判别力缺口**（`mstatus.MIE` 恒 1 ⇒ 全局门控从未起作用）
      ⇒ 重排激励补判别点后转红 ⇒ 已如实登记

## T5 —— 契约与验收

- [x] **无 spec 变更**（纯覆盖增量 ⇒ 归档用 `--skip-specs`）
- [x] 全量重建 0 error · `ctest -LE perf` 零回归
- [x] 计数锚点同步（parity 源文件数 63 → **64**）
