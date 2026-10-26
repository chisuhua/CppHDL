/**
 * @file axi_interconnect_4x4.h
 * @brief 4x4 AXI4 Crossbar Interconnect with Round-Robin Arbiter
 * 
 * 4 master x 4 slave AXI4 full crossbar with:
 * - Round-robin arbiter
 * - Address decoder (top 2 bits of [31:30] route to slave 0-3)
 * - Combinational routing on all 5 channels (AW, W, B, AR, R)
 * - Unmapped addresses return DECERR (ADR-010 compliance)
 */

#pragma once

#include "ch.hpp"
#include "component.h"
#include "axi4/interconnect_common.h"

namespace axi4 {

using namespace ch::core;

template <
    unsigned ADDR_WIDTH = 32,
    unsigned DATA_WIDTH = 32,
    unsigned ID_WIDTH = 4,
    typename Map = AxiDefaultInterconnectMap
>
class AxiInterconnect4x4 : public ch::Component {
public:
    static constexpr unsigned NUM_BYTES = DATA_WIDTH / 8;
    
    __io(
        // Master 0 ports
        ch_in<ch_uint<ADDR_WIDTH>> m0_awaddr; ch_in<ch_uint<ID_WIDTH>> m0_awid;
        ch_in<ch_uint<4>> m0_awlen; ch_in<ch_uint<3>> m0_awsize; ch_in<ch_uint<2>> m0_awburst;
        ch_in<ch_bool> m0_awvalid; ch_out<ch_bool> m0_awready;
        ch_in<ch_uint<DATA_WIDTH>> m0_wdata; ch_in<ch_uint<ID_WIDTH>> m0_wid;
        ch_in<ch_uint<NUM_BYTES>> m0_wstrb; ch_in<ch_bool> m0_wlast; ch_in<ch_bool> m0_wvalid;
        ch_out<ch_bool> m0_wready;
        ch_out<ch_uint<ID_WIDTH>> m0_bid; ch_out<ch_uint<2>> m0_bresp; ch_out<ch_bool> m0_bvalid;
        ch_in<ch_bool> m0_bready;
        ch_in<ch_uint<ADDR_WIDTH>> m0_araddr; ch_in<ch_uint<ID_WIDTH>> m0_arid;
        ch_in<ch_uint<4>> m0_arlen; ch_in<ch_uint<3>> m0_arsize; ch_in<ch_uint<2>> m0_arburst;
        ch_in<ch_bool> m0_arvalid; ch_out<ch_bool> m0_arready;
        ch_out<ch_uint<DATA_WIDTH>> m0_rdata; ch_out<ch_uint<ID_WIDTH>> m0_rid;
        ch_out<ch_uint<2>> m0_rresp; ch_out<ch_bool> m0_rlast; ch_out<ch_bool> m0_rvalid;
        ch_in<ch_bool> m0_rready;
        
        ch_in<ch_uint<ADDR_WIDTH>> m1_awaddr; ch_in<ch_uint<ID_WIDTH>> m1_awid;
        ch_in<ch_uint<4>> m1_awlen; ch_in<ch_uint<3>> m1_awsize; ch_in<ch_uint<2>> m1_awburst;
        ch_in<ch_bool> m1_awvalid; ch_out<ch_bool> m1_awready;
        ch_in<ch_uint<DATA_WIDTH>> m1_wdata; ch_in<ch_uint<ID_WIDTH>> m1_wid;
        ch_in<ch_uint<NUM_BYTES>> m1_wstrb; ch_in<ch_bool> m1_wlast; ch_in<ch_bool> m1_wvalid;
        ch_out<ch_bool> m1_wready;
        ch_out<ch_uint<ID_WIDTH>> m1_bid; ch_out<ch_uint<2>> m1_bresp; ch_out<ch_bool> m1_bvalid;
        ch_in<ch_bool> m1_bready;
        ch_in<ch_uint<ADDR_WIDTH>> m1_araddr; ch_in<ch_uint<ID_WIDTH>> m1_arid;
        ch_in<ch_uint<4>> m1_arlen; ch_in<ch_uint<3>> m1_arsize; ch_in<ch_uint<2>> m1_arburst;
        ch_in<ch_bool> m1_arvalid; ch_out<ch_bool> m1_arready;
        ch_out<ch_uint<DATA_WIDTH>> m1_rdata; ch_out<ch_uint<ID_WIDTH>> m1_rid;
        ch_out<ch_uint<2>> m1_rresp; ch_out<ch_bool> m1_rlast; ch_out<ch_bool> m1_rvalid;
        ch_in<ch_bool> m1_rready;
        
        ch_in<ch_uint<ADDR_WIDTH>> m2_awaddr; ch_in<ch_uint<ID_WIDTH>> m2_awid;
        ch_in<ch_uint<4>> m2_awlen; ch_in<ch_uint<3>> m2_awsize; ch_in<ch_uint<2>> m2_awburst;
        ch_in<ch_bool> m2_awvalid; ch_out<ch_bool> m2_awready;
        ch_in<ch_uint<DATA_WIDTH>> m2_wdata; ch_in<ch_uint<ID_WIDTH>> m2_wid;
        ch_in<ch_uint<NUM_BYTES>> m2_wstrb; ch_in<ch_bool> m2_wlast; ch_in<ch_bool> m2_wvalid;
        ch_out<ch_bool> m2_wready;
        ch_out<ch_uint<ID_WIDTH>> m2_bid; ch_out<ch_uint<2>> m2_bresp; ch_out<ch_bool> m2_bvalid;
        ch_in<ch_bool> m2_bready;
        ch_in<ch_uint<ADDR_WIDTH>> m2_araddr; ch_in<ch_uint<ID_WIDTH>> m2_arid;
        ch_in<ch_uint<4>> m2_arlen; ch_in<ch_uint<3>> m2_arsize; ch_in<ch_uint<2>> m2_arburst;
        ch_in<ch_bool> m2_arvalid; ch_out<ch_bool> m2_arready;
        ch_out<ch_uint<DATA_WIDTH>> m2_rdata; ch_out<ch_uint<ID_WIDTH>> m2_rid;
        ch_out<ch_uint<2>> m2_rresp; ch_out<ch_bool> m2_rlast; ch_out<ch_bool> m2_rvalid;
        ch_in<ch_bool> m2_rready;
        
        ch_in<ch_uint<ADDR_WIDTH>> m3_awaddr; ch_in<ch_uint<ID_WIDTH>> m3_awid;
        ch_in<ch_uint<4>> m3_awlen; ch_in<ch_uint<3>> m3_awsize; ch_in<ch_uint<2>> m3_awburst;
        ch_in<ch_bool> m3_awvalid; ch_out<ch_bool> m3_awready;
        ch_in<ch_uint<DATA_WIDTH>> m3_wdata; ch_in<ch_uint<ID_WIDTH>> m3_wid;
        ch_in<ch_uint<NUM_BYTES>> m3_wstrb; ch_in<ch_bool> m3_wlast; ch_in<ch_bool> m3_wvalid;
        ch_out<ch_bool> m3_wready;
        ch_out<ch_uint<ID_WIDTH>> m3_bid; ch_out<ch_uint<2>> m3_bresp; ch_out<ch_bool> m3_bvalid;
        ch_in<ch_bool> m3_bready;
        ch_in<ch_uint<ADDR_WIDTH>> m3_araddr; ch_in<ch_uint<ID_WIDTH>> m3_arid;
        ch_in<ch_uint<4>> m3_arlen; ch_in<ch_uint<3>> m3_arsize; ch_in<ch_uint<2>> m3_arburst;
        ch_in<ch_bool> m3_arvalid; ch_out<ch_bool> m3_arready;
        ch_out<ch_uint<DATA_WIDTH>> m3_rdata; ch_out<ch_uint<ID_WIDTH>> m3_rid;
        ch_out<ch_uint<2>> m3_rresp; ch_out<ch_bool> m3_rlast; ch_out<ch_bool> m3_rvalid;
        ch_in<ch_bool> m3_rready;
        
        ch_out<ch_uint<ADDR_WIDTH>> s0_awaddr; ch_out<ch_uint<ID_WIDTH>> s0_awid;
        ch_out<ch_uint<4>> s0_awlen; ch_out<ch_uint<3>> s0_awsize; ch_out<ch_uint<2>> s0_awburst;
        ch_out<ch_bool> s0_awvalid; ch_in<ch_bool> s0_awready;
        ch_out<ch_uint<DATA_WIDTH>> s0_wdata; ch_out<ch_uint<ID_WIDTH>> s0_wid;
        ch_out<ch_uint<NUM_BYTES>> s0_wstrb; ch_out<ch_bool> s0_wlast; ch_out<ch_bool> s0_wvalid;
        ch_in<ch_bool> s0_wready;
        ch_in<ch_uint<ID_WIDTH>> s0_bid; ch_in<ch_uint<2>> s0_bresp; ch_in<ch_bool> s0_bvalid;
        ch_out<ch_bool> s0_bready;
        ch_out<ch_uint<ADDR_WIDTH>> s0_araddr; ch_out<ch_uint<ID_WIDTH>> s0_arid;
        ch_out<ch_uint<4>> s0_arlen; ch_out<ch_uint<3>> s0_arsize; ch_out<ch_uint<2>> s0_arburst;
        ch_out<ch_bool> s0_arvalid; ch_in<ch_bool> s0_arready;
        ch_in<ch_uint<DATA_WIDTH>> s0_rdata; ch_in<ch_uint<ID_WIDTH>> s0_rid;
        ch_in<ch_uint<2>> s0_rresp; ch_in<ch_bool> s0_rlast; ch_in<ch_bool> s0_rvalid;
        ch_out<ch_bool> s0_rready;
        
        ch_out<ch_uint<ADDR_WIDTH>> s1_awaddr; ch_out<ch_uint<ID_WIDTH>> s1_awid;
        ch_out<ch_uint<4>> s1_awlen; ch_out<ch_uint<3>> s1_awsize; ch_out<ch_uint<2>> s1_awburst;
        ch_out<ch_bool> s1_awvalid; ch_in<ch_bool> s1_awready;
        ch_out<ch_uint<DATA_WIDTH>> s1_wdata; ch_out<ch_uint<ID_WIDTH>> s1_wid;
        ch_out<ch_uint<NUM_BYTES>> s1_wstrb; ch_out<ch_bool> s1_wlast; ch_out<ch_bool> s1_wvalid;
        ch_in<ch_bool> s1_wready;
        ch_in<ch_uint<ID_WIDTH>> s1_bid; ch_in<ch_uint<2>> s1_bresp; ch_in<ch_bool> s1_bvalid;
        ch_out<ch_bool> s1_bready;
        ch_out<ch_uint<ADDR_WIDTH>> s1_araddr; ch_out<ch_uint<ID_WIDTH>> s1_arid;
        ch_out<ch_uint<4>> s1_arlen; ch_out<ch_uint<3>> s1_arsize; ch_out<ch_uint<2>> s1_arburst;
        ch_out<ch_bool> s1_arvalid; ch_in<ch_bool> s1_arready;
        ch_in<ch_uint<DATA_WIDTH>> s1_rdata; ch_in<ch_uint<ID_WIDTH>> s1_rid;
        ch_in<ch_uint<2>> s1_rresp; ch_in<ch_bool> s1_rlast; ch_in<ch_bool> s1_rvalid;
        ch_out<ch_bool> s1_rready;
        
        ch_out<ch_uint<ADDR_WIDTH>> s2_awaddr; ch_out<ch_uint<ID_WIDTH>> s2_awid;
        ch_out<ch_uint<4>> s2_awlen; ch_out<ch_uint<3>> s2_awsize; ch_out<ch_uint<2>> s2_awburst;
        ch_out<ch_bool> s2_awvalid; ch_in<ch_bool> s2_awready;
        ch_out<ch_uint<DATA_WIDTH>> s2_wdata; ch_out<ch_uint<ID_WIDTH>> s2_wid;
        ch_out<ch_uint<NUM_BYTES>> s2_wstrb; ch_out<ch_bool> s2_wlast; ch_out<ch_bool> s2_wvalid;
        ch_in<ch_bool> s2_wready;
        ch_in<ch_uint<ID_WIDTH>> s2_bid; ch_in<ch_uint<2>> s2_bresp; ch_in<ch_bool> s2_bvalid;
        ch_out<ch_bool> s2_bready;
        ch_out<ch_uint<ADDR_WIDTH>> s2_araddr; ch_out<ch_uint<ID_WIDTH>> s2_arid;
        ch_out<ch_uint<4>> s2_arlen; ch_out<ch_uint<3>> s2_arsize; ch_out<ch_uint<2>> s2_arburst;
        ch_out<ch_bool> s2_arvalid; ch_in<ch_bool> s2_arready;
        ch_in<ch_uint<DATA_WIDTH>> s2_rdata; ch_in<ch_uint<ID_WIDTH>> s2_rid;
        ch_in<ch_uint<2>> s2_rresp; ch_in<ch_bool> s2_rlast; ch_in<ch_bool> s2_rvalid;
        ch_out<ch_bool> s2_rready;
        
        ch_out<ch_uint<ADDR_WIDTH>> s3_awaddr; ch_out<ch_uint<ID_WIDTH>> s3_awid;
        ch_out<ch_uint<4>> s3_awlen; ch_out<ch_uint<3>> s3_awsize; ch_out<ch_uint<2>> s3_awburst;
        ch_out<ch_bool> s3_awvalid; ch_in<ch_bool> s3_awready;
        ch_out<ch_uint<DATA_WIDTH>> s3_wdata; ch_out<ch_uint<ID_WIDTH>> s3_wid;
        ch_out<ch_uint<NUM_BYTES>> s3_wstrb; ch_out<ch_bool> s3_wlast; ch_out<ch_bool> s3_wvalid;
        ch_in<ch_bool> s3_wready;
        ch_in<ch_uint<ID_WIDTH>> s3_bid; ch_in<ch_uint<2>> s3_bresp; ch_in<ch_bool> s3_bvalid;
        ch_out<ch_bool> s3_bready;
        ch_out<ch_uint<ADDR_WIDTH>> s3_araddr; ch_out<ch_uint<ID_WIDTH>> s3_arid;
        ch_out<ch_uint<4>> s3_arlen; ch_out<ch_uint<3>> s3_arsize; ch_out<ch_uint<2>> s3_arburst;
        ch_out<ch_bool> s3_arvalid; ch_in<ch_bool> s3_arready;
        ch_in<ch_uint<DATA_WIDTH>> s3_rdata; ch_in<ch_uint<ID_WIDTH>> s3_rid;
        ch_in<ch_uint<2>> s3_rresp; ch_in<ch_bool> s3_rlast; ch_in<ch_bool> s3_rvalid;
        ch_out<ch_bool> s3_rready;

        // 复位（低有效，**在时钟沿生效** —— 配 `select(!io().rst_n, <init>, ...)`）。
        // `rst_n` 是**必需端口**，实例化点 MUST 显式接线；未接线读出为 0 = **永久复位**
        // （与 `axi-slave-conformance` 的「未接线的 rst_n 等价于永久复位」同源）。
        // 互连层是 rst_n 链路的**中间节点**，MUST 与下游 slave 在同一复位窗口内复位。
        ch_in<ch_bool> rst_n;
    )
    
