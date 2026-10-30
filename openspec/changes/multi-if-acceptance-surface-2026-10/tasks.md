# Tasks: multi-if-acceptance-surface-2026-10

## 0. 只读核实（实施前）

- [ ] 0.1 核实 `elif` / `else_` 形参是 `const T &`（`if.h:43,51`）⇒ 实参已在进入
      函数体前完成转换 ⇒ 判据 MUST 前置到模板重载
- [ ] 0.2 核实 `ch_uint<M>→ch_uint<N>`（M>N）**非 explicit** 且走 `bits<N-1,0>`
      （`uint.h:110-122`）
- [ ] 0.3 核实 `HardwareType<port<…>>` = **true**（`operators.h:40-42` + `io.h:125/201`）
      ⇒ **MUST NOT** 用 `HardwareType` 判 port
- [ ] 0.4 核实 port 只有带约束的 `operator ch_uint<W>()` + 无约束 `operator lnode<T>()`
      （`io.h:229/232`）⇒ `ch_bool` 端口 MUST 经 `lnode` 兜底
- [ ] 0.5 核实 `select` 在 `result_width <= 1` 时返回 `ch_bool`（`operators.h:134-135`）
- [ ] 0.6 核实 `CHERROR` / `CHFATAL` **只打日志不抛异常** ⇒ MUST 用 `CHFATAL_EXCEPTION`

## 1. 实现（`include/chlib/if.h`）

- [ ] 1.1 `ctor` / `elif` / `else_` **模板化**（`<typename U>` + `if constexpr`）
- [ ] 1.2 **D1** 硬件分支宽度判据：`ch_width_v<VT> <= ch_width_v<T>`；更宽 ⇒
      `static_assert`（消息含两个宽度 + `if_then<ch_uint<W>>` 指引）；窄者零扩展
- [ ] 1.3 **D2-a** `ch_literal_impl<V,W>` 按 `W` 走同一条静态判据（走 1.2 即可）
- [ ] 1.4 **D2-b** 裸整数字面量：豁免类型宽度判据 + 建图期**值域检查**
      （`CHFATAL_EXCEPTION`）
- [ ] 1.5 **D2-c** 负数算术字面量 ⇒ 拒绝
- [ ] 1.6 **D3** `operator T()`：`branches_.size() < 2` ⇒ `CHFATAL_EXCEPTION`；
      顺带删不可达的 `branches_.empty()` 分支（Zero-Debt）
- [ ] 1.7 **D4** port 归一化（`branch_vt` + `normalize_branch`，含 `lnode<ch_bool>`
      兜底）；`T` 为纯算术 ⇒ `static_assert` 拒绝
- [ ] 1.8 **D5** `operator T()` 返回类型恒为 `T`（`result_width<=1` 时收敛）
- [ ] 1.9 **D6** 六种写法 MUST 保持合法（回归 `conditional_demo` 全绿）
- [ ] 1.10 头注释写「一句话契约」（**引用** spec，不复制，防漂移）

## 2. 护栏（`scripts/verify_multi_if_width.sh`）

- [ ] 2.1 实现 5 步机制（见 `design.md` §2）：编译 / 关键词 grep / **中性化对照** /
      正向 fixture / fail-closed
- [ ] 2.2 每个「拒绝」fixture **MUST** 配中性化对照（用 `-D` 宏中性化对应断言）
- [ ] 2.3 每个 fixture 在 **GCC 与 Clang 各编一次**
- [ ] 2.4 挂 CI **两处**（主路径 + `--self-test`）
- [ ] 2.5 `--self-test` 用 fixture 自证（去掉一条对照 ⇒ 护栏必须红）

## 3. 测试

- [ ] 3.1 拒绝 fixture ×5（`R_A` / `R_E` / `R_D` / `R_lit` / `R_neg`）
- [ ] 3.2 建图期拒绝 ×2（`T_B` 单分支 / `T_range` 字面量越界）⇒ `REQUIRE_THROWS_AS`
- [ ] 3.3 正向 fixture ×8（`P_wide` / `P_else` / `P_multi_no_else` / `P_explicit` /
      `P_intlit` / `P_port` / `P_port_bool` / `P_bool1`）
- [ ] 3.4 运行期金标：`P_wide` 的 `0x0A5` / `0x00F`（**手推**，MUST NOT 抄仿真输出）

## 4. 变异验证（每条**先确认编译成功**）

- [ ] 4.1 **M1** 宽度断言放宽 ⇒ `R_A` / `R_E` 意外通过 ⇒ 护栏红
- [ ] 4.2 **M2** `zext` → `sext` ⇒ `0x1A5` ⇒ 红
- [ ] 4.3 **M3** 去掉值域检查 ⇒ `T_range` 不抛 ⇒ 红
- [ ] 4.4 **M4** 删 `size() < 2` ⇒ `T_B` 不抛 ⇒ 红
- [ ] 4.5 **M5** 去 port 归一化 ⇒ `P_port` 退回模板深渊 ⇒ 红
- [ ] 4.6 **M6** 「更宽 ⇒ 拒绝」误写「不等 ⇒ 拒绝」⇒ `P_wide` 失败 ⇒ 红
- [ ] 4.7 **如实登记**无判别力的变异：「zext 换恒等存储」不红 ⇒ 该维度依赖编译期证据
- [ ] 4.8 变异 harness 用 `scripts/lib/mutation_harness.py`（已入仓，含三处硬化）

## 5. 验收与归档

- [ ] 5.1 `ctest -LE perf` 零回归
- [ ] 5.2 `run_all_samples.sh` 全绿（**含 `conditional_demo` 自检**）
- [ ] 5.3 `run_all_ported_tests.sh` 全绿
- [ ] 5.4 全部护栏（含 `--self-test`）exit 0
- [ ] 5.5 计数锚点核对并同步（新测试文件 ⇒ ctest 条目数可能 +N ⇒ 更新
      `CTEST_TOTAL` / non-perf / `tests/` 文件数）
- [ ] 5.6 归档（**禁** `--skip-specs`；合并 MODIFIED ×1 + ADDED ×1）
