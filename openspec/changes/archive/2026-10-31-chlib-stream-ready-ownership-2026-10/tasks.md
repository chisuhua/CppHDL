# Tasks: chlib-stream-ready-ownership-2026-10

> 实施 **ADR-054**（`docs/adr/ADR-054-stream-ready-ownership-form.md`）的**形态 E**
> （默认值 + 可覆盖）。
> ⚠️ 期望值 MUST **手推**；变异 MUST **先确认编译成功**（编译期判据除外 ⇒ 如实登记）。
> ⚠️ 判据 MUST 是**效果判据**（MUST NOT 词法）⇒ ⇒ 否则「合法的默认恒就绪」被误判
> ⇒ ⇒ 判别力归零。
> ⚠️ 护栏自检 MUST **原地投毒**（投毒副本不会被 include ⇒ 判据恒绿）。

## 0. 订正与固化

- [x] **0.1** `design.md` §12.4 / §13.3 / §13.4 / §13.5 / §13.6 **订正**
      （§13.3「修法实测仍不传播」= **把探针输出读反了** ⇒ ⇒ 形态 E **第 4 次尝试即成功**）
      + 追加 **§14**（形态 E 打通的实测证据 + 写使能承重实测）
- [x] **0.2** 本文件改写为**形态 E**（初版全是形态 B 的「延迟构造」条目 ⇒ ⇒ 与 ADR-054 冲突）
- [x] **0.3** 提交形态 E 的**核心实现**（`include/chlib/stream.h`）⇒ ⚠️ **需用户确认后提交**

## 1. 实现

- [x] **1.1** `StreamFifoResult` MUST 持有**默认 ready 源**（`default_ready_source`）
      ⇒ ⇒ **源持久有效**（MUST NOT 用局部变量 / 临时字面量）
- [x] **1.2** 默认驱动 MUST 用 **`<<=`** 打在 `pop_stream.ready` 的 **field proxy** 上
      ⇒ ⇒ **目标非 literal**（MUST NOT `operator=` 重绑定 ⇒ ⇒ literal 不可被 `set_src`）
- [x] **1.3** 内部 FIFO 的**读使能 MUST 引用「字段节点本身」**
      ⇒ ⇒ 否则覆盖不传播 ⇒ ⇒ **静默失效**
- [x] **1.4** **写使能 MUST 只用 `input_stream.valid`**（⚠️ **承重**：实测恢复旧写法
      ⇒ ⇒ `Invalid operands` ⇒ ⇒ 组合子完全不可用）⇒ ⇒ ⚠️ **行为变更** ⇒ ⇒ 单独 commit
- [x] **1.5** `stream_fifo_connect_ready(result, consumer_ready)`：形参 MUST **模板化**
      + `to_operand` 归一化（⚠️ `const ch_bool&` ⇒ ⇒ `ch_in<ch_bool>` 端口绑不上
      ⇒ ⇒ ADR-053 / E3 同型教训）
- [x] **1.6** **多驱动守卫**：`connect_ready` MUST 断言「当前驱动仍是默认节点」
      ⇒ ⇒ 否则 MUST **`CHFATAL_EXCEPTION`**（⚠️ `CHERROR`/`CHFATAL` **只打日志不抛**
      ⇒ ⇒ ADR-050 已踩）
- [x] **1.7** `stream_queue` 别名同步（⇒ ⇒ 需 `stream_queue_connect_ready`）
- [x] **1.8** `StreamBuilder::queue<DEPTH>()`（`include/chlib/stream_builder.h:56`）的
      **反压暴露方式**（⚠️ 链式 API 无处插 `connect_ready` ⇒ ⇒ **待裁决**）
- [x] **1.9** `stream_mux_connect_ready` 的连接手段 `operator=` ⇒ **`<<=`**
      （ADR-045 决策 0-B 第 2 条 ⇒ ⇒ 字段**孤儿化** ⇒ ⇒ 静默）

## 2. 行为证据（⚠️ `stream_fifo` 至今**零行为覆盖** ⇒ ⇒ MUST 有可执行证据）

- [x] **2.1** `tests/chlib/test_stream.cpp`（**既有** target ⇒ ⇒ **计数锚点不变**）
      新增 `stream_fifo` 行为用例，**4 组手推金标**：
      - **2.1.a** `rready=0` ⇒ 写 4 个 ⇒ `full == 1` **且** 队首**不动**（停在 `0x11`）
      - **2.1.b** 第 5 个写 ⇒ `full` 仍 `1` **且** 队首仍 `0x11`（**自保护**）
      - **2.1.c** `rready=1` ⇒ 队首 `0x22` ⇒ 再一拍 `0x33`（**恢复弹出**）
      - **2.1.d** **不调** `connect_ready` ⇒ 队首**逐拍推进**、`full` **恒 0**
        （= **旧语义对照** ⇒ ⇒ 钉住「零破坏」承诺）
