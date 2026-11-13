# Tasks: 固件装载链路（显式映射 + 统一加载端口 + 转储器 / ELF 装载）

> **实施顺序固定**（见 `design.md` §1）⇒ ⇒ MUST 按序执行。
> ⚠️ **L1 与 L2 MUST 同批**（二者都改 SoC 顶层端口契约 ⇒ ⇒ 公开面登记只写一次）。
> ⚠️ **与 `memory-timing-contract` 同批实施** ⇒ ⇒ **先做那一个**（D-TCM 读时序先定）。

## S0 — 开工前基线（**零改动**）

- [ ] 记录基线：`ctest -N` 总数 · 现有 `prog_*` 语义（字索引）· 编码器副本数（**3 处**）
- [ ] 确认工作区干净（`git status --short` 为空）⇒ ⇒ **禁** `git add -A`

```bash
cd /workspace/project/CppHDL
ctest --test-dir build -N | tail -1
grep -rn 'has_store(t, 0x' tests/ | wc -l
bash scripts/verify_test_counts.sh
```

## S1 — L1：显式映射表 + 越界诊断（**先红后绿**）

- [ ] 写**越界投毒判据**：地址设到区域外 ⇒ ⇒ **MUST 有诊断** ⇒ ⇒ **先红**
- [ ] `examples/riscv-mini/src/rv32i_pipeline_top.h`：显式 `{base, size}` 表
  ⇒ ⇒ `size` **由容量派生**（禁手写）· `dtcm.base == 0x8001_0000`
- [ ] **PC 复位值 MUST 由 `itcm.base` 派生**（禁第二个字面量）
- [ ] ⚠️ **对齐要求是 16 KB 而非 4 KB** ⇒ ⇒ 「截断等价的最小判据」按 16 KB 取
- [ ] ⚠️ **受影响地址字面量清单**逐条登记（`grep -n 'has_store(t, 0x'` ⇒ ⇒ **禁**只报计数增量）

```bash
cd /workspace/project/CppHDL
cmake --build build -j$(nproc)
bash scripts/verify_port_naming.sh
```

## S2 — L2：统一加载端口（**与 S1 同批**）

- [ ] 加载接口语义 = `(we, addr[字节地址], data[word], size)` ⇒ ⇒ 译码到 I-TCM / D-TCM
- [ ] ⚠️ **新端口 MUST 具名**（R17 基线 ⇒ ⇒ 护栏 P1 会拦）
- [ ] 公开面登记：`docs/CHANGELOG.md` + `.rddf/checks/public-api-surface.md` 各 1 行
  （⚠️ **前置登记** ⇒ ⇒ 「字索引 → 字节地址」的迁移指引）
- [ ] 旧「字索引」语义 MUST NOT 残留

```bash
cd /workspace/project/CppHDL
cmake --build build -j$(nproc)
bash scripts/verify_port_naming.sh
bash scripts/verify_public_api_visibility.sh
```

## S3 — L3：编码器库化

- [ ] 新增 `include/cpu/riscv/rv32i_asm_encode.h`（header-only · `constexpr`）
- [ ] 既有 **3 处副本**归一为 **1 份**（`enc_r` / `enc_i` 等定义**唯一**）
- [ ] ⚠️ **`FirmwareEncoderCrossCheck` MUST 保留**（手算常量 vs helper）⇒ ⇒ **禁**顺手删
- [ ] 新头文件有**仓内消费者**（既有测试 + 新工具）⇒ ⇒ 不违反 ZERO-DEBT

```bash
cd /workspace/project/CppHDL
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure -R 'rv32i|firmware|encoder'
bash scripts/verify_header_self_contained.sh
```

## S4 — L4：转储器工具

- [ ] `tools/rv32i-fw-dump` 可独立运行 · 三种输出（`vh` / `mem` / `manifest`）
- [ ] `manifest` MUST 携带**基址** + **入口** + 「字索引 → 地址」映射
- [ ] ⚠️ `tools/` 下的二进制 **MUST NOT 链 `examples/**`**（独立自测载体）

```bash
cd /workspace/project/CppHDL
cmake --build build --target rv32i-fw-dump -j$(nproc)
bash scripts/verify_example_test_wiring.sh
```

## S5 — L5：ELF 装载

- [ ] 解析 ELF32 header + `PT_LOAD` ⇒ ⇒ `p_paddr` / `p_filesz` / `p_memsz`
- [ ] `.bss`（`memsz > filesz`）**补零** ⇒ ⇒ 旧内容不留
- [ ] ⚠️ **非零重定位 ⇒ 报错**（不是静默忽略）
- [ ] ⚠️ **不解析符号表** ⇒ ⇒ 符号留给未来工具

```bash
cd /workspace/project/CppHDL
cmake --build build --target rv32i-fw-dump -j$(nproc)
ctest --test-dir build --output-on-failure -R 'firmware|elf'
```

## S6 — L6：装载可观测性 + 收口

- [ ] 装载后**走真实读通路**读回 ⇒ ⇒ TB 断言「装进去的值 == 期望」
  ⇒ ⇒ ⚠️ **不是**「loader 返回成功」
- [ ] 一个带 `.data` 的固件可装并读回 ⇒ ⇒ **D-TCM 加载通路非零覆盖**
- [ ] 计数锚点同步（用 `scripts/update_test_count_anchors.sh` ⇒ ⇒ **禁**手写数字）
- [ ] 全护栏复跑 + 验收报告落 `.rddf/reviews/`

```bash
cd /workspace/project/CppHDL
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
bash scripts/verify_openspec_delta.sh
```

## Out of Scope（**本 change 不做**）

- ❌ 符号表解析 / 重定位实现（**非零重定位报错**即可）
- ❌ MMIO 设备接入（**只留位**）
- ❌ CSR / ECALL / FENCE —— 属 `rv32i-isa-gap-closure`
- ❌ cache 接入 —— 独立立项
