# Tasks: stream-ready-ownership-correction-2026-10

## C1：契约侧（主 spec delta）

- [x] **T1.1** R1 正文**收窄**（fifo 类保留）+ 新增「本条的适用范围」段
      （转发类 MUST NOT 被声称「已兑现」+ 义务**显式移交** P1 台账 §3）
      ⇒ ⇒ **Scenario 标题逐字不动** · **Scenario 正文逐字保留**
- [x] **T1.2** R2 补「**覆盖生效的三个必要条件**」（目标已存在且非 literal ·
      源非空 · 源持久）+「三类失效形态不同 MUST NOT 用单条判据覆盖」
      ⇒ ⇒ **顺带订正「field proxy」的理由表述**（→ 「已存在的非 literal 节点」）
- [x] **T1.3** R3 补「`<<=` 的**充分条件** = 目标节点 MUST 已存在」
      +「『用 `<<=`』MUST NOT 被当作『覆盖已生效』的充分条件」
      +「调用方的 stream MUST 先建立字段节点」（含 `create_field_nodes()`）
      + 标注「ADR-045 决策 0-A 落地时本条经 MODIFIED 退役」
- [x] **T1.4** 顶部断言加**范围限定**（**仅 fifo 类**）
      ⇒ ⇒ ⚠️ `## Purpose` 的订正**不在 delta 内生效**（archive 不合并）
      ⇒ ⇒ 归档后**另行补写**主 spec 的 `## Purpose`
- [x] **T1.5** `bash scripts/verify_openspec_delta.sh` ⇒ exit 0

## C2：ADR-054 订正（**architect 权限 · 用户已批准代改 ⇒ 加注「角色越权执行」**）

- [x] **T2.1** §3.2 第 1 条：订正「field proxy」理由 ⇒ ⇒「目标 MUST 是**已存在的
      非 literal** 节点」+ 补「`<<=` 在空目标上另建节点 ⇒ ⇒ 调用方被孤儿化」
- [x] **T2.2** §3.2 第 1 条：删掉**末 3 bullet**（形态 B 残留，与前 2 bullet
      直接矛盾 ⇒ ⇒ ADR `:48` 自称「不保留两版」**不成立**）
- [x] **T2.3** §3.1 形态 A 的「显式追认」段：**收窄适用范围**（覆盖语义已有
      契约载体 ⇒ ⇒ 冗余但无害；「空目标建节点」是**新依赖** ⇒ ⇒ MUST NOT
      作契约依据 ⇒ ⇒ 正确路径 = 让该分支**不可达**）
- [x] **T2.4** §4 **整节**重写：删形态 B 残留（「结果在 `connect_ready` 之前
      不完整 ⇒ 全部调用点 MUST 迁移」·「未调 `connect_ready` ⇒ 响亮
      Value not found」）+ 删悬空残片 + 补形态 E 的真实后果
- [x] **T2.5** §5 Verification：① 「grep-lint 抓到 `ready = true`」（**词法判据**）
      ⇒ ⇒ 改为**效果判据**表述 ② 随 §3 同步订正
- [x] **T2.6** `:27`「关联 change: **待建**」⇒ ⇒ 路径订正为已归档

## C3：实现侧注释订正（**零行为变更**）

- [x] **T3.1** `include/chlib/stream.h:50`：「field proxy」⇒ ⇒ 订正
- [x] **T3.2** `include/chlib/stream.h:347`：同上
- [x] **T3.3** `include/chlib/stream.h:353`：同上
      ⇒ ⇒ **零行为变更**（只改注释）⇒ ⇒ **不得**顺手改实现

## C4：判别力用例（进**既有** target ⇒ ⇒ **计数锚点不变**）

- [x] **T4.1** `tests/chlib/test_stream.cpp`：新增「`connect_ready` 后**调用方持有的**
      ready 节点**确有驱动**且**值跟随**」用例
      ⇒ ⇒ **变体 A**（调用方**未** `as_master`）⇒ ⇒ MUST 断言
      `impl != nullptr` 且 `num_srcs() > 0`
- [x] **T4.2** 变异验证：把 `stream_mux_connect_ready` 的 `<<=` 改回 `operator=`
      ⇒ ⇒ **先确认编译成功** ⇒ ⇒ 用例 MUST **转红** ⇒ ⇒ **还原 + 刷新 mtime +
      重建**（否则跑脏二进制）

## C5：护栏

- [x] **T5.1** `scripts/verify_stream_ready_ownership.sh` 头注释：明确 **R1 只覆盖
      fifo 侧** ⇒ ⇒ 转发类**尚无**行为判据 ⇒ ⇒ 与 R1 的「适用范围」段一致
- [x] **T5.2** `scripts/lib/stream_ready_ownership.py`：R6 的 `CALL_RE` 覆盖面
      说明 ⇒ ⇒ 明确**当前只覆盖** `stream_fifo` / `stream_queue`
      ⇒ ⇒ 转发类的调用点判据属 **P1**

## C6：台账回写（**P1/P2 台账 · 依 critic SF5/SF6 + N2/N3**）

- [x] **T6.1** `.rddf/improvements/stream-forward-ready-closure-2026-10.md`：
      拆分 **P1-a / P1-b / P1-c** + `queue<>()` 优先级**最高** + 「arbiter×4 +
      2 处先读后写」措辞订正 + 守卫统一规格（V3 实测后的方向）+ fork 注释
      MUST 重写
- [x] **T6.2** `.rddf/improvements/stream-arbiter-rr-rotation-2026-10.md`：
      记录 P0 发现 ⑤ 的**实测**依据（`current_channel` 死节点）

## C7：验收与归档

- [x] **T7.1** `openspec validate stream-ready-ownership-correction-2026-10 --type spec --strict` ⇒ **valid**
- [x] **T7.2** `ctest -LE perf` ⇒ **194/194**（零回归）
- [x] **T7.3** 全部护栏（含 `--self-test`）⇒ exit 0
- [x] **T7.4** **计数锚点不变**：205 / 38 / 29 / 184 / 194 / 17 · samples 39/39 · examples 28/28
- [x] **T7.5** 第 **2** 轮 critic：只提交「**BLOCKER 闭合证据 + 本轮新增 diff**」
- [ ] **T7.6** 归档（**禁** `--skip-specs`）⇒ ⇒ 归档后**另行补写**主 spec 的 `## Purpose`
      （先例 `core-lnode-entry-set`）
