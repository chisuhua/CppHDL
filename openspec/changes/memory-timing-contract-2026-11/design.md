# Design: 存储器时序契约（D-TCM 寄存读 + MEM 数据对齐 A0）

> **变更提案** = `proposal.md` · **架构依据** = `docs/adr/ADR-068-*`（Accepted · V1–V10）+
> `docs/adr/ADR-069-mem-data-alignment.md`（**Accepted** · V1–V15 · 决策 = **A0**）
> ⚠️ **本文档只讲「怎么实施」** —— 「做什么 / 为什么」在 `proposal.md`；
> 「契约说什么」在 `specs/**`（delta，archive 时并入主 spec）。
> ⚠️ **顺序**：本 change MUST 先于 **B4**（时序金标只算一遍）。

## 1. 实施顺序（**不可乱序**）

```
T1（D-TCM 改 sread）
  ⇒ T4（写相位契约）
    ⇒ T2（A0 三处强制结构变更）      ← ⚠️ 三者同批，漏一 ⇒ ⇒ 静默错值
      ⇒ T3（反压 data_ready 真接线）
        ⇒ T5（时序金标手推重算）
          ⇒ T6（判别性用例：紧邻 SW/LW + 反压 DUT）
```

**为什么这个顺序**：
- **T1 先于 T4** —— 写相位只有在「读是 1 拍」之后才有「相位」这个概念。
- **T2 三处同批** —— ① 前推档位 2 改指 M2、② D11 改指下一提交者值、③ 停顿链加一级
  ⇒ ⇒ **任一单独改**都会让数据落在**错误的级** ⇒ ⇒ 表现为「值错但无报错」
  ⇒ ⇒ ⇒ **必须同批**（ADR-069 §3.3）。
- **T3 先于 T5** —— 反压会改停顿拍数 ⇒ ⇒ 先接反压再算金标，否则金标要算两遍。
- **T6 最后** —— 判别性用例依赖新时序的最终形态。

## 2. T1 —— D-TCM 的 **load 通路**改寄存读（⚠️ **不是**单行换 `sread`）

⚠️ **实施期实测订正（2026-11-13）**：`rv32i_tcm.h:113` 的 `aread` **同时**服务**两条**路径：

| 路径 | 位置 | 需求 | 改法 |
|---|---|---|---|
| ① **load 数据通路** | `rv32i_tcm.h:128` `io().rdata <<= old_word;` | 固定 1 拍寄存读（BRAM 可推断） | ⇒ **新增 `sread` 读口** |
| ② **字节写的读-改-写** | `rv32i_tcm.h:113-117` | **同拍**合并（组合读返回边沿前的旧值） | ⇒ **保持 `aread` 不变** |

⇒ ⇒ ⚠️ **「把 `aread` 换成 `sread`」的单行改法会静默破坏 `sb` / `sh`**
（合并结果变成上一拍的值）⇒ ⇒ **判据 MUST 抓它**。
⇒ ⇒ ✅ `ch_mem` 支持**双读口**（`aread` + `sread`）⇒ ⇒ 同址并存**可行**。

```cpp
// include/cpu/pipeline/rv32i_tcm.h（改后）
// ② 字节写 RMW：**保持** 0 拍组合读
ch_uint<DATA_WIDTH> old_word;
old_word <<= memory.aread(io().addr);                  // ← 不变
auto lane_mask = axi4::detail::byte_lane_mask<DATA_WIDTH>(io().strbe);
auto merged_word = axi4::detail::byte_lane_apply<DATA_WIDTH, DATA_WIDTH>(
    old_word, io().wdata, lane_mask);
memory.write(io().addr, merged_word, io().write & io().valid);

// ① load 通路：**新增** 1 拍寄存读
auto load_data = memory.sread(io().addr, ch_bool(true)); // ← 新增
io().rdata <<= load_data;                               // ← 由 old_word 改为 load_data
```

⚠️ **`sread` 的 `enable` 是电平**（不是脉冲）⇒ ⇒ 取指侧（`InstrTCM:56`）已是这个形态 ⇒ ⇒ D-TCM 与其**对齐**。

⇒ ⇒ **产物判据**：D-TCM 的 **load 通路** MUST NOT 退化为「容量:1 组合 mux」
⇒ ⇒ ⚠️ 判据 MUST **只看 load 通路** —— 字节写 RMW 的组合读**合法存在**（否则 ⇒ ⇒ 恒红）。

## 3. T2 —— A0 的三处强制结构变更（**同批**）

| # | 位置 | 改什么 | 漏了会怎样 |
|---|---|---|---|
| ① | 前推**档位 2** | 数据源 `memwb_*` → **M2** | 前推给出**上一拍**的旧值 ⇒ ⇒ 错值 |
| ② | **D11**（WB→ID 旁路） | 源 → 「**下一提交者值**」 | ID 级读到**已退休**的值 ⇒ ⇒ 错值 |
| ③ | `hazard_unit` | 停顿链**加一级**（`stall_hold_d3` 或等价） | 停顿**短 1 拍** ⇒ ⇒ 数据未就绪就被采样 ⇒ ⇒ 错值 |

