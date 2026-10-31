# Tasks: stream-forward-ready-closure-p1a-2026-11

## C1：契约侧

- [x] **T1.1** R1 的「转发类 MUST NOT 被读作『已合规』」Scenario **正文原地订正**
      （⚠️ **标题 MUST NOT 改**）⇒ ⇒ `demux` / `fork` 仍**未**收口 ⇒ ⇒ 该
      Scenario **继续在约**
- [x] **T1.2** ADDED「转发类组合子的 ready 所有权 MUST 是默认值 + 可覆盖」
      + 5 个 Scenario（上游真收到反压 · 未覆盖时恒就绪 · `connect_ready` 幂等 ·
      MUST NOT 硬编码常量 ready · 死路转活路）
- [x] **T1.3** `bash scripts/verify_openspec_delta.sh` ⇒ exit 0
- [x] **T1.4** `openspec validate … --type change --strict` ⇒ **valid**
      ⇒ ⇒ ⚠️ **正文 MUST ≤500 字符**（CLI 1.14.3 规则；⚠️ 仓库基线 21 个失败
      ⇒ ⇒ **MUST NOT** 新增失败）

## C2：ADR-054 修订（**architect 权限 · 用户已批准代改 ⇒ 加注「角色越权执行」**）

- [x] **T2.1** §3.2 第 2 条：mux / arbiter 类由「**两阶段**」改为「**形态 E
      （单阶段）+ `connect_ready` 为幂等便利入口**」+ 记「用户 2026-11-01 裁决」
      + 记「零破坏」的实测依据（现状 = 死路）
- [x] **T2.2** §3.2 第 3 条（长期方向）**保持不变** ⇒ ⇒ ✅ 已核对：
      **未改动**（⇒ ⇒ 「未连接 ⇒ 响亮失败」仍是长期方向 ⇒ ⇒ 不受本次影响）
- [x] **T2.3** §5 Verification ⇒ ⇒ 补转发类的判据（**效果判据**）
- [x] **T2.4** §4 后果 ⇒ ⇒ 补「转发类收口」条目 + 「公开 API 变更（结果 struct
      加成员）」

## C3：实现（**逐个组合子** ⇒ ⇒ 每改一个就编译 + 跑用例）

- [x] **T3.1** `stream.h`：`stream_mux` 内建默认驱动（**具名的局部** `ch_bool`
      源 ⇒ ⇒ 探针 ⑩ A 实测：**可被覆盖** ⇒ ⇒ ⇒ **零公开 API 变更** ⇒ ⇒
      ⇒ ⇒ **无需**给 `StreamMuxResult` 加成员）+ **内部**建立
      `input_streams[i].ready <<= (is_selected & output_stream.ready)`
      ⇒ ⇒ ⚠️ **删掉** `out_ready` 局部常量（mux 本来就没有，但 `connect_ready`
      里的 `output_stream.ready` 引用 MUST 保持）
      ⚠️⚠️ **2026-11-01 critic BLOCKER 订正**：原表述「加成员」⇒ ⇒ ⇒ 已改
      ⇒ ⇒ ⇒ ⇒ ⇒ **「节点是否持久」才是判据**（「持有者是谁」不是）⇒ ⇒ ⇒ ⇒
      ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ 局部具名 `ch_bool` 与「结果对象持有成员」**行为等价**
- [x] **T3.2** `stream.h`：`stream_mux_connect_ready` ⇒ ⇒ **幂等化**
      （重复调用不报错）+ **源为空抛异常**（`CHFATAL_EXCEPTION`）
- [x] **T3.3** `stream.h`：`stream_join` 内建默认驱动（**具名的局部** `ch_bool`
      源 ⇒ ⇒ 同 T3.1 ⇒ ⇒ **零公开 API 变更**）+ `input_streams[i].ready <<=
      output_stream.ready`
- [x] **T3.4** `stream.h`：filters×6（`throw_when` / `take_when` / `halt_when` /
      `translate_with` / `map` / `transpose`）⇒ ⇒ `=` ⇒ `<<=` + 建默认驱动
- [x] **T3.5** `stream.h`：`stream_combine_with` ⇒ ⇒ `=` ⇒ `<<=` + 建默认驱动
- [x] **T3.6** `stream.h`：`stream_priority_arbiter` ⇒ ⇒ 删 `out_ready` 局部
      常量 + 建默认驱动 + `input_streams[i].ready <<= (...)`
- [x] **T3.7** `stream.h`：`stream_arbiter_round_robin` ⇒ ⇒ 同 T3.6
      ⇒ ⇒ ⚠️ **RR 轮转本身**属 P2 ⇒ ⇒ **本次只修 ready 链**
- [x] **T3.8** `stream_arbiter.h`：`stream_arbiter_lock` / `stream_arbiter_sequence`
      ⇒ ⇒ 同 T3.6（**arbiter×4** ⇒ ⇒ 其中 2 处在此文件）
