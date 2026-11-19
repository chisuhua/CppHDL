// examples/axi4/phase3a_top.h
//
// Phase 3A 的**顶层组件**（从 `phase3a_complete.cpp` 抽出 · `axi4-convergence-tail-2026-11` B1）。
//
// ⚠️ **三角色分离**（蓝图 §4.3.3 · `example-file-boundary-2026-11` 的收口）：
//   · **本文件** = 设计定义（只做实例化与连接 · **无 `main`**）
//   · 激励驱动 = `tb_phase3a_example.cpp`（`main` + `RESULT:` 协议行）
//   ⇒ ⇒ 与 `axi4_lite_example_top.h` / `axi_soc_example_top.h` **同型**。
//
// ⚠️ **本设计不进 RTL 产品清单**（无 `--emit-rtl` · 无 emitter target ⇒
//   `.rddf/checks/examples-rtl-sim-targets.tsv` 无该行）⇒ ⇒ 端口**未具名**
//   （ADR-075 E1 的具名要求只对有产物的设计生效）。
//   ⇒ ⇒ 若未来把它纳入产品面（follow-up）⇒ ⇒ MUST **先具名**（否则
//   `manifest.cpp_member` 只能是 `unnamed_input` ⇒ 验证源无法按名寻址）。

#pragma once

#include "axi4/axi4_lite_slave.h"
#include "axi4/axi_interconnect_4x4.h"
#include "axi4/axi_to_axilite.h"
#include "ch.hpp"
#include "component.h"

#include <string>

namespace axi4 {
namespace example {

using namespace ch::core; // ch_uint / ch_bool / ch_reg / select / 0_d 等

/// **根组件名**（**单一真相源**）—— 与另两个设计头的根名常量同型
inline constexpr const char *kPhase3aRootName = "top";

class Phase3ATop : public ch::Component {
public:
    __io(
        // 测试控制
        ch_in<ch_bool> start;
        ch_in<ch_bool> is_write;
        ch_in<ch_uint<32>> addr;
        ch_in<ch_uint<32>> wdata;
        ch_out<ch_uint<32>> rdata;
        ch_out<ch_bool> done;
        
        // 复位（低有效，在时钟沿生效）—— 透传到 converter 与 AXI4-Lite slave
        ch_in<ch_bool> rst_n;
    )
    
    Phase3ATop(ch::Component* parent = nullptr, const std::string& name = "phase3a_top")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // ========================================================================
        // 1. AXI4 4x4 Interconnect
        // ========================================================================
        
        ch::ch_module<AxiInterconnect4x4<32, 32, 4>> interconnect{"interconnect"};
        // 互连层是 rst_n 链路的中间节点：MUST 透传父级复位源（MUST NOT tie 常量，
        // 那会永久掩盖复位路径）。顶层 rst_n 已驱动 converter / slave，此处是纯增量。
        interconnect.io().rst_n <<= io().rst_n;
        
        // ========================================================================
        // 2. AXI to AXI-Lite Converter (从设备 0 路径)
        // ========================================================================
        
        ch::ch_module<AxiToAxiLiteConverter<32, 32>> converter{"converter"};
        
        // ========================================================================
        // 3. AXI4-Lite Slave (GPIO 模拟)
        // ========================================================================
        
        ch::ch_module<AxiLiteSlave<32, 32, 4>> axil_slave{"axil_slave"};
        
        // ========================================================================
        // 连接：Interconnect → Converter → AXI4-Lite Slave
        // ========================================================================
        
        // Interconnect 从设备 0 → Converter AXI4 主设备
        converter.io().axi_awaddr <<= interconnect.io().s0_awaddr;
        converter.io().axi_awid <<= interconnect.io().s0_awid;
        converter.io().axi_awlen <<= interconnect.io().s0_awlen;
        converter.io().axi_awvalid <<= interconnect.io().s0_awvalid;
        interconnect.io().s0_awready <<= converter.io().axi_awready;
        
