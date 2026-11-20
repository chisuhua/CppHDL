## 2026-11-21 — SoC 顶层删掉零消费的 `timer_irq` 占位端口（台账 id=27 落地）

**是什么**：`examples/axi4/axi_soc_example_top.h` 的根 `__io` 里有一个 `timer_irq` 输入端口
⇒ ⇒ **DUT 从不读它**（全仓 grep：声明 1 命中 · 消费者 0 命中）⇒ ⇒ 它**在顶层端口面里但没有
语义** ⇒ ⇒ 下游会误以为「接上就能收中断」⇒ ⇒ 且 TB 还被迫为它接一个常量
（否则 verilator 严格 flags 下 `%Warning-PINMISSING` 致命）⇒ ⇒ **端口存在**被误当成
**端口有义务**。

⇒ ⇒ 架构裁决（`ADR-074` §7.7 · 台账 **id=27** · `ext_clk` 同型先例 · 用户 **2026-11-20**
追认）= **删除该占位端口** ⇒ ⇒ 本条 = 该裁决的实施。

**改了什么**

- **设计**：`examples/axi4/axi_soc_example_top.h` 删 `ch_in<ch_bool> timer_irq` ⇒ ⇒
  根自有 `__io` **12 → 11**（8 in + 4 out → **7 in + 4 out**）⇒ ⇒ 顶层端口 **14 → 13**。
- **验证面**：`.verif` 删 `SET timer_irq 0` · TB 的 `make_inputs()` 删该输入项（根 `ch_in`
  全集 8 → 7）· `make_stimulus()` 每行删最后一列（14 行）· `kExpectedTopPorts` 14 → 13 ·
  `tb_axi_soc_example.sv` **由 `.verif` 重新渲染**（单一真相源 ⇒ 禁手改生成物）。
- **期望源 / 清单**：`.rddf/checks/soc-rtl-expectations.md`（端口表删行 10 + 重编号 +
  输入常量 + 场景出处 + 盲区段）· `.rddf/checks/examples-rtl-sim-targets.tsv`（SoC 行 note）。
- **残留登记**：台账 **id=27** 与残留登记表**两侧同批**转「已收口」（闭口证据 = 归档 change）。

**行为不变**：删的是**零消费**端口 ⇒ ⇒ 四个场景的期望值**逐字不变**
（`VERIF-CHECKS: 15` / `RESULT: checks=15 fails=0` 不变）⇒ ⇒ 产物形态判据
（module 集 5 · lint · 双形态逐拍等价）全绿。

**⚠️ 不动库层 `AxiLiteUart::irq`**（台账 **id=26** 的裁决 = **不删**：它是 in-force spec
`dual-backend-test-coverage` 强制的行为覆盖载体）⇒ ⇒ 两者语义**不可混用**。

## 2026-11-20 — stream_arbiter 示例进 RTL 产品面（stream 目录 4/4 · 并修掉一处 codegen 宽度缺陷）

**是什么**：`examples/stream/` 的 arbiter demo 此前是「`main()` + 内联激励 + `std::cout`
比对」⇒ ⇒ 没有设计定义、没有 RTL 产物、期望值散在代码里。现在按同型形态产品化：
设计定义 `stream_arbiter_top.h`（16 根具名端口）→ 激励驱动 `tb_stream_arbiter_demo.cpp`
（默认 `.verif` 驱动 / `--emit-rtl` 发射）→ 验证源 `tb_stream_arbiter_demo.verif`
（5 scenario / **38** 断言）→ 生成的独立 TB `tb_stream_arbiter_demo.sv`。

**⚠️ 与前三个示例的本质差异**：arbiter 是**时序**的（内部 `last_grant_reg` 轮转反馈寄存器）
⇒ ⇒ 判据 MUST 用 `TICK` / `WAIT` 推进拍 ⇒ ⇒ 且**相位**（首拍授予谁）取决于实现的状态初值。

**改了什么**

- **新增** `examples/stream/stream_arbiter_top.h`：`chlib::stream_arbiter_round_robin<ch_uint<8>, 4>`
  的具名端口包装（4 路输入流 + 输出流 + `selected[1:0]`）。
- **改名 + 双模式** `stream_arbiter_demo.cpp` → `tb_stream_arbiter_demo.cpp`；默认模式只跑
  `.verif`（`RESULT: checks=38 fails=0`），`--emit-rtl <dir>` 产出 `top.v` + `manifest.json`。
- **清单登记**：`.rddf/checks/examples-rtl-sim-targets.tsv` 新增行（自发行 · `generated=yes` ·
  `interp_checks=38` == `min_checks=38`）⇒ ⇒ `REGISTERED_TARGETS` **14 → 15**；
  落选表删 `stream_arbiter_demo` 行 ⇒ ⇒ 守恒 `27 == 15 ∪ 12`。
- **期望源**：`.rddf/checks/examples-rtl-expectations.md#stream-arbiter-demo`
  （module 集 `{top}` · 顶层端口 **18** = 框架 2 + 根自有 16 · `VERIF-CHECKS: 38` + 5 个 scenario 锚点）。

**🔴 顺带修掉一处 codegen 宽度缺陷（`src/codegen_verilog.cpp`）**

arbiter 是**第一个被发射**的轮转组合子 ⇒ ⇒ 它的 RTL 产物**编译不过**（Verilator
`WIDTHTRUNC` × 5）⇒ ⇒ 根因 = hygiene 模式只把**操作数**宽度写成显式 cast，
**结果宽度没对齐** ⇒ ⇒ `mod` 节点的声明宽（框架按「模字面量 ⇒ 结果宽 = `bit_width(N-1)`」
定宽 = 2 bit）小于运算自然宽（`max(两侧)` = 3 bit）⇒ ⇒ 赋值处**隐式截断** ⇒ ⇒ 告警。

⇒ ⇒ 修法 = **把截断写成显式 cast**（与既有卫生口径同型）⇒ ⇒ **语义逐位不变**
（显式截断 ≡ 隐式截断）⇒ ⇒ 产物从 `assign x = a % 3'h4;` 变成
`assign x = 2'(a % 3'h4);` ⇒ ⇒ Verilator 零告警。

⚠️ **卫生产物刻意不带 blanket `lint_off`**（护栏设计如此 ⇒ 宽度错位 MUST 真的被看见）
⇒ ⇒ 这类缺陷**只能**在产品化过程中暴露 ⇒ ⇒ 也说明「9 个 example 卫生产物零告警」
那条旧结论**只覆盖当时那 9 个**。

**说明**

- `spec_checks = 37`（场景 1–5）；`no_valid_no_grant` 里的 `selected == 0` 属
  impl-contract（= `priority_encoder(0)` 的取值，**非**契约条款）。
- ⚠️ **本产品面【不锁相位】**：capability `chlib-stream-arbitration` 明文
  「MUST NOT 要求某一具体相位」⇒ ⇒ 判据一律写成**效果属性**（每路在有限拍内被服务，
  用 `WAIT` + 紧界表达）⇒ ⇒ 逐拍金标已在 `tests/chlib/test_stream.cpp` 的 `[rotation]`
  判别力用例里 ⇒ ⇒ 产品面**不重复**锁它。
- **判别力自证**：M1 `out_valid` 恒 0 → 3 条红；M2 `selected` 恒 0 → 3 条红；
  M3 `last_grant` 反馈恒 0（= capability 记载的**历史缺陷形态**：轮转从不发生）
  → **9 条红**（含 **5 条 `WAIT` 超时**）⇒ ⇒ 证明「相位无关」的判据**照样**抓得住轮转失效。

## 2026-11-20 — stream_fork 示例进 RTL 产品面（stream 目录第 3 个产品单元）

**是什么**：`examples/stream/` 的 fork demo 此前是「`main()` + 内联激励 + `std::cout`
比对」⇒ ⇒ 没有设计定义、没有 RTL 产物、期望值散在代码里。现在按 mux 的同型形态
产品化：设计定义 `stream_fork_top.h`（12 根具名端口）→ 激励驱动 `tb_stream_fork_demo.cpp`
（默认 `.verif` 驱动 / `--emit-rtl` 发射）→ 验证源 `tb_stream_fork_demo.verif`
（4 scenario / **26** 断言）→ 生成的独立 TB `tb_stream_fork_demo.sv`。

**改了什么**

- **新增** `examples/stream/stream_fork_top.h`：`chlib::stream_fork<ch_uint<8>, 3>` 的
  具名端口包装（输入流 + 3 路输出流）⇒ ⇒ 调用方先 `as_master()` 建字段节点，ready 链用
  `<<=`，消费端用 `<<=` **覆盖**输出 ready 的默认驱动（形态 E · 单阶段）。
- **改名 + 双模式** `stream_fork_demo.cpp` → `tb_stream_fork_demo.cpp`；默认模式只跑
  `.verif`（`RESULT: checks=26 fails=0`），`--emit-rtl <dir>` 产出 `top.v` + `manifest.json`。
- **清单登记**：`.rddf/checks/examples-rtl-sim-targets.tsv` 新增行（自发行 · `generated=yes` ·
  `interp_checks=26` == `min_checks=26`）⇒ ⇒ `REGISTERED_TARGETS` **13 → 14**；
  落选表删 `stream_fork_demo` 行 ⇒ ⇒ 守恒 `27 == 14 ∪ 13`。
- **期望源**：`.rddf/checks/examples-rtl-expectations.md#stream-fork-demo`
  （module 集 `{top}` · 顶层端口 **14** = 框架 2 + 根自有 12 · `VERIF-CHECKS: 26` + 4 个 scenario 锚点）。

**说明**

- `spec_checks = 19`（场景 2–4：广播语义 / ready 的 AND 语义 / valid 只依赖 `in.valid`）；
  场景 1 的「复位后默认态」属 impl-contract（框架复位置 0）。
- ⚠️ **AND 语义 MUST 覆盖两面**（capability 明文要求）：场景 2 判「全部为高 ⇒ 1」，
  场景 3 把**中间那一路**拉低判「任一为低 ⇒ 0」⇒ ⇒ 可区分「OR」与「只看第一路」。
- ⚠️ **只覆盖同步语义**：`synchronous = false` 在建图期**响亮拒绝**（ADR-057 D2）
  ⇒ ⇒ 异步 fork **不在判据面内**（**如实登记**，不假装覆盖）。
- arbiter **本次未做**（独立 change）⇒ ⇒ 仍留在落选表并带归位条件。

## 2026-11-20 — stream_demux 示例进 RTL 产品面（stream 目录第 2 个产品单元）

**是什么**：`examples/stream/` 的 demux demo 此前是「`main()` + 内联激励 + `std::cout`
比对」⇒ ⇒ 没有设计定义、没有 RTL 产物、期望值散在代码里。现在按 mux 的同型形态
产品化：设计定义 `stream_demux_top.h`（16 根具名端口）→ 激励驱动 `tb_stream_demux_demo.cpp`
（默认 `.verif` 驱动 / `--emit-rtl` 发射）→ 验证源 `tb_stream_demux_demo.verif`
（4 scenario / **38** 断言）→ 生成的独立 TB `tb_stream_demux_demo.sv`。

**改了什么**

- **新增** `examples/stream/stream_demux_top.h`：`chlib::stream_demux<ch_uint<8>, 4>` 的
  具名端口包装（输入流 + `select[1:0]` + 4 路输出流）⇒ ⇒ 调用方先 `as_master()` 建字段
  节点，ready 链用 `<<=`，消费端用 `<<=` **覆盖**输出 ready 的默认驱动（形态 E · 单阶段）。
- **改名 + 双模式** `stream_demux_demo.cpp` → `tb_stream_demux_demo.cpp`；默认模式只跑
  `.verif`（`RESULT: checks=38 fails=0`），`--emit-rtl <dir>` 产出 `top.v` + `manifest.json`。
- **清单登记**：`.rddf/checks/examples-rtl-sim-targets.tsv` 新增行（自发行 · `generated=yes` ·
  `interp_checks=38` == `min_checks=38`）⇒ ⇒ `REGISTERED_TARGETS` **12 → 13**；
  落选表删 `stream_demux_demo` 行 ⇒ ⇒ 守恒 `27 == 13 ∪ 14`。
- **期望源**：`.rddf/checks/examples-rtl-expectations.md#stream-demux-demo`
  （module 集 `{top}` · 顶层端口 **18** = 框架 2 + 根自有 16 · `VERIF-CHECKS: 38` + 4 个 scenario 锚点）。

**说明**

- `spec_checks = 32`（场景 2–4：分发语义 / ready 方向与隔离 / valid 只依赖 `in.valid`）；
  场景 1 的「复位后默认态」属 impl-contract（框架复位置 0）。
- ⚠️ **「越界 ⇒ 挡住上游」条在本设计不可达**（`select` 2 bit 且 `N = 4` ⇒ 恒在范围内）
  ⇒ ⇒ **如实登记**在期望源段（不假装覆盖）。
- demux 与 mux 的 **ready 方向相反**（demux = 消费端 ⇒ 上游；mux = 上游侧扇出）
  ⇒ ⇒ 判据面不重叠 ⇒ ⇒ 不是重复覆盖。
- arbiter / fork **本次未做**（各自独立 change）⇒ ⇒ 仍留在落选表并带归位条件。

## 2026-11-20 — stream_mux 示例进 RTL 产品面（stream 目录的首个产品单元）

**是什么**：`examples/stream/` 的 mux demo 此前是「`main()` + 内联激励 + `std::cout`
比对」⇒ ⇒ 没有设计定义、没有 RTL 产物、期望值散在代码里。现在按 axi4 的同型形态
产品化：设计定义 `stream_mux_top.h`（16 根具名端口）→ 激励驱动 `tb_stream_mux_demo.cpp`
（默认 `.verif` 驱动 / `--emit-rtl` 发射）→ 验证源 `tb_stream_mux_demo.verif`
（4 scenario / **24** 断言）→ 生成的独立 TB `tb_stream_mux_demo.sv`。

**改了什么**

- **新增** `examples/stream/stream_mux_top.h`：`chlib::stream_mux<4, ch_uint<8>>` 的
  具名端口包装（4 路 × {payload, valid, ready} + `select[1:0]` + 输出流）⇒ ⇒
  调用方先 `as_master()` 建字段节点，ready 链用 `<<=`（capability 契约）。
- **改名 + 双模式** `stream_mux_demo.cpp` → `tb_stream_mux_demo.cpp`；默认模式只跑
  `.verif`（`RESULT: checks=24 fails=0`），`--emit-rtl <dir>` 产出 `top.v` + `manifest.json`。
- **清单登记**：`.rddf/checks/examples-rtl-sim-targets.tsv` 新增行（自发行 · `generated=yes` ·
  `interp_checks=24` == `min_checks=24`）⇒ ⇒ `REGISTERED_TARGETS` **11 → 12**；
  落选表删 `stream_mux_demo` 行 ⇒ ⇒ 守恒 `27 == 12 ∪ 15`。
- **期望源**：`.rddf/checks/examples-rtl-expectations.md#stream-mux-demo`
  （module 集 `{top}` · 顶层端口 **18** = 框架 2 + 根自有 16 · `VERIF-CHECKS: 24` + 4 个 scenario 锚点）。

**说明**

- `spec_checks = 18`（场景 2–4：选择语义 / ready 扇出 / valid 不外泄）；场景 1 的
  「复位后默认态」属 impl-contract（框架复位置 0）。
- demux / arbiter / fork **本次未做**（用户选定 mux 试点）⇒ ⇒ 仍留在落选表并带归位条件。

## 2026-11-20 — SoC 迁 `.verif`（R13 闭口 · 双载体消灭）

**是什么**：`tb_axi_soc_example` 的验证语义此前有**两个物理载体** —— C++ 侧 2 条（写 `0xAB`）
+ **手写** `.sv` 14 条（写 `0xA5`）⇒ ⇒ 连期望值都不同，任何一侧改了另一侧不会红。
现在收敛为**一份** `.verif`（4 scenario / **15** 条断言）⇒ ⇒ 解释器侧与生成的 Verilog TB
由同一份源驱动。

**改了什么**

- **新增** `examples/axi4/tb_axi_soc_example.verif`：帧起点用 `WAIT uart_tx low 240`
  （ADR-074 §7.3 D3′ 受裁用法 —— 入口电平**未成立** ⇒ 等跃迁沿）⇒ ⇒ 起点锚定后各位中点
  = **字面量偏移**（bit 周期 16 拍）⇒ ⇒ **不需要**数组 / 变长偏移（那正是 ADR-074 §7.4
  选项 E 被否决的机制）。
- **`tb_axi_soc_example.cpp`**：默认模式改调库层 `run_verif_file`（Lite 之后的**第二个**
  消费者 ⇒ 验证「同一份实现」不是口号）· `--emit-rtl` / `--check-products` 路径不动。
- **`tb_axi_soc_example.sv`**：由验证源**重新生成**（替换手写版）⇒ ⇒ ⑥' 逐字节新鲜度 /
  ⑥‴ 无 TODO / ⑧'' 无 `unnamed` 字面量 **首次对 SoC 有可判对象**。
- **tsv SoC 行原子翻转**：`generated no→yes` · `interp_checks 2→15` · `min_checks 14→15`
  ⇒ ⇒ `verify_verif_migration_state.sh --require-closed`（R13 归档门）**PASS**。
- **期望源** `.rddf/checks/soc-rtl-expectations.md` 增 `VERIF-CHECKS: 15`（**人写**）+
  4 个 `#### scenario:` 锚点（与验证源双向 1:1）。

**判别力自证**（手推期望值 · 非抄输出）

| 变异 | 结果 |
|---|---|
| 数据位 bit1 期望 `1→0` | `uart_tx_frame_8n1#3` 转红（id 映射正确） |
| `WAIT` 的 `max=0` | `#0` 转红（证明它**至少推进 1 拍**、不是 no-op） |
| `WAIT` **前**插 `TICK 8` | 4 条数据位判据转红（证明**锚点**决定后续偏移，不是碰巧对上） |

⚠️ 如实登记：`WAIT` 的 `max` 改 **1** **不**转红 —— 帧起点在写握手后 **1 拍内**就出现
（matrix m0→s1 是**组合**路由，期望源已登记）⇒ 该写法**不**作为判据。

**断言数变化**：C++ 侧 2 → **15** · TB 侧 14 → **15** ⇒ ⇒ 两侧相等（⑨ 等式）。
旧 C++ 的 `done` 断言**移除**：`done` 是**粘滞**信号（首笔后恒 1 ⇒ 上升沿不可判，期望源已登记）
⇒ 值正确已蕴含事务完成。逐条映射表见 change 的 `tasks.md`。

**遗留（交 architect）**：蓝图 `docs/architecture/2026-11-17-rtl-verification-single-source.md`
的 **R13 行**与 §「手写过渡态」段仍写「未闭」⇒ ⇒ 该文件属 architect 写集，本次未改。

## 2026-11-20 — `.verif` 的 `WAIT` 增补电平形参（ADR-074 §7.2 D2′）

**是什么**：`WAIT` 从「只等**非零**」扩展为 `WAIT <port> [<high|low>] <max>` ⇒ ⇒ 可表达
「等目标电平的**跃迁沿**」（帧起点等待）⇒ ⇒ 为 SoC 迁 `.verif` 铺路（蓝图 §7 R13 的闭口前提）。

**改了什么**

- **三处同步**（缺一即两端各理解一次 ⇒ 静默差 1 拍）：`verif.h` 加 `WaitLevel`（**不复用**
  `Reset` 的 0/1 电平 —— 默认 0 会被读成 `low` ⇒ 语义翻转）· 扫描器 2/3 实参 + `level` 关键字
  校验（数字 / `HIGH` ⇒ 非零退出且**点名 `level`**）· 解释器与渲染器**逐字同语义**。
- **`high` ≡ 非零 ≡ 缺省**（ADR-074 §7.2 的 2026-11-20 dated 订正）⇒ **不是**「N 位全 1」
  （二者仅 N=1 时等价）⇒ 判定是**零值测试** ⇒ 不需要端口位宽。
- **向后兼容**：缺省写法语义逐字不变 ⇒ Lite 的 checked-in `.sv` **逐字节不变**
  （`cmp` + sha256 双证）。
- **运行期同结论**判据（`tests/test_verif_wait_level.cpp`）：同一份 `.verif` 在 IR 解释器与
  **真 verilator 渲染 TB** 各跑一次 ⇒ 逐条 `(check_id, PASS|FAIL)` 比对 + 6 条时序构例
  （含「恰在 max 拍命中 / max−1 必红」的变异自证）。⚠️ 这是对既有「静态 `check_id` 集合比对」
  的补强 —— 后者对**时序敏感**原语**结构性失明**。

