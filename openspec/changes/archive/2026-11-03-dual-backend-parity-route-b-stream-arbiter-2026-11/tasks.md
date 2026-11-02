# Tasks：路线 B（I 批）—— stream 仲裁器族进双后端 parity

## C0 —— 可转性 4 问

- [x] ① 语义是否已被别的 dual 文件覆盖？ ⇒ **否**（两个符号在 54 个 dual 文件里各 0 次）
- [x] ② 断言形态有判别力吗？ ⇒ 变异 7/7 全抓
- [x] ③ 与后端无关吗？ ⇒ **否**（真跑仿真 + 读顶层端口）
- [x] ④ 形态值得新写吗？ ⇒ 既有测试**全是裸 context** ⇒ R6 排除项 ⇒ **不可转换** ⇒ 新写
- [x] ⚠️ **刻意避开** `stream_arbiter_lock`（带实测缺陷 ⇒ 先修后覆盖）

## C1 —— 新文件（**原文件 MUST NOT 修改**）

- [x] `tests/chlib/test_stream_arbiter_dual.cpp`
      ⇒ pair 1 `stream_arbiter_priority`（8 组：高位优先 / ready 门控差异）
      ⇒ pair 2 `stream_arbiter_round_robin`（8 组：真轮转 / 无请求时相位保持）
- [x] `tests/CMakeLists.txt`：1 条 `add_dual_backend_test`
- [x] `tests/test_stream_arbiter.cpp` / `tests/chlib/test_stream.cpp` **未修改**

## C2 —— 判据（N2′）与判别力

- [x] **N2′**：**192 == 2 × 96**
- [x] 变异 **7/7 全抓**
- [x] 期望值**手推**（独立 Python 模型 ⇒ 探针实测逐位吻合）
- [x] ⚠️ 如实登记**一个无效变异**（M6 初版用 `1_d` ⇒ 恒绿 ⇒ 相位不可观测 ⇒ 改 `2_d`）

## C3 —— 缺陷登记（本批的副产品）

- [x] 🔴 框架层：跨 DUT 状态污染 ⇒ `bit_select` 越界 ⇒
      `.rddf/improvements/cross-dut-bit-select-out-of-range-2026-11.md`
- [x] 🔴 `priority_encoder` 的注释与实现相反 ⇒
      `.rddf/improvements/priority-encoder-doc-contradicts-implementation-2026-11.md`
- [x] 🔴 三个 stream 宽度适配器「构建能过、一仿真就 SIGSEGV」⇒
      `.rddf/improvements/stream-width-adapters-crash-on-simulate-2026-11.md`

## C4 —— 命名诚实性（B8 的 R3）

- [x] proposal 带 `<!-- touched-components: stream_arbiter_priority, stream_arbiter_round_robin -->`
- [x] 登记表 **+1 条** + 升级 `stream_arbiter_round_robin` 的判别性用例为双后端版
- [x] 护栏 + 自检全绿

## C5 —— 计数与验收

- [x] 实测（全量重建后）：`ctest -N` **236** · `-L verilator_e2e` **68** · `-LE perf` **225** ·
      `verify_dual_backend_parity.sh` **59 / 265** · `tests/chlib/test_*.cpp` **50**
- [x] AGENTS.md 锚点同步（4 anchor + 3 NOTES 计数 + NOTES 条目）
- [x] 全量重建后 `ctest -LE perf` **225/225** 全绿
- [x] 全部 `scripts/verify_*.sh` exit 0
- [x] 归档用 `--skip-specs`（本 change 显式声明无 spec 变更）
