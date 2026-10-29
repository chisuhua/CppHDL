# Tasks: spi-fsm-fixture-live-logic-2026-10

## 0. 准备（**承重约束核实** —— 全部**实测**，非推理）
- [x] 0.1 `reg->next` 的多次赋值语义 —— 实测 `regimpl::set_next` = `set_src(0, next)`
      （`include/ast/ast_nodes.h:47-54`）⇒ **last-wins（覆盖）**
      ⇒ ⇒ **每个寄存器 MUST 只有一个写入点**
- [x] 0.2 `ch_state_machine::build()` 的遍历语义 —— 实读
      `include/chlib/state_machine.h:296-301`（**按 state index 遍历**执行全部
      `on_active`）⇒ ⇒ 写在 `on_active` 内的 `reg->next` 必被**后执行者**覆盖
- [x] 0.3 `transition_when` 的 `target` MUST 是**编译期常量** —— 实读
      `include/chlib/state_machine.h:228-231`（`build()` 注册转移时固化 target）
      ⇒ ⇒ **禁**用三元 `? :` 选 target
- [x] 0.4 `ch_bool::operator uint64_t()` 只对 const 节点返真值 —— 实读
      `src/lnode/bool.cpp:44-49` ⇒ ⇒ 一切 `if` / 三元在**建图期**恒假
- [x] 0.5 `port<ch_bool>` 无 `ch_bool` 转换 —— 实读 `include/core/io.h:232`
      （**只有** `ch_uint` 有）⇒ ⇒ `start` MUST 经 `ch_reg<ch_bool>` 中转
      ⇒ 代价：条件**晚 1 拍**
- [x] 0.6 寄存器 proxy 在**组合表**里 ⇒ tick 末尾 comb 重算 ⇒ 采样点 = 边沿后
      —— 实读 `src/simulator_init.cpp:139-143`（`default:` ⇒ comb）
- [x] 0.7 `instr_reg::eval()` 用 `assign_truncate` ⇒ 4-bit 中间值落进 3-bit 寄存器
      **安全截断** —— 实读 `src/ast/instr_reg.cpp:56`
- [x] 0.8 ⚠️ **只读实测缺陷现状**（探针 `/tmp/opencode/probe_spi.cpp`）：
      `shift` 恒 `0x00` / 状态卡死 TRANSFER / `mosi` 恒 0 / `bit_count` 从不 reset
      ⇒ ⇒ 三处缺陷**实测确认**，非推理

## 1. 实现（`tests/test_verilator_complex_e2e.cpp`）
- [x] 1.1 `on_active` 里**只**写 `transition_when`（**不**碰任何 `reg->next`）
- [x] 1.2 TRANSFER 拆成**两条** `transition_when`（`last_bit` → STOP /
      `!last_bit` → TRANSFER）⇒ **禁**三元选 target
- [x] 1.3 `start_latch` 提到 `describe()` 顶层（闭包内**不**建寄存器）
- [x] 1.4 `sm.build()` **之后**写 2 个寄存器的**唯一** next 写入点
      （`bit_count` / `shift`，各一条 `select` 组合表达式）
- [x] 1.5 `sm.build()` **之后**写 3 个输出的**唯一**驱动点
      （`dbg_state` / `mosi_out` / `dbg_bit_count`）
- [x] 1.6 `mosi` 取 `shift[7]`（MSB-first）⇒ 与 spec 场景的
      「发送字节的 MSB-first 序列」一致（发送字节 MUST **非回文**，见 2.7）
- [x] 1.7 **未**用三元 `? :`；**未**用 `if (硬件值)`
- [x] 1.8 编译 0 error / 0 **新增** warning（`logger.h:194` 是**既有噪声**）

## 2. 语义断言（**判别力是承重件** —— 期望值**手推**，不得抄仿真输出）
- [x] 2.1 采样点在**每次 `tick(1)` 之后**（⇒ 边沿后 ⇒ 探针 §2.4）
- [x] 2.2 **手推**状态序列金标（2 次传输）：
      `IDLE, START, TRANSFER×8, STOP, IDLE, IDLE, START, TRANSFER×8, STOP, IDLE…`
      （`design.md` §3.1 的逐拍表）⇒ ⇒ **每次传输都必过 STOP**
- [x] 2.3 **手推**`bit_count` 序列金标：
      `0,0,0,1,2,3,4,5,6,7,7,7,7,0,0,1,2,3,4,5,6,7,7…`
      ⇒ ⇒ 第 2 次装载处（tick 13）**必须**回到 0 ⇒ 复位**可观测**
- [x] 2.4 **手推**`mosi` 序列金标：`0,0,1,0,0,1,0,1,1,0,0,0,0,0,1,0,0,1,0,1,1,0,0…`
      ⇒ 每个 TRANSFER 段 = `1,0,0,1,0,1,1,0` = **`0x96`** 的 MSB-first ✔
      ⇒ 2 次传输 ⇒ 16 个 TRANSFER 拍 ⇒ 两段完全相同 ✔
