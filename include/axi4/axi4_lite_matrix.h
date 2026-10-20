/**
 * AXI4-Lite 2x2 Matrix
 * 
 * 连接 2 个主设备到 2 个从设备，支持：
 * - 轮询仲裁
 * - 地址解码
 * - 错误响应
 */

#pragma once

#include "ch.hpp"
#include "component.h"

namespace axi4 {

using namespace ch::core;

template <unsigned ADDR_WIDTH = 32, unsigned DATA_WIDTH = 32>
class AxiLiteMatrix2x2 : public ch::Component {
public:
    // 端口方向约定（与 ch::operator<<= 的连接语义一致）：
    //   * ch_in  = **父模块驱动**本组件的该端口（master 侧请求 / slave 侧 READY）
    //   * ch_out = **本组件驱动**该端口（返回给 master / 转发给 slave）
    // 声明反了会让 `io().x = ...` 变成运行时 no-op（"Cannot assign value to
    // input port!"）—— 端口"看起来"在连，实际信号恒为未驱动值。
    __io(
        // 主设备 0 接口
        ch_in<ch_uint<ADDR_WIDTH>> m0_awaddr;
        ch_in<ch_uint<2>> m0_awprot;
        ch_in<ch_bool> m0_awvalid;
        ch_out<ch_bool> m0_awready;

        ch_in<ch_uint<DATA_WIDTH>> m0_wdata;
        ch_in<ch_uint<DATA_WIDTH/8>> m0_wstrb;
        ch_in<ch_bool> m0_wvalid;
        ch_out<ch_bool> m0_wready;

        ch_out<ch_uint<2>> m0_bresp;
        ch_out<ch_bool> m0_bvalid;
        ch_in<ch_bool> m0_bready;

        ch_in<ch_uint<ADDR_WIDTH>> m0_araddr;
        ch_in<ch_uint<2>> m0_arprot;
        ch_in<ch_bool> m0_arvalid;
        ch_out<ch_bool> m0_arready;

        ch_out<ch_uint<DATA_WIDTH>> m0_rdata;
        ch_out<ch_uint<2>> m0_rresp;
        ch_out<ch_bool> m0_rvalid;
        ch_in<ch_bool> m0_rready;

        // 主设备 1 接口
        ch_in<ch_uint<ADDR_WIDTH>> m1_awaddr;
        ch_in<ch_uint<2>> m1_awprot;
        ch_in<ch_bool> m1_awvalid;
        ch_out<ch_bool> m1_awready;

        ch_in<ch_uint<DATA_WIDTH>> m1_wdata;
        ch_in<ch_uint<DATA_WIDTH/8>> m1_wstrb;
        ch_in<ch_bool> m1_wvalid;
        ch_out<ch_bool> m1_wready;

        ch_out<ch_uint<2>> m1_bresp;
        ch_out<ch_bool> m1_bvalid;
        ch_in<ch_bool> m1_bready;

        ch_in<ch_uint<ADDR_WIDTH>> m1_araddr;
        ch_in<ch_uint<2>> m1_arprot;
        ch_in<ch_bool> m1_arvalid;
        ch_out<ch_bool> m1_arready;

        ch_out<ch_uint<DATA_WIDTH>> m1_rdata;
        ch_out<ch_uint<2>> m1_rresp;
        ch_out<ch_bool> m1_rvalid;
        ch_in<ch_bool> m1_rready;

        // 从设备 0 接口
        ch_out<ch_uint<ADDR_WIDTH>> s0_awaddr;
        ch_out<ch_uint<2>> s0_awprot;
        ch_out<ch_bool> s0_awvalid;
        ch_in<ch_bool> s0_awready;

        ch_out<ch_uint<DATA_WIDTH>> s0_wdata;
        ch_out<ch_uint<DATA_WIDTH/8>> s0_wstrb;
        ch_out<ch_bool> s0_wvalid;
        ch_in<ch_bool> s0_wready;

        ch_in<ch_uint<2>> s0_bresp;
        ch_in<ch_bool> s0_bvalid;
        ch_out<ch_bool> s0_bready;

        ch_out<ch_uint<ADDR_WIDTH>> s0_araddr;
        ch_out<ch_uint<2>> s0_arprot;
        ch_out<ch_bool> s0_arvalid;
        ch_in<ch_bool> s0_arready;

        ch_in<ch_uint<DATA_WIDTH>> s0_rdata;
        ch_in<ch_uint<2>> s0_rresp;
        ch_in<ch_bool> s0_rvalid;
        ch_out<ch_bool> s0_rready;

        // 从设备 1 接口
        ch_out<ch_uint<ADDR_WIDTH>> s1_awaddr;
        ch_out<ch_uint<2>> s1_awprot;
        ch_out<ch_bool> s1_awvalid;
        ch_in<ch_bool> s1_awready;

        ch_out<ch_uint<DATA_WIDTH>> s1_wdata;
        ch_out<ch_uint<DATA_WIDTH/8>> s1_wstrb;
        ch_out<ch_bool> s1_wvalid;
        ch_in<ch_bool> s1_wready;

        ch_in<ch_uint<2>> s1_bresp;
        ch_in<ch_bool> s1_bvalid;
        ch_out<ch_bool> s1_bready;

        ch_out<ch_uint<ADDR_WIDTH>> s1_araddr;
        ch_out<ch_uint<2>> s1_arprot;
        ch_out<ch_bool> s1_arvalid;
        ch_in<ch_bool> s1_arready;

        ch_in<ch_uint<DATA_WIDTH>> s1_rdata;
        ch_in<ch_uint<2>> s1_rresp;
        ch_in<ch_bool> s1_rvalid;
        ch_out<ch_bool> s1_rready;
    )
    
