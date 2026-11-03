# Tasks：头文件自包含性（修缺陷 + 新护栏）

## T1 —— 全量实测（**先量，后改**）

- [x] 162 个 `include/**/*.h` 逐个单独编译（`-fsyntax-only`）
      ⇒ ⇒ **21 个不自包含** ⇒ ⇒ 分类：
      - **7 个真缺陷**（公共头 / 公共入口）⇒ ⇒ 本 change 修
      - **14 个分片头**（设计意图：被聚合头 include）⇒ ⇒ 登记豁免

## T2 —— 修缺陷（**零行为变更**）

- [x] `include/bundle/stream_bundle_member_inlines.h`：`chlib::` → `::chlib::`（4 处）
      ⇒ ⇒ **修 `#include "chlib.h"` 单独编译失败**（公共入口 · 仓内零消费者
      ⇒ ⇒ ⇒ 从未被验证过）
- [x] `include/cpu/pipeline/stages/id_stage.h` / `if_stage.h` / `rv32i_pipeline.h`：
      include 路径改 `include/` 相对
- [x] `include/cpu/pipeline/stages/ex_stage.h`：补 `rv32i_decoder.h`
- [x] `include/cpu/cache/{i,d}_cache_async.h`：补 `*_sync.h`

## T3 —— 死代码与注释订正（**零行为变更**）

- [x] `ex_stage.h`：**删** `BRANCH_*` 常量块（与实现的 funct3 编码**相反**
      且 `BRANCH_LTU=4` **恰好等于**正确的 `BLT=4` ⇒ 更易误导；且**零消费者**）
      ⇒ ⇒ 保留注释说明「为何删除」+ `branch_type` 端口注释订正（编码 = funct3）
- [x] `combinational.h`：`priority_encoder` 注释订正（「优先处理低位」⇒ **高位优先**）
      ⇒ ⇒ 并注明「本仓的『优先级』有两个方向」（`priority_encoder` 高位 vs
      `priority_selector` 低位）

## T4 —— 护栏

- [x] `scripts/lib/header_self_contained.py`（分析器）
      - R0 fail-closed · R1 · R2a（豁免仍必要）· R2b（集合一致）· R2c（头存在）
      - **增量缓存**：缓存键 = 该头的**传递 include 闭包**指纹 + 全局盐
        （编译器版本 + flags + `external/inipp`）⇒ ⇒ 单头检查 ≈ 1.5–3 s
        ⇒ ⇒ 全扫 ≈ 1–2 min ⇒ ⇒ **自检 7 次全扫从 12 min 降到 ≈ 2 min**
- [x] `scripts/lib/header_self_contained_exempt.txt`（**纯数据** · 两种 kind）
- [x] `scripts/verify_header_self_contained.sh`（R0 + 判据 + self-test）
- [x] `.github/workflows/ci.yml` 挂 **2 处**（主路径 + self-test）

## T5 —— 变异 / 投毒证据

- [x] self-test **3/3 全抓** + 还原后复绿
      - `break-include` ⇒ R1 红
      - `drop-exemption` ⇒ R2a 红
      - `add-exemption` ⇒ R2a 红
- [x] ⚠️ **首版投毒恒绿 ⇒ 已修正**（「删掉 include」会被传递性 include 补上
      ⇒ ⇒ 改成「include 路径写错」⇒ ⇒ 确定性失败）
- [x] ⚠️ **能力探测的一处修正**（实测）：只探「`<expected>` 能否被 include」
      **不够** —— 某 libc++ 环境下 include **成功**但 `std::expected` **不存在**
      ⇒ ⇒ 探测 MUST **用到那个名字**（`std::expected<void,int> e{};`）

## T6 —— 独立立项（**不在本 change 修**）

- [x] `stdlib-expected-missing-in-libcxx-2026-11`
      ⇒ ⇒ `std::expected` 在 libc++ 不存在 ⇒ ⇒ macOS 开 verilator 构建失败
      ⇒ ⇒ 改公开 API ⇒ ⇒ 需 ADR ⇒ ⇒ 本 change 只按能力绕开
- [x] `stream-builder-chain-silent-word-loss-2026-11`：**状态同步为已修复**
      ⇒ ⇒ 实测：ADR-055 的 `drive_pending_ready(result.push_stream.ready)` 已落地
      ⇒ ⇒ 判据 = `tests/test_stream_builder.cpp` 的「queue() 的反压 MUST 能传过
      builder 链」+ `tests/test_stream_builder_backpressure_dual.cpp`（双后端）

## T7 —— 验收

- [x] 全量重建 **0 error**
- [x] `ctest -LE perf` **230/230**
- [x] 全部护栏 exit 0（含 C5 行数锚点同步：`include/cpu/AGENTS.md` 6464 → **6506**）
- [x] 计数锚点**不变**（无新测试文件 ⇒ `CTEST_TOTAL` / parity 计数不动）
