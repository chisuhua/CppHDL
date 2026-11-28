# Proposal: S4c-1a —— I-L1 的 AXI4-full 化 + 修「假 line」（D7）

> **来源**：ADR-078（`docs/adr/ADR-078-dual-product-lines-and-full-line-scope.md` · **Accepted**）的
> **D7**（「原地升级 `include/cpu/cache/i_cache_sync.h`」· 范围 = **S3 完整形态**：两处一起改）
> + 母 improvement `rv32i-soc-platform-2026-11` 的 S4c 批次。
> **前置**：S4c-0（`2026-11-28-rv32i-full-product-line-2026-11`）**已归档**。
> **spec 变更**：`cpu-cache-subsystem` **ADDED ×2**（I 侧 fill 契约 + I 侧 lane 寻址契约）。

---

## Why

1. **I 侧存在与 D 侧同款的「假 line」缺陷** —— `i_cache_sync.h:151` 读 = `select(hit, bits<31,0>(cache_data), …)`
   ⇒ **恒取 line 低 32 位**；`:116` 算出的 `offset` 全文件**未参与数据选择**；
   `:179,183` fill = `zext<128>(axi_data)` 只填低 32 位 ⇒ ⇒ **同一 line 的 4 个地址恒读到首字**。
   ⇒ **G-7 §二⑥ 已把该形态定性为「不是性能问题，是功能错误」「读选 lane **无论协议都必须做**」**
   ⇒ ⇒ 本批 MUST 一并修（**不得**只做 fill 侧）。
2. **I 侧的 AXI 侧不是标准 AXI4** —— 只有 5 根**简化握手**（`axi_addr/axi_valid/axi_ready/axi_data/axi_resp_valid`），
   无 AR/R 分离、无 `len/size/burst`、无 ID、无 `r_last` ⇒ ⇒ 单拍读协议 ⇒ ⇒ fill 效率与
   `TcmAxiSlave`（**标准 AXI4-full slave**，D 侧已在用）**对不上** ⇒ ⇒ full 线接不上。
3. **S3 已有同构的参照实现** —— `d_cache_sync.h:158-265`（D 侧）：`lane_sel = bits<3,2>(addr)` ·
   4-beat burst fill（`ARLEN=3`/`ARSIZE=2`/`ARBURST=INCR`）· `r_last` 收尾 · 用当拍 `r_data` 拼整行。
   ⇒ ⇒ 本批是**同构复制**，风险与不确定性大幅降低。
4. **爆炸半径已知且可枚举** —— wrapper `i_cache_prefetch.h:92,104-108`（逐根驱动内层 5 根简化口）
   + 4 个测试文件（`test_cache_sync.cpp` / `test_cache_complete.cpp` / `test_cache_smoke.cpp` 双后端 /
   `test_cache_async_dual.cpp` 等）⇒ ⇒ 旧激励（单拍 `axi_resp_valid`、地址 offset 全 0）
   在 burst 下**完不成 fill** ⇒ ⇒ MUST 同批改写。
5. **既有测试对 lane 选择零判别力** —— 既有 I 侧 fill/read 地址全部 16 字节对齐（offset 恒 0）
   ⇒ ⇒ 必须新增 **offset≠0** 的**手推**判别性用例（ADR-078 AC8 / R7）。

## 范围拆分（**为什么本批不含 D14**）

ADR-078 的 D8 把 S4c-1 写成「取指接口 fork + I-L1 两处升级 + 核的 AXI4-full 取指口」三块。
实施期调研后**拆成两批**：

| 批 | 内容 | 依赖 |
|---|---|---|
| **S4c-1a**（**本批**） | **D7**：I 侧 cache 的 AXI 侧 full 通道化 + 修「假 line」 | 无（**对端 = 已存在的 `TcmAxiSlave`** ⇒ 可独立验证） |
| S4c-1b | **D14** 取指接口 fork（支持 `instr_ready` 反压）+ full 线 pipeline 变体 + 核的 AXI4-full 取指主口 | 依赖 1a（I 侧要有 full 对端）⇒ 另立 |

**拆分依据**：① D7 的验证对端 `TcmAxiSlave` 是**已存在的标准 AXI4-full slave**（D 侧在用）
⇒ ⇒ 1a **不需要**核的 full 取指主口 ⇒ ⇒ 可独立完成与验证；② D14 的核心难点是
**取指侧反压**（`instr_ready` 现在恒为 1 · `if_stage.h:370-372`）⇒ 该形态在仓内**从未被处理过**
（D 侧不走取指路径）⇒ ⇒ 与 D7 是**不同量级**的工作 ⇒ ⇒ 混在一批会让 D7 的判别力被 D14 的风险淹没。

## What Changes

### A. `include/cpu/cache/i_cache_sync.h` —— **两处一起改**（D7）