- [x] **T3.9** ⚠️ **已移出本 change**（2026-11-01 实测后的裁决）
      ⇒ ⇒ **实测**：`sync_fifo` 的**写口自保护**（`chlib/fifo.h:74`
      `will_write = wren & !is_full(count)`）⇒ ⇒ ⇒ **FIFO 没有「写口 ready」
      输出** ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 上游回接在当前 fifo 语义下「无路可走」**
      ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 修法需改 `build()` 语义 + 与 ADR-054 §3.2 第 1 条
      （读使能 = 消费端 ready，可覆盖）**存在张力** ⇒ ⇒ ⇒ ⇒ ⇒ ⇒
      ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 需 architect 裁决** ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 移出**
      ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ **另立**
      `.rddf/improvements/stream-builder-chain-silent-word-loss-2026-11.md` ✔
      ⇒ ⇒ ⇒ ⚠️ **且实测订正了台账的描述**：「链长 ≥ 2 才丢」**不准确**
      ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 链长 1 也丢**（数据**先出现后消失** ⇒ ⇒ 被下游吞掉）
- [x] **T3.10** ⚠️ **`demux` / `fork` MUST NOT 在本 change 改动**（P1-b / P1-c）

## C4：判别力用例（进**既有** target ⇒ ⇒ **计数锚点不变**）

- [x] **T4.1** `tests/chlib/test_stream.cpp`：转发类**上游真收到反压**
      （断言**调用方持有的** ready 节点 `num_srcs() > 0` **且值 = 0**）
- [x] **T4.2** `tests/chlib/test_stream.cpp`：未覆盖时**恒就绪**（= 零破坏）
- [x] **T4.3** `tests/chlib/test_stream.cpp`：`connect_ready` **幂等**
      （重复调用不抛）+ **空源抛异常**
- [x] **T4.4** ⚠️ **已移出本 change**（随 T3.9）⇒ ⇒ ⇒ 「零丢字」用例属
      `stream-builder-chain-silent-word-loss-2026-11` ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒ ⇒
      ⇒ ⇒ ⇒ ⚠️ **本 change MUST NOT** 加该用例（⇒ ⇒ 修法未定 ⇒ ⇒ ⇒
      ⇒ ⇒ ⇒ ⇒ ⇒ **⇒ 加了会「先钉住一个未定的语义」）**
- [x] **T4.5** 变异验证：每条**先确认编译成功**
      （⇒ ⇒ 否则跑旧二进制 ⇒ ⇒ 假绿）
      ⚠️ 至少：filters 的 `=` ⇒ `<<=` · mux 的 `<<=` ⇒ `=` ·
      arbiter 的硬编码 `out_ready = true` 复原

## C5：护栏

- [x] **T5.1** R7：**转发类的行为判据**（跑新用例 ⇒ ⇒ 消费端不就绪 ⇒ ⇒
      **调用方** ready = 0）
- [x] **T5.2** R8：**转发类的静态判据**（`stream.h` + `stream_arbiter.h` 内
      **MUST NOT** 有 `out_ready = true` 这类局部常量 ⇒ ⇒ ⚠️ **MUST NOT** 用
      「源码含 `true`」这种**词法**判据 ⇒ ⇒ 改用「**结果 struct MUST 有
      `default_ready_source` 成员**」+「**MUST NOT** 有 `operator=` 形态的
      ready 写入」这类**结构**判据）
- [x] **T5.3** **穷举**：`stream.h` + `stream_arbiter.h` 内**所有**接受/产生
      ready 的入口（⚠️ 「只查已知符号 ⇒ 漏掉同族入口」是本仓登记的护栏盲区
      ⇒ ⇒ **穷举** MUST 用**结构**判据 ⇒ ⇒ **MUST NOT** 靠符号清单）
- [x] **T5.4** 自检：**原地投毒**（⚠️ 投毒副本不会被 include ⇒ ⇒ 恒绿）
      ⇒ ⇒ 至少 3 种：filters 的 `=` ⇒ `<<=` · arbiter 复原硬编码 ·
      删掉 `default_ready_source` 成员
- [x] **T5.5** ⚠️ **`restore()` 的第三方改动检测 MUST 保留**（P0' 实测：还原会
      覆盖工作区改动 ⇒ ⇒ 加固后才安全）

## C6：台账回写

- [x] **T6.1** `.rddf/improvements/stream-forward-ready-closure-2026-10.md`
      ⇒ ⇒ P1-a 的**实施结果** + 剩余（P1-b / P1-c）+ 本轮 10 个探针的结论

## C7：验收与归档

- [x] **T7.1** `ctest -LE perf` ⇒ **194/194**（零回归）
- [x] **T7.2** 全部护栏（含 `--self-test`）⇒ exit 0
- [x] **T7.3** samples **39/39** · examples **28/28**
- [x] **T7.4** **计数锚点不变**：205 / 38 / 29 / 184 / 194 / 17
- [x] **T7.5** `openspec validate --specs --strict` ⇒ ⇒ **失败数 = 21（基线）**
      ⇒ ⇒ **MUST NOT** 新增失败
- [ ] **T7.6** critic 评审（**新 change** ⇒ ⇒ 独立预算）
- [ ] **T7.7** 归档（**禁** `--skip-specs`）⇒ ⇒ 归档后**另行补写**主 spec 的
      `## Purpose`
