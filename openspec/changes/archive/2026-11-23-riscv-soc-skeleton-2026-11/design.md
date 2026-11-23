# Design: S1 结构骨架 —— `Rv32iSocTop` + `SocTraceCounter`（`riscv-soc-skeleton-2026-11`）

- **上游**: `proposal.md`（AC1–AC13）· **决策**: ADR-077（`Accepted` 2026-11-23 · V1/V2/V4 为权威数字）
- **实施角色**: builder（本文件由 builder 定稿）
- **零回归面**: `examples/riscv-mini/**` 与 `include/**` **一字不动**（AC5 · base SHA 钉死）

---

## 1. 文件布局

```
examples/riscv-soc/
├── src/
│   ├── rv32i_soc_top.h        # SoC 根（Rv32iSocTop · 无 main）
│   └── soc_trace_counter.h    # 观测 IP（SocTraceCounter）
├── tb_riscv_soc.cpp           # 激励驱动（三模式 · 有 main）
├── tb_riscv_soc.sv            # 独立 Verilog TB（手写 · generated=no）
├── CMakeLists.txt
└── AGENTS.md                  # 含核复合体 34 端口契约表（AC6）
```

### 1.1 跨 example include 的处理（**关键**）

`rv32i_soc_top.h` 需要 `#include "rv32i_pipeline_top.h"`（住 `examples/riscv-mini/src/`）。

- **护栏核查结论**（2026-11-23 逐条读源码）：
  - `verify_no_duplicate_rv32i_headers.sh` **C3** 的扫描面 = **根 `tests/**`** 的 TU ⇒ 不覆盖 examples 之间 ✅
  - `verify_example_only_headers.sh` 判的是「`include/**` 下有没有 example-only 头」⇒ 与 examples 互引无关 ✅
  - C2 管的是「根 `tests/CMakeLists.txt` 不得有 example 的 `-I`」⇒ 局部 `target_include_directories` 在 examples 内不受限 ✅
- **做法**：`examples/riscv-soc/CMakeLists.txt` 内 `target_include_directories(tb_riscv_soc PRIVATE ${CMAKE_SOURCE_DIR}/examples/riscv-mini/src)`。
  写 `#include "rv32i_pipeline_top.h"`（**不带 `examples/` 前缀** ⇒ 不触发任何路径形态判据）。
- ⚠️ **ADR-077 D1 已批准该依赖**（核复合体住 riscv-mini · SoC 根住 riscv-soc ⇒ 跨目录依赖是决策的结果，不是巧合）。

---

## 2. `SocTraceCounter`（观测 IP）

```cpp
class SocTraceCounter : public ch::Component {
public:
    __io(
        ch_in<ch_bool>      store_event;      // ← core.io().obs_data_write_en
        ch_in<ch_bool>      retire_event;     // ← core.io().dbg_wb_write_en
        ch_out<ch_uint<16>> store_count;
        ch_out<ch_uint<16>> writeback_count;
    )
};
```

- **计数语义**：`count->next = select(ev, count + 1, count)`（饱和到 `0xFFFF` 不必要 —— 16 位在 60 拍内不会溢出；**但** ZERO-DEBT 要求无静默 UB：用 `select(count == 0xFFFF, count, count + 1)` 做饱和，成本 1 个比较器）。
- **复位**：`ch_reg<ch_uint<16>>(0_d)` ⇒ 框架 `default_reset` 清零。
- ⚠️ **两个事件的可靠性已核实**：
  - `obs_data_write_en` = `exmem_is_store` ⇒ store 指令进入 EX/MEM 的那拍，**单拍** ✅
  - `dbg_wb_write_en` = `wb_stage.io().write_en` = `needs_writeback & rd_not_zero & io().mem_valid`
    ⇒ `mem_valid` 限定**已在内**（`wb_stage.h:110`，注释记载「不加则会重复写回」已修）⇒ **单拍** ✅

---

## 3. `Rv32iSocTop`（SoC 根）