⇒ ⇒ **三者都无独立报错** ⇒ ⇒ 必须**同批 + 同 commit**（ADR-069 V13–V15）。

⚠️ **实施期实测澄清（2026-11-13）**：三者**不是三个独立选择**，而是 **T1 的三个必然推论** ——
根因 = 「D-TCM 的 load 数据晚 1 拍可用（`sread`）」：
  ① 前推档位 2 的源必须改指**再往下一级**（否则喂的是**过期一拍**的值）
  ② D11 的源必须改指「**下一提交者值**」（否则喂的是**已退休**的值）
  ③ 停顿链必须加一级（否则消费者在数据就绪**之前**采样 ⇒ ⇒ 读到上一拍旧值）
⇒ ⇒ ⇒ **所以它们 MUST 同批**：任一单独改 ⇒ ⇒ 数据落在**错误的级** ⇒ ⇒ 「值错但无报错」。

⇒ ⇒ ⚠️ **停顿链加一级 ⇒ ⇒ `hazard_unit` 对外端口契约不变**（`stall` 仍是布尔输出）
⇒ ⇒ ⇒ 对外**无 API 变更** ⇒ ⇒ 不需要公开面登记（**除**下面的 `data_ready`）。

## 4. T3 —— `data_ready` 变真

- `include/cpu/pipeline/rv32i_pipeline.h:61` 的 `data_ready` 从**死端口**接真信号。
- MEM 级在 `data_ready == false` 时：冻结 MEM/WB 寄存器 + 向前级发 stall。
- ⚠️ **语义登记**：`data_ready` = 「**固定 1 拍延迟**」的读就绪，**不是握手**
  ⇒ ⇒ 契约里 MUST NOT 写成「等待就绪 / 往返」。
- ⚠️ **判别力前提**：MUST 有**能拉低 `data_ready`** 的测试专用 DUT
  ⇒ ⇒ 否则该通路**零覆盖** ⇒ ⇒ 判据恒绿。

## 5. T4 / T6 —— 写相位与判别性用例

- **T6（read-during-write）**：紧邻 `SW x1, 0(x2)` / `LW x3, 0(x2)`（**同址 · 零间隔**）
  ⇒ ⇒ 断言 `x3 == 写入值`。
  ⚠️ **现固件的「隔 4 字」形态 MUST NOT 冒充该判据**（`SW`@word16 / `LW`@word20）⇒ ⇒ 新用例 MUST **紧邻**。
- **相位**：写使能 MUST 与被覆盖地址**同拍** ⇒ ⇒ 判据 MUST 覆盖 `sb`/`sh` 的
  **4 字节车道 × 2 半字车道**（现有仅 2 个用例 ⇒ 判别力不足）。
- **相位错位探针**：故意把写使能错开一拍 ⇒ ⇒ 判据 MUST 转红（证明有判别力）。

## 6. T5 —— 时序金标手推重算

受影响的期望值（**全部 MUST 手推**）：

| 期望值 | 旧基线 | 变化来源 |
|---|---|---|
| `perf_instructions` | 178 | 停顿拍数变化 ⇒ ⇒ 退休数变化（**具体值 MUST 手推**） |
| `load_use_stalls` | 6 | 停顿 3 → **4** 拍 |
| `control_stalls` | 4 | 控制停顿拍数平移 |
| store 拍数 | 5 | MEM/WB 装载点后移 1 拍 |
| `dbg_id_pc` 逐拍序列 | — | 拍号平移 |
| 24 条写回序列 | — | 同上 |

⚠️ **禁抄仿真输出** —— 两后端同源 ⇒ ⇒ parity 对该族缺陷**结构性失明**
⇒ ⇒ **期望值 MUST 手推** ⇒ ⇒ **且手推值本身要能判别**（禁对称值 / 位回文）。

⚠️ **`rv32i-commit-path` 的基线 MUST MODIFIED 订正**（标题逐字保留 · 见 delta）。

## 7. 验证策略（**判别力优先**）

| 层 | 判据 |
|---|---|
| 产物 | D-TCM 读 MUST NOT 是组合 mux（BRAM 可推断的前提） |
| 产物 | `verilator --lint-only` 干净 · `scripts/verify_flat_rtl_emit.sh` 绿 |
| 行为 | T6 紧邻 SW/LW 判据（read-during-write）· 相位判据（4×2 车道全覆盖） |
| 行为 | **相位错位探针 MUST 转红**（判别力自证） |
| 反压 | 能拉低 `data_ready` 的 DUT + 有效链保持断言 |
| 不回归 | 20 项 golden 全绿（取指填充不变量 + 3 存储 golden + sb/sh lane + 3 冲刷未写回 + 4 派生计数器） |
| 不回归 | `test_rv32i_soc_dual_parity` 双后端 parity 仍成立（⚠️ **但它不是证据** —— 两后端同改 ⇒ 恒绿） |
| 不回归 | 计数锚点同步（`CTEST_TOTAL` / `DUAL_BACKEND_PARITY_PAIRS` / `VERILATOR_E2E_TOTAL` 如有变动） |

