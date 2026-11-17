# Tasks: verilator-pch-toolchain-fix-2026-11

> **task 定义与判据的唯一权威** = `proposal.md`（§验收标准 · §What Changes）。
> ⚠️ **无 spec delta**（工具链修复 ⇒ 零 capability 契约变更）。

## 执行序

`T1`（CMake 注入 + 重建 verilator）→ `T2`（新护栏 + 负向对照）→ `T3`（删绕行 + 护栏复跑）
→ `T4`（回归）→ `T5`（文档 + CHANGELOG）→ `archive`

**硬约束**

1. **T1 MUST 先于 T3** —— 删绕行之前工具链必须修好（否则护栏必红 ⇒ 中间态不过 CI）。
2. **T1 MUST NOT 改 submodule** —— 修复只在仓内 `CMakeLists.txt` 层 ⇒ ⇒ submodule 保持干净。
3. **T2 的负向对照 MUST NOT 碰真实安装产物** —— 用安装根的**副本** ⇒ ⇒ 零风险。

---

## T1 — CMake 注入 PCH 选项 + 重建 verilator

- [x] `CMakeLists.txt`：`ExternalProject_Add(verilator …)` 的 `CMAKE_ARGS` 追加
      `-DCFG_CXXFLAGS_PCH_I=…` / `-DCFG_GCH_IF_CLANG=…`（按 `CMAKE_CXX_COMPILER_ID` 分支）
- [x] 重建：`cmake --build build --target verilator` ⇒ ⇒ 安装产物的
      `CFG_CXXFLAGS_PCH_I` 必须**非空**（`-include` / `-include-pch`）
- [x] `git -C third_party/verilator status --short` 为空（submodule 零改动 ⇒ 实测为空）

验证：

```bash
cmake --build build --target verilator
grep -n 'CFG_CXXFLAGS_PCH_I\|CFG_GCH_IF_CLANG' build/verilator-install/include/verilated.mk
git -C third_party/verilator status --short
```

---

## T2 — 新护栏 `scripts/verify_verilator_pch.sh`（含负向对照）

- [x] ① **构建期指纹**：`CFG_CXXFLAGS_PCH_I` 非空 **且** 与 `verilator -getenv CXX` 的族属匹配
      （GNU ⇒ `-include`；Clang ⇒ `-include-pch`）
- [x] ② **行为判据**：用**自带的最小设计**（现场生成，不依赖仓内产物）强制 `--output-split`
      编译 ⇒ rc=0
- [x] ③ **负向对照**（`--self-test`）：`cp -a` 安装根 ⇒ 把副本的 `CFG_CXXFLAGS_PCH_I` 清空
      ⇒ 同一编译**必红** ⇒ ⇒ 还原（副本即用即弃 ⇒ 真实安装零风险）
- [x] ④ fail-closed：缺 verilator / 缺安装产物 ⇒ exit 2（禁静默跳过）
- [x] ⑤ **可执行位** = `100755`（`verify_guard_reachability.sh` 的判据 ②）

验证：

```bash
bash scripts/verify_verilator_pch.sh
bash scripts/verify_verilator_pch.sh --self-test
```

---

## T3 — 护栏删绕行（`--output-split 0`）

- [x] `scripts/verify_examples_rtl_sim.sh`：flags 去掉 `--output-split 0` + 注释改为
      「工具链已修 ⇒ ⇒ 默认路径（自动 split ⇒ PCH）被**真实覆盖**」
- [x] `verify_examples_rtl_sim.sh` **11 项 PASS** + `--self-test` 全绿（投毒数 +1 = 新增 ③ 的同型）
- [x] `riscv-mini-rv32i-soc`（**自动** split · 不加任何 flag）编译 rc=0

验证：

```bash
bash scripts/verify_examples_rtl_sim.sh
bash scripts/verify_examples_rtl_sim.sh --self-test
```

---

## T4 — 回归

- [x] `ctest --test-dir build -LE perf -j$(nproc)` = **255/255**
- [x] 既有护栏全绿（hierarchical / port-naming / flat-emit / public-api / counts / parity /
      ledger / roadmap / examples 两条 / delta）

验证：

```bash
ctest --test-dir build -LE perf -j"$(nproc)"
```

---

## T5 — 文档 + CHANGELOG

- [x] `docs/developer_guide/verilator-integration.md`：新增「PCH 与自动 split」小节
      （根因：CMake 构建缺 autoconf 的 PCH 探测 · 修复形态 · 判据 · 上游修法与升级复核义务）
- [x] `docs/CHANGELOG.md` 一条（构建面变更 ⇒ 用户可见）

---

## T6 — CI 接线 + 护栏可达性登记

- [x] `.github/workflows/ci.yml`：`pr-verilator-fast` job 加一步（那里才有真 verilator）
- [x] `.rddf/checks/guard-reachability.md`：登记表**同步新增一行**（双向 1:1 棘轮）
- [x] `scripts/verify_guard_reachability.sh` PASS

---

## archive

- [x] `openspec archive verilator-pch-toolchain-fix-2026-11 --yes`（**无 spec 合并** ⇒
      只移动 change 目录）⇒ 已归档为 `openspec/changes/archive/2026-11-17-verilator-pch-toolchain-fix-2026-11/`

---

## 实测证据（收口记录）

| AC | 证据 |
|---|---|
| AC1 | `CFG_CXXFLAGS_PCH_I = -include`（安装产物）；`verilator -getenv CXX` = `g++-14` ⇒ 族属匹配 |
| AC2 | 最小设计（300 条 assign）强制 `--output-split 200` ⇒ `VM_PARALLEL_BUILDS = 1` ⇒ `make` rc=0 且产出 `Vtop__pch.h.fast.gch` |
| AC3 | 安装根**副本**清空 flag ⇒ 同一编译 rc=2 + `error: Vtop__pch.h.fast: linker input file not found` |
| AC4 | rv32i_soc **默认 flag**（无 `--output-split`）⇒ 自动 split ⇒ `.gch` 产出 + `-include` 消费 ×3 ⇒ rc=0（45s） |
| AC5 | `verify_examples_rtl_sim.sh` **11 项 PASS** + `--self-test` PASS |
| AC6 | `ctest -LE perf` **255/255** |
| AC7 | `git -C third_party/verilator status --short` **为空** |
| AC8 | `verilator-integration.md` 新增「PCH 与自动 output-split」小节 · CHANGELOG 一条 |

⚠️ **开工期间发现的一处无关缺陷（**不在本 change 范围**）**：`scripts/verify_example_test_wiring.sh`
对 example 二进制的超时**硬编码 300s**，而 `test_rv32i_soc_dual_parity` 在 CMake 里声明
`TIMEOUT 600`、本机高负载下单跑实测 **390s** ⇒ ⇒ 该护栏会**误红**（exit 124）。与 PCH 无关
（该测试不经 `--output-split`），已如实上报，待用户裁决是否单独修。