```cpp
__io(
    ch_in<ch_bool>      ext_rst{"ext_rst"};
    ch_in<ch_bool>      prog_we{"prog_we"};
    ch_in<ch_uint<32>>  prog_addr{"prog_addr"};
    ch_in<ch_uint<32>>  prog_data{"prog_data"};
    ch_out<ch_uint<16>> store_count{"store_count"};
    ch_out<ch_uint<16>> writeback_count{"writeback_count"};
)
```

⇒ 根自有 **6** · 产品端口 = 框架 2 + 6 = **8**（ADR-077 V1）。

```cpp
ch::ch_layer<Rv32iPipelineTop>     core{"core", "Rv32iCoreComplex"};
ch::ch_layer<SocTraceCounter>      trace{"trace", "SocTraceCounter"};
```

- **档 ③ 成层**（实例处给模块类型名）：`Rv32iPipelineTop` / `SocTraceCounter` **都没有** `CH_HIERARCHY_LAYER`（已核实）⇒ 两参数形态是**唯一**可用重载。
- **`ch_layer` 支持带参构造**（`module.h:162-185`）⇒ `Rv32iPipelineTop` 的 `itcm_init`/`dtcm_init` 走默认空 ⇒ **与基线逐字节等价**（其构造注释明文）。
- 后代 `ch_in` 全驱动（ADR-073 D2）：
  - `core.io().ext_rst/prog_we/prog_addr/prog_data` ← 根
  - `trace.io().store_event` ← `core.io().obs_data_write_en`
  - `trace.io().retire_event` ← `core.io().dbg_wb_write_en`
- 根 `ch_out` 全来自 `trace`（**根自有语义** ⇒ 不是后代信号直通，符合 ADR-077 D1）。

### 3.1 产物 module 集（手推 · 禁从 `ch_layer` 调用点派生）

| # | module | 来源 |
|---|---|---|
| 1 | `top` | 根 |
| 2 | `Rv32iCoreComplex` | `ch_layer` 档③ 类型名 |
| 3 | `SocTraceCounter` | 同上 |
| 4–10 | `IfStage` `IdStage` `ExStage` `MemStage` `WbStage` `HazardUnit` `ForwardingMux_32_` | 核复合体**内部**的 7 个 `ch_layer`（`rv32i_pipeline.h:140-153`） |

⇒ **10** · 与 ADR-077 V2 逐字一致。

### 3.2 核复合体 module 端口 = 34

- **自有 32**（4 in + 28 out）= `Rv32iPipelineTop` 的 `__io`（**已被 C6 冻结快照**，名+方向+位宽）
- **框架 2** = `default_clock` / `default_reset`（核内 **inline** 的 TCM `sread` 寄存器落在该 module ⇒ 需要时钟/复位）
- ⇒ 34 = 2 + 32

---

## 4. TB 三模式（`tb_riscv_soc.cpp`）

| 模式 | 内容 |
|---|---|
| **默认**（无参） | 解释器路径：装载 7 字 → 释放复位 → 跑 60 拍 → 手推断言 ⇒ `RESULT: checks=L fails=0` |
| `--emit-rtl [--hierarchical] --out <dir>` | 发射（走库层 `strip_mode_flags` / `require_out_dir` / `run_emit_cli`） |
| `--check-products --out <dir>` | 发射 + `check_products` 四件套 |

**fail-closed**：默认模式收到任何未知参数 ⇒ exit 2；发射模式缺 `--out` ⇒ exit 2。

### 4.1 程序与手推台账（**零新造**）

复用核级 7 字程序（`.rddf/checks/examples-rtl-expectations.md` 的「小程序」表 · 编码已 `static_assert` 交叉校验）：

| 字 | 指令 | 手推结果 |
|---|---|---|
| 0 | `addi x1,x0,42` | 写回 x1=42 |
| 1 | `nop` | — |
| 2 | `addi x2,x0,11` | 写回 x2=11 |
| 3 | `nop` | — |
| 4 | `add x3,x1,x2` | 写回 x3=53 |
| 5 | `sw x3,0(x0)` | store addr=0 data=53 |
| 6 | `jal x0,0` | 自环（rd=x0 ⇒ 不写回） |

