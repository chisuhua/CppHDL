# Design: S4c-1a —— I-L1 的 AXI4-full 化 + 修「假 line」

> 决策依据 = ADR-078 的 **D7**；参照实现 = `include/cpu/cache/d_cache_sync.h`（S3 第 1 阶段 · `81097dc`）。

---

## 1. I 侧 vs D 侧：逐项差异（**照搬前必须先看清**）

| 维度 | **I 侧**（`i_cache_sync.h`）| **D 侧**（`d_cache_sync.h` · 参照）|
|---|---|---|
| 前端 | `addr/req` → `data/ready/miss`（**只读** · 无 `wdata`/`we`/`strbe`）| `addr/wdata/req/we/strbe` → `rdata/ready/miss` |
| **命中延迟** | **1 拍 `sread`**（`data_mem_way0.sread(index)` · `:140-141`）| **组合 `aread`** |
| **`ready` 语义** | `:157` `select(hit, true, io().axi_ready)` ⇒ **「请求已被接受，1 拍后数据到」** | **组合**「hit 或数据已就绪」⇒ **「数据此刻可用」** |
| `miss` | 组合（hit 取反 · `:136,158`）| 组合（同）|
| AXI 请求 | `:167-168` `axi_valid = miss & req` · `axi_addr = 块地址`（`& 0xFFFFFFF0`）| AR 通道 + `ar_len=3`/`ar_size=2`/`ar_burst=INCR` |
| fill | 单拍 `zext<128>(axi_data)`（`:179,183` ⇒ **只填低 32 位**）| 4-beat burst · `beat0..3` · `r_last` 收尾 · **当拍 `r_data`** 拼整行 |
| 读选择 | `bits<31,0>(cache_data)`（`:151` ⇒ **恒取首字**）| `lane_sel = bits<3,2>(addr)` + 4 选 1（`:158-186`）|

### 1.1 三个**必须自己判断**的差异（不能照抄）

| # | 差异 | 结论 |
|---|---|---|
| **①** | **I 侧是只读的** ⇒ AXI 侧只需 **AR + R 读通道**（**无** AW/W/B 写通道）⇒ ⇒ 端口数比 D 侧少（D 侧 19 根含写侧）| ⇒ **精确化 ADR-078 的「AXI4 full 5 通道」表述**：I-L1 只需 **full 读通道**（`ar_*` + `r_*` + `r_last` + `ar_id`）⇒ ⇒ 与 ADR-078 D7 的意图一致但更省 ⇒ ⇒ 需在 ADR-078 加 dated 订正（**决策未变**：仍是 full 协议、仍走 burst）|
| **②** | **`ready` 语义不同**（I = 「已接受」，D = 「数据可用」）⇒ ⇒ **不可照搬 D 侧的组合 ready** | I 侧 MUST 保持既有契约：**hit ⇒ 1 拍后数据到；miss ⇒ 0，fill 完成后 ⇒ 1** ⇒ ⇒ 需新增 `fill_done` 状态位（hit 路径**零延迟变化**）|
| **③** | **命中数据是 `sread`（寄存器读）** ⇒ ⇒ 1 拍后才拿到 128 位 line | lane 选择 MUST 在**拿到 line 之后**做 ⇒ ⇒ `hit_word = select(lane_sel, bits<31,0>(line), bits<63,32>(line), …)`（与 D 侧同构，只是数据源是 `sread` 而非 `aread`）|

### 1.2 ⚠️ 必须避开的坑（S3 实测）

`81097dc` 记录：**`fill_complete` 那拍 `beat3` 寄存器尚未装载** ⇒ ⇒ 直接用它拼整行 ⇒
**lane 3 恒 0** ⇒ ⇒ 拼整行 MUST 用**当拍 `r_data`**（D 侧 `:250-265` 的做法）⇒ ⇒ I 侧同构实现。

## 2. 改造清单（逐文件）

