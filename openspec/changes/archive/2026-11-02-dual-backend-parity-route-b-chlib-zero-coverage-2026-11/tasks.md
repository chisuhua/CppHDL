# Tasks：路线 B（新文件）—— chlib 三个零覆盖组件进双后端 parity

## C0 —— 先做「可转性三问」（**第一步不是写代码**）

- [x] 产出**实测覆盖矩阵**：「含 `DUAL_BACKEND_TEST_PAIR(` 调用」的源文件全集
      × `include/chlib/*.h`」⇒ 得出**零 dual 覆盖**的 chlib 组件清单
      （`arithmetic_advance.h` · `bitwise.h` · `onehot.h` · `selector_arbiter.h` ·
      `stream_arbiter.h` · `stream_pipeline.h` · `stream_width_adapter.h`）
- [x] 逐个候选文件答三问（重复？ · 有判别力？ · 与后端无关？）⇒ 10 个候选文件里
      **只有 3 个**既有价值又形态可行（详见 proposal 的表）
- [x] ⚠️ **发现计划漏项**：`test_bitwise.cpp` 与 `test_arithmetic_advance.cpp`
      **不在** E/F 批清单里 ⇒ ⇒ 本批补做 bitwise；`arithmetic_advance` 留下一批
- [x] ⚠️ **第 4 问**（本批新增）：「形态上不可转换」≠「不值得覆盖」⇒ ⇒ 必须允许
      「新写」路线（自带 DUT + 手推期望），并**如实登记它是新写**

## C1 —— 三个新文件（**既有单后端测试 MUST NOT 修改**）

- [x] `tests/chlib/test_bitwise_dual.cpp`（2 pair / 100 断言/后端）
      ⇒ DUT 1 `BitwiseDetectorsDut`（6 个检测器）· DUT 2 `BitwiseOpsDut`
      （位反转 / 位交换 / 2 种位域插入）
- [x] `tests/chlib/test_onehot_dual.cpp`（3 pair / 16 断言/后端）
      ⇒ `onehot_dec_module<4>` · `onehot_enc_module<4>`（**零 DUT 复制**）·
      函数式 `onehot_dec`/`onehot_enc`（运行期输入）
- [x] `tests/chlib/test_forwarding_dual.cpp`（1 pair / 10 断言/后端）
      ⇒ `chlib::ForwardingMux<32>`（**零 DUT 复制**）× 5 个档位组合
- [x] `tests/CMakeLists.txt`：3 条 `add_dual_backend_test`
- [x] 既有 `tests/chlib/test_bitwise.cpp` / `test_onehot.cpp` / `test_forwarding.cpp`
      **均未修改**

## C2 —— 判据（N2′）与判别力

- [x] **N2′**：`200 == 2 × 100` · `32 == 2 × 16` · `20 == 2 × 10`
- [x] 变异 **15/15 全抓**（每条先确认仍可编译；还原用 `cp` 后重建）：
      bitwise M1–M9（链方向 / 逐位贡献 / 移位量 / 反馈取反 / 忘记清域 / 源位未互换）·
      onehot O1–O3（`bit_select` 索引 / 移位量 / 比较取反）·
      forwarding F1–F3（`is_jump` 取反 / 档位比较取反 / 两档源互换）
- [x] ⚠️ **如实登记两条判别力边界**：
      · `onehot` 的「链方向」在**合法单置位输入**下不可判别（方向只改多置位结果，
        而多置位是**未定义**行为 ⇒ 本文件**不**断言它 ⇒ 不冻结未定义行为）
      · `bit_field_insert` 的 `inserted2` 与 `inserted3` 同函数 ⇒ M5 先在
        `inserted3` 转红 ⇒ `inserted2` 无**专属**变异（共享断言组）
- [x] ⚠️ **如实登记一条期望值手算笔误**：`inserted2` 的 `0xE2|0x14` 被写成
      `0xE6`（实为 `0xF6`）⇒ 被**双后端一致地**证伪 ⇒ 改为按实现公式逐行重算
- [x] 全部输入端口**逐个显式驱动**

## C3 —— 计数与验收

- [x] 实测（**全量重建后**）：`ctest -N` **229** · `-L verilator_e2e` **61** ·
      `-LE perf` **218** · `verify_dual_backend_parity.sh` **PARITY_COUNT=52 /
      PAIRS=249** · `tests/chlib/test_*.cpp` **44**
- [x] AGENTS.md 锚点同步（4 个 anchor + 3 条 NOTES 计数 + NOTES 条目）
- [x] ⚠️ **全量重建后**跑 `ctest -LE perf` **218/218**
- [x] 全部 `scripts/verify_*.sh` exit 0
- [x] 归档：`--skip-specs`（`.openspec.yaml` 已显式声明）