    AxiInterconnect4x4(ch::Component* parent = nullptr, const std::string& name = "axi_interconnect_4x4")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    // ======================================================================
    // 状态
    // ======================================================================
    // 全部 `ch_reg` MUST 有 `select(!io().rst_n, <init>, ...)` 复位 pattern
    // （低有效，**在时钟沿生效** —— 落在 `->next` 表达式上由时钟沿采样，**不是**异步复位）；
    // `<init>` MUST 带该寄存器的**显式位宽**。
    //
    // 设计要点：
    //   * 路由目标在 **AW / AR 握手当拍**锁存 ⇒ W / B / R 通道 MUST 用**锁存值**转发，
    //     MUST NOT 用活的 `awaddr` / `araddr`（否则 master 握手后改写地址会改错落点）。
    //   * 握手**决策**（`s*_awvalid` / `s*_arvalid` 的正向转发、`m*_awready` / `m*_arready`
    //     的反查）MUST 用**当拍**地址解码 + **当拍** grant（AXI 要求 master 在
    //     `awvalid && !awready` 期间保持地址稳定，故握手窗口内的活地址是安全的）。
    //   * 写 / 读 MUST 各自独立仲裁（AXI4 的 AW/W/B 与 AR/R 是独立通道组）。
    struct IcState {
        ch_reg<ch_uint<2>> aw_ptr; // 写仲裁 round-robin 指针
        ch_reg<ch_uint<2>> ar_ptr; // 读仲裁 round-robin 指针

        // 写侧：AW 握手当拍锁存
        ch_reg<ch_uint<2>> wr_route_m0, wr_route_m1, wr_route_m2, wr_route_m3;
        ch_reg<ch_bool>    wr_mapped_m0, wr_mapped_m1, wr_mapped_m2, wr_mapped_m3;
        ch_reg<ch_bool>    wr_active_m0, wr_active_m1, wr_active_m2, wr_active_m3;

        // 读侧：AR 握手当拍锁存
        ch_reg<ch_uint<2>> rd_route_m0, rd_route_m1, rd_route_m2, rd_route_m3;
        ch_reg<ch_bool>    rd_mapped_m0, rd_mapped_m1, rd_mapped_m2, rd_mapped_m3;
        ch_reg<ch_bool>    rd_active_m0, rd_active_m1, rd_active_m2, rd_active_m3;

        // error responder（写）：未映射地址由互连层自己应答
        ch_reg<ch_bool> err_w_busy;      // 已被一笔未映射写占用（AW 已握手，W 未收完）
        ch_reg<ch_bool> err_w_valid;     // bvalid，**最后一个 W beat 被接受后**才置位
        ch_reg<ch_uint<4>> err_w_remaining;  // 当前 W beat **之后**还需被接受的 beat 数
        ch_reg<ch_uint<ID_WIDTH>> err_w_id;   // 锁存的 awid（DECERR 响应 MUST 回显事务 ID）

        // error responder（读）：未映射读返回 `arlen + 1` 个 DECERR beat
        ch_reg<ch_bool> err_r_valid;         // rvalid（流式：整个 burst 期间为 1）
        ch_reg<ch_uint<4>> err_r_remaining;  // 当前 R beat **之后**还需被返回的 beat 数
        ch_reg<ch_uint<ID_WIDTH>> err_r_id;   // 锁存的 arid（DECERR 响应 MUST 回显事务 ID）

        IcState()
            : aw_ptr(0_d), ar_ptr(0_d),
              wr_route_m0(0_d), wr_route_m1(0_d), wr_route_m2(0_d), wr_route_m3(0_d),
              wr_mapped_m0(false), wr_mapped_m1(false), wr_mapped_m2(false), wr_mapped_m3(false),
              wr_active_m0(false), wr_active_m1(false), wr_active_m2(false), wr_active_m3(false),
              rd_route_m0(0_d), rd_route_m1(0_d), rd_route_m2(0_d), rd_route_m3(0_d),
              rd_mapped_m0(false), rd_mapped_m1(false), rd_mapped_m2(false), rd_mapped_m3(false),
              rd_active_m0(false), rd_active_m1(false), rd_active_m2(false), rd_active_m3(false),
              err_w_busy(false), err_w_valid(false), err_w_remaining(0_d), err_w_id(0_d),
              err_r_valid(false), err_r_remaining(0_d), err_r_id(0_d) {}
    };

    // -------- 4 选 1 正向转发（**单次赋值** —— MUST NOT 写成「输出读自己」的自环，
    //          那会让拓扑排序报 `Cycle detected in IR graph`）--------
    // `V` 独立推导：实参是 `ch_in<T>`（端口），T 只由 `ch_out<T>` 与 `dflt` 决定
    // ⇒ 避免「deduced conflicting types for T」。端口直接进 `select`（与旧代码同一形态）。
    template <typename T, typename V>
    static void fwd4(ch_out<T> &dst, const ch_bool &s0, const ch_bool &s1,
                     const ch_bool &s2, const ch_bool &s3,
                     const V &v0, const V &v1, const V &v2, const V &v3, const T &dflt) {
        dst = select(s0, v0, select(s1, v1, select(s2, v2, select(s3, v3, dflt))));
    }

    // -------- 按 2 bit 索引 4 选 1（反向通道用**锁存路由**反查）--------
    // ⚠️ 第 4 个实参 MUST 是**真实的 s3 信号** —— 退化成常量会让 `idx==3` 恒返回该常量
    // （这正是旧实现里「slave 3 的 bresp 恒 DECERR」的成因）。
    template <typename V>
    static auto idx4(const ch_uint<2> &idx, const V &v0, const V &v1,
                     const V &v2, const V &v3) {
        return select(idx == ch_uint<2>(0_d), v0,
               select(idx == ch_uint<2>(1_d), v1,
               select(idx == ch_uint<2>(2_d), v2, v3)));
    }

    // -------- 地址窗口命中：`addr & ~(size-1) == base`（编译期常量掩码）--------
    // 地址位掩码（ADDR_WIDTH 位全 1）—— `ADDR_WIDTH % 64` 避免 `1 << 64` 的 UB。
    static constexpr uint64_t addr_bits_mask() {
        static_assert(ADDR_WIDTH > 0 && ADDR_WIDTH <= 64,
                      "ADDR_WIDTH MUST 在 1..64 —— 0 会让掩码恒 0（全地址命中），"
                      "> 64 会静默丢掉地址高位");
        return ADDR_WIDTH >= 64 ? ~uint64_t(0)
                                 : ((uint64_t(1) << (ADDR_WIDTH % 64)) - uint64_t(1));
    }

    static ch_bool window_hit(const ch_uint<ADDR_WIDTH> &addr, uint64_t base, uint64_t size) {
        const uint64_t mask = ~(size - 1ull);
        // ⚠️ MUST 先把 base / mask 截到 ADDR_WIDTH 位 —— 否则 `ch_uint<ADDR_WIDTH>(mask)`
        //    会用 48/64 位值构造 32 bit 逻辑值 ⇒ **调试构建**（assert 打开）触发
        //    `CH_CHECK`「value out of range」；NDEBUG 构建下静默截断（行为仍正确）
        //    ⇒ 该缺陷此前被 NDEBUG 掩盖。
        const uint64_t m = mask & addr_bits_mask();
        const uint64_t b = base & addr_bits_mask();
        return (addr & ch_uint<ADDR_WIDTH>(m)) == ch_uint<ADDR_WIDTH>(b);
    }