⇒ **`store_count == 1`** · **`writeback_count == 3`**（两值不同 ⇒ 事件源接反必红）

### 4.2 默认模式的断言集（逐条手推）

| # | 断言 | 依据 |
|---|---|---|
| A1 | 初始 `store_count == 0` ∧ `writeback_count == 0` | 复位清零 |
| A2 | 装载期（`ext_rst=1`）两计数保持 0 | 装载期核被复位 ⇒ 无事件 |
| A3 | 释放后 `store_count == 1` | 1 条 `sw`（ISA 语义） |
| A4 | 释放后 `writeback_count == 3` | 3 次写回（x1/x2/x3） |
| A5 | 两计数在观测窗口内**单调不减** | 计数语义 |
| A6 | `jal` 自环后计数**不再增长**（尾段 20 拍差值 == 0） | 自环不产生写回/store |

⚠️ **A2 的判据价值**：装载期复位失效（`ext_rst` 接反）⇒ 装载被当指令执行 ⇒ A2 必红。这是 R7 风险（proposal）的唯一机械面。

### 4.3 `--check-products` 的值侧

```cpp
const std::vector<std::string> kExpectedModules = {
    "top", "Rv32iCoreComplex", "SocTraceCounter",
    "IfStage", "IdStage", "ExStage", "MemStage", "WbStage",
    "HazardUnit", "ForwardingMux_32_",
};
constexpr std::size_t kExpectedTopPorts = 8;
constexpr std::size_t kMinVaryingObservations = 2;
```

**激励面**（`make_inputs` · 顺序与 `make_stimulus` 逐字一致）：
`ext_rst` · `prog_we` · `prog_addr` · `prog_data`（**4 个根 `ch_in` 全覆盖**）。

**观测面**（`make_observations` · 只能取根 `ch_out`）：`store_count` · `writeback_count`。

⚠️ **激励 MUST ≥10 拍且值不同**（签批 spec `rtl-soc-output` 的硬要求）⇒ 激励序列 = 装载 7 字（7 拍）+ 释放 + 若干拍（≥5）⇒ 合计 ≥12 拍，且 `prog_addr` 每拍变化 ✅。

---

## 5. `.sv` 独立 TB（`tb_riscv_soc.sv`）

- **只读 8 根根端口**；`EXPECT-SRC: .rddf/checks/soc-rtl-expectations.md#riscv-soc` · `generated=no`（ADR-074 D7 逃生口）
- 判据（与 C++ 侧**同一组手推字面量**）：A1–A6 的 Verilog 版（`RESULT: checks=L fails=0`）
- ⚠️ `.verif` 封闭原语集表达不了「装载 7 字 + 释放 + 尾段差值」三步（与核级 `.sv` 同因）⇒ 手写是**终态**，不是欠账。

---

## 6. 登记面（8 处 + 1 新登记面）

| # | 位置 | 前 → 后 |
|---|---|---|
| ① | `examples/CMakeLists.txt` 的 `CH_TRACKED_EXAMPLE_TARGETS` | 28 → **29** |
| ② | `.rddf/checks/examples-rtl-sim-exclusions.tsv` 守恒 | `28 == 16 ∪ 12` → **`29 == 17 ∪ 12`** |
| ③ | `.rddf/checks/examples-rtl-sim-targets.tsv` | 新增 1 行 `kind=soc` |
| ④ | 锚点 `REGISTERED_SOC` | 1 → **2**（`REGISTERED_HIER` **不动**） |
| ⑤ | `.rddf/checks/soc-rtl-expectations.md` | 新增 `## riscv_soc {#riscv-soc}`（手推 · `VERIF-CHECKS`） |
| ⑥ | 根 `AGENTS.md`「examples 的 main() 程序」 | 32 → **33**（`CTEST_TOTAL` **不变**） |
| ⑦ | `improvement-suggestions.md` 索引行 | 补 1 行（AC10 · 状态词 `**draft**`） |
| ⑧ | `docs/CHANGELOG.md` + `examples/riscv-soc/AGENTS.md` + `docs/developer_guide/rv32i-rtl-delivery-and-sim.md` | 新增/更新 |
| ⑨ | 核复合体 34 端口表（AC6） | **落 `examples/riscv-soc/AGENTS.md`**（单一真相源 · 不另建登记文件 ⇒ 少一处漂移面） |

