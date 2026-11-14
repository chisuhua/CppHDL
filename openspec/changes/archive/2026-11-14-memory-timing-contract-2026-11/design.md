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
| **T2-③** | `include/cpu/pipeline/rv32i_pipeline.h` | `:226-234`（D11 旁路判据 + 数据源） | ⚠️ **2026-11-13 订正**（ADR-069 §7.1.2 · 两级优先旁路 · **不是**替换）⇒ ⇒ **新增 M2 档**（判据 = `m2_valid & (m2_is_alu\|m2_is_load\|m2_is_jump) & (m2_rd_addr != 0)` ∧ `m2_rd_addr == rs{1,2}_addr_id`；数据 = **M2 级同构写回 mux**，叶 = M2 寄存器 + `sread` 输出寄存器 ⇒ 不成环）⇒ ⇒ **WB 档逐字保留**（判据 `wb_write_en` / `wb_stage.io().rd_addr_out`；数据 `wb_stage.io().write_data`）⇒ ⇒ **优先级 = M2 档 > WB 档**（同拍命中同一 rd 时 M2 更年轻 ⇒ youngest-wins）⇒ ⇒ 🔴 **`wb_stage` 的写回 mux MUST NOT 前移**（前移 ⇒ `reg_file.write` 单写口采到更年轻指令 ⇒ 真正提交的那条**永不写回**）⇒ ⇒ ⚠️ **D11 的 M2 档 MUST 排在「load 侧车道 mux 改用 M2 地址/funct3」之后**（否则按**后继指令的地址**选车道 ⇒ 静默错 lane） |
| **T2-④** | `include/cpu/pipeline/hazard_unit.h` | `:141-142` + `:253-268` | **新增** `ch_reg<ch_bool> stall_hold_d3{false, "stall_hold_d3"};` + `stall_hold_d3->next = stall_hold_d2;` + `io().stall = … | stall_hold_d3;` | 固定 3 拍 ⇒ ⇒ **4 拍** |

⚠️ **实施时的两个易错点**：
1. **`m2_write_data` MUST 是「下一提交者值」**，不是 `wb_stage.io().write_data`（后者已被延后 1 拍 ⇒ ⇒ 喂的是**过期一拍**）
2. **M2 的数据源 MUST 是 MEM 级的输出**（`mem_stage.io().alu_result_out` / `mem_stage.io().mem_data`）
   ⇒ ⇒ **不是** `exmem_*`（那不含 MEM 的读数据）⇒ ⇒ 否则 load 的数据会被丢掉一拍

### 9.1 ⚠️ **实施期订正（2026-11-13）**：§9 的 M2 清单**与 ADR-069 A0-① 冲突** ⇒ 已订正

⚠️ **本节订正 §9 上表的 M2 内容**（上表的 8 个「数据寄存器」清单**作废**）⇒ ⇒ 依据 =
`docs/adr/ADR-069-mem-data-alignment.md` §3.3 **A0-①** + §5 **V5**。

**为什么必须订正**：`sread` ⇒ **load 数据晚 1 拍可用** ⇒ ⇒ 车道选择（`mem_stage.h:101-114`）
在 **MEM 拍（T）** 已经**没有数据可选** ⇒ ⇒ 车道选择**必须**搬到 **T+1** ⇒ ⇒
而 T+1 的地址**不是** `EX/MEM` 的（那时 EX/MEM 已是**后继**指令）⇒ ⇒ **必须**用
**M2 锁存的地址 / funct3** ⇒ ⇒ ⇒ **这正是 ADR-069 V5 明写的那条**
（「load 侧车道选择 **MUST 搬进 M2 的时序**（用 M2 锁存的地址/funct3）」）。

⇒ ⇒ **正确的 M2 内容**（= 元数据 + 原始字，**不是**数据寄存器）：

| 成员 | 宽度 | 装载源（E_{T+1}） | 作用 |
|---|---|---|---|
| `m2_valid` | 1 | `exmem_valid` | MEM/WB′ 的装载使能 |
| `m2_is_alu` / `m2_is_jump` / `m2_is_load` | 1 | `exmem_is_alu` / `exmem_is_jump` / `exmem_is_load` | WB 写回 mux 的选择位 |
| `m2_rd_addr` | 5 | `mem_stage.io().rd_addr_out` | 写回目标 |
| `m2_link_value` | 32 | `exmem_link_value` | JAL/JALR 的返回地址（**MUST 同拍 transit**，否则写回**后继**的返回地址） |
| `m2_alu_result` | 32 | `exmem_alu_result` | ALU 结果 **兼** 访存地址（load/store 时 = 地址）⇒ ⇒ 车道选择用它的低 2/1 位 |
| `m2_funct3` | 3 | `exmem_funct3` | 车道 / 扩展方式的选择位 |
| `m2_word` | 32 | `io().data_read_data`（= TCM 的 `sread` 输出，在 T+1 = **data(A_i)**） | **原始字**（未做车道选择） |

