# SoC 迁 `.verif`（R13 闭口）

## Why

蓝图 `docs/architecture/2026-11-17-rtl-verification-single-source.md` §7 **R13** 登记：SoC 产品的
验证语义有**两个物理载体** ——

| 载体 | 断言数 | 写入值 |
|---|---|---|
| `tb_axi_soc_example.cpp`（C++ 侧解释器） | **2** | `gpio_out == 0xAB` |
| `tb_axi_soc_example.sv`（**手写** Verilog TB） | **14** | `gpio_out == 0xA5` |

⇒ ⇒ 不只是「两份代码」，连**期望值都不同**（`0xAB` vs `0xA5`）⇒ ⇒ 任何一侧改了另一侧不会红。

根因（ADR-074 §7.1）：D2 的**封闭原语集**无法表达「等帧起点」这一整类判据 ⇒ ⇒ 手写是当时的
过渡态。**ADR-074 §7.2 D2′** 已为该缺口补上 `WAIT <port> [<high|low>] <max>`（已归档 change
`2026-11-20-verif-wait-level-2026-11` 落地）⇒ ⇒ 本 change 是它的**第一个真实消费者**，也是
**R13 闭口**的那一步。

## What Changes

### ① 新增验证源 `examples/axi4/tb_axi_soc_example.verif`

4 个 scenario，覆盖原手写 TB 的 4 个场景：

| scenario | 内容 | 断言数 |
|---|---|---|
| `bringup_and_gpio_write` | 释放 `rst_n` + 写 GPIO `DATA_OUT`(0x00) = `0xA5` ⇒ `gpio_out == 0xA5` | 1 |
| `gpio_read_data_in` | 读 GPIO `DATA_IN`(0x04) ⇒ `rdata == 0x5A` | 1 |
| `uart_tx_frame_8n1` | 写 UART `TX_DATA`(0x8000_0000) = `0x5A` ⇒ **帧起点等待** + 起始位 / 8 数据位 / 停止位 | 11 |
| `low_active_reset_clears_gpio` | `rst_n` 拉低 ⇒ `gpio_out == 0` 且 `uart_tx` 回空闲高 | 2 |

**帧起点用 `WAIT uart_tx low 240`**（ADR-074 §7.3 D3′ **点名**的受裁用法：等待**入口处尚未成立**
的目标电平 ⇒ 等的是**跃迁沿**）。
⇒ ⇒ 这一步替掉了手写 TB 的 `tx_samples[]` 逐拍采样数组 + `first_low` 定位（那正是 ADR-074 §7.4
选项 E 被否决的机制 —— 运行期数据决定后续采样点，撞「IR 里只有字面量」不变量）。
⇒ ⇒ 帧起点一旦由 `WAIT` 锚定，后续各位中点就是**字面量偏移**（bit 周期 16 拍，手推自期望源）
⇒ ⇒ **不需要**数组 / 变长偏移 / 表达式。

⚠️ **禁** `WAIT uart_tx high`：空闲时入口电平**已成立** ⇒ D3′ 明禁（`WAIT` 会立即返回 ⇒ 静默错语义）。

### ② `tb_axi_soc_example.cpp` 改调库层 helper

C++ 侧 2 条手写断言（`done` + 值）→ `rtl_emit::run_verif_file(...)`（该 helper 由上一个 change
的 AC7 下沉，Lite 已在用 ⇒ ⇒ 本 change 是它的**第二个消费者**，验证「同一份实现被两个 example
共用」不是口号）。

⇒ ⇒ C++ 侧断言数从 **2** 变 **N（== 验证源静态展开数）** ⇒ ⇒ 「同一份 `.verif` 驱动两侧」成立。

### ③ 生成物替换手写 TB（**同一 commit 原子翻转**）

`examples/axi4/tb_axi_soc_example.sv` 由 `--emit-tb-verif` **重新生成** ⇒ ⇒ 此后它是**生成物**
（⑥' 逐字节新鲜度、⑥‴ 无 TODO 残留、⑧'' 无 `unnamed` 字面量 首次对 SoC 有可判对象）。
⇒ ⇒ 手写版内容不丢：期望值推导全在 `.rddf/checks/soc-rtl-expectations.md#soc-example`，实现细节在
git 历史。

### ④ 清单行**原子翻转**（`generated=no` → `yes`）

`.rddf/checks/examples-rtl-sim-targets.tsv` 的 SoC 行：`generated` · `verif_path` ·
`interp_checks`（2 → N）· `min_checks`（14 → N）· `status` 同一次改动内一起变
⇒ ⇒ 不留「半迁态」（`verify_verif_migration_state.sh` 的两态自洽判据正是为此）。

### ⑤ 期望源登记补齐机器可读边

`.rddf/checks/soc-rtl-expectations.md#soc-example` 增 `VERIF-CHECKS: N` 行 + 4 个
`#### scenario: <name>` 锚点（与验证源 `@scenario` **双向 1:1**）+ scenario→判据出处表。
⚠️ `VERIF-CHECKS:` 的值 MUST **人写**（禁由生成器输出回填 ⇒ 否则「生成器少发断言」时两侧同时变 ⇒ 判据恒绿）。

### ⑥ 旧 → 新 断言**映射表**（AC-R13′-2 的硬要求）

逐条登记「旧 14 + 旧 2 条 C++ 断言」的**去向**（被哪条新断言承载 / 被合并 / 被移除及理由）
⇒ ⇒ 防「为让等式成立而删断言」。

## Capabilities

`examples-rtl-sim-coverage`（既有 capability）—— 本 change **不改** spec（判据本体在 ADR-074 §7，
`skip_specs: true`）。

