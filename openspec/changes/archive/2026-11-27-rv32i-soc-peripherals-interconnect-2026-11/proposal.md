# Proposal: S4b —— 5 个 Lite 外设 + **新写 N×M Lite 互联**

> **来源**：`.rddf/improvements/rv32i-soc-platform-2026-11.md` 的 **S4b** 批次 +
> **用户 2026-11-27 裁决：互联形态 = ③ 新写 N×M Lite 互联**（决策材料
> `.rddf/checks/decision-request-g6-mmio-ownership-2026-11.md` 文首）。
> **前置**：S4a（`2026-11-24-rv32i-soc-mmio-channel-2026-11`）**已归档**；IP 冻结批已闭。
> **spec 变更**：`axi-interconnect-routing` **ADDED ×5**（端口面 / 路由锁存 / 仲裁与串行 / DECERR / 窗口表同源）。

---

## Why

1. **S4a 的 2×2 矩阵装不下 G-6 的地址表**：矩阵只有 **2 个从口**，而库层表有 **7 个 MMIO 段**
   ⇒ ⇒ 现装配（`SocLiteMap`）只映射了 GPIO + 一个「核永不发」的哨兵窗口
   ⇒ ⇒ 其余 6 段全部落在 **DECERR**。
2. **用户已裁互联形态 = ③ 新写 N×M**（不是级联矩阵、不是单窗口 + 二级译码）
   ⇒ ⇒ 本 change 的 IP 面因此是**新增**，不是改造。
3. **S4b 是 S4c / S5 / S6 的地基**：DMA（S4c）是**第二个 master**；L2/DRAM（S6）是**追加 slave**
   ⇒ ⇒ 互联 MUST 在参数上支持 N 主 / M 从，否则每加一个设备就要重写互联。
4. **窗口表 MUST 与库层地址表**同源**（单一真相）**：S4a 的 `SocLiteMap` 是手写的
   `windows[2]` ⇒ ⇒ 再加 6 段就会变成**第二份地址真相** ⇒ ⇒ 必须改为**派生 + 编译期断言**。

## What Changes

### A. 新 IP：`AxiLiteCrossbar<Map>`（`include/axi4/axi_lite_crossbar.h`）

- **端口面固定 2 master × 6 slave**（每面 19 根 + `rst_n`，全部具名）：ADR-075 E1 要求
  端口具名，数组化端口只能得到 `unnamed_input_N` ⇒ 会新增命名债 ⇒ 端口数不做模板展开。
- 参数化自由度保留在 `Map::windows[W]`：`W` 任意，`target` = slave 索引或 `kErr`
  （多出来的段 —— clint / dma —— 指 `kErr`，**不留死从口**）。
- **实现路线（用户 2026-11-27 约束）：数据面复用 `chlib` 的 stream 组合子**，不手写
  valid/ready 转发：
  - 5 条通道各建一条 stream 平面（AW/W/B/AR/R）；
  - 选主用 `chlib::stream_mux` · 选从用 `chlib::stream_demux`（第 `M` 路 = `ERR`）；
  - 仲裁用 `chlib::round_robin_selector`（每方向独立；握手当拍才锁存状态 ⇒ 未接受的请求
    MUST 保持、MUST NOT 被吞）；
  - 接线纪律照抄 `examples/stream/`（`as_master()` + `ready` 用 `<<=`）。
- 语义**复用** `axi-interconnect-routing` 的既有契约：AW/AR 握手当拍**锁存路由** ·
  写/读路由**各自独立** · W/B 用写路由、R 用读路由 · 在途门控 · 单次赋值。
- 未映射 / `ERR` 窗口：**互连层自应答 DECERR**（Lite 单拍；不产生 `last`）。

### B. 库层地址表：追加 `kTimer` 段（**D-1 · ✅ 已确认 2026-11-27**）

- `kTimer` 追加在 `kDma` 之后（`0x8010_7000`）⇒ ⇒ **既有 7 段地址零移动**
  ⇒ ⇒ `kMmioBytes` 由 `7×4KB` 变 `8×4KB`（派生，禁手写）。
- ✅ **D-1 已确认**（用户 2026-11-27「继续 P2」）⇒ ⇒ 采纳本节取值；否决路径（TIMER 延到 S5）
  **未发生**。

### C. SoC 装配（`examples/riscv-soc/src/rv32i_soc_top.h`）