    // -------- 当拍地址解码：`target` = 命中的 slave 索引，`mapped` = 是否落在窗口内 --------
    void decode_addr(const ch_uint<ADDR_WIDTH> &addr, ch_uint<2> &target, ch_bool &mapped) {
        // ⚠️ 全部 4 个窗口都 MUST 校验（此前只校验了 [0]）—— 窗口定义是**编译期常量**，
        //    传错（size 非 2 的幂 / base 未对齐）会得到**静默的错误解码**。
        static_assert(axi_window_is_valid(Map::windows[0]), "地址窗口 0 非法：size MUST 非 0 且为 2 的幂，base MUST 按 size 对齐");
        static_assert(axi_window_is_valid(Map::windows[1]), "地址窗口 1 非法：size MUST 非 0 且为 2 的幂，base MUST 按 size 对齐");
        static_assert(axi_window_is_valid(Map::windows[2]), "地址窗口 2 非法：size MUST 非 0 且为 2 的幂，base MUST 按 size 对齐");
        static_assert(axi_window_is_valid(Map::windows[3]), "地址窗口 3 非法：size MUST 非 0 且为 2 的幂，base MUST 按 size 对齐");
        static_assert(axi_window_fits(Map::windows[0], ADDR_WIDTH),
                      "地址窗口 0 超出 ADDR_WIDTH 位地址空间：base 的高位会被静默丢掉 "
                      "⇒ 别名到低地址（与低窗口双命中 ⇒ 静默错路由）");
        static_assert(axi_window_fits(Map::windows[1], ADDR_WIDTH),
                      "地址窗口 1 超出 ADDR_WIDTH 位地址空间：base 的高位会被静默丢掉 "
                      "⇒ 别名到低地址（与低窗口双命中 ⇒ 静默错路由）");
        static_assert(axi_window_fits(Map::windows[2], ADDR_WIDTH),
                      "地址窗口 2 超出 ADDR_WIDTH 位地址空间：base 的高位会被静默丢掉 "
                      "⇒ 别名到低地址（与低窗口双命中 ⇒ 静默错路由）");
        static_assert(axi_window_fits(Map::windows[3], ADDR_WIDTH),
                      "地址窗口 3 超出 ADDR_WIDTH 位地址空间：base 的高位会被静默丢掉 "
                      "⇒ 别名到低地址（与低窗口双命中 ⇒ 静默错路由）");
        ch_bool h0 = window_hit(addr, Map::windows[0].base, Map::windows[0].size);
        ch_bool h1 = window_hit(addr, Map::windows[1].base, Map::windows[1].size);
        ch_bool h2 = window_hit(addr, Map::windows[2].base, Map::windows[2].size);
        ch_bool h3 = window_hit(addr, Map::windows[3].base, Map::windows[3].size);
        mapped = h0 | h1 | h2 | h3;
        target = select(h0, ch_uint<2>(0_d),
                 select(h1, ch_uint<2>(1_d),
                 select(h2, ch_uint<2>(2_d), ch_uint<2>(3_d))));
    }

    // -------- round-robin 仲裁：在途保持授权 + 指针指向无请求者时让位 + 回绕 --------
    template <typename R0, typename R1, typename R2, typename R3>
    void arbitrate(const ch_uint<2> &ptr,
                   const R0 &req0, const R1 &req1, const R2 &req2, const R3 &req3,
                   const ch_bool &act0, const ch_bool &act1,
                   const ch_bool &act2, const ch_bool &act3,
                   ch_bool &grant0, ch_bool &grant1, ch_bool &grant2, ch_bool &grant3) {
        ch_bool sel0 = (ptr == ch_uint<2>(0_d));
        ch_bool sel1 = (ptr == ch_uint<2>(1_d));
        ch_bool sel2 = (ptr == ch_uint<2>(2_d));

        // 旋转到「以 ptr 为起点」的请求顺序
        ch_bool q0 = select(sel0, req0, select(sel1, req1, select(sel2, req2, req3)));
        ch_bool q1 = select(sel0, req1, select(sel1, req2, select(sel2, req3, req0)));
        ch_bool q2 = select(sel0, req2, select(sel1, req3, select(sel2, req0, req1)));
        ch_bool q3 = select(sel0, req3, select(sel1, req0, select(sel2, req1, req2)));

        // 旋转空间里的优先编码：q0 优先 ⇒ 指针指向的 master 优先；
        // 它无请求时 q0=0 ⇒ 后续请求者接管（**让位**，修永久饿死）
        ch_bool g0 = q0;
        ch_bool g1 = select(q0, ch_bool(false), q1);
        ch_bool g2 = select(q0 | q1, ch_bool(false), q2);
        ch_bool g3 = select(q0 | q1 | q2, ch_bool(false), q3);

        // 旋转回真实 master 序号
        ch_bool rr0 = select(sel0, g0, select(sel1, g3, select(sel2, g2, g1)));
        ch_bool rr1 = select(sel0, g1, select(sel1, g0, select(sel2, g3, g2)));
        ch_bool rr2 = select(sel0, g2, select(sel1, g1, select(sel2, g0, g3)));
        ch_bool rr3 = select(sel0, g3, select(sel1, g2, select(sel2, g1, g0)));

        // 在途事务的 master MUST 保持授权（active 优先于轮询）
        ch_bool any_act = act0 | act1 | act2 | act3;
        grant0 = select(any_act, act0, rr0);
        grant1 = select(any_act, act1, rr1);
        grant2 = select(any_act, act2, rr2);
        grant3 = select(any_act, act3, rr3);
    }