**顺带登记（**不修**）**：实施中发现两条**前置序列**差异（非 `WAIT` 引入）——
① 渲染式 TB 的复位脉冲末尾多一个时钟沿（解释器无）② `RESET` 原语的去断言与最后一个
`@(posedge)` 同拍（与 DUT 的 `always_ff` 竞态）⇒ 两者都会让「依赖复位后绝对拍数」的场景
两端差 1 拍 ⇒ 修哪一侧属**语义决策** ⇒ 交 architect（改渲染器文本会破「Lite 产物逐字节不变」）。

**文档**：`docs/usage_guide/04-testbench-guide.md` 的 `WAIT` 行 + 新增「`WAIT` 的已知边界」
（非零语义 · X 态两端不对称 · >64 位端口只覆盖低 64 位 · `max=0` 恒 FAIL）。

## 2026-11-20 — phase3a 进 RTL 产品面（端口具名 + `--emit-rtl` + 冻结手推期望）

**是什么**：`examples/axi4/phase3a`（4×4 Interconnect + AXI-to-Lite Converter + AXI4-Lite
Slave）从「只有 C++ 侧验证」变成**登记在册的 RTL 产品面** ⇒ ⇒ 它的 **Verilog 产物首次被
独立验证**（此前只有解释器路径的 2 条断言）。

**改了什么**

- **端口具名**（ADR-075 E1 · 硬前置）：`phase3a_top.h` 的 7 个顶层端口全部具名 ⇒ ⇒ 产物端口名
  = `top_<C++ 成员名>` ⇒ ⇒ 独立 TB 能**按名连线**（未具名 ⇒ `top_unnamed_input_N` ⇒ 不可能）。
  连带：豁免表删 7 行 ⇒ 棘轮 `EXPECTED_EXEMPT_PORTS` **1384 → 1377**。
- **`--emit-rtl` 模式**（照 Lite / SoC 同型 ⇒ 复用库层 `strip_mode_flags` / `require_out_dir` /
  `run_emit_cli`）⇒ ⇒ 默认模式行为**逐字不变**。
- **可发射性前置**（ADR-073 D2 的同型）：interconnect 的后代 `ch_in`（m1–m3 各 14 根 +
  s1–s3 各 11 根 = **75 根**）此前**悬空** ⇒ ⇒ 发射 fail-closed（实测 75 条
  `dangling_input` 诊断）⇒ ⇒ 补 tie-off 到常量 0（**与「未接线读出 0」行为等价** ⇒ C++ 侧
  2 条断言仍绿 ⇒ 可复算）；另补 `m0_wid`（**此前漏接**，同属悬空）。
- **独立 TB**（**手写** · `examples/axi4/tb_phase3a_example.sv`）：9 根连线（框架 2 + 根自有 7）
  + **6 条断言** ⇒ `RESULT: checks=6 fails=0`。
- **期望源**（`.rddf/checks/examples-rtl-expectations.md#phase3a-example` · **手推**）：
  module 集 = **{top}**（1 个）· 顶层端口表 = **9** 行 · 6 条断言的逐条推导 · 变异自证登记。
- **清单**：新增 `tb_phase3a_example` 行 ⇒ `REGISTERED_TARGETS` **10 → 11**；**落选表删该行**
  （原理由「阶段汇总回归 · 非独立产品单元」已被本 change 取代 ⇒ 理由留痕在 proposal）⇒ ⇒
  守恒式 `27 == 11 ∪ 16`（两处陈旧值 `28 == 10 ∪ 18` 同批订正）。

**用户可见变更**

| 项 | 前 | 后 |
|---|---|---|
| `phase3a` 的 RTL 产物 | **不可发射**（75 条 `dangling_input` ⇒ exit 1） | **可发射**（exit 0 · 1 module · 9 端口） |
| 产物端口名 | `top_unnamed_input_N` 形态 | `top_start` / `top_addr` / `top_rdata` / … （**具名**） |
| `tb_phase3a_example --emit-rtl --out <dir>` | 不支持 | **支持**（与 Lite / SoC 同型） |
| 独立 TB | 无 | `tb_phase3a_example.sv`（6 条断言 · `checks=6 fails=0`） |
| 端口命名棘轮 | 1384 | **1377**（7 根具名 ⇒ 债减少） |

**手推期望的两处订正（实测暴露 · 已如实登记在期望源）**

1. 初稿把断言 ⑤ 写成「`rst_n` 拉低 ⇒ `done == 0`」⇒ ⇒ **错** —— `done_reg` 的复位源是**框架**
   `default_reset`（`ch_reg` 共享 default_reset），**不是**根 `rst_n`。
2. 二次订正写成「`rst_n` 拉低 ⇒ 再读同址 ⇒ `rdata == 0`」⇒ ⇒ **也错** —— 实测读回**仍是 0xAB**
   ⇒ ⇒ 根因 = 拉低 `rst_n` 后 **master 的 FSM 仍在等 R 响应**（不受 `rst_n` 影响）⇒ ⇒ 而 slave
   已被复位 ⇒ ⇒ **响应永不到来** ⇒ ⇒ 读事务**根本不完成** ⇒ ⇒ 「读回 0」**不可达** ⇒ ⇒ 已订正为
   断言 ⑥「读事务**不完成** ⇒ `done` 仍 == 0」（可观测且有判别力）。

**变异自证（打在临时产物上 ⇒ 仓内产物零改动）**：M1（`rdata` 恒 0）⇒ **断言 ④ 转红** ·
M2（`done` 恒 0）⇒ **断言 ②③ 同时转红** · 还原复跑 `checks=6 fails=0` ✅

## 2026-11-20 — C++ 侧断言纳入判据守护 + `phase3a` 拆三角色（`axi4-convergence-tail`）

**问题一（判据面）**：`tb_axi_soc_example` 的 **C++ 侧 2 条断言无任何守护** —— ⑥ 读的是
`tb` 列指向的 `.sv`（checks=14），⑨ 只作用于 note 含 `interp-entry` 的行（SoC 行没有）
⇒ ⇒ 实测把 C++ 侧换成输出 `RESULT: checks=0 fails=0` 的 stub ⇒ **exit 0 且护栏无反应**。
蓝图 §7 R13 允许「C++ 侧条数 ≠ `.sv` 侧」是过渡态的必然，但「条数无人守护」不是。

**问题二（结构面）**：`phase3a_complete.cpp`（463 行）是该目录最后一个「设计定义 + `main`
一体」的 example，且结论是**散文**（「Phase 3A AXI-Lite 链路仿真通过」）而非协议行
⇒ ⇒ 护栏无法对账条数 ⇒ ⇒ 与已退役的 `axi4_full_example` 是同一族缺陷（只打印不判定）。

**改了什么**

- **清单新增 `interp_checks` 列**（13 → **14 列** · `.rddf/checks/examples-rtl-sim-targets.tsv`）：
  C++ 侧入口的**等式值**；自发行（`emitter == target`）MUST 登记且 > 0，其余写 `-` 哨兵。
  值：Lite = **113** · SoC = **2**（≠ `.sv` 侧的 14 ⇒ 两侧语义不同，MUST 各自登记）。
- **⑨ 作用域从 note 的自由文本标记迁到列**：`interp-entry` 约定**退役**（自由文本承载
  判据 = 脆弱）；等式改为 `checks == interp_checks`（**不是** `min_checks`）。
- **新增反向判据 ⑨'**：`emitter == target` ⇒ MUST 有 `interp_checks` 且 > 0；非自发行
  写了数字 ⇒ 也红。⇒ ⇒ 未来任何自发射产品单元**自动**纳入守护（不依赖谁记得改护栏）。
- **`phase3a_complete.cpp` 拆三角色**：`phase3a_top.h`（设计定义 · **无 `main`**）+
  `tb_phase3a_example.cpp`（`main` + **2 条断言** + `RESULT: checks=2 fails=0`）·
  原文件删除 · target 改名 `phase3a_complete` → `tb_phase3a_example`。
- 顺带删掉 `phase3a` 输出里的**假陈述**「Verilog generation ready」（本 example 从不产出
  RTL 产物 —— 无 emitter target · 无清单登记行）。

**用户可见变更**

| 项 | 前 | 后 |
|---|---|---|
| `examples/axi4/phase3a_complete.cpp` | 存在（463 行 · 设计 + main 一体） | 拆为 `phase3a_top.h` + `tb_phase3a_example.cpp` |
| example 二进制名 | `phase3a_complete` | `tb_phase3a_example` |
| CTest 条目 `spinalhdl_axi4_phase3a` | 存在 | **存在（名字不变 ⇒ 非缩面）** |
| `phase3a` 的结论输出 | 散文「…链路仿真通过」 | `RESULT: checks=2 fails=0`（协议行） |
| `phase3a --bogus` | exit 0 | **exit 2**（默认模式不接受参数） |
| SoC 的 C++ 侧断言（2 条） | **无守护**（全删也绿） | 由 ⑨ 按 `interp_checks=2` 对账 |

**判据面自证**（`--self-test` 19 → **21 条**投毒）：新增 ⑨'（自发行缺登记 ⇒ 红）与
⑨ 对 SoC 行的等式（登记值改 ≠ 2 ⇒ 红）各一条投毒，**各自转红**；2 条必绿用例仍绿。

锚点：`CTEST_TOTAL` / non-perf / `tests/test_*.cpp` / `EXAMPLES_TEST_SOURCES` /
`EXPECTED_EXEMPT_PORTS`（1384 · 豁免表 7 行仅路径改名）**全部不变**。

## 2026-11-19 — 发射入口的模式标记与 `--out` 校验下沉库层（修 Lite 侧 fail-open）

**问题**：`examples/axi4/` 的两个 tb（`tb_axi4_lite_example` · `tb_axi_soc_example`）**各写一份**
「剥离模式标记 + 校验 `--out` + 调 `run_emit_cli`」的逻辑 ⇒ ⇒ 两份拷贝必然漂移 —— 实测：
SoC 侧 2026-11-18 查实并修了「`--out --emit-hygiene` 会把 flag 当目录名」，**Lite 侧漏修**
⇒ ⇒ `tb_axi4_lite_example --emit-rtl --out --emit-hygiene` **exit 0 且静默建出名为
`--emit-hygiene` 的目录**（fail-open）。

**改了什么**

- **新增公开 API**（`include/rtl_emit/cli.h` + `src/rtl_emit/cli.cpp`）：
  - `rtl_emit::strip_mode_flags(args, flags, hit)` —— 位置无关地剥离模式标记，
    其余 argv **原序保留**
  - `rtl_emit::require_out_dir(args, out_dir, error)` —— 取 `--out` 并**校验形态**
    （必须出现 / 必须紧跟取值 / 取值不得为空、不得以 `-` 开头）⇒ 失败 ⇒ 调用方退出码 **2**
- 两个 tb 改用这两个共用实现 ⇒ 重复代码消除 ⇒ **Lite 侧的 fail-open 同时修复**
- `tb_axi_soc_example` 的 `make_inputs` **补齐根的全部 8 个 `ch_in`**（原缺 `uart_rx` ·
  `timer_irq`）⇒ 「双形态逐拍等价」的输入面不再缺项；`make_stimulus` 同步补 2 列
- `phase3a_complete.cpp`：删掉**假动作**输出（打印「Generating Verilog…」但 `toVerilog`
  早已被注释 ⇒ 实际什么都没生成 ⇒ 会向用户谎报）
- `examples/axi4/AGENTS.md`：PATTERNS 首句订正为三角色分离（原句对无 `main` 的
  `*_top.h` 不成立）

**用户可见变更**

| 项 | 前 | 后 |
|---|---|---|
| `tb_axi4_lite_example --emit-rtl --out --emit-hygiene` | **exit 0** + 建出 `--emit-hygiene/` | **exit 2** + 零落盘 |
| `tb_axi_soc_example --check-products --out --emit-hygiene` | exit 2 | exit 2（不变） |
| Lite 的 RTL 产物 | `top.v` / `manifest.json` | **逐字节相同**（sha256 `20e0f88e…` / `cf7b40f6…`） |
| SoC 的 `CHECK 1..4`（产品级自检） | PASS | PASS（输入面补齐后仍 PASS） |

**新增测试**：`tests/test_emit_cli_modes.cpp`（9 用例 / 22 断言）—— 把「`--out` 的取值不能是
另一个 flag」钉成必红判据。锚点回填：`CTEST_TOTAL` 269 → **270** · non-perf 258 → **259** ·
`tests/test_*.cpp` 159 → **160**。

## 2026-11-18 — `axi4_full_example` 退役（显式缩面 · `example-file-boundary-2026-11` AC27）

**为什么**：它与 `tests/test_axi4_full_burst.cpp` 是**同一行为的两个载体**（一个是可运行的
example 程序，一个是 dual-backend 测试）⇒ ⇒ 保留 example 只是多养一个入口。

**删了什么**（一次删干净，避免半截状态）：`examples/axi4/axi4_full_example.cpp` 退役；
连带 target · CTest 条目 `spinalhdl_axi4_full` · `CH_TRACKED_EXAMPLE_TARGETS`（**28 → 27**）·
`install(TARGETS)` 该项；`.rddf/checks/examples-rtl-sim-exclusions.tsv` 的该行；
`.rddf/checks/port-naming-exempt.tsv` 的 **31 行**（`EXPECTED_EXEMPT_PORTS` **1415 → 1384**）。

**这是显式登记的缩面**（不是静默消失）。

**一行摘要**：`examples/axi4/axi4_full_example.cpp` 退役（`example-file-boundary-2026-11` AC27）⇒ 行为覆盖迁至 `tests/test_axi4_full_burst.cpp`（dual-backend · 仍绿 · 实测 15.26 s）。

## 2026-11-18 — e2e 用例名随 DUT 改名（`example-file-boundary-2026-11` B1 · AC5）

`tests/test_axi_lite_verilator_e2e.cpp` 的 DUT 由 example 专属的 `AxiLiteTop` 换成库组件
`AxiLiteSlave<32, 32, 4>` ⇒ ⇒ 用例名 MUST 同步 —— 否则名字里留着一个**已退役的类**，
后人按名检索会指向不存在的 DUT。

| 项 | 前 | 后 |
|---|---|---|
| TEST_CASE 名 | `AXI4-Lite VerilatorBackend E2E AxiLiteTopPortAccessSnapshot` | `AXI4-Lite VerilatorBackend E2E AxiLiteSlavePortAccessSnapshot` |

断言数 14 → 14（零变化）· 无 SECTION 增删 · 路径不变 · 该用例仍绿
（`ctest -R test_axi_lite_verilator_e2e` ⇒ Passed）。

## 2026-11-18 — 产物级自检的规则下沉到库层（`ch::rtl_emit` · F2）

**问题**：SoC 的产品级自检（4 项判据 + 编排）此前住在 example 里 ⇒ ⇒ 判定逻辑是
**产品无关**的（module 集相等 / 端口面 / lint 退出码 / 双形态逐拍等价）⇒ ⇒ 想给第二个
产品复用就只能**复制一份**（那正是本 change 立项要消的「同一语义多个物理载体」）。

**改了什么**

- **新增公开 API**（`include/rtl_emit/product_check.h` + `src/rtl_emit/product_check.cpp`）：
  - `product_modules(verilog_path)` —— 产物里的 module 名集合
  - `tamper_hier(...)` —— 变异自证（把某条被观测信号的顶层连接取反 ⇒ 产出篡改副本）
  - `ProductCheckSpec` / `ProductCheckHooks` / `check_products(...)` —— 4 项判据 + **编排**
    （顺序 / `failures` 累加 / `eq/` 产出 / 结论行 / 退出码 0-1 全在库层）
- **值留在 example**：期望 module 集 · 期望顶层端口数 · 激励面 · 观测面 · 变异目标 ·
  `--top-module` 名 · 最小变化观测量阈值 ⇒ ⇒ 库层**不内置**字面根名 / 阈值
- **owner 基准由建出来的根组件派生**（`Component::name()`）⇒ ⇒ 根名可配，不再依赖
  「三处字面量恰好一致」；两份设计头新增 `kSocRootName` / `kLiteRootName` 作**单一真相源**
- `examples/axi4/tb_axi_soc_example.cpp` 543 → **388 行**（规则移出；只剩值 + 三模式 main）

**顺带**（同一批自查修掉的 fail-open · 上游已单独提交）

| 缺陷 | 前 | 后 |
|---|---|---|
| `--check-products` 的 lint 发现 | CWD 相对路径 ⇒ 从 `build/` 跑静默 `SKIP` | `CPPHDL_SOURCE_DIR` 搜索根 + **SKIP 升格 FAIL** |
| lint 执行 | `std::system` 拼串（空格路径断裂 + 注入面） | `fork` + `execl`（argv 式） |
| `--out` 取值 | `--out --emit-hygiene` 把 flag 当目录名 | 取值以 `-` 开头 ⇒ exit 2 |

**用户可见变更 / 新增公开面**

| 项 | 说明 |
|---|---|
| 新公开头 | `include/rtl_emit/product_check.h`（登记于 `.rddf/checks/example-only-headers.tsv` 豁免通道） |
| 新库 TU | `src/rtl_emit/product_check.cpp` |
| `check_products` 的退出码 | **只 0/1**（用法错误按失败计 ⇒ fail-closed）⇒ ⇒ 与搬移前**一致** |
| 产物 | **逐字节不变**（`top.v` / `manifest.json` / `eq/{flat,hier}/top.v` 四件 sha256 相同 ⇒ `.rddf/checks/product-check-equivalence-2026-11-18.md`） |

## 2026-11-18 — AXI4-SoC 示例去 main + 产品自检内联（`example-file-boundary-2026-11` B6）

**问题**：SoC 侧有**两个**可执行入口（`axi_soc_example` 的激励 + `emit_rtl_soc` 的发射/自检），
而设计定义（`AxiSocTop` + `AxiLiteTestMaster`）早已独立在 `axi_soc_example_top.h` ⇒ ⇒ 文件名里
的 `axi_soc_example` 名不副实（它只剩 `main`），且 `toVerilog("axi_soc_example.v")` 把产物写进 CWD。

**改了什么**

- `axi_soc_example.cpp` → **`tb_axi_soc_example.cpp`**（与 `tb_axi4_lite_example.cpp` 对称）
- **三模式**：默认（激励 + `RESULT: checks=2 fails=0`）· `--emit-rtl <dir>`（恒 `--hierarchical`）·
  `--check-products`（= 发射 + 4 项自检）
- **删除** `examples/axi4/emit_rtl_soc.cpp` + target `emit_rtl_axi_soc`；其 checker 内联进 tb
  （用户裁决：值侧 MUST 留在 example ⇒ 不新设中间头）
- 删 `toVerilog("axi_soc_example.v")` ⇒ **零 CWD 落盘**
- 清单 tsv 的 SoC 行 → `tb_axi_soc_example` + `emitter_args=--emit-rtl`（④″ 自检改指同一二进制）
- CMake target 改名（**CTest 条目名 `spinalhdl_axi4_soc` 不变**）· tracked · install ·
  `input-port-required-registry.md` · 两份 `AGENTS.md`

**顺带修掉三个 fail-open 缺陷**（自查发现，均在同一 checker 里）

| 缺陷 | 前 | 后 |
|---|---|---|
| `find_verilator` 用 CWD 相对路径 | 从 `build/` 跑 ⇒ CHECK 3 静默 `SKIP`（lint **从未**被验） | 改用 `CPPHDL_SOURCE_DIR` 搜索根；**SKIP 升格为 FAIL** |
| `std::system` 拼串跑 lint | 路径含空格 ⇒ 静默 `SKIP`；且**有 shell 注入面** | 改 `fork` + `execl`（argv 式，参数与注入面同时消除） |
| `--out` 取下一个 argv | `--out --emit-hygiene` ⇒ 把 `--emit-hygiene` 当目录名 | 取值以 `-` 开头 ⇒ **exit 2** |

**用户可见变更**

| 项 | 前 | 后 |
|---|---|---|
| SoC 的 RTL 发射 | `build/examples/axi4/emit_rtl_axi_soc --hierarchical --out <dir>` | `build/examples/axi4/tb_axi_soc_example --emit-rtl --out <dir>` |
| SoC 的产品级自检 | `emit_rtl_axi_soc ... --check-products` | `tb_axi_soc_example --check-products --out <dir>` |
| SoC 激励 | `build/examples/axi4/axi_soc_example`（无结论行） | `build/examples/axi4/tb_axi_soc_example`（`RESULT: checks=2 fails=0`） |
| 产物 | — | 形态与 module 集**逐字不变**（5 module · 顶层 14 端口） |

## 2026-11-18 — RTL 发射入口并入 example（`--emit-rtl` 模式 · `emit-cli-into-example`）

