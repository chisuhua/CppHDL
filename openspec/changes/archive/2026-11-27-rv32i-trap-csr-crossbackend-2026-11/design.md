# design · trap / CSR 的核级跨后端 golden（步 3 · P0-2）

> **Change**：`rv32i-trap-csr-crossbackend-2026-11` · **母 improvement**：同名
> **纪律**：期望值 MUST 手推 · 逐拍 diff **无掩码** · 变异证明判据非空转

---

## 1. 载体（**复用**步 2 的 fixture，不新建文件）

```
test_rv32i_soc_dual_parity.cpp
├─ run_both(fw, cycles, tag)          ← 新增：2×device + 2×Simulator（verilator 缺席 ⇒ ok=false）
├─ require_per_cycle_equal(a, b)      ← 新增：39 根逐拍 diff（带 INFO 诊断）
├─ collect_writebacks(trace)          ← 新增：按时间序提取写回
├─ TEST_CASE Rv32iSoc - TrapEntryCrossBackend   ← S2
└─ TEST_CASE Rv32iSoc - CsrInstrsCrossBackend   ← S3
```

**为什么复用而不是新建测试文件**：① spec 要求「同一 TEST_CASE 内 2×device + 2×Simulator」；
② 步 2 已经把 `load_firmware`（span 版）/ `AxilSlave` / `obs_snapshot` 备齐。
⇒ ⇒ 新建文件等于把 200 行 fixture 再抄一遍（两份漂移风险）。

---

## 2. trap 的手推（**禁抄仿真输出**）

| 位置 | 内容 | 手推 |
|---|---|---|
| word 0x000 | `0x0000_0000` | 未知 opcode ⇒ `illegal_reason = 2` |
| word 0x040 | `csrrs x1, mepc, x0` = `0x3410_20F3` | x1 = **0x8000_0000**（触发指令**自身**的 PC） |
| word 0x041 | `csrrs x2, mcause, x0` = `0x3420_2173` | x2 = **2** |
| word 0x042 | `jal x0, 0` | 自环（rd = 0 ⇒ 无写回） |

- trap 落点 = `mtvec` 复位值 `0x8000_0100` ⇒ 核顶层字索引 `(0x8000_0100 >> 2) & 0xFFF` = **0x040**
  ⇒ ⇒ 镜像 MUST 覆盖 0x40..0x42。
- ⇒ 写回序列**恰好两条**（`LUI`/`ADDI` 不参与）；触发指令**永不退休**（`retired == 0` 于触发拍）。

**判别位**（不是「随便一条错就红」）：
- 若 `mepc` 写成 `pc+4`（常见缺陷）⇒ 写回 0x8000_0004 ⇒ 红；
- 若 `mcause` 编码错（例如写成 3）⇒ 红；
- 若 trap 后**只杀一拍**（`trap_flush_d1` 漏）⇒ `trigger+2` 的 `id_valid == 1` ⇒ 红。

---

## 3. CSR 的手推

| 指令 | 手推 |
|---|---|
| `lui x1, 0x12345` / `addi x1, x1, 0x678` | x1 = 0x1234_5678 |
| `csrrw x2, mscratch, x1` | x2 = 0x0 · mscratch = 0x1234_5678 |
| `csrrs x3, mscratch, x0` | x3 = 0x1234_5678（`rs1 = x0` ⇒ 不写） |
| `csrrc x4, mscratch, x1` | x4 = 0x1234_5678 · mscratch = **0**（`old & ~x`） |
| `csrrwi x5, mscratch, 0x1f` | x5 = 0x0 · mscratch = 0x1f |
| `csrrsi x6, mscratch, 0x10` | x6 = 0x1f · mscratch = 0x1f |
| `csrrci x7, mscratch, 0x0f` | x7 = 0x1f · mscratch = **0x10** |
| `csrrs x8, mscratch, x0` | x8 = 0x10 |

⇒ 写回 **9 条**。**判别位**：`csrrc` 用 `^ x` 而非 `& ~x` ⇒ mscratch 变成 0x1234_5678（`old ^ old` = 0？
⚠️ 注意：这里 `old == x` ⇒ `& ~x` 与 `^ x` **都给 0** ⇒ ⇒ **本程序分辨不出二者**；
⇒ ⇒ 判别力由 `test_rv32i_csr_instructions` 的**第二个**用例（`old=0x0F` / `x=0xF0`）提供，
本项**如实登记**：本用例的 `csrrc` 不是 `^ vs & ~` 的判别位（避免「以为覆盖了」）。
⇒ ⇒ 本用例真正钉的是：6 种 `funct3` 的 `csr_wdata` 选择 + 读回顺序 + **跨后端一致性**。

---

## 4. 变异（判别力自证）

| 变异 | 做法 | 期望 |
|---|---|---|
| **M1** | `wbs[0].second` 0x8000_0000 → 0x8000_0004（模拟 `mepc = pc+4` 缺陷的**期望值**） | 红 ⇒ 证明该断言真的在比 |
| **M2** | CSR 期望表里 `{4, 0x1234_5678}` → `{4, 0x0}` | 红 ⇒ 证明 `csrrc` 读回被判 |

（变异只改**期望值**不改被测实现 —— 这里的目的是证明「断言不是恒真」，不是证明 RTL 正确。）

---

## 5. 边界与错误路径

| 路径 | 处置 |
|---|---|
| verilator 缺席 | `run_both` 返回 `ok=false` ⇒ 调用方 `return`（**不用 SKIP**：全 SKIP ⇒ exit 4 ⇒ CTest 红） |
| `initialize()` false / `port_access_` 空 | 同上 |
| 镜像未覆盖 trap 落点 | 0x040 读到 0（又一条未知 opcode）⇒ 写回会多于 2 条 ⇒ 红（自解释） |

---

## 6. 边界（**明确不做**）

| # | 不做 | 原因 |
|---|---|---|
| B1 | P1（反压期操作数完整性） | 独立项（盘点 §5） |
| B2 | `ECALL`/`EBREAK`/`SRET` 的 cause 值 | 已由 `test_rv32i_trap_entry` 的解释器判据覆盖；跨后端化归后续（本项只做**未知 opcode** 这一条触发源） |
| B3 | 中断 | 核顶层无 `irq` 端口（S5） |