1. **fill 路径**：单拍 → **4-beat burst fill**（`ARLEN=3` / `ARSIZE=2` / `ARBURST=INCR`）
   ⇒ AXI 侧从 5 根简化口扩为 **AXI4-full 读通道**（`ar_*` + `r_*` + `r_last` + `ar_id`）
2. **读路径**：恒取低 32 位 → **按 `addr[3:2]` 选 lane**（4 选 1）⇒ ⇒ **「假 line」闭合**
3. **hit 延迟不变**（1 拍 `sread`）⇒ ⇒ `i_cache_async.h` **零 diff** ⇒ ADR-041 的变体族**不扩张**
4. ⚠️ **必须避开 S3 踩过的 off-by-one** —— `fill_complete` 那拍 `beat3` 寄存器**尚未装载**
   ⇒ ⇒ 拼整行 MUST 用**当拍 `r_data`**（否则 lane 3 恒 0）

### B. 爆炸半径（同批改写，缺一即悬挂）

- `include/cpu/cache/i_cache_prefetch.h`（wrapper：内层实例化 + 5 根简化口的接线）
- `tests/chlib/test_cache_sync.cpp` · `tests/chlib/test_cache_complete.cpp`
  （旧激励是单拍 `axi_resp_valid` ⇒ burst 下完不成 fill）
- `examples/riscv-mini/tests/test_cache_smoke.cpp`（双后端 wrapper 用例，断言简化口的形状）
- 其余 `ICacheSync` 消费者（`test_cache_async_dual` / `test_cache_pipeline_integration` /
  `test_cache_simple` / `test_phase4_integration`）

### C. 新增判别性用例（**offset≠0** · 手推）

- 同一 line 的 **4 个不同地址各返回各自的字**（手推 4 个不同值）⇒ ⇒ 修复前 **MUST 红**
- fill 走 **4-beat burst**（断言 `ARLEN=3` / `ARSIZE=2` / `ARBURST=INCR`）⇒ ⇒ 单拍激励**完不成 fill**

## 验收标准

- [ ] **AC1 lane 寻址（读选 lane）**：I 侧同一 line 的 4 个地址（offset 0/4/8/12）**各返回对应的字**
      （手推 4 个不同值）⇒ ⇒ **修复前 MUST 红**（现状恒读首字）
- [ ] **AC2 burst fill**：I 侧 fill MUST 走 **4-beat burst**（`ARLEN=3` / `ARSIZE=2` / `ARBURST=INCR`）
      ⇒ ⇒ 旧的单拍 `axi_resp_valid` 激励**完不成 fill**
- [ ] **AC3 hit 延迟不变 + Async 零 diff**：hit 仍是 **1 拍 `sread`**；`include/cpu/cache/i_cache_async.h`
      **零 diff** ⇒ ⇒ ADR-041 的变体族（命中延迟维度）不扩张
- [ ] **AC4 爆炸半径同批清零**：wrapper `i_cache_prefetch.h` + 4 个测试文件**已同批改写** ⇒ ⇒
      **零**遗留的简化口接线 / 单拍激励（grep 旧形态为 0）
- [ ] **AC5 D 侧形态对齐**：I 侧的 fill/读选择形态与 `d_cache_sync.h` 的 **S3 形态同构**
      （`lane_sel = bits<3,2>(addr)` · 4-beat · `r_last` 收尾）⇒ ⇒ 且**避开 S3 的 off-by-one**
      （lane 3 MUST 有值 ⇒ 用当拍 `r_data` 拼整行）
- [ ] **AC6 mini 线零行为变化**：`ctest` **全绿**（285/285）· 5 个既有护栏全绿 ·
      mini 线产物端口表**逐字不变**（I 侧无生产消费者 ⇒ 行为面零变化）
- [ ] **AC7 I-L1 capability 缺口**：新建 capability 承载 **I-L1** 契约 ⇒ ⇒ 补上
      `openspec/specs/rv32i-dcache-timing/spec.md:13` 的**悬空声明**（「取指侧的 I-L1
      各有独立 capability」—— 该 capability **在 specs 下不存在**）
- [ ] **AC8 反向判据**：把 I 侧的读选择改回 `bits<31,0>` ⇒ ⇒ **AC1 MUST 红**（证明判据有判别力）

## Out of Scope（明确不做）

1. **取指接口 fork**（D14）· **核的 AXI4-full 取指主口** ⇒ S4c-1b
2. **full 线核 / pipeline / 顶层** ⇒ S4c-1b
3. **`i_cache_async.h`**（Async 变体）⇒ 零 diff
4. **I-L1 接入核**（接进取指路径）⇒ S4c-1b
5. **mini 线的任何行为 / 产物 / 端口面改动**