**问题**：AXI4-Lite 的 RTL 发射入口是**独立可执行文件** `emit_rtl_axi4_lite`（32 行薄壳：注册
`AxiLiteTop` + 调库层 `run_emit_cli`）⇒ ⇒ 与 `tb_axi4_lite_example` **重复同一个 factory**
⇒ ⇒ 同一设计有两个 main、两份根组件声明。

**改了什么**

- **`tb_axi4_lite_example` 新增 `--emit-rtl` 模式**：位置无关剥离该标记 ⇒ 其余 argv 透传库层
  `run_emit_cli`；默认模式**仍是验证模式**（`.verif` 驱动）
- **删除** `examples/axi4/emit_rtl.cpp` + target `emit_rtl_axi4_lite`
- **fail-closed 两条**：默认模式收到任何未知参数 ⇒ **exit 2**；`--emit-rtl` 缺 `--out` ⇒ **exit 2**
  （**不回落**库层默认 `rtl_out/` ⇒ 不写 CWD）
- 清单 `.rddf/checks/examples-rtl-sim-targets.tsv` **增 `emitter_args` 列**（12 → 13 列；无参写 `-` 哨兵）
  ⇒ 该行 `emitter` → `tb_axi4_lite_example`、`emitter_args` → `--emit-rtl`
- 护栏 `verify_examples_rtl_sim.sh`：按**列名**取 `emitter_args`（③ 发射 / ⑥' 重新生成两处）；
  新增**字段数守恒**具名 fail（空单元 ⇒ 提示写 `-`）；`--self-test` 增至 **17 条投毒**
  （新增「`emitter_args` 置 `-` ⇒ ③ 必红」）+ **1 条必绿用例**（中间插列 ⇒ 按名字取列 ⇒ 仍绿）

**用户可见变更**

| 项 | 前 | 后 |
|---|---|---|
| AXI4-Lite 的 RTL 发射 | `build/examples/axi4/emit_rtl_axi4_lite --out <dir>` | `build/examples/axi4/tb_axi4_lite_example --emit-rtl --out <dir>` |
| 产物 | — | **逐字节相同**（`top.v` · `manifest.json` · TB 三者的 sha256 对照通过） |

## 2026-11-18 — AXI4-Lite 示例改由 `.verif` 驱动（C++ 侧）

**问题**：`.verif` 已经是**唯一语义来源**，但接着它的只有**渲染式 TB**；C++ 侧
（`examples/axi4/axi4_lite_example.cpp`）仍是**手写激励 + 只打印不判定**的检查器
（`Axi4TimingChecker` 的 5 个 `check_*` 只 `std::cout`，**不影响退出码**）⇒ ⇒
「同一语义两个载体」在 C++ 侧仍然存在，且 `VcdRecorder` 喂的是**硬编码常量**
（不是真实波形）⇒ ⇒ 产物**误导**。

**改了什么**

- **`examples/axi4/axi4_lite_example.cpp` → `tb_axi4_lite_example.cpp`**（CMake target 同步；
  **CTest 条目名 `spinalhdl_axi4_lite` 不变**）⇒ ⇒ 219 行 → **88 行**，**零**激励/断言/期望值
- `main()` 改为 **`parse_verif` → `build_rtl_manifest` → `run_verif`** ⇒ ⇒ C++ 侧与渲染式 TB
  由**同一份 IR** 驱动 ⇒ ⇒ 断言不可能漂移
- 验证源路径 = **编译期绝对路径**（`CPPHDL_SOURCE_DIR`）⇒ ⇒ 与 CWD 无关
  （仓根 / `build/` / `build/tests/` / 任意空目录 ⇒ 均 exit 0）
- **fail-closed**：验证源打不开 / 解析失败 / manifest 有诊断 / 执行失败 ⇒ **非零退出**且
  **不打印** `RESULT:` 行；失败明细（`CHECK <id> FAIL`）进 stderr
- **自洽退出码**：仅当 `res.ok && res.checks == verif_check_count(prog) && res.fails == 0` 才 `return 0`
- **删除** `VcdRecorder`（喂常量的假波形）· `Axi4TimingChecker`（只打印）· 手写激励 ·
  `toVerilog("axi4_lite_example.v")`（在 CWD 落游离产物）

**用户可见变更**

| 项 | 前 | 后 |
|---|---|---|
| 二进制名 | `axi4_lite_example` | **`tb_axi4_lite_example`**（CTest 名不变） |
| 产物 | `axi4_lite_waveform.vcd`（**手喂常量 · 非忠实波形**）+ `axi4_lite_example.v`（CWD 落盘） | **零落盘**（波形改由渲染式 TB 的 `VcdRecorder`/TB 生成；Verilog 由 `emit_rtl_axi4_lite` 发射） |
| 结论行 | 无（只打印散文） | `RESULT: checks=113 fails=0`（十进制 · 单行 · 无 ANSI） |
| 断言可判定性 | 5 个检查**只打印** ⇒ 不影响退出码 | **113 条**全部参与退出码 |

**护栏**：`scripts/verify_examples_rtl_sim.sh` 新增 **⑨**（C++ 侧条数 == 清单 `min_checks` ·
按 note 列 `interp-entry` 约定取作用域 · 找不到二进制 ⇒ fail-closed）+ `source` 列路径存在性判据；
`--self-test` 增至 **16 条投毒**（新增「解释器条数不等」与「source 路径不存在」两条，各自必红）。

## 2026-11-17 — **RTL 验证的单一真相源**（`.verif` → IR → 两个执行后端 · AXI4 示例改造）

**问题**：examples 的 RTL 验证有**三组重复载体** —— ① 期望值（`.rddf/checks/*.md` 的散文 +
TB 里的字面量 + C++ 侧的断言各写一遍）② 端口身份（TB 里**手写**端口映射表，靠「按编号反推」
才能连对 —— 实测踩过「awaddr/wdata 互换 ⇒ 15 条判据误红」）③ 发射实现（`emit_rtl_soc.cpp`
自解析命令行 + 自发射，**483 行**里约 380 行重复库层已有能力）⇒ ⇒ 需求「希望项目代码不要
过于碎片化」直接落在这三组上。

**改了什么**

- **新增「验证源」`.verif`**（`include/rtl_emit/verif.h` + `src/rtl_emit/verif_{scan,run,render}.cpp`）：
  扫描器在**生成期**把表达式全部求值成字面量（`REPEAT` 在扫描期展开、`_i` 在扫描期绑定）
  ⇒ ⇒ **IR 里没有表达式** ⇒ ⇒ 两个执行后端**不需要各自解释表达式** ⇒ ⇒ **语义真的只有一份**
  （① IR 解释器驱动 `Simulator`；② Verilog 渲染器生成 `.sv` TB）
  ⚠️ 时序锚点：驱动在「沿后采样窗」内（不消耗拍），观测在「最近一次结算之后」
  ⇒ ⇒ Verilog 侧统一 `@(posedge clk); #1;`（**禁 `#0`**）
  ⇒ ⇒ `SET → EXPECT` **MUST** 先 `SETTLE`（扫描器**静态**强制 ⇒ 不靠人记得写）
  ⇒ ⇒ 集合外构造 / 文法外 token / 嵌套 `REPEAT` 一律 **fail-closed**
- `src/rtl_emit/tb_stub.cpp`：拆出 `write_tb_prelude` / `write_tb_reset_pulse` /
  `write_tb_epilogue` ⇒ ⇒ **骨架**与**渲染式 TB** 共用同一份端口连线/时钟/复位实现
  （防两份副本漂移）
- `src/rtl_emit/cli.cpp`：`--emit-tb-verif <path.verif>` / `--tb-verif-out <path>`，
  并打印机器可读的 `VERIF-CHECKS: <N>`（供护栏做三方等式）
- `include/simulator.h` + `src/simulator.cpp`：新增 `set_value_by_node_id()` /
  `get_value_by_node_id()`（**未命中返回 `false`/`nullopt`，绝不静默读 0**）
- **顶层组件上提**（消「双权威」）：`AxiLiteTop` → `include/axi4/axi4_lite_example_top.h`，
  `AxiLiteTestMaster` / `AxiSocTop` → `include/axi4/axi_soc_example_top.h`
  ⇒ ⇒ `tests/test_axi_lite_verilator_e2e.cpp` 不再**复制**一份 `AxiLiteTop`
- **32 个顶层端口具名**（ADR-075）：`AxiLiteTop` 20 个 + `AxiSocTop` 12 个
  ⇒ ⇒ 产物端口名由 `top_unnamed_input_N` 改为 `top_<C++ 成员名>`
  ⇒ ⇒ 豁免表 `1447 → 1415`（`AxiLiteTestMaster` 的 23 个成层端口**不在**本次范围）
- `examples/axi4/tb_axi4_lite_example.sv` 改为**由 `.verif` 生成**（`generated=yes`）
  ⇒ ⇒ 端口连线来自 `manifest.port_table[]` ⇒ ⇒ **无手写映射表**
  ⇒ ⇒ 实测 `RESULT: checks=113 fails=0`（Verilator）· 解释器侧 `fails=0`（同一份 IR）
- `examples/axi4/emit_rtl_soc.cpp` 483 → 330 行：公共发射路径（解析 / 发射 / manifest /
  诊断 fail-closed）全部走**库层 `run_emit_cli`**（`--hierarchical`）⇒ ⇒ 只保留
  **SoC 专属**的 `--check-products`（冻结 module 集 / 顶层端口数 / 双形态逐拍等价 —— 都是
  **设计专属**期望值，按 ADR-074「不覆盖」条不下沉库层）
- `scripts/verify_examples_rtl_sim.sh`：① 列号**从 `#COLUMNS:` 推导**（禁位置绑定 ——
  中间插列会静默错列）② ⑥ 由**下界**改为**等式**（`.md` 登记 == `.verif` 静态展开 == 运行期）
  ③ 新增 **⑥'** 生成物新鲜度（逐字节重生成比对）**⑥''** 期望源锚点 ↔ 验证源 scenario
  **双向 1:1** **⑥‴** TB 无 TODO 残留 + 每行断言带 `check_id` **⑥⁗** 生成覆盖度三方一致
  **⑧''** TB 可判定为生成物 ④ `--self-test` 8 → **13 条投毒**（新增 5 条各自必红）
- `.rddf/checks/examples-rtl-sim-targets.tsv`：新增 `generated` 列；`min_checks` 语义改为等式
- `.rddf/checks/examples-rtl-expectations.md`：`axi4-lite-example` 段新增 7 个
  `#### scenario:` 锚点（与 `.verif` 的 `@scenario` 双向 1:1）
- 新增 `tests/test_verif_single_source.cpp`（7 用例 / 33 断言）：扫描器 fail-closed +
  `REPEAT` 扫描期展开 + **IR 解释器**跑通仓内真实验证源

**⚠️ 已知未覆盖**：`examples/axi4/tb_axi_soc_example.sv` 仍是**手写** TB
（清单 `generated=no` + 具体原因）—— UART 8N1 帧解码需要「逐拍采样成数组 + 首个低电平
定位 + 变长位偏移」，**封闭原语集无此构造** ⇒ ⇒ 不硬塞进 DSL。

## 2026-11-17 — **RV32I 核的 RTL 交付 + Verilator 仿真环境**（一条命令跑通）

**背景**：层次化进产品面后，用户侧仍缺两件东西 —— ① 一份说清「RV32I 核的 RTL 产物长什么样」
的交付文档；② 一个**开箱即用**的 Verilator 仿真环境（此前只在护栏内部跑，用户要用得自己拼命令）。

**改了什么**
- 新增 `scripts/rv32i_rtl_sim_env.sh`：**一条命令**跑通四种形态的完整流程 ——
  生成产物 → `verilator --lint-only` → `verilator --binary --timing` 编译独立 TB → 运行 ⇒
  打印 `RESULT: checks=<N> fails=0` ⇒ ⇒ **扁平与层次化跑出同一组判据**
  ⇒ ⇒ 「层次化 ≡ 扁平」在**产品路径**上得到证据（不只在用例路径上）
  ⇒ ⇒ fail-closed（缺 verilator / 缺发射器 / lint 非零 / 编译失败 / 断言失败 ⇒ 非零退出）
- 新增 `docs/developer_guide/rv32i-rtl-delivery-and-sim.md`：交付物清单 · **产物形态**
  （顶层端口面 **34** = 框架 2 + 根自有 32 · 模块树 **8** module · 存储映射
  I-TCM `0x8000_0000` / D-TCM `0x8001_0000`（各 16 KB · PC 复位 = I-TCM base）·
  时钟复位（**真时钟源 = `default_clock`**）· ISA 边界（**未覆盖 = 特权级**））
  + **Verilator 仿真环境**（一条命令 / 手动四步 / 判据与期望源 / **8 条常见陷阱**）
  + 「换成你自己的设计」的模板
- `examples/riscv-mini/src/rv32i_pipeline_top.h`：**订正一处过时注释**（事实订正 · 无行为变更）
  —— 原写「固件 MUST 无数据冒险：lib 版 `HazardUnit` 不接数据前推」⇒ ⇒ **与实际装配矛盾**
  （ADR-047 D2 的前推 mux = `chlib::ForwardingMux<32>`，装在**流水线装配层**
  `rv32i_pipeline.h::build_forwarding_paths()` ⇒ ⇒ 层次化产物里可见 `ForwardingMux_32_` module）
  ⇒ ⇒ 订正为「前推已接入」并指向 ADR-047 §5.5 + ADR-069（**不复制**其判据）
- 根 `AGENTS.md`：「WHERE TO LOOK」+「COMMANDS」各加一行（交付入口 + 一条命令）

**实测**：脚本 PASS —— 扁平（1 module · 34 端口 · lint 干净 · `checks=24 fails=0`）·
层次化（8 module · 34 端口 · lint 干净 · `checks=24 fails=0`）· 产物端口表 ==
manifest `port_table[]`（34 == 34）。

## 2026-11-17 — **层次化 RTL 进入产品面**（诉求 ⑤「层次化输出示例」：CLI + 独立 TB + 覆盖登记）

**问题**：层次化输出此前只有**测试级**载体（`examples/riscv-mini/tests/test_rv32i_hier_rtl.cpp`
在用例内发射）⇒ ⇒ **用户拿不到产物**（无 CLI ⇒ 拿不到 `.v`；且没有独立 Verilog TB 能跑它）
⇒ ⇒ 「层次化输出」停在**特性**层，没到**产品**层 ⇒ ⇒ 与「RTL 生成能力产品化」的目标不符。

**改了什么**
- `include/rtl_emit/cli.h` + `src/rtl_emit/cli.cpp`：`CliOptions` 新增 `hierarchical`
  （`--hierarchical`）⇒ ⇒ **通用生成器**层面支持层次化：用户在自己的可执行文件里登记设计
  + 加一个 flag 即可（复用「库 + 薄 CLI」形态）；层次化分支走 `toVerilogHierarchical`
  并**显式判 `ok`**（该入口不抛异常 ⇒ 不加判定会让发射失败静默通过）
- 新增 `examples/riscv-mini/emit_rtl_hier.cpp` + CMake target `emit_rtl_rv32i_soc_hier`
  ⇒ ⇒ RV32I SoC 的**层次化形态**产品级交付（薄壳：强制 `--hierarchical`）
- **复用同一份独立 TB**（`examples/riscv-mini/tb_rv32i_soc.sv`）⇒ ⇒ 依据 = 两种形态的
  **顶层端口表同源**（都走 `make_flat_module_view` ⇒ 34 端口逐字逐序相同）⇒ ⇒ 同一套判据
  跑两种形态 ⇒ 实测 `RESULT: checks=24 fails=0`（与扁平**完全相同**）
- `.rddf/checks/examples-rtl-sim-targets.tsv`：新增 `kind=hier` 桶（`REGISTERED_HIER: 1`）
  ⇒ ⇒ 独立计数（不改「首批 10」的口径）
- `scripts/verify_examples_rtl_sim.sh`：⓪ 升为**三桶守恒**；新增 **⑧' 层次化形态判据**
  （`kind=hier` 的产物 MUST 含 ≥ 2 个 module 且含根 `top`）⇒ ⇒ ⚠️ ⑧（端口表）对
  「层次化静默退化成扁平」**结构性失明**（退化后端口表照样相同）⇒ ⇒ ⑧' 补上该盲区
- `scripts/lib/flat_rtl_emit_check.py`：新增 `--modules` / `--strip-modules-except`
  （后者供投毒用；裁剪复用**同一个**分析器 ⇒ 不自研第二套解析器）
- **判别力自证**：`--self-test` 新增投毒 8（`RTL_SIM_TAMPER=flatten_hier`：把层次化产物
  压成单 module）⇒ ⇒ ⑧' MUST 转红
- 文档：`.rddf/checks/examples-rtl-expectations.md` 补「层次化形态」段（**不新增期望源** ——
  层次化不引入新语义，另写一份必然漂移）· `examples/riscv-mini/AGENTS.md` 结构表 ·
  `docs/usage_guide/05-component-design.md` 新增 **§5.3.0**（两个入口 + TB 复用前提）

**实测**：层次化产物 = **单文件 8 module**（7 stage + `top`）· 顶层端口 **34**（6 in + 28 out）·
`verilator --lint-only --top-module top` **exit 0** · 同一 TB `checks=24 fails=0` ·
主护栏 `verify_examples_rtl_sim` **PASS（12 项）** · 守恒护栏 `28 == 10 ∪ 18` 不变。

## 2026-11-17 — **vendored Verilator 的 PCH 工具链修复**（自动 split 的模型可编译了）

**问题**：vendored Verilator 走 **CMake** 构建 ⇒ 上游 autoconf 的 PCH 选项探测
（`third_party/verilator/configure.ac:704-712`）**没有变量源** ⇒ ⇒ 生成的 `verilated.mk`
里 `CFG_CXXFLAGS_PCH_I` 为**空** ⇒ ⇒ GCC 拿不到 PCH ⇒ ⇒ **任何「自动 output-split」的模型
（> 20000 statements）C++ 构建必失败**（实测：`g++: error: Vtb_xxx__pch.h.fast:
linker input file not found`）⇒ ⇒ 影响 `verify_examples_rtl_sim.sh`（rv32i_soc）与
`VerilatorBackend` 的 `verilator --cc`（**潜伏 CI 红**：模型一大就炸）。

**改了什么**（`verilator-pch-toolchain-fix-2026-11`）
- `CMakeLists.txt`：给 verilator 的 `ExternalProject` **注入** `CFG_CXXFLAGS_PCH_I` /
  `CFG_GCH_IF_CLANG`（镜像上游 autoconf 判定：GCC ⇒ `-include`；Clang ⇒ `-include-pch` + `.gch`）
  ⇒ ⇒ **零 submodule 改动**（升级不丢补丁）
- `scripts/verify_examples_rtl_sim.sh`：**删掉** `--output-split 0` 绕行 ⇒ ⇒ 护栏**真实覆盖**
  「自动 split ⇒ PCH」路径（此前绕行把缺陷藏成了潜伏 CI 红）
- 新增护栏 `scripts/verify_verilator_pch.sh`（构建期指纹 + 强制 split 编译 + **负向对照**：
  安装根**副本**清空 flag ⇒ 必红 ⇒ ⇒ 真实安装产物零风险）+ CI 一步 + 可达性登记表一行
- 文档：`docs/developer_guide/verilator-integration.md` 新增「PCH 与自动 output-split」小节

**实测**：rv32i_soc（3066 行 Verilog · **不加任何 flag** ⇒ 自动 split ⇒ 真的走 PCH）编译 rc=0；
强制 split 的最小设计 rc=0 且产出 `.gch`；flag 清空的副本 ⇒ 必红（`linker input file not found`）。

## 2026-11-17 — **产品面声明 + 四件套判据（护栏新增判据⑧）**（ADR-072 D2 / D3 / D4）

**背景**：「产品化完成」不能只是「有个发射器」—— 一个**产品单元** = manifest + Verilog 产物 +
**独立可跑的 TB** + 覆盖登记（四件套）；且 RV32I 的 ISA 边界与「冻结是终态」这类**运行时可观测**
的限制 MUST 显式声明（不声明 = 误导性宣传）。

**改了什么**（`rtl-productization-surface-2026-11` **T13**）
- `scripts/verify_examples_rtl_sim.sh` 新增**判据⑧**：逐个产品单元断言 manifest 的
  `port_table[]` **==** 该单元产物端口表（**逐字逐序**）⇒ ⇒ 其余三件由 ③ / ⑤⑥ / ⓪ 承担
- **判别力自证是正/负成对**（`--self-test` 投毒 6/7）：篡改 `port_table` 里**一个端口名** ⇒ 必红 ·
  产物里**多插一个下沉端口** ⇒ 必红（「删掉 manifest」只证明会打开文件，不证明区分对错）
