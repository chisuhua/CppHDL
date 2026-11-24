// rv32i_core_top.h
//
// RV32I 核 IP 顶层（pipeline + I-TCM + D-TCM）。
//
// ⚠️ 端口契约（ADR-046 D10 / D10-rev，2026-10-25 C3
//    `rv32i-soc-dual-parity-2026-10`）：
//    - 观测 MUST 走顶层 `ch_out`（verilator 只能读顶层端口）；
//    - 固件 MUST 能经 `prog_*` 写进 I-TCM（此前本顶层根本没接 TCM 写端口
//      ⇒ imem 恒 0 ⇒ 只能跑 NOP）；
//    - 端口集合由 `scripts/verify_no_duplicate_rv32i_headers.sh` 的 C6 快照钉住。
//
// ⚠️ TCM 规模 `ADDR_WIDTH = 12`（4096 字 = 16 KB）：
//    `1 << 20` 字的 `ch_mem` 在 verilator 下是 4 MB 展开数组，对 parity 固件
//    毫无收益。代价 = **地址高位被截断**（`pc` / 数据地址只看低 12 位）
//    ⇒ 固件与数据 MUST 落在 < 0x1000。
//    IF 阶段 `pc_reg` 复位为 0x80000000（`stages/if_stage.h`），其低 12 位为 0
//    ⇒ 从字 0 开始取指，按 +4 递增。
//
// ⚠️ **数据前推已接入**（ADR-047 D2）：前推 mux = **既有**的 `chlib::ForwardingMux<32>`
//    （装在**流水线装配层** `rv32i_pipeline.h::build_forwarding_paths()` ⇒ ⇒ **不在**
//    `HazardUnit` 里）⇒ ⇒ ADR-046 D3 #14 的组合环风险已由「独立 mux」消解
//    （D5 悬置项指的正是「把 EX 级组合输出接回 hazard」那条环饵 ⇒ ⇒ 不要再照那条接线）
//    ⇒ ⇒ `HazardUnit::rs1_data` / `rs2_data` 仍是**直通**（组件层语义 · 不变）
// ⚠️ **固件的冒险处理 MUST 以 ADR-047 §5.5 + ADR-069 为准**（档位 1 **不覆盖 load**
//    · load-use 停顿的拍数由 ADR-069 决策 A0 规定）⇒ ⇒ 本文件**不复制**其判据。
#pragma once

#include "ch.hpp"
#include "component.h"
#include "cpu/pipeline/rv32i_pipeline.h"
#include "cpu/pipeline/rv32i_tcm.h"
#include "cpu/cache/d_cache_sync.h"
#include "axi4/tcm_axi_slave.h"
#include "cpu/riscv/rv32i_mem_map.h"
#include "axil_mmio_master.h"   // 核侧 AXI4-Lite 主设备（S4a · G-9.4）

#include <cstdint>
#include <vector>

using namespace ch::core;
using namespace riscv;

class Rv32iCoreTop : public ch::Component {
public:
    // ⚠️ **2026-11-14 · L1**：容量真相**上移到库**（`rv32i_mem_map.h`）
    //    ⇒ ⇒ 本层**不再**持有第二个字面量 ⇒ ⇒ 声明与实现**不可能**不一致
    static constexpr unsigned kTcmAddrWidth = riscv::kTcmAddrWidth;

