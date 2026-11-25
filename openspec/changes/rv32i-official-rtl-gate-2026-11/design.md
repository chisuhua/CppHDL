# design · 官方套件的 RTL 产物级门禁

> **Change**：`rv32i-official-rtl-gate-2026-11`
> 本文件只讲**推导与证据**（根因 / 覆盖分析 / 形态取舍 / 墙钟账 / 被排除的方案）
> ⇒ ⇒ **决策与代价**在 `proposal.md` ⇒ ⇒ **可枚举的改动清单**在 `tasks.md`。

---

## 1. 缺陷本体：`mem` 初值清零块的**宽度**

### 1.1 发射点（`src/codegen_verilog.cpp`）

`ModuleEmitter::print_mem_init()`（`:1740`）发射 `ch_mem` 的 Verilog 初值块：

```cpp
out << "    initial begin\n";
if (!init_data.empty()) {                                        // ← 只有带初值才有 for 循环
    out << "        for (int __i = 0; __i < " << depth << "; __i = __i + 1)\n";
    out << "            " << mem_name << "[__i] = 1'b0;\n";    // ← 🔴 缺陷点（:1752`）
}
for (i in init_data) { out << "        " << mem_name << "[" << i << "] = " << literal << ";\n"; }
out << "    end\n";
```

⇒ ⇒ `mem_name` 是 `logic [31:0] <name> [0:N]` ⇒ ⇒ 元素宽度 = **32**
⇒ ⇒ `= 1'b0` 是 **1-bit 常量赋给 32-bit 目标** ⇒ ⇒ Verilator `WIDTHEXPAND`。

### 1.2 同族修法**已存在**（⇒ ⇒ 这是漏网，不是新问题）

`src/codegen_verilog.cpp:1883-1894`（mem 读口的 proxy 赋值）：

```cpp
// ⚠️ **产物卫生**（C6）：`1'b0` 赋给更宽的 proxy ⇒ Verilator 报
//   `WIDTHEXPAND`（实测 RV32I 命中 1 条）⇒ ⇒ 卫生模式下写成**目标宽度**
//   的零字面量（`<W>'h0`），语义不变（都是零值）。
if (view_.emit_hygiene) {
    out << "            " << proxy_name << " <= " << data_proxy->size() << "'h0;\n";
} else {
    out << "            " << proxy_name << " <= 1'b0;\n";
}
```

⇒ ⇒ **同一族问题（C6 产物卫生）在那处已按 `<W>'h0` 修过** ⇒ ⇒ 本处照搬同一手法即可
⇒ ⇒ **宽度来源**：`memimpl::data_width()`（`include/ast/memimpl.h:56`）= 元素宽度
⇒ ⇒ **不是** `node->size()`（那是 `data_width * depth` ⇒ 会写出 131072 位宽 ⇒ 语法错误）。

### 1.3 为什么 `emit_hygiene` 分支对 mem 初值块**不适用**

`print_mem_init` 在 `view_.emit_hygiene == false` 时也发射 `1'b0` ⇒ ⇒ 默认产物同样红。
⇒ ⇒ 但**默认产物**顶部带 blanket `lint_off WIDTHEXPAND/WIDTHTRUNC`
（`verify_examples_rtl_sim.sh:463` 记载）⇒ ⇒ 默认产物 lint 绿、**卫生产物**红。
⇒ ⇒ 与 proxy 那处**不同**：proxy 那处的 `1'b0` 在默认产物里也**没有** blanket 覆盖
（因为它不在文件顶部的 blanket 范围？）—— 实测：proxy 那处在**默认产物**里同样红
⇒ ⇒ 那处才需要 `emit_hygiene` 分支。

⚠️ **本 change 的取舍**：mem 初值块**不做 `emit_hygiene` 分支** ⇒ ⇒
**无条件**写 `<W>'h0` ⇒ ⇒ 理由：
① `<W>'h0` 与 `1'b0` **语义完全等价**（都是零值）⇒ ⇒ **零行为变化**；
② 无条件写 ⇒ ⇒ **默认产物与卫生产物字节一致** ⇒ ⇒ 免掉「两个形态字节不同」的漂移；
③ 门禁 lint **只跑卫生产物** ⇒ ⇒ 默认产物带 blanket `lint_off` ⇒ ⇒ 不受影响。

⇒ ⇒ **影响面**：只有 `init_data` 非空的 mem ⇒ ⇒ **带固件产物的字节会变**
（`1'b0` → `32'h0`）⇒ ⇒ 无固件产物**逐字节不变**（AC5 判它）。

---

## 2. 覆盖分析：为什么既有护栏没抓到