- 层次化产物（`kind=soc`）按**根 module 名**取端口表（产物是单文件多 module 且子模块在前）
- `docs/usage_guide/05-component-design.md` 新增 **§5.9 产品面声明**：四件套 · ISA 边界
  （含 `FENCE`=NOP · 非法指令 4 态 · **不含特权级** · **冻结是终态**）· 独立 TB 的收尾骨架

## 2026-11-17 — **CLI 默认交付卫生产物**（`--emit-hygiene` 默认 on）

**改了什么**（`rtl-productization-surface-2026-11` **T12** · ADR-072 **D7**）
- `ch::rtl_emit::CliOptions::emit_hygiene` 默认 `false → true` ⇒ ⇒ `cpphdl-emit-rtl` 不带 flag
  时产出**自身 lint 干净**的产物（无 blanket `lint_off`）
- 逃生口 `--no-emit-hygiene`（与旧产物形态做对照）
- ⚠️ **只改 CLI 层**：库层 `EmitOptions{}` / `verilogwriter(ctx)` **不动**
  ⇒ ⇒ 49 处 ctx-only 调用点产物**逐字节不变**（`STATIC_REQUIRE(ch::EmitOptions{}.emit_hygiene == false)` 仍绿）
- 护栏里的显式 `--emit-hygiene` **保留**（护栏 MUST 显式打卫生产物，不靠默认值）

## 2026-11-17 — **顶层端口面收敛 + 冒泡机制退役**（ADR-072 D1-f / D1-h）

**问题**：manifest 声称的顶层端口表与产物**不自洽**（`is_top_io: true` = 32，产物顶层 = 130
⇒ ⇒ 98 个顶层端口是后代信号）；且「悬空后代输入冒泡成顶层 input」把**漏接线**变成看起来
合法的新顶层端口（编号随 eval-list 序漂移、无语义）。

**改了什么**（`rtl-productization-surface-2026-11` **T15 + T6**）
- **后代 `ch_out` 不再上任何 module 的端口表**（含根）⇒ 一律体内 `logic`
- **悬空后代输入 MUST 被响亮拒绝**：`ok = false` + `error` 点名（端口名 + module + 层级），
  `files` 为空；**根自己**的悬空 `ch_in` 不受影响（它就是顶层 `input`）
  ⇒ ⇒ ⚠️ **迁移指引（下游 MUST 照此改，否则发射直接失败）**：被拒的输入只有**两条**合法出路 ——
  **① 接上驱动源**（父层 `u.io().din <<= io().din;`）或 **② 父层显式 tie-off 常量**
  （`u.io().din <<= ch_bool(false);` / `<<= ch_uint<8>(0_d);`）⇒ ⇒ **tie-off 的输入有 driver**
  ⇒ ⇒ 不触发拒绝（两条各有**机械**判据：`tests/test_hierarchy_verilog.cpp` 的
  「WITH a driver is accepted」与「tied off by the parent is accepted」两条**配对正向**用例）
  ⇒ ⇒ ⚠️ **注意**：`inline` 折叠进根的后代输入**同样被拒**（它不在根自有 `__io` 里 ⇒
  不是合法的顶层 `input`）⇒ ⇒ 旧行为（把它冒泡成顶层 `input`）**不再存在**
- 物理删除：`UndrivenPolicy::Bubble`（只剩 `Reject`）· `HierarchyPortOrigin::Bubbled/InlineFolded` ·
  `PortProvenanceOrigin::Bubbled/InlineFolded` · `BubbledInput` / `bubbled_inputs` ·
  `ensure_bubbled_port` · `dangling` 向量（公开面登记见 `.rddf/checks/public-api-surface.md`）
- 用户可见：**RV32I 分层产物顶层 34 端口**（= `default_clock` / `default_reset` + 4 `ch_in` +
  28 `ch_out`，与 `AGENTS.md` 手推锚一致）；ctx-only 遗留扁平产物**逐字节不变**（130）
- 护栏同步：`flat_rtl_emit_check.py` 的 F3 改为**只消费** manifest 的 `port_table[]`（逐字逐序）·
  `verify_hierarchical_rtl.sh` R6/R8 期望集重算（冒泡靶子换成「同层两兄弟」正向设计）

## 2026-11-17 — **manifest schema v1 → v2**（新增产品端口表 `port_table[]`）

**背景**：v1 只有 `is_top_io`（语义 = 根组件自有 IO）⇒ ⇒ 消费方**无法**据此复原产物端口面。

**改了什么**（`rtl-productization-surface-2026-11` **T4**）
- manifest 新增**顶层数组** `port_table: [{name, dir, width, role}]`（**按产物顺序**）
- `schema` → `rtl-emit-manifest-v2`；`ports[]` **仍是全量**（10 字段不变）
- **端口面的唯一权威 = `port_table[]`**；`is_top_io` 退为**消费方筛选信号**
- 产物卫生头里的元数据同步为 `schema=rtl-emit-manifest-v2`

## 2026-11-17 — **新增带根扁平发射入口**（`toVerilog(path, Component&, EmitOptions)`）

**背景**：RTL 生成能力的**产品面**要求「**manifest 声称的端口表 == 产物的端口表**」。
既有扁平入口只收 `ctx` ⇒ ⇒ 拿不到「根组件」⇒ ⇒ 端口面无法收敛到**根组件自有 `__io`**
（`VerilatorBackend` 同理拿不到 root ⇒ ⇒ 它继续走遗留路径）。

**改了什么**（`rtl-productization-surface-2026-11` **T2**）
- 新增 `ch::toVerilog(const std::string &, Component &, const EmitOptions &)` —— **带根**产品入口
- 新增 `ch::make_flat_module_view(Component &, bool)` —— 端口选择 = `default_clock` →
  `default_reset` → **根组件自有**（`node->get_parent() == &root`）的无 driver `ch_in` / 全部 `ch_out`
- `verilogwriter` 新增带根构造

⚠️ **只增不改**：既有 `toVerilog(path, ctx)` / `toVerilog(path, ctx, options)` 的**签名与产物
逐字节不变**（49 处调用点零改动）⇒ ⇒ 下游既有代码**零动作**；需要产品端口面时改用带根重载。

**实测（手推对照）**：`root{a,q} ⊃ leaf{din,dout}` ⇒ ⇒ ctx-only = **5** 端口
（含后代 `top_leaf_dout`）· 带根 = **4** 端口（`default_clock` / `default_reset` / `top_a` /
`top_q`）⇒ ⇒ 后代 `ch_out` 降为体内 `logic`；`verilator --lint-only --top-module top` **exit 0**。

## 2026-11-16 — **ADR-073 v1.1 dated 订正**（SoC 产物形态 · **无行为变更 · 纯文档**）

**背景**：`soc-rtl-product-and-tb-2026-11` 立项时跑的 **metis 第 1 轮** gap 分析查出 ADR-073
（Accepted · 2026-11-16）有 **7 处事实错误** ⇒ ⇒ 按 dated 订正落库（**D1–D10 方向不变**）。

**订正 7 处**（全部由工具复算确认，见 `.rddf/reviews/ADR-073-...md` §6）：
- 未驱动后代 `ch_in` **30 → 31**（补 `master.rst_n` —— `describe()` 不消费但**节点仍创建**）
- 扁平顶层端口 **44 → 45**（+ 31 冒泡）
- D1「`AxiSocTop` = 5」⇒「根 module 名 `top`（默认，非组件类名）+ 4 成层 = 5」
- `done` 是**粘滞**（非单拍脉冲）⇒ ⇒ J-4 观测集去 `done`、激励 **≥4 → ≥10 拍**
- s1↔uart 接线 **「19 根双向」→「19 根单向」**（11 正 + 8 反）
- V6 删「未映射地址 DECERR」场景（顶层无 `bresp` + matrix 默认映射铺满 ⇒ ⇒ 不可观测）
- `master.rst_n` 补驱动 **无行为影响**（`describe()` 零引用 `rst_n`）

**oracle 第 2 轮复核**：7 处事实全成立；抓到 1 BLOCKER（§5 V5 漏改）+ 2 SHOULD_FIX
⇒ ⇒ 全闭。**不改决策、不需取代流程**。⚠️ 连带：`proposal.md` 的「用户可见变更」第 3 条 +
**G-2 门**把 `master.rst_n` 记为「行为变更」⇒ ⇒ 该登记不成立 ⇒ ⇒ 移交 planner 改「无行为影响的发射前置」+ 关 G-2。

## 2026-11-16 — **`funct3` / `funct7` 保留组合检测**（B4 · G1 扩展 · ADR-071 v1.2）

**问题**：G1 当初**只覆盖 opcode** ⇒ ⇒ `funct3` / `funct7` 的 **ISA 保留组合**
（例：`LOAD` 的 `funct3=011` · `OP` 的 `funct7=0x40`）被当作**合法指令执行**
⇒ ⇒ 与「未知 opcode 被当 NOP」**同型**的静默缺陷族。

**改了什么**：
- `riscv::Rv32iDecoder` 新增 **保留组合判定** + 输出 `illegal_reason`（2 bit · **4 态**：
  `0` 合法 · `1` 已知未实现 · `2` 未知 opcode · **`3` 保留组合**）⇒ ⇒ **单一来源**
- `riscv::IdStage` **改为转发**库组件的 reason（删 4 个本地局部量 ⇒ 零死代码）
- 判定表 = **ADR-071 §2.6.1**（`OP`/`OP-IMM`/`LOAD`/`STORE`/`BRANCH`/`JALR` 逐个写死合法组合）

⚠️ **关键事实（手推 · 实测踩过）**：**U 型（`LUI`/`AUIPC`）与 J 型（`JAL`）没有 `funct3` 字段**
—— bits 14:12 是**立即数位** ⇒ ⇒ 把「`funct3` MUST 为 `000`」套到它们上会**误判合法指令**
（`lui x10, 0x1` = `0x00001537` 的 bits 14:12 = `001` = 合法）⇒ ⇒ 已写成**负向对照** Scenario。

**行为影响（实测）**：仓内固件**未踩**保留组合 ⇒ ⇒ **金标零变化**
（`ctest -LE perf` 255/255 · 独立 Verilog TB `checks=24 fails=0`）。
⚠️ 产物顶层端口 **+1**（decoder 的新输出占了 `_0` ⇒ ⇒ 既有 8 根 decoder 端口**整体 +1 位移**）。

**顺带挖出两处缺陷（都靠「手推」抓到）**：
1. 差分表的 `sub x1,x2,x3` 编码 `0x803100B3` 的 **`funct7=0x40`** = **保留组合**
   ⇒ ⇒ 已修正为 `0x403100B3`（`funct7=0x20`）
2. 我为新用例手推的编码**第一版写错**（`0x00001083` = `funct3=001` = `LH` ⇒ 合法）
   ⇒ ⇒ 被**同一条判据当场抓住** ⇒ ⇒ 修正为 `0x00003083`

spec：`rv32i-isa-semantics` 的 reason 码 **3 态 → 4 态** + 判定表指针 + **单一来源**规则。

## 2026-11-16 — **陷阱入口的 `mcause` 现在真的写得进去**（B4 · G5）

**缺陷（2026-11-10 登记 · 今天修）**：`riscv::ExceptionUnit` 的 CSR 写是
`write_mepc = write_mcause = do_trap_entry` ⇒ ⇒ 两者**同时**为真 ⇒ ⇒
`csr_addr = select(write_mepc, CSR_MEPC, …)` ⇒ ⇒ **MEPC 恒命中** ⇒ ⇒
**`mcause` 分支永远不可达** ⇒ ⇒ **`mcause` 永远写不到**（读回恒 0）。
⚠️ 它能活到今天的原因：**既有测试只查写口的值**，**从不查 `CsrBank` 的读回** ⇒ 零信号。

**修法 = 跨两拍**（`CsrBank` **零改动** ⇒ 不触发「新增输入端口 ⇒ 全部实例化点 MUST 显式驱动」）：
- **拍 1** 写 `mepc`（= `current_pc`）并**锁存** `exception_cause`
- **拍 2** 写 `mcause`（= **锁存的**原因码 ⇒ ⇒ 与 `mepc` **同配对**）

**判别力**：新增**复合 DUT**（`ExceptionUnit` → `CsrBank`）⇒ ⇒ 断言落在 **`CsrBank` 的读回值**上
（写口是组合输出 ⇒ 沿后读到的是**下一拍**的写 ⇒ ⇒ 观察不到「陷阱当拍」）⇒ ⇒ 这正是原缺陷缺的那一维。
**变异自证**：还原旧形态 ⇒ ⇒ 复合 DUT 用例**转红**；还原 + **重建** ⇒ 复绿。

⚠️ **下游注意**：依赖「陷阱当拍 `csr_write_addr == CSR_MEPC`」的断言 **MUST 改** —— 沿后读到的是
**下一拍**的写（= `CSR_MCAUSE`）⇒ ⇒ 「陷阱当拍写 mepc」MUST 改用 `CsrBank.mepc` 的读回值观察。
**端口面零变更**（`ExceptionUnit.__io` 未动 · `CsrBank` 未动）。

spec：新增 capability **`rv32i-privilege`**（种子 Requirement：陷阱入口 MUST 把 `mepc` 与 `mcause`
都写进去且可读回）⇒ ⇒ `mtval` 仍**恒 0**（无数据源 ⇒ 属特权级立项）。

## 2026-11-15 — **新增：RV32I 的层次化 RTL 交付物**（诉求 ④）+ 发射器「双类消费者」修复（ADR-070 D1/D2）

**交付物**：`Rv32iPipeline` 的 **7 个 stage 成员**声明为**实例级成层**
（`ch::ch_layer` + 显式模块类型名）⇒ ⇒ 默认粒度（`RespectDeclarations`）下层次化产物
= **8 个 module**（`IfStage` / `IdStage` / `ExStage` / `MemStage` / `WbStage` / `HazardUnit` /
`ForwardingMux_32_` + `top`）⇒ ⇒ **扁平产物逐字节不变**（成层声明只写两个标志位）。

**⚠️ 模块类型名 = 明文非契约**（**用户裁决 · 2026-11-15** · ADR-070 §6 ⑦ 的待裁决项）：
`IfStage` · `IdStage` · `ExStage` · `MemStage` · `WbStage` · `HazardUnit` ·
`ForwardingMux<32>`（经 `sanitize_verilog_name` ⇒ 产物中为 `ForwardingMux_32_`）
⇒ ⇒ 它们是产物里的**名字**，改名 **不**构成破坏性变更（唯一义务 = 产物重新 lint 干净）。
⇒ ⇒ 稳定性来源是**成层粒度**（`ch_layer` 7 处），**不是**名字。
`load_lane`（B3 新增）**不**在该集合内 ⇒ 保持 inline。

**缺陷修复（ADR-070 D1 · H-A / H-B）**：同一子模块输出被「兄弟子模块的输入」与
「父体表达式 / 父 `output` 端口」**同时**消费时，发射器把分析层记录的**消费点名字**当成
pin 的实参 ⇒ ⇒ 判为双驱动 ⇒ ⇒ RV32I 层次化发射 **20 条边** fail-closed。
现：跨 module 边按「中转网 / 父侧落点网」发射，父 `output` 端口由显式 `assign` 取值。
⚠️ **反向不变式**：`into_child == true`（父驱动子 **input**）的双驱动**仍** fail-closed
—— IR 的源赋值是 last-wins ⇒ 放行等于**静默丢掉一个驱动**。

**公开面（追加）**：`HierarchyEmitResult::rewired_pins`（`<module>:<instance>.<port> -> <net>`）
⇒ ⇒ 「哪些 pin 被改接」从**日志**（`CHWARN`）升级为**结构化字段**。

**判据**：`test_rv32i_hier_rtl`（层次化产物：静态检查零 `%Error` / 零 `%Warning` ·
确定性 · 单连接不变式 · 顶层端口表 · 扁平 ≡ 层次化 **198 拍 × 9 观测** ·
**手推**写回序列 `{(x1,10),(x2,3),(x3,13),(x4,13),(x5,23)}` · 篡改一条 `assign` ⇒ **转红**）。

**承载文档**：`docs/adr/ADR-070-rv32i-hierarchical-layering-granularity.md`（D1/D2/D3）·
`openspec/changes/archive/2026-11-15-rv32i-hierarchical-example-2026-11`（ADDED ×2 + MODIFIED ×1）

## 2026-11-15 — **修复：`flush` 与「未知 opcode 冻结」同拍时重定向被丢掉**（ADR-071 审稿发现）

**缺陷（oracle 对抗式复核发现 · 同一批内自查出）**：上一批落地的「未知 opcode ⇒ 冻结」
引入本仓**第二个** stall 来源 ⇒ ⇒ 它与 `flush`（分支/跳转重定向）**可以同拍**；
而 `if_stage` 的 PC 更新门是 `!rst & !stall` ⇒ ⇒ **stall 无条件压过 redirect** ⇒ ⇒

1. **重定向被丢掉**（PC 被冻住 ⇒ 分支目标被跳过）
2. 下一拍 `valid = 0` ⇒ **冻结只活 1 拍**（不是终态）
3. 非法字被**静默**跳过 + `illegal_reason` 回落到 0

⇒ ⇒ **「跑飞零信号」被 G1 自己造出来**（且两后端同构 ⇒ parity 结构性失明）。

**修法**：`pc_update_en = !rst & !(stall & !(flush & branch_valid))`
⇒ ⇒ **`flush`（重定向）优先于冻结 stall**，但**仅当确有重定向**。

- **与 ID 级对齐**：`id_stage` 的 `idex_bubble = flush | stall` 本来就优先造气泡。
- **语义正确性**：`flush` 只来自 EX 级分支/跳转 ⇒ 同拍 ID 级持的是其 **fall-through**
  （= **错误路径**）⇒ 杀掉错误路径上的非法字**正确**；正确路径上的非法字**不可能**与
  `flush` 同拍 ⇒ 冻结在正确路径上仍是**终态**。
- **既有契约零改动**：「`flush` 但无重定向」在流水线内不可达 ⇒ `tests/test_rv32i_pipeline_behavior.cpp`
  Batch 4-a 的「`stall=1 ∧ flush=1 ∧ branch_valid=0` ⇒ PC 冻结」**原样通过**。

**判别力证据（变异自证）**：新增用例
`test_rv32i_soc_dual_parity` 的 `Rv32iSoc - flush wins over illegal-freeze stall`
（taken 分支 + fall-through 非法字 `0xFFFFFFFF` · `rd=31`）⇒ ⇒ 断言「分支目标的 `addi x5`
写回出现」+「x31 **未**被写回」+「`illegal_reason == 2` 恰在碰撞拍」
⇒ ⇒ **反转 `flush_redirect`（回退到 stall 优先）⇒ `REQUIRE(saw_x5)` 转红**（实测）⇒ 还原重建复绿。

**承载文档**：`docs/adr/ADR-071-fence-and-illegal-opcode-semantics.md`（D3-b 优先级 / D3-c 缺陷史 /
D1 假设登记与重开触发器）· `openspec/changes/illegal-freeze-flush-priority-2026-11`（1 Requirement / 4 Scenario）

**不在本项**：真 trap 语义（`mtvec` 接入）= 特权级独立立项 · `funct3`/`funct7` 非法组合 = residual

## 2026-11-15 — **RV32I 未知 opcode 可观测 + 冻结 · `FENCE` 显式化**（B4 · G3 → G1+G4）

**问题（实测 · 2026-11-10 登记）**：未识别的 opcode 被**静默当 NOP 逐条退休**
⇒ ⇒ 程序跑飞到非法指令时**零报错 / 零计数器 / 零波形信号**（只能二分固件定位）；
`FENCE`（`0x0F`）根本不在译码表里 ⇒ ⇒ 与「未知」混为一谈；
I-TCM 在程序末尾之后读到 `ch_mem` 初值 **0**（本身是未知 opcode）
⇒ ⇒ 「跑出程序尾部」完全不可见（既有 176 条退休里约 76 条来自这些 0 字）。

**顺序（critic 第 2 轮改向 · `git log` 可验）**：
1. **G3**：装载器把程序尾部填成 **NOP**（`0x00000013`）⇒ ⇒ 尾部不再产生未知 opcode
2. **G1 + G4**：未知 opcode 检测 + `FENCE` 显式化（同批）

**新增 / 变更**：
- `stages/id_stage.h` 新增输出 `illegal_reason`（2 bit：`0` 合法 · `1` **已知未实现**
  = `OP_SYSTEM`（ECALL/EBREAK/CSR/MRET）· `2` **未知 opcode**）
  ⇒ ⇒ 判据含 `io().valid`（否则 flush 后的残留旧指令会在**错误路径**上冻结流水线）