    __io(
        // 时钟 / 复位
        // ⚠️ **2026-11-14 · R8 改判（A2 · 用户裁决）**：**`ext_clk` 端口已删除**
        //   ⇒ ⇒ 理由链：① 它是**死端口**（被赋值但**零消费者** —— 产物里**所有**
        //   流水线寄存器都锁在框架 `default_clock`：`always_ff @(posedge default_clock …)`
        //   ⇒ ⇒ 对行为**零影响**）② 「SoC 由 ext_clk 驱动」这个直觉**是错的**
        //   ⇒ ⇒ 留着它 = 让用户按名字接线时踩第一个坑（**无任何报错**）
        //   ⇒ ⇒ ③ 删前已做**针对性实测**（R8.2 判据）：`ext_clk` **0/1 交替翻转**
        //   ⇒ ⇒ 60 拍 × 27 观测**逐拍全等**（1351 断言）⇒ ⇒ 零行为影响**实测**支撑
        //   ⇒ ⇒ ④ 连带把**同样零消费者**的 `riscv::Rv32iPipeline::clk` 一并删
        //   （否则它会变成**悬空输入** ⇒ ⇒ 产物里出现 undriven wire ⇒ ⇒ verilator
        //   严格模式下 **UNDRIVEN 致命**；且 ZERO-DEBT 要求零消费者端口不得留存）
        //   ⇒ ⇒ **真驱动源 = 框架 `default_clock`**（TB 里接 `clk` 即可）
        //   ⇒ ⇒ 「真让 ext_clk 生效」= **另独立立项**（改 codegen 时钟选择 ⇒ 全仓影响）
        //   ⇒ ⇒ 落地登记：`.rddf/checks/decision-request-rv32i-ext-clock-dead-port-2026-11.md`
        //   ⇒ ⇒ 期望登记：`.rddf/checks/examples-rtl-expectations.md` 的 D8 条。
        ch_in<ch_bool>          ext_rst{"ext_rst"};

        // 固件加载（写 I-TCM；加载期保持 ext_rst = 1，计数器不会被污染）
        ch_in<ch_bool>          prog_we{"prog_we"};
        // 固件写地址（**字地址**，32 位）—— 接到 12 位 TCM 时高位被截断
        // （与 pc / 数据地址同一约束：地址必须 < 0x1000）
        ch_in<ch_uint<32>>      prog_addr{"prog_addr"};
        ch_in<ch_uint<32>>      prog_data{"prog_data"};

        // 观测输出（verilator 可读；ADR-035 R3：位宽 MUST <= 64）
        ch_out<ch_uint<32>>     obs_instr_addr{"obs_instr_addr"};
        ch_out<ch_uint<32>>     obs_data_addr{"obs_data_addr"};
        ch_out<ch_uint<32>>     obs_data_write_data{"obs_data_write_data"};
        ch_out<ch_bool>         obs_data_write_en{"obs_data_write_en"};
        ch_out<ch_uint<48>>     obs_perf_cycles{"obs_perf_cycles"};
        ch_out<ch_uint<48>>     obs_perf_instructions{"obs_perf_instructions"};
        ch_out<ch_uint<32>>     obs_perf_branch_count{"obs_perf_branch_count"};
        ch_out<ch_uint<32>>     obs_perf_branch_mispredict{"obs_perf_branch_mispredict"};
        ch_out<ch_uint<32>>     obs_perf_load_use_stalls{"obs_perf_load_use_stalls"};
        ch_out<ch_uint<32>>     obs_perf_control_stalls{"obs_perf_control_stalls"};

        // 诊断输出（指令流 / 分支判定 / 有效链的逐级可观测性）
        ch_out<ch_uint<32>>     dbg_id_instr{"dbg_id_instr"};
        ch_out<ch_uint<32>>     dbg_id_pc{"dbg_id_pc"};
        ch_out<ch_bool>         dbg_id_valid{"dbg_id_valid"};
        // ⚠️ 这三个是「**EX 级**那条指令」的类型（接 `id_stage` 的 **ID/EX
        //    寄存后**的输出），**不是**「ID 级正在译码的那条」——
        //    （同组的 `dbg_id_instr` / `dbg_id_pc` / `dbg_id_valid` 才是真 ID 级）
        ch_out<ch_bool>         dbg_id_is_branch{"dbg_id_is_branch"};
        ch_out<ch_bool>         dbg_id_is_load{"dbg_id_is_load"};
        ch_out<ch_bool>         dbg_id_is_store{"dbg_id_is_store"};
        ch_out<ch_bool>         dbg_branch_taken{"dbg_branch_taken"};
        ch_out<ch_bool>         dbg_wb_write_en{"dbg_wb_write_en"};
        ch_out<ch_uint<32>>     dbg_wb_write_data{"dbg_wb_write_data"};
        ch_out<ch_uint<5>>      dbg_wb_rd_addr{"dbg_wb_rd_addr"};
        ch_out<ch_uint<32>>     dbg_exmem_alu_result{"dbg_exmem_alu_result"};
        ch_out<ch_uint<32>>     dbg_exmem_imm{"dbg_exmem_imm"};
        ch_out<ch_uint<32>>     dbg_exmem_rs2_data{"dbg_exmem_rs2_data"};
        ch_out<ch_uint<32>>     obs_data_rdata{"obs_data_rdata"};   // D-TCM 读数据（aread，0-cycle）
        // 有效链的逐级观测（EX/MEM → MEM/WB → WB）
        ch_out<ch_bool>         dbg_exmem_valid{"dbg_exmem_valid"};
        ch_out<ch_bool>         dbg_mem_valid_out{"dbg_mem_valid_out"};
        // load-use 冒险检测脉冲（与「停顿拍数」区分）
        ch_out<ch_bool>         dbg_load_use_detected{"dbg_load_use_detected"};
        // ⚠️ **G1/G2（2026-11-15 · B4）**：未知 opcode 的**可观测面**
        //    0 = 合法 · 1 = **已知未实现**（`OP_SYSTEM` = ECALL/EBREAK/CSR/MRET）
        //    · 2 = **未知 opcode** ⇒ ⇒ 「非法」1 bit 位 = `(值 != 0)`
        //    ⚠️ 只留一个 2 bit 端口 ⇒ 不新增「level 位」的**冗余**端口
        ch_out<ch_uint<2>>      dbg_illegal_reason{"dbg_illegal_reason"};

        // ==================================================================
        // **AXI4-Lite 主口**（19 根 · S4a · change `rv32i-soc-mmio-channel-2026-11`）
        // ==================================================================
        //
        // **G-9 裁决**：外设住 SoC 层 ⇒ ⇒ 核只引一条 **AXI4-Lite 主口**作为它的
        // **MMIO 通道**（⇒ ⇒ 核端口面 34 → **53**）。
        // 端口集合 = SoC 侧 2×2 Lite 互联的 **m0 面**（与之**同型** ⇒ 接线无适配层）
        // ⇒ ⇒ 接线无适配层。
        //
        // ⚠️ **核级 TB 必须把这 8 根输入显式 tie 0**（核级程序 MUST NOT 做 MMIO 访问）
        // ⇒ ⇒ 未接线 ⇒ 读出未定义值 ⇒ 依赖默认值会让判据「碰巧」通过。
        // ⚠️ **不新增 `irq` 口**（G-5 契约的实施在 **S5**）。
        ch_out<ch_uint<32>>     axil_awaddr{"axil_awaddr"};
        ch_out<ch_uint<2>>      axil_awprot{"axil_awprot"};
        ch_out<ch_bool>         axil_awvalid{"axil_awvalid"};
        ch_in<ch_bool>          axil_awready{"axil_awready"};

        ch_out<ch_uint<32>>     axil_wdata{"axil_wdata"};
        ch_out<ch_uint<4>>      axil_wstrb{"axil_wstrb"};
        ch_out<ch_bool>         axil_wvalid{"axil_wvalid"};
        ch_in<ch_bool>          axil_wready{"axil_wready"};

        ch_in<ch_uint<2>>       axil_bresp{"axil_bresp"};
        ch_in<ch_bool>          axil_bvalid{"axil_bvalid"};
        ch_out<ch_bool>         axil_bready{"axil_bready"};

        ch_out<ch_uint<32>>     axil_araddr{"axil_araddr"};
        ch_out<ch_uint<2>>      axil_arprot{"axil_arprot"};
        ch_out<ch_bool>         axil_arvalid{"axil_arvalid"};
        ch_in<ch_bool>          axil_arready{"axil_arready"};

        ch_in<ch_uint<32>>      axil_rdata{"axil_rdata"};
        ch_in<ch_uint<2>>       axil_rresp{"axil_rresp"};
        ch_in<ch_bool>          axil_rvalid{"axil_rvalid"};
        ch_out<ch_bool>         axil_rready{"axil_rready"};
    )