        converter.io().axi_wdata <<= interconnect.io().s0_wdata;
        converter.io().axi_wstrb <<= interconnect.io().s0_wstrb;
        converter.io().axi_wlast <<= interconnect.io().s0_wlast;
        converter.io().axi_wvalid <<= interconnect.io().s0_wvalid;
        interconnect.io().s0_wready <<= converter.io().axi_wready;
        
        interconnect.io().s0_bid <<= converter.io().axi_bid;
        interconnect.io().s0_bresp <<= converter.io().axi_bresp;
        interconnect.io().s0_bvalid <<= converter.io().axi_bvalid;
        converter.io().axi_bready <<= interconnect.io().s0_bready;
        
        converter.io().axi_araddr <<= interconnect.io().s0_araddr;
        converter.io().axi_arid <<= interconnect.io().s0_arid;
        converter.io().axi_arlen <<= interconnect.io().s0_arlen;
        converter.io().axi_arvalid <<= interconnect.io().s0_arvalid;
        interconnect.io().s0_arready <<= converter.io().axi_arready;
        
        interconnect.io().s0_rdata <<= converter.io().axi_rdata;
        interconnect.io().s0_rid <<= converter.io().axi_rid;
        interconnect.io().s0_rresp <<= converter.io().axi_rresp;
        interconnect.io().s0_rlast <<= converter.io().axi_rlast;
        interconnect.io().s0_rvalid <<= converter.io().axi_rvalid;
        converter.io().axi_rready <<= interconnect.io().s0_rready;
        
        // 复位链路：顶层 → converter → AXI4-Lite slave
        // （`rst_n` 是 converter 与 slave 的必需端口，未接线读出为 0 = 永久复位）
        converter.io().rst_n <<= io().rst_n;
        axil_slave.io().rst_n <<= converter.io().axil_rst_n;
        
        // Converter AXI4-Lite 从设备 → AXI4-Lite Slave
        axil_slave.io().awaddr <<= converter.io().axil_awaddr;
        axil_slave.io().awprot <<= converter.io().axil_awprot;
        axil_slave.io().awvalid <<= converter.io().axil_awvalid;
        converter.io().axil_awready <<= axil_slave.io().awready;
        
        axil_slave.io().wdata <<= converter.io().axil_wdata;
        axil_slave.io().wstrb <<= converter.io().axil_wstrb;
        axil_slave.io().wvalid <<= converter.io().axil_wvalid;
        converter.io().axil_wready <<= axil_slave.io().wready;
        
        converter.io().axil_bresp <<= axil_slave.io().bresp;
        converter.io().axil_bvalid <<= axil_slave.io().bvalid;
        axil_slave.io().bready <<= converter.io().axil_bready;
        
        axil_slave.io().araddr <<= converter.io().axil_araddr;
        axil_slave.io().arprot <<= converter.io().axil_arprot;
        axil_slave.io().arvalid <<= converter.io().axil_arvalid;
        converter.io().axil_arready <<= axil_slave.io().arready;
        
        converter.io().axil_rdata <<= axil_slave.io().rdata;
        converter.io().axil_rresp <<= axil_slave.io().rresp;
        converter.io().axil_rvalid <<= axil_slave.io().rvalid;
        axil_slave.io().rready <<= converter.io().axil_rready;
        
        // ========================================================================
        // 4. 内部 AXI4 Full master（协议合规）
        // ========================================================================
        // 缺陷修复（`axi4-interconnect-routing-2026-10` Task 8）：
        //   ① `io().done` 此前被 `<<=` 驱动**两次**（`m0_bvalid` 与 `m0_rvalid`）
        //      ⇒ 同一个 `ch_out` 双 driver ⇒ 行为未定义。
        //      `io().rdata` 同样被驱动两次（常量 0 与 `m0_rdata`）。
        //   ② 旧写法把 `awvalid` / `wvalid` **同拍**拉高，而下游
        //      `AxiToAxiLiteConverter` 与 `AxiLiteSlave` 都是**相位分离**的
        //      （`aw_done` 是寄存器 ⇒ 同拍 AW+W 的 W **不被接受**）
        //      ⇒ 同拍激励会让写数据**永久丢失**。
        //   ③ `done` 组合直出 ⇒ 测试台在 `tick()` **之后**采样必然错过
        //      （组合脉冲在 tick 结束后已被改写）。
        //   ⇒ 改为：AWVALID / WVALID 各自**保持到对应握手**；地址 / 写数据
        //      在 `start` 时**锁存**；`done` 与 `rdata` 各由**一个**寄存器驱动。
        ch_reg<ch_uint<2>> mstate(0_d);          // 0=idle 1=write 2=read
        ch_reg<ch_bool> aw_done(ch_bool(false));  // AW 已握手
        ch_reg<ch_bool> w_done(ch_bool(false));   // W 已握手
        ch_reg<ch_bool> ar_done(ch_bool(false));  // AR 已握手
        ch_reg<ch_uint<32>> addr_reg(0_d);       // 事务期间地址保持稳定
        ch_reg<ch_uint<32>> wdata_reg(0_d);
        ch_reg<ch_uint<32>> rdata_reg(0_d);
        ch_reg<ch_bool> done_reg(ch_bool(false)); // 完成脉冲（注册一拍）