- `rv32i_pipeline.h`：**行为策略 = 冻结** —— `if_stage.stall` 与 `id_stage.stall`
  均 OR 上 `illegal_reason != 0`（PC 不前进 · ID/EX 装气泡 ⇒ 该指令**永不退休**）；
  新增输出 `dbg_illegal_reason`
- `rv32i_decoder.h`：新增 `OP_FENCE = 0x0F` ⇒ ⇒ **FENCE / FENCE.I 本版 = NOP**
  （单核无 cache ⇒ 无排序语义）⇒ ⇒ 属**已知**指令，**不**产生 `illegal_reason`
- SoC 顶层新增 `dbg_illegal_reason`（2 bit）⇒ ⇒ 顶层端口 **31 → 32** ·
  产物端口 **126 → 129**

**行为影响（实测支撑）**：
- **既有 golden 退休数不变**（`non_retired == 23` · `retire == kCycles - 24`）
  ⇒ ⇒ 因为 G3 先把尾部填成 NOP（NOP ≡ 0 字的时序/写回语义）
- 独立 Verilog TB `checks=24 fails=0`
- ⚠️ **G1 顺带挖出一个真缺陷**：`tests/test_verilator_complex_e2e.cpp` 的 imem 数组
  只有 **15** 个初值（第 16 字被 value-init 成 `0` = 未知 opcode），
  而注释一直写「6..15: NOP」⇒ ⇒ 修法 = 补第 16 个 NOP
- spec：新增 capability delta `rv32i-isa-semantics`（2 Requirement / 7 Scenario）

**不在本批**：`funct3`/`funct7` 非法组合 · G5（`mcause` 写缺陷）·
CSR / `ECALL` / `EBREAK` / `MRET` / 中断 / trap 入口（独立立项）

# CppHDL Changelog

User-visible changes to the CppHDL HDL library. Entries are grouped by
the change that introduced them (see `openspec/changes/` and
`openspec/specs/`).

---

## 2026-11-14 — **EX 级的算术核心改由库组件 `Rv32iAlu` 承担**（B4 · C3）

### Changed

- **`riscv::ExStage` 的算术核心**（`include/cpu/pipeline/stages/ex_stage.h`）
  ⇒ ⇒ 内联的 **10 个 ALU 分支**（ADD/SUB/AND/OR/XOR/SLL/SRL/SRA/SLT/SLTU）
  **已删** ⇒ ⇒ 改由**库组件** `riscv::Rv32iAlu` 承担
  ⇒ ⇒ 「RV32I 重复实现」收口（此前**两份** ALU 语义并存 ⇒ ⇒ 漂移风险）
  ⇒ ⇒ ⚠️ **「第二操作数选择」仍留在 EX 级**（`select(is_i_type, imm, rs2_data)`）
  —— 它**不是** ALU 的一部分 ⇒ ⇒ 组件只拿到算好的 `operand_b`
  ⇒ ⇒ **行为零变化**（既有 golden + 时序金标全绿为证）
  ⇒ ⇒ ⚠️ **产物顶层端口面 114 → 118**（+5 = ALU 组件内部节点冒泡）
  ⇒ ⇒ **根组件自有端口仍是 31**（不变）⇒ ⇒ 原 113 个端口**名一字未改**（只有新增）

### Removed

- **`riscv::Rv32iAlu` 的 `mem_size` / `mem_signed`（输出）**
  ⇒ ⇒ **全仓零消费者** ⇒ ⇒ ZERO-DEBT ⇒ ⇒ 且接入后会**冒泡成顶层端口**（无用端口面）
  ⇒ ⇒ ⚠️ `funct3` 输入**保留**（组件独立使用时仍有意义）

### Verification

- 「**未知 opcode 行为零变化**」有**显式断言**（`ExStage - unknown opcode keeps its
  ALU semantics`）⇒ ⇒ 未知 opcode ⇒ ⇒ 第二操作数 = `rs2_data` ⇒ ⇒ 结果 = `rs1 + rs2`
  ⇒ ⇒ 组件化若悄悄改了它 ⇒ ⇒ 断言**转红**

---

## 2026-11-14 — **ALU 操作码编码归一**（B4 · C2）

### Removed

- **`riscv::ALU_OP_*`（10 个常量）**（`include/cpu/pipeline/stages/ex_stage.h`）
  ⇒ ⇒ 该头原**自带**一套 ALU op 编码（ADD 0 · SUB **1** · AND **2** · OR **3** · XOR 4
  · SLL **5** · SRL **6** · SRA **7** · SLT **8** · SLTU **9**）
  ⇒ ⇒ 与库组件 `riscv::Rv32iAlu` 的 `riscv::ALU_*`（ADD 0 · SLL **1** · SLT **2** · SLTU **3**
  · XOR 4 · SRL **5** · OR **6** · AND **7** · SUB **10** · SRA **11**）**8 / 10 项不同**
  ⇒ ⇒ `alu_op = 3` 在旧侧 = **OR**、新侧 = **SLTU** ⇒ ⇒ 不归一就接入 = **零报错的行为漂移**
  ⇒ ⇒ **唯一权威 = `riscv::ALU_*`**（`include/cpu/pipeline/rv32i_alu.h`）

### Changed

- ⚠️ **「未定义 `alu_op` 值」集合随之变化**（C2 归一的**连带后果** ⇒ ⇒ **如实登记**）：
  | | 已定义 | 未定义 |
  |---|---|---|
  | 旧（`ALU_OP_*`） | 0–9 | 10–15 |
  | 新（`ALU_*`） | 0–7 · 10 · 11 | **8 · 9** · 12–15 |
  ⇒ ⇒ 下游若喂 `alu_op ∈ {8, 9}`（旧 = SLT / SLTU）⇒ ⇒ 新语义 = **结果 0**
  ⇒ ⇒ ⚠️ **仓内流水线不会产出 8/9**（`alu_op_calc` 按 `funct3` 派生 ⇒ 落点全在已定义域）⇒ ⇒ **行为零变化** ✓
- ⚠️ **迁移指引**：下游若按**旧** `ALU_OP_*` 编码喂 `alu_op` ⇒ ⇒ **MUST 改用 `riscv::ALU_*`**
  （**旧编码不是「另一个等价编码」** ⇒ ⇒ 它会**算出别的操作**）
  ⇒ ⇒ 新编码表：`ADD 0 · SLL 1 · SLT 2 · SLTU 3 · XOR 4 · SRL 5 · OR 6 · AND 7 · SUB 10 · SRA 11`
- **行为零变化**：归一只改**表示**，两侧的 select 链引用**同一套常量** ⇒ ⇒ 指令行为不变
  ⇒ ⇒ 既有 golden（20 项）+ 时序金标（`load_use_stalls` 8 · `non_retired` 23 · 退休数 176）**零漂移**
- ⚠️ **本批的「归一正确性」判据自带**：既有手推表 `kExCases` 的 `alu_op` **输入列**按新编码
  重映射（28 行 · 9 行实际变化）⇒ ⇒ **期望值一字未动** ⇒ ⇒ 映射错一条 ⇒ ⇒ 那条**必红**

---

## 2026-11-14 — **删掉 RV32I SoC 的 `ext_clk` 死端口**（R8 改判 · 用户裁决 A2）

### Removed

- **`Rv32iPipelineTop::ext_clk`（输入）**（`examples/riscv-mini/src/rv32i_pipeline_top.h`）
  ⇒ ⇒ 顶层端口面 **32 → 31**（产物 **114 → 113** 端口 · 6 输入 + 107 输出）
  ⇒ ⇒ **理由**：它是**死端口** —— 被赋值但**零消费者**（产物里**所有**流水线寄存器都锁在
  框架 `default_clock`：`always_ff @(posedge default_clock …)`）⇒ ⇒ 对行为**零影响**
  ⇒ ⇒ 却让「SoC 由 `ext_clk` 驱动」这个**错的**直觉留在端口面上 ⇒ ⇒ 用户按名字接线时
  踩的第一个坑，且**无任何报错**
  ⇒ ⇒ ⚠️ **迁移指引**：TB 里**删掉** `top_ext_clk` 那根线（不删 ⇒ verilator `PINMISSING` **致命**）
  ⇒ ⇒ **真时钟源 = 框架 `default_clock`**（TB 里接 `clk` 即可）
- **`riscv::Rv32iPipeline::clk`（输入）**（`include/cpu/pipeline/rv32i_pipeline.h`）
  ⇒ ⇒ **零消费者** ⇒ ⇒ ZERO-DEBT：零消费者端口不得留存
  ⇒ ⇒ 连带删（否则它会变成**悬空输入** ⇒ ⇒ 产物里 undriven wire ⇒ ⇒ verilator 严格模式下
  **UNDRIVEN 致命**）

### 行为影响

- **零行为变化**（**实测**支撑，非推断）：删前做针对性实测 ⇒ ⇒ `ext_clk` **0/1 交替翻转**
  ⇒ ⇒ **60 拍 × 27 个输出观测逐拍全等**（1351 断言）⇒ ⇒ 零影响
  ⇒ ⇒ 删后既有 golden（手推期望值）**全绿** ⇒ ⇒ 行为等价
- ⚠️ 「真让 `ext_clk` 生效」（改 codegen 的时钟选择 ⇒ 全仓影响）= **另独立立项**

---

## 2026-11-14 — B3 批 · **固件装载走内存初值**（`firmware-load-chain` L2 路线 A / L6）

### Added

- **`InstrTCM` / `DataTCM` 新增**可选**构造参数 `init_words`**
  （`include/cpu/pipeline/rv32i_tcm.h`）⇒ ⇒ 传**字镜像**（下标 = 字索引）
  ⇒ ⇒ 内部 `ch_mem(init_words, …)` 带 `init_data`
  ⇒ ⇒ 发射产物里自动出现 `initial begin mem[i] = … end`（既有 `print_mem_init`）
  ⇒ ⇒ **TB 零装载动作** · **顶层端口面一字不变**
  ⇒ ⇒ ⚠️ **默认空 ⇒ 与基线逐字节等价**（同一条 `init_memory(…, {})` 路径）
  ⇒ ⇒ 迁移指引：无（**非破坏性**；只在显式传参时改变产物内容）
- **`Rv32iPipelineTop` 新增 `itcm_init` / `dtcm_init` 构造参数**
  （`examples/riscv-mini/src/rv32i_pipeline_top.h`）⇒ ⇒ 透传到 TCM 构造
  ⇒ ⇒ 🔴 **`dtcm_init` 是 `.data` 段的落点** —— 此前 `prog_*` **只**接 I-TCM
  ⇒ ⇒ 有初始化 `.data` 的程序**装不进去**（本项修掉这一点）
- **`examples/riscv-mini/src/rv32i_fw_demo.h`** ⇒ ⇒ 带 `.data` 的示例固件（4 条指令，编码**手推**）
- **`examples/riscv-mini/emit_rtl_fw_init.cpp`** ⇒ ⇒ 「带固件初值」的发射器
  （`emit_rtl_rv32i_soc_fw_init`）⇒ ⇒ 产物自带 `initial` 初值块

### Changed

- **装载路线变更（方案层，非行为回退）**：原设计的「统一加载端口
  `(we, addr[字节], data[size])`」**移出本 change** ⇒ ⇒ 装载改为**发射期**完成
  ⇒ ⇒ 理由：路线 B 需付三项代价（D-TCM 写端口 **MULTIDRIVEN** · 装载期读地址共用 ·
  端口契约变更）⇒ ⇒ 运行期装载口 = 「**可装载性接口**」⇒ ⇒ 独立立项

---

## 2026-11-14 — B3 批 · **新增固件装载地基 + 编码器库**（`firmware-load-chain` L1/L3/L4/L5）

### Added

- **`include/cpu/riscv/rv32i_mem_map.h`**（**L1**）⇒ ⇒ RV32I 的**地址映射单一真相**：
  `kTcmAddrWidth`（12 · 字索引位宽）→ `kTcmBytes = 1<<(kTcmAddrWidth+2)`（**16 KB** ·
  **派生**）→ `kItcm{0x8000_0000, 16KB}` / `kDtcm{0x8001_0000, 16KB}` → `kPcReset = kItcm.base`
  ⇒ ⇒ ⚠️ **修掉一处声明-实现不一致**：`address_decoder.h` 此前声明 TCM 为 **64 KB**
  而硬件只有 **16 KB** ⇒ ⇒ `[0x8000_4000, 0x8001_0000)` 的访问会被**静默截断**
  ⇒ ⇒ 现由同一份派生 ⇒ ⇒ 那段落进「区域外」⇒ ⇒ **可诊断**（不再静默丢高位）
  ⇒ ⇒ 迁移指引：下游若硬编码 `ADDR_ITCM_SIZE == 0x10000` ⇒ ⇒ **MUST 改为 16384**
- **`include/cpu/riscv/rv32i_asm_encode.h`**（**L3**）⇒ ⇒ RV32I **指令编码器**
  （header-only · `constexpr`）：`NOP` · `enc_{r,i,s,b,j,u}` · 17 个便捷别名
  ⇒ ⇒ 固件与转储器**共用同一份** ⇒ ⇒ 两份编码器漂移的风险归零
  ⇒ ⇒ ⚠️ **手算交叉校验**（`FirmwareEncoderCrossCheck`）**保留**并且现在校验的是库
- **`tools/rv32i-fw-dump`**（**L4+L5**）⇒ ⇒ RV32I 固件**转储器 / ELF 装载器**：
  `--in <fw.elf|fw.json> --format mem|vh|manifest --out <path>`
  ⇒ ⇒ `manifest`（schema `rv32i-fw-manifest-v1`）携带**入口** + **基址/尺寸** + 逐字的**字节地址**
  ⇒ ⇒ ELF32 + `PT_LOAD` · `.bss`（`p_memsz > p_filesz`）**补零** · **越界 ⇒ exit 3**（禁静默截断）
  ⇒ ⇒ ⚠️ **不解析符号表**（已知边界：`.rela` 在 section 里 ⇒ 静态重定位检查**不完整**）
  ⇒ ⇒ ⚠️ 本二进制 **不链 `examples/**`**（沿用 `rtl-emit-product-tooling` C1 纪律）
- **`include/cpu/pipeline/load_lane_selector.h`** ⇒ ⇒ load 的**字节道（4）/ 半字道（2）选择 +
  符号 / 零扩展**（语义**逐字**迁自 `MemStage`）⇒ ⇒ 因为 `sread` 让 load 数据**晚 1 拍**可用
  ⇒ ⇒ 车道选择 MUST 在 **M2 的下游**做（ADR-069 **V5**）
- **`tests/chlib/test_hazard_unit_stall.cpp`** ⇒ ⇒ `HazardUnit` 的**组件级停顿链真值表**
  （ADR-069 **V12** · 36 断言 / 3 用例）⇒ ⇒ 钉住「检测条件持续 N 拍 ⇒ 停顿 **N+2** 拍」

---

---

## 2026-11-14 — B3 批 · **D-TCM 寄存读 + MEM 数据对齐（A0）**（`memory-timing-contract-2026-11`）

### Changed

- **`riscv::DataTCM` 的读口**（`include/cpu/pipeline/rv32i_tcm.h`）⇒ ⇒ 读口由
  **全组合读**（`aread` · 0 拍）改为**寄存读**（`sread` · **1 拍**）
  ⇒ ⇒ 字节写的「读-改-写」仍用 `aread`（**零改动** ⇒ ⇒ `ch_mem` 无字节使能写 API）
  ⇒ ⇒ 依据：ADR-068（Accepted · V7）· ADR-069（Accepted · 决策 **A0**）
  ⇒ ⇒ **迁移指引**：下游若按「**同拍**读回数据」写的 TB ⇒ ⇒ **MUST 改成「下一拍」**
  ⇒ ⇒ ⚠️ **沿级 read-old 不变**（同址 store→load 的**指令级**语义是 **read-new** ⇒ ⇒ ISA 不变）
- **`Rv32iPipeline` 的 `data_ready`**（`include/cpu/pipeline/rv32i_pipeline.h`）⇒ ⇒ 语义**登记**为
  「**固定 1 拍延迟**（**不是握手**）」⇒ ⇒ **driver 仍恒 1**
  ⇒ ⇒ ⚠️ **本批不实现真反压**（「驱动 0 ⇒ MEM 冻结」属「接 cache」独立立项 ⇒ ⇒ ADR-068 V7 胜出）
- **RV32I 的 load-use 停顿拍数：3 → 4 拍**（每处紧邻 load→use）⇒ ⇒ 固件的
  `load_use_stalls` 基线 **6 → 8** · `non_retired` **21 → 23** · 退休数 **178 → 176**
  ⇒ ⇒ 依据：ADR-069 §2.1（**EX→WB 级数 2 → 3**）· §7.1.1（拍级表）
  ⇒ ⇒ **迁移指引**：下游若锁死这三个数 ⇒ ⇒ MUST 同步更新（**手推值**，非仿真抄值）

### Removed

- **`riscv::MemStage` 的 `mem_read_data`（输入）/ `mem_data`（输出）**
  （`include/cpu/pipeline/stages/mem_stage.h`）⇒ ⇒ 因为 `sread` 让 load 数据**晚 1 拍**可用
  ⇒ ⇒ MEM 级（拍 N+1）**已无**正确数据 ⇒ ⇒ 若仍在那里做车道选择 ⇒ ⇒ 选的是**上一拍**的数据
  ⇒ ⇒ **静默错车道** ⇒ ⇒ load 的**车道选择 / 符号扩展** MUST 迁到 **M2 的下游**
  ⇒ ⇒ 依据：ADR-069 **A0** · §7.2 · **V5**
  ⇒ ⇒ **迁移指引**：下游若驱动 `mem_read_data` 或消费 `mem_data` ⇒ ⇒ **改用**
  `riscv::LoadLaneSelector`（新组件 · `include/cpu/pipeline/load_lane_selector.h` · 语义**逐字**不变）
  ⇒ ⇒ ⚠️ **`MemStage::is_load` 保留**（公开契约，虽已无内部消费者）

### Added

- **`riscv::LoadLaneSelector`**（`include/cpu/pipeline/load_lane_selector.h`）⇒ ⇒ load 的
  **字节道（4）/ 半字道（2）选择 + 符号扩展 / 零扩展**（`LB`/`LH` 符号 · `LBU`/`LHU` 零 · `LW` 整字）
  ⇒ ⇒ 语义**逐字**迁自原 `MemStage` ⇒ ⇒ 组件级双后端覆盖**不缩面**

---

## 2026-11-13 — P8 批 · **运行期 A/B 验证（interpreter vs JIT）移除**

### Removed

- **`Simulator::set_ab_verification()` / `is_ab_verification()` / `ab_verification_`**（`include/simulator.h`）⇒ ⇒
  自 dual-function JIT 重构（`tick_comb` + `tick_seq`）之后，A/B 比对逻辑**从未**重新设计
  ⇒ ⇒ 开启该 flag 只会在每个 `tick()` 打一条 `CHWARN`，**不改变任何行为**
  ⇒ ⇒ 按 Zero-Debt「No dead code」**已删除**（含 `tick()` 里那段 WARN 与两个只为它存在的局部量
  `ab_result_a` / `run_ab_verify`）
  ⇒ ⇒ 依据：ADR-035 **v2.2 · R9** · phase-6 **P8**
  ⇒ ⇒ **迁移指引**：下游若调用 `set_ab_verification(true)` ⇒ ⇒ **直接删掉该调用**
  （它本来就不做任何事）⇒ ⇒ 若依赖 `is_ab_verification()` 返回真值 ⇒ ⇒ 该真值**从来不代表任何比对结果**
  ⇒ ⇒ **残余风险**：运行期**无** interpreter-vs-JIT 双跑校验
  ⇒ ⇒ 缓解 = **golden 测试**（`tests/jit/test_jit_golden.cpp` · 以**手推期望值**逐值断言）+ JIT 默认 ON

---

## 2026-11-13 — R17.2 批 · **RV32I SoC 顶层端口具名**（用户 ⑤ 的原始诉求 · 首个真实消费）

### Changed

