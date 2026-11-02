# 提案：`sread` 的**禁读拍**语义在两条后端不一致（真 parity 分歧）→ **已修复**

- **登记来源**：`dual-backend-parity-route-b-pipeline-2026-11` 实施期实测（2026-11-02）
  写 `chlib::memory` 的 dual 覆盖时被**第一次**撞出
- **规格影响**：**有** —— capability `mem-read-timing` **ADDED ×1**
  ⇒ 归档 **MUST NOT** 用 `--skip-specs`
- **严重度**：🔴 **真分歧** —— 同一个设计在两条后端上给出**不同输出**

## 分歧本体（两侧代码逐字对照）

`ch_mem::sread(addr, enable)` 在 **`enable == 0`（禁读拍）** 时：

| 后端 | 修复前行为 | 代码 |
|---|---|---|
| **interpreter** | 保持上一拍的值 | `src/ast/instr_mem.cpp` 的 `if (!enabled) return;` |
| **Verilog / Verilator** | 清零 | `src/codegen_verilog.cpp` 的 `else proxy <= 1'b0;` |

⚠️ **关键细节：那个 `else` 不是 reset 分支**。生成的形状是

```systemverilog
always_ff @(posedge default_clock or posedge default_reset) begin
    if (!default_reset)
        if (en)
            proxy <= mem[addr];
    else
        proxy <= 1'b0;
end
```

按 Verilog 的 **dangling-else** 规则，`else` 与**内层** `if (en)` 配对
⇒ ⇒ 这是「`enable == 0` 清零」；`default_reset` 为高时整个外层体被跳过
（**保持**，不是清零）。

⚠️ 原源码注释写的是「Reset branch clears the read proxy」⇒ ⇒ **措辞与实现相反**
⇒ 已一并订正（只改注释，输出逐字节不变）。

## 精确影响面（已实测确认）

1. **`aread` 不受影响**：`ch_mem::aread` 传给 `create_mem_read_port` 的 enable 是
   `nullptr` ⇒ 恒为「读」，不存在禁读拍。⚠️ interpreter 侧的 async 分支里也有同形
   的 `if (!enabled) return;`，但 `enable_data_ptr_` 恒空 ⇒ **不可达**（已加防复发
   注释，避免将来支持 aread enable 时重造同型分歧）。
2. **受影响的生产组件只有 `chlib::memory`**：`single_port_ram`（`sread(addr, !we)`）
   与 `dual_port_ram` 的两个端口 —— 即**写的那一拍**。
   `chlib/fifo.h` 用恒 `ch_bool(true)` ⇒ 不受影响；
   `tests/test_mem_read_timing_dual.cpp` 同样只用恒使能 ⇒ 从未覆盖这条路径。
3. **既有断言覆盖 = 0**：`tests/test_mem.cpp` 里 sread 相关的 4 个 SECTION 全是
   结构断言（`impl() != nullptr` / `has_enable()`），**不跑仿真**；
   `tests/test_mem_connection.cpp:43` 虽有可变 `enable_in` 端口，但用例**恒驱动 1**
   ⇒ ⇒ 禁读拍从未被任何测试触及。

## 裁决（2026-11-02）= 方案 A：对齐 Verilog（清零）

理由三条：

1. **只改 Simulator 一侧** ⇒ 既有用户生成的 Verilog **零变化**
   （改 codegen 会让外部 RTL 的禁读拍从「清零」变成「保持陈旧值」）；
2. 「不读就没有数据」比「读到上一拍的陈旧值」更不易被下游误用 —— 陈旧值可能被
   当成有效数据参与运算；
3. 生成代码的 `else` 语义本就是「`enable == 0` 清零」，**Simulator 是偏离的一侧**。

⚠️ 否决的方案 B（对齐 interpreter「保持」）：需要给生成的 `always_ff` **另加**
真正的 reset 分支（否则复位期间 proxy 保持 X/未知），改动面更大且**改变对外 RTL
行为**。方案 C（允许两后端不同）与 `mem-read-timing` §Purpose 的「两者完全一致」
直接矛盾 ⇒ 等于把 drift 制度化。

## What Changes（已实施）

| 文件 | 内容 |
|---|---|
| `src/ast/instr_mem.cpp` | `instr_mem_sync_read_port::eval` 的禁读拍：`return;` ⇒ **清零后 return**；async 分支加防复发注释（行为不变） |
| `src/codegen_verilog.cpp` | **只改注释**（订正 dangling-else / reset 的反向表述），生成代码逐字节不变 |
| `tests/test_mem_sread_disable_dual.cpp`（**新**） | 2 个 `DUAL_BACKEND_TEST_PAIR`：enable 翻转（6 拍）· 禁读拍换地址不采样（6 拍）⇒ **24 == 2 × 12** |
| `tests/chlib/test_memory_dual.cpp`（**新**） | `chlib::memory` 的 RAM 族 dual 覆盖（写拍 = **0**）⇒ **26 == 2 × 13** |
| `tests/CMakeLists.txt` | 2 条 `add_dual_backend_test` |
| `openspec/specs/mem-read-timing/spec.md` | **ADDED ×1**（3 个 Scenario：清零且一致 / 裁决可追溯 / RAM 族被覆盖） |

## 判据与判别力

- **N2′**：`24 == 2 × 12` · `26 == 2 × 13`（有/无 verilator 实测）
- **期望值全部手推**（文件头逐拍），修复后**两后端一次通过**（这本身是「推导正确」
  的证据 —— 修复前同样期望值会红）
- **变异 2/2 全抓**（每条先确认仍可编译）：
  - M1 把 interpreter 改回 `return;`（复现分歧）⇒ 2 个用例 failed
  - M2 把 codegen 改成「禁读拍保持」（删 dangling else）⇒ 2 个用例 failed
- **回归探针**：`test_memory_dual` 的写拍期望值是 **0** ⇒ 任一侧被改回「保持」即红

## Verification（前瞻 · 可勾选）

- [x] 裁决落地（方案 A）且理由写进 capability 的 Scenario
- [x] 禁读拍的**逐拍**双后端判据存在且期望值手推
- [x] N2′ 成立（24 == 2×12 · 26 == 2×13）
- [x] 变异证明判别力（2/2），且 M1 就是「复现原分歧」
- [x] `chlib::memory` 的 dual 覆盖补回（3 个 RAM 变体）
- [x] `ctest -LE perf` **213/213** 零回归（interpreter 改动影响面广，必须全量）
- [x] `openspec validate mem-read-timing --strict` valid