        auto is_idle = (mstate == ch_uint<2>(0_d));
        auto is_write = (mstate == ch_uint<2>(1_d));
        auto is_read = (mstate == ch_uint<2>(2_d));

        auto start_write =
            select(io().start, select(io().is_write, is_idle, ch_bool(false)), ch_bool(false));
        auto start_read =
            select(io().start, select(io().is_write, ch_bool(false), is_idle), ch_bool(false));
        auto any_start = start_write | start_read;

        // 地址 / 写数据在 start 时锁存（AXI 要求事务期间保持稳定）
        addr_reg->next = select(any_start, io().addr, addr_reg);
        wdata_reg->next = select(start_write, io().wdata, wdata_reg);

        // ---------------- AW 通道 ----------------
        interconnect.io().m0_awaddr<<=addr_reg;
        interconnect.io().m0_awid<<=ch_uint<4>(0_d);
        interconnect.io().m0_awlen<<=ch_uint<4>(0_d);
        interconnect.io().m0_awsize<<=ch_uint<3>(2_d);
        interconnect.io().m0_awburst<<=ch_uint<2>(1_d);
        // AWVALID **保持到 AW 握手**（合规 master 的义务）
        auto aw_active = select(is_write, select(aw_done, ch_bool(false), ch_bool(true)),
                                 ch_bool(false));
        interconnect.io().m0_awvalid<<=aw_active;

        // ---------------- W 通道 ----------------
        interconnect.io().m0_wdata<<=wdata_reg;
        interconnect.io().m0_wstrb<<=ch_uint<4>(15_d);
        interconnect.io().m0_wlast<<=ch_bool(true);
        // WVALID **保持到 W 握手**（与 AW 分开保持 ⇒ 相位分离安全）
        auto w_active = select(is_write, select(w_done, ch_bool(false), ch_bool(true)),
                               ch_bool(false));
        interconnect.io().m0_wvalid<<=w_active;

        // ---------------- B 通道 ----------------
        auto b_hs = interconnect.io().m0_bvalid & interconnect.io().m0_bready;
        interconnect.io().m0_bready<<=is_write;

        // ---------------- AR 通道 ----------------
        interconnect.io().m0_araddr<<=addr_reg;
        interconnect.io().m0_arid<<=ch_uint<4>(0_d);
        interconnect.io().m0_arlen<<=ch_uint<4>(0_d);
        interconnect.io().m0_arsize<<=ch_uint<3>(2_d);
        interconnect.io().m0_arburst<<=ch_uint<2>(1_d);
        auto ar_active = select(is_read, select(ar_done, ch_bool(false), ch_bool(true)),
                                ch_bool(false));
        interconnect.io().m0_arvalid<<=ar_active;

        // ---------------- R 通道 ----------------
        auto r_hs = interconnect.io().m0_rvalid & interconnect.io().m0_rready;
        interconnect.io().m0_rready<<=is_read;
        rdata_reg->next = select(r_hs, interconnect.io().m0_rdata, rdata_reg);

