# Proposal: vendored Verilator 的 **PCH 工具链修复** —— 让「自动 output-split」的模型可编译

> ⚠️ **独立 change**（不混进已归档的 `rtl-productization-surface-2026-11`）——
> 那条 change 只是在护栏里**绕行**了本缺陷（`--output-split 0`），**缺陷本身仍在**。
> ⚠️ **本 change 不改任何 capability 契约** ⇒ ⇒ **无 spec delta** ⇒ ⇒ 归档时无 spec 合并。

---

## Why

### 缺陷（实测 · 可复算 · 2026-11-17）

**vendored Verilator 的 CMake 构建漏了两个 configure 变量 ⇒ GCC 分支拿不到 PCH ⇒
任何「自动 output-split」的模型 C++ 构建必失败。**

| 环节 | 事实 |
|---|---|
| 上游 autoconf | `third_party/verilator/configure.ac:704-712` **会**探测并定义 `CFG_CXXFLAGS_PCH_I`（GCC=`-include` / Clang=`-include-pch`）+ `CFG_GCH_IF_CLANG`（Clang=`.gch`） |
| 本仓的构建路径 | verilator 走 **CMake**（`CMakeLists.txt` 的 `ExternalProject_Add(verilator …)`）⇒ `third_party/verilator/CMakeLists.txt:125` 的 `configure_file(include/verilated.mk.in … @ONLY)` **没有变量源** ⇒ 代入**空串** |
| 生成物 | `build/verilator-install/include/verilated.mk:48` = `CFG_CXXFLAGS_PCH_I = `（空） |
| 爆炸点 | `VK_PCH_I_FAST = $(CFG_CXXFLAGS_PCH_I) Vtb_x__pch.h.fast` ⇒ 展开成**裸文件名** ⇒ GCC 按未知扩展名当 **linker input** ⇒ `g++-14: error: Vtb_xxx__pch.h.fast: linker input file not found` |
| 触发条件 | `VM_PARALLEL_BUILDS=1` ⇒ Verilator **自动** split（默认阈值 **20000 statements** · `V3Options.h:336`）⇒ 用 PCH ⇒ ⇒ **中等以上模型必中** |

**实测对照（与本仓 HDL 无关 ⇒ 纯工具链缺陷）**：

| 场景 | 修前 | 修后（注入 `-include`） |
|---|---|---|
| counter（40 行 Verilog）**强制** `--output-split 200` | rc=2 + `linker input file not found` | **rc=0** |
| rv32i_soc（3066 行 Verilog）**不加任何 flag**（自动 split） | rc=2 | **rc=0** |
| PCH 是否真被采用 | — | ✅ `-Winvalid-pch` 无告警；对照实验（损坏 `.gch`）⇒ GCC 立刻报 `one or more PCH files were found, but they were invalid` ⇒ 「无告警 + rc=0」= 真用了 |

### 影响面（**潜伏 CI 红**）

| 调用方 | 现状 | 风险 |
|---|---|---|
| `scripts/verify_examples_rtl_sim.sh`（`riscv-mini-rv32i-soc`） | 曾红 ⇒ 在 `rtl-productization-surface-2026-11` 里用 `--output-split 0` **绕行** | 已遮蔽（但绕行本身有代价，见下） |
| `src/core/verilator_backend.cpp`（`verilator --cc`，**不带** split flag） | 77 个 `verilator_e2e` 全绿（模型都低于 20000 statements 阈值） | ⚠️ **潜伏红**：换个更大的模型就炸 |
| CI / 用户脚本（任何用 vendored verilator 编大模型的地方） | — | 同上 |

### 为什么现在做

1. **绕行不是修复**：`--output-split 0` 禁掉了 split ⇒ 大模型退化成**单个巨型 `.cpp`** ⇒ 内存/时间风险 ⇒ ⇒ 它把「工具链缺陷」换成了「编译资源风险」；
2. **用户 2026-11-17 裁决**：护栏**不保留** `--output-split 0` ⇒ ⇒ 必须先把工具链修好 ⇒ ⇒ 护栏才能真正覆盖「自动 split ⇒ PCH」这条路径；
3. **判别力为零的现状**：今天**没有任何判据**覆盖这个缺陷 ⇒ ⇒ 升级 submodule / 换镜像都可能悄悄把它带回来 ⇒ ⇒ 必须补机械判据。

