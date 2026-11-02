# Tasks：`sread` 禁读拍语义对齐（方案 A · 对齐 Verilog 清零）

## C1 —— 修复（**只改 Simulator 一侧**）

- [x] `src/ast/instr_mem.cpp`：`instr_mem_sync_read_port::eval` 的禁读拍
      `return;` ⇒ **清零后 return**（`sdata_type(0, bitwidth())`）
- [x] `src/ast/instr_mem.cpp`：async 分支加防复发注释（**行为不变** ——
      `aread` 恒传 `nullptr` ⇒ `enable_data_ptr_` 恒空 ⇒ 该分支不可达）
- [x] `src/codegen_verilog.cpp`：**只订正注释**（原写「Reset branch clears」
      ⇒ 实际是 `enable == 0` 清零；dangling-else 把 `else` 绑到内层 `if (en)`）
      ⇒ 生成代码**逐字节不变**

## C2 —— 双后端判据（**新文件**）

- [x] `tests/test_mem_sread_disable_dual.cpp`（**2** pair）
      ⇒ pair 1 enable 翻转（6 拍，含 ★3 个「清零 vs 保持」判别点）
      ⇒ pair 2 禁读拍换地址不采样（6 拍）
- [x] `tests/chlib/test_memory_dual.cpp`（**2** pair）
      ⇒ `single_port_ram`（5 拍）· `dual_port_ram` 两端口独立（4 拍）
      ⇒ 写拍期望值 = **0**
- [x] `tests/CMakeLists.txt`：2 条 `add_dual_backend_test`

## C3 —— 判据（N2′）与判别力

- [x] `test_mem_sread_disable_dual`：**24 == 2 × 12**
- [x] `test_memory_dual`：**26 == 2 × 13**
- [x] 期望值**全部手推**；修复后两后端**一次通过**（修复前同一组期望值会红）
- [x] 变异 **2/2 全抓**（每条先确认仍可编译）：
      M1 interpreter 改回 `return;`（复现分歧）⇒ 2 failed ·
      M2 codegen 改成禁读拍保持（删 dangling else）⇒ 2 failed
- [x] 还原用 `cp`（刷新 mtime ⇒ 强制重建）⇒ 复验回到 24/26 全绿

## C4 —— spec 与计数

- [x] capability `mem-read-timing` **ADDED ×1**（3 个 Scenario：清零且一致 /
      裁决可追溯 / `chlib::memory` RAM 族被覆盖）⇒ 归档 **MUST NOT** `--skip-specs`
- [x] 锚点：`CTEST_TOTAL` **224** · `VERILATOR_E2E_TOTAL` **56** ·
      `DUAL_BACKEND_PARITY_TOTAL` **47** · `PAIRS` **237** · non-perf **213** ·
      `tests/test_*.cpp` **127** · `tests/chlib/test_*.cpp` **41**
- [x] `ctest -LE perf` **213/213** 零回归
- [x] `openspec validate sread-disable-cycle-divergence-2026-11 --strict` valid
- [x] `verify_openspec_delta.sh` 4/4 绿

## C5 —— 连带解锁

- [x] `chlib::memory` 的 dual 覆盖补回（原在
      `dual-backend-parity-route-b-pipeline-2026-11` 被推迟）
- [ ] 路线 A 的 **D 批**（`test_mem_comb_read` / `test_mem_connection` /
      `test_mem_timing`）→ **本 change 已解锁**，但**不在本 change 范围**
      （那是覆盖工作，另批次推进）