⇒ ⇒ ⚠️ **`m2_word` 是「原始字」而不是「已选好的值」** ⇒ ⇒ 因为**车道选择需要原始字与地址同拍**
⇒ ⇒ 而原始字只在 T+1 可用 ⇒ ⇒ ⇒ **选择只能在 T+2 做**（此时 M2 的三个字段都已是**寄存器值**）
⇒ ⇒ ⇒ **MEM/WB′ 的数据入端 = `select(m2_is_load, lane_select(m2_word, m2_alu_result, m2_funct3), 0)`**
（**组合**函数 ⇒ ⇒ **不需要**额外的「已选好值」寄存器 ⇒ ⇒ ⇒ M2 只有 **9** 个成员）。

⇒ ⇒ 🔴 **连带面（ADR-069 A0-① + V5 的必然推论，本 change 之前未登记）**：
**load 的字节道 / 半字道 / 符号扩展语义从 `MemStage` 移到流水线（M2 的下游）**
⇒ ⇒ `MemStage` 的 `io().mem_data` 语义**必须变**（从「已选好 + 已扩展的值」⇒ ⇒ 「**原始字**」）
⇒ ⇒ ⇒ **`MemStage` 是公开组件** ⇒ ⇒ **公开面变更** ⇒ ⇒ MUST 走
`public-api-surface.md` + `docs/CHANGELOG.md` 登记 ⇒ ⇒ 且 `include/cpu/AGENTS.md`
的「load 侧的字节道契约」段 MUST 同步改写（**契约载体迁移**）。
⇒ ⇒ ⇒ ⚠️ **好消息**：`MemStage` 在生产代码里**只有 1 个消费者**（`rv32i_pipeline.h`）
⇒ ⇒ ⇒ 契约迁移面**封闭**（无跨仓下游）。

⚠️ **ADR-069 A0-① 的一处措辞订正（登记给 architect 补 addendum）**：
该行写「`sread` 的**数据不驻留 M2**，而是**组合流入 MEM/WB′**」⇒ ⇒ **按本 change 的拍级推导，
这句在时序上不可实现**（MEM/WB′ 在 E_{T+2} 装载 ⇒ ⇒ 它的数据入端必须在 T+2 稳定
⇒ ⇒ 原始字 MUST 在某处**寄存 1 拍** ⇒ ⇒ 那个寄存器**就是 M2** 的 `m2_word`）
⇒ ⇒ ⇒ **决策本身（A0 / EX→WB = 3 级 / 停顿 4 拍 / 写侧零 churn）不受影响**
⇒ ⇒ ⇒ 本 change 按「**原始字寄存在 M2 的 `m2_word`**（选择仍在 M2 的**下游**做）」实施
⇒ ⇒ ⇒ 并把该措辞订正登记给 architect（**不阻塞实施**：V1–V15 全部仍成立）。

⇒ ⇒ ⚠️ **地址端口零冲突的验证**（实施期必须复核）：
T 拍 TCM 地址 = `exmem_alu_result`（= A_i）⇒ ⇒ `sread` 在 T 拍发出 ⇒ ⇒ T+1 拍 `rdata` = **data(A_i)**
⇒ ⇒ 同时 T+1 拍 TCM 地址 = A_{i+1}（EX/MEM 已前进）⇒ ⇒ 那是**下一次**读，**无害**
⇒ ⇒ ⇒ **单地址口即可**，**不需要**新增端口 ⇒ ⇒ ⇒ 「写侧零 churn」成立
⇒ ⇒ ⇒ 且 **store 侧的地址/车道移位/`strbe` 全部留在 T 拍**（V5：「搬移了 ⇒ 反而错」）。

⚠️ **T5 的前置**（实施期实测发现）：草案里的金标现值
`perf_instructions = 178` / `load_use_stalls = 6` / `control_stalls = 4`
**未在 `tests/` / `examples/` 里作为断言常量出现** ⇒ ⇒ 它们**可能**是 perf report 的**观测值**
⇒ ⇒ ⇒ **T5 开工前 MUST 先定位断言位置**（`kExpectedWritebacks` 24 条写回序列 +
`obs_perf_*` 观测端口）⇒ ⇒ ⚠️ **禁**把「观测值」当「断言值」。

---

