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

## 2. T1 —— D-TCM 改寄存读

```cpp
// include/cpu/pipeline/rv32i_tcm.h（现状）
ch_uint<DATA_WIDTH> old_word;
old_word <<= memory.aread(io().addr);          // 0 拍组合读

// 改后
auto read_data = memory.sread(io().addr, ch_bool(true));   // 1 拍寄存读
```

⚠️ **注意 `sread` 的语义**：`sread(addr, enable)` 的 `enable` 是**电平**（不是脉冲）
⇒ ⇒ 取指侧（`InstrTCM`）已是这个形态 ⇒ ⇒ D-TCM 与其**对齐**。

⇒ ⇒ **产物判据**：D-TCM 的读 MUST NOT 退化为「容量:1 组合 mux」
⇒ ⇒ 判法 = 产物里 MUST NOT 出现把整个存储体按地址组合选通的连续赋值。

## 3. T2 —— A0 的三处强制结构变更（**同批**）

| # | 位置 | 改什么 | 漏了会怎样 |
|---|---|---|---|
| ① | 前推**档位 2** | 数据源 `memwb_*` → **M2** | 前推给出**上一拍**的旧值 ⇒ ⇒ 错值 |
| ② | **D11**（WB→ID 旁路） | 源 → 「**下一提交者值**」 | ID 级读到**已退休**的值 ⇒ ⇒ 错值 |
| ③ | `hazard_unit` | 停顿链**加一级**（`stall_hold_d3` 或等价） | 停顿**短 1 拍** ⇒ ⇒ 数据未就绪就被采样 ⇒ ⇒ 错值 |

⇒ ⇒ **三者都无独立报错** ⇒ ⇒ 必须**同批 + 同 commit**（ADR-069 V13–V15）。

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