| # | 文件 | 动作 |
|---|---|---|
| 1 | `include/cpu/cache/i_cache_sync.h` | AXI 侧 5 根简化口 → **full 读通道**；fill → 4-beat；读 → lane 选择；新增 `fill_done` |
| 2 | `include/cpu/cache/i_cache_prefetch.h` | 内层实例化 + **AXI 侧**接线（5 根 → full 读通道）；**前端转发不变** ⇒ ⇒ 爆炸半径比 ADR 估的**更小** |
| 3 | `tests/chlib/test_cache_sync.cpp` | 5 个双后端 pair：旧激励（单拍 `axi_resp_valid`）→ burst；**新增 offset≠0 lane 判据** |
| 4 | `tests/chlib/test_cache_complete.cpp` | ICache fill 用例（地址 `0x1000` offset 0）→ burst + **新增 offset≠0** |
| 5 | `examples/riscv-mini/tests/test_cache_smoke.cpp` | 双后端 wrapper 用例（断言简化口形状）→ full 读通道 |
| 6 | `tests/chlib/test_cache_async_dual.cpp` · `test_cache_pipeline_integration.cpp` · `test_cache_simple.cpp` · `tests/chlib/test_phase4_integration.cpp` | 随 AXI 侧形态变化的同批改写 |
| 7 | `include/cpu/cache/i_cache_async.h` | **零 diff**（Async 变体无 AXI 侧 ⇒ 命中延迟维度不变） |

## 3. 判据形态（**手推** · 禁抄仿真输出）

### 3.1 lane 寻址（AC1）

- 构造一行 **4 个不同的字**（手推值 `0x11111111` / `0x22222222` / `0x33333333` / `0x44444444`）
- fill 后**依次读** offset `0` / `4` / `8` / `12` ⇒ ⇒ **各返回对应的字**
- ⇒ **修复前 MUST 红**（现状恒读首字 ⇒ 4 次读全返回 `0x11111111`）

### 3.2 burst fill（AC2）

- 断言 fill 期间 AXI 侧发出 `ARLEN=3` / `ARSIZE=2` / `ARBURST=INCR`
- ⇒ ⇒ 旧的单拍 `axi_resp_valid` 激励**完不成 fill**（⇒ 既有测试 MUST 改写）

### 3.3 命中延迟不变（AC3）

- hit 后 **1 拍**数据就绪（与 `DataTCM` 的 `sread` 对齐）⇒ 断言拍数

## 4. 失败模式与缓解

| # | 失败模式 | 缓解 |
|---|---|---|
| F1 | **只改 fill 不改读**（ADR-078 R7 明确禁止）⇒ ⇒ 「假 line」缺陷被冻结 ⇒ 判据全绿 | D7 明文「两处一起改」+ AC1 的 offset≠0 用例（修复前 MUST 红）|
| F2 | **off-by-one** ⇒ lane 3 恒 0 ⇒ AC1 绿但**数据错** | AC1 用 4 个**不同**值（lane 3 恒 0 会红）· 拼整行用当拍 `r_data` |
| F3 | **照搬 D 侧的组合 ready** ⇒ ⇒ I 侧的「1 拍后数据到」契约被破坏 ⇒ mini 线（虽无生产消费者）语义漂移 | §1.1 ② 明确保留 I 侧 ready 语义 + AC3 的拍数断言 |
| F4 | **爆炸半径漏改**（wrapper / 4 个测试文件）⇒ ⇒ 编译失败或测试恒绿 | AC4 用 grep 判「零遗留旧形态」 |
| F5 | **把 `ready` 当成「数据可用」** ⇒ ⇒ 与 `IfStage` 的「instr_ready 恒 1」前提冲突 ⇒ ⇒ 取指侧行为漂移 | 本批**不接核**（I 侧无生产消费者）⇒ ⇒ 只在 S4c-1b 处理取指反压 |

## 5. 与后续批次的边界

- **本批不改**：`include/cpu/pipeline/**`（取指协议）· `examples/riscv-mini/src/**`（核）· `examples/riscv-full/**`
  （full 线核）⇒ ⇒ **mini 线零行为变化**（I 侧**无生产消费者**：核的取指仍走 I-TCM 直连）
- **S4c-1b 接手**：把本批的 I-L1 接进 full 线取指路径（fork `IfStage` 以支持 `instr_ready` 反压）
  + 核的 AXI4-full 取指主口 + full 线核 / pipeline / 顶层