- [x] **2.2** **多驱动**用例（调用方**已自行驱动** ready ⇒ ⇒ `connect_ready` MUST 抛异常）
      ⇒ ⚠️ **必需**（否则删掉守卫也看不出来 ⇒ ⇒ 变异 M3 恒绿）
- [x] **2.3** 写使能变更的**独立**用例（`wren` 由 `valid & ready` ⇒ `valid`）
- [x] **2.4** ⚠️ **双后端 parity 本次【不】纳入**（**决定**：避免一次动 3 个计数锚点
      ⇒ ⇒ `VERILATOR_E2E_TOTAL` 保持 **38** ⇒ ⇒ 独立立项）（⇒ ⇒ `VERILATOR_E2E_TOTAL` 锚点
      保持 **38** ⇒ ⇒ 避免一次动 3 个计数锚点）⇒ ⇒ 独立立项

## 3. 归位 2 个 disabled sample

- [x] **3.1** `stream_component_example`：迁移到新 API + 修 **4 处** `ch_in` 直写
      （`:44-47` / `:78-80` / `:119` ⇒ ⇒ **逐处核实端口方向**）
- [x] **3.2** `stream_handshake_example`：迁移到新 API + 修 **2 处** `ch_in` 直写
      （`:92` / `:131`）
- [x] **3.3** `samples/disabled_samples.txt` 移除这 2 行（3 → 1）+ 更新判定基准
- [x] **3.4** `samples/CMakeLists.txt` 的 `CH_SAMPLE_TARGETS` 补 2 个（37 → **39**）
      ⇒ ⇒ samples 护栏**数量守恒**

## 4. 护栏

- [x] **4.1** `scripts/verify_stream_ready_ownership.sh`（**新建**）：
      R0 fail-closed（fixture 缺失 ≠ 编译失败）· R1 **效果**判据（反压阻止推进）·
      R2 **原地投毒** · R3 调用点（调 `stream_fifo` 后 MUST 有 `connect_ready`
      **或** 对 `pop_stream.ready` 的 `<<=` ⇒ ⇒ **或**显式豁免标记 ⇒ ⇒ 豁免**非静默**）·
      R4 多驱动 · R5 `full` 可达 · R6 **结构**（MUST NOT `operator=` 驱动 ready 字段）
- [x] **4.2** `--self-test`（**原地投毒** + `trap` 还原 + **还原后 MUST 再确认一次绿**）
- [x] **4.3** CI 挂载（主路径 + `--self-test`）

## 5. 验证

- [x] **5.1** **变异 3/3**（每条**先确认编译成功**）：
      M1 读使能改引用**局部常量节点**（⇒ ⇒ 钉住「覆盖不传播」这条承重依赖）·
      M2'' 字段连接 `<<=` ⇒ `operator=`（⇒ ⇒ 钉住「连接手段 MUST `<<=`」）·
      M3 删掉双驱动守卫（⇒ ⇒ **依赖 2.2 的真实双驱动用例**）
- [x] **5.2** `ctest -LE perf` 零回归 · samples / examples 零回归
- [x] **5.3** 全部护栏（含 `--self-test`）exit 0 · **计数锚点不变**
      （`CTEST_TOTAL` **205** · `VERILATOR_E2E_TOTAL` **38** ·
      `DUAL_BACKEND_PARITY_TOTAL` **29** · `DUAL_BACKEND_PARITY_PAIRS` **184** ·
      non-perf **194** · `EXAMPLES_TEST_SOURCES` **17**）

## 6. 归档

- [x] **6.1** 归档（**禁** `--skip-specs`）⇒ 合并 `chlib-stream-ready-ownership` ADDED ×4
- [x] **6.2** 归档后 MUST 补写主 spec 的 `## Purpose`（archive **不合并** delta 的 Purpose）
- [x] **6.3** ⚠️ **写使能变更的契约化**：它是**行为变更** ⇒ ⇒ MUST 落进
      `chlib-stream-semantics`（MODIFIED）或新 capability 的 NOTE ⇒ ⇒
      ⚠️ `MODIFIED` delta MUST **逐字保留**全部既有 Scenario
      ⇒ ⇒ ⚠️ **Scenario 标题也 MUST NOT 改**（CLI 按标题匹配）

## 7. architect 决策（⚠️ builder 不得单方面改已签批 ADR）

- [x] **7.1** 撤销 ADR-054 的「**已裁决 · 【待验证】**」标注（现已**验证成立**）
- [x] **7.2** ADR-054 §3.2 第 1 条**补一句**：默认驱动的**源 MUST 活得比结果对象久**
      ⇒ ⇒ 落地形态 = **结果对象持有源节点**（`default_ready_source`）
      ⇒ ⇒ MUST NOT 指向**局部**变量 / **临时**字面量
- [x] **7.3** capability delta 是否吸收「写使能」条款（见 6.3）
