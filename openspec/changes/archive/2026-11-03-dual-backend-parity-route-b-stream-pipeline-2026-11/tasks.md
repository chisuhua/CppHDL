# Tasks：路线 B（G 批）—— 三个 stream pipe 进双后端 parity

## C0 —— 可转性 4 问 + 只读查重

- [x] ① 语义是否已被别的 dual 文件覆盖？ ⇒ **否**（三个符号在 54 个 dual 文件里各 0 次）
- [x] ② 断言形态有判别力吗？ ⇒ 变异 7/7 全抓
- [x] ③ 与后端无关吗？ ⇒ **否**（真跑仿真 + 读顶层端口）
- [x] ④ 形态值得新写吗？ ⇒ 既有测试**全是裸 context** ⇒ R6 排除项 ⇒ **不可转换** ⇒ 只能新写
- [x] **只读查重**（建议 3）⇒ 4 个候选全部查过 ⇒ **挖出 2 个真缺陷** ⇒ 已独立立项

## C1 —— 新文件（**原文件 MUST NOT 修改**）

- [x] `tests/chlib/test_stream_pipeline_dual.cpp`
      ⇒ 3 个 wrapper DUT（m2s / s2m / half）+ 3 个 `DUAL_BACKEND_TEST_PAIR`
      ⇒ 8 拍 × 3 输出 × 3 pair = **72 断言/后端**
- [x] `tests/CMakeLists.txt`：1 条 `add_dual_backend_test`
- [x] `tests/test_stream_pipeline.cpp` **未修改**

## C2 —— 判据（N2′）与判别力

- [x] **N2′**：**144 == 2 × 72**
- [x] 变异 **7/7 全抓**（含 ADR-055 修复前的两个形态：m2s 丢数据 · s2m 方向反了的自环）
- [x] 期望值**手推**（独立 Python 模型逐拍生成 ⇒ **一次通过**）
- [x] ⚠️ 如实登记**两条判别力边界**（s2m 的「ready 被寄存」不可区分 · 「1 拍延迟」的相对表述）

## C3 —— 只读查重的副产品（两个既有缺陷 ⇒ 独立立项）

- [x] `chlib::stream_arbiter_lock` **恒选通道 0**（且输出别的通道的数据 ⇒ 静默数据损坏）
      ⇒ `.rddf/improvements/stream-arbiter-lock-selects-channel-zero-2026-11.md`
- [x] `chlib::round_robin_arbiter` **不轮转**（`next_ptr` 恒 0）
      ⇒ `.rddf/improvements/selector-round-robin-not-rotating-2026-11.md`
- [x] ⚠️ **本批刻意不做**这两个组件的 dual parity（会给缺陷盖章 ⇒ 先修后覆盖）

## C4 —— 计数与验收

- [x] 实测（全量重建后）：`ctest -N` **234** · `-L verilator_e2e` **66** · `-LE perf` **223** ·
      `verify_dual_backend_parity.sh` **57 / 261** · `tests/chlib/test_*.cpp` **48**
- [x] AGENTS.md 锚点同步（4 anchor + 3 NOTES 计数 + 2 条 NOTES 条目）
- [x] 全量重建后 `ctest -LE perf` **223/223** 全绿
- [x] 全部 `scripts/verify_*.sh` exit 0
- [x] 归档用 `--skip-specs`（本 change 显式声明无 spec 变更）
