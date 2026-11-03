# Tasks：chlib stream 宽度适配器重写（ADR-064 实施）

## T1 —— 重写（D2 / D3 / D4 / D5 / D6）

- [x] `stream_narrow_to_wide`：形态 E 默认 ready 驱动 + `input.ready <<= out_free`
      + 累积器在组完成时清零 + 宽字用 `merged`（**含最后一个窄字**）
      + 输出 `valid` / `payload` **寄存**（+1 拍，可观测）
- [x] `stream_wide_to_narrow`：只修 ready 所有权（数据通路已核对正确）
      + ⚠️ `input.ready` MUST 先定义再参与 `accept`
- [x] `stream_width_adapter` 同宽分支：形态 E
- [x] 文件头补接线纪律：调用方 MUST `as_master()`（否则 `ready` 无节点）

## T2 —— 物理删除（D7）

- [x] `stream_extend` / `stream_truncate` 删除（零消费者 + 零测试 + 静默失效）
- [x] 留注释说明：若将来需要 MUST 走独立立项，MUST NOT 复活旧实现

## T3 —— 判据（D9）

- [x] `tests/chlib/test_stream_width_adapter_dual.cpp`（**新**，4 个
      `DUAL_BACKEND_TEST_PAIR` / **152 == 2 × 76** 断言）
      ⇒ pair 1 数据通路 + 跨组隔离 · pair 2 背压 · pair 3 拆分 · pair 4 同宽直通
- [x] 既有 `test_stream_width_adapter.cpp` 的 **8 个结构性用例保留**（18 断言仍绿）

## T4 —— 变异验证（ADR-064 §5.5；每条先确认仍可编译；还原用 `cp` 后重建）

- [x] M1 去掉 `result.ready` 默认驱动 ⇒ **CRASH**（崩溃复现）
- [x] M2 `input.ready <<=` 改回 `operator=` ⇒ 🔴 exit=4
- [x] M3 累积器不清零 ⇒ 🔴 exit=2（⚠️ **首轮恒绿**：测试数据 `0xA0|0xB0==0xB0`
      掩盖了污染 ⇒ 换位错开数据后转红 —— 见 T5）
- [x] M4 输出改组合 ⇒ 🔴 exit=4
- [x] M5 宽字用 `accumulator`（不含当前字）⇒ 🔴 exit=4

## T5 —— 如实登记：**测试数据的判别力缺口**（本轮实测挖出）

- [x] 首版数据组 1 = `0xA0..0xA3` / 组 2 = `0xB0..0xB3` ⇒ 每个字节都满足
      「组 1 **或** 组 2 == 组 2」⇒ 按位或**掩盖**了跨组污染 ⇒ M3 恒绿
      ⇒ ⇒ 改为组 1 高半字节（`0xF0,0xE1,0xD2,0xC3`）/ 组 2 低半字节
      （`0x0F,0x1E,0x2D,0x3C`）⇒ M3 转红
- [x] 该约束已写进测试文件注释与 capability 的 Scenario NOTE（防后人再踩）

## T6 —— 契约与验收

- [x] capability `chlib-stream-ready-ownership` **ADDED ×1**（1 Requirement + 4 Scenario）
- [x] `verify_openspec_delta.sh` 4 条全绿 · `openspec validate --strict` 通过
- [x] 全量重建后 `ctest -LE perf` 零回归
- [x] 计数锚点同步（parity 源文件数 61 → **62**）
- [x] 归档**未**用 `--skip-specs`

## 不做

- [ ] `stream_arbiter_lock`（恒选通道 0）· `round_robin_arbiter`（不轮转）
      ⇒ 各自独立立项，不在本 change
- [ ] `priority_encoder` 注释与实现相反（`include/chlib/converter.h`）⇒ 独立立项