### 6.1 AC6 判据载体的落点决策（**实施后定稿**）

**落点 = 新脚本 `scripts/verify_riscv_soc_core_ports.sh`**（已挂 CI · 含 `--self-test` **7** 条投毒 [V5 两条兄弟边由 P7 守]）：

| 判据 | 内容 |
|---|---|
| V1 | 产物里 `Rv32iCoreComplex` 的 module 端口表 == `examples/riscv-soc/AGENTS.md` 的 34 行表（**逐字逐序**） |
| V2 | 端口数 == 34（框架 2 + 核自有 32） |
| V3 | 核自有 32 条（去 `top_core_` 前缀）== `verify_no_duplicate_rv32i_headers.sh` 的 **C6 快照** ⇒ 三方闭环 |
| V4 | `examples/riscv-soc/src/**` 下**无**微架构头的同名副本（ADR-077 D1 / **R4 缓解的落点**） |
| V5 | **两条真兄弟边**（ADR-070 D4③ · **AC8**）：产物里 `SocTraceCounter` 的两个事件输入 MUST 由 `Rv32iCoreComplex` 的输出驱动（**结构化端口连接** `.top_trace_<port>(top_core_<src>)`，不是散文） |

**为什么是独立脚本而不是扩 `verify_examples_rtl_sim.sh`**：
- 那个脚本 470+ 行且有 21 条投毒 self-test ⇒ 改它会动既有期望值（风险 > 收益）；
- 独立脚本可**自跑发射**（不依赖 tsv 注册的产物落点）⇒ 也能在 `pr-verilator-fast` 单独成 step。

**为什么表不在 `.rddf/checks/` 另建一份**：表是**人读契约**（开发者要查）⇒ 放 AGENTS.md；
护栏**解析该表**（不做第二份拷贝）⇒ 单一真相源，零漂移面。

⚠️ **已实测的判别力**：**7** 条投毒各自必红（P1 删行 / P2 改方向 / P3 改位宽 / P4 改端口名 /
P5 与 C6 不符 / P6 微架构副本 / P7 兄弟边改接常量）。
⚠️ **self-test 抓到的真实缺陷**：V4 初版写 `find ... | grep -q .`，在 `set -o pipefail` 下
`grep -q` 早退 ⇒ `find` 收 SIGPIPE ⇒ 管道非零 ⇒ **判据恒绿**（P6 转红失败暴露）⇒ 已改为
`find -print -quit` + `[ -n "$found" ]`。

---

## 7. 实施顺序（与 `tasks.md` 对应）

1. **T1** 骨架：目录 + CMake + `SocTraceCounter` + 空根（只接线，无 TB）⇒ 先让 `cmake --build` 通过
2. **T2** 根装配完成 + 默认模式 TB（A1–A6）⇒ `RESULT` 出现
3. **T3** `--emit-rtl` + `--check-products`（四件套）⇒ `PRODUCTS-CHECK` 通过
4. **T4** `.sv` 独立 TB + 两侧同值
5. **T5** 登记 8 处 + 新登记面
6. **T6** 护栏（AC11 新面 + AC12 回归面）

⚠️ **每步跑验证**（编译 + 该步的判据），不积累未验证变更。

> 📌 **2026-11-23 · dated 注记（R3a 改名批）**：本 change 的 AC2 / AC6 / design 的
> `Rv32iCoreComplex` 期望集**已被** `openspec/changes/riscv-core-rename-2026-11/`（**R3a**）
> 更新为 **`Rv32iCoreTop`**（模块类型名 = **非契约** · ADR-077 D2）⇒ ⇒
> **重新验收本 change 时**：module 集合判据按「**除名字外**相等」理解
> ⇒ ⇒ 端口表 == **34** / 集合基数 == **10** 两项**意图不变**。
> ⚠️ 本 change 的**历史正文不改写**（存量不追溯）⇒ ⇒ 上述注记是唯一的对接点。