## 8. 风险与回滚

| 风险 | 缓解 |
|---|---|
| 三处结构变更漏一 ⇒ ⇒ **静默错值** | 同批 + 同 commit + 每处一条判别性用例 |
| 金标手推错 ⇒ ⇒ 假绿 | 手推推导**逐条留痕**；变异自证（还原缺陷 ⇒ 判据转红 ⇒ 还原复绿） |
| 改 `hazard_unit` ⇒ ⇒ 影响既有 load-use 停顿测试 | 对外端口契约不变 ⇒ ⇒ 既有测试**只应**在拍号上平移 ⇒ ⇒ 平移不符 ⇒ ⇒ 查三处强制变更 |
| 回滚 | 本 change 独立 ⇒ ⇒ 可整体 revert（时序金标随之还原） |

---

## 9. 逐文件精确实施清单（**2026-11-13 实测定位** · 让落地从「探索」变成「机械执行」）

> ⚠️ **同批 + 同 commit**（ADR-069 §3.3）⇒ ⇒ 下面 **T1 / T2 必须一个 commit 落地**。

| # | 文件 | 位置 | 改什么 | 备注 |
|---|---|---|---|---|
| **T1-a** | `include/cpu/pipeline/rv32i_tcm.h` | `:112-117` | **保持** `aread`（字节写 RMW） | **不动** |
| **T1-b** | `include/cpu/pipeline/rv32i_tcm.h` | `:128` | `io().rdata <<= old_word;` ⇒ ⇒ `io().rdata <<= load_data;`（`load_data = memory.sread(io().addr, ch_bool(true))`，**新增**一行） | ⚠️ 唯一新增读口 |
| **T2-ⓐ** | `include/cpu/pipeline/rv32i_pipeline.h` | 成员区（`exmem_*` 之后 · 约 `:158`） | **新增 M2 寄存器组**：`m2_is_alu` · `m2_is_jump` · `m2_is_load` · `m2_rd_addr` · `m2_alu_result` · `m2_link_value` · `m2_mem_data` · `m2_valid` | M2 = 「MEM 级输出」的寄存器化点 |
| **T2-ⓑ** | `include/cpu/pipeline/rv32i_pipeline.h` | `:427-437`（`memwb_*.next =`） | 装载源 `exmem_valid` / `mem_stage.io().*` ⇒ ⇒ **`m2_valid` / `m2_*`**；`memwb_valid->next` ⇒ ⇒ `m2_valid` | MEM/WB 延后 1 拍 |
| **T2-②** | `include/cpu/pipeline/rv32i_pipeline.h` | `:584-585`（`fwd_mux.io().mem_wb_result`） | 源 `wb_stage.io().write_data` ⇒ ⇒ **`m2_write_data`**（与 `wb_stage` 同构的组合 select，源为 `m2_*`） | 前推档位 2 |
| **T2-③** | `include/cpu/pipeline/rv32i_pipeline.h` | `:226-234`（D11 旁路判据 + 数据源） | 判据 `wb_write_en` / `wb_stage.io().rd_addr_out` ⇒ ⇒ **`m2_write_en` / `m2_rd_addr`**；数据源 ⇒ ⇒ **`m2_write_data`** | 「下一提交者值」 |
| **T2-④** | `include/cpu/pipeline/hazard_unit.h` | `:141-142` + `:253-268` | **新增** `ch_reg<ch_bool> stall_hold_d3{false, "stall_hold_d3"};` + `stall_hold_d3->next = stall_hold_d2;` + `io().stall = … | stall_hold_d3;` | 固定 3 拍 ⇒ ⇒ **4 拍** |

⚠️ **实施时的两个易错点**：
1. **`m2_write_data` MUST 是「下一提交者值」**，不是 `wb_stage.io().write_data`（后者已被延后 1 拍 ⇒ ⇒ 喂的是**过期一拍**）
2. **M2 的数据源 MUST 是 MEM 级的输出**（`mem_stage.io().alu_result_out` / `mem_stage.io().mem_data`）
   ⇒ ⇒ **不是** `exmem_*`（那不含 MEM 的读数据）⇒ ⇒ 否则 load 的数据会被丢掉一拍

⚠️ **T5 的前置**（实施期实测发现）：草案里的金标现值
`perf_instructions = 178` / `load_use_stalls = 6` / `control_stalls = 4`
**未在 `tests/` / `examples/` 里作为断言常量出现** ⇒ ⇒ 它们**可能**是 perf report 的**观测值**
⇒ ⇒ ⇒ **T5 开工前 MUST 先定位断言位置**（`kExpectedWritebacks` 24 条写回序列 +
`obs_perf_*` 观测端口）⇒ ⇒ ⚠️ **禁**把「观测值」当「断言值」。