---

## What Changes

### A. **工具链修复（零 submodule 改动）**

`CMakeLists.txt` 的 `ExternalProject_Add(verilator … CMAKE_ARGS …)` **注入两个 configure 变量**
（镜像上游 `configure.ac:704-712` 的判定，按编译器族属取值）：

```cmake
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  set(_cppp_vlt_pch_i "-include-pch")   # Clang：-include-pch + .gch 后缀
  set(_cppp_vlt_gch ".gch")
else()
  set(_cppp_vlt_pch_i "-include")      # GCC：-include（GCC 自动找 <hdr>.gch）
  set(_cppp_vlt_gch "")
endif()
#   → CMAKE_ARGS 追加 -DCFG_CXXFLAGS_PCH_I=${_cppp_vlt_pch_i} -DCFG_GCH_IF_CLANG=${_cppp_vlt_gch}
```

**为什么可行（已取证）**：`third_party/verilator/CMakeLists.txt:125` 是 `configure_file(… @ONLY)`
⇒ `@CFG_CXXFLAGS_PCH_I@` 由 **CMake 变量**求值 ⇒ cache 变量 `-D` 可见 ⇒ ⇒ **零 submodule 改动**
⇒ ⇒ submodule 保持干净（升级不丢补丁）。

### B. **护栏去掉绕行**

`scripts/verify_examples_rtl_sim.sh` **删掉 `--output-split 0`** ⇒ ⇒ 11 个产品单元回到
**默认路径**（自动 split ⇒ 真实覆盖 PCH 路径）⇒ ⇒ 缓存 key 随之变化（一次性重编）。

### C. **新增工具链判据（缺陷回归即红）**

新护栏 `scripts/verify_verilator_pch.sh`（fail-closed）：

| # | 判据 | 判别力 |
|---|---|---|
| ① | **构建期**：安装产物的 `CFG_CXXFLAGS_PCH_I` **非空** 且与 `verilator -getenv CXX` 的**族属**匹配（GNU ⇒ `-include`；Clang ⇒ `-include-pch`） | flag 空 ⇒ 必红（**这是缺陷的直接指纹**） |
| ② | **行为**：用**自带的最小设计**强制 `--output-split` 编译 ⇒ rc=0 | 修前 rc=2（`linker input file not found`） |
| ③ | **负向对照**（`--self-test`）：把**安装根的副本**的 `verilated.mk` 的 flag 清空 ⇒ 同一编译**必红** | 证明判据**区分对错**（不是「会打开文件」） |

⚠️ **负向对照不碰真实安装产物**：用 `cp -a` 复制安装根（21 MB）到临时目录 ⇒ 破坏副本 ⇒
⇒ **真实工具链零风险**（不需要 trap 还原）。

### D. **CI 接线 + 护栏可达性登记**

`ci.yml` 的 `pr-verilator-fast` job（**那里才有真 verilator**）加一步 ⇒ ⇒
`.rddf/checks/guard-reachability.md` 的登记表**同步新增一行**（双向 1:1 棘轮）。

### E. **文档**

`docs/developer_guide/verilator-integration.md`：记**根因**（CMake vs autoconf 的探测缺口）+
**修复形态** + **判据** + **上游修法**（给 verilator 提 PR 才是正解；仓内注入是等效绕行 ⇒
升级 submodule 时**须复核**）。

---

## 验收标准

> ⚠️ **期望值 MUST 手推 / 可复算**（禁抄工具输出）。

- [ ] **AC1** 🔴 **构建期指纹**：安装产物的 `CFG_CXXFLAGS_PCH_I` **非空** 且与
      `verilator -getenv CXX` 的族属匹配 ⇒ 判据 = 新护栏 ①
- [ ] **AC2** 🔴 **行为判据**：强制 split 编译**最小设计** ⇒ rc=0（修前 rc=2）⇒ 判据 = 新护栏 ②
- [ ] **AC3** 🔴 **负向对照**：把安装根**副本**的 flag 清空 ⇒ 同一编译**必红**
      ⇒ ⇒ 证明 AC2 不是恒绿 ⇒ 判据 = 新护栏 `--self-test`