    AxiLiteMatrix2x2(ch::Component* parent = nullptr, const std::string& name = "axi_matrix_2x2")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // 仲裁器：轮询
        ch_reg<ch_bool> arb_state(ch_bool(false));  // 0=主设备 0, 1=主设备 1

        // ---------------- 在途事务跟踪 ----------------
        // AXI4-Lite 允许 AW 与 W **相隔任意周期**（本项目的 slave 是相位分离的），
        // 且 master 在 AW 握手后即可撤掉 AWVALID —— 因此"当前哪个 master 在用总线、
        // 走哪个从设备"必须**在 AW/AR 握手时锁存**，不能靠活的 awvalid 推导
        // （活的 grant 在 AW 握手后即掉 → W 通道被路由到别的从设备 → 写丢失）。
        ch_reg<ch_bool> m0_active(ch_bool(false));   // m0 有在途写事务
        ch_reg<ch_bool> m0_route_s1(ch_bool(false)); // m0 的在途事务走 s1
        ch_reg<ch_bool> m1_active(ch_bool(false));
        ch_reg<ch_bool> m1_route_s1(ch_bool(false));

        
        // 地址解码：高位选择从设备
        auto addr_bit_m0 = io().m0_awaddr >> ch_uint<ADDR_WIDTH>(ADDR_WIDTH-1_d);
        auto addr_bit_m1 = io().m1_awaddr >> ch_uint<ADDR_WIDTH>(ADDR_WIDTH-1_d);
        
        auto select_s1_m0 = (addr_bit_m0 != ch_uint<ADDR_WIDTH>(0_d));
        auto select_s1_m1 = (addr_bit_m1 != ch_uint<ADDR_WIDTH>(0_d));

        // 读通道地址**单独解码**（原实现用写地址的解码结果路由读事务）
        auto ar_addr_bit_m0 = io().m0_araddr >> ch_uint<ADDR_WIDTH>(ADDR_WIDTH-1_d);
        auto ar_addr_bit_m1 = io().m1_araddr >> ch_uint<ADDR_WIDTH>(ADDR_WIDTH-1_d);
        auto ar_select_s1_m0 = (ar_addr_bit_m0 != ch_uint<ADDR_WIDTH>(0_d));
        auto ar_select_s1_m1 = (ar_addr_bit_m1 != ch_uint<ADDR_WIDTH>(0_d));

        // AW 握手（目标从设备在握手当拍由地址解码确定并锁存）
        auto m0_aw_hs = io().m0_awvalid & select(m0_active, ch_bool(false), ch_bool(true));
        auto m1_aw_hs = io().m1_awvalid & select(m1_active, ch_bool(false), ch_bool(true));
        // 锁存路由 → 目标从设备（1 = s1）
        auto m0_to_s0 = select(m0_route_s1, ch_bool(false), ch_bool(true));
        auto m1_to_s0 = select(m1_route_s1, ch_bool(false), ch_bool(true));
        
