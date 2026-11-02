# Tasks：路线 A（E 批）—— Async cache 进双后端 parity + 用事实裁决 spec 矛盾

## C0 —— 先过「可转性 4 问」

- [x] ① 语义是否已被别的 dual 文件覆盖？ ⇒ **Async：否**（`test_cache_sync.cpp` 只覆盖
      **Sync** 10 pair）；**Sync：是**（重复 ⇒ 本批**不做** Sync 6 个用例）
- [x] ② 断言形态有判别力吗？ ⇒ 变异 11/11 全抓
- [x] ③ 与后端无关吗？ ⇒ **否**（真跑仿真 + 读顶层端口）
- [x] ④ 形态值得新写吗？ ⇒ 原用例的激励可**逐位沿用**，但期望值来源升级为手推
      + 4 个原用例从未断言的维度
- [x] **先做缺陷甄别**：发现两个 async 头**不自包含**（`*CacheConfig` 在 `*_sync.h`）

## C1 —— 新文件（**原文件 MUST NOT 修改**）

- [x] `tests/chlib/test_cache_async_dual.cpp`
      ⇒ pair 1 `ICacheAsync`（冷不填充 / 冷填充 / hit / **`req=0` 门控**）
      ⇒ pair 2 `DCacheAsync` 读（含**非对齐**地址 `0x2004`）
      ⇒ pair 3 `DCacheAsync` 写直达（**含「写入真的落进 cache」**）
- [x] `tests/CMakeLists.txt`：1 条 `add_dual_backend_test`
- [x] `tests/chlib/test_cache_complete.cpp` **未修改**

## C2 —— 判据（N2′）与判别力

- [x] **N2′**：**146 == 2 × 73**
- [x] 变异 **11/11 全抓**；每个断言组有可追溯变异
- [x] ⚠️ **我自己补掉两处判别力缺口**：块对齐掩码（补非对齐地址 + M8）·
      `axi_valid & req` 门控（补 `req=0` 拍 + M11）
- [x] ⚠️ 如实登记**陷阱 T6**（填充在同一 tick 内可见）与**第 3 次推导错**
      （填充拍的 `miss` 期望写成 1，被双后端一致证伪）

## C3 —— spec 订正（本批的第二个产出）

- [x] 实测确认 **Async dual-run 全绿** ⇒ Scenario「spec R6 排除清单遵守」的
      `MUST NOT` **被证伪** ⇒ 订正为 `MAY` + 三条实测约束
- [x] **Scenario 标题未改**（CLI 按标题匹配 ⇒ 改标题会被判「丢弃旧 + 新增」）
- [x] delta 放在 `specs/dual-backend-test-coverage/spec.md` 的 `## MODIFIED Requirements`
- [x] `verify_openspec_delta.sh` 4 条全绿 · `openspec validate --strict` valid

## C4 —— 计数与验收

- [x] 实测（全量重建后）：`ctest -N` **232** · `-L verilator_e2e` **64** ·
      `-LE perf` **221** · `verify_dual_backend_parity.sh` **55 / 257** ·
      `tests/chlib/test_*.cpp` **46**
- [x] AGENTS.md 锚点同步（4 anchor + 3 NOTES 计数 + NOTES 条目）
- [x] 全量重建后跑 `ctest -LE perf` 全绿
- [x] 全部 `scripts/verify_*.sh` exit 0
- [x] 归档 **不用** `--skip-specs`（本 change 有 spec delta）