    /// ⚠️ **`itcm_init` / `dtcm_init` = 装载链路的**路线 A**（`firmware-load-chain` L2）**
    ///    ⇒ ⇒ 传**字镜像**（下标 = 字索引）⇒ ⇒ 内部 `ch_mem` 带 `init_data`
    ///    ⇒ ⇒ **产物里自动出现 `initial begin mem[i] = … end`**（`print_mem_init`）
    ///    ⇒ ⇒ **TB 零装载动作** · **零端口变更**（`__io` 一字未动）
    ///    ⇒ ⇒ 空（默认）⇒ ⇒ 与基线**逐字节等价**（同一条 `init_memory(…, {})` 路径）
    ///    ⇒ ⇒ 🔴 **`dtcm_init` 就是 `.data` 段的落点** —— `prog_*` **只**接 I-TCM
    ///    ⇒ ⇒ 有初始化 `.data` 的程序此前**装不进去**
    Rv32iCoreTop(ch::Component* parent = nullptr,
                     const std::string& name = "rv32i_core_top",
                     const std::vector<uint32_t>& itcm_init = {},
                     const std::vector<uint32_t>& dtcm_init = {})
        : ch::Component(parent, name),
          itcm_init_(itcm_init),
          dtcm_init_(dtcm_init) {}