- [x] 2.5 断言**两后端都**满足金标（**不只是**彼此相等）
- [x] 2.6 保留原 parity 断言（逐拍两后端一致）⇒ parity 是**必要非充分**条件
- [x] 2.7 ⚠️ **判别力前提（实测踩到）**：发送字节 MUST **非回文**
      （MSB-first ≠ LSB-first）⇒ ⚠️ 原 spec 的 `0xA5` = `1010 0101` 是**回文**
      ⇒ 其 MSB-first 与 LSB-first **完全相同**（都是 `1,0,1,0,0,1,0,1`）
      ⇒ ⇒ 「位序」维度**零判别力** ⇒ 「`mosi` == MSB-first 序列」这条断言
      **抓不到**位序错 ⇒ ⇒ 发送字节改为 **`0x96`** = `1001 0110`
      （MSB-first = `1,0,0,1,0,1,1,0`；LSB-first = `0,1,1,0,1,0,0,1` ⇒ **不同**）
- [x] 2.8 ⚠️ **判别力前提（实测踩到）**：传输次数 MUST ≥ **2**
      ⇒ 单次传输时「装载时复位计数器」**不可观测**（计数器在装载前本来就是 0）
      ⇒ ⇒ 变异 **M6 全绿**（实测）⇒ ⇒ 加第 2 次传输（`start` 在 cycle 12 再拉高）
      ⇒ ⇒ M6 转红

## 3. 变异验证（**每条先确认编译成功**，否则跑旧二进制 ⇒ 假绿）
- [x] 3.0 ⚠️ **注入器 fail-closed** —— 变异用**行号**注入
      （`/tmp/opencode/mutate_spi.py`）：行号越界 / 目标子串不匹配 ⇒ `MUTATE_FAIL`
      且 **exit 2** ⇒ 杜绝「注入失败却跑旧代码 ⇒ 假绿」
      ⇒ 驱动 `/tmp/opencode/run_spi_mutations.sh`：注入 → 编译（**必须 0 error**）
      → 运行（**必须红**）→ 复原
- [x] 3.1 M1 装载条件的状态写错（IDLE → START）⇒ 装载永不发生 ⇒ ✅ **转红**
      （`mosi_seq`）
- [x] 3.2 M2 **恢复三元选 target**（`last_bit ? STOP : TRANSFER`）= **原缺陷形态**
      ⇒ ✅ **转红**（`state_seq`）
- [x] 3.3 M3 `STOP` 的条件恒假 ⇒ `STOP` 不可达 ⇒ ✅ **转红**（`state_seq`）
- [x] 3.4 M4 位序反（`mosi` 取 `shift[0]` = LSB-first）⇒ ✅ **转红**（`mosi_seq`）
      ⇒ ⚠️ **只因 `0x96` 非回文才转红**（`0xA5` ⇒ 该变异**恒绿**）
- [x] 3.5 M5 移位量 1 → 2 ⇒ ✅ **转红**（`mosi_seq`）
- [x] 3.6 M6 装载时**不复位**计数器 ⇒ ✅ **转红**（`count_seq`）
      ⇒ ⚠️ **只因做 2 次传输才转红**（单次传输时该变异**恒绿** ⇒ 实测发现）
- [x] 3.7 M7 `last_bit` 判据 7 → 6 ⇒ 少发一位 ⇒ ✅ **转红**（`state_seq`）
- [x] 3.8 M8 `mosi` 的非 TRANSFER 分支改成 MSB ⇒ 输出门控缺失
      ⇒ ✅ **转红**（`mosi_seq`）
- [x] 3.9 M9 去掉 `& !last_bit` ⇒ 计数器在最后一位也 +1（回绕）
      ⇒ ✅ **转红**（`count_seq`）
- [x] 3.10 ⇒ ⇒ **9/9 全被抓**，每条编译错误数 = **0**
- [x] 3.11 ⭐ **「旧 fixture + 新断言 ⇒ 转红」**（`/tmp/opencode/revert_describe.py`
      把 `describe()` 换回 HEAD 的旧版，但保留新 TB 与新断言）
      ⇒ ✅ **转红**（首个失败 = `count_seq[0]`：**1 ≠ 0**）
      ⇒ ⇒ 直接证明「旧断言（仅 parity）长期假绿」被打破

## 4. 验收
- [x] 4.1 `ctest -LE perf` ⇒ **191/191**（357.03 s）
- [x] 4.2 `run_all_ported_tests.sh` ⇒ **28/28**
- [x] 4.3 `run_all_samples.sh` ⇒ **35/35**
- [x] 4.4 **计数锚点不变**：`CTEST_TOTAL` **202** / `EXAMPLES_TEST_SOURCES` **17** /
      `ctest -L verilator_e2e` **38** / non-perf **191**
      （本 change **不新增**测试文件 ⇒ `EXAMPLES_TEST_SOURCES` 亦不变）
- [x] 4.5 12 个 `scripts/verify_*.sh`（含 3 个 `--self-test`）全 exit 0
- [x] 4.6 `openspec validate --specs --strict` 的 warning 属**既有 CLI 版本噪声**
      （仓钉 1.4.1 / 本机 1.14.3；CI **不**跑该命令）
- [x] 4.7 生成的 Verilog 结构性证据：状态寄存器**无自环**
      （`top_reg <= top_mux_select_5;`）· 三个输出**各 1 条** assign ·
      `eq_eq_1..8` **有消费者**（5–7 次）

## 5. 文档与归档
- [x] 5.1 根 `AGENTS.md` NOTES 记录
- [x] 5.2 `.rddf/roadmap.md` 的 sprint 表增行（**簿记**，非 feature `status`）
- [x] 5.3 `openspec archive spi-fsm-fixture-live-logic-2026-10 --yes`
      （**禁** `--skip-specs` —— 本 change 有 2 个 MODIFIED delta）
