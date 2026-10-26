# Design: RV32I 字节道写语义

## 1. 数据通路（改动后）

```
                     mem_stage (funct3)
                          │
                          ├─ store_byte_enable_out ──► data_strbe (4 bit)
                          │        (sb: 0001/0010/0100/1000 by byte_offset
                          │         sh: 0011/1100 by half_offset
                          │         sw: 1111)
                          │        ⚠️ 掩码用 `addr[1]`（half_offset），
                          │        而 lane 对齐用 `addr[1:0]` ⇒ 二者**只在
                          │        对齐 sh 时一致**（对齐 ⇒ 偏移 0 或 2）；
                          │        非对齐 sh ⇒ 两者不一致（见 §5）
                          ▼
   exmem_rs2_data ──► shl<32>(rs2, lane_shift) ──► data_write_data
                          ▲ lane_shift = 8 * addr[1:0]（仅 SB/SH）
   exmem_alu_result ──┘（addr[1:0]）
                          │
   data_addr ──► SoC: bits<13,2>(addr) ──► dtcm.addr（字地址）
                                        └─► dtcm.strbe = data_strbe
   dtcm 内部：old = mem.aread(addr)
              merged = (old & ~mask) | (wdata & mask)   mask = byte_lane_mask(strbe)
              mem.write(addr, merged, write & valid)
```

## 2. 关键决策

### D1：lane 对齐放**流水线侧**（planner 2026-10-26 裁决）

- **理由**：RISC-V store 的源数据本就**低位对齐**，由**总线层**按 `addr[1:0]`
  选车道；放在 pipeline 侧与既有 AXI 路径（`include/axi4/byte_lane.h`）**同构**；
  TCM 侧只做「读-改-写 + 掩码合并」，语义单一、可独立测。
- **反例（被否决）**：在 TCM 侧做 lane 选择 ⇒ TCM 需要知道「源字节在低位」
  这一约定 ⇒ 与 AXI 侧语义分叉 ⇒ 两处都要改。

### D2：`sw` 零行为回退

`lane_shift` 只在 **SB/SH**（funct3 = 000/001）时非零 ⇒ `sw` 的 `data_write_data`
仍等于 `exmem_rs2_data` ⇒ **非对齐 `sw` 的既有行为不变**（整字写到所在字地址）。

### D3：合并在 `DataTCM` 内做（不改 `ch_mem`）

`ch_mem::write(addr, data, en)` 只写**整字** ⇒ 字节写 MUST 是读-改-写。
⚠️ 组合读（`aread`）返回**边沿前**的值 ⇒ 合并结果 = 「旧字 + 新字节」⇒
与 RISC-V 的字节写语义一致。

### D4：`DataTCM` 的 `strbe` 是**新增输入端口** ⇒ 全部实例化点 MUST 驱动

⚠️ **实测发现**：未驱动的输入端口在本框架里读到的是**全 1**（不是 0）⇒
恰好等价于「全车道写」⇒ 整字写用例会**碰巧**通过 ⇒ 那是**未定义行为依赖**
（两个后端的未驱动初值未必相同）⇒ 4 个实例化点全部显式接线。
⇒ 这条被写进新 capability `rv32i-isa-semantics` 的 Requirement。

### D5：SoC 的 32 端口契约**不变**

本 change **不新增**顶层端口 ⇒ **无需**改 ADR-046 **D10**（32 端口冻结）。
观测证据用既有的 `obs_data_addr` / `obs_data_write_data` / 寄存器写回即可。

## 3. 时序

| 阶段 | 拍 | 事件 |
|---|---|---|
| T | 指令进 EX/MEM | `exmem_funct3` / `exmem_alu_result` / `exmem_rs2_data` 锁存 |
| T+1 | MEM 级 | `store_byte_enable_out` 算出（funct3 ⇒ 车道掩码）；`data_write_en` = `exmem_is_store` |
| T+1 | MEM 级 | `data_write_data` = `shl<32>(rs2, 8*addr[1:0])` |
| T+1→T+2 | 写沿 | `DataTCM` 组合读出该字 ⇒ 合并 ⇒ 整字写回 |
| T+2 | 后续 `lw` 的 MEM 级 | 组合读出**已含**字节写的字 ⇒ 读回正确 |

⚠️ `sb` 之后**紧邻**的 `lw`（同拍/下一拍）会读到**旧值**（无写转发）⇒
这是 RISC-V 允许的（无 fence 时不保证顺序）⇒ 测试固件里 `sb` 与 `lw` 之间
留了 3 个 NOP（与既有 `sw`→`lw` 的间距一致）。

## 4. 验证

| 判据 | 证据 |
|---|---|
| 字节写生效 | `lw x12 <- [0x100]` ⇒ `0x030A`（`0x0A` 的 lane 1 被 `3` 替换） |
| 半字写生效 | `lw x13 <- [0x120]` ⇒ `0xD0000`（lane 2-3 = `13`） |
| **lane 对齐** | store 拍上的写数据 = `0x300` / `0xD0000`（**不是** `3` / `13`） |
| 反向 | 未对齐的写数据 MUST NOT 出现在 trace |
| `sw` 零回退 | 既有 3 个 store 的 golden（地址/值/计数）全部不变 |
| 双后端 | interpreter + verilator **逐拍全等**（27 个观测值，无掩码） |
| 回归 | `ctest -LE perf` 零回归 |
| 变异 | 去掉 lane 对齐 ⇒ store 拍数据变 `3`/`13` ⇒ 断言转红；去掉 `strbe` 接线 ⇒ 整字覆盖 ⇒ 读回 `0x0300` ⇒ 转红 |

## 5. 已知边界

- **非对齐 `sw`**：**保持既有行为**（整字写到所在字地址），**不**引入地址不对齐异常。
- ⚠️ **非对齐 `sh` 的行为**已改变（2026-10-26 critic SHOULD_FIX #1 订正）：
  改动前 `strbe` 无消费者 ⇒ 任何写都是**整字覆盖**；
  改动后是「**按 `addr[1:0]` 移位** + **按 `addr[1]` 选半字掩码**」的
  **部分写** —— 例如 `sh` 到偏移 1 ⇒ byte0 被清 0、byte1 = `rs2[7:0]`
  ⇒ **不是**整字覆盖。
  ⚠️ RISC-V 允许实现自定义非对齐行为（真实现通常触发地址不对齐异常）⇒
  本实现**不做**异常，行为如上；**测试固件 MUST 只用对齐访问**。
- **`sb` 与后续 `lw` 同拍**：无写转发 ⇒ 读到旧值（RISC-V 允许）。
- **`DataTCM` 只支持 32 位数据宽度**的 lane 语义（`byte_lane.h` 支持 8/16/32/64，
  但本 SoC 用 32）。
