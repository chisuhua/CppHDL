# Tasks: dual-backend-parity-batch-b-2026-11

> 勾选规则：**commit 之后**按 `git show --name-only <sha>` 核对实际改动再勾。

## T1 筛选纪律（**三条硬门槛**）

- [x] **T1.1** 门槛 ①：**零局部 DUT 类**（局部类无法传给文件作用域的宏
      ⇒ ⇒ 需提升 ⇒ ⇒ 与「原文件 MUST NOT 修改」冲突）
- [x] **T1.2** 门槛 ②：**零平凡断言**（`REQUIRE(true)` ⇒ ⇒ dual 零增量价值）
      ⇒ ⇒ 淘汰 `test_axi_pwm_simple`（且它**自带 main** ⇒ ⇒ 不可用 macro）
- [x] **T1.3** 门槛 ③：**无 `std::cout` 重定向**（断言 C++ 侧打印 ⇒ ⇒
      双后端无意义 + stdout 全局状态会互相干扰）⇒ ⇒ 淘汰 `test_assert`
      的 2 个宏用例
- [x] **T1.4** ⇒ ⇒ **唯一通过全部门槛 = `tests/chlib/test_assert.cpp`**
      （DUT = `chlib::AssertChecker`，**库头类型** ⇒ ⇒ 零原文件修改）

## T2 🔴 成本模型的**二次订正**（**本 change 最重要的产出**）

- [x] **T2.1** 逐 **0.01 s** 打点（假 HOME 全冷）⇒ ⇒ **elaboration 只 0.012 s**
- [x] **T2.2** ⇒ ⇒ **10.66 s 是【C++ 编译 + 链接 `libVtop.so`】**
- [x] **T2.3** ⇒ ⇒ 第 2/3/4 个 pair **各自又花 ~10.6 s** ⇒ ⇒ ⇒
      **⇒ 【每个 pair 一次编译】**（根因：macro 为每个 pair 建独立 device
      ⇒ ⇒ ⇒ 上下文 verilog 不同 ⇒ ⇒ ⇒ **缓存 key 不同**）
- [x] **T2.4** ⇒ ⇒ **成本单位 = 【pair 数】**（不是 DUT 数 / 文件数）
- [x] **T2.5** ⇒ ⇒ **连带废止**「每 pair 恰 1 条 `REQUIRE`」这条约定
      （**当时给的理由「拆 pair 不增墙钟」是错的** ⇒ ⇒ 拆 pair = **增加**
      编译次数 ⇒ ⇒ ⇒ **断言强度 MUST NOT 被判据绑架**）
- [x] **T2.6** ⇒ ⇒ 判据改为 **XML reporter 逐 TEST_CASE 比对 `successes`**
      （**已实测可行**）

## T3 实现

- [x] **T3.1** 新增 `tests/chlib/test_assert_behavior.cpp`（**4 个 dual pair**）
- [x] **T3.2** **原 `tests/chlib/test_assert.cpp` MUST NOT 修改** ⇒ ⇒ 零回归
- [x] **T3.3** **期望值全部手推**（`failed = enable & !condition` 真值表）
- [x] **T3.4** `tests/CMakeLists.txt` 注册 `add_dual_backend_test`
- [x] **T3.5** ④ 号用例**保留 2 条 `REQUIRE`**（⇒ ⇒ 正是旧判据失效的场景
      ⇒ ⇒ 新判据覆盖它 ✔）

## T4 计数锚点同步（**每批必做**）

- [x] **T4.1** `CTEST_TOTAL` 206 ⇒ ⇒ **207**
- [x] **T4.2** `VERILATOR_E2E_TOTAL` 39 ⇒ ⇒ **40**
- [x] **T4.3** `DUAL_BACKEND_PARITY_TOTAL` 30 ⇒ ⇒ **31**
- [x] **T4.4** `DUAL_BACKEND_PARITY_PAIRS` 185 ⇒ ⇒ **189**
- [x] **T4.5** `tests/chlib/test_*.cpp` 31 ⇒ ⇒ **32**
- [x] **T4.6** 根 `AGENTS.md` 的**叙述值**同步（4 处）

## T5 plan 同步

- [x] **T5.1** 成本模型整段改写（单位 = pair 数 + 修正后的批次预算）
- [x] **T5.2** 验收模板判据改写（XML per-TEST_CASE）+ **废止**两版旧写法
- [x] **T5.3** 墙钟账本回填（A = 13.5 s · **B1 = 44.6 s** · 累计 58.1 s）
- [x] **T5.4** §4 标题「按 DUT 聚合」⇒ ⇒ **「按 pair 聚合」**

## T6 验收

- [x] **T6.1** `10 assertions in 8 test cases`（8 = 2×4 pair · 10 = 5×2）
      ⇒ ⇒ **verilator 半边真跑**（且**已用 XML per-TEST_CASE 判据核验**）
- [x] **T6.2** 冷缓存墙钟 **44.6 s** ⇒ ⇒ 已回填账本
- [x] **T6.3** `ctest -LE perf` 全绿（**196/196**）
- [x] **T6.4** 全部护栏 exit 0 · `verify_test_counts.sh` exit 0
- [x] **T6.5** `verify_dual_backend_parity.sh` 双锚点匹配
- [x] **T6.6** `openspec validate --type change --strict` ⇒ ⇒ **valid**

## T7 归档

- [x] **T7.1** ⚠️ **零 spec 变更声明**（`.openspec.yaml`：`schema` + `skip_specs`）
- [ ] **T7.2** critic 评审（本 change 独立预算）

## 验证命令

```bash
cmake --build build --target test_assert_behavior -j"$(nproc)"
./build/tests/test_assert_behavior                 # ⇒ 10 assertions / 8 test cases
./build/tests/test_assert_behavior -r xml          # ⇒ 逐 TEST_CASE 的 successes
ctest --test-dir build -LE perf --output-on-failure
bash scripts/verify_test_counts.sh
bash scripts/verify_dual_backend_parity.sh
openspec validate dual-backend-parity-batch-b-2026-11 --type change --strict
```
