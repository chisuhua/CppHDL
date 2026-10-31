# Tasks: chlib-stream-ready-ownership-2026-10

> 实施 **ADR-054**（`docs/adr/ADR-054-stream-ready-ownership-form.md`）。
> ⚠️ 期望值 MUST **手推**；变异 MUST **先确认编译成功**（编译期判据除外 ⇒ 如实登记）。

## 1. 立项
- [x] 1.1 ADR-054 裁决（architect，2026-10-31）
- [x] 1.2 change 目录 + `proposal.md` + `design.md` + `tasks.md` + capability delta
- [x] 1.3 **前置探针**：`ch_uint`/`ch_bool` 的 `<<=` 在节点为 null 时建节点 ✔
- [x] 1.4 **端到端探针**：形态 B「延迟构造」可行（读使能取**同一节点**）✔

## 2. 实现
- [ ] 2.1 `StreamFifoResult` MUST 携带**输入 stream**
- [ ] 2.2 `stream_fifo` 改**延迟构造**（构造期不产出任何字段）
- [ ] 2.3 新增 `stream_fifo_connect_ready(result)`（内部 FIFO **此时**才建）
- [ ] 2.4 读使能取消费端 ready 的**同一节点**（**MUST NOT** 引入代理）
- [ ] 2.5 `stream_queue` 别名同步
- [ ] 2.6 `StreamBuilder::queue<DEPTH>()` 调用点同步
- [ ] 2.7 `stream_mux_connect_ready` 的连接手段 `operator=` ⇒ **`<<=`**
- [ ] 2.8 迁移 `samples/spinalhdl_stream_example.cpp` 的调用点

## 3. 归位 2 个 disabled sample
- [ ] 3.1 `stream_component_example`：迁移到新 API + 修 **4 处** `ch_in` 直写
- [ ] 3.2 `stream_handshake_example`：迁移到新 API + 修 **2 处** `ch_in` 直写
- [ ] 3.3 `samples/disabled_samples.txt` 移除这 2 行 + 更新判定基准
- [ ] 3.4 `samples/CMakeLists.txt` 的 `CH_SAMPLE_TARGETS` 补 2 个

## 4. 护栏
- [ ] 4.1 `scripts/verify_stream_ready_ownership.sh`（**新建**）：R0 ~ R5
- [ ] 4.2 `--self-test`（反向 fixture + **原地投毒**）
- [ ] 4.3 CI 挂载（主路径 + `--self-test`）

## 5. 验证
- [ ] 5.1 **R5**：`full` **可达**（反压可断言）⇒ 运行期用例
- [ ] 5.2 **R4**：未调 `connect_ready` ⇒ **可诊断**失败
- [ ] 5.3 **变异 4/4**（M1 ~ M4，每条**先确认编译成功**）
- [ ] 5.4 `ctest -LE perf` 零回归 · samples / examples 零回归
- [ ] 5.5 全部护栏（含 `--self-test`）exit 0 · **计数锚点不变**

## 6. 归档
- [ ] 6.1 归档（**禁** `--skip-specs`）⇒ 合并 `chlib-stream-ready-ownership` ADDED ×4
- [ ] 6.2 归档后 MUST 补写主 spec 的 `## Purpose`（archive **不合并** delta 的 Purpose）