- [ ] **AC4** 🔴 **真实路径覆盖**：`riscv-mini-rv32i-soc`（3066 行 Verilog · **自动** split）
      **不加任何 flag** 编译 ⇒ rc=0 ⇒ ⇒ 判据 = `verify_examples_rtl_sim.sh`（**零** `--output-split 0`）
- [ ] **AC5** 🔴 **护栏全绿**：`scripts/verify_examples_rtl_sim.sh` **11 项 PASS** +
      `--self-test` **8 条投毒**（含新增的 ③）各自转红
- [ ] **AC6** 🔴 **回归**：`ctest -LE perf` **255/255** + 既有护栏全绿
- [ ] **AC7** 🔴 **submodule 干净**：`git -C third_party/verilator status --short` 为空
      ⇒ ⇒ 修复**只**在仓内 CMake 层（升级 submodule 不丢补丁）
- [ ] **AC8** 🔴 **文档同步**：`docs/developer_guide/verilator-integration.md` 记根因 + 修复 + 判据；
      `docs/CHANGELOG.md` 一条

### AC ↔ task 承载矩阵（**每个 AC MUST 有 task 承载**）

| AC | task |
|---|---|
| AC1 · AC2 · AC3 | **T2**（新护栏 + `--self-test` 负向对照） |
| AC4 · AC5 | **T3**（去掉 `--output-split 0` + 护栏复跑） |
| AC6 | **T4**（回归） |
| AC7 | **T1**（CMake 注入 ⇒ submodule 零改动） |
| AC8 | **T5**（文档 + CHANGELOG） |

---

## Capabilities

| capability | 动作 | 理由 |
|---|---|---|
| （**无**） | — | ⚠️ 本 change 修的是**构建工具链**（vendored Verilator 的 configure 变量），**不**改任何 capability 的**产品契约**：端口面 / manifest schema / 层级语义**零改动** ⇒ ⇒ **无 spec delta** ⇒ ⇒ 归档时**无** spec 合并（`openspec archive` 会照常移动 change 目录） |

⚠️ **与已归档 change 的关系**：`rtl-productization-surface-2026-11`（T5/B7）在护栏里用
`--output-split 0` 绕行过本缺陷 ⇒ ⇒ **本 change 把那个绕行删掉** ⇒ ⇒ 两者的护栏改动**必须同批**
（否则中间态 = 绕行已删但工具链未修 ⇒ ⇒ 护栏必红）。

---

## 用户可见变更

| 面 | 变更 |
|---|---|
| **构建** | `cmake --build build --target verilator` 的产物多出正确的 PCH 选项 ⇒ ⇒ **自动 split 的模型可编译** |
| **护栏** | `verify_examples_rtl_sim.sh` 的 flags 变化 ⇒ ⇒ **缓存 key 变化** ⇒ ⇒ 首次跑会重编 11 个模型（一次性） |
| **新增护栏** | `scripts/verify_verilator_pch.sh`（+ CI 一步 + 可达性登记表一行） |
| **文档** | `docs/developer_guide/verilator-integration.md` 新增「PCH 与自动 split」小节 |

---

## Out of Scope

- ❌ **改 `third_party/verilator` 内部**（submodule 保持干净 ⇒ ⇒ 不做「就地打补丁」）
- ❌ **升级 verilator submodule**（另案；升级后**必须**复核本 change 的注入是否仍需要）
- ❌ **给上游 verilator 提 PR**（那是上游仓的动作；本仓只登记为「正解」并保留等效绕行）
- ❌ **改模型 / 产物 / capability 契约**（零改动）
- ❌ **改 `VerilatorBackend` 的实现**（它受益于工具链修复，但**不需要**代码改动 ⇒ ⇒ 未来更大的模型
  才可能触发 ⇒ ⇒ 由 AC4 + 既有 e2e 覆盖）

---

## 🚪 门（**开工前 MUST 闭**）

| # | 事项 | 状态 |
|---|---|---|
| **P-1** | 用户 2026-11-17 裁决：护栏**不保留** `--output-split 0` ⇒ ⇒ 本 change 是**前提条件**（不是可选优化） | ✅ 已闭 |
| **P-2** | 与已归档 change 的**护栏改动同批**（删绕行 ⇒ 必须同时修好工具链） | ✅ 本 change 即该同批 |
| **P-3** | **submodule 零改动**（修复只在仓内 CMake 层） | ⬜ 开工时核对 `git -C third_party/verilator status --short` 为空 |