        // 仲裁逻辑：请求同时包含写地址与读地址两个通道
        // （原实现只看 awvalid → 读事务永远拿不到总线 → AR 从不发出）
        auto m0_req = io().m0_awvalid | io().m0_arvalid;
        auto m1_req = io().m1_awvalid | io().m1_arvalid;

        // 选择哪个主设备：
        //   1) 有在途事务的 master 保持总线
        //   2) 否则按轮询仲裁（arb_state 指示优先方），**但优先方无请求时
        //      必须让给另一方** —— 原实现 `select(!arb_state, m0_req, false)`
        //      在 arb_state=1 且只有 m0 请求时两个 grant 都是 0 → 总线死锁
        auto prefer_m0 = select(arb_state, ch_bool(false), ch_bool(true));
        auto m0_only = select(m0_req, select(m1_req, ch_bool(false), ch_bool(true)), ch_bool(false));
        auto m1_only = select(m1_req, select(m0_req, ch_bool(false), ch_bool(true)), ch_bool(false));
        auto grant_m0 = select(m0_active, ch_bool(true),
                               select(m1_active, ch_bool(false),
                                      select(prefer_m0,
                                             select(m0_req, ch_bool(true), ch_bool(false)),
                                             select(m0_only, ch_bool(true), ch_bool(false)))));
        auto grant_m1 = select(m1_active, ch_bool(true),
                               select(m0_active, ch_bool(false),
                                      select(prefer_m0,
                                             select(m1_only, ch_bool(true), ch_bool(false)),
                                             select(m1_req, ch_bool(true), ch_bool(false)))));
        
        // 更新仲裁器：在**整笔事务完成**（B/R 响应握手）后轮转。
        // 原实现用 AW 握手作为 transaction_done —— AXI4-Lite 允许 AW 与 W 相隔
        // 任意周期（本项目的 slave 是相位分离的），AW 握手一完成就切换授权 →
        // WDATA 随后到达时已被路由到另一条通道 → 写数据丢失。
        auto b_done_s0 = io().s0_bvalid & io().s0_bready;
        auto b_done_s1 = io().s1_bvalid & io().s1_bready;
        auto r_done_s0 = io().s0_rvalid & io().s0_rready;
        auto r_done_s1 = io().s1_rvalid & io().s1_rready;
        auto transaction_done = b_done_s0 | b_done_s1 | r_done_s0 | r_done_s1;
        arb_state->next = select(transaction_done, !arb_state, arb_state);

        // 在途事务标志：AW 握手置位，响应握手（B/R 完成）清零
        // 目标从设备在 AW 握手当拍锁存（AXI 允许 AW 与 W 相隔任意周期）
        auto m0_resp_done = (io().m0_bvalid & io().m0_bready) | (io().m0_rvalid & io().m0_rready);
        auto m1_resp_done = (io().m1_bvalid & io().m1_bready) | (io().m1_rvalid & io().m1_rready);
        m0_active->next = select(m0_aw_hs, ch_bool(true), select(m0_resp_done, ch_bool(false), m0_active));
        m1_active->next = select(m1_aw_hs, ch_bool(true), select(m1_resp_done, ch_bool(false), m1_active));
        m0_route_s1->next = select(m0_aw_hs, select_s1_m0, m0_route_s1);
        m1_route_s1->next = select(m1_aw_hs, select_s1_m1, m1_route_s1);
        
        // ==================== 写地址通道 ====================
        
        // 主设备 0 → 从设备
        auto grant_m0_s0 = select(grant_m0, select(!select_s1_m0, ch_bool(true), ch_bool(false)), ch_bool(false));
        auto grant_m0_s1 = select(grant_m0, select_s1_m0, ch_bool(false));
        
        // 主设备 1 → 从设备
        auto grant_m1_s0 = select(grant_m1, select(!select_s1_m1, ch_bool(true), ch_bool(false)), ch_bool(false));
        auto grant_m1_s1 = select(grant_m1, select_s1_m1, ch_bool(false));


