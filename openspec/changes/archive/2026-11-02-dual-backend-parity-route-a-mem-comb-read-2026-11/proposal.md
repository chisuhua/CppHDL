# 提案：路线 A（D 批）—— `ch_mem` **写口 × `aread` 交互**进双后端 parity

- **依据**：`.rddf/plans/dual-backend-full-coverage-2026-11.md` §4 **D 批**（路线 A）
- **规格影响**：**无 spec 变更**（`.openspec.yaml` 声明 `skip_specs: true`）
- **规模**：1 个新测试文件（2 pair）+ 1 条 CMake 注册 + 计数锚点
- **关联**：`objective-test-coverage-100pct` · capability `dual-backend-test-coverage`

## Why

`tests/test_mem_comb_read.cpp` 的 8 个 TEST_CASE **全部是裸 `ch_device` 单后端**。
但逐个核对「转双后端是否有增量」后，**只有 2 个**真正需要新文件。这个结论与交接
文档的初步判断（「6 可转」）不同，逐条依据如下（读实现 + 变异实测）：

| 原用例 | 结论 | 依据 |
|---|---|---|
| `:203` aread 同拍可见 | ❌ 不转 | ① 形态是「输入端口驱动地址 → `tick()` → 读」⇒ **无法区分 0/1-cycle**（T2）；② 数据通路（aread → `ch_out`，输入地址直连）**已在** `test_mem_connection_dual.cpp` pair 2 覆盖 |
| `:247` sread 1-cycle | ❌ 不转 | 同上（数据通路对应 `test_mem_connection_dual.cpp` pair 1） |
| `:289` aread + 寄存器地址 | ❌ 不转 | **已被** `test_mem_read_timing_dual.cpp` 覆盖，且用**同一判别形态**（寄存器驱动地址）+ **同一组初值/期望**（0xE5 / 0xD4 / 0xE5…）⇒ 重复 = 零增量 |
| `:323` sread + 寄存器地址 | ❌ 不转 | 同上（该文件一对 pair 同时断言 aread 与 sread 两侧） |
| `:368` / `:439` | ❌ 不转 | **codegen 文本断言**，与跑哪个后端**无关** |
| `:474` 跨拍写后读 | ✅ **转** | 裸 `ch_mem` 的**写口**在双后端下**零覆盖**（实测：`test_mem_connection_dual` / `test_mem_read_timing_dual` / `test_mem_sread_disable_dual` 的 `.write(` 计数全为 **0**；`tests/chlib/test_memory_dual.cpp` 的写口属 `chlib::memory` RAM 族） |
| `:531` 同拍写 + aread | ✅ **转** | 同上 |

## 🔴 T2：为什么 `:203` / `:247` **不可能**有判别力

`set_input_value(addr) → tick() → get_port_value()` 形态：地址是**输入端口**，TB 在
`tick()` **之前**写进 `data_map_` ⇒ 沿上采样到的是**新**地址 ⇒ 「0-cycle 读新地址」
与「1-cycle 在沿上采样新地址」**观测值相同**。
⚠️ 「tick 之间观测」也救不了：`get_port_value` 在两后端都**只读 `data_map_`**
（`include/simulator.h:113-140`），`set_input_value` 不触发组合求值 ⇒ 两侧都读到
**上一拍**值 ⇒ 对 0/1-cycle 同样不可判别。
⇒ 唯一可判别的形态 = 地址由**同沿提交的 `ch_reg`** 驱动（沿前值 ≠ 沿后值）
⇒ 而这正是 `test_mem_read_timing_dual.cpp` 已经做的事 ⇒ 本文件**不再重复时序**。

## What Changes

| 文件 | 内容 |
|---|---|
| `tests/test_mem_comb_read_dual.cpp`（**新**） | 2 个 `DUAL_BACKEND_TEST_PAIR`（**7 断言/后端**）：pair 1 跨拍写后读 + `we=0` 不写的负向对照 · pair 2 同拍写+aread 的写后值 + 持久化 |
| `tests/CMakeLists.txt` | 1 条 `add_dual_backend_test`（含「为什么只有 2 个 pair」的注释） |
| 计数锚点 | `CTEST_TOTAL` 225 → **226** · `VERILATOR_E2E_TOTAL` 57 → **58** · `DUAL_BACKEND_PARITY_TOTAL` 48 → **49** · `PAIRS` 241 → **243** · non-perf 214 → **215** · `tests/test_*.cpp` 128 → **129** |

## ⚠️ 一处**新增**（不是转换，如实登记）

pair 1 的 tick3 断言（`we=0` 却给 `wdata=0x77` ⇒ 必须读回 `0xFF`）在原文件 `:474`
里**不存在** —— 原文件的 `we=0` 拍从未验证「**真的不写**」。新增理由：这一条断言
同时判别**三种**失败模式（写没落地 ⇒ 0xF6 · `we` 被忽略 ⇒ 0x77 · 写串槽 ⇒ 其它
初值）⇒ 它让 pair 1 对变异 M3 / M4 都有判别力，而不是只钉「写能落地」。

## Verification（前瞻 · 可勾选）

- [x] **N2′**：**14 == 2 × 7**（有/无 verilator 实测）
- [x] 4 条变异**全部被抓**（每条先确认仍可编译；还原用 `cp` 后重建）：
      M1 codegen `aread`→`always_ff` ⇒ pair 2 红 ·
      M2 interpreter `aread` 归入 sequential ⇒ pair 2 红 ·
      M3 codegen 写口忽略 `we` ⇒ pair 1（`:185` 先红，读回 `0x00`）+ pair 2（`:224`）·
      M4 codegen 写口忽略地址 ⇒ pair 1（`:196`）+ pair 2（`:217`）
- [x] 每条 pair 都有 **≥2** 条变异能抓它（无填充 pair）
- [x] 原文件 `tests/test_mem_comb_read.cpp` **未修改**
- [x] 全量重建后 `ctest -LE perf` 零回归
- [x] 计数锚点同步且两个计数护栏双绿
