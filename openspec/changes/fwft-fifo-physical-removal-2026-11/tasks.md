# Tasks：`chlib::fwft_fifo` 物理删除（B4）

## C1 —— 删除组件 + 回归禁令

- [x] `include/chlib/fifo.h`：删除 `FwftFifoResult` + `fwft_fifo()` + 文档块
      （≈ 92 行）
- [x] `tests/chlib/test_fifo.cpp`：删除 `TEST_CASE("FIFO: fwft_fifo")`
      （⚠️ **唯一**消费者；**零判别力** —— 只断言 reset 后 `empty`/`full`，
      从未断言 show-ahead）⇒ ⇒ `ctest` 条目数不变
- [x] 同文件：两处 async-fifo 注释不再引用已删符号 ⇒ 改为
      「本仓没有 FWFT 组件 ⇒ FWFT/show-ahead 请用 `sync_fifo::q_async`」
- [x] `scripts/lib/stream_domain_closure.py`：新增 **R11**（两面判据）
      ⇒ ① 符号 MUST NOT 回归 ② `q_async` MUST 仍在
- [x] 判别力验证（**手动探针**，该护栏的投毒框架只支持清单投毒）：
      把 `fwft_fifo` 写回 `fifo.h` ⇒ R11 转红；还原 ⇒ 复绿
- [x] `docs/adr/ADR-008`：§4 替代方案清单去掉 `fwft_fifo` + 变更日志 v1.2
      （**决策零改动**）
- [x] `.rddf/checks/stream-domain-closure.md` 第 14 行：
      实例已消除（记录删除 + R11）；`debt:` 改指 **B8**（通用条款仍未兑现）
      ⇒ 该行**保持 ❌**（不因删实例就改 ✅ —— 通用义务未完成）

## C2 —— spec 订正

- [x] `chlib-stream-semantics` 的 **MODIFIED ×1**：
      `#### Scenario: fwft_fifo MUST NOT 被当作 FWFT 参照` **内容**改为
      「MUST 不存在 + MUST NOT 重新引入同名组件 + 由结构护栏守住」
      ⇒ ⚠️ **标题保留**（CLI 按标题匹配 Scenario；改标题 = 丢弃旧 + 新增 ⇒ 拒绝归档）
      ⇒ 正文加「本 Scenario 的标题已过时」注记（ADR-053 同型先例）

## 验收

- [x] `include/chlib/fifo.h`（剥注释）无 `fwft_fifo` / `FwftFifoResult`
- [x] `q_async` 仍在
- [x] `verify_stream_domain_closure.sh` PASS（R11 绿）+ `--self-test` 6/6
- [ ] `ctest -LE perf` 零回归
- [ ] `openspec validate fwft-fifo-physical-removal-2026-11 --type change --strict` valid
- [ ] `scripts/verify_openspec_delta.sh` 4 条全绿
- [ ] 归档：**禁用** `--skip-specs`（MODIFIED ×1 需合并进主 spec）
- [ ] 计数锚点**不变**（无新增测试文件 / 无 ctest 条目变化）
