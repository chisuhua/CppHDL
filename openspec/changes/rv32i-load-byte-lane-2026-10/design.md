# Design: RV32I load 字节道固件验证（LB / LH / LBU / LHU）

- **change**：`rv32i-load-byte-lane-2026-10`（**有** spec 变更 ⇒ 归档 **禁** `--skip-specs`）
- **架构依据**：无（**不改实现** ⇒ 只验证 `mem_stage.h:82-163` 的既有 load 路径）
- **5 段草案**：`.rddf/improvements/rv32i-load-byte-lane-2026-10.md`
- **本文件定位**：**实施设计**（固件布局 + 金标手工推导 + 变异计划）

## 1. 机制读码结论（**已实现，零实现改动**）

`include/cpu/pipeline/stages/mem_stage.h:82-163`：

| `funct3` | 指令 | 车道选择 | 扩展 |
|---|---|---|---|
| `000` | LB | `addr[1:0]` → byte0..3 | **符号**扩展（`0xFFFFFF00 \| zext`） |
| `001` | LH | `addr[1]` → half0/half1 | **符号**扩展（`0xFFFF0000 \| zext`） |
| `010` | LW | 整字 | 无 |
| `100` | LBU | `addr[1:0]` → byte0..3 | **零**扩展 |
| `101` | LHU | `addr[1]` → half0/half1 | **零**扩展 |

⚠️ **半字道按 `addr[1]` 选 ⇒ 半字对齐**（`addr[0]` 被忽略）⇒ 用例 MUST 用
**偶地址**（`0x100` / `0x102`）⇒ misaligned half 不在范围（Non-Goals）。

## 2. 固件设计（判别性固件，**独立**于既有 108 字固件）

⚠️ **独立** ⇒ 既有固件的 golden（24 项写回序列 / 停顿计数 / 退休数 178 / `non_retired` 21）
**零变化** ⇒ 「既有 golden 的变化 MUST 全部可归因」⇒ 本次归因 = **零**。

数据准备（`0x100` 处的字）：

```
w0   ADDI x1, x0, 0xD4        ← 准备 0xD4（字 0xA1B2C3D4 的 lane 0）
w1   ADDI x2, x0, 0xC3        ← lane 1
w2   ADDI x3, x0, 0xB2        ← lane 2
w3   ADDI x4, x0, 0xA1        ← lane 3
w4   SW x1, 0x100             ← [0x100] = 0xD4（低字节）
w5   SW x2, 0x101             ← [0x101] = 0xC3
w6   SW x3, 0x102             ← [0x102] = 0xB2
w7   SW x4, 0x103             ← [0x103] = 0xA1
     ⇒ [0x100] 的字 = 0xA1B2C3D4（每字节都不同 ⇒ 任何 lane 选错都暴露）
```

⚠️ **`SW` 的 lane 语义**：`DataTCM` 的写是**读-改-写** ⇒ 4 次 `SW` 依次写 4 个字节
⇒ 最终 `[0x100]` = `0xA1B2C3D4`（每次 `SW` 只改自己的车道）。
⇒ **前置**：该行为已被 ① 的 `rv32i-store-byte-enable-2026-10` 固化
（spec `rv32i-isa-semantics` 的「字节道写 MUST 只写选中的车道」）。

读回（每条 load 的结果 MUST 写进一个**独立**寄存器 ⇒ 写回序列即断言载体）：

| word | 指令 | 期望（手推） | 反向期望 |
|---|---|---|---|
| 12 | `LB x5, 0x100(x0)` | lane0 = `0xD4` ⇒ 符号扩展 `0xFFFFFFD4` | 零扩展 ⇒ `0x000000D4` |
| 13 | `LB x6, 0x101(x0)` | lane1 = `0xC3` ⇒ `0xFFFFFFC3` | lane 恒取 0 ⇒ `0xFFFFFFD4` |
| 14 | `LBU x7, 0x102(x0)` | lane2 = `0xB2` ⇒ `0x000000B2` | 符号扩展 ⇒ `0xFFFFFFB2` |
| 15 | `LBU x8, 0x103(x0)` | lane3 = `0xA1` ⇒ `0x000000A1` | lane 恒取 0 ⇒ `0x000000D4` |
| 16 | `LH x9, 0x100(x0)` | half0 = `0xC3D4` ⇒ `0xFFFFC3D4` | 符号扩展错 ⇒ `0x0000C3D4` |
| 17 | `LH x10, 0x102(x0)` | half1 = `0xA1B2` ⇒ `0xFFFFA1B2` | lane 恒取 0 ⇒ `0xFFFFC3D4` |
| 18 | `LHU x11, 0x100(x0)` | half0 ⇒ `0x0000C3D4` | 符号扩展 ⇒ `0xFFFFC3D4` |
| 19 | `LHU x12, 0x102(x0)` | half1 ⇒ `0x0000A1B2` | lane 恒取 0 ⇒ `0x0000C3D4` |

⇒ **8 条读回 + 4 条写回（x1..x4）= 12 项写回序列**（顺序 = 程序顺序）。

⚠️ **依赖距离**：load 与其消费者 MUST 隔开足够距离（本固件用 NOP 隔 3 条 ⇒ 距离 4
⇒ 走**寄存器堆路径** ⇒ 不依赖前推）⇒ 避免把「前推」混进本用例的判别里。

## 3. 断言设计（**全部手工推导**，禁抄仿真输出）

1. **写回序列**（最强）：12 项逐项比对（不去重 —— 任何连续重复都是真回归）；
2. **每条 load 的值**：由写回序列承载（`x5..x12`）；
3. **反向期望**：上表逐条给出（`X_old != X_new`）；
4. **双后端逐拍全等**（27 个顶层输出，**无掩码**）。

## 4. 变异验证（每条先确认**变异后仍编译成功**）

| # | 变异 | 预期转红 | 抓的是 |
|---|---|---|---|
| M1 | `mem_stage.h` 的 byte 选择恒取 lane0（`selected_byte = byte0`） | 写回序列 4 项错 | 字节道选择 |
| M2 | LB/LH 的符号扩展写成零扩展 | `x5`/`x6`/`x9`/`x10` 错 | 扩展方式 |
| M3 | half 选择取反（`half_offset` 反了） | `x9`/`x10`/`x11`/`x12` 错 | 半字道选择 |
| M4 | `funct3` 分派漏 `LBU`/`LHU`（恒 0） | `x7`/`x8`/`x11`/`x12` 错 | 分派完整性 |

⚠️ M1–M4 **改的是实现**（`mem_stage.h`）⇒ 变异后 MUST **重建**并复跑；
还原后 MUST 再重建 ⇒ 不得把变异二进制留在 build 目录。

## 5. 规格与簿记

- **spec delta**：`rv32i-isa-semantics` **ADDED ×1**（已写好）⇒ 归档**禁** `--skip-specs`
- **计数三者均不变**（用例进既有 target `test_rv32i_soc_dual_parity`）
- **簿记**：`feat-rv32i-isa-completeness` 的「SB/SH/LB/LH/LBU/LHU」验收项勾选 +
  `roadmap.md` + 根 `AGENTS.md §NOTES`
- **文档**：根 `AGENTS.md §KNOWN LIMITATIONS` 的「ISA 覆盖缺口」条目收窄
  （LB/LH/LBU/LHU 已覆盖 ⇒ 只剩 CSR/ECALL/FENCE）
