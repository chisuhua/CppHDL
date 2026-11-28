# Tasks: S4c-1a —— I-L1 的 AXI4-full 化 + 修「假 line」

> **change**：`rv32i-i-l1-axi-full-2026-11` · **范围** = ADR-078 的 **D7**（I 侧 cache 的 AXI 侧
> full 通道化 + 修「假 line」）⇒ S4c-1b（取指 fork + 核的 full 取指主口）**另立**。
> **参照实现**：`include/cpu/cache/d_cache_sync.h`（S3 第 1 阶段 · `81097dc`）。

---

## P0 · 决策门（已闭）

- [x] **T0.1** ADR-078 **D7** 已定（原地升级 · 两处一起改 · hit 延迟不变 · `i_cache_async.h` 零 diff）
      ⇒ ⇒ **S4c-1 拆分**（design §「范围拆分」）：本批 = D7（对端 = 已存在的 `TcmAxiSlave`
      ⇒ 可独立验证）· S4c-1b = D14 + 核的 full 取指主口（取指反压是**不同量级**）
- [x] **T0.2** 用户 2026-11-28 裁决：继续推进下一批 ⇒ ⇒ 本批为 **S4c-1a**

## P1 · I 侧 AXI 侧 full 读通道 + burst fill（组件级）

- [ ] **T1.1** `i_cache_sync.h` 的 AXI 侧：5 根简化口（`axi_addr/axi_valid/axi_ready/axi_data/axi_resp_valid`）
      → **AXI4-full 读通道**（`ar_addr/ar_len/ar_size/ar_burst/ar_id/ar_valid/ar_ready` +
      `r_data/r_resp/r_last/r_id/r_valid/r_ready`）⇒ ⇒ **只读 ⇒ 无写通道**（design §1.1 ①）
- [ ] **T1.2** fill 路径：单拍 `zext<128>(axi_data)` → **4-beat burst**（`ARLEN=3`/`ARSIZE=2`/`ARBURST=INCR`）
      + `beat0..3` + `r_last` 收尾 ⇒ ⇒ ⚠️ **拼整行 MUST 用当拍 `r_data`**（避 S3 的 off-by-one
      ⇒ 否则 lane 3 恒 0）
- [ ] **T1.3** `ready` 语义**保持 I 侧既有契约**（design §1.1 ②）：hit ⇒ 1 拍后数据到；
      miss ⇒ 0，fill 完成后 ⇒ 1（新增 `fill_done` 状态位）⇒ ⇒ **MUST NOT** 照搬 D 侧的组合 ready
- [ ] **T1.4** `miss` ⇒ 触发 AR 通道（锁存 AR 握手）+ 块地址（`& 0xFFFFFFF0`）⇒ ⇒
      `ar_valid` 不得依赖 `ar_ready`（`include/cpu/AGENTS.md` 的 cache wrapper 硬约束）

## P2 · 读路径：lane 选择（**修「假 line」**）

- [ ] **T2.1** 读选择：`bits<31,0>(cache_data)` → **按 `addr[3:2]` 选 lane**（4 选 1）
      ⇒ ⇒ 数据源是 `sread`（1 拍）⇒ ⇒ 选择 MUST 在拿到 line 之后
- [ ] **T2.2** **`i_cache_async.h` 零 diff**（命中延迟维度不变 ⇒ ADR-041 变体族不扩张）

## P3 · 爆炸半径（同批改写）

- [ ] **T3.1** `include/cpu/cache/i_cache_prefetch.h`：内层实例化 + **AXI 侧**接线（5 根 → full 读通道）
      ⇒ ⇒ **前端转发不变** ⇒ ⇒ 爆炸半径比 ADR 估的**更小**
- [ ] **T3.2** `tests/chlib/test_cache_sync.cpp`（5 个双后端 pair）：旧激励 → burst
- [ ] **T3.3** `tests/chlib/test_cache_complete.cpp`（ICache fill 用例）
- [ ] **T3.4** `examples/riscv-mini/tests/test_cache_smoke.cpp`（双后端 wrapper 用例）
- [ ] **T3.5** 其余消费者（`test_cache_async_dual` / `test_cache_pipeline_integration` /
      `test_cache_simple` / `test_phase4_integration`）

## P4 · 判据 + capability

- [ ] **T4.1** **lane 寻址**判据（AC1）：一行 **4 个不同字**（手推 `0x11111111`/`0x22222222`/
      `0x33333333`/`0x44444444`）· fill 后依次读 offset 0/4/8/12 ⇒ 各返回对应字 ⇒
      ⇒ **修复前 MUST 红**
- [ ] **T4.2** **burst fill** 判据（AC2）：断言 `ARLEN=3`/`ARSIZE=2`/`ARBURST=INCR`
- [ ] **T4.3** **命中延迟**判据（AC3）：hit 后 **1 拍**数据就绪
- [ ] **T4.4** **反向判据**（AC8）：把读选择改回 `bits<31,0>` ⇒ ⇒ AC1 MUST 红（实测）
- [ ] **T4.5** 新 capability **`rv32i-i-l1-timing`**（承载 I-L1 契约）⇒ ⇒ 补上
      `openspec/specs/rv32i-dcache-timing/spec.md:13` 的**悬空声明**
- [ ] **T4.6** `cpu-cache-subsystem` 的 delta（ADDED ×2：I 侧 fill 契约 + I 侧 lane 寻址契约）

## P5 · 回归与收口

- [ ] **T5.1** 全量重建 0 error · `ctest` **全绿**（285/285）· 5 个既有护栏全绿 ·
      mini 线产物端口表**逐字不变**（I 侧**无生产消费者** ⇒ 行为面零变化）
- [ ] **T5.2** `openspec validate --strict` exit 0 · `verify_openspec_delta.sh` 4/4
- [ ] **T5.3** ADR-078 加 dated 订正（**只读 I-L1 只需 full 读通道** · **决策未变**）
- [ ] **T5.4** 母 improvement `rv32i-soc-platform-2026-11` 的 `进度:` 回填
- [ ] **T5.5** `verifier` 独立验收（AC1–AC8 逐条工具证据 + 遗留清扫）

## P6 · 归档

- [ ] **T6.1** `openspec archive rv32i-i-l1-axi-full-2026-11 --yes`

---

## 硬约束

1. **D7 的两处 MUST 一起改**（fill + 读选择）⇒ 只做其一 ⇒ **review 阻塞**（ADR-078 R7）。
2. **`ready` MUST 保持 I 侧既有语义**（「已接受 · 1 拍后数据到」）⇒ ⇒ 照搬 D 侧的组合 ready
   ⇒ **review 阻塞**（会破坏取指侧的既有契约）。
3. **拼整行 MUST 用当拍 `r_data`** ⇒ ⇒ 用未装载的 `beat3` ⇒ **review 阻塞**（lane 3 恒 0）。
4. **`i_cache_async.h` MUST 零 diff**（命中延迟维度不变 ⇒ ADR-041 变体族不扩张）。
5. **爆炸半径 MUST 同批清零**（wrapper + 4 个测试文件）⇒ 漏改 ⇒ 编译失败或测试恒绿 ⇒ **review 阻塞**。
6. **本批 MUST NOT 动** `include/cpu/pipeline/**` · `examples/riscv-mini/src/**` · `examples/riscv-full/**`
   ⇒ ⇒ 取指反压与核的 full 主口属 **S4c-1b**。