        // 合并两个主设备的地址（m0 优先）—— 必须**单次赋值**：
        // 旧写法先赋 m0 再用 `select(grant_m1_s0, m1_awaddr, io().s0_awaddr)` 读回
        // 自己来表达优先级，形成"输出读自己"的自环 → 拓扑排序报
        // "Cycle detected in IR graph"（此前因赋值是 no-op 而被掩盖）。
        auto s0_awaddr_val = select(grant_m0_s0, io().m0_awaddr,
                                     select(grant_m1_s0, io().m1_awaddr, ch_uint<ADDR_WIDTH>(0_d)));
        auto s1_awaddr_val = select(grant_m0_s1, io().m0_awaddr,
                                     select(grant_m1_s1, io().m1_awaddr, ch_uint<ADDR_WIDTH>(0_d)));
        io().s0_awaddr = s0_awaddr_val;
        io().s1_awaddr = s1_awaddr_val;

        // AWVALID / AWPROT 转发（原实现**完全缺失** —— 从设备的 AWVALID 是悬空输出，
        // 恒为未驱动值 0 → 从设备永不接受 AW 地址，事务在第一拍就断掉）
        io().s0_awvalid = select(grant_m0_s0, io().m0_awvalid,
                           select(grant_m1_s0, io().m1_awvalid, ch_bool(false)));
        io().s1_awvalid = select(grant_m0_s1, io().m0_awvalid,
                           select(grant_m1_s1, io().m1_awvalid, ch_bool(false)));
        io().s0_awprot = select(grant_m0_s0, io().m0_awprot,
                          select(grant_m1_s0, io().m1_awprot, ch_uint<2>(0_d)));
        io().s1_awprot = select(grant_m0_s1, io().m0_awprot,
                          select(grant_m1_s1, io().m1_awprot, ch_uint<2>(0_d)));
        
        // 从设备就绪
        
        // 主设备就绪
        io().m0_awready = select(!select_s1_m0, io().s0_awready, io().s1_awready);
        io().m1_awready = select(!select_s1_m1, io().s0_awready, io().s1_awready);
        
        // ==================== 写数据通道 (简化) ====================
        
        io().s0_wdata = select(grant_m0, select(m0_to_s0, io().m0_wdata, ch_uint<DATA_WIDTH>(0_d)),
                          select(grant_m1, select(m1_to_s0, io().m1_wdata, ch_uint<DATA_WIDTH>(0_d)), ch_uint<DATA_WIDTH>(0_d)));
        io().s1_wdata = select(grant_m0, select(m0_to_s0, ch_uint<DATA_WIDTH>(0_d), io().m0_wdata),
                          select(grant_m1, select(m1_to_s0, ch_uint<DATA_WIDTH>(0_d), io().m1_wdata), ch_uint<DATA_WIDTH>(0_d)));
        
        io().s0_wstrb = select(grant_m0, select(m0_to_s0, io().m0_wstrb, ch_uint<DATA_WIDTH/8>(0_d)),
                          select(grant_m1, select(m1_to_s0, io().m1_wstrb, ch_uint<DATA_WIDTH/8>(0_d)), ch_uint<DATA_WIDTH/8>(0_d)));
        io().s1_wstrb = select(grant_m0, select(m0_to_s0, ch_uint<DATA_WIDTH/8>(0_d), io().m0_wstrb),
                          select(grant_m1, select(m1_to_s0, ch_uint<DATA_WIDTH/8>(0_d), io().m1_wstrb), ch_uint<DATA_WIDTH/8>(0_d)));
        
        // WVALID 转发（原先**完全缺失** —— s0_wvalid 声明为输入且无任何赋值，
        // 导致从设备的 WVALID 恒为未驱动值 → 写数据永不被接受）
        // W 通道：**按锁存的路由**转发（AW 握手时已确定目标从设备）
        io().s0_wvalid = select(grant_m0, select(m0_to_s0, io().m0_wvalid, ch_bool(false)),
                          select(grant_m1, select(m1_to_s0, io().m1_wvalid, ch_bool(false)), ch_bool(false)));
        io().s1_wvalid = select(grant_m0, select(m0_to_s0, ch_bool(false), io().m0_wvalid),
                          select(grant_m1, select(m1_to_s0, ch_bool(false), io().m1_wvalid), ch_bool(false)));

        
        io().m0_wready = select(!select_s1_m0, io().s0_wready, io().s1_wready);
        io().m1_wready = select(!select_s1_m1, io().s0_wready, io().s1_wready);
        