| 护栏 | 它 lint 哪种产物 | 该形态的 `init_data` | TCM 初值块 | lint 结果 |
|---|---|---|---|---|
| `verify_rv32i_rtl_sim_env.sh` | `--emit-rtl --out <d> --emit-hygiene`（**默认固件** `rv32i_fw_demo::text()/data()`） | **空** | **无**（for 循环不发射） | 绿 |
| `verify_examples_rtl_sim.sh`（`kind=target`） | 同上（`tb_rv32i_soc`） | **空** | **无** | 绿 |
| `verify_examples_rtl_sim.sh`（`kind=hier`） | `--emit-rtl --hierarchical` | **空** | **无** | 绿 |
| `verify_examples_rtl_sim.sh`（`kind=soc`） | SoC 顶层 | **空** | **无** | 绿 |
| `verify_rv32i_fw_init_rtl.sh` | `--emit-rtl --firmware-init`（**带初值**） | **非空** | **有** | ⚠️ **它不跑 lint**（只判端口面 + 两次发射确定性）⇒ ⇒ 缺口 |
| **本 change** | `--emit-rtl --firmware <镜像目录>` | **非空** | **有** | 🔴 红（修后绿） |

⇒ ⇒ **盲区的精确形状**：「带固件初值」这条形态**存在**产物（`verify_rv32i_fw_init_rtl.sh`
就在用它）⇒ ⇒ 但**没有任何一处对它的形态跑 lint**。

---

## 3. 通用 TB 的设计（为什么它**不需要**装载镜像）

### 3.1 镜像在哪

`--emit-rtl --firmware <dir>` ⇒ ⇒ 镜像经 `ch_mem::init_data` ⇒ ⇒ **落在产物的
`initial` 块里**（实测：`top_itcm_itcm_memory[0] = 32'h93;` 对应镜像首字 `00000093`）。

⇒ ⇒ **TB 不需要装载** ⇒ ⇒ TB 是**镜像无关**的 ⇒ ⇒ 一个 TB 跑 40 个镜像
⇒ ⇒ 这正是「通用 TB」与既有 `tb_rv32i_soc.sv`（固定激励）的**本质差别**。

### 3.2 判据口径（与核级门禁**同源**）

上游 env 的协议：`tohost == 1` ⇒ PASS；`tohost == TESTNUM | 1` ⇒ FAIL。
⇒ ⇒ 观测 = 核的**既有**根端口上的 store 提交事件
（`top_obs_data_write_en` + `top_obs_data_addr` == `0x8001_0000` + `top_obs_data_write_data`）
⇒ ⇒ **零新增端口** ⇒ ⇒ 53 端口契约表与 C6 快照**不动**。

### 3.3 复位序列与拍数上限（**实测依据**，不是拍的）

- **复位**：框架复位 `default_reset` **高有效** + 核内 `ext_rst` **高有效**
  ⇒ ⇒ 复位期 `ext_rst` MUST 保持 1（否则计数器被装载污染 —— `examples/riscv-mini/AGENTS.md`
  的 IP 集成契约）⇒ ⇒ TB 采用「`rst=1` + `ext_rst=1` 保持 **4 拍** ⇒ 释放」，
  **与既有 `tb_rv32i_soc.sv` 同序列**（保证两个 TB 的观测语义可比）。

- **拍数上限**：解释器门禁用 `kMaxCycles = 4000`（`test_rv32i_official_suite.cpp:66`）
  ⇒ ⇒ RTL 侧用**同一上限** ⇒ ⇒ 两后端判据口径一致。
  ⚠️ **余量实测**（40 个镜像在 RTL 上的 **`tohost` 写入拍数**）：**41 ~ 1552**
  （最坏 = `rv32ui-p-ld_st` 的 1552）⇒ ⇒ 4000 上限的余量 **≈ 61%**。
  ⇒ ⇒ 门禁**记录**每个测试的实际拍数 ⇒ ⇒ 余量若被吃掉（某测试需要更多拍）
  ⇒ ⇒ 会以「没读到 `tohost`」的形式**显式红**，而不是静默截断。
  ⚠️ **口径辨析**：早前审查阶段 scratch 脚本记录的 `fetches`（≈2018–2772）是
  「`dbg_id_valid` 的拍数」，**不是**「`tohost` 写入拍数」⇒ ⇒ 两者不可混用
  （`retired > cycles` 那种组合本身就说明口径不同）⇒ ⇒ 本 change 的余量论证
  **只用** `tohost` 写入拍数（门禁自身打印的那个 `cycle=`）。

---

## 4. 门禁形态的取舍：脚本 vs CTest vs 扩展既有护栏

| 方案 | 墙钟 | 判据口径改动 | 既有护栏影响 | 裁决 |
|---|---|---|---|---|
| **A：40 个 CTest 条目** | 40 × ≈16 s ≈ **11 min** | `CTEST_TOTAL` **282 → 322** | 🔴 `verify_example_test_wiring.sh` 的 **C3 会实跑每个 examples 的 ctest 条目**（硬编码 300 s timeout）⇒ ⇒ 该护栏从 ≈10 min 涨到 ≈20 min | ❌ |
| **B：1 个 CTest 条目 + 过滤参数** | ≈11 min | `CTEST_TOTAL` +1 | 🔴 护栏 C3 按「**条目名 = 可执行文件名**」**无参**运行 ⇒ ⇒ 跑的是**默认集合** ⇒ 与条目**不等价** | ❌（上一轮已踩过同型坑） |
| **C：扩展 `verify_examples_rtl_sim.sh`（加 `kind=official`）** | 同 A | 🔴 `kind` 是**封闭集合** `{target, dut, soc, hier}` + **三个独立计数锚点** + 守恒判据「数据行总数 == 三桶之和」⇒ ⇒ 加第四桶要改**三处**判据 | 🔴 改既有护栏的判据口径 | ❌（改动最小化原则） |
| **D：独立脚本 + CI job** | 同 A（可抽样） | ✅ **零**既有判据改动 | ✅ 零 | ✅ **采纳** |

