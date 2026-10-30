# Tasks: chlib-guard-contribution-priority-2026-10

## 0. 只读核实（实施前）

- [x] 0.1 核实 ADR-050 `contrib` 的**旧**链构造方向（正向状态序 ⇒ 高编号优先）
- [x] 0.2 核实共享组合器的方向（`rbegin→rend` ⇒ 先收集的在外层）
- [x] 0.3 核实 `is_in(S)` 的**互斥性是构造保证**（同一寄存器对互不相同的枚举
      字面量作比较）
- [x] 0.4 核实 `chlib-state-machine-semantics` 主 spec **无**优先级条款
- [x] 0.5 核实 `chlib-conditional-semantics` 主 spec **无**析构兜底条款、
      **无**接受面限制

## 1. 契约面（builder）

- [x] 1.1 `chlib-state-machine-semantics`：**ADDED ×1**「状态守卫贡献的优先级
      MUST 是『先匹配者胜』」+ 3 个 Scenario
- [x] 1.2 `chlib-conditional-semantics`：**ADDED ×1**「条件块的析构 MUST 兼容
      抛出路径」+ 2 个 Scenario
- [x] 1.3 `chlib-conditional-semantics`：**MODIFIED ×1**「权威表面 MUST 唯一」
      ⇒ 追加接受面限制（E1/E2/E3）的**已知限制 NOTE** + 1 个 Scenario

## 2. 实现面（builder）

- [x] 2.1 `include/chlib/guard_contribution.h`：头注释明确「先匹配者胜」的
      **方向** + 记录「抽取前后的方向差异」+ 指向本 change（**零行为变更**）
- [x] 2.2 **结构级断言**：新增 `tests/chlib/test_guard_contribution.cpp`
      ⇒ 断言最外层 mux 的守卫 = **第一个**贡献的守卫
- [x] 2.3 断言在**单测**里做（**不依赖**运行期求值 ⇒ 运行期不可观测的必然）

## 3. 文档面（builder，**决策未变**，仅订正事实表述）

- [x] 3.1 `docs/adr/ADR-051-chlib-conditional-semantics.md` **D10**：把
      「抽取是**纯重构**，不是行为变更」**订正**为如实表述（链序/优先级维度
      **是**行为变更；新行为符合仓内惯例 ⇒ 旧行为属偶然反转）
- [x] 3.2 同 ADR **§5.4** 的 M6 条目：补「链序**运行期**不可观测 ⇒ 金标钉不住
      ⇒ ⇒ **必须**结构级断言」
- [x] 3.3 同 ADR **§7**：追加本轮的**实施期实测**记录（Verilog diff 的
      逐字节证据 + 口径裁决）

## 4. 变异验证（每条**先确认编译成功**；注入器满足三处硬化）

- [x] 4.1 M1 `compose_guard_chain` 的 `rbegin→rend` ⇒ `begin→end`
      ⇒ **结构级断言**转红
- [x] 4.2 M2 同一变异 ⇒ **运行期**用例**仍绿**（反向判据 ⇒ 证明「运行期
      不可观测」这条事实）
- [x] 4.3 注入器三处硬化：`--no-tests=error` · target ⊆ `build_targets` 的
      fail-closed 校验 · 还原后与 `git show HEAD:` 比对

## 5. 验收与归档

- [x] 5.1 既有金标**逐项不变**（`uart_tx` 的 `busy` 高 **3906** 拍 + `0xA5`
      位序；`complex_e2e` 的 `0x96` MSB-first 16 位 + 状态序列 + `bit_count`
      复位）
- [x] 5.2 `ctest -LE perf` 零回归
- [x] 5.3 全部护栏（含 `--self-test`）exit 0
- [x] 5.4 计数锚点核对（⚠️ **新增**测试文件 ⇒ ctest **条目数 +1** ⇒
      `CTEST_TOTAL` 202 → **203** · non-perf 191 → **192** · `verilator_e2e` **38**
      不变 · `tests/chlib/test_*.cpp` 29 → **30** ⇒ 与
      `scripts/verify_test_counts.sh` 同步）
- [x] 5.5 归档（**禁** `--skip-specs` ⇒ 两个 capability 的 delta 合并）