        // ==================== 写响应通道 (简化) ====================
        
        io().m0_bresp = select(!select_s1_m0, io().s0_bresp, io().s1_bresp);
        io().m1_bresp = select(!select_s1_m1, io().s0_bresp, io().s1_bresp);
        
        io().m0_bvalid = select(!select_s1_m0, io().s0_bvalid, io().s1_bvalid);
        io().m1_bvalid = select(!select_s1_m1, io().s0_bvalid, io().s1_bvalid);
        
        io().s0_bready = select(grant_m0_s0, io().m0_bready,
                           select(grant_m1_s0, io().m1_bready, ch_bool(false)));
        io().s1_bready = select(grant_m0_s1, io().m0_bready,
                           select(grant_m1_s1, io().m1_bready, ch_bool(false)));
        
        // ==================== 读地址通道 (简化) ====================
        
        // AR 通道：按**读地址**解码路由（写通道的 grant 组合只适用于 AW）
        auto grant_m0_s0_ar = select(grant_m0, select(!ar_select_s1_m0, ch_bool(true), ch_bool(false)), ch_bool(false));
        auto grant_m1_s0_ar = select(grant_m1, select(!ar_select_s1_m1, ch_bool(true), ch_bool(false)), ch_bool(false));
        auto grant_m0_s1_ar = select(grant_m0, ar_select_s1_m0, ch_bool(false));
        auto grant_m1_s1_ar = select(grant_m1, ar_select_s1_m1, ch_bool(false));
        io().s0_araddr = select(grant_m0_s0_ar, io().m0_araddr,
                           select(grant_m1_s0_ar, io().m1_araddr, ch_uint<ADDR_WIDTH>(0_d)));
        io().s1_araddr = select(grant_m0_s1_ar, io().m0_araddr,
                           select(grant_m1_s1_ar, io().m1_araddr, ch_uint<ADDR_WIDTH>(0_d)));
        // ARVALID / ARPROT 转发（同 AW 通道，原实现缺失）
        io().s0_arvalid = select(grant_m0_s0_ar, io().m0_arvalid,
                           select(grant_m1_s0_ar, io().m1_arvalid, ch_bool(false)));
        io().s1_arvalid = select(grant_m0_s1_ar, io().m0_arvalid,
                           select(grant_m1_s1_ar, io().m1_arvalid, ch_bool(false)));
        io().s0_arprot = select(grant_m0_s0_ar, io().m0_arprot,
                          select(grant_m1_s0_ar, io().m1_arprot, ch_uint<2>(0_d)));
        io().s1_arprot = select(grant_m0_s1_ar, io().m0_arprot,
                          select(grant_m1_s1_ar, io().m1_arprot, ch_uint<2>(0_d)));
        
        
        io().m0_arready = select(!ar_select_s1_m0, io().s0_arready, io().s1_arready);
        io().m1_arready = select(!ar_select_s1_m1, io().s0_arready, io().s1_arready);
        
        // ==================== 读数据通道 (简化) ====================
        
        io().m0_rdata = select(!ar_select_s1_m0, io().s0_rdata, io().s1_rdata);
        io().m0_rresp = select(!ar_select_s1_m0, io().s0_rresp, io().s1_rresp);
        io().m1_rdata = select(!ar_select_s1_m1, io().s0_rdata, io().s1_rdata);
        io().m1_rresp = select(!ar_select_s1_m1, io().s0_rresp, io().s1_rresp);
        
        io().m0_rvalid = select(!ar_select_s1_m0, io().s0_rvalid, io().s1_rvalid);
        io().m1_rvalid = select(!ar_select_s1_m1, io().s0_rvalid, io().s1_rvalid);
        
        io().s0_rready = select(grant_m0_s0, io().m0_rready,
                           select(grant_m1_s0, io().m1_rready, ch_bool(false)));
        io().s1_rready = select(grant_m0_s1, io().m0_rready,
                           select(grant_m1_s1, io().m1_rready, ch_bool(false)));
    }
};

} // namespace axi4
