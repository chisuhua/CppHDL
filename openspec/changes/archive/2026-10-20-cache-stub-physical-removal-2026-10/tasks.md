## Tasks

> 方案：**variant (b)** —— 不做 BYPASS mode（用户决议 2026-10-20）。
> 3 个 atomic commit，每个 commit 独立可构建 + 独立可回归。
> **测试可执行数全程不变**（改而非删）→ `CTEST_TOTAL: 169` / non-perf 158 不动。

---

- [x] ### Task 1: Atomic Commit 1 — 重写 `tests/chlib/test_cache_pipeline_integration.cpp`（5 个 TEST_CASE 全部）

- **action**:
  - include 换成 `#include "cpu/cache/i_cache_sync.h"` + `#include "cpu/cache/d_cache_sync.h"`
  - 删 `#pragma GCC diagnostic push / ignored "-Wdeprecated-declarations" / pop`
  - `ch::ch_device<ICache>` → `ch::ch_device<ICacheSync>`；`ch::ch_device<DCache>` → `ch::ch_device<DCacheSync>`
  - `STATIC_REQUIRE(std::is_base_of_v<ch::Component, ICache>)` → `ICacheSync`（DCache 同理）
  - 断言改为真实 cache 时序（端口名与 stub 完全兼容，改写是机械的）：

    | TEST_CASE | 新断言 |
    |---|---|
    | Type Check | `STATIC_REQUIRE(is_base_of_v<ch::Component, ICacheSync>)` / `DCacheSync` |
    | I-Cache Integration | cycle1 冷读（`addr=0x1000, req=1, axi_ready=1`）→ `miss==1`、`axi_valid==1`、`axi_addr==0x1000`；cycle2（`req=0, axi_resp_valid=1, axi_data=0xDEADBEEF`）→ 填充；cycle3 同址再读 → `miss==0`、`data==0xDEADBEEF` |
    | D-Cache Integration | 冷读（`addr=0x2000, we=0`）→ `miss==1`、`axi_valid==1`、`axi_addr==0x2000`；写通道（`we=1, wdata=0xCAFEF00D`）→ `axi_wdata==0xCAFEF00D`、`axi_we==1`；填充后读 → `rdata==axi_rdata` |
    | Dual Cache Access | 保持双 device 独立 context 断言（语义不变） |
    | AXI Interface | 改为真实握手：miss 且 `axi_ready=0` → `ready==0`（不再是 stub 的 `ready==1`）；hit 后 `data` 取 cache 值 |

  - 时序模板取自 `tests/chlib/test_cache_sync.cpp`（已 dual-backend 验证），
    **不要新造时序**；若实测与模板不符，先记录差异再改断言
  - 2 个依赖 `Rv32iPipelineWithCache` 的 SKIP 保持 SKIP（该类仍不存在）
- **verify**:
  - `cmake --build build -j$(nproc) --target test_cache_pipeline` 0 error
  - `ctest -R test_cache_pipeline --output-on-failure` PASS
  - 该 TU 编译 0 个 `-Wdeprecated-declarations` 警告

---

- [x] ### Task 2: 改 `tests/chlib/test_cache_simple.cpp`（2 个 TEST_CASE，改而非删）

- **action**:
  - include 换成 `cpu/cache/{i,d}_cache_sync.h`
  - `ICache icache{nullptr, "icache"};` → `ICacheSync icache{nullptr, "icache"};`（DCache 同理）
  - 删 `#pragma GCC diagnostic` 包裹
  - 保留 `REQUIRE(true)` smoke 语义（该文件的定位就是 instantiate smoke）
- **verify**: `ctest -R test_cache_simple --output-on-failure` PASS

---

- [x] ### Task 3: 改 `tests/chlib/test_phase4_integration.cpp`（7 个 TEST_CASE，改而非删）

- **action**:
  - include 换成 `cpu/cache/{i,d}_cache_sync.h`
  - `ch::ch_device<ICache>` / `ch::ch_device<DCache>` → `ICacheSync` / `DCacheSync`
    （**HazardUnit 相关的 4 个 case 原样保留**，`rv32i_hazard_complete.h` include 不动）
  - 删 `#pragma GCC diagnostic` 包裹
- **verify**: `ctest -R test_phase4_chlib --output-on-failure` PASS

---

- [x] ### Task 4: 中间回归（Commit 1 之后）