⇒ ⇒ **D 的形态**：`scripts/verify_rv32i_official_rtl.sh`（fail-closed · `--full` / `--subset N`）
⇒ ⇒ CI 落点 = `pr-verilator-fast`（抽样）/ `nightly-verilator`（全量）
⇒ ⇒ 护栏可达性类型 = `gate-event-only` ⇒ ⇒ `.rddf/checks/guard-reachability.md`
由 `verify_guard_reachability.sh --render` **自动渲染**（⇒ **禁手改**）。

---

## 5. 墙钟账（实测）

| 环节 | 单个 | 40 个 |
|---|---|---|
| 发射（`--emit-rtl --firmware`） | ≈1 s | ≈40 s |
| verilator **编译** | ≈15 s | ≈**10 min**（主导项） |
| verilator **运行** | **0.14 s** | ≈6 s |
| 解释器路径（对照） | ≈10 s | ≈7 min |

⇒ ⇒ **冷跑 ≈ 10 min**，与解释器侧（7 min）**同量级** ⇒ ⇒ 不是「更慢」，是「**判的是另一件事**」。
⇒ ⇒ **增量**：verilator 编译产物按内容哈希缓存 ⇒ ⇒ 镜像不变 ⇒ ⇒ 二次跑只需 ≈1 min。

---

## 6. 判别力设计（防恒绿）

| 判据 | 形态 | 自证方式 |
|---|---|---|
| **正向** | 40 个官方镜像 ⇒ `tohost == 1` | 上游 env 协议（**外部权威**，非自拟） |
| **负向对照** | 手写镜像写 `3` ⇒ MUST 读到 `3` | 读到 `1` ⇒ **门禁红**（判定被吞）⇒ ⇒ 证明门禁**不是恒绿** |
| **变异自证** | 把 TB 的 `tohost` 地址改错 | 门禁**必红** ⇒ ⇒ 证明地址判据**在起作用** |

⚠️ **负向镜像的手推依据**（4 个字）：
`lui x5, 0x80010`（`0x800102B7`）⇒ `x5 = 0x8001_0000`（= `tohost` 地址）·
`addi x6, x0, 3`（`0x00300313`）⇒ `x6 = 3` ·
`sw x6, 0(x5)`（`0x0062A023`）⇒ 把 **3** 写进 `tohost` ·
`jal x0, 0`（`0x0000006F`）⇒ 停机。
⇒ ⇒ 与**核级**门禁的负向对照**逐字节同源**（`test_rv32i_official_suite.cpp` 的负向用例）。

---

## 7. 被排除的方案（附理由）

| # | 方案 | 排除理由 |
|---|---|---|
| ① | TB 用 `$readmemh` 自行装载镜像 | 镜像**已在产物里**（`initial` 块）⇒ ⇒ 自行装载会与产物初值**双源** ⇒ ⇒ 且 `prog_*` 端口速率装载 4096 字需 4096+ 拍 ⇒ ⇒ 无收益 |
| ② | 把官方套件**塞进** `verify_examples_rtl_sim.sh` 的 tsv 登记表 | 该表的 `kind` 是**封闭集合** + 三个独立计数锚点 ⇒ ⇒ 改它 = 改判据口径（见 §4） |
| ③ | 只跑**抽样**不做全量 | 全量才是「官方套件全过」的**同一口径** ⇒ ⇒ 抽样只作为 **PR 快通道**（nightly 跑全量）⇒ ⇒ 两者并存 |
| ④ | 修 lint 时写成 `32'h0`（硬编码宽度） | mem 元素宽度**可非 32**（`ch_mem<ch_uint<8>, N>`）⇒ ⇒ MUST 取 `data_width()` |
| ⑤ | 在 `print_mem_init` 里做 `emit_hygiene` 分支 | 两个形态字节**不一致** ⇒ ⇒ 引入漂移面；且 `<W>'h0` 语义等价 ⇒ ⇒ 无条件写更省（见 §1.3） |
| ⑥ | 给门禁加「综合（yosys）」步骤 | 本机**无 yosys** ⇒ ⇒ 引入一个**本地不可跑**的门禁 ⇒ ⇒ 违反「护栏 MUST 在 CI 里真正 executed 且绿」（`guard-reachability.md` 纪律 3）⇒ ⇒ 登记为后续项 |
