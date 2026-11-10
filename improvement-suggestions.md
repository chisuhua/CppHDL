# improvement-suggestions —— 改进提案索引

> **本文件** = `.rddf/improvements/` 下 improvement 草稿的**索引**（一行一条）⇒ ⇒ ⇒
> 提案本体在 `.rddf/improvements/<name>.md`（★ **5 段 / 7 段**草稿 ⇒ 创建前 MUST 过
> `skills/rdd-workflow-brainstorm/scripts/pre_create_brainstorm_check.sh` 的 HARD-GATE 校验）。
>
> ★ **生命周期**：本文件（提案池 · 待审查）⇒ ⇒ **审查批准** ⇒ ⇒ `improvement-approved.md`（已批准 · 待实施）
> ⇒ ⇒ ⇒ 实施 / 归档后从本表移除。

| 提案 | 优先级 | 来源 | 添加时间 | 状态 |
|---|---|---|---|---|
| [mem-port-arg-cross-layer-2026-11](.rddf/improvements/mem-port-arg-cross-layer-2026-11.md) | P1 | ADR-066 §2.7 决策 ⑦（归下游独立立项）· 台账 §7 行 20 | 2026-11-09 | 📝 draft（**待 P4 的 architect 范围裁定**落地后才可转 approved ⇒ 才可立项为 change） |
| [chlib-dtor-noexcept-composability-2026-11](.rddf/improvements/chlib-dtor-noexcept-composability-2026-11.md) | P1 | 上游 issue #28 **第 ② 半**（析构 `noexcept(false)` ⇒ 类型不可组合）· **ADR-067（Accepted 2026-11-09 · 用户拍板）** | 2026-11-09 | ✅ **approved**（架构阻塞已解除 ⇒ **可立项为 change** ⇒ 实施 = phase-6 **P15**） |
| [public-api-remove-visibility-2026-11](.rddf/improvements/public-api-remove-visibility-2026-11.md) | P1 | 上游 issue #28 **第 ① 半**（公开 API 移除的跨仓可见性 · CHANGELOG 载体 · 判决规则边界）· phase-6 P12 | 2026-11-09 | ✅ **approved（用户拍板 R1 + R2）+ 已实施并归档**（change = `openspec/changes/archive/2026-11-09-public-api-remove-visibility/` · 实施 = phase-6 **P14**） |
| [rtl-emit-product-tooling-2026-11](.rddf/improvements/rtl-emit-product-tooling-2026-11.md) | P1 | 用户 2026-11-10 拍板「① 生成器作为项目提供的能力」+ 实测（`toVerilog` 产物 115 端口里 80 个是内部输出 · 1 个悬空活端口） | 2026-11-10 | 📝 draft（**metis 第 1 轮已处置** ⇒ 逐条订正记录在文末「metis 第 1 轮 gap 分析与处置」段 ⇒ 待用户拍板 ⇒ 拍板后转 builder 流程） |
| [port-naming-ratchet-2026-11](.rddf/improvements/port-naming-ratchet-2026-11.md) | P1 | 用户 2026-11-10 拍板「⑤ 端口命名按你建议」⇒ 方案 A′（框架先行 + 基线棘轮 · 2769 处声明不做一次性 codemod） | 2026-11-10 | 📝 draft（**metis 第 1 轮已处置** ⇒ 逐条订正记录在文末「metis 第 1 轮 gap 分析与处置」段 ⇒ 待用户拍板 ⇒ 拍板后转 builder 流程） |
| [firmware-load-chain-2026-11](.rddf/improvements/firmware-load-chain-2026-11.md) | P1 | 用户 2026-11-10 拍板「② 固件转储器 + ELF 装载」+ 实测（D-TCM 无加载通路 · 地址静默截断 ⇒ ELF 当前不可行） | 2026-11-10 | 📝 draft（**metis 第 1 轮已处置** ⇒ 逐条订正记录在文末「metis 第 1 轮 gap 分析与处置」段 ⇒ 待用户拍板 ⇒ 拍板后转 builder 流程） |
| [memory-timing-contract-2026-11](.rddf/improvements/memory-timing-contract-2026-11.md) | P1 | 用户 2026-11-10 拍板「#12 改成寄存器读」+ 实测（D-TCM 组合读 ⇒ 推断不出 BRAM · `data_ready` 是死端口） | 2026-11-10 | 📝 draft（**metis 第 1 轮已处置** ⇒ 逐条订正记录在文末「metis 第 1 轮 gap 分析与处置」段 ⇒ 待用户拍板 ⇒ 拍板后转 builder 流程） |
| [rv32i-dead-component-consolidation-2026-11](.rddf/improvements/rv32i-dead-component-consolidation-2026-11.md) | P2 | 用户 2026-11-10 拍板「#6 按你建议」+ 实测消费点普查（⚠️ 订正：非零消费者，是「被测但未被交付」） | 2026-11-10 | 📝 draft（**metis 第 1 轮已处置** ⇒ 逐条订正记录在文末「metis 第 1 轮 gap 分析与处置」段 ⇒ 待用户拍板 ⇒ 拍板后转 builder 流程） |
| [rv32i-isa-gap-closure-2026-11](.rddf/improvements/rv32i-isa-gap-closure-2026-11.md) | P1 | 实测（未知 opcode 静默当 NOP · `ExceptionUnit.mcause` 恒不可达）+ phase-6 P6「CSR/ECALL/FENCE MUST 独立立项」边界 | 2026-11-10 | 📝 draft（**metis 第 1 轮已处置** ⇒ 逐条订正记录在文末「metis 第 1 轮 gap 分析与处置」段 ⇒ 待用户拍板 ⇒ 拍板后转 builder 流程） |
| [examples-rtl-sim-coverage-2026-11](.rddf/improvements/examples-rtl-sim-coverage-2026-11.md) | P2 | 用户 2026-11-10 拍板「#3 按你建议」+ 实测（生成产物形态零测试 · 独立 RTL 仿真仅 1 个临时实例 · verilator ≈8s/设计） | 2026-11-10 | 📝 draft（**metis 第 1 轮已处置** ⇒ 逐条订正记录在文末「metis 第 1 轮 gap 分析与处置」段 ⇒ 待用户拍板 ⇒ 拍板后转 builder 流程） |