- **verify**:
  - `cmake --build build -j$(nproc)` 0 error
  - `ctest -LE perf --output-on-failure` → **158/158 PASS**（计数不变）
  - `bash scripts/verify_test_counts.sh` exit 0

---

- [x] ### Task 5: Atomic Commit 2 — `git rm` stub 头文件

- **action**:
  - `git rm include/cpu/cache/i_cache_stub.h`
  - `git rm include/cpu/cache/d_cache_stub.h`
  - `tests/CMakeLists.txt` **不需要改动**（3 个 test 文件都保留）
- **verify**:
  - `git ls-files include/cpu/cache/*_stub.h` → 空
  - `cmake --build build -j$(nproc)` 0 error
  - `ctest -LE perf` → 158/158 PASS

---

- [x] ### Task 6: Atomic Commit 2 — Doc sync

- **action**:
  - `include/cpu/AGENTS.md` **STRUCTURE 表**：删 `i_cache_stub.h / d_cache_stub.h` 行
  - `include/cpu/AGENTS.md` **TESTS 表**：`test_phase4_chlib` 行补一句 "cache 使用
    `ICacheSync` / `DCacheSync`"
  - 根 `AGENTS.md` §KNOWN LIMITATIONS：删 "Cache stubs ... are pass-through" 段
    （注意该段里还有 riscv-mini wrapper 与 "不要 co-include stub" 的说明，
    删整段时保留 riscv-mini wrapper 那部分信息）
  - 根 `AGENTS.md` **测试计数不动**（169 / 158 / `tests/chlib/test_*.cpp`=29 /
    `tests/test_*.cpp`=99 均不变）
- **verify**:
  - `grep -rn "cache_stub" AGENTS.md include/cpu/AGENTS.md | wc -l` → 0
  - `grep -rn "#include.*_cache_stub\.h" --include='*.h' --include='*.cpp' include/ src/ tests/ examples/ | wc -l` → 0
  - `grep -rn "chlib::ICache\b\|chlib::DCache\b" --include='*.h' --include='*.cpp' include/ src/ tests/ examples/ | wc -l` → 0

---

- [x] ### Task 7: Atomic Commit 3 — ADR-037 追加 Executed note

- **action**:
  - `docs/adr/ADR-037-cache-stub-removal.md` §5 Status **追加**一条 dated update：
    - 决议：variant (b)（不做 BYPASS mode），用户 2026-10-20 拍板
    - 理由：`*_complete.h` 已是 `*_sync.h` 的 alias（ADR-041），BYPASS mode 无落点；
      stub 语义不构成合法 cache 行为；上游 improvement §5.4/§6 architect 推荐 (b)
    - 提前 6 个月执行的理由：0 生产调用方 + 3 个 blocker 已解除 + dual-backend 覆盖已就位
    - 标记 `Executed`（ADR 正文 Decision 章节保持不动，append-only）
- **verify**:
  - `grep -q "Executed" docs/adr/ADR-037-cache-stub-removal.md` → 0
  - ADR 的 §1-§4 内容未被改写（`git diff` 只含 §5 追加）

---

- [x] ### Task 8: 最终全量回归

- **verify**:
  - `ctest -LE perf --output-on-failure` → **158/158 PASS**
  - `ctest -L verilator_e2e --output-on-failure` → 全 PASS（双后端不退化）
  - `cmake --build build -j$(nproc) 2>&1 | grep -E "error|Error"` → 0 行
  - `bash scripts/verify_test_counts.sh` exit 0
  - `openspec validate cache-stub-physical-removal-2026-10 --strict` → valid
  - `openspec validate --specs --strict` → 通过

---

- [x] ### Task 9: archive

- **action**:
  - `openspec archive cache-stub-physical-removal-2026-10 --yes`
    （**不带 `--skip-specs`** —— 本 change 有 spec 变更，根 AGENTS.md §OPENSPEC WORKFLOW 禁止）
- **verify**:
  - `ls openspec/changes/archive/ | grep cache-stub-physical-removal-2026-10`
  - `openspec/specs/cpu-cache-subsystem/spec.md` 中不再有 "stub_stub.h deprecation is
    preserved" Requirement，且新增 "cache stub 物理移除且行为覆盖由 Sync 变体承接"
  - `openspec validate --specs --strict` 通过