- `AxiLiteMatrix2x2` → `AxiLiteCrossbar<SocCrossbarMap>`：
  - m0 = 核的 MMIO 主口（19 根，直连）；m1 = **DMA 预留**（tie 0，S4c 接真设备）。
  - slaves：`gpio` / `uart` / `timer` / `spi` / `i2c` / `pwm`（6 个真实从口）。
  - windows：8 段（G-6 的 7 段 + `kTimer`），其中 **CLINT / DMA 段 → `ERR`**
    （设备分别在 S5 / S4c 接入；**不留死从口**）。
- `SocCrossbarMap` **派生**自 `rv32i_mem_map.h`，禁手写基址。

### D. 判据与登记面

- 每个外设一条**总线级** RD/WR 回环（手推地址 + 值）。
- `.rddf/checks/soc-rtl-expectations.md` 增「S4b」节（每外设的地址 / 寄存器 / 期望值）。
- `.rddf/checks/examples-rtl-sim-targets.tsv` 的 `tb_riscv_soc` 行：`min_checks` 重算（等式）。
- 公开面：`.rddf/checks/public-api-surface.md` + `docs/CHANGELOG.md`（新 IP + SoC 形态变化）。

## 验收标准

- [ ] **AC1 路由完备（单元级）**：`AxiLiteCrossbar` 对 **每个 (master, slave) 对**
      （2×6 = **12 对**）提供完整路由（逐对覆盖）；任一配对缺失 ⇒ 判据红。
- [ ] **AC2 锁存 / 在途**：AW/AR 握手后目标从**锁存值**转发；在途期间 `m*_awready` /
      `m*_arready` MUST 为 0；第二条 AW MUST NOT 被静默吞掉。
- [ ] **AC3 仲裁**：两 master 同时请求同一 slave ⇒ **AW / AR 各自**轮转（不是固定优先级）；
      未获授权的请求 MUST 保持（不丢）；焊接受当拍才锁存路由；指针指向无请求者时让位 + 回绕。
      另：**同一 slave 的写 / 读各自串行**（第二个 `ready` 与 `valid` 一起为 0）。
- [ ] **AC4 DECERR**：`ERR` 窗口（CLINT / DMA）与**完全未映射**地址
      都返回 `bresp = rresp = DECERR`，且**不挂死**（单拍完成）。
- [ ] **AC5 窗口表同源**：`SocCrossbarMap::windows` **派生**自 `rv32i_mem_map.h`
      ⇒ 判据 = 编译期 `static_assert`（逐段 base/size 相等）+ 运行期「每段首字节只命中自己」。
- [ ] **AC6 外设回环（总线级）**：6 个外设各 1 条 RD/WR 回环（手推地址 + 值）⇒ 全绿；
      写入值 MUST 手推，禁从仿真输出转录。
- [ ] **AC7 零回归**：核级 24 判据 / 既有 SoC 10 判据 / 双后端 parity / 扁平产物行为
      **零变化**；A8 的 DECERR 探针（`0x8010_0000`）**语义不变**（仍是 CLINT 段 ⇒ `ERR`）。
- [ ] **AC8 形态登记**：SoC 根端口**不变**（10）· 层次化产物 module 数 = 旧 + 6
      （crossbar + 5 新外设）· `--check-products` 的 module / 端口 / 未驱动输入 0 全绿。
- [ ] **AC9 反向判据**：把 crossbar 的窗口派生改回手写字面量（或把 `ERR` 指向 s0）
      ⇒ AC4 / AC5 **至少一条转红**（证明判据有判别力）。

## Out of Scope（明确不做）

| 项 | 理由 | 归属 |
|---|---|---|
| CLINT 实体接入 | 它带 `mtip`，依赖核 `irq` 口 + privilege | **S5** |
| DMA 实体接入（`AxiDma`） | 需取指侧总线化 + 装载路径 | **S4c** |
| 外设**物理引脚**上根端口（uart/spi/i2c/pwm 的片外信号） | G-8（外部总线/根端口面）**未裁** ⇒ 现在加 = 提前改变产品端口面 | **G-8 / S6** |
| `bresp` / `rresp` 的**错误上报给软件** | 核侧 master 当前不消费错误响应（S4a 已登记） | **S5**（与 trap 同批） |
| L2 / DRAM | S6 | — |
