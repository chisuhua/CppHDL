# Tasks: header-odr-inline-fix-2026-11

> 判据来源：proposal.md §验收标准 · capability `core-header-odr-safety`（ADDED ×1）。
> ⚠️ **本 change 是 `hierarchical-rtl-output` 主线的唯一硬前置**（T2a）—— 层次化 emitter
> 是库级代码 ⇒ 任何 2+ TU 的下游项目都会被该 ODR 缺陷咬。

## T1 修复

- [x] 1.1 `include/module.h` 的 `build_hierarchical_name` 加 `inline`
      （附注释说明「MUST be `inline`」+ 指向护栏脚本，防后人误删）
- [x] 1.2 三条既有分支（`parent == nullptr` / 父路径为空 / `"unnamed"`）**逐字不变**

## T2 判据（链接期，不是文本扫描）

- [x] 2.1 `tests/fixtures/header_odr_link/tu_a.cpp`：仅 `#include "ch.hpp"` + 一个 anchor 函数
- [x] 2.2 `tests/fixtures/header_odr_link/tu_b.cpp`：仅 `#include "ch.hpp"` + `main` 调 anchor
      ⇒ ⚠️ **两个 TU 都不调 `ch::` 的运行期符号** —— 否则 fixture 要链 `libcpphdl`
      ⇒ ⇒ 「未定义符号」会与「multiple definition」混在一起
- [x] 2.3 `scripts/verify_header_odr.sh`：
      - **R0** fixture 存在性（fail-closed：缺失 ⇒ 报「缺 fixture」而非「链接失败」）
      - **R1** 两 TU 分别**编译**（编译失败与链接失败**分开报告**）→ 再**链接**
      - **R2** `--self-test` 原地投毒（去 `inline`）⇒ 必须转红**且原因是** `multiple definition`
        ⇒ 还原后必须**逐字节相同**且复绿
- [x] 2.4 编译/链接 flags 处理「未使用的 inline 体引用库符号」：
      `-ffunction-sections -fdata-sections` + `-Wl,--gc-sections`
      ⇒ ⚠️ 已验证 GC **不会**顺手丢掉重复定义（投毒后仍报 1 条 ⇒ 判别力保住）

## T3 接线

- [x] 3.1 CI 挂两处（主路径 + `--self-test`），各带 WHY 注释
- [x] 3.2 **不**新增 ctest 条目 ⇒ 计数锚点（`CTEST_TOTAL` / non-perf /
      `tests/test_*.cpp` / `DUAL_BACKEND_PARITY_*`）**全部不变**

## T4 验收

- [x] 4.1 `bash scripts/verify_header_odr.sh` → PASS
- [x] 4.2 `bash scripts/verify_header_odr.sh --self-test` → PASS（投毒转红 + 还原复绿）
- [x] 4.3 **g++ 与 clang++ 各跑一次**（`--self-test` 均 PASS）
- [x] 4.4 全量重建后 `ctest -LE perf` 零回归
- [x] 4.5 `openspec validate header-odr-inline-fix-2026-11 --type change --strict` valid
- [x] 4.6 `bash scripts/verify_openspec_delta.sh` 4/4

## 验证命令

```bash
cd /workspace/project/CppHDL
bash scripts/verify_header_odr.sh
bash scripts/verify_header_odr.sh --self-test
cmake --build build -j"$(nproc)"
ctest -LE perf --output-on-failure
openspec validate header-odr-inline-fix-2026-11 --type change --strict
bash scripts/verify_openspec_delta.sh
```