- **`Rv32iPipelineTop` 的 32 个顶层端口**（`examples/riscv-mini/src/rv32i_pipeline_top.h`）⇒ ⇒
  产物端口名由 `top_unnamed_input*` / `top_unnamed_output*` 改为 **`top_<C++ 成员名>`**
  （例：`top_ext_clk` · `top_prog_we` · `top_obs_data_addr` · `top_dbg_id_pc`）
  ⇒ ⇒ ⚠️ **端口数量 / 方向 / 宽度 / 顺序全部不变**（顶层 **114** 个端口一一对应）
  ⇒ ⇒ **只改名字** ⇒ ⇒ **行为零变化**（TB 期望值只依赖信号语义）
  ⇒ ⇒ 依据：`port-naming-ratchet` **N2 补遗「首批消化 32 端口」**（R17.2 ·
  architect 裁量闭 R17.1 =「现在做」）
  ⇒ ⇒ **迁移指引**：下游若按 `top_unnamed_*` **序号**硬编码顶层端口 ⇒ ⇒ **MUST 改为按名取**
  （这正是本项的立意 —— 此前写 TB 必须查 port map）
  ⇒ ⇒ ⚠️ **后代冒泡端口仍无名字**（`top_pipeline_*` / `top_itcm_*` / `top_dtcm_*`，**80** 个）
  ⇒ ⇒ **不在本项范围** ⇒ ⇒ 归属后续批次
- **端口命名棘轮基线** ⇒ ⇒ `scripts/verify_port_naming.sh` 的 `EXPECTED_EXEMPT_PORTS`
  **1484 → 1452**（存量未具名端口减 32）⇒ ⇒ `.rddf/checks/port-naming-exempt.tsv` 同步减 32 行

---

## 2026-11-12 — B3 批 · **公开面前置登记**（D-TCM 内存时序 + 固件加载链路 · **未实施**）

### Changed

> ⚠️ **本段是「前置登记」，不是「已发布变更」** ⇒ ⇒ 对应的两个 change **尚未立项**
> ⇒ ⇒ 行为**尚未改变** ⇒ ⇒ 登记的目的是让**下游 TB 提前知道**「什么会变、怎么迁」。

- **`riscv::DataTCM` 读口 + `Rv32iPipeline::data_ready`** ⇒ ⇒ D-TCM 由**全组合读**
  （`aread` · 0 拍）改为**寄存读**（`sread` · 1 拍）⇒ ⇒ MEM 级数据采样相位整体后移
  ⇒ ⇒ `data_ready` 由「恒 `true`」重定义为**反压输入**
  ⇒ ⇒ 依据：`memory-timing-contract` **T1 / T3**（**ADR-069**（**Accepted** · 2026-11-12 ·
  决策 = **候选 ②（A0）**）· **ADR-068**（Accepted））
- **`Rv32iPipelineTop` 的加载端口族** ⇒ ⇒ `prog_addr` 语义由「**字索引**」改为「**字节地址**」
  ⇒ ⇒ 且新增 D-TCM 加载通路（现 `prog_*` **只**接 I-TCM ⇒ ⇒ 有初始化 `.data` 的程序
  **装不进去**）⇒ ⇒ 依据：`firmware-load-chain` **L1–L6**（**未立项**）

⇒ ⇒ **两个 change 立项前**，本段即为下游的**迁移依据**（按旧语义写的 TB / TB 骨架 **会断**）。

## 2026-11-12 — B2 批 · **修 UART RX demo 的两个缺陷**（位反转 + 采样点不缩放）

### Fixed

- **`examples/spinalhdl-ported/uart_rx/uart_rx.cpp`**
  ⇒ ⇒ **D7：解出的字节被位反转**（旧：左移累加 ⇒ 首个采样位被推到 bit7）
  ⇒ ⇒ 线上 LSB first ⇒ 线上 `0x10` ⇒ **解出 `0x08`** · `0x11` ⇒ `0x88` ⇒ ⇒ 改为
  **右移累加 + 新位进 MSB** ⇒ ⇒ 首个采样位落在 bit0
  ⇒ ⇒ **D6：采样点硬编码 `div == 4`**（⇒ 只对 `prescale == 9` 成立 ⇒ 别的 `prescale`
  在**位的前沿**采样 ⇒ 抖动即采错）⇒ ⇒ 改为 `div == (prescale >> 1)`
  ⇒ ⇒ `prescale == 9` 时**行为完全不变**（9/2 = 4）⇒ ⇒ 零回归

### ⚠️ **D7 为什么藏了这么久**（值得记住的一条）

example 自带的 C++ demo 用 **0x5A** 做测试值 ⇒ ⇒ **0x5A 是位回文**
⇒ ⇒ **恰好掩盖**了位反转缺陷 ⇒ ⇒ demo 一直报 PASS。
⇒ ⇒ demo 的测试值已改为 **0x4B**（非回文）⇒ ⇒ 实测：重新注入缺陷后 demo
**FAILURE（Expected 0x4B, got 0xd2）** ⇒ ⇒ **C++ 侧断言现在真有判别力**。
⇒ ⇒ **纪律**：**测试值必须避开对称值**（位回文 / 字节回文），否则实现级的
**位序 / 端序**缺陷会被**恰好**掩盖。

### 顺带修的既有告警

`uart_rx.cpp` demo `main()` 里 10 处 C++ 条件把 `&&` 写成 `&` ⇒ ⇒ 本文件本次被改动
⇒ ⇒ 按 Zero-Debt「改动文件 MUST 零告警」⇒ ⇒ 改为 `&&`（语义等价）。

### 行为变更（非符号移除）

`uart_rx` demo 的 `data` 输出不再位反转、采样点随 `prescale` 缩放。
登记：`.rddf/checks/public-api-surface.md`「行为变更」节 +
`.rddf/checks/decision-request-uart-rx-defects-2026-11.md`。

---

## 2026-11-12 — B2 批 · **修 FIFO demo 的 `full` 门限**（D5 · 两处同改 · 行为变更）

### Fixed

- **`samples/fifo_example.cpp`** + **`examples/spinalhdl-ported/fifo/stream_fifo_example.cpp`**
  ⇒ ⇒ **同型缺陷两处同改**（Zero-Debt）
  ⇒ ⇒ 旧写法 `full = (wr+1 == rd)` 在**完整指针**空间里比较 ⇒ 门限 = `2^(addr_width+1)-1`
  （N=16 ⇒ **31**）⇒ ⇒ **`full` 永不置位** ⇒ ⇒ **第 N+1 次写入起地址混叠**
  （实测：push 20 个后 pop，最老的 4 个已被覆盖丢失）
  ⇒ ⇒ 改为经典方案：**地址位相等 + wrap 位不同** ⇒ **容量 = N**
  ⇒ ⇒ 与 `empty`（`rd == wr`，全部位相等）自洽
  ⚠️ **库组件 `chlib::sync_fifo` 未变**（它一直是对的 —— `tests/chlib/test_fifo.cpp` 断言满态）
  ⇒ ⇒ 缺陷只在**手写 demo** ⇒ ⇒ **不涉及库 API 契约变更**

### 行为变更（非符号移除）

手写 FIFO demo 的 `full` 端口现在会在**真正满时**置位（之前恒 0）。
若曾依赖「`full` 恒 0」，那个依赖本就不成立。
登记：`.rddf/checks/public-api-surface.md`「行为变更」节 +
`.rddf/checks/decision-request-fifo-full-threshold-2026-11.md`（已结案 · 选项 A 落地）。

### 判别力（TB 判据随之**正面放开**）

`tb_fifo_stream_example.sv` 现在正面判 `full` 门限（在途 15 ⇒ 0 · 16 ⇒ 1）
⇒ ⇒ **变异 M5 = 把 `full` 回退成旧公式 ⇒ TB 转红** ⇒ ⇒ **修复承重**（可被判出）。

---

## 2026-11-12 — B2 批 · 第 6 个 TB（fifo_stream）+ **一个实测的实现缺陷**（D5 · 待裁决）

### Added

- **`examples/spinalhdl-ported/fifo/tb_fifo_stream_example.sv`**
  ⇒ **4 场景 / 58 断言 / 27 条 spec-derived** · `RESULT: checks=58 fails=0`
  ⇒ 判据：**顺序保持**（3 元素）· **容量边界**（**15 在途全取回、不丢数据**）·
    **metamorphic R1 置换性**（逐项恒等）+ **R2 `empty` 不变量**（TB 侧在途计数）
- **`.rddf/checks/decision-request-fifo-full-threshold-2026-11.md`**（新）
  ⇒ D5 的**可裁决决策请求单**（三选项 + 建议 + 裁决点）

### ⚠️ 一个**实测**的实现缺陷（D5 · 不在 TB 里固化）

`fifo_stream_example` 的 `full` 用**完整 5 位指针**比较
（`full = (wr+1 == rd)`）⇒ **门限 = 31 个在途**，而内存只有 **16 槽**
⇒ ⇒ **第 16 次写入起地址混叠** —— 实测 push 20 个后 pop，
最老的 4 个（`10 11 12 13`）**已被覆盖丢失**，`full` 从不置位。

⇒ ⇒ **TB 的处置（刻意）**：**不判** `full` 在 15/16 在途处的取值
（16 槽 FIFO 的正确语义可以是 15 或 16 在途 ⇒ **两种正确实现此处也不一致**
⇒ ⇒ 判它 = 固化偶然选择）· 数据完整性断言**限定 ≤ 15 在途** · `full` 只判「≤ 14 在途时为 0」。

⇒ ⇒ **变异自证**（对 DUT 变异）：把 `full` 改成 **4 位地址比较**（= 更正确的实现）
⇒ ⇒ TB **仍然全绿** ⇒ ⇒ 证明 TB **不锁死**当前实现的门限；
另 3 条变异（读地址偏移 / `empty` 粘住 / 写地址偏移）**各自转红**。

### 期望源口径订正（原清单写错了接口）

原清单写「缺 **ready/valid** 握手」⇒ ⇒ **实测接口是
`din/push/pop/dout/empty/full`**（`stream_fifo_example.cpp:28-34`）⇒ ⇒ **没有 ready/valid**
⇒ ⇒ 判据对象改为 **push/pop + empty/full**。

---

## 2026-11-11 — B2 批 · 自查纠偏：metamorphic 判据的「标注不实」

> 用户要求「检查已完成的示例是否都通过测试、是否符合预期目标」⇒ 做了全量自查（清缓存强制重编译）。

### 通过性（清缓存 · 全量重编译）

5 个 verified **全部通过**：`counter`(54) · `counter_simple`(768) · `pwm`(13) ·
`gpio`(9) · `quadrature`(7) ⇒ **无「命中旧缓存」的假绿**。

### ❌ 自查发现的实质问题：**metamorphic 判据 3/5 标注不实**

| TB | 原宣称 | 实际 | 处置 |
|---|---|---|---|
| `counter_simple` | 「周期 256」 | 第二轮**又断言绝对值** `== n`（依赖第一轮期望） | ✅ **改为记录第一轮实测值**再比 ⇒ 真相对（768 checks） |
| `counter` | 「静止性 / clear 后重建」 | 两者都是**绝对判据**（期望写死） | ✅ 新增**场景 7：逐拍相对判据** `enable=1⇒+1 / =0⇒不变`（40 拍确定性伪随机）⇒ 54 checks |
| `gpio` | （无） | **缺** | ✅ 新增**场景 G：自反性「输入恒定 8 拍 ⇒ 中断恒 0」**（无状态依赖）⇒ 9 checks |
| `pwm` | 「256 拍高电平数 == duty」 | 真相对 | ✅ 无需改 |
| `quadrature` | 「正转 N 后反转 N 回原点」 | 真相对 | ✅ 无需改 |

### 两次「写错的判据」及其否决理由（都写进登记文档留档）

1. **`counter` 的「区间 Δ == enable 拍数」**：实测 `delta=20 beats=19` ⇒ **边界差 1**
   （`base` 采样与第一个 `posedge` 的先后无法稳定约定）⇒ 改为**逐拍比较相邻值**
   （无基准值依赖）。
2. **`gpio` 的「一次跳变 ⇒ 恰好 1 拍脉冲」**：实测 `pulse_count=0` ⇒
   与本 TB 前面的场景状态**强耦合**（同步链相位不确定）⇒ 改为**自反性判据**。

### 严格 flags 继续抓 TB 自身缺陷（已修）

`%Warning-IMPLICITSTATIC`（`integer` / `logic` 在 `begin/end` 块里声明）·
`%Warning-WIDTHEXPAND`（`integer` vs 8/16 位）—— 共 3 处。

### 验证

清缓存全量：5 verified 全绿（54 / 768 / 13 / 9 / 7）· `--self-test` 5 投毒各自红 +
还原复绿 · 5 pending（`--budget=5`）

---

## 2026-11-11 — B2 批 · 第 5 个 TB（quadrature_encoder）+ 一条期望源边界裁决

### Added
- **`examples/spinalhdl-ported/quadrature_encoder/tb_quadrature_encoder_example.sv`**
  ⇒ **语义级**判据（不逐拍抄中间值）· `RESULT: checks=7 fails=0`
  ⇒ 复位=0 · 正转 2 周期 ⇒ 幅值 8 · **metamorphic：反转 2 周期回原点** ·
    正转 1 周期 ⇒ 幅值 4 · **Z 上升 ⇒ 清零** · `a_sync/b_sync` = 2 拍延迟

### 一条期望源边界裁决（**重要**）
首版按 `count_up`/`count_down` 的**公式**推出「`11→10` 是 +1」，**实测是 -1**。
⇒ ⇒ 判定：正交编码器的规范只保证**幅值**（一周期 = 4 计数）与**方向名**（CW/CCW），
**「CW 对应 +1 还是 -1」是计数器的符号约定 ⇒ 属 `impl-contract`**
⇒ ⇒ TB 改为**符号无关**的判据（`(pos - base) mod 65536 ∈ {+m, -m}`）+ metamorphic
（正转 N 后反转 N 回原点 —— 与符号无关）⇒ ⇒ **不硬判符号**，理由写进登记文档。
⇒ ⇒ 这是「期望源分级」的一次**真实应用**：不是所有能从实现推出来的量都该硬判。

### 严格 flags 再抓两条 TB 自身缺陷（已修）
- `%Warning-PINMISSING`：**inline 后代的 4 个输出也是顶层端口** ⇒ TB 未连
  （`top_enc_unnamed_output*`）⇒ 已补。
- `%Warning-WIDTHEXPAND`：`integer`（32 位）与 16 位 `position` 混用
  ⇒ task 参数改 `[15:0]`，`pos0` 改 `logic [15:0]`。

### 验证
- E5：**5 项 verified**（counter 14 · counter_simple 512 · pwm 13 · gpio 8 ·
  quadrature 7）+ **5 项 pending**（`--budget=5`）· `--self-test` 全绿

---

## 2026-11-11 — B2 批 · P1 期望源独立性（消除循环论证）

> 来源：`@metis` 审查指出「**期望表用实现表达式书写 = 转录不是推导** ⇒ 循环论证」。

### Added
- **`.rddf/checks/examples-rtl-expectations.md`** —— **可审阅的期望登记**：
  每个 DUT 一节 = 上游参考（**SpinalHDL Scala 原文**，逐字转录）· 「输入序列 → 期望输出」表 ·
  推导前提（impl-contract）· **metamorphic 判据** · **首次跑红记录**。
  ⇒ ⇒ TB 只写 `EXPECT-SRC: .rddf/checks/examples-rtl-expectations.md#<anchor>`，
  **不再**把被测 `.cpp` 当出处。
- **期望分两级**（TSV 的 `spec_checks` 列）：
  **spec-derived（强）** = 上游 Scala / 外部规范；**impl-contract（弱）** = 延迟/拍数/tie-off。
- **护栏新判据**（`verify_examples_rtl_sim.sh`）：
  - `EXPECT-SRC:` 缺失 ⇒ 红
  - `EXPECT-SRC:` **指向 `.cpp`** ⇒ 红（转录实现 = 循环论证）—— 判别力实测 ✓
  - `status=verified` 但 `spec_checks < 3` ⇒ **软告警**（impl-contract only：只能防漂移，
    不能证明符合规范）⇒ 不硬判是因为 `gpio_example` 的上游偏离**待裁决**。

### metamorphic 判据（相对判据 · 不依赖绝对值）
- `counter_simple`：**周期 256**（`v[n] == v[n+256]`）⇒ 512 拍（前半绝对 · 后半相对）
- `pwm`：**256 拍内高电平数 == duty**（只依赖「counter 跑满 0..255」+ `pwm = en ∧ (counter < duty)`）
- `counter`：**静止性**（`enable=0 ∧ clear=0` 区间内值恒定）+ **clear 后重建**
- `gpio`：**静止性** + **屏蔽性**（`interrupt_enable=0` ⇒ 恒 0）

### ⚠️ 一条实质发现（已登记，待裁决）
**`gpio_example` 的 C++ 实现与上游 Scala 语义不同**：

| 维度 | 上游 Scala | 被测 C++ |
|---|---|---|
| 同步级数 | **1 级** | **2 级** |
| 边沿判定 | **单 bit**（`io.pin` 是 `Bool`） | **8 位逐位**（非零即中断） |
| 端口 | `pin` / `interrupt` / `value` | 多出 `output_enable` / `interrupt_enable` + `output_reg` 锁存 |

⇒ ⇒ 该 TB 的期望源只能标 `impl-contract`（`spec_checks=0`）⇒ ⇒
**待裁决**：是有意扩展还是移植缺陷？在裁决前 TB 的作用是**防静默漂移**。

### 严格 flags 的即时收益（实测）
去掉 `-Wno-fatal` 后立刻抓到 TB 自己的 `%Warning-IMPLICITSTATIC`
（`integer` 在 `begin/end` 块里声明）⇒ ⇒ 已改到 module 级。
**这就是「无 `-Wno-fatal`」的判别力证明**。

### 验证
- E5：4 项 verified 全绿（counter 14 · counter_simple **512** · pwm **13** · gpio 8）+ 6 项 pending 放行
- `--self-test`：基线绿 + 5 投毒各自红 + 还原复绿
- `EXPECT-SRC → .cpp` 投毒 ⇒ 红 ✓
- 4 条其它护栏全绿

---

## 2026-11-11 — B2 批 · 护栏判别力加固（metis 审查后的 P0/P2 项）

> 来源：`@metis` 审查「B2 剩余 6 项该继续还是调整」⇒ 结论**先调整**（护栏目前没有判别力）。

### Fixed（P0 · 护栏本身）
- **`--self-test` 之前根本没实现**：脚本接受该参数但**没有分支** ⇒ 会静默跑完整 check
  ⇒ ⇒ Acceptance「投毒必红」**悄悄满足不了**。现实现 **5 条投毒**，各自可判红：
  ① 删 TB 的 `RESULT` 行 ② `checks=0` ③ `REGISTERED_TARGETS` 与实际行数不符
  ④ `status=verified` 但 TB 缺失 ⑤ `status=pending` 未放行 ⇒ 基线绿 + 5 投毒全红 + 还原复绿。
- **护栏从不读 `status`**（`pending` 行只要 TB 能打 `RESULT` 就转绿 = 「缺失=失败」的静默旁路）
  ⇒ ⇒ 现在 `pending` **必红**；`--allow-pending --budget=<N>` 是**唯一**的过渡放行形态
  （**真跳过**，标注「已放行但未验证」；预算 MUST 单调下降，缺 `--budget` 直接红）。
- **`checks > 0` 太弱**（删到 1 条断言仍绿）⇒ ⇒ 清单新增 `min_checks`（运行时下界）
  + `scenarios`（语义场景数，`counter_simple` 是 **1 场景 × 256 拍**，不是 256 个语义）
  + `spec_checks`（外部规范支撑的断言数，`>=3` 才算真正 verified —— **当前全为 0，待 P1**）
  + `kind`（`target` / `dut`）。
- **无 TIMEOUT** ⇒ ⇒ 加 `timeout ${TB_TIMEOUT:-120}`（挂死 TB 会拖死 `pr-verilator-fast`）。
- **登记数锚点**：清单加 `# REGISTERED_TARGETS: 10` ⇒ ⇒ 删一行即红（防静默缩面）；
  数据行为 0 ⇒ **exit 2**（fail-closed）。
- **`-Wno-fatal` 让宽度错位只 warning ⇒ 绿** ⇒ ⇒ 去掉它（仓内 `verify_hierarchical_rtl.sh`
  的 R1 早有「禁 `-Wno-fatal`」纪律）。
- **产用卫生产物**（`--emit-hygiene`）：它**不带** blanket `lint_off` ⇒ ⇒ 宽度问题真的报出来
  （实测：**9 个 example 的卫生产物 `--lint-only` 全部零告警零错误**）。
- **CI 缓存补 `~/.cache/cpphdl/rtl_sim`**：之前只缓存 `verilator` ⇒ ⇒ **E7 在 CI 完全失效**
  ⇒ ⇒ 每个 PR 冷编译 10 个 TB（metis：「按本地判会悄悄满足」）。
- **E8 的 `--check` 在 `BUILD_DIR` 缺失时静默 exit 0** ⇒ ⇒ 改为 **exit 2**（fail-closed）；
  `apply` / `dry-run` 仍允许缺 build。