    void describe() override {
        IcState s;

        // ================= 1. 每 master 的**当拍**地址解码 =================
        // 写侧 MUST 用 awaddr、读侧 MUST 用 araddr（此前读通道复用写地址 ⇒ 读事务按写地址路由）
        ch_uint<2> aw_target_m0, aw_target_m1, aw_target_m2, aw_target_m3;
        ch_uint<2> ar_target_m0, ar_target_m1, ar_target_m2, ar_target_m3;
        ch_bool aw_mapped_m0, aw_mapped_m1, aw_mapped_m2, aw_mapped_m3;
        ch_bool ar_mapped_m0, ar_mapped_m1, ar_mapped_m2, ar_mapped_m3;
        decode_addr(io().m0_awaddr, aw_target_m0, aw_mapped_m0);
        decode_addr(io().m1_awaddr, aw_target_m1, aw_mapped_m1);
        decode_addr(io().m2_awaddr, aw_target_m2, aw_mapped_m2);
        decode_addr(io().m3_awaddr, aw_target_m3, aw_mapped_m3);
        decode_addr(io().m0_araddr, ar_target_m0, ar_mapped_m0);
        decode_addr(io().m1_araddr, ar_target_m1, ar_mapped_m1);
        decode_addr(io().m2_araddr, ar_target_m2, ar_mapped_m2);
        decode_addr(io().m3_araddr, ar_target_m3, ar_mapped_m3);

        // ================= 2. 仲裁（AW / AR 各自独立）=================
        // 请求取自该通道**自己的** valid；轮转只由该方向**事务真正完成**触发
        ch_bool aw_grant_m0, aw_grant_m1, aw_grant_m2, aw_grant_m3;
        arbitrate(s.aw_ptr,
                  io().m0_awvalid, io().m1_awvalid, io().m2_awvalid, io().m3_awvalid,
                  s.wr_active_m0, s.wr_active_m1, s.wr_active_m2, s.wr_active_m3,
                  aw_grant_m0, aw_grant_m1, aw_grant_m2, aw_grant_m3);
        ch_bool ar_grant_m0, ar_grant_m1, ar_grant_m2, ar_grant_m3;
        arbitrate(s.ar_ptr,
                  io().m0_arvalid, io().m1_arvalid, io().m2_arvalid, io().m3_arvalid,
                  s.rd_active_m0, s.rd_active_m1, s.rd_active_m2, s.rd_active_m3,
                  ar_grant_m0, ar_grant_m1, ar_grant_m2, ar_grant_m3);

        // ================= 3. error responder 的接受能力 =================
        // ⚠️ 读侧**不需要**独立的 `busy` 标志：`err_r_valid` 已覆盖
        //    「AR 已握手 ⇒ 响应正在流式返回」整个窗口（两者转移完全相同 ⇒ 恒等）。
        ch_bool err_w_can_accept = (!s.err_w_busy) & (!s.err_w_valid);
        ch_bool err_r_can_accept = (!s.err_r_valid);

        // ================= 4. 每 (slave, master) 的转发选择 =================
        // 正向（AW / AR）：当拍地址解码 + 当拍 grant + 已映射
        // [slave][master]
        ch_bool aw_sel[4][4];
        ch_bool ar_sel[4][4];
        for (int mi = 0; mi < 4; ++mi) {
            const ch_bool &g = (mi == 0) ? aw_grant_m0 : (mi == 1) ? aw_grant_m1
                             : (mi == 2) ? aw_grant_m2 : aw_grant_m3;
            const ch_bool &mp = (mi == 0) ? aw_mapped_m0 : (mi == 1) ? aw_mapped_m1
                              : (mi == 2) ? aw_mapped_m2 : aw_mapped_m3;
            const ch_uint<2> &tg = (mi == 0) ? aw_target_m0 : (mi == 1) ? aw_target_m1
                                : (mi == 2) ? aw_target_m2 : aw_target_m3;
            // ⚠️ MUST 吃 `!wr_active[mi]` 门控 —— 否则该 master 在自己的写事务
            //    在途期间合法地举起**下一笔** `awvalid`（AXI4 允许多 outstanding）时，
            //    本笔 AW 会被**转发**给 slave 并被其接受，而互连侧 `awready=0`
            //    ⇒ 互连从未锁存这条路由 ⇒ slave 等不到 W、master 等不到 awready
            //    ⇒ **双向永久死锁**（幻影 AW）。
            const ch_bool &act = (mi == 0) ? s.wr_active_m0 : (mi == 1) ? s.wr_active_m1
                               : (mi == 2) ? s.wr_active_m2 : s.wr_active_m3;
            for (int si = 0; si < 4; ++si) {
                aw_sel[si][mi] = (!act) & g & mp & (tg == ch_uint<2>(si));
            }
            const ch_bool &g2 = (mi == 0) ? ar_grant_m0 : (mi == 1) ? ar_grant_m1
                              : (mi == 2) ? ar_grant_m2 : ar_grant_m3;
            const ch_bool &mp2 = (mi == 0) ? ar_mapped_m0 : (mi == 1) ? ar_mapped_m1
                               : (mi == 2) ? ar_mapped_m2 : ar_mapped_m3;
            const ch_uint<2> &tg2 = (mi == 0) ? ar_target_m0 : (mi == 1) ? ar_target_m1
                                 : (mi == 2) ? ar_target_m2 : ar_target_m3;
            // 同上：AR 侧也 MUST 吃 `!rd_active[mi]` 门控（幻影 AR）
            const ch_bool &act2 = (mi == 0) ? s.rd_active_m0 : (mi == 1) ? s.rd_active_m1
                                : (mi == 2) ? s.rd_active_m2 : s.rd_active_m3;
            for (int si = 0; si < 4; ++si) {
                ar_sel[si][mi] = (!act2) & g2 & mp2 & (tg2 == ch_uint<2>(si));
            }
        }

        // 反向（W/B 用写路由锁存；R 用读路由锁存）—— MUST NOT 用活的地址或活的 grant
        ch_bool wr_sel[4][4];
        ch_bool rd_sel[4][4];
        for (int mi = 0; mi < 4; ++mi) {
            const ch_bool &act = (mi == 0) ? s.wr_active_m0 : (mi == 1) ? s.wr_active_m1
                               : (mi == 2) ? s.wr_active_m2 : s.wr_active_m3;
            const ch_bool &mp = (mi == 0) ? s.wr_mapped_m0 : (mi == 1) ? s.wr_mapped_m1
                              : (mi == 2) ? s.wr_mapped_m2 : s.wr_mapped_m3;
            const ch_uint<2> &rt = (mi == 0) ? s.wr_route_m0 : (mi == 1) ? s.wr_route_m1
                                : (mi == 2) ? s.wr_route_m2 : s.wr_route_m3;
            for (int si = 0; si < 4; ++si) {
                wr_sel[si][mi] = act & mp & (rt == ch_uint<2>(si));
            }
            const ch_bool &act2 = (mi == 0) ? s.rd_active_m0 : (mi == 1) ? s.rd_active_m1
                               : (mi == 2) ? s.rd_active_m2 : s.rd_active_m3;
            const ch_bool &mp2 = (mi == 0) ? s.rd_mapped_m0 : (mi == 1) ? s.rd_mapped_m1
                               : (mi == 2) ? s.rd_mapped_m2 : s.rd_mapped_m3;
            const ch_uint<2> &rt2 = (mi == 0) ? s.rd_route_m0 : (mi == 1) ? s.rd_route_m1
                                 : (mi == 2) ? s.rd_route_m2 : s.rd_route_m3;
            for (int si = 0; si < 4; ++si) {
                rd_sel[si][mi] = act2 & mp2 & (rt2 == ch_uint<2>(si));
            }
        }

        // ================= 5. 正向通道：16 对全扇出 =================
        // ---- slave 0 ----
        fwd4(io().s0_awaddr, aw_sel[0][0], aw_sel[0][1], aw_sel[0][2], aw_sel[0][3],
             io().m0_awaddr, io().m1_awaddr, io().m2_awaddr, io().m3_awaddr, ch_uint<ADDR_WIDTH>(0_d));
        fwd4(io().s0_awid, aw_sel[0][0], aw_sel[0][1], aw_sel[0][2], aw_sel[0][3],
             io().m0_awid, io().m1_awid, io().m2_awid, io().m3_awid, ch_uint<ID_WIDTH>(0_d));
        fwd4(io().s0_awlen, aw_sel[0][0], aw_sel[0][1], aw_sel[0][2], aw_sel[0][3],
             io().m0_awlen, io().m1_awlen, io().m2_awlen, io().m3_awlen, ch_uint<4>(0_d));
        fwd4(io().s0_awsize, aw_sel[0][0], aw_sel[0][1], aw_sel[0][2], aw_sel[0][3],
             io().m0_awsize, io().m1_awsize, io().m2_awsize, io().m3_awsize, ch_uint<3>(0_d));
        fwd4(io().s0_awburst, aw_sel[0][0], aw_sel[0][1], aw_sel[0][2], aw_sel[0][3],
             io().m0_awburst, io().m1_awburst, io().m2_awburst, io().m3_awburst, ch_uint<2>(0_d));
        io().s0_awvalid = (aw_sel[0][0] & io().m0_awvalid) | (aw_sel[0][1] & io().m1_awvalid) |
                          (aw_sel[0][2] & io().m2_awvalid) | (aw_sel[0][3] & io().m3_awvalid);
        fwd4(io().s0_wdata, wr_sel[0][0], wr_sel[0][1], wr_sel[0][2], wr_sel[0][3],
             io().m0_wdata, io().m1_wdata, io().m2_wdata, io().m3_wdata, ch_uint<DATA_WIDTH>(0_d));
        fwd4(io().s0_wid, wr_sel[0][0], wr_sel[0][1], wr_sel[0][2], wr_sel[0][3],
             io().m0_wid, io().m1_wid, io().m2_wid, io().m3_wid, ch_uint<ID_WIDTH>(0_d));
        fwd4(io().s0_wstrb, wr_sel[0][0], wr_sel[0][1], wr_sel[0][2], wr_sel[0][3],
             io().m0_wstrb, io().m1_wstrb, io().m2_wstrb, io().m3_wstrb, ch_uint<NUM_BYTES>(0_d));
        fwd4(io().s0_wlast, wr_sel[0][0], wr_sel[0][1], wr_sel[0][2], wr_sel[0][3],
             io().m0_wlast, io().m1_wlast, io().m2_wlast, io().m3_wlast, ch_bool(false));
        io().s0_wvalid = (wr_sel[0][0] & io().m0_wvalid) | (wr_sel[0][1] & io().m1_wvalid) |
                         (wr_sel[0][2] & io().m2_wvalid) | (wr_sel[0][3] & io().m3_wvalid);
        io().s0_bready = (wr_sel[0][0] & io().m0_bready) | (wr_sel[0][1] & io().m1_bready) |
                         (wr_sel[0][2] & io().m2_bready) | (wr_sel[0][3] & io().m3_bready);
        fwd4(io().s0_araddr, ar_sel[0][0], ar_sel[0][1], ar_sel[0][2], ar_sel[0][3],
             io().m0_araddr, io().m1_araddr, io().m2_araddr, io().m3_araddr, ch_uint<ADDR_WIDTH>(0_d));
        fwd4(io().s0_arid, ar_sel[0][0], ar_sel[0][1], ar_sel[0][2], ar_sel[0][3],
             io().m0_arid, io().m1_arid, io().m2_arid, io().m3_arid, ch_uint<ID_WIDTH>(0_d));
        fwd4(io().s0_arlen, ar_sel[0][0], ar_sel[0][1], ar_sel[0][2], ar_sel[0][3],
             io().m0_arlen, io().m1_arlen, io().m2_arlen, io().m3_arlen, ch_uint<4>(0_d));
        fwd4(io().s0_arsize, ar_sel[0][0], ar_sel[0][1], ar_sel[0][2], ar_sel[0][3],
             io().m0_arsize, io().m1_arsize, io().m2_arsize, io().m3_arsize, ch_uint<3>(0_d));
        fwd4(io().s0_arburst, ar_sel[0][0], ar_sel[0][1], ar_sel[0][2], ar_sel[0][3],
             io().m0_arburst, io().m1_arburst, io().m2_arburst, io().m3_arburst, ch_uint<2>(0_d));
        io().s0_arvalid = (ar_sel[0][0] & io().m0_arvalid) | (ar_sel[0][1] & io().m1_arvalid) |
                          (ar_sel[0][2] & io().m2_arvalid) | (ar_sel[0][3] & io().m3_arvalid);
        io().s0_rready = (rd_sel[0][0] & io().m0_rready) | (rd_sel[0][1] & io().m1_rready) |
                         (rd_sel[0][2] & io().m2_rready) | (rd_sel[0][3] & io().m3_rready);

        // ---- slave 1 ----
        fwd4(io().s1_awaddr, aw_sel[1][0], aw_sel[1][1], aw_sel[1][2], aw_sel[1][3],
             io().m0_awaddr, io().m1_awaddr, io().m2_awaddr, io().m3_awaddr, ch_uint<ADDR_WIDTH>(0_d));
        fwd4(io().s1_awid, aw_sel[1][0], aw_sel[1][1], aw_sel[1][2], aw_sel[1][3],
             io().m0_awid, io().m1_awid, io().m2_awid, io().m3_awid, ch_uint<ID_WIDTH>(0_d));
        fwd4(io().s1_awlen, aw_sel[1][0], aw_sel[1][1], aw_sel[1][2], aw_sel[1][3],
             io().m0_awlen, io().m1_awlen, io().m2_awlen, io().m3_awlen, ch_uint<4>(0_d));
        fwd4(io().s1_awsize, aw_sel[1][0], aw_sel[1][1], aw_sel[1][2], aw_sel[1][3],
             io().m0_awsize, io().m1_awsize, io().m2_awsize, io().m3_awsize, ch_uint<3>(0_d));
        fwd4(io().s1_awburst, aw_sel[1][0], aw_sel[1][1], aw_sel[1][2], aw_sel[1][3],
             io().m0_awburst, io().m1_awburst, io().m2_awburst, io().m3_awburst, ch_uint<2>(0_d));
        io().s1_awvalid = (aw_sel[1][0] & io().m0_awvalid) | (aw_sel[1][1] & io().m1_awvalid) |
                          (aw_sel[1][2] & io().m2_awvalid) | (aw_sel[1][3] & io().m3_awvalid);
        fwd4(io().s1_wdata, wr_sel[1][0], wr_sel[1][1], wr_sel[1][2], wr_sel[1][3],
             io().m0_wdata, io().m1_wdata, io().m2_wdata, io().m3_wdata, ch_uint<DATA_WIDTH>(0_d));
        fwd4(io().s1_wid, wr_sel[1][0], wr_sel[1][1], wr_sel[1][2], wr_sel[1][3],
             io().m0_wid, io().m1_wid, io().m2_wid, io().m3_wid, ch_uint<ID_WIDTH>(0_d));
        fwd4(io().s1_wstrb, wr_sel[1][0], wr_sel[1][1], wr_sel[1][2], wr_sel[1][3],
             io().m0_wstrb, io().m1_wstrb, io().m2_wstrb, io().m3_wstrb, ch_uint<NUM_BYTES>(0_d));
        fwd4(io().s1_wlast, wr_sel[1][0], wr_sel[1][1], wr_sel[1][2], wr_sel[1][3],
             io().m0_wlast, io().m1_wlast, io().m2_wlast, io().m3_wlast, ch_bool(false));
        io().s1_wvalid = (wr_sel[1][0] & io().m0_wvalid) | (wr_sel[1][1] & io().m1_wvalid) |
                         (wr_sel[1][2] & io().m2_wvalid) | (wr_sel[1][3] & io().m3_wvalid);
        io().s1_bready = (wr_sel[1][0] & io().m0_bready) | (wr_sel[1][1] & io().m1_bready) |
                         (wr_sel[1][2] & io().m2_bready) | (wr_sel[1][3] & io().m3_bready);
        fwd4(io().s1_araddr, ar_sel[1][0], ar_sel[1][1], ar_sel[1][2], ar_sel[1][3],
             io().m0_araddr, io().m1_araddr, io().m2_araddr, io().m3_araddr, ch_uint<ADDR_WIDTH>(0_d));
        fwd4(io().s1_arid, ar_sel[1][0], ar_sel[1][1], ar_sel[1][2], ar_sel[1][3],
             io().m0_arid, io().m1_arid, io().m2_arid, io().m3_arid, ch_uint<ID_WIDTH>(0_d));
        fwd4(io().s1_arlen, ar_sel[1][0], ar_sel[1][1], ar_sel[1][2], ar_sel[1][3],
             io().m0_arlen, io().m1_arlen, io().m2_arlen, io().m3_arlen, ch_uint<4>(0_d));
        fwd4(io().s1_arsize, ar_sel[1][0], ar_sel[1][1], ar_sel[1][2], ar_sel[1][3],
             io().m0_arsize, io().m1_arsize, io().m2_arsize, io().m3_arsize, ch_uint<3>(0_d));
        fwd4(io().s1_arburst, ar_sel[1][0], ar_sel[1][1], ar_sel[1][2], ar_sel[1][3],
             io().m0_arburst, io().m1_arburst, io().m2_arburst, io().m3_arburst, ch_uint<2>(0_d));
        io().s1_arvalid = (ar_sel[1][0] & io().m0_arvalid) | (ar_sel[1][1] & io().m1_arvalid) |
                          (ar_sel[1][2] & io().m2_arvalid) | (ar_sel[1][3] & io().m3_arvalid);
        io().s1_rready = (rd_sel[1][0] & io().m0_rready) | (rd_sel[1][1] & io().m1_rready) |
                         (rd_sel[1][2] & io().m2_rready) | (rd_sel[1][3] & io().m3_rready);

        // ---- slave 2 ----
        fwd4(io().s2_awaddr, aw_sel[2][0], aw_sel[2][1], aw_sel[2][2], aw_sel[2][3],
             io().m0_awaddr, io().m1_awaddr, io().m2_awaddr, io().m3_awaddr, ch_uint<ADDR_WIDTH>(0_d));
        fwd4(io().s2_awid, aw_sel[2][0], aw_sel[2][1], aw_sel[2][2], aw_sel[2][3],
             io().m0_awid, io().m1_awid, io().m2_awid, io().m3_awid, ch_uint<ID_WIDTH>(0_d));
        fwd4(io().s2_awlen, aw_sel[2][0], aw_sel[2][1], aw_sel[2][2], aw_sel[2][3],
             io().m0_awlen, io().m1_awlen, io().m2_awlen, io().m3_awlen, ch_uint<4>(0_d));
        fwd4(io().s2_awsize, aw_sel[2][0], aw_sel[2][1], aw_sel[2][2], aw_sel[2][3],
             io().m0_awsize, io().m1_awsize, io().m2_awsize, io().m3_awsize, ch_uint<3>(0_d));
        fwd4(io().s2_awburst, aw_sel[2][0], aw_sel[2][1], aw_sel[2][2], aw_sel[2][3],
             io().m0_awburst, io().m1_awburst, io().m2_awburst, io().m3_awburst, ch_uint<2>(0_d));
        io().s2_awvalid = (aw_sel[2][0] & io().m0_awvalid) | (aw_sel[2][1] & io().m1_awvalid) |
                          (aw_sel[2][2] & io().m2_awvalid) | (aw_sel[2][3] & io().m3_awvalid);
        fwd4(io().s2_wdata, wr_sel[2][0], wr_sel[2][1], wr_sel[2][2], wr_sel[2][3],
             io().m0_wdata, io().m1_wdata, io().m2_wdata, io().m3_wdata, ch_uint<DATA_WIDTH>(0_d));
        fwd4(io().s2_wid, wr_sel[2][0], wr_sel[2][1], wr_sel[2][2], wr_sel[2][3],
             io().m0_wid, io().m1_wid, io().m2_wid, io().m3_wid, ch_uint<ID_WIDTH>(0_d));
        fwd4(io().s2_wstrb, wr_sel[2][0], wr_sel[2][1], wr_sel[2][2], wr_sel[2][3],
             io().m0_wstrb, io().m1_wstrb, io().m2_wstrb, io().m3_wstrb, ch_uint<NUM_BYTES>(0_d));
        fwd4(io().s2_wlast, wr_sel[2][0], wr_sel[2][1], wr_sel[2][2], wr_sel[2][3],
             io().m0_wlast, io().m1_wlast, io().m2_wlast, io().m3_wlast, ch_bool(false));
        io().s2_wvalid = (wr_sel[2][0] & io().m0_wvalid) | (wr_sel[2][1] & io().m1_wvalid) |
                         (wr_sel[2][2] & io().m2_wvalid) | (wr_sel[2][3] & io().m3_wvalid);
        io().s2_bready = (wr_sel[2][0] & io().m0_bready) | (wr_sel[2][1] & io().m1_bready) |
                         (wr_sel[2][2] & io().m2_bready) | (wr_sel[2][3] & io().m3_bready);
        fwd4(io().s2_araddr, ar_sel[2][0], ar_sel[2][1], ar_sel[2][2], ar_sel[2][3],
             io().m0_araddr, io().m1_araddr, io().m2_araddr, io().m3_araddr, ch_uint<ADDR_WIDTH>(0_d));
        fwd4(io().s2_arid, ar_sel[2][0], ar_sel[2][1], ar_sel[2][2], ar_sel[2][3],
             io().m0_arid, io().m1_arid, io().m2_arid, io().m3_arid, ch_uint<ID_WIDTH>(0_d));
        fwd4(io().s2_arlen, ar_sel[2][0], ar_sel[2][1], ar_sel[2][2], ar_sel[2][3],
             io().m0_arlen, io().m1_arlen, io().m2_arlen, io().m3_arlen, ch_uint<4>(0_d));
        fwd4(io().s2_arsize, ar_sel[2][0], ar_sel[2][1], ar_sel[2][2], ar_sel[2][3],
             io().m0_arsize, io().m1_arsize, io().m2_arsize, io().m3_arsize, ch_uint<3>(0_d));
        fwd4(io().s2_arburst, ar_sel[2][0], ar_sel[2][1], ar_sel[2][2], ar_sel[2][3],
             io().m0_arburst, io().m1_arburst, io().m2_arburst, io().m3_arburst, ch_uint<2>(0_d));
        io().s2_arvalid = (ar_sel[2][0] & io().m0_arvalid) | (ar_sel[2][1] & io().m1_arvalid) |
                          (ar_sel[2][2] & io().m2_arvalid) | (ar_sel[2][3] & io().m3_arvalid);
        io().s2_rready = (rd_sel[2][0] & io().m0_rready) | (rd_sel[2][1] & io().m1_rready) |
                         (rd_sel[2][2] & io().m2_rready) | (rd_sel[2][3] & io().m3_rready);

        // ---- slave 3 ----
        fwd4(io().s3_awaddr, aw_sel[3][0], aw_sel[3][1], aw_sel[3][2], aw_sel[3][3],
             io().m0_awaddr, io().m1_awaddr, io().m2_awaddr, io().m3_awaddr, ch_uint<ADDR_WIDTH>(0_d));
        fwd4(io().s3_awid, aw_sel[3][0], aw_sel[3][1], aw_sel[3][2], aw_sel[3][3],
             io().m0_awid, io().m1_awid, io().m2_awid, io().m3_awid, ch_uint<ID_WIDTH>(0_d));
        fwd4(io().s3_awlen, aw_sel[3][0], aw_sel[3][1], aw_sel[3][2], aw_sel[3][3],
             io().m0_awlen, io().m1_awlen, io().m2_awlen, io().m3_awlen, ch_uint<4>(0_d));
        fwd4(io().s3_awsize, aw_sel[3][0], aw_sel[3][1], aw_sel[3][2], aw_sel[3][3],
             io().m0_awsize, io().m1_awsize, io().m2_awsize, io().m3_awsize, ch_uint<3>(0_d));
        fwd4(io().s3_awburst, aw_sel[3][0], aw_sel[3][1], aw_sel[3][2], aw_sel[3][3],
             io().m0_awburst, io().m1_awburst, io().m2_awburst, io().m3_awburst, ch_uint<2>(0_d));
        io().s3_awvalid = (aw_sel[3][0] & io().m0_awvalid) | (aw_sel[3][1] & io().m1_awvalid) |
                          (aw_sel[3][2] & io().m2_awvalid) | (aw_sel[3][3] & io().m3_awvalid);
        fwd4(io().s3_wdata, wr_sel[3][0], wr_sel[3][1], wr_sel[3][2], wr_sel[3][3],
             io().m0_wdata, io().m1_wdata, io().m2_wdata, io().m3_wdata, ch_uint<DATA_WIDTH>(0_d));
        fwd4(io().s3_wid, wr_sel[3][0], wr_sel[3][1], wr_sel[3][2], wr_sel[3][3],
             io().m0_wid, io().m1_wid, io().m2_wid, io().m3_wid, ch_uint<ID_WIDTH>(0_d));
        fwd4(io().s3_wstrb, wr_sel[3][0], wr_sel[3][1], wr_sel[3][2], wr_sel[3][3],
             io().m0_wstrb, io().m1_wstrb, io().m2_wstrb, io().m3_wstrb, ch_uint<NUM_BYTES>(0_d));
        fwd4(io().s3_wlast, wr_sel[3][0], wr_sel[3][1], wr_sel[3][2], wr_sel[3][3],
             io().m0_wlast, io().m1_wlast, io().m2_wlast, io().m3_wlast, ch_bool(false));
        io().s3_wvalid = (wr_sel[3][0] & io().m0_wvalid) | (wr_sel[3][1] & io().m1_wvalid) |
                         (wr_sel[3][2] & io().m2_wvalid) | (wr_sel[3][3] & io().m3_wvalid);
        io().s3_bready = (wr_sel[3][0] & io().m0_bready) | (wr_sel[3][1] & io().m1_bready) |
                         (wr_sel[3][2] & io().m2_bready) | (wr_sel[3][3] & io().m3_bready);
        fwd4(io().s3_araddr, ar_sel[3][0], ar_sel[3][1], ar_sel[3][2], ar_sel[3][3],
             io().m0_araddr, io().m1_araddr, io().m2_araddr, io().m3_araddr, ch_uint<ADDR_WIDTH>(0_d));
        fwd4(io().s3_arid, ar_sel[3][0], ar_sel[3][1], ar_sel[3][2], ar_sel[3][3],
             io().m0_arid, io().m1_arid, io().m2_arid, io().m3_arid, ch_uint<ID_WIDTH>(0_d));
        fwd4(io().s3_arlen, ar_sel[3][0], ar_sel[3][1], ar_sel[3][2], ar_sel[3][3],
             io().m0_arlen, io().m1_arlen, io().m2_arlen, io().m3_arlen, ch_uint<4>(0_d));
        fwd4(io().s3_arsize, ar_sel[3][0], ar_sel[3][1], ar_sel[3][2], ar_sel[3][3],
             io().m0_arsize, io().m1_arsize, io().m2_arsize, io().m3_arsize, ch_uint<3>(0_d));
        fwd4(io().s3_arburst, ar_sel[3][0], ar_sel[3][1], ar_sel[3][2], ar_sel[3][3],
             io().m0_arburst, io().m1_arburst, io().m2_arburst, io().m3_arburst, ch_uint<2>(0_d));
        io().s3_arvalid = (ar_sel[3][0] & io().m0_arvalid) | (ar_sel[3][1] & io().m1_arvalid) |
                          (ar_sel[3][2] & io().m2_arvalid) | (ar_sel[3][3] & io().m3_arvalid);
        io().s3_rready = (rd_sel[3][0] & io().m0_rready) | (rd_sel[3][1] & io().m1_rready) |
                         (rd_sel[3][2] & io().m2_rready) | (rd_sel[3][3] & io().m3_rready);

        // ================= 6. 握手决策（当拍地址 + 当拍 grant + 在途门控）=================
        // 在途期间 MUST 为 0 —— 否则 master 认为 AW/AR 被接受而互连悄悄吞掉（AW 静默丢失）
        ch_bool m0_awready = (!s.wr_active_m0) & select(aw_grant_m0,
                                 select(aw_mapped_m0,
                                        idx4(aw_target_m0, io().s0_awready, io().s1_awready,
                                             io().s2_awready, io().s3_awready),
                                        err_w_can_accept),
                                 ch_bool(false));
        ch_bool m1_awready = (!s.wr_active_m1) & select(aw_grant_m1,
                                 select(aw_mapped_m1,
                                        idx4(aw_target_m1, io().s0_awready, io().s1_awready,
                                             io().s2_awready, io().s3_awready),
                                        err_w_can_accept),
                                 ch_bool(false));
        ch_bool m2_awready = (!s.wr_active_m2) & select(aw_grant_m2,
                                 select(aw_mapped_m2,
                                        idx4(aw_target_m2, io().s0_awready, io().s1_awready,
                                             io().s2_awready, io().s3_awready),
                                        err_w_can_accept),
                                 ch_bool(false));
        ch_bool m3_awready = (!s.wr_active_m3) & select(aw_grant_m3,
                                 select(aw_mapped_m3,
                                        idx4(aw_target_m3, io().s0_awready, io().s1_awready,
                                             io().s2_awready, io().s3_awready),
                                        err_w_can_accept),
                                 ch_bool(false));
        io().m0_awready = m0_awready;
        io().m1_awready = m1_awready;
        io().m2_awready = m2_awready;
        io().m3_awready = m3_awready;

        ch_bool m0_wready = s.wr_active_m0 & select(s.wr_mapped_m0,
                                 idx4(s.wr_route_m0, io().s0_wready, io().s1_wready,
                                      io().s2_wready, io().s3_wready),
                                 !s.err_w_valid);
        ch_bool m1_wready = s.wr_active_m1 & select(s.wr_mapped_m1,
                                 idx4(s.wr_route_m1, io().s0_wready, io().s1_wready,
                                      io().s2_wready, io().s3_wready),
                                 !s.err_w_valid);
        ch_bool m2_wready = s.wr_active_m2 & select(s.wr_mapped_m2,
                                 idx4(s.wr_route_m2, io().s0_wready, io().s1_wready,
                                      io().s2_wready, io().s3_wready),
                                 !s.err_w_valid);
        ch_bool m3_wready = s.wr_active_m3 & select(s.wr_mapped_m3,
                                 idx4(s.wr_route_m3, io().s0_wready, io().s1_wready,
                                      io().s2_wready, io().s3_wready),
                                 !s.err_w_valid);
        io().m0_wready = m0_wready;
        io().m1_wready = m1_wready;
        io().m2_wready = m2_wready;
        io().m3_wready = m3_wready;

        ch_bool m0_arready = (!s.rd_active_m0) & select(ar_grant_m0,
                                 select(ar_mapped_m0,
                                        idx4(ar_target_m0, io().s0_arready, io().s1_arready,
                                             io().s2_arready, io().s3_arready),
                                        err_r_can_accept),
                                 ch_bool(false));
        ch_bool m1_arready = (!s.rd_active_m1) & select(ar_grant_m1,
                                 select(ar_mapped_m1,
                                        idx4(ar_target_m1, io().s0_arready, io().s1_arready,
                                             io().s2_arready, io().s3_arready),
                                        err_r_can_accept),
                                 ch_bool(false));
        ch_bool m2_arready = (!s.rd_active_m2) & select(ar_grant_m2,
                                 select(ar_mapped_m2,
                                        idx4(ar_target_m2, io().s0_arready, io().s1_arready,
                                             io().s2_arready, io().s3_arready),
                                        err_r_can_accept),
                                 ch_bool(false));
        ch_bool m3_arready = (!s.rd_active_m3) & select(ar_grant_m3,
                                 select(ar_mapped_m3,
                                        idx4(ar_target_m3, io().s0_arready, io().s1_arready,
                                             io().s2_arready, io().s3_arready),
                                        err_r_can_accept),
                                 ch_bool(false));
        io().m0_arready = m0_arready;
        io().m1_arready = m1_arready;
        io().m2_arready = m2_arready;
        io().m3_arready = m3_arready;

        // ================= 7. 反向通道：按**锁存路由**反查 =================
        const ch_uint<2> DECERR_V(static_cast<unsigned>(AxiResp::DECERR));

        // ---- m0 ----
        ch_bool m0_bvalid = s.wr_active_m0 & select(s.wr_mapped_m0,
                                idx4(s.wr_route_m0, io().s0_bvalid, io().s1_bvalid,
                                     io().s2_bvalid, io().s3_bvalid),
                                s.err_w_valid);
        // 未映射写回显锁存的 `awid`（DECERR 响应 MUST 回显事务 ID，
        // 同 `Axi4Slave` 的 `write_id_reg` 形态）。
        ch_uint<ID_WIDTH> m0_bid = select(s.wr_active_m0,
                                select(s.wr_mapped_m0,
                                       idx4(s.wr_route_m0, io().s0_bid, io().s1_bid,
                                            io().s2_bid, io().s3_bid),
                                       s.err_w_id),
                                ch_uint<ID_WIDTH>(0_d));
        io().m0_bvalid = m0_bvalid;
        io().m0_bid = m0_bid;
        // ⚠️ 响应载荷 MUST 也按**在途标志**门控（见 spec §「每个 slave 的响应 MUST
        //    回到发起它的 master」）—— 否则本 master 没有在途事务时，`bresp` / `bid`
        //    仍会镜像某个从设备的当前值（可能是**另一个 master 正在用**的那个）。
        io().m0_bresp = select(s.wr_active_m0,
                                select(s.wr_mapped_m0,
                                       idx4(s.wr_route_m0, io().s0_bresp, io().s1_bresp,
                                            io().s2_bresp, io().s3_bresp), DECERR_V),
                                ch_uint<2>(0_d));
        ch_bool m0_rvalid = s.rd_active_m0 & select(s.rd_mapped_m0,
                                idx4(s.rd_route_m0, io().s0_rvalid, io().s1_rvalid,
                                     io().s2_rvalid, io().s3_rvalid),
                                s.err_r_valid);
        // MUST 按**在途标志**门控 —— 否则空闲时 rlast 恒为 1
        // (旧判据 err_r_remaining == 0 在复位后即为真 => 实测空闲时 m0_rlast=1)。
        // `err_r_remaining` 是「当前拍**之后**还需被返回的 beat 数」
        // => 当前拍是末拍 <=> 剩余 == 0。
        ch_bool m0_rlast = select(s.rd_active_m0,
                                select(s.rd_mapped_m0,
                                       idx4(s.rd_route_m0, io().s0_rlast, io().s1_rlast,
                                            io().s2_rlast, io().s3_rlast),
                                       s.err_r_remaining == ch_uint<4>(0_d)),
                                ch_bool(false));
        io().m0_rvalid = m0_rvalid;
        io().m0_rlast = m0_rlast;
        io().m0_rdata = select(s.rd_active_m0,
                                select(s.rd_mapped_m0,
                                       idx4(s.rd_route_m0, io().s0_rdata, io().s1_rdata,
                                            io().s2_rdata, io().s3_rdata),
                                       ch_uint<DATA_WIDTH>(0_d)),
                                ch_uint<DATA_WIDTH>(0_d));
        // ⚠️ 同 `m*_rdata`：MUST 按在途标志门控（否则空闲时会镜像某个从设备的
        //    `rid` —— 可能正是**另一个 master 的在途读**在用的值）。
        //    未映射读回显锁存的 `arid`（DECERR 响应 MUST 回显事务 ID，
        //    同 `Axi4Slave` 的 `read_id_reg` 形态）。
        io().m0_rid = select(s.rd_active_m0,
                               select(s.rd_mapped_m0,
                                      idx4(s.rd_route_m0, io().s0_rid, io().s1_rid,
                                           io().s2_rid, io().s3_rid),
                                      s.err_r_id),
                               ch_uint<ID_WIDTH>(0_d));
        io().m0_rresp = select(s.rd_active_m0,
                                select(s.rd_mapped_m0,
                                       idx4(s.rd_route_m0, io().s0_rresp, io().s1_rresp,
                                            io().s2_rresp, io().s3_rresp), DECERR_V),
                                ch_uint<2>(0_d));

        // ---- m1 ----
        ch_bool m1_bvalid = s.wr_active_m1 & select(s.wr_mapped_m1,
                                idx4(s.wr_route_m1, io().s0_bvalid, io().s1_bvalid,
                                     io().s2_bvalid, io().s3_bvalid),
                                s.err_w_valid);
        // 未映射写回显锁存的 `awid`（DECERR 响应 MUST 回显事务 ID，
        // 同 `Axi4Slave` 的 `write_id_reg` 形态）。
        ch_uint<ID_WIDTH> m1_bid = select(s.wr_active_m1,
                                select(s.wr_mapped_m1,
                                       idx4(s.wr_route_m1, io().s0_bid, io().s1_bid,
                                            io().s2_bid, io().s3_bid),
                                       s.err_w_id),
                                ch_uint<ID_WIDTH>(0_d));
        io().m1_bvalid = m1_bvalid;
        io().m1_bid = m1_bid;
        io().m1_bresp = select(s.wr_active_m1,
                                select(s.wr_mapped_m1,
                                       idx4(s.wr_route_m1, io().s0_bresp, io().s1_bresp,
                                            io().s2_bresp, io().s3_bresp), DECERR_V),
                                ch_uint<2>(0_d));
        ch_bool m1_rvalid = s.rd_active_m1 & select(s.rd_mapped_m1,
                                idx4(s.rd_route_m1, io().s0_rvalid, io().s1_rvalid,
                                     io().s2_rvalid, io().s3_rvalid),
                                s.err_r_valid);
        // MUST 按**在途标志**门控 —— 否则空闲时 rlast 恒为 1
        // (旧判据 err_r_remaining == 0 在复位后即为真 => 实测空闲时 m0_rlast=1)。
        // `err_r_remaining` 是「当前拍**之后**还需被返回的 beat 数」
        // => 当前拍是末拍 <=> 剩余 == 0。
        ch_bool m1_rlast = select(s.rd_active_m1,
                                select(s.rd_mapped_m1,
                                       idx4(s.rd_route_m1, io().s0_rlast, io().s1_rlast,
                                            io().s2_rlast, io().s3_rlast),
                                       s.err_r_remaining == ch_uint<4>(0_d)),
                                ch_bool(false));
        io().m1_rvalid = m1_rvalid;
        io().m1_rlast = m1_rlast;
        io().m1_rdata = select(s.rd_active_m1,
                                select(s.rd_mapped_m1,
                                       idx4(s.rd_route_m1, io().s0_rdata, io().s1_rdata,
                                            io().s2_rdata, io().s3_rdata),
                                       ch_uint<DATA_WIDTH>(0_d)),
                                ch_uint<DATA_WIDTH>(0_d));
        // ⚠️ 同 `m*_rdata`：MUST 按在途标志门控（否则空闲时会镜像某个从设备的
        //    `rid` —— 可能正是**另一个 master 的在途读**在用的值）。
        //    未映射读回显锁存的 `arid`（DECERR 响应 MUST 回显事务 ID，
        //    同 `Axi4Slave` 的 `read_id_reg` 形态）。
        io().m1_rid = select(s.rd_active_m1,
                               select(s.rd_mapped_m1,
                                      idx4(s.rd_route_m1, io().s0_rid, io().s1_rid,
                                           io().s2_rid, io().s3_rid),
                                      s.err_r_id),
                               ch_uint<ID_WIDTH>(0_d));
        io().m1_rresp = select(s.rd_active_m1,
                                select(s.rd_mapped_m1,
                                       idx4(s.rd_route_m1, io().s0_rresp, io().s1_rresp,
                                            io().s2_rresp, io().s3_rresp), DECERR_V),
                                ch_uint<2>(0_d));

        // ---- m2 ----
        ch_bool m2_bvalid = s.wr_active_m2 & select(s.wr_mapped_m2,
                                idx4(s.wr_route_m2, io().s0_bvalid, io().s1_bvalid,
                                     io().s2_bvalid, io().s3_bvalid),
                                s.err_w_valid);
        // 未映射写回显锁存的 `awid`（DECERR 响应 MUST 回显事务 ID，
        // 同 `Axi4Slave` 的 `write_id_reg` 形态）。
        ch_uint<ID_WIDTH> m2_bid = select(s.wr_active_m2,
                                select(s.wr_mapped_m2,
                                       idx4(s.wr_route_m2, io().s0_bid, io().s1_bid,
                                            io().s2_bid, io().s3_bid),
                                       s.err_w_id),
                                ch_uint<ID_WIDTH>(0_d));
        io().m2_bvalid = m2_bvalid;
        io().m2_bid = m2_bid;
        io().m2_bresp = select(s.wr_active_m2,
                                select(s.wr_mapped_m2,
                                       idx4(s.wr_route_m2, io().s0_bresp, io().s1_bresp,
                                            io().s2_bresp, io().s3_bresp), DECERR_V),
                                ch_uint<2>(0_d));
        ch_bool m2_rvalid = s.rd_active_m2 & select(s.rd_mapped_m2,
                                idx4(s.rd_route_m2, io().s0_rvalid, io().s1_rvalid,
                                     io().s2_rvalid, io().s3_rvalid),
                                s.err_r_valid);
        // MUST 按**在途标志**门控 —— 否则空闲时 rlast 恒为 1
        // (旧判据 err_r_remaining == 0 在复位后即为真 => 实测空闲时 m0_rlast=1)。
        // `err_r_remaining` 是「当前拍**之后**还需被返回的 beat 数」
        // => 当前拍是末拍 <=> 剩余 == 0。
        ch_bool m2_rlast = select(s.rd_active_m2,
                                select(s.rd_mapped_m2,
                                       idx4(s.rd_route_m2, io().s0_rlast, io().s1_rlast,
                                            io().s2_rlast, io().s3_rlast),
                                       s.err_r_remaining == ch_uint<4>(0_d)),
                                ch_bool(false));
        io().m2_rvalid = m2_rvalid;
        io().m2_rlast = m2_rlast;
        io().m2_rdata = select(s.rd_active_m2,
                                select(s.rd_mapped_m2,
                                       idx4(s.rd_route_m2, io().s0_rdata, io().s1_rdata,
                                            io().s2_rdata, io().s3_rdata),
                                       ch_uint<DATA_WIDTH>(0_d)),
                                ch_uint<DATA_WIDTH>(0_d));
        // ⚠️ 同 `m*_rdata`：MUST 按在途标志门控（否则空闲时会镜像某个从设备的
        //    `rid` —— 可能正是**另一个 master 的在途读**在用的值）。
        //    未映射读回显锁存的 `arid`（DECERR 响应 MUST 回显事务 ID，
        //    同 `Axi4Slave` 的 `read_id_reg` 形态）。
        io().m2_rid = select(s.rd_active_m2,
                               select(s.rd_mapped_m2,
                                      idx4(s.rd_route_m2, io().s0_rid, io().s1_rid,
                                           io().s2_rid, io().s3_rid),
                                      s.err_r_id),
                               ch_uint<ID_WIDTH>(0_d));
        io().m2_rresp = select(s.rd_active_m2,
                                select(s.rd_mapped_m2,
                                       idx4(s.rd_route_m2, io().s0_rresp, io().s1_rresp,
                                            io().s2_rresp, io().s3_rresp), DECERR_V),
                                ch_uint<2>(0_d));

        // ---- m3 ----
        ch_bool m3_bvalid = s.wr_active_m3 & select(s.wr_mapped_m3,
                                idx4(s.wr_route_m3, io().s0_bvalid, io().s1_bvalid,
                                     io().s2_bvalid, io().s3_bvalid),
                                s.err_w_valid);
        // 未映射写回显锁存的 `awid`（DECERR 响应 MUST 回显事务 ID，
        // 同 `Axi4Slave` 的 `write_id_reg` 形态）。
        ch_uint<ID_WIDTH> m3_bid = select(s.wr_active_m3,
                                select(s.wr_mapped_m3,
                                       idx4(s.wr_route_m3, io().s0_bid, io().s1_bid,
                                            io().s2_bid, io().s3_bid),
                                       s.err_w_id),
                                ch_uint<ID_WIDTH>(0_d));
        io().m3_bvalid = m3_bvalid;
        io().m3_bid = m3_bid;
        io().m3_bresp = select(s.wr_active_m3,
                                select(s.wr_mapped_m3,
                                       idx4(s.wr_route_m3, io().s0_bresp, io().s1_bresp,
                                            io().s2_bresp, io().s3_bresp), DECERR_V),
                                ch_uint<2>(0_d));
        ch_bool m3_rvalid = s.rd_active_m3 & select(s.rd_mapped_m3,
                                idx4(s.rd_route_m3, io().s0_rvalid, io().s1_rvalid,
                                     io().s2_rvalid, io().s3_rvalid),
                                s.err_r_valid);
        // MUST 按**在途标志**门控 —— 否则空闲时 rlast 恒为 1
        // (旧判据 err_r_remaining == 0 在复位后即为真 => 实测空闲时 m0_rlast=1)。
        // `err_r_remaining` 是「当前拍**之后**还需被返回的 beat 数」
        // => 当前拍是末拍 <=> 剩余 == 0。
        ch_bool m3_rlast = select(s.rd_active_m3,
                                select(s.rd_mapped_m3,
                                       idx4(s.rd_route_m3, io().s0_rlast, io().s1_rlast,
                                            io().s2_rlast, io().s3_rlast),
                                       s.err_r_remaining == ch_uint<4>(0_d)),
                                ch_bool(false));
        io().m3_rvalid = m3_rvalid;
        io().m3_rlast = m3_rlast;
        io().m3_rdata = select(s.rd_active_m3,
                                select(s.rd_mapped_m3,
                                       idx4(s.rd_route_m3, io().s0_rdata, io().s1_rdata,
                                            io().s2_rdata, io().s3_rdata),
                                       ch_uint<DATA_WIDTH>(0_d)),
                                ch_uint<DATA_WIDTH>(0_d));
        // ⚠️ 同 `m*_rdata`：MUST 按在途标志门控（否则空闲时会镜像某个从设备的
        //    `rid` —— 可能正是**另一个 master 的在途读**在用的值）。
        //    未映射读回显锁存的 `arid`（DECERR 响应 MUST 回显事务 ID，
        //    同 `Axi4Slave` 的 `read_id_reg` 形态）。
        io().m3_rid = select(s.rd_active_m3,
                               select(s.rd_mapped_m3,
                                      idx4(s.rd_route_m3, io().s0_rid, io().s1_rid,
                                           io().s2_rid, io().s3_rid),
                                      s.err_r_id),
                               ch_uint<ID_WIDTH>(0_d));
        io().m3_rresp = select(s.rd_active_m3,
                                select(s.rd_mapped_m3,
                                       idx4(s.rd_route_m3, io().s0_rresp, io().s1_rresp,
                                            io().s2_rresp, io().s3_rresp), DECERR_V),
                                ch_uint<2>(0_d));

        // ================= 8. 握手脉冲 =================
        ch_bool aw_hs_m0 = io().m0_awvalid & m0_awready;
        ch_bool aw_hs_m1 = io().m1_awvalid & m1_awready;
        ch_bool aw_hs_m2 = io().m2_awvalid & m2_awready;
        ch_bool aw_hs_m3 = io().m3_awvalid & m3_awready;
        ch_bool ar_hs_m0 = io().m0_arvalid & m0_arready;
        ch_bool ar_hs_m1 = io().m1_arvalid & m1_arready;
        ch_bool ar_hs_m2 = io().m2_arvalid & m2_arready;
        ch_bool ar_hs_m3 = io().m3_arvalid & m3_arready;
        ch_bool b_hs_m0 = m0_bvalid & io().m0_bready;
        ch_bool b_hs_m1 = m1_bvalid & io().m1_bready;
        ch_bool b_hs_m2 = m2_bvalid & io().m2_bready;
        ch_bool b_hs_m3 = m3_bvalid & io().m3_bready;
        ch_bool r_hs_m0 = m0_rvalid & io().m0_rready;
        ch_bool r_hs_m1 = m1_rvalid & io().m1_rready;
        ch_bool r_hs_m2 = m2_rvalid & io().m2_rready;
        ch_bool r_hs_m3 = m3_rvalid & io().m3_rready;

        // ================= 9. 写侧状态：路由锁存 + 在途标志 + 仲裁指针 =================
        s.wr_route_m0->next = select(!io().rst_n, ch_uint<2>(0_d),
                              select(aw_hs_m0, aw_target_m0, s.wr_route_m0));
        s.wr_route_m1->next = select(!io().rst_n, ch_uint<2>(0_d),
                              select(aw_hs_m1, aw_target_m1, s.wr_route_m1));
        s.wr_route_m2->next = select(!io().rst_n, ch_uint<2>(0_d),
                              select(aw_hs_m2, aw_target_m2, s.wr_route_m2));
        s.wr_route_m3->next = select(!io().rst_n, ch_uint<2>(0_d),
                              select(aw_hs_m3, aw_target_m3, s.wr_route_m3));
        s.wr_mapped_m0->next = select(!io().rst_n, ch_bool(false),
                               select(aw_hs_m0, aw_mapped_m0, s.wr_mapped_m0));
        s.wr_mapped_m1->next = select(!io().rst_n, ch_bool(false),
                               select(aw_hs_m1, aw_mapped_m1, s.wr_mapped_m1));
        s.wr_mapped_m2->next = select(!io().rst_n, ch_bool(false),
                               select(aw_hs_m2, aw_mapped_m2, s.wr_mapped_m2));
        s.wr_mapped_m3->next = select(!io().rst_n, ch_bool(false),
                               select(aw_hs_m3, aw_mapped_m3, s.wr_mapped_m3));
        s.wr_active_m0->next = select(!io().rst_n, ch_bool(false),
                               select(aw_hs_m0, ch_bool(true),
                               select(b_hs_m0, ch_bool(false), s.wr_active_m0)));
        s.wr_active_m1->next = select(!io().rst_n, ch_bool(false),
                               select(aw_hs_m1, ch_bool(true),
                               select(b_hs_m1, ch_bool(false), s.wr_active_m1)));
        s.wr_active_m2->next = select(!io().rst_n, ch_bool(false),
                               select(aw_hs_m2, ch_bool(true),
                               select(b_hs_m2, ch_bool(false), s.wr_active_m2)));
        s.wr_active_m3->next = select(!io().rst_n, ch_bool(false),
                               select(aw_hs_m3, ch_bool(true),
                               select(b_hs_m3, ch_bool(false), s.wr_active_m3)));

        // 轮转条件 MUST 是**该方向事务真正完成**（NOT 任一 slave 的 ready/bvalid）
        // 注：`b_hs_mX` 已蕴含 `wr_active_mX`（`mX_bvalid` 本身就被它门控）⇒
        // 不必重复相与（与 `rd_done` 同构）。
        ch_bool wr_done = b_hs_m0 | b_hs_m1 | b_hs_m2 | b_hs_m3;
        s.aw_ptr->next = select(!io().rst_n, ch_uint<2>(0_d),
                         select(wr_done,
                                (s.aw_ptr + ch_uint<2>(1_d)) & ch_uint<2>(3_d),
                                s.aw_ptr));

        // ================= 10. 读侧状态 =================
        s.rd_route_m0->next = select(!io().rst_n, ch_uint<2>(0_d),
                              select(ar_hs_m0, ar_target_m0, s.rd_route_m0));
        s.rd_route_m1->next = select(!io().rst_n, ch_uint<2>(0_d),
                              select(ar_hs_m1, ar_target_m1, s.rd_route_m1));
        s.rd_route_m2->next = select(!io().rst_n, ch_uint<2>(0_d),
                              select(ar_hs_m2, ar_target_m2, s.rd_route_m2));
        s.rd_route_m3->next = select(!io().rst_n, ch_uint<2>(0_d),
                              select(ar_hs_m3, ar_target_m3, s.rd_route_m3));
        s.rd_mapped_m0->next = select(!io().rst_n, ch_bool(false),
                               select(ar_hs_m0, ar_mapped_m0, s.rd_mapped_m0));
        s.rd_mapped_m1->next = select(!io().rst_n, ch_bool(false),
                               select(ar_hs_m1, ar_mapped_m1, s.rd_mapped_m1));
        s.rd_mapped_m2->next = select(!io().rst_n, ch_bool(false),
                               select(ar_hs_m2, ar_mapped_m2, s.rd_mapped_m2));
        s.rd_mapped_m3->next = select(!io().rst_n, ch_bool(false),
                               select(ar_hs_m3, ar_mapped_m3, s.rd_mapped_m3));
        // 读事务只在**末拍被接受**时才算完成（burst 中途的 r_hs 不清 active）
        s.rd_active_m0->next = select(!io().rst_n, ch_bool(false),
                               select(ar_hs_m0, ch_bool(true),
                               select(r_hs_m0 & m0_rlast, ch_bool(false), s.rd_active_m0)));
        s.rd_active_m1->next = select(!io().rst_n, ch_bool(false),
                               select(ar_hs_m1, ch_bool(true),
                               select(r_hs_m1 & m1_rlast, ch_bool(false), s.rd_active_m1)));
        s.rd_active_m2->next = select(!io().rst_n, ch_bool(false),
                               select(ar_hs_m2, ch_bool(true),
                               select(r_hs_m2 & m2_rlast, ch_bool(false), s.rd_active_m2)));
        s.rd_active_m3->next = select(!io().rst_n, ch_bool(false),
                               select(ar_hs_m3, ch_bool(true),
                               select(r_hs_m3 & m3_rlast, ch_bool(false), s.rd_active_m3)));

        ch_bool rd_done = (r_hs_m0 & m0_rlast) | (r_hs_m1 & m1_rlast) |
                          (r_hs_m2 & m2_rlast) | (r_hs_m3 & m3_rlast);
        s.ar_ptr->next = select(!io().rst_n, ch_uint<2>(0_d),
                         select(rd_done,
                                (s.ar_ptr + ch_uint<2>(1_d)) & ch_uint<2>(3_d),
                                s.ar_ptr));

        // ================= 11. error responder（写）=================
        // 未映射写：互连层自己接受 AW/W 并返回 DECERR；`bvalid` MUST 是**寄存器**
        // 且保持到 `bready`（组合单拍脉冲会让合规 master 永久死锁）
        // MUST 逐 beat 收完才能应答 (critic BLOCKER #1)：
        // 一笔写的 beat 数 = awlen + 1。旧实现在**第一个** W beat 握手时就置位
        // err_w_valid => (1) m*_wready 随之恒 0 => 后续 beat 永不被接受
        // => master 持 wvalid 永久挂死；(2) B 响应在 wlast 之前发出，违反 AXI。
        // => `err_w_remaining` 记「当前拍**之后**还需被接受的 beat 数」，逐拍递减
        //    （递减带 `!= 0` 守卫 => 末拍不会下溢回绕）；
        //    `err_w_valid` (= BVALID) 只在最后一拍被接受后才置位。
        // 宽度 = 4 bit：`awlen` 最大 15，而初值就是 `awlen`（不是 `awlen + 1`）
        // => 4 bit 恰好装下 0..15。⚠️ MUST NOT 改成 5 bit ——
        //    `ch_uint<5>` 无法从 `ch_in<ch_uint<4>>` 端口构造（无隐式转换，实测编译失败）。
        ch_bool err_aw_sel_m0 = aw_hs_m0 & (!aw_mapped_m0);
        ch_bool err_aw_sel_m1 = aw_hs_m1 & (!aw_mapped_m1);
        ch_bool err_aw_sel_m2 = aw_hs_m2 & (!aw_mapped_m2);
        ch_bool err_aw_sel_m3 = aw_hs_m3 & (!aw_mapped_m3);
        ch_bool err_aw_hs = err_aw_sel_m0 | err_aw_sel_m1 | err_aw_sel_m2 | err_aw_sel_m3;
        // 锁存事务 ID (DECERR 响应 MUST 回显，同 Axi4Slave 的 write_id_reg 形态)
        ch_uint<ID_WIDTH> err_aw_id =
            select(err_aw_sel_m0, io().m0_awid,
            select(err_aw_sel_m1, io().m1_awid,
            select(err_aw_sel_m2, io().m2_awid, io().m3_awid)));
        // 本笔写在「当前拍之后」还需被接受的 beat 数 = awlen
        ch_uint<4> err_aw_len = select(err_aw_sel_m0, io().m0_awlen,
                                select(err_aw_sel_m1, io().m1_awlen,
                                select(err_aw_sel_m2, io().m2_awlen, io().m3_awlen)));
        ch_bool err_w_hs = (io().m0_wvalid & m0_wready & (!s.wr_mapped_m0)) |
                           (io().m1_wvalid & m1_wready & (!s.wr_mapped_m1)) |
                           (io().m2_wvalid & m2_wready & (!s.wr_mapped_m2)) |
                           (io().m3_wvalid & m3_wready & (!s.wr_mapped_m3));
        // 末拍判定：剩余 beat 数 == 1 <=> 当前这一拍是最后一拍
        ch_bool err_w_last_hs = err_w_hs & (s.err_w_remaining == ch_uint<4>(0_d));
        ch_bool err_b_hs = s.err_w_valid &
                           ((s.wr_active_m0 & (!s.wr_mapped_m0) & io().m0_bready) |
                            (s.wr_active_m1 & (!s.wr_mapped_m1) & io().m1_bready) |
                            (s.wr_active_m2 & (!s.wr_mapped_m2) & io().m2_bready) |
                            (s.wr_active_m3 & (!s.wr_mapped_m3) & io().m3_bready));
        s.err_w_busy->next = select(!io().rst_n, ch_bool(false),
                              select(err_aw_hs, ch_bool(true),
                              select(err_b_hs, ch_bool(false), s.err_w_busy)));
        s.err_w_remaining->next = select(!io().rst_n, ch_uint<4>(0_d),
                              select(err_aw_hs, err_aw_len,
                              select(err_w_hs & (s.err_w_remaining != ch_uint<4>(0_d)),
                                     s.err_w_remaining - ch_uint<4>(1_d),
                                     s.err_w_remaining)));
        s.err_w_id->next = select(!io().rst_n, ch_uint<ID_WIDTH>(0_d),
                              select(err_aw_hs, err_aw_id, s.err_w_id));
        s.err_w_valid->next = select(!io().rst_n, ch_bool(false),
                               select(err_w_last_hs, ch_bool(true),
                               select(err_b_hs, ch_bool(false), s.err_w_valid)));

        // ================= 12. error responder（读）=================
        // 返回 `arlen + 1` 个 beat，每拍 rresp=DECERR，**末拍** rlast=1
        // （只回 1 拍会让 master 等不到 rlast 而挂死）
        ch_bool err_ar_sel_m0 = ar_hs_m0 & (!ar_mapped_m0);
        ch_bool err_ar_sel_m1 = ar_hs_m1 & (!ar_mapped_m1);
        ch_bool err_ar_sel_m2 = ar_hs_m2 & (!ar_mapped_m2);
        ch_bool err_ar_sel_m3 = ar_hs_m3 & (!ar_mapped_m3);
        ch_bool err_ar_hs = err_ar_sel_m0 | err_ar_sel_m1 | err_ar_sel_m2 | err_ar_sel_m3;
        // 锁存事务 ID (DECERR 响应 MUST 回显，同 Axi4Slave 的 read_id_reg 形态)
        ch_uint<ID_WIDTH> err_ar_id =
            select(err_ar_sel_m0, io().m0_arid,
            select(err_ar_sel_m1, io().m1_arid,
            select(err_ar_sel_m2, io().m2_arid, io().m3_arid)));
        // 本笔读在「当前拍之后」还需被返回的 beat 数 = arlen
        ch_uint<4> err_ar_len = select(err_ar_sel_m0, io().m0_arlen,
                                select(err_ar_sel_m1, io().m1_arlen,
                                select(err_ar_sel_m2, io().m2_arlen, io().m3_arlen)));
        ch_bool err_r_beat_hs = s.err_r_valid &
                                ((s.rd_active_m0 & (!s.rd_mapped_m0) & io().m0_rready) |
                                 (s.rd_active_m1 & (!s.rd_mapped_m1) & io().m1_rready) |
                                 (s.rd_active_m2 & (!s.rd_mapped_m2) & io().m2_rready) |
                                 (s.rd_active_m3 & (!s.rd_mapped_m3) & io().m3_rready));
        // `err_r_remaining` 与写侧同构：「当前拍**之后**还需被返回的 beat 数」
        // （初值 = `arlen`）=> 末拍 <=> 剩余 == 0，递减带 `!= 0` 守卫。
        // ⚠️ 旧实现用 `arlen` 作初值却以「剩余 == 0 即末拍」+ 无守卫递减
        //    => 把 `arlen + 1` 个 beat 少算一拍，且末拍时下溢回绕。
        ch_bool err_r_last_hs = err_r_beat_hs & (s.err_r_remaining == ch_uint<4>(0_d));
        s.err_r_remaining->next = select(!io().rst_n, ch_uint<4>(0_d),
                                   select(err_ar_hs, err_ar_len,
                                   select(err_r_beat_hs & (s.err_r_remaining != ch_uint<4>(0_d)),
                                          s.err_r_remaining - ch_uint<4>(1_d),
                                          s.err_r_remaining)));
        s.err_r_id->next = select(!io().rst_n, ch_uint<ID_WIDTH>(0_d),
                              select(err_ar_hs, err_ar_id, s.err_r_id));
        s.err_r_valid->next = select(!io().rst_n, ch_bool(false),
                               select(err_ar_hs, ch_bool(true),
                               select(err_r_last_hs, ch_bool(false), s.err_r_valid)));
    }
};

} // namespace axi4