        // ---------------- 握手标志 ----------------
        auto aw_hs = interconnect.io().m0_awvalid & interconnect.io().m0_awready;
        auto w_hs = interconnect.io().m0_wvalid & interconnect.io().m0_wready;
        auto ar_hs = interconnect.io().m0_arvalid & interconnect.io().m0_arready;
        aw_done->next = select(aw_hs, ch_bool(true), select(start_write, ch_bool(false), aw_done));
        w_done->next = select(w_hs, ch_bool(true), select(start_write, ch_bool(false), w_done));
        ar_done->next = select(ar_hs, ch_bool(true), select(start_read, ch_bool(false), ar_done));

        // ---------------- 状态机 ----------------
        auto write_done = select(b_hs, is_write, ch_bool(false));
        auto read_done = select(r_hs, is_read, ch_bool(false));
        mstate->next = select(start_write, ch_uint<2>(1_d),
                       select(start_read, ch_uint<2>(2_d),
                       select(write_done, ch_uint<2>(0_d),
                       select(read_done, ch_uint<2>(0_d), mstate))));

        // 完成脉冲：新事务开始时清零，响应握手时置位
        done_reg->next = select(any_start, ch_bool(false),
                         select(write_done | read_done, ch_bool(true), done_reg));

        // 单 driver：`done` / `rdata` 各只被赋值一次（此前是双 driver）
        io().done = done_reg;
        io().rdata = rdata_reg;
        
        // 其他主设备接口 (未使用，接地)
        interconnect.io().m1_awvalid <<= ch_bool(false);
        interconnect.io().m1_wvalid <<= ch_bool(false);
        interconnect.io().m1_bready <<= ch_bool(false);
        interconnect.io().m1_arvalid <<= ch_bool(false);
        interconnect.io().m1_rready <<= ch_bool(false);
        
        interconnect.io().m2_awvalid <<= ch_bool(false);
        interconnect.io().m2_wvalid <<= ch_bool(false);
        interconnect.io().m2_bready <<= ch_bool(false);
        interconnect.io().m2_arvalid <<= ch_bool(false);
        interconnect.io().m2_rready <<= ch_bool(false);
        
        interconnect.io().m3_awvalid <<= ch_bool(false);
        interconnect.io().m3_wvalid <<= ch_bool(false);
        interconnect.io().m3_bready <<= ch_bool(false);
        interconnect.io().m3_arvalid <<= ch_bool(false);
        interconnect.io().m3_rready <<= ch_bool(false);
        
        // 从设备 1-3 接口 (未使用，接地)
        interconnect.io().s1_awready <<= ch_bool(false);
        interconnect.io().s1_wready <<= ch_bool(false);
        interconnect.io().s1_bresp <<= ch_uint<2>(0_d);
        interconnect.io().s1_bvalid <<= ch_bool(false);
        interconnect.io().s1_arready <<= ch_bool(false);
        interconnect.io().s1_rdata <<= ch_uint<32>(0_d);
        interconnect.io().s1_rresp <<= ch_uint<2>(0_d);
        interconnect.io().s1_rvalid <<= ch_bool(false);
        
        interconnect.io().s2_awready <<= ch_bool(false);
        interconnect.io().s2_wready <<= ch_bool(false);
        interconnect.io().s2_bresp <<= ch_uint<2>(0_d);
        interconnect.io().s2_bvalid <<= ch_bool(false);
        interconnect.io().s2_arready <<= ch_bool(false);
        interconnect.io().s2_rdata <<= ch_uint<32>(0_d);
        interconnect.io().s2_rresp <<= ch_uint<2>(0_d);
        interconnect.io().s2_rvalid <<= ch_bool(false);
        
        interconnect.io().s3_awready <<= ch_bool(false);
        interconnect.io().s3_wready <<= ch_bool(false);
        interconnect.io().s3_bresp <<= ch_uint<2>(0_d);
        interconnect.io().s3_bvalid <<= ch_bool(false);
        interconnect.io().s3_arready <<= ch_bool(false);
        interconnect.io().s3_rdata <<= ch_uint<32>(0_d);
        interconnect.io().s3_rresp <<= ch_uint<2>(0_d);
        interconnect.io().s3_rvalid <<= ch_bool(false);
    }
};

} // namespace example
} // namespace axi4