### Removed（P2 · AI slop 清理）
- **删「同线相等」断言**（`tb_counter_example` / `tb_counter_simple_example` /
  `tb_gpio_example`）：两者在产物里是 `assign A = B;` 的**同一条线** ⇒ ⇒ 断言**恒真**
  ⇒ 零信息量，且把「inline 后代复制出同名端口」这个**实现产物**固化成契约。
  ⇒ ⇒ 端口存在性由「编译通过」保证。
- **注明 `$isunknown` 的局限**：本 runner 下 Verilator 把 X 优化成已知值
  ⇒ ⇒ 它对「未驱动」**无判别力**（真正的 X 判据在 E1 ⑤ 的内容判据）⇒
  保留它是为对齐 E3 的「TB MUST 断言 `!==x`」约定，并在 TB 注释里写明。

### 验证
- E5：严格模式 **6 红 4 绿**（pending 必红）· `--allow-pending --budget=6` **绿** ·
  预算收紧到 5 ⇒ 红 · 缺 `--budget` ⇒ 红
- `--self-test`：基线绿 + **5 条投毒各自转红** + 还原复绿（仓内 TB 完好）
- 3 个 examples 护栏全绿

---

## 2026-11-11 — B2 批（E1 契约判据 + E4 名单 + E5 护栏 + E7 缓存 + 首个手写 TB）

### Added
- **`tests/test_rtl_emit_contract.cpp`**（**E1** · 28 断言 / 5 用例）：只保留 C5 的 F1–F8
  **没有**的部分 —— 跨进程确定性 · 产物结构（`module`/`endmodule` 配平 · **端口数 ==
  manifest 的顶层端口数**）· **「按名驱动」的机械证据**（改 manifest 端口名 ⇒ TB
  **编译失败**）· **产物变异 ⇒ TB 必红** · **X 检查**。
  ⚠️ **端口口径**按 2026-11-11 用户裁决 **A**（`ports[]` = 全部节点端口）⇒ 比的是
  「顶层端口数」⇒ ⇒ 判别力**只**能来自**含子组件**的设计（单组件设计里两者恰好相等
  ⇒ ⇒ 变异恒绿 ⇒ 实测踩中）。
- **`.rddf/checks/examples-rtl-sim-targets.tsv`**（**E4**）：首批 10 项逐条登记
  （target · source · component · **emitter** · tb · status · **「缺什么」**）。
- **`scripts/verify_examples_rtl_sim.sh`**（**E5**）：TB 存在 → 编译通过 → 跑过 →
  断言**非空**（以**运行时证据** `RESULT: checks=<N> fails=0` 且 N>0 判定 ⇒ ⇒
  **禁**自研 Verilog 静态分析器）⇒ ⇒ **缺失 = 失败**（不跳过）。