## 10. T5 —— 时序金标的**手推**（**2026-11-13 · 先于实施**）

> ⚠️ **本节是「先推后写」的留痕** ⇒ ⇒ 期望值在**实施前**推导完毕 ⇒ ⇒
> ⇒ ⇒ 实施后若实测与本节**不一致** ⇒ ⇒ **那说明实现错了** ⇒ ⇒ **禁**把实测值回填成本节的「推导」
> ⇒ ⇒ （禁抄仿真输出：两后端同源 ⇒ ⇒ parity 对该族缺陷**结构性失明**）。

### 10.1 断言位置（**实施前的定位结果** · 草案里的 178/6/4 的真实出处）

| 期望值 | **真实断言位置** | 形态 |
|---|---|---|
| `load_use_stalls == 6` | `examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp:863` | 硬编码字面量 `6u` |
| `control_stalls == 4` | 同上 `:871` | 硬编码字面量 `4u` |
| `perf_instructions == kCycles - 22` | 同上 `:925`（`kCycles = 200` ⇒ **178**） | **表达式**（不是字面量） |
| `non_retired == 21` | 同上 `:924` | 硬编码字面量 `21u` |
| 24 条写回序列 | 同上 `:390`（`kExpectedWritebacks`）+ `:953-959` | **值 + 顺序** |
| `store_cycles == 5` | 同上 `:836` | **计数** |
| 取指填充期不变量（`ta[0]`/`ta[1]`） | 同上 `:782-784` | **值 + 拍号** |
| `branch_count == 2` / `mispredict == 1` / `hazards == 2` | 同上 `:852` / `:854` / `:991` | **计数** |
| 5 个 `has_store` 金标 + 4 个反向期望 | 同上 `:787-796` / `:840-845` | **值** |

⇒ ⇒ ⚠️ **草案的「178 / 6 / 4」三个数**里，**6 与 4 确是断言字面量**，而 **178 是 `kCycles - 22` 的表达式**
⇒ ⇒ ⇒ **不是**「观测值」⇒ ⇒ 但**它同样是拍数派生量** ⇒ ⇒ A0 下**必变** ⇒ ⇒ MUST 手推。

### 10.2 分段分解（**唯一锚点** = `non_retired` 的 fill / stall / flush 三段）

> **来源** = `openspec/specs/rv32i-commit-path/spec.md:344-347`（「更稳的锚点是 `non_retired` 的**分段分解**」）
> ⇒ ⇒ 现状：**fill 3 + stall 6 + flush 12 = 21** ⇒ ⇒ 退休 = 199 − 21 = **178**。

| 段 | 现状 | A0 后 | 推导（**为什么这一段变 / 不变**） |
|---|:--:|:--:|---|
| **fill**（取指填充期） | **3** | **3**（**不变**） | 退休计数器 `commit_count` 的增量源 = `mem_stage.io().mem_valid_out` = `exmem_valid`（**MEM 级**指令有效）⇒ ⇒ **M2 在 MEM 的下游** ⇒ ⇒ **退休计数的时点不动** ⇒ ⇒ ⇒ fill 段不变（⚠️ **这是本节最容易搞错的一格** —— 若把计数源改成 M2/MEM-WB′ 的 valid ⇒ fill 会变成 4 ⇒ ⇒ 而那**违反** capability `rv32i-commit-path` 的「「本拍有一条指令退休」MUST 等价于「MEM 级持有**有效**指令」」） |
| **stall**（load-use 停顿） | **6** | **8** | ADR-069 §2.1：**EX→WB 级数 2 → 3**（`EX/MEM → M2 → MEM/WB`）⇒ ⇒ 每处停顿 **3 → 4 拍** ⇒ ⇒ 固件里**两处**紧邻 load→use（word 20/21 · word 43/44）⇒ ⇒ **6 → 8**（ADR-069 §2.3 代价③ **明写**这个数） |
| **flush**（控制重定向气泡） | **12** | **12**（**不变**） | 每次重定向 3 拍 × **4** 次（taken beq + JAL@64 + JALR@80 + JAL@100）⇒ ⇒ flush 的**杀指令点**在 IF/ID / ID/EX ⇒ ⇒ **M2 在其下游** ⇒ ⇒ ⇒ flush 段不变 |
| **合计 `non_retired`** | **21** | **23** | 3 + 8 + 12 |
| **退休数 `perf_instructions`** | **178** | **176** | (kCycles − 1) − `non_retired` = 199 − 23 |

### 10.3 **不变**的期望值（**逐条给理由** · 禁「大概不变」）

