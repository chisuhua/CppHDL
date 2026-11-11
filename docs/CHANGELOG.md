# CppHDL Changelog

User-visible changes to the CppHDL HDL library. Entries are grouped by
the change that introduced them (see `openspec/changes/` and
`openspec/specs/`).

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