- **E7 缓存**：key = SHA-1(产物 + TB + flags + verilator 版本 + **TB 名**）⇒ ⇒
  落在 `~/.cache/cpphdl/rtl_sim/<key>/`（**独立目录** ⇒ 不与既有后端缓存冲突）
  ⇒ ⇒ 实测 **6.2s → 0.23s**（命中）⇒ ⇒ 命中仍**验证产物存在**（缺失 ⇒ 重编译）。
- **10 个 `emit_rtl_*` target**（方案 3，用户裁决）：每 example 一个 ⇒ ⇒ 护栏有
  「产物从哪来」的**唯一**答案 ⇒ ⇒ example 源码**零改动**。
- **首个手写 TB** `examples/spinalhdl-ported/counter/tb_counter_simple_example.sv`
  ⇒ ⇒ **256 拍逐拍检查**（自由运行 +1 计数器，含 8 位回绕）
  ⇒ ⇒ `RESULT: checks=256 fails=0`。

### Fixed
- **E7 的缓存 key 用错了**：key 用了**路径**而不是**内容** ⇒ ⇒ 改了 TB 的**内容**
  但路径不变 ⇒ ⇒ key 不变 ⇒ ⇒ **命中旧缓存** ⇒ ⇒ 跑的是**旧 TB**
  （实测：改 gpio 的手推表后仍「命中缓存」⇒ ⇒ 护栏说绿、其实验的是旧版本）
  ⇒ ⇒ 改为**内容哈希**（产物 + TB 各自 sha1）⇒ ⇒ 判别力实测：
  改 TB 内容 ⇒ **不**命中（重新编译）✓

### Fixed / 口径（实测踩出来的）
- **Verilator 把 X 优化成已知值** ⇒ 「制造 X 看断言转红」**不可能成立**
  （`8'bx` 在 `--x-assign unique` / `--x-initial unique|fast|0` 的**全部**组合下
  `$isunknown` 都是 false；未驱动的 output 也当 0）⇒ ⇒ E1 ⑤ 改为**内容判据**。
- **复位释放必须避开 posedge 采样竞态**：在 `@(posedge clk)` 之后**同一时刻**拉低
  `rst` ⇒ 该沿的 `always_ff` 看到**新值** ⇒ ⇒ 复位拍被吃掉 ⇒ ⇒ 计数从 2 开始
  （手推 1、实测 2）⇒ ⇒ 改在 **`negedge`** 释放。
- **「跳到第 N 拍再查一次」的 task 是错法**：期望值按「第 n 拍」写、task 内却只等
  **1** 拍 ⇒ ⇒ 期望值与拍数**错位**（实测 `fails=6`）⇒ ⇒ 改**逐拍检查**。

### 验证
- ctest **263/263 PASS** · 12 条护栏全绿 · examples **28/28**
- E5 护栏：首批 10 项中 **1 项 verified**（`counter_simple_example`）· 其余 9 项
  **红**（TB 缺失 = 失败）⇒ ⇒ **因此 E6（CI 接线）还不能做**

---

## 2026-11-11 — B2 批（E2 测试台骨架 + E8 锚点单一更新入口）

### Added
- **`--emit-tb-stub`**（`examples-rtl-sim-coverage` **E2**）：`tools/cpphdl-emit-rtl`
  额外产出 `<out>/tb_<module>_stub.sv` —— **只发骨架**：
  端口清单注释块（来自 manifest）· 端口信号（输入 tie-off）· DUT 实例化 ·
  时钟/复位驱动 · VCD 开关 · **留空的激励/断言位**（`// TODO:`）。
  ⚠️ **为什么只发骨架**：激励与期望值是**领域知识**（UART 波特率 / AXI 握手 /
  PWM 占空比表）⇒ ⇒ 自动生成只能产出「编译通过但什么都没验」的 TB。
  新增 `include/rtl_emit/tb_stub.h` + `src/rtl_emit/tb_stub.cpp`（库侧，可复用）。
- **收尾约定**：补完的 TB MUST 打印 `RESULT: checks=<N> fails=0`
  —— 运行护栏（E5）按该行判红绿（**不靠退出码**：`$finish` 恒返回 0）。
- `scripts/update_test_count_anchors.sh`（**E8**）：**一次命令同步 5 个计数锚点**
  （`CTEST_TOTAL` / `VERILATOR_E2E_TOTAL` / `DUAL_BACKEND_PARITY_TOTAL` /
  `DUAL_BACKEND_PARITY_PAIRS` / `EXAMPLES_TEST_SOURCES`）· `--check`（CI 用）·
  `--dry-run`。⚠️ **只同步、不推导** —— 锚点本身是「静默缩面」的防线，必须留在
  文件里可 review；计数一律**复用既有脚本的口径**（不重算，避免第二套实现）。

### 口径（实测发现，已在代码注释登记）
- **manifest 的 `ports[]` ⊋ 产物的顶层端口表**：manifest 收全部
  `type_input`/`type_output`（含**子组件的有 driver input**）⇒ ⇒ 后者在扁平化后是
  **内部 wire**。TB 骨架因此需要过滤：
  `role ∈ {clock,reset}` ∪ `dir == output` ∪ `is_top_io`
  （与 F3 的端口契约对齐）。⚠️ 不过滤 ⇒ Verilator 报
  `Instance attempts to connect to 'X', but it is a variable`（实测踩中）。
  ⚠️ `openspec/specs/rtl-emit-portability/spec.md` **未规定** `ports[]` 的范围
  ⇒ ⇒ 「manifest == 产物端口表」这条判据**当前不成立**（B2 的 E1 需据此调整）。

### 测试
- `tests/test_rtl_tb_stub.cpp`（**22 断言 / 5 用例**）：只连顶层端口（内部 wire
  不得连线）· 骨架不含实际断言 · 收尾 `RESULT` 行 · 时钟/VCD 骨架 · 确定性（无时间戳）
- **判别力自证**：去掉顶层端口过滤 ⇒ 判据 1 转红 ✓（还原后重建复绿）

---

## 2026-11-11 — C6 返工（B1 验收 BLOCKER-1 + 4 项判据子项）

> 来源：`.rddf/reviews/b1-batch-verify-2026-11.md`（B1 验收 **不通过**）⇒ 修完再复审。

### Fixed
- **BLOCKER-1：剪枝误剪「仍被引用」的字面量 ⇒ 卫生产物无法编译**
  （实测：RV32I **扁平 + 层次化双双** `Can't find definition of variable`，exit=1；
  最小复现 = **一句 `select()`**）。
  **根因**：剪枝曾按「使用者**类型**」一刀切（假设 `op`/`mux`/`bitsupdate` 都内联常量）
  ⇒ ⇒ 实际 `print_mux` **按名字引用**、`print_binary_op` **只内联 RHS**
  ⇒ ⇒ 前提是假的。
  **修法**：新增 `is_literal_referenced_by(user, lit)` —— **逐使用者、逐位置**
  判定「文本里会不会出现这个名字」，与各 `print_*` 的实现对齐
  （`binary` 的 rhs · `concat` 两侧 · `rotate`/`bit_sel`/`bits_extract` 的 rhs ·
  `mux`/`bitsupdate` 不内联 · 不发射的 `op` 路径）。
- **剪枝的三条「非图边引用」保护**（每一条都是实测踩出来的）：
  ① 模块端口；② `body_epilogue`（跨 module 实例连接）+ **`port_drives_node` 的目标名**
  （驱动重定向）；③ **端口 `assign` 的终点 driver**（`print_logic` 的 walk 终点）。
- **walk 逻辑统一**：`print_logic` 的 output / input 分支与剪枝原先**各写一份**
  ⇒ ⇒ 三处**不一致**（input 只穿透未命名 proxy · output 穿透全部 · 重定向 proxy 停在自身）
  ⇒ ⇒ 抽出 `port_driver_endpoint()`，**三处共用** ⇒ 从此不会漂移。
- **子组件输入端口可剪**：扁平化后它是**内部 wire**（不是模块端口）⇒ ⇒ 无消费者时
  其声明与驱动 `assign` 都是死代码（实测 RV32I 最后 6 个零引用全是这一类）。

### Changed（判据补齐 · 全部仅在 `emit_hygiene` 开启时生效）
- **C6-③ 复位写法规范化**：`if (!reset) q <= next; else q <= INIT;`
  ⇒ ⇒ `if (reset) q <= INIT; else q <= next;`（逐拍语义等价，分支互换）
  · 两处：`emit_always_ff` 与 `print_reg` 的「无 next」分支。
- **C6-④ `lint_off` 收窄**：卫生模式下**不发**任何 blanket `lint_off`
  ⇒ ⇒ 用**显式 size cast**（`N'(expr)`）把隐式的宽度扩展/截断写成意图
  （新增 `hygiene_cast()`，接入 `print_binary_op` / `print_proxy` /
  `print_logic` 的 output+input 分支 / mem 读口复位值）
  ⇒ ⇒ **RV32I 的 23 条宽度告警归零**（19 `WIDTHEXPAND` + 4 `WIDTHTRUNC`）。
- **C6-① 补 `default_nettype wire`**：`` `default_nettype `` 是**编译指令**
  ⇒ ⇒ 宏状态**跨文件延续** ⇒ ⇒ 不恢复会让后续 include 的文件里所有隐式线网报错
  ⇒ ⇒ 文件末尾恢复（**每文件一次**，与头部 `none` 配对）。

### 护栏（C5）
- **F1 改为「两段式基线」且靶改为卫生产物**：段① 原样 lint（基线）· 段② 剥掉
  `lint_off` 后仍绿。⚠️ 原 F1 跑**默认产物**，其隐含前提「工具设计恰好无宽度告警」
  已破 —— 而**默认产物在真实设计上必然有 23 条宽度告警**（那正是 blanket 要盖的东西）
  ⇒ ⇒ 「剥掉 blanket 后干净」对默认产物是**不可满足**的判据。
- **F7 的 M4 从空操作改为真判定**（旧实现 `if 绿; then : # 只记录; fi` ⇒ 两条分支都不失败）
  ⇒ ⇒ 投毒 = 用 `lint_off` **盖住一条真实宽度告警** ⇒ 剥掉后 F1-b **必红**。
- **F8 新增「无未声明引用」**（`--undeclared-refs` · **不依赖 verilator**）
  ⇒ ⇒ BLOCKER-1 的**直接**判据，与 lint 门互为兜底。
- 工具自带设计加入 **mux 选字面量**形态 ⇒ ⇒ F8 成为 BLOCKER-1 的**常驻回归靶**。

### 验证
- **RV32I 卫生产物（扁平 + 层次化）**：零引用 **0** · 恒等直通 **0** ·
  **未声明引用 0** · verilator 告警 **0** · `--lint-only` **exit=0**
  （返工前：扁平/层次化均 `exit=1`，≥12 处 `Can't find definition`）
- **默认路径逐字节不变**（扁平 + 层次化的 RV32I 产物与 B1 前基线 `diff` 为空）
- **行为等价**：同一 testbench 驱动 `flat_std.v` 与 `flat_hyg.v`
  ⇒ ⇒ **200 拍 × 107 个输出端口逐拍完全一致**
- **判别力自证（3 条变异各自转红，还原后重建复绿）**：
  M1 恢复「mux 内联」⇒ 用例 5 红 · M2 去掉链上 proxy 种子 ⇒ 用例 6 红 ·
  M3 不发 `default_nettype wire` ⇒ 用例 7 红
- `tests/test_rtl_emit_hygiene.cpp` **38 断言 / 7 用例**

---

## 2026-11-11 — C6 产物卫生（`emit_hygiene` 开关）

### Added
- **`EmitOptions::emit_hygiene` / `HierarchyEmitOptions::emit_hygiene`** —— 产物卫生开关
  （`rtl-emit-product-tooling` **C6**）⇒ ⇒ **默认 `false`** ⇒ ⇒ **默认路径产物逐字节不变**
  （实测：RV32I SoC 的层次化产物与改动前**逐字节相同**）。
  ⚠️ **为什么 MUST 默认 off**：「层次化产物逐字节不变」这条 AC **只在**该开关默认 off 时成立。
  CLI 侧：`tools/cpphdl-emit-rtl --emit-hygiene`。
- 开启后做四件事（**全部可判别**，C5 的 **F8** 判据）：
  1. **`` `default_nettype none ``** + **确定性**元数据头
     （⚠️ **时间只进 manifest** ⇒ ⇒ 产物**不含时间戳** ⇒ ⇒ 仍然确定性）；
  2. **不发 blanket `lint_off`** ⇒ ⇒ 产物必须**自身** lint 干净
     （那两条 `WIDTHEXPAND` / `WIDTHTRUNC` 豁免不再靠一条全局注释盖住）；
  3. **剪零引用信号**（**不动点**：`dead(N) ⟺ 纯组合类型 ∧ N 的全部使用者也 dead`）
     ⇒ ⇒ 一趟做不到（实测：层次化路径一趟剩 **156** 个）⇒ ⇒ 不动点后扁平产物**归零**；
     ⚠️ 含**字面量内联**特例（`print_op` 把常量内联成 `5'h0` ⇒ 图里「有使用者」
     但**文本里是死的** ⇒ ⇒ 用「users 全死」规则永远剪不掉这类）；
  4. **proxy 恒等直通**（只被转发的 proxy 与零引用信号一并剪掉）。
  ⚠️ **两条保护 MUST NOT 剪**：① **模块端口**（外部可观测面）；
  ② 出现在**实例连接文本**（`.port(net)`）里的名字 —— 那是**跨 module** 的连接
  ⇒ ⇒ 图里**看不到这条边** ⇒ ⇒ 剪掉会**静默断线**（本项有专门的判别性用例）。

### Changed
- **无**（默认路径零变化 ⇒ ⇒ 既有 golden / 快照 / Verilator 缓存全部不受影响）。

### 已知残留
- ~~层次化产物仍有 12 个零引用 `ch_in`~~ ⇒ ⇒ **已于同日返工中消除**（见上方
  「C6 返工」条目：子组件输入端口在扁平化后是内部 wire ⇒ 无消费者即可剪；
  模块端口由 `view_.ports` 保护，不受影响）。

---

## 2026-11-11 — B1 批（端口命名地基 · N1 / N2 / N3 / P1 / C4 / C1 / C3 / C5）

### Added
- **`tools/cpphdl-emit-rtl`** —— **库 + 薄 CLI** 的 RTL 发射器（C1）。
  ⚠️ **形态**：`ch::rtl_emit::run_emit_cli()` 是**库**（`include/rtl_emit/cli.h`）⇒ ⇒
  用户在自己的可执行文件里**登记**自己的设计 ⇒ ⇒ 退出码 / 失败语义**全仓一致**；
  本仓的二进制只是**一个使用者**（+ 自测载体）⇒ ⇒ **MUST NOT** 链 `examples/**`。
  ⚠️ **退出码（fail-closed）**：**0** 成功 · **1** 运行失败或**存在诊断** · **2** 用法错误
  ⇒ ⇒ **任何异常都不得返回 0**。
- **`include/rtl_emit/manifest.h`** —— 端口清单（manifest）的**参考实现**
  （capability `rtl-emit-portability` 的 schema `rtl-emit-manifest-v1`）：
  10 个字段 + `diagnostics[]` + JSON 序列化 ⇒ ⇒ **端口按名字排序** ⇒ ⇒ **确定性**。
- **`scripts/verify_port_naming.sh`** —— 端口命名的**棘轮**护栏（N2）：
  product 侧（`include/**` + `examples/**`）逐端口判定 · 存量 **1484** 条登记在
  `.rddf/checks/port-naming-exempt.tsv` ⇒ ⇒ **债只许减不许增**。
  ⚠️ P1 额外判「**标识符 == 显式名字串**」⇒ ⇒ 框架**无反射** ⇒ ⇒ 运行期唯一存活的
  是**显式命名串** ⇒ ⇒ manifest 的 `cpp_member` 只能是它。
- **`scripts/verify_flat_rtl_emit.sh`** —— 扁平产物的**产物级**护栏（C5）：
  F1 剥掉 blanket `lint_off` 后重 lint · F2 确定性 · F3 顶层端口集 == 清单 ·
  F4 无悬空顶层输入 · F5 无自赋值死逻辑 · F6 backend 产物 == 产品产物（**当前不适用**）。

### Changed
- **显式命名的端口**的 Verilog 名**绕开** eval-list 序计数器（N3）⇒ ⇒ 端口名
  **对图的无关改动稳定** ⇒ ⇒ 「按名引用」的外部资产（TB / wrapper / 文档）不再**静默失配**。
  ⚠️ **零 churn**：现有端口**全部未具名** ⇒ ⇒ 产物**逐字节不变**（实测）。
  ⚠️ 冲突判定是**端口作用域**的局部判定（点名报告 + 退回计数器拿唯一名）⇒ ⇒
  **不**拒绝整份设计。

### Fixed
- ⚠️ **端口名冲突的误报**（N3 实施期实测）：输出端口的框架默认名是
  **`unnamed_output`**（不是 `unnamed_input`）⇒ ⇒ 第一版判据漏了它 ⇒ ⇒ 多个
  `unnamed_output` 被误判成「显式名」⇒ ⇒ 日志刷出假 ERROR ⇒ ⇒ 被
  `verify_example_test_wiring.sh` 的「跑但报错 ≠ 通过」判据抓到 ⇒ ⇒ 已修。

---

## 2026-11-11 — `rtl-emit-manifest-schema-2026-11`（B1 批 · C4 · 纯规格）

### Added
- **端口清单（manifest）schema `rtl-emit-manifest-v1`** —— 新 capability `rtl-emit-portability`
  的第一条 Requirement（change `rtl-emit-manifest-schema-2026-11`）。
  字段（10 个，零缺失）：`name` / `dir` / `width` / `role`（`clock`·`reset`·`data`）/
  `driven` / `effective` / `owner` / `is_top_io` / `cpp_member` / `semantic`（v1 恒 `null`）。
  - ⚠️ **`driven`（**节点级**「是否被消费」）与 `effective`（**功能级**「是否产生可观测行为」）
    是两个独立字段** —— 「有连线但无功能」（标准样例 = `ext_clk`：接到 `pipeline.io().clk`，
    而流水线内对 `io().clk` 零消费者）这一类**单布尔表达不了**。
  - ⚠️ `is_top_io` 的筛选是**消费方的选择**，MUST NOT 成为发射器的**默认行为**。
  - **库代码零改动** ⇒ ⇒ **端口面、端口顺序、命名规则一字未改**。

### Changed
- **无**（本 change 只新增规范，不改任何既有 Requirement）。

---

## 2026-11-11 — `rv32i-dead-component-consolidation`（B0 批）

### Removed
- **`riscv::ForwardingUnit`**（`include/cpu/pipeline/rv32i_forwarding.h`）——
  零交付路径消费者 + 独有语义为空集（WB 档无覆盖 · `id_reg_read` 死输入）。
  改用仓内权威的 `riscv::HazardUnit`（已接入真实流水线）+ `chlib::ForwardingMux`。
- **`riscv::HazardDetectionUnit`**（`include/cpu/pipeline/hazard_detection_unit.h`）——
  零交付路径消费者 + 其 MEM 级 load-use 检测点在流水线里**永不触发**。改用 `riscv::HazardUnit`。
- **`riscv::HazardUnitComplete`**（`include/cpu/pipeline/rv32i_hazard_complete.h`）——
  零交付路径消费者 + 2 个死输入 + 3 档语义；唯一存在理由是 parity 夹具。改用 `riscv::HazardUnit`。

> ⚠️ **这是显式批准的覆盖缩面**（用户 2026-11-11 批准）：
> `DUAL_BACKEND_PARITY_PAIRS` 290→284 · `DUAL_BACKEND_PARITY_TOTAL` 66→65 ·
> `VERILATOR_E2E_TOTAL` 77→76 · `CTEST_TOTAL` 257（增删相抵）。

### Changed
- **`ch::build_hierarchical_name()` / `ch::Component::name()` 的子路径拼接不再按父路径
  的「字面值」剥离路径段**（`node-name-prefix-consistency` P1）。唯一受影响的情形：
  根组件的 name 为空（归一成 `"unnamed"`）⇒ ⇒ 其子组件的 `name()` 从裸局部名
  （`"kid"`）变为带父前缀（`"unnamed.kid"`）⇒ ⇒ **按 `name()` 精确匹配裸局部名的下游
  代码 MUST 更新**；常规路径（父名非空）**逐字节不变**。
  ⚠️ **已登记代价**：空名与用户真名 `"unnamed"` 在 `name()` 上**永久不可区分**
  ⇒ ⇒ 彻底解决须另立 item。
- **顶层 RTL 端口表（RV32I SoC）：顶层 input 8 → 7**（`rv32i-dead-component-consolidation` C8）。
  `mem_is_load` 的装配点补上 driver ⇒ ⇒ 它不再是「**无 driver 的** input」⇒ ⇒ 按契约
  不再出现在顶层端口面。**行为零变化**（该路在流水线里永不触发）。
  下游若按端口**数量/序号**硬编码 ⇒ ⇒ MUST 改为**按名**取。

### Fixed
- **`riscv::Rv32iAlu` 的 SLTU 是无符号比较**（此前 `bits<31,31>(sub_result)` 取的是
  **结果位**而非**借位位**）⇒ ⇒ `0 <u 0xFFFFFFFF` 曾得 **0**（期望 1）。
- **`sanitize_verilog_name` 的关键字表补全为 IEEE 1800-2017 保留字全集（246 词 ·
  verilator 实测）**（`port-naming-ratchet` N3）。此前 `bit` / `time` / `event` / `table`
  等**合法 C++ 成员名会产出非法 Verilog**（`logic bit;` ⇒ 语法错误）。
  **实测对现网产物零影响**（8585 个标识符中 0 个与关键字完全相等）。

---

## 2026-10-15 — `verilator-hier-input-flattening-fix`

### Fixed
- **`VerilatorBackend` now propagates driven submodule `ch_in` ports
  end-to-end.** `codegen_verilog.cpp::print_header` previously flattened
  driven submodule `ch_in`s (anything with `inputimpl::driver() != nullptr`,
  e.g. `sub.io().x <<= some_wire`) to top-level Verilog `input` ports that
  nothing drove. The matching `print_logic` switch had no `type_input`
  branch, so there was no `assign` to feed the driver value into the
  Vtop field. Verilator initialised those ports to X (=0 after the
  `initialize()` reset pulse), silently disabling every `ch_in` below
  the top component. After the fix:
  - `print_header` filters out driven inputs from the top-port list.
  - `print_decl` declares them as internal `logic` (via `emit_signal_decl`).
  - `print_logic` adds a `type_input` branch that walks the unnamed-proxy
    chain and emits `assign <in> = <driver>;` (mirroring the `type_output`
    branch).
  - `VerilatorBackend::emit_sim_main_postlude` skips them in
    `set_input_Vtop` / `get_field_ptr_Vtop` (the Vtop field does not
    exist), and `build_port_access_table` does not add them to
    `port_access_` so `sync_inputs_to_vtop` will not try to write them.
  JIT already handled this correctly (`src/jit/jit_ir_gen_lnode_io.cpp:31-46`).
  This brings the Verilog codegen path into parity with JIT semantics.

### Resolved (was a known limitation of `verilator-issue-25-fix`)
- The `verilator-issue-25-fix` entry below mentioned that
  "RVALID / RDATA readback via `data_map_` returns 0" and was tracked
  under issue #25 / option A in the Oracle audit. That symptom was
  actually the same hierarchical-input bug, not a separate issue #25
  register-propagation problem: in `test_axi_lite_verilator_e2e.cpp`,
  `slave.io().awaddr <<= io().awaddr` etc. flatten `slave.io().awaddr`
  to a top-level `input` port the testbench never drives, which then
  propagates through to `rvalid`/`rdata` via the `always_ff` busy
  latch. With the codegen fix, the AXI4-Lite WriteReadReg0ThroughBackend
  fixture now asserts `rvalid == 1 && rdata == 0xDEADBEEF` as a real
  REQUIRE, and the RV32I 3-instruction parity fixture runs
  `perf_instructions == 12` on both interpreter and verilator (was 0).

### AGENTS.md
- New VERILATOR BACKEND RULE: hierarchical input driver MUST be emitted
  by codegen. The four-site contract is now enforced (filter, declare,
  assign, backend skip). Reflects the actual root cause of the
  previously-tracked symptom.

---

## 2026-09-23 — `verilator-issue-25-fix`

### Fixed
- **`VerilatorBackend::eval_sequential` performs a 2-step clock toggle** so
  Verilator's `VlClockSig`/`__Vclklast__` edge detection fires
  `always_ff @(posedge clk)` blocks. The clock Vtop field is written
  directly via the new `clock_field_ptr_` (NOT round-tripped through
  `data_map_`); `sync_inputs_to_vtop(exclude_clock=true)` skips clock/reset
  on the input pass, and `sync_outputs_from_vtop()` skips clock/reset on
  the output pass (they are infrastructure, never user-visible). Before
  the fix, `always_ff` blocks never fired — counter-style regressions
  read 0 even though `eval()` was called, AXI4-Lite `awready`/`wready`/
  `bvalid` combinational handshakes did not propagate to `data_map_`, and
  sequential `rdata` reads returned 0. After the fix, the e2e tier
  (`test_verilator_e2e_harness`, `test_axi_lite_verilator_e2e`) passes with
  `REQUIRE` assertions, and TC-07 depth=1000 verilator runs at
  ~580 ticks/sec (was ~34 ticks/sec in the broken baseline). Also fixed
  `emit_sim_main_postlude` (`src/core/verilator_backend.cpp`) to use the
  codegen's `get_verilog_name(node)` instead of `cpp_safe_name(node->name())`
  — the codegen applies a `_N` uniqueness suffix that `lnode->name()`
  does not carry, and without the fix the generated accessor code had
  `&v->top.slave.unnamed_input` (with literal dots) which verilator
  rejected as a hard compile error.

---

## 2026-09-22 — `verilator-perf-three-way`

### Fixed
- **`get_literal_str` masks literal value to target width.**
  `src/codegen_verilog.cpp::verilogwriter::get_literal_str` (single-arg
  overload, `src/codegen_verilog.cpp:142-166`) now masks `value` to
  `width` bits before emitting the SystemVerilog `<width>'h<hex>` literal.
  Previously, the function trusted `lit.bv_.size()` (the reported width)
  but emitted the raw `lit.bv_.words_[0]`, which `bitvector::operator=`
  does not mask (`include/bv/bv_assign.h:60-70`). Constructs like
  `ch_uint<8>(999)` therefore reported `width=8` while carrying 999, and
  the codegen emitted `8'h3e7` — a hard SystemVerilog syntax error
  (`Too many digits for N bit number`). Affected the three-way perf
  benchmark TC-07 (XOR chain) at `depth=1000`, where hundreds of
  intermediate `ch_uint<8>` literals produced 744 verilator errors and
  broke the Verilator compile. After the fix, TC-07 depth=1000 compiles
  cleanly and the Verilator row reports `34.54 ticks/sec` (was 0.00).
  `width >= 64` path is preserved (`ADR-035` wide-signal rejection
  caps port widths at 64; any wider literal would have been rejected
  upstream).

### Added
- **`perf_three_way` ctest entry** for CI-friendly three-way perf
  smoke tests. Runs TC-07 (XOR chain depth=10/100/1000) and TC-08
  (register chain) via `perf_tests --tc=07 --tc=08 --report=json` in a
  subprocess-isolated execution context (F2 perf subprocess isolation).
  TIMEOUT 360s, LABELS `perf;verilator`. The full `--all` perf suite
  remains available via the `perf_tests` ctest entry (TIMEOUT 1800).
  See `tests/benchmark/CMakeLists.txt:103-128`.

### Test coverage
- **`tests/test_verilator_three_way.cpp`** (391 lines, 12 test cases
  / 28 assertions, all PASS) covers:
  - Boundary values: `ch_uint<8>(1)` → `8'h1`, `ch_uint<8>(0)` → `8'h0`,
    `ch_uint<8>(255)` → `8'hff`, `ch_uint<8>(256)` → `8'h0` (mask drops
    high bit), `ch_uint<8>(999)` → `8'he7` (the regression case).
  - Multiple widths: 8, 16, 32, 64.
  - `print_concat` path (concatenation RHS literal).
  - `zext<32>` path (zero-extend output literal).
  - Random fuzz: 100 trials of random `(width, value)` pairs asserting
    no `'<width>'h<overflow-hex>` literal is emitted.
  - TC-07 end-to-end codegen for depth=10/100/1000.
  - Real `verilator --lint-only` integration (skipped if verilator not
    on PATH).

---

## 2026-09-21 — `adr-035-phase-3-complete`

See `openspec/specs/verilator-backend-e2e/spec.md` and
`openspec/specs/jit-llvm-compat-tests/spec.md` for the archived
specifications.

---

## 2026-09-22 — `axi4lite-verilator-e2e` (Gap F2 closure)

### Added
- **`tests/test_axi_lite_verilator_e2e.cpp`** (268 lines, 2 test cases /
  9 assertions) closes Gap F2 from the `verilator-perf-three-way`
  review: `examples/axi4/axi4_lite_example.cpp` runs only through the
  interpreter — no VerilatorBackend e2e coverage existed for any AXI
  class. Two test cases:
  - `AxiLiteTopPortAccessSnapshot`: VerilatorBackend compiles an
    `AxiLiteTop` fixture (wraps `AxiLiteSlave<32,32,4>` via
    `ch::ch_module`), dlopens `libVtop.so`, and verifies
    `port_access_` is populated with all 17 AXI4-Lite IO signals
    (10 inputs + 7 outputs + clock + reset).
  - `WriteReadReg0ThroughBackend`: dispatches through Simulator +
    VerilatorBackend, performs a write transaction (AW + W →
    `reg0 = 0xDEADBEEF`) and a read transaction (AR → R), and logs
    `RVALID` / `RDATA` for nightly inspection.

### Known limitations carried forward (issue #25)
- ~~**`RVALID` / `RDATA` readback via `data_map_` returns 0 even though
  the generated Verilog has pure `assign top_slave_unnamed_output =
  top_slave_mux_select_1;` (combinational).** Originally Oracle
  audit (C12) suggested this would propagate; in practice it does
  not, indicating issue #25 extends beyond `always_ff` reg updates to
  the **entire `sync_outputs_from_vtop` path**. Strong evidence that
  the VerilatorBackend eval-after-input-sync sequence does not
  refresh wire outputs. Resolution tracked under
  `verilator-issue-25-fix` (proposed next change, option A in the
  Oracle audit).~~ **Resolved 2026-10-15 by
  `verilator-hier-input-flattening-fix`**: the symptom was actually the
  hierarchical-input flattening bug (driven submodule `ch_in` ports
  became top-level input ports that nothing drove). Once the codegen
  emits `assign` for those ports, `RVALID` / `RDATA` propagate normally
  through the existing `sync_outputs_from_vtop` path. No changes to
  the sync machinery were needed.
- ~~Test case 2 uses `INFO(...)` only for the readback values; no
  `CHECK` is asserted on `RVALID` / `RDATA` until the underlying
  Verilator eval-eval-sync issue is fixed.~~ **Resolved 2026-10-15**:
  `test_axi_lite_verilator_e2e.cpp::WriteReadReg0ThroughBackend` now
  asserts `REQUIRE(rvalid == 1)` + `REQUIRE(rdata == 0xDEADBEEF)`.

### Test invocation
- Tagged `[verilator][e2e][axi4lite][m5]`. PR matrix (no verilator
  binary) SKIPs both cases via the standard `tool_available("verilator")`
  gate. Nightly Verilator CI (`.github/workflows/ci.yml`) runs them
  for real.

### Files
- `tests/test_axi_lite_verilator_e2e.cpp` (new)
- `tests/CMakeLists.txt` (+5)

---

## 2026-11-09 — 用户可见变更载体上线（`public-api-remove-visibility`）+ `chlib-dtor-composability-2026-11`（含 2026-10-15 以来的一次性回填）

> ⚠️ **本条是「载体机制」上线**（issue #28 的处置）：本仓的用户可见变更此前**只**
> 记录到 **2026-10-15** ⇒ ⇒ 而此后 **122** 个 change 已归档（截至 2026-11-09 实测 ⇒
> `ls openspec/changes/archive | awk 'substr($0,1,10)>="2026-10-16"' | wc -l`）⇒ ⇒ **破坏性变更对
> 下游不可见**（真实后果：下游 ChipForge 在 2026-11-08 才发现
> `ch_state_machine::is_built()` 已被移除 ⇒ ⇒ 编译失败 ⇒ ⇒ 上游 issue #28）。
> ⇒ ⇒ **机制**（R1）：**每个归档 change MUST 携带一行「用户可见变更」**
> ⇒ ⇒ 载体 = 归档件的 `## 用户可见变更` 段 + 本文件（变更级摘要）
> ⇒ ⇒ **符号级**明细 = `.rddf/checks/public-api-surface.md` 的
> 「已移除 / 已改名的公开符号」一节。

### Removed（**破坏性** · 跨仓下游 MUST 迁移）

- **`chlib::ch_state_machine` 的 15 个死 / 误导 API**（ADR-050 D2/D3 · `c7b0b28`）：
  `is_built()` · `on_exit` / `exit_action` · `on_entry` / `entry_action` ·
  `then()` / `next_state_on_exit()` / `has_exit_transition()` · `state_changed` ·
  `tick_state()` · `CH_STATE_MACHINE` / `CH_STATE` / `CH_STATE_MACHINE_BUILD` 宏 ·
  `state_def::parent`（+ `friend`）。
  ⇒ ⇒ **改名**：`current_state()` → **`entry_state()`**（旧名返回**编译期** entry
  ⇒ ⇒ 运行时状态用 `current_state_uint()` / `is_in()`）。
  ⇒ ⇒ **符号级迁移指引** = `.rddf/checks/public-api-surface.md` 的移除登记表。
- **`get_lnode` 的「无约束通用入口」②**（ADR-053 决策 C）：改用 bundle 自带的
  `impl()`（约束偏序下**恒胜** ⇒ ⇒ 爆炸半径 0）。
- **`include/cpu/cache/*_stub.h`**（pass-through stub 族 · ADR-037 · 2026-10-20）：
  改用 `i_cache_{sync,async}.h` / `d_cache_{sync,async}.h`。
- **`bundle_concat` / `bundle_slice_view`**（ADR-043 · 2026-10-22）：两者当时均为
  **静默失效**（前者字段不在位布局里、后者根本不切片）⇒ 改用 bundle **字段直连**
  （ADR-042 之后字段是一等节点）。迁移指引见 `.rddf/checks/public-api-surface.md`。
- **`conditional_reg_assign`** → **改名** `conditional_assign`（ADR-051 · 2026-10-30）：
  旧名暗示「只对寄存器」，而输出端口同样需要条件化。
- **cache wrapper 的死数据 / 死配置面**（ZERO-DEBT · phase-6 **P9** · 2026-11-09）：
  `include/cpu/cache/d_cache_write_buffer.h` 的 `WriteBufferEntry` / `WriteBufferConfig`，
  `include/cpu/cache/i_cache_prefetch.h` 的 `prefetch_data` / `prefetch_addr` /
  `prefetch_valid` / `PrefetchConfig` ⇒ ⇒ **零消费者**的死数据 ⇒ ⇒ 下游若引用 MUST **自建**；
  ⇒ ⇒ 两个 wrapper 的 `__io` 签名与**可观测行为不变** ⇒ ⇒ **真 FIFO 写缓冲 / 真预取 = 独立立项**。
  迁移指引见 `.rddf/checks/public-api-surface.md`。

### Added

- **`chlib::ch_state_machine` 的「可组合形态」**（ADR-067 · `chlib-dtor-composability-2026-11`）：
  新增第三模板实参 `chlib::dtor_detection`（默认 `automatic` ⇒ **既有用户零改动**）
  ⇒ ⇒ `dtor_detection::explicit_only` 的析构为 `noexcept` ⇒ ⇒ **该类型可作
  「有虚析构的类」的按值成员**（修掉下游的编译失败）⇒ ⇒ 检出改走
  **显式 `check_unresolved()`（可捕获）+ 析构期终止兜底（fail-closed）**。
- **`chlib::ch_state_machine::contrib()` / `resolve_hold()` / `resolve_or()`**
  （ADR-050 D4）：状态守卫贡献 API（`include/chlib/state_machine.h`）。

### Deprecated（**尚未移除**）

- `ch_bool` / `port<ch_bool>` 上的 `&&` / `||`（ADR-036）⇒ ⇒ 改用 `&` / `|`；
  ⚠️ **移除窗口 = 日历锁：最早 2027-04-17**。

### Changed（契约层）

- `chlib::conditional_block` 的析构 MUST 为 `noexcept(false)`
  （ADR-051 · `chlib-conditional-semantics`）⇒ ⇒ **「析构非 `noexcept` 的类型」
  MUST NOT 作为「有虚析构的类」的按值成员或基类**（ADR-067 决策 ④）。
- `chlib::ch_state_machine` 的析构期检出 MUST 可捕获（ADR-050 D5）⇒ ⇒ **默认形态
  逐字保留**；「可组合形态」的检出 MUST 是 **fail-closed** 的（ADR-067 决策 ②）。
- **stream 组合子的 ready 语义**（下游若依赖旧行为 MUST 复核）：`stream_demux` 的 ready
  方向改为**消费端 ⇒ 上游**（ADR-056 · 2026-11-01，与 `stream_mux` 对称）· `stream_fork`
  同步语义为 `input.ready = AND(各输出 ready)`，`synchronous=false` **响亮拒绝**
  （ADR-057 · 2026-11-01）· 组合子的 ready **所有权**形态 = ADR-054（2026-10-31 修订）。
- **`stream_width_adapter` 重写**（ADR-064 · 2026-11-03）：三族 adapter 的语义与
  命名对齐（详见 `.rddf/checks/chlib-naming-honesty-registry.md`）。
- **节点命名 / 归属策略**（ADR-059 · 2026-11-02）：生成的 Verilog 中**子模块端口名**
  与节点归属按「构造作用域」修正 ⇒ ⇒ 依赖**旧生成名**的下游脚本需要复核。

---

## Format

Entries are reverse-chronological. Each entry may have sections:
**Added**, **Changed**, **Deprecated**, **Removed**, **Fixed**,
**Security**. The opening line of each entry is the date and the
OpenSpec change name (or external release tag if no change is in
flight).