## Impact

- **SoC 首次进入「生成式 TB」面**：⑥'/⑥''/⑥‴/⑧''/⑥⁗ 从「对 SoC 无对象」变为「有对象」。
- **`verify_verif_migration_state.sh`**：态 A（未迁）→ 态 B（已迁）⇒ ⇒ 归档时
  `--require-closed` 必须能过（R13 闭口门）。
- **断言数**：C++ 侧 2 → N；TB 侧 14 → N ⇒ ⇒ 两侧相等（⑨ 等式）。
- **锚点**：`min_checks` 等值变化（`CTEST_TOTAL` / `VERILATOR_E2E_TOTAL` **不变** —— 不新增 ctest 条目）。

## Out of Scope

| 项 | 理由 |
|---|---|
| 删除根自有 `timer_irq` 占位端口（台账 id=27） | 台账裁决已定方向，但**状态仍 `已知`**（事实未变）⇒ ⇒ 独立 change 收口（ADR-074 §7.7） |
| B0 的两条**前置序列**差异（复位脉冲多一个沿 · `RESET` 去断言无 `#1`） | 修哪一侧属**语义决策**（改渲染器文本会破 Lite 的「逐字节不变」）⇒ ⇒ 交 architect，不在本 change |
| 扩展原语集（数组 / 变长偏移 / 条件表达式） | ADR-074 §7.4 已逐条否决 ⇒ ⇒ 本 change **只用**既有封闭集 |
| 其他 example（phase3a / spinalhdl-ported 等）迁 `.verif` | 独立立项 |
| `--check-products`（双形态等价 CHECK 4） | 既有能力，本 change 不动 |

## 用户可见变更

| 项 | 前 | 后 |
|---|---|---|
| `tb_axi_soc_example` 的验证语义载体 | **两个**（C++ 侧 2 条 + 手写 `.sv` 14 条，**写入值都不同**） | **一个**（`.verif` 4 scenario / N 条断言驱动两侧） |
| `tb_axi_soc_example.sv` | **手写**（`generated=no`） | **生成物**（`generated=yes` ⇒ ⑥' 逐字节新鲜度可判） |
| `tb_axi_soc_example` 的 C++ 侧断言数 | 2（且写 `0xAB`） | N（与 TB 侧相等，写 `0xA5`） |
| `RESULT:` 行 | C++ `checks=2` · TB `checks=14` | **两侧同 N** |

## 验收标准

| # | 判据 | 方式 |
|---|---|---|
| AC1 | `examples/axi4/tb_axi_soc_example.verif` 存在，**只用封闭原语集**，4 个 `@scenario` 与期望源 `#### scenario:` 锚点**双向 1:1** | 扫描器 + ⑥'' |
| AC2 | 帧起点用 `WAIT uart_tx low <max>`（D3′ 受裁用法）· **无** `WAIT uart_tx high`（入口即成立 ⇒ D3′ 禁） | grep + 读文件 |
| AC3 | `tb_axi_soc_example.sv` **由验证源逐字节重现**（⑥'）· 无 TODO 残留（⑥‴）· 无 `unnamed` 字面量（⑧''） | 护栏 ⑥'/⑥‴/⑧'' |
| AC4 | tsv SoC 行**同一次改动**翻转：`generated=yes` · `verif_path` 非空 · `interp_checks == min_checks == N` · `status=verified` | awk + 护栏 |
| AC5 | **旧 → 新 映射表**存在且逐条（旧 TB 14 + 旧 C++ 2）：每条标注「新载体」或「移除理由」 | 读文件 |
| AC6 | `tb_axi_soc_example.cpp` 改调 `run_verif_file`（同一份实现）· 默认模式 `RESULT: checks=N fails=0` · `--emit-rtl` / `--check-products` 行为不变 | 运行 + grep |
| AC7 | `verify_examples_rtl_sim.sh` **PASS**（SoC 行首次走 ⑥'/⑥''/⑥‴/⑧''/⑥⁗）· `--self-test` **PASS** | shell |
| AC8 | `scripts/verify_verif_migration_state.sh --require-closed` **PASS**（R13 闭口门）· 不带 flag 的两态自洽仍 PASS | shell |
| AC9 | 期望源含 `VERIF-CHECKS: N`（**人写**）+ 4 个 `#### scenario:` 锚点；期望值仍**手推**（禁抄仿真输出） | 读文件 |
| AC10 | **变异自证**：改 `.verif` 的某数据位期望（如 `0x5A` 的 bit1）⇒ 至少一条构例转红（两端同时） | 手动变异 + 还原 |
| AC11 | 判别力：`WAIT` 的**两条**变异必红 —— ① `max` 改 `0` ⇒ 帧场景的 `#0` 必红（证明它**至少推进 1 拍**、不是 no-op）② `WAIT` **前**插 `TICK 8` ⇒ 锚点偏移 ⇒ 数据位判据必红（证明「帧起点锚定」决定后续偏移，不是碰巧对上）。⚠️ 实测登记：`max` 改 **1** **不**转红（帧起点在写握手后 1 拍内就出现 —— matrix m0→s1 是**组合**路由，期望源已登记）⇒ 该写法**不**作为判据 | 手动变异 + 还原 |
| AC12 | `ctest` **全绿** · `verify_test_counts.sh` 绿（锚点按实测回填，若变） | shell |
| AC13 | 归档：`openspec archive ... --skip-specs` ⇒ 主 spec **零变更** · `verify_public_api_visibility.sh` PASS（归档件含本段） | shell |
