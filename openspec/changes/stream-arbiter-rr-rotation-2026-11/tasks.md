# Tasks: stream-arbiter-rr-rotation-2026-11

> 勾选规则：**commit 之后**按 `git show --name-only <sha>` 核对实际改动再勾
> （MUST NOT 先勾后做）。

## T1 实测与设计（**先证据后动手**）

- [x] **T1.1** 探针确认缺陷（`/tmp/opencode/p2/probe_rr.cpp`）：`two_valid`
      ⇒ 恒选 2 · `all_valid` ⇒ 恒选 1 ⇒ ⇒ **饿死确认**
- [x] **T1.2** 探针定位根因（`probe_selector.cpp`）：`round_robin_selector`
      **逐项正确**（5 个 `last_grant` 值全对）⇒ ⇒ **缺陷 100% 在调用方**
- [x] **T1.3** 摸清影响面：4 个调用点逐个核对 ⇒ ⇒ **零既有断言破坏**
- [x] **T1.4** 逐拍递推金标（`design.md` §2.3 的表）⇒ ⇒ 与实测**逐项一致**

## T2 实现

- [x] **T2.1** `include/chlib/stream.h`：删死节点 `current_channel`
- [x] **T2.2** 新增反馈寄存器 `last_grant_reg`（one-hot，**零初值**）
- [x] **T2.3** `round_robin_selector` 的 `last_grant` 改为读寄存器
      （**不再**恒传 `0_d`）
- [x] **T2.4** `last_grant_reg->next` = **仅当 `arb_result.valid`** 时更新，
      否则**保持自身**（MUST NOT 写 `0_d` —— 否则相位被静默重置）
- [x] **T2.5** 确认**不成环**（反馈经寄存器 ⇒ 组合环被隔断）

## T3 判别力用例

- [x] **T3.1** 用例 A：`two_valid`（0&2）⇒ 金标 `0,2,0,2,0,2`（6 拍逐拍）
- [x] **T3.2** 用例 B：`all_valid` ⇒ 金标 `2,3,0,1,2,3,0,1`（8 拍逐拍）
- [x] **T3.3** 用例 C：唯一有效通道 ⇒ 仍被选中（既有语义回归锁）
- [x] **T3.4** **反向判据**：把反馈断回常量 ⇒ 用例 MUST 转红
      （实测：`cycle 0 selected=1` 期望 2 ⇒ 2 failed）
- [x] **T3.5** 还原 + 重编 + 复绿（38 断言）；还原 **MUST** 刷新 mtime

## T4 命名/文档诚实化（**零行为变更**）

- [x] **T4.1** `include/chlib/stream_arbiter.h`：`stream_arbiter_sequence` 的
      头注释订正 —— 删「using round-robin selection」/「wraps around」，
      如实写「固定优先级扫描、较高索引优先」
- [x] **T4.2** 结果 struct 的注释同步（「Fixed order arbitration, wraps around」
      ⇒ 固定优先级）
- [x] **T4.3** 确认 `sequence` / `priority` 的**行为逐位不变**
      （既有 `test_stream` 的 arbiter 段仍绿）

## T5 契约

- [x] **T5.1** **新建** capability `chlib-stream-arbitration`
      （ADDED ×2：轮转 MUST 真轮转 · 命名与文档 MUST 一致）
- [x] **T5.2** 两条 Requirement 正文 ≤500 字符（实测 285 / 247）
- [x] **T5.3** `openspec validate --type change --strict` ⇒ **valid**
- [ ] **T5.4** 归档后**手工补写** `openspec/specs/chlib-stream-arbitration/spec.md`
      的 `## Purpose`（⚠️ CLI **不合并** delta 的 Purpose）

## T6 护栏（复用既有投毒框架，**不造新脚本**）

- [x] **T6.1** 在 `scripts/lib/stream_ready_ownership.py` 加投毒
      `rr-feedback-const`（反馈 ⇒ 常量，= 修复前形态）
- [x] **T6.2** 归入「**仅行为判据**」类别（同 P1-a 的 `drop-multidrive-guard`）
      ⇒ ⇒ 自检**MUST NOT** 对它强制「静态判据也红」
- [x] **T6.3** `--self-test` 实测：该投毒 **行为用例转红** + 还原后复绿

## T7 验证与归档

- [x] **T7.1** `ctest -LE perf` 全绿（零回归）
- [x] **T7.2** 全部护栏 + 全部 `--self-test` exit 0
- [x] **T7.3** `run_all_samples.sh` + `run_all_ported_tests.sh` 全绿
      （⚠️ 两者都含 `stream_arbiter` 消费者）
- [x] **T7.4** 计数锚点不变（`CTEST_TOTAL` / `verilator_e2e` /
      `DUAL_BACKEND_PARITY_*` / `EXAMPLES_TEST_SOURCES`）
- [x] **T7.5** `verify_openspec_delta.sh` 4 条全绿
- [x] **T7.6** critic 评审（本 change 独立预算）
- [ ] **T7.7** 归档（**禁** `--skip-specs`）⇒ ⇒ 归档后补写 Purpose（T5.4）

## 验证命令

```bash
cmake --build build --target test_stream -j"$(nproc)"
./build/tests/test_stream "[rotation]"
ctest --test-dir build -LE perf --output-on-failure
bash scripts/verify_stream_ready_ownership.sh
bash scripts/verify_stream_ready_ownership.sh --self-test
bash scripts/run_all_samples.sh
./run_all_ported_tests.sh
openspec validate stream-arbiter-rr-rotation-2026-11 --type change --strict
bash scripts/verify_openspec_delta.sh
```