| 期望值 | 为什么不变 |
|---|---|
| **24 条写回序列**（`kExpectedWritebacks`） | A0 只改**时序**，不改**语义** ⇒ ⇒ 写回的**值**与**顺序**（程序序）不变 ⇒ ⇒ 只有**拍号**平移（⚠️ ADR-069 V9：**非均匀平移** ⇒ ⇒ 断言按**身份**对齐，不按拍号 zip） |
| `store_cycles == 5` | store 的**写**在 **MEM 拍**发生（地址取自 EX/MEM ⇒ **照旧**）⇒ ⇒ A0 不改写的拍 ⇒ ⇒ 计数不变（只有拍号平移） |
| `control_stalls == 4` | 它计的是**重定向次数**（不是拍数）⇒ ⇒ A0 不改重定向的**次数** ⇒ ⇒ 4 不变 |
| `branch_count == 2` / `mispredict == 1` | 计的是**分支条数 / 误预测次数** ⇒ ⇒ 不变 |
| `hazards == 2` | `dbg_load_use_detected` 是**单拍脉冲** ⇒ ⇒ 数的是**冒险次数**（2 处）⇒ ⇒ **不变**（⚠️ **不是** `load_use_stalls`！两者 MUST 分清） |
| 5 个 `has_store` 金标（地址 + 值） | 语义不变（load 的值仍正确、store 的值与地址不变）⇒ ⇒ 不变 |
| 4 个反向期望（`has_store(…, 值+1)` 等） | 同上 ⇒ ⇒ 不变 |
| `ta[0].dbg_id_pc == 0x80000000` / `ta[0].dbg_id_valid == 0` / `ta[1].dbg_id_valid == 1` | 取指侧**零改动**（imem 不动 ⇒ ADR-069 §4「零改动」）⇒ ⇒ 填充期不变量不变（⚠️ `primed` 门控与 `fetch_pc_reg` 的跳拍**照旧**） |
| `x8_writes == 1` | 写回次数不变 ⇒ ⇒ 不变 |
| `obs_data_addr` / `obs_data_write_data` / `obs_data_write_en` 的**值** | A0 的卖点就是「**写侧零 churn**」（地址呈现拍不变）⇒ ⇒ 不变 |

### 10.4 手推值 → 断言改动的**最小集**（**只改这 3 个**）

| 断言 | 现值 | 新值 |
|---|---|---|
| `REQUIRE(ta.back()[kIdxLoadUseStalls] == 6u);`（`:863`） | `6u` | **`8u`** |
| `REQUIRE(non_retired == 21u);`（`:924`） | `21u` | **`23u`** |
| `REQUIRE(ta.back()[kIdxInstructions] == kCycles - 22u);`（`:925`） | `kCycles - 22u` | **`kCycles - 24u`** |

⇒ ⇒ ⚠️ **其余断言 MUST 一字不动** ⇒ ⇒ 若实施后**别的**断言转红 ⇒ ⇒ **那不是「基线变了」**
⇒ ⇒ ⇒ **那是实现错了** ⇒ ⇒ MUST 逐条查因（**禁**顺手改期望值）。

⇒ ⇒ ⚠️ **手推值本身的可判别性**（tasks S5 的要求）：`8` / `23` / `176` 三个数
**非对称、非回文、非字节回文** ⇒ ⇒ 且它们**互相独立**（`8` 是 2×4、`23` 是 3+8+12、`176` 是 199−23）
⇒ ⇒ ⇒ 三者**同时**对 ⇒ ⇒ 单改一个 ⇒ ⇒ 另两个必红 ⇒ ⇒ ⇒ 判别力不依赖任一个数的「正确性」。

### 10.5 与 ADR-069 V8 / V9 / V11 的对应

| ADR 判据 | 本节对应 |
|---|---|
| **V8**（`perf_load_use_stalls` 6 → 8） | §10.2 的 stall 段 + §10.4 的第 1 条 |
| **V9**（24 条写回序列 · 带归因标签 · **非均匀平移**） | §10.3 的第 1 行（**顺序与值不变 ⇒ 归因标签 = `SHIFT`**）⇒ ⇒ ⚠️ `STALE` = **一票否决** ⇒ ⇒ ⇒ 实现若让某条写回的数据变了 ⇒ ⇒ **不是**基线问题 ⇒ ⇒ 是实现 bug |
| **V11**（spec 的 `load_use_stalls` 基线 6→8 MUST MODIFIED） | 载体 = 本 change 的 delta `specs/rv32i-commit-path/spec.md`（**已就位** · Scenario「时序基线修订后的退休数与停顿拍数」）⇒ ⇒ ⇒ archive 时并入主 spec |