    /// 初值字镜像（**空 = 无初值** ⇒ ⇒ 零行为变化）
    std::vector<uint32_t> itcm_init_;
    std::vector<uint32_t> dtcm_init_;

    void create_ports() override {
        new (io_storage_) io_type;
    }

    void describe() override {
        ch::ch_module<Rv32iPipeline> pipeline{"pipeline"};
        // ⚠️ **L2（路线 A）**：把**初值字镜像**透传给 TCM 的构造
        //    ⇒ ⇒ `ch_module<T, Args...>` 已支持参数透传 ⇒ ⇒ 零新机制
        ch::ch_module<InstrTCM<kTcmAddrWidth, 32>, const std::vector<uint32_t> &>
            itcm{"itcm", itcm_init_};
        ch::ch_module<DataTCM<kTcmAddrWidth, 32>, const std::vector<uint32_t> &>
            dtcm{"dtcm", dtcm_init_};
        // ⚠️ **2026-11-23 · S3（G-7 = full AXI4 主干）**：D-TCM 前加 D-L1 + AXI4 full。
        //    `DCacheSync` 在 `chlib`（顶层 namespace）· `TcmAxiSlave` 在 `axi4`。
        ch::ch_module<::chlib::DCacheSync> dcache{"dcache"};
        ch::ch_module<axi4::TcmAxiSlave<32, 32, kTcmAddrWidth>> dslave{"dslave"};
        // ⚠️ **S4a（G-9.4）**：MMIO 通道的**核内一端**（请求面 ← 流水线 · AXI 主口 → 顶层）
        ch::ch_module<AxilMmioMaster> mmio_master{"mmio_master"};

        // ---- I-TCM：取指 + 固件写 ----
        //
        // ⚠️ **地址单位**：`InstrTCM` / `DataTCM` 内部的 `ch_mem` 是按
        //    **字索引**寻址的（`memory.sread(io().addr, …)`），而流水线的
        //    `instr_addr` / `data_addr` 是**字节地址**（pc = 0x80000000 + 4k）。
        //    ⇒ 必须右移 2 位取字索引，否则取指**快 4 倍**（实测：ID 级解码的
        //    字比 PC 快 4 个字 ⇒ 分支在错误的 PC 上被判定 ⇒ 程序跑飞）。
        //    既有 fixture `tests/test_verilator_complex_e2e.cpp` 用
        //    `bits<3, 2>(cpu.io().instr_addr)` 正是这个修正。
        //    ⚠️ 位宽 MUST 正确：`bits<MSB,LSB>` 是**闭区间** ⇒ 宽度 =
        //    MSB−LSB+1 ⇒ 字索引需要 **kTcmAddrWidth 位**（12 位 ⇒ 4096 个
        //    字索引）⇒ 切片 MUST 是 `bits<kTcmAddrWidth+1, 2]`（13 位 ⇒ 低 12 位）
        //    —— 此前写成 `+2`（14 位）⇒ **多一位**（off-by-one），只因 bit 13
        //    恒 0 且端口静默截断才没暴露（第 15 轮 critic SHOULD_FIX #2）。
        itcm.io().addr <<= bits<kTcmAddrWidth + 1, 2>(pipeline.io().instr_addr);
        itcm.io().write_en <<= io().prog_we;
        itcm.io().write_addr <<= io().prog_addr;
        itcm.io().write_data <<= io().prog_data;
        pipeline.io().instr_data <<= itcm.io().data;
        pipeline.io().instr_ready <<= itcm.io().ready;

        // ---- D-Cache + AXI4 full → TcmAxiSlave → D-TCM（**TCM 旁路** · S3）----
        // 数据通路：pipeline（**字节**地址）→ DCacheSync → AXI4 full → TcmAxiSlave
        //           → DataTCM（**字**地址由 slave 内部换算）
        // ⚠️ `pipeline.data_req` = `load|store` 且 MEM 级有效 ⇒ 避免对**每条**指令
        //    做 cache 查找（读有副作用：miss ⇒ 发 AXI burst）
        // ⚠️ `pipeline.data_ready ← cache.ready` ⇒ **S2 打通的冻结链第一次真正生效**
        //    （cache hit = 1 拍；miss = 反压等待 4-beat fill）
        // ---- **pre-cache 分流**（G-9.3 · 判据方向 = 「地址 ∈ MMIO 段」）----
        //
        // ⚠️ **判据方向**（G-9 订正块）：MUST 是「∈ MMIO 段 ⇒ 旁路」，
        //    **不是**「∈ D-TCM 窗口 ⇒ 走 cache」⇒ ⇒ 因为字节地址 **0**
        //    （核级 / SoC 级程序都有 `sw x3, 0(x0)`）不在 D-TCM 窗口内
        //    ⇒ ⇒ 取反形式会把它推进 MMIO 旁路 ⇒ ⇒ 核级 TB 无 MMIO 从设备 ⇒ **永久挂死**。
        //
        // ⚠️ `riscv::in_mmio()` 是 **constexpr（C++ 数值）** 函数 ⇒ ⇒ **不能**作用于
        //    **硬件值**（`ch_uint<32>`）⇒ ⇒ 这里用**同一对边界常量**构造硬件比较器
        //    ⇒ ⇒ 边界仍**派生自库层表**（单一真相）⇒ ⇒ **没有**第二份口径。
        static constexpr uint32_t kMmioLo = static_cast<uint32_t>(riscv::kMmioBase);
        static constexpr uint32_t kMmioHi =
            static_cast<uint32_t>(riscv::kMmioBase + riscv::kMmioBytes);
        auto addr_in_mmio = (pipeline.io().data_addr >= ch_uint<32>(kMmioLo)) &
                            (pipeline.io().data_addr < ch_uint<32>(kMmioHi));

        dcache.io().addr  <<= pipeline.io().data_addr;
        dcache.io().wdata <<= pipeline.io().data_write_data;
        // ⚠️ **MMIO 请求 MUST NOT 进 D-L1**：`DCacheSync` 的 `aw_valid = we & req`
        //    （`d_cache_sync.h:287`）⇒ ⇒ store-to-MMIO 会在 D-TCM 的 AXI 从口上发起
        //    一次**别名地址**的写（`aw_addr = addr & ~3`，而 `TcmAxiSlave` 只看低
        //    `kTcmAddrWidth` 位）⇒ ⇒ **静默污染 D-TCM**；读同理会白跑一次 burst fill
        //    （且 Lite 外设没有 `r_last` ⇒ ⇒ fill 永不完成 ⇒ ⇒ 流水线冻结，见 §Why 2）。
        dcache.io().req   <<= pipeline.io().data_req & !addr_in_mmio;
        dcache.io().we    <<= pipeline.io().data_write_en;
        dcache.io().strbe <<= pipeline.io().data_strbe;
        // （读数据的 mux 在下方 —— ⚠️ 它的**选择线 MUST 与消费拍对齐**，见那里的注释）
        // ⚠️ `data_ready` 是**无条件**冻结信号（S2 的语义：拉低即冻结）
        //    ⇒ ⇒ 无访存请求时 MUST 视为「就绪」⇒ ⇒ 否则 cache 的 `ready` 在空闲拍
        //    为 0 会把整条流水线**永久冻死**（实测踩中：24 条判据 23 条红）。
        // ⚠️ **2026-11-24 · change `rv32i-commit-event-pulse-2026-11`**：
        //    `data_ready` 的表达式**提成局部变量** ⇒ ⇒ 因为「store 提交事件」
        //    MUST 在**总线侧**派生（见下方 `obs_data_write_en` 的注释）
        //    ⇒ ⇒ 两处共用**同一个**表达式节点 ⇒ ⇒ 值恒等、无第二套口径。
        // ⚠️ **S4a**：`data_ready` 是**两条路径的 mux**（仍是**单个**表达式节点
        //    ⇒ ⇒ store 观测事件 `data_write_en & data_ready_calc` **与路径无关**）。
        //    ⚠️ 禁 mux `data_write_en` / `data_write_data` / `data_strbe`
        //    （功能信号 MUST 保持电平 · 观测事件的**路径无关性**由本行的 mux 保证）。
        auto data_ready_calc = (!pipeline.io().data_req) |
            select(addr_in_mmio, mmio_master.io().ready, dcache.io().ready);
        pipeline.io().data_ready <<= data_ready_calc;

        // ---- **读数据 mux**（⚠️ 选择线 MUST 与**消费拍**对齐 · 2026-11-24 critic BLOCKER）----
        //
        // 🔴 **为什么不能用当拍的 `addr_in_mmio`**：
        //    `data_read_data` 是在 **M2 级组合消费**的（`rv32i_pipeline.h:345`：
        //    `load_lane.raw_word <<= io().data_read_data`，车道选择的 addr / funct3
        //    取自 **M2 寄存器**）⇒ ⇒ 而消费那一拍的 `data_addr` 已是**后继**指令
        //    在 EX/MEM 的地址 ⇒ ⇒ 用它会：
        //      ① MMIO load 之后跟**非 MMIO** 指令（最常见：「`lw` 外设寄存器 → 下一条
        //         ALU 用它」）⇒ ⇒ 选到 `dcache.rdata`（D-L1 的**保持寄存器** ⇒ 陈旧值）
        //      ② D-L1 load 之后跟 **MMIO** 地址 ⇒ ⇒ 反向选到陈旧的 `prdata`
        //    ⇒ ⇒ 两向都是**静默错值**（而且核级 24 判据**看不见** —— 核级全程 ∉ MMIO）。
        //
        // ⇒ ⇒ 修法：把选择线做成**与 M2 元数据同门控锁存**的寄存器 —— 门控 =
        //    `!data_ready`（= 流水线的 `mem_stall`，`rv32i_pipeline.h:276`
        //    逐字同源）⇒ ⇒ 与 `m2_alu_result` / `m2_funct3` 的 `->next` 同一条沿、
        //    同一个门控 ⇒ ⇒ **不可能错位**。
        ch_reg<ch_bool> rd_path_mmio(ch_bool(false));
        auto mem_stall_now = !data_ready_calc;
        rd_path_mmio->next = select(mem_stall_now, rd_path_mmio, addr_in_mmio);
        pipeline.io().data_read_data <<=
            select(rd_path_mmio, mmio_master.io().prdata, dcache.io().rdata);

        // ---- 核侧 MMIO 主设备接线（请求面 ← 流水线 MEM 级 · 只在 MMIO 段取请求）----
        mmio_master.io().preq   <<= pipeline.io().data_req & addr_in_mmio;
        mmio_master.io().pwe    <<= pipeline.io().data_write_en;
        mmio_master.io().paddr  <<= pipeline.io().data_addr;
        mmio_master.io().pwdata <<= pipeline.io().data_write_data;
        mmio_master.io().pstrbe <<= pipeline.io().data_strbe;
        // 复位 = 核的 `ext_rst`（**高有效** ⇒ ⇒ 与 master 的 `rst` 同极性）
        mmio_master.io().rst    <<= io().ext_rst;

        // ---- AXI4-Lite 主口 → 核顶层端口（19 根 · 直连，无适配层）----
        io().axil_awaddr  <<= mmio_master.io().awaddr;
        io().axil_awprot  <<= mmio_master.io().awprot;
        io().axil_awvalid <<= mmio_master.io().awvalid;
        mmio_master.io().awready <<= io().axil_awready;
        io().axil_wdata   <<= mmio_master.io().wdata;
        io().axil_wstrb   <<= mmio_master.io().wstrb;
        io().axil_wvalid  <<= mmio_master.io().wvalid;
        mmio_master.io().wready <<= io().axil_wready;
        mmio_master.io().bresp  <<= io().axil_bresp;
        mmio_master.io().bvalid <<= io().axil_bvalid;
        io().axil_bready  <<= mmio_master.io().bready;
        io().axil_araddr  <<= mmio_master.io().araddr;
        io().axil_arprot  <<= mmio_master.io().arprot;
        io().axil_arvalid <<= mmio_master.io().arvalid;
        mmio_master.io().arready <<= io().axil_arready;
        mmio_master.io().rdata  <<= io().axil_rdata;
        mmio_master.io().rresp  <<= io().axil_rresp;
        mmio_master.io().rvalid <<= io().axil_rvalid;
        io().axil_rready  <<= mmio_master.io().rready;

        // cache ↔ TcmAxiSlave（AXI4 full 全通道直连）
        dslave.io().ar_addr  <<= dcache.io().ar_addr;
        dslave.io().ar_id    <<= ch_uint<4>(0_d);
        dslave.io().ar_len   <<= dcache.io().ar_len;
        dslave.io().ar_size  <<= dcache.io().ar_size;
        dslave.io().ar_burst <<= dcache.io().ar_burst;
        dslave.io().ar_valid <<= dcache.io().ar_valid;
        dcache.io().ar_ready <<= dslave.io().ar_ready;

        dcache.io().r_data  <<= dslave.io().r_data;
        dcache.io().r_resp  <<= dslave.io().r_resp;
        dcache.io().r_last  <<= dslave.io().r_last;
        dcache.io().r_valid <<= dslave.io().r_valid;
        dslave.io().r_ready <<= dcache.io().r_ready;

        dslave.io().aw_addr  <<= dcache.io().aw_addr;
        dslave.io().aw_id    <<= ch_uint<4>(0_d);
        dslave.io().aw_len   <<= dcache.io().aw_len;
        dslave.io().aw_size  <<= dcache.io().aw_size;
        dslave.io().aw_burst <<= dcache.io().aw_burst;
        dslave.io().aw_valid <<= dcache.io().aw_valid;
        dcache.io().aw_ready <<= dslave.io().aw_ready;

        dslave.io().w_data  <<= dcache.io().w_data;
        dslave.io().w_strb  <<= dcache.io().w_strb;
        dslave.io().w_last  <<= dcache.io().w_last;
        dslave.io().w_valid <<= dcache.io().w_valid;
        dcache.io().w_ready <<= dslave.io().w_ready;

        dcache.io().b_resp  <<= dslave.io().bresp;
        dcache.io().b_valid <<= dslave.io().bvalid;
        dslave.io().bready  <<= dcache.io().b_ready;

        // TcmAxiSlave → D-TCM（单口：`valid` 由 slave 驱动，不再恒 1）
        dtcm.io().addr  <<= dslave.io().tcm_addr;
        dtcm.io().write <<= dslave.io().tcm_write;
        dtcm.io().wdata <<= dslave.io().tcm_wdata;
        dtcm.io().strbe <<= dslave.io().tcm_strbe;
        dtcm.io().valid <<= dslave.io().tcm_valid;
        dslave.io().tcm_ready <<= dtcm.io().ready;
        dslave.io().tcm_rdata <<= dtcm.io().rdata;

        // ---- 时钟 / 复位 ----
        pipeline.io().rst <<= io().ext_rst;
        // ⚠️ **2026-11-14 · R8 改判**：`pipeline.io().clk` 的赋值**已删**
        //   （`riscv::Rv32iPipeline::clk` 本身也已删 —— 它**零消费者** ⇒ ZERO-DEBT）
        //   ⇒ ⇒ 流水线寄存器全部锁在**框架 `default_clock`**（产物级实测）

        // ---- 观测输出 ----
        io().obs_instr_addr <<= pipeline.io().instr_addr;
        io().obs_data_addr <<= pipeline.io().data_addr;
        io().obs_data_write_data <<= pipeline.io().data_write_data;
        // ⚠️⚠️ **store 提交事件 MUST 是「单拍脉冲」**（2026-11-24 · change
        //    `rv32i-commit-event-pulse-2026-11`）：
        //    `pipeline.io().data_write_en` 是**功能信号**（→ D-L1 的 `we`）且是
        //    **电平** ⇒ ⇒ 反压期间 store 停留在 EX/MEM ⇒ ⇒ 该电平**持续多拍**
        //    ⇒ ⇒ 「store 事件次数」被污染成「事件时长之和」。
        //    ⇒ ⇒ **不能**在流水线里用 `!mem_stall` 门控它（**组合环** ⇒ 仿真
        //    `FATAL: Cycle detected`，2026-11-24 实测）⇒ ⇒ 在**总线侧**派生：
        //    `data_write_en & data_ready` ⇒ ⇒ 对 store 而言 `data_ready` =
        //    `dcache.ready` = `aw_fire` ⇒ ⇒ 事件 = **AXI 写地址握手那一拍**
        //    ⇒ ⇒ 每次 store **恰好一次** ⇒ ⇒ 端口名 / 方向 / 位宽全部不变
        //    ⇒ ⇒ **核端口面仍是 34**（C6 快照不动）。
        //    ⚠️ **零功能影响**：功能侧仍用 `pipeline.io().data_write_en`（电平）
        //    ⇒ ⇒ D-L1 的写握手次数**恒为 1**（今日实测：既有固件 store 事件 = 5
        //    = store 条数 ⇒ ⇒ 今日**零可观测变化** ⇒ ⇒ 脉冲化的**判别力**归 S4a
        //    （MMIO 事务必然多拍 ⇒ 那时才可判别）—— 本 change **如实登记**。
        io().obs_data_write_en <<= pipeline.io().data_write_en & data_ready_calc;
        io().obs_perf_cycles <<= pipeline.io().perf_cycles;
        io().obs_perf_instructions <<= pipeline.io().perf_instructions;
        io().obs_perf_branch_count <<= pipeline.io().perf_branch_count;
        io().obs_perf_branch_mispredict <<= pipeline.io().perf_branch_mispredict;
        io().obs_perf_load_use_stalls <<= pipeline.io().perf_load_use_stalls;
        io().obs_perf_control_stalls <<= pipeline.io().perf_control_stalls;

        // ---- 诊断输出 ----
        io().dbg_id_instr <<= pipeline.io().dbg_id_instr;
        io().dbg_id_pc <<= pipeline.io().dbg_id_pc;
        io().dbg_id_valid <<= pipeline.io().dbg_id_valid;
        io().dbg_id_is_branch <<= pipeline.io().dbg_id_is_branch;
        io().dbg_id_is_load <<= pipeline.io().dbg_id_is_load;
        io().dbg_id_is_store <<= pipeline.io().dbg_id_is_store;
        io().dbg_branch_taken <<= pipeline.io().dbg_branch_taken;
        io().dbg_wb_write_en <<= pipeline.io().dbg_wb_write_en;
        io().dbg_wb_write_data <<= pipeline.io().dbg_wb_write_data;
        io().dbg_wb_rd_addr <<= pipeline.io().dbg_wb_rd_addr;
        io().dbg_exmem_alu_result <<= pipeline.io().dbg_exmem_alu_result;
        io().dbg_exmem_imm <<= pipeline.io().dbg_exmem_imm;
        io().dbg_exmem_rs2_data <<= pipeline.io().dbg_exmem_rs2_data;
        io().obs_data_rdata <<= dtcm.io().rdata;
        io().dbg_exmem_valid <<= pipeline.io().dbg_exmem_valid;
        io().dbg_mem_valid_out <<= pipeline.io().dbg_mem_valid_out;
        io().dbg_load_use_detected <<= pipeline.io().dbg_load_use_detected;
        io().dbg_illegal_reason <<= pipeline.io().dbg_illegal_reason;
    }
};
