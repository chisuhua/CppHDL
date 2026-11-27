/**
 * @file axi_lite_crossbar.h
 * @brief AXI4-Lite 2×6 互联（**数据面由 `chlib` stream 组合子实现**）
 *
 * 契约：`openspec/changes/rv32i-soc-peripherals-interconnect-2026-11/
 *        specs/axi-interconnect-routing/spec.md`（ADDED ×5）
 *
 * 端口面固定为 **2 个 master × 6 个 slave**（S4b 的 SoC 形态）：
 *   * 每个面 19 根 AXI4-Lite 通道（无 ID / burst / last）
 *   * 地址是**绝对地址**（MUST NOT 重定基）
 *
 * ── 实现路线（用户 2026-11-27 约束：尽量复用 `chlib`）─────────────────
 * AXI4-Lite 的 5 个通道与 `ch_stream<T>` **逐条对应** ⇒ ⇒ 数据面只用 stream：
 *
 * | 通道 | 平面 | 组合子 |
 * |---|---|---|
 * | AW / AR | 选主：`stream_mux<2>`（grant 选主）→ 选从：`stream_demux<7>`（地址选从） |
 * | W      | 每 master `stream_demux<7>`（按锁存写路由）→ 每 slave `stream_mux<2>`（按 owner） |
 * | B / R  | 每 slave `stream_demux<2>`（按 owner）→ 每 master `stream_mux<7>`（按锁存路由，末档 = ERR） |
 *
 * ⇒ ⇒ **内部 MUST NOT 手写 `ready = ...valid...` 的反压转发**：反压由组合子
 *     驱动（`chlib-stream-ready-ownership` 的既有契约）。
 * ⇒ ⇒ 唯一手写 `valid/ready` 的地方是**边界适配**（19 根端口 ↔ 5 条 stream）
 *     与**在途门控**（`!active` 把 master 侧的 `valid/ready` 钉掉）。
 *
 * ── 语义（复用 `axi-interconnect-routing` 的既有契约）─────────────────
 * * AW / AR 握手当拍**锁存路由**；W/B 用**写路由**、R 用**读路由**（读写独立）。
 * * 每方向独立 round-robin（`chlib::round_robin_selector`），握手后推进指针。
 * * 单 master 单 outstanding（同向）；**同一 slave 的写/读各自串行**（owner 唯一）。
 * * 未映射 / `kErr` 窗口 ⇒ ⇒ 互连层自应答 `DECERR`（`bresp/rresp = 0b11`）。
 * * 低有效 `rst_n`（时钟沿生效），全部状态寄存器带复位 pattern。
 *
 * ⚠️ **为什么 ERR 必须是 demux 的「第 M 路」**（不是「select 越界」）：
 *   `stream_demux` 的契约是「越界 ⇒ 全 `valid=0` **且** `input.ready=0`」
 *   ⇒ ⇒ 用越界表达 ERR 会让事务**永久挂死**。
 */

#pragma once

#include "axi4/interconnect_common.h"
#include "ch.hpp"
#include "chlib/stream.h"
#include "component.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace axi4 {

using namespace ch::core;

template <typename Map, unsigned ADDR_WIDTH = 32, unsigned DATA_WIDTH = 32>
class AxiLiteCrossbar : public ch::Component {
  public:
    static constexpr unsigned kNumMasters = 2;
    static constexpr unsigned kNumSlaves = 6;
    static constexpr unsigned kNumRoutes = kNumSlaves + 1;  // 末档 = ERR
    static constexpr unsigned kErrRoute = kNumSlaves;

    static constexpr unsigned kOwnerW = compute_idx_width(kNumMasters);
    static constexpr unsigned kRouteW = compute_idx_width(kNumRoutes);
    static constexpr unsigned kNumWindows = std::size(Map::windows);

    static_assert(ADDR_WIDTH > 0 && ADDR_WIDTH <= 64,
                  "ADDR_WIDTH MUST 在 1..64 —— 0 会让地址掩码恒 0（全地址命中）");
    static_assert(DATA_WIDTH == 32,
                  "本批只验证 32 位数据面（跨位宽未验证 ⇒ 不做静默泛化）");
    static_assert(kNumWindows > 0, "Map::windows MUST 非空");

    using ADDR = ch_uint<ADDR_WIDTH>;
    using DATA = ch_uint<DATA_WIDTH>;
    using STRB = ch_uint<DATA_WIDTH / 8>;
    using RESP = ch_uint<2>;

    // 打包后的 stream payload（**wide `ch_uint`** ⇒ `stream_mux` 走快路径）
    using AwPayload = ch_uint<ADDR_WIDTH + 2>;              // {prot, addr}
    using ArPayload = ch_uint<ADDR_WIDTH + 2>;              // {prot, addr}
    using WPayload = ch_uint<DATA_WIDTH + DATA_WIDTH / 8>;  // {strb, data}
    using BPayload = ch_uint<2>;                            // {resp}
    using RPayload = ch_uint<DATA_WIDTH + 2>;               // {resp, data}

    __io(
        ch_in<ch_bool> rst_n{"rst_n"};
        
        // ---- master 0 ----
        ch_in<ch_uint<ADDR_WIDTH>> m0_awaddr{"m0_awaddr"};
        ch_in<ch_uint<2>> m0_awprot{"m0_awprot"};
        ch_in<ch_bool> m0_awvalid{"m0_awvalid"};
        ch_out<ch_bool> m0_awready{"m0_awready"};
        ch_in<ch_uint<DATA_WIDTH>> m0_wdata{"m0_wdata"};
        ch_in<ch_uint<DATA_WIDTH/8>> m0_wstrb{"m0_wstrb"};
        ch_in<ch_bool> m0_wvalid{"m0_wvalid"};
        ch_out<ch_bool> m0_wready{"m0_wready"};
        ch_out<ch_uint<2>> m0_bresp{"m0_bresp"};
        ch_out<ch_bool> m0_bvalid{"m0_bvalid"};
        ch_in<ch_bool> m0_bready{"m0_bready"};
        ch_in<ch_uint<ADDR_WIDTH>> m0_araddr{"m0_araddr"};
        ch_in<ch_uint<2>> m0_arprot{"m0_arprot"};
        ch_in<ch_bool> m0_arvalid{"m0_arvalid"};
        ch_out<ch_bool> m0_arready{"m0_arready"};
        ch_out<ch_uint<DATA_WIDTH>> m0_rdata{"m0_rdata"};
        ch_out<ch_uint<2>> m0_rresp{"m0_rresp"};
        ch_out<ch_bool> m0_rvalid{"m0_rvalid"};
        ch_in<ch_bool> m0_rready{"m0_rready"};
        
        // ---- master 1 ----
        ch_in<ch_uint<ADDR_WIDTH>> m1_awaddr{"m1_awaddr"};
        ch_in<ch_uint<2>> m1_awprot{"m1_awprot"};
        ch_in<ch_bool> m1_awvalid{"m1_awvalid"};
        ch_out<ch_bool> m1_awready{"m1_awready"};
        ch_in<ch_uint<DATA_WIDTH>> m1_wdata{"m1_wdata"};
        ch_in<ch_uint<DATA_WIDTH/8>> m1_wstrb{"m1_wstrb"};
        ch_in<ch_bool> m1_wvalid{"m1_wvalid"};
        ch_out<ch_bool> m1_wready{"m1_wready"};
        ch_out<ch_uint<2>> m1_bresp{"m1_bresp"};
        ch_out<ch_bool> m1_bvalid{"m1_bvalid"};
        ch_in<ch_bool> m1_bready{"m1_bready"};
        ch_in<ch_uint<ADDR_WIDTH>> m1_araddr{"m1_araddr"};
        ch_in<ch_uint<2>> m1_arprot{"m1_arprot"};
        ch_in<ch_bool> m1_arvalid{"m1_arvalid"};
        ch_out<ch_bool> m1_arready{"m1_arready"};
        ch_out<ch_uint<DATA_WIDTH>> m1_rdata{"m1_rdata"};
        ch_out<ch_uint<2>> m1_rresp{"m1_rresp"};
        ch_out<ch_bool> m1_rvalid{"m1_rvalid"};
        ch_in<ch_bool> m1_rready{"m1_rready"};
        
        // ---- slave 0 ----
        ch_out<ch_uint<ADDR_WIDTH>> s0_awaddr{"s0_awaddr"};
        ch_out<ch_uint<2>> s0_awprot{"s0_awprot"};
        ch_out<ch_bool> s0_awvalid{"s0_awvalid"};
        ch_in<ch_bool> s0_awready{"s0_awready"};
        ch_out<ch_uint<DATA_WIDTH>> s0_wdata{"s0_wdata"};
        ch_out<ch_uint<DATA_WIDTH/8>> s0_wstrb{"s0_wstrb"};
        ch_out<ch_bool> s0_wvalid{"s0_wvalid"};
        ch_in<ch_bool> s0_wready{"s0_wready"};
        ch_in<ch_uint<2>> s0_bresp{"s0_bresp"};
        ch_in<ch_bool> s0_bvalid{"s0_bvalid"};
        ch_out<ch_bool> s0_bready{"s0_bready"};
        ch_out<ch_uint<ADDR_WIDTH>> s0_araddr{"s0_araddr"};
        ch_out<ch_uint<2>> s0_arprot{"s0_arprot"};
        ch_out<ch_bool> s0_arvalid{"s0_arvalid"};
        ch_in<ch_bool> s0_arready{"s0_arready"};
        ch_in<ch_uint<DATA_WIDTH>> s0_rdata{"s0_rdata"};
        ch_in<ch_uint<2>> s0_rresp{"s0_rresp"};
        ch_in<ch_bool> s0_rvalid{"s0_rvalid"};
        ch_out<ch_bool> s0_rready{"s0_rready"};
        
        // ---- slave 1 ----
        ch_out<ch_uint<ADDR_WIDTH>> s1_awaddr{"s1_awaddr"};
        ch_out<ch_uint<2>> s1_awprot{"s1_awprot"};
        ch_out<ch_bool> s1_awvalid{"s1_awvalid"};
        ch_in<ch_bool> s1_awready{"s1_awready"};
        ch_out<ch_uint<DATA_WIDTH>> s1_wdata{"s1_wdata"};
        ch_out<ch_uint<DATA_WIDTH/8>> s1_wstrb{"s1_wstrb"};
        ch_out<ch_bool> s1_wvalid{"s1_wvalid"};
        ch_in<ch_bool> s1_wready{"s1_wready"};
        ch_in<ch_uint<2>> s1_bresp{"s1_bresp"};
        ch_in<ch_bool> s1_bvalid{"s1_bvalid"};
        ch_out<ch_bool> s1_bready{"s1_bready"};
        ch_out<ch_uint<ADDR_WIDTH>> s1_araddr{"s1_araddr"};
        ch_out<ch_uint<2>> s1_arprot{"s1_arprot"};
        ch_out<ch_bool> s1_arvalid{"s1_arvalid"};
        ch_in<ch_bool> s1_arready{"s1_arready"};
        ch_in<ch_uint<DATA_WIDTH>> s1_rdata{"s1_rdata"};
        ch_in<ch_uint<2>> s1_rresp{"s1_rresp"};
        ch_in<ch_bool> s1_rvalid{"s1_rvalid"};
        ch_out<ch_bool> s1_rready{"s1_rready"};
        
        // ---- slave 2 ----
        ch_out<ch_uint<ADDR_WIDTH>> s2_awaddr{"s2_awaddr"};
        ch_out<ch_uint<2>> s2_awprot{"s2_awprot"};
        ch_out<ch_bool> s2_awvalid{"s2_awvalid"};
        ch_in<ch_bool> s2_awready{"s2_awready"};
        ch_out<ch_uint<DATA_WIDTH>> s2_wdata{"s2_wdata"};
        ch_out<ch_uint<DATA_WIDTH/8>> s2_wstrb{"s2_wstrb"};
        ch_out<ch_bool> s2_wvalid{"s2_wvalid"};
        ch_in<ch_bool> s2_wready{"s2_wready"};
        ch_in<ch_uint<2>> s2_bresp{"s2_bresp"};
        ch_in<ch_bool> s2_bvalid{"s2_bvalid"};
        ch_out<ch_bool> s2_bready{"s2_bready"};
        ch_out<ch_uint<ADDR_WIDTH>> s2_araddr{"s2_araddr"};
        ch_out<ch_uint<2>> s2_arprot{"s2_arprot"};
        ch_out<ch_bool> s2_arvalid{"s2_arvalid"};
        ch_in<ch_bool> s2_arready{"s2_arready"};
        ch_in<ch_uint<DATA_WIDTH>> s2_rdata{"s2_rdata"};
        ch_in<ch_uint<2>> s2_rresp{"s2_rresp"};
        ch_in<ch_bool> s2_rvalid{"s2_rvalid"};
        ch_out<ch_bool> s2_rready{"s2_rready"};
        
        // ---- slave 3 ----
        ch_out<ch_uint<ADDR_WIDTH>> s3_awaddr{"s3_awaddr"};
        ch_out<ch_uint<2>> s3_awprot{"s3_awprot"};
        ch_out<ch_bool> s3_awvalid{"s3_awvalid"};
        ch_in<ch_bool> s3_awready{"s3_awready"};
        ch_out<ch_uint<DATA_WIDTH>> s3_wdata{"s3_wdata"};
        ch_out<ch_uint<DATA_WIDTH/8>> s3_wstrb{"s3_wstrb"};
        ch_out<ch_bool> s3_wvalid{"s3_wvalid"};
        ch_in<ch_bool> s3_wready{"s3_wready"};
        ch_in<ch_uint<2>> s3_bresp{"s3_bresp"};
        ch_in<ch_bool> s3_bvalid{"s3_bvalid"};
        ch_out<ch_bool> s3_bready{"s3_bready"};
        ch_out<ch_uint<ADDR_WIDTH>> s3_araddr{"s3_araddr"};
        ch_out<ch_uint<2>> s3_arprot{"s3_arprot"};
        ch_out<ch_bool> s3_arvalid{"s3_arvalid"};
        ch_in<ch_bool> s3_arready{"s3_arready"};
        ch_in<ch_uint<DATA_WIDTH>> s3_rdata{"s3_rdata"};
        ch_in<ch_uint<2>> s3_rresp{"s3_rresp"};
        ch_in<ch_bool> s3_rvalid{"s3_rvalid"};
        ch_out<ch_bool> s3_rready{"s3_rready"};
        
        // ---- slave 4 ----
        ch_out<ch_uint<ADDR_WIDTH>> s4_awaddr{"s4_awaddr"};
        ch_out<ch_uint<2>> s4_awprot{"s4_awprot"};
        ch_out<ch_bool> s4_awvalid{"s4_awvalid"};
        ch_in<ch_bool> s4_awready{"s4_awready"};
        ch_out<ch_uint<DATA_WIDTH>> s4_wdata{"s4_wdata"};
        ch_out<ch_uint<DATA_WIDTH/8>> s4_wstrb{"s4_wstrb"};
        ch_out<ch_bool> s4_wvalid{"s4_wvalid"};
        ch_in<ch_bool> s4_wready{"s4_wready"};
        ch_in<ch_uint<2>> s4_bresp{"s4_bresp"};
        ch_in<ch_bool> s4_bvalid{"s4_bvalid"};
        ch_out<ch_bool> s4_bready{"s4_bready"};
        ch_out<ch_uint<ADDR_WIDTH>> s4_araddr{"s4_araddr"};
        ch_out<ch_uint<2>> s4_arprot{"s4_arprot"};
        ch_out<ch_bool> s4_arvalid{"s4_arvalid"};
        ch_in<ch_bool> s4_arready{"s4_arready"};
        ch_in<ch_uint<DATA_WIDTH>> s4_rdata{"s4_rdata"};
        ch_in<ch_uint<2>> s4_rresp{"s4_rresp"};
        ch_in<ch_bool> s4_rvalid{"s4_rvalid"};
        ch_out<ch_bool> s4_rready{"s4_rready"};
        
        // ---- slave 5 ----
        ch_out<ch_uint<ADDR_WIDTH>> s5_awaddr{"s5_awaddr"};
        ch_out<ch_uint<2>> s5_awprot{"s5_awprot"};
        ch_out<ch_bool> s5_awvalid{"s5_awvalid"};
        ch_in<ch_bool> s5_awready{"s5_awready"};
        ch_out<ch_uint<DATA_WIDTH>> s5_wdata{"s5_wdata"};
        ch_out<ch_uint<DATA_WIDTH/8>> s5_wstrb{"s5_wstrb"};
        ch_out<ch_bool> s5_wvalid{"s5_wvalid"};
        ch_in<ch_bool> s5_wready{"s5_wready"};
        ch_in<ch_uint<2>> s5_bresp{"s5_bresp"};
        ch_in<ch_bool> s5_bvalid{"s5_bvalid"};
        ch_out<ch_bool> s5_bready{"s5_bready"};
        ch_out<ch_uint<ADDR_WIDTH>> s5_araddr{"s5_araddr"};
        ch_out<ch_uint<2>> s5_arprot{"s5_arprot"};
        ch_out<ch_bool> s5_arvalid{"s5_arvalid"};
        ch_in<ch_bool> s5_arready{"s5_arready"};
        ch_in<ch_uint<DATA_WIDTH>> s5_rdata{"s5_rdata"};
        ch_in<ch_uint<2>> s5_rresp{"s5_rresp"};
        ch_in<ch_bool> s5_rvalid{"s5_rvalid"};
        ch_out<ch_bool> s5_rready{"s5_rready"};
        
    )

    AxiLiteCrossbar(ch::Component *parent = nullptr,
                    const std::string &name = "axi_lite_crossbar")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    // =================================================================
    // 编译期：地址窗口合法性（写错 ⇒ 编译失败，不是静默错路由）
    // =================================================================
    static constexpr bool windows_are_valid() {
        for (unsigned i = 0; i < kNumWindows; ++i) {
            const auto &w = Map::windows[i];
            if (!axi_window_is_valid(AxiAddrWindow{w.base, w.size})) {
                return false;
            }
            if (!axi_window_fits(AxiAddrWindow{w.base, w.size}, ADDR_WIDTH)) {
                return false;
            }
            if (w.target != kErr && w.target >= kNumSlaves) {
                return false;
            }
        }
        return true;
    }
    static_assert(windows_are_valid(),
                  "Map::windows 非法：size MUST 为 2 的幂、base MUST 按 size 对齐且"
                  "落在 ADDR_WIDTH 位内、target MUST 是 slave 索引或 kErr");

    static constexpr uint64_t addr_bits_mask() {
        return ADDR_WIDTH >= 64 ? ~uint64_t(0)
                                : ((uint64_t(1) << (ADDR_WIDTH % 64)) - uint64_t(1));
    }

    static ch_bool window_hit(const ADDR &addr, uint64_t base, uint64_t size) {
        // ⚠️ MUST 先把 base / mask 截到 ADDR_WIDTH 位 —— 否则调试构建触发
        //    `CH_CHECK`「value out of range」（NDEBUG 下静默截断）。
        const uint64_t m = (~(size - 1ull)) & addr_bits_mask();
        const uint64_t b = base & addr_bits_mask();
        return (addr & ADDR(m)) == ADDR(b);
    }

    // 地址 → 路由档（0..M-1 = slave；`kErrRoute` = 未映射 / ERR 窗口）。
    // 窗口越靠前优先级越高（循环从后往前 ⇒ ⇒ 窗口 0 最后写入）。
    static ch_uint<kRouteW> decode_addr(const ADDR &addr) {
        ch_uint<kRouteW> t = make_uint<kRouteW>(kErrRoute);
        for (unsigned k = 0; k < kNumWindows; ++k) {
            const unsigned idx = kNumWindows - 1 - k;
            const auto &w = Map::windows[idx];
            const ch_bool hit = window_hit(addr, w.base, w.size);
            const unsigned target = (w.target == kErr) ? kErrRoute : w.target;
            t = select(hit, make_uint<kRouteW>(target), t);
        }
        return t;
    }

    // slave j 是否已有在途事务（写 / 读各自一套）。同一 slave 的写/读 MUST 串行
    // ⇒ ⇒ W / B / R 平面的 `owner` 才唯一（否则两条 master 的 W 会抢同一个 mux）。
    static ch_bool busy_of(const std::array<ch_bool, kNumMasters> &act,
                           const std::array<ch_uint<kRouteW>, kNumMasters> &route,
                           unsigned j) {
        const ch_uint<kRouteW> t = make_uint<kRouteW>(j);
        return (act[0] & (route[0] == t)) | (act[1] & (route[1] == t));
    }

    // 该 slave 的在途 owner（无在途时返 0；调用方 MUST 同时用 `busy_of` 判有效）。
    static ch_uint<kOwnerW> owner_of(const std::array<ch_bool, kNumMasters> &act,
                                     const std::array<ch_uint<kRouteW>, kNumMasters> &route,
                                     unsigned j) {
        const ch_uint<kRouteW> t = make_uint<kRouteW>(j);
        const ch_bool b0 = act[0] & (route[0] == t);
        const ch_bool b1 = act[1] & (route[1] == t);
        return select(b0, make_uint<kOwnerW>(0),
                      select(b1, make_uint<kOwnerW>(1), make_uint<kOwnerW>(0)));
    }

    // =================================================================
    // 状态
    // =================================================================
    struct State {
        ch_reg<ch_uint<2>> aw_last_grant{ch_uint<2>(0_d)};
        ch_reg<ch_uint<2>> ar_last_grant{ch_uint<2>(0_d)};

        ch_reg<ch_bool> wr_active_m0{ch_bool(false)};
        ch_reg<ch_bool> wr_active_m1{ch_bool(false)};
        ch_reg<ch_uint<kRouteW>> wr_route_m0{ch_uint<kRouteW>(0_d)};
        ch_reg<ch_uint<kRouteW>> wr_route_m1{ch_uint<kRouteW>(0_d)};

        ch_reg<ch_bool> rd_active_m0{ch_bool(false)};
        ch_reg<ch_bool> rd_active_m1{ch_bool(false)};
        ch_reg<ch_uint<kRouteW>> rd_route_m0{ch_uint<kRouteW>(0_d)};
        ch_reg<ch_uint<kRouteW>> rd_route_m1{ch_uint<kRouteW>(0_d)};

        // ERR 路的本地响应（DECERR）：写 / 读各自一套，保持到 master 接受
        ch_reg<ch_bool> err_b_pending_m0{ch_bool(false)};
        ch_reg<ch_bool> err_b_pending_m1{ch_bool(false)};
        ch_reg<ch_bool> err_r_pending_m0{ch_bool(false)};
        ch_reg<ch_bool> err_r_pending_m1{ch_bool(false)};
    };

    void describe() override {
        State s;

        // =============================================================
        // 0. 端口指针表（把具名端口按 master / slave 编号收进数组）
        // =============================================================
        // master 读侧（crossbar 读 master 的请求）
        const std::array<const ch_in<ADDR> *, kNumMasters> m_awaddr{&io().m0_awaddr,
                                                                    &io().m1_awaddr};
        const std::array<const ch_in<RESP> *, kNumMasters> m_awprot{&io().m0_awprot,
                                                                    &io().m1_awprot};
        const std::array<const ch_in<ch_bool> *, kNumMasters> m_awvalid{&io().m0_awvalid,
                                                                        &io().m1_awvalid};
        const std::array<const ch_in<DATA> *, kNumMasters> m_wdata{&io().m0_wdata,
                                                                   &io().m1_wdata};
        const std::array<const ch_in<STRB> *, kNumMasters> m_wstrb{&io().m0_wstrb,
                                                                   &io().m1_wstrb};
        const std::array<const ch_in<ch_bool> *, kNumMasters> m_wvalid{&io().m0_wvalid,
                                                                       &io().m1_wvalid};
        const std::array<const ch_in<ch_bool> *, kNumMasters> m_bready{&io().m0_bready,
                                                                       &io().m1_bready};
        const std::array<const ch_in<ADDR> *, kNumMasters> m_araddr{&io().m0_araddr,
                                                                    &io().m1_araddr};
        const std::array<const ch_in<RESP> *, kNumMasters> m_arprot{&io().m0_arprot,
                                                                    &io().m1_arprot};
        const std::array<const ch_in<ch_bool> *, kNumMasters> m_arvalid{&io().m0_arvalid,
                                                                        &io().m1_arvalid};
        const std::array<const ch_in<ch_bool> *, kNumMasters> m_rready{&io().m0_rready,
                                                                       &io().m1_rready};
        // master 写侧（crossbar 驱动回 master）
        const std::array<ch_out<ch_bool> *, kNumMasters> m_awready{&io().m0_awready,
                                                                   &io().m1_awready};
        const std::array<ch_out<ch_bool> *, kNumMasters> m_wready{&io().m0_wready,
                                                                  &io().m1_wready};
        const std::array<ch_out<RESP> *, kNumMasters> m_bresp{&io().m0_bresp, &io().m1_bresp};
        const std::array<ch_out<ch_bool> *, kNumMasters> m_bvalid{&io().m0_bvalid,
                                                                  &io().m1_bvalid};
        const std::array<ch_out<ch_bool> *, kNumMasters> m_arready{&io().m0_arready,
                                                                   &io().m1_arready};
        const std::array<ch_out<DATA> *, kNumMasters> m_rdata{&io().m0_rdata, &io().m1_rdata};
        const std::array<ch_out<RESP> *, kNumMasters> m_rresp{&io().m0_rresp, &io().m1_rresp};
        const std::array<ch_out<ch_bool> *, kNumMasters> m_rvalid{&io().m0_rvalid,
                                                                  &io().m1_rvalid};

        // slave 读侧（crossbar 读 slave 的响应）
        const std::array<const ch_in<ch_bool> *, kNumSlaves> s_awready{
            &io().s0_awready, &io().s1_awready, &io().s2_awready,
            &io().s3_awready, &io().s4_awready, &io().s5_awready};
        const std::array<const ch_in<ch_bool> *, kNumSlaves> s_wready{
            &io().s0_wready, &io().s1_wready, &io().s2_wready,
            &io().s3_wready, &io().s4_wready, &io().s5_wready};
        const std::array<const ch_in<RESP> *, kNumSlaves> s_bresp{
            &io().s0_bresp, &io().s1_bresp, &io().s2_bresp,
            &io().s3_bresp, &io().s4_bresp, &io().s5_bresp};
        const std::array<const ch_in<ch_bool> *, kNumSlaves> s_bvalid{
            &io().s0_bvalid, &io().s1_bvalid, &io().s2_bvalid,
            &io().s3_bvalid, &io().s4_bvalid, &io().s5_bvalid};
        const std::array<const ch_in<ch_bool> *, kNumSlaves> s_arready{
            &io().s0_arready, &io().s1_arready, &io().s2_arready,
            &io().s3_arready, &io().s4_arready, &io().s5_arready};
        const std::array<const ch_in<DATA> *, kNumSlaves> s_rdata{
            &io().s0_rdata, &io().s1_rdata, &io().s2_rdata,
            &io().s3_rdata, &io().s4_rdata, &io().s5_rdata};
        const std::array<const ch_in<RESP> *, kNumSlaves> s_rresp{
            &io().s0_rresp, &io().s1_rresp, &io().s2_rresp,
            &io().s3_rresp, &io().s4_rresp, &io().s5_rresp};
        const std::array<const ch_in<ch_bool> *, kNumSlaves> s_rvalid{
            &io().s0_rvalid, &io().s1_rvalid, &io().s2_rvalid,
            &io().s3_rvalid, &io().s4_rvalid, &io().s5_rvalid};
        // slave 写侧（crossbar 驱动给 slave）
        const std::array<ch_out<ADDR> *, kNumSlaves> s_awaddr{
            &io().s0_awaddr, &io().s1_awaddr, &io().s2_awaddr,
            &io().s3_awaddr, &io().s4_awaddr, &io().s5_awaddr};
        const std::array<ch_out<RESP> *, kNumSlaves> s_awprot{
            &io().s0_awprot, &io().s1_awprot, &io().s2_awprot,
            &io().s3_awprot, &io().s4_awprot, &io().s5_awprot};
        const std::array<ch_out<ch_bool> *, kNumSlaves> s_awvalid{
            &io().s0_awvalid, &io().s1_awvalid, &io().s2_awvalid,
            &io().s3_awvalid, &io().s4_awvalid, &io().s5_awvalid};
        const std::array<ch_out<DATA> *, kNumSlaves> s_wdata{
            &io().s0_wdata, &io().s1_wdata, &io().s2_wdata,
            &io().s3_wdata, &io().s4_wdata, &io().s5_wdata};
        const std::array<ch_out<STRB> *, kNumSlaves> s_wstrb{
            &io().s0_wstrb, &io().s1_wstrb, &io().s2_wstrb,
            &io().s3_wstrb, &io().s4_wstrb, &io().s5_wstrb};
        const std::array<ch_out<ch_bool> *, kNumSlaves> s_wvalid{
            &io().s0_wvalid, &io().s1_wvalid, &io().s2_wvalid,
            &io().s3_wvalid, &io().s4_wvalid, &io().s5_wvalid};
        const std::array<ch_out<ch_bool> *, kNumSlaves> s_bready{
            &io().s0_bready, &io().s1_bready, &io().s2_bready,
            &io().s3_bready, &io().s4_bready, &io().s5_bready};
        const std::array<ch_out<ADDR> *, kNumSlaves> s_araddr{
            &io().s0_araddr, &io().s1_araddr, &io().s2_araddr,
            &io().s3_araddr, &io().s4_araddr, &io().s5_araddr};
        const std::array<ch_out<RESP> *, kNumSlaves> s_arprot{
            &io().s0_arprot, &io().s1_arprot, &io().s2_arprot,
            &io().s3_arprot, &io().s4_arprot, &io().s5_arprot};
        const std::array<ch_out<ch_bool> *, kNumSlaves> s_arvalid{
            &io().s0_arvalid, &io().s1_arvalid, &io().s2_arvalid,
            &io().s3_arvalid, &io().s4_arvalid, &io().s5_arvalid};
        const std::array<ch_out<ch_bool> *, kNumSlaves> s_rready{
            &io().s0_rready, &io().s1_rready, &io().s2_rready,
            &io().s3_rready, &io().s4_rready, &io().s5_rready};

        // 状态寄存器指针表（供循环里写 `->next`）
        const std::array<ch_reg<ch_bool> *, kNumMasters> st_wr_active{
            &s.wr_active_m0, &s.wr_active_m1};
        const std::array<ch_reg<ch_uint<kRouteW>> *, kNumMasters> st_wr_route{
            &s.wr_route_m0, &s.wr_route_m1};
        const std::array<ch_reg<ch_bool> *, kNumMasters> st_rd_active{
            &s.rd_active_m0, &s.rd_active_m1};
        const std::array<ch_reg<ch_uint<kRouteW>> *, kNumMasters> st_rd_route{
            &s.rd_route_m0, &s.rd_route_m1};
        const std::array<ch_reg<ch_bool> *, kNumMasters> st_err_b{
            &s.err_b_pending_m0, &s.err_b_pending_m1};
        const std::array<ch_reg<ch_bool> *, kNumMasters> st_err_r{
            &s.err_r_pending_m0, &s.err_r_pending_m1};

        // =============================================================
        // 1. 状态快照（读 `ch_reg` MUST 走隐式转换）
        // =============================================================
        const ch_bool rst = !ch_bool(io().rst_n.impl());
        const std::array<ch_bool, kNumMasters> wr_act{s.wr_active_m0, s.wr_active_m1};
        const std::array<ch_uint<kRouteW>, kNumMasters> wr_route{s.wr_route_m0,
                                                                s.wr_route_m1};
        const std::array<ch_bool, kNumMasters> rd_act{s.rd_active_m0, s.rd_active_m1};
        const std::array<ch_uint<kRouteW>, kNumMasters> rd_route{s.rd_route_m0,
                                                                s.rd_route_m1};
        const std::array<ch_bool, kNumMasters> err_b_pend{s.err_b_pending_m0,
                                                          s.err_b_pending_m1};
        const std::array<ch_bool, kNumMasters> err_r_pend{s.err_r_pending_m0,
                                                          s.err_r_pending_m1};

        // =============================================================
        // 2. AW 平面：stream_mux（grant 选主）→ stream_demux（地址选从）
        // =============================================================
        const ch_bool aw_req0 = ch_bool((*m_awvalid[0]).impl()) & !wr_act[0];
        const ch_bool aw_req1 = ch_bool((*m_awvalid[1]).impl()) & !wr_act[1];
        const ch_uint<2> aw_req =
            select(aw_req0, ch_uint<2>(1_d), ch_uint<2>(0_d)) |
            select(aw_req1, ch_uint<2>(2_d), ch_uint<2>(0_d));
        const ch_uint<2> aw_lg = s.aw_last_grant;
        const auto aw_rr = chlib::round_robin_selector<kNumMasters>(aw_req, aw_lg);
        const ch_uint<kOwnerW> aw_idx =
            chlib::onehot_to_binary<kNumMasters>(aw_rr.grant);

        std::array<ch_stream<AwPayload>, kNumMasters> aw_in;
        for (unsigned i = 0; i < kNumMasters; ++i) {
            aw_in[i].as_master();  // MUST：组合子才能把反压钉到 ready 上
        }
        aw_in[0].payload = concat(RESP((*m_awprot[0]).impl()), ADDR((*m_awaddr[0]).impl()));
        aw_in[0].valid = aw_req0;
        aw_in[1].payload = concat(RESP((*m_awprot[1]).impl()), ADDR((*m_awaddr[1]).impl()));
        aw_in[1].valid = aw_req1;

        auto aw_mux = chlib::stream_mux<kNumMasters, AwPayload>(aw_in, aw_idx);
        const ch_uint<kRouteW> aw_target =
            decode_addr(bits<ADDR_WIDTH - 1, 0>(aw_mux.output_stream.payload));
        auto aw_demux = chlib::stream_demux<AwPayload, kNumRoutes>(aw_mux.output_stream,
                                                                   aw_target);
        // 目标 slave 忙（同一 slave 已有在途写）⇒ ⇒ 本拍不接受（MUST NOT 丢请求，
        // master 保持 valid；下拍重试）。
        // ⚠️ `valid` MUST 也按忙门控 —— 只掐 `ready` 不够：slave 侧看到
        //    `awvalid=1 & awready=1` 就是一次握手（会与在途事务撞车）。
        std::array<ch_bool, kNumSlaves> wr_busy;
        for (unsigned j = 0; j < kNumSlaves; ++j) {
            wr_busy[j] = busy_of(wr_act, wr_route, j);
            aw_demux.output_streams[j].ready <<=
                ch_bool((*s_awready[j]).impl()) & !wr_busy[j];
        }
        for (unsigned j = 0; j < kNumSlaves; ++j) {
            *s_awvalid[j] = aw_demux.output_streams[j].valid & !wr_busy[j];
            *s_awaddr[j] = bits<ADDR_WIDTH - 1, 0>(aw_demux.output_streams[j].payload);
            *s_awprot[j] =
                bits<ADDR_WIDTH + 1, ADDR_WIDTH>(aw_demux.output_streams[j].payload);
        }
        std::array<ch_bool, kNumMasters> awready_v;
        for (unsigned i = 0; i < kNumMasters; ++i) {
            // 在途门控：同向只允许 1 笔（否则 master 认为被接受而互连静默吞掉）
            awready_v[i] = aw_mux.input_streams[i].ready & !wr_act[i];
            *m_awready[i] = awready_v[i];
        }
        const ch_bool aw_fire = aw_mux.output_stream.valid & aw_mux.output_stream.ready;

        // =============================================================
        // 3. W 平面：每 master demux（按锁存写路由）→ 每 slave mux（按 owner）
        // =============================================================
        std::array<ch_stream<WPayload>, kNumMasters> w_in;
        for (unsigned i = 0; i < kNumMasters; ++i) {
            w_in[i].as_master();
        }
        w_in[0].payload = concat(STRB((*m_wstrb[0]).impl()), DATA((*m_wdata[0]).impl()));
        w_in[0].valid = ch_bool((*m_wvalid[0]).impl()) & wr_act[0];
        w_in[1].payload = concat(STRB((*m_wstrb[1]).impl()), DATA((*m_wdata[1]).impl()));
        w_in[1].valid = ch_bool((*m_wvalid[1]).impl()) & wr_act[1];

        const std::array<chlib::StreamDemuxResult<WPayload, kNumRoutes>, kNumMasters>
            w_demux{chlib::stream_demux<WPayload, kNumRoutes>(w_in[0], wr_route[0]),
                    chlib::stream_demux<WPayload, kNumRoutes>(w_in[1], wr_route[1])};
        std::array<ch_bool, kNumMasters> wready_v{
            w_demux[0].input_stream.ready & wr_act[0],
            w_demux[1].input_stream.ready & wr_act[1]};
        for (unsigned i = 0; i < kNumMasters; ++i) {
            *m_wready[i] = wready_v[i];
        }
        for (unsigned j = 0; j < kNumSlaves; ++j) {
            std::array<ch_stream<WPayload>, kNumMasters> cand{
                w_demux[0].output_streams[j], w_demux[1].output_streams[j]};
            auto m = chlib::stream_mux<kNumMasters, WPayload>(
                cand, owner_of(wr_act, wr_route, j));
            *s_wvalid[j] = m.output_stream.valid;
            *s_wdata[j] = bits<DATA_WIDTH - 1, 0>(m.output_stream.payload);
            *s_wstrb[j] =
                bits<DATA_WIDTH + DATA_WIDTH / 8 - 1, DATA_WIDTH>(m.output_stream.payload);
            m.output_stream.ready <<= ch_bool((*s_wready[j]).impl());
        }
        const std::array<ch_bool, kNumMasters> w_fire{
            ch_bool((*m_wvalid[0]).impl()) & wready_v[0],
            ch_bool((*m_wvalid[1]).impl()) & wready_v[1]};

        // =============================================================
        // 4. B 平面：每 slave demux（按 owner 反查）→ 每 master mux（按锁存路由）
        // =============================================================
        std::array<ch_stream<BPayload>, kNumSlaves> b_in;
        for (unsigned j = 0; j < kNumSlaves; ++j) {
            b_in[j].as_master();
            b_in[j].payload = RESP((*s_bresp[j]).impl());
            b_in[j].valid = ch_bool((*s_bvalid[j]).impl());
        }
        const std::array<chlib::StreamDemuxResult<BPayload, kNumMasters>, kNumSlaves>
            b_demux{
                chlib::stream_demux<BPayload, kNumMasters>(b_in[0],
                                                           owner_of(wr_act, wr_route, 0)),
                chlib::stream_demux<BPayload, kNumMasters>(b_in[1],
                                                           owner_of(wr_act, wr_route, 1)),
                chlib::stream_demux<BPayload, kNumMasters>(b_in[2],
                                                           owner_of(wr_act, wr_route, 2)),
                chlib::stream_demux<BPayload, kNumMasters>(b_in[3],
                                                           owner_of(wr_act, wr_route, 3)),
                chlib::stream_demux<BPayload, kNumMasters>(b_in[4],
                                                           owner_of(wr_act, wr_route, 4)),
                chlib::stream_demux<BPayload, kNumMasters>(b_in[5],
                                                           owner_of(wr_act, wr_route, 5))};
        for (unsigned j = 0; j < kNumSlaves; ++j) {
            *s_bready[j] = b_demux[j].input_stream.ready;
        }

        std::array<ch_stream<BPayload>, kNumMasters> err_b_s;
        for (unsigned i = 0; i < kNumMasters; ++i) {
            err_b_s[i].as_master();
            err_b_s[i].payload = make_uint<2>(static_cast<unsigned>(AxiResp::DECERR));
            err_b_s[i].valid = err_b_pend[i];
        }
        std::array<chlib::StreamMuxResult<kNumRoutes, BPayload>, kNumMasters> b_mux{
            chlib::stream_mux<kNumRoutes, BPayload>(
                std::array<ch_stream<BPayload>, kNumRoutes>{
                    b_demux[0].output_streams[0], b_demux[1].output_streams[0],
                    b_demux[2].output_streams[0], b_demux[3].output_streams[0],
                    b_demux[4].output_streams[0], b_demux[5].output_streams[0],
                    err_b_s[0]},
                wr_route[0]),
            chlib::stream_mux<kNumRoutes, BPayload>(
                std::array<ch_stream<BPayload>, kNumRoutes>{
                    b_demux[0].output_streams[1], b_demux[1].output_streams[1],
                    b_demux[2].output_streams[1], b_demux[3].output_streams[1],
                    b_demux[4].output_streams[1], b_demux[5].output_streams[1],
                    err_b_s[1]},
                wr_route[1])};
        std::array<ch_bool, kNumMasters> bvalid_v{wr_act[0] & b_mux[0].output_stream.valid,
                                                  wr_act[1] & b_mux[1].output_stream.valid};
        for (unsigned i = 0; i < kNumMasters; ++i) {
            *m_bvalid[i] = bvalid_v[i];
            // 载荷 MUST 也按在途门控 —— 否则空闲时 `bresp` 会镜像某个 slave 的当前值
            *m_bresp[i] = select(wr_act[i], b_mux[i].output_stream.payload,
                                 make_uint<2>(0));
            b_mux[i].output_stream.ready <<= ch_bool((*m_bready[i]).impl());
        }
        const std::array<ch_bool, kNumMasters> b_fire{
            bvalid_v[0] & ch_bool((*m_bready[0]).impl()),
            bvalid_v[1] & ch_bool((*m_bready[1]).impl())};

        // =============================================================
        // 5. AR 平面（与 AW 同构，独立仲裁 / 独立路由）
        // =============================================================
        const ch_bool ar_req0 = ch_bool((*m_arvalid[0]).impl()) & !rd_act[0];
        const ch_bool ar_req1 = ch_bool((*m_arvalid[1]).impl()) & !rd_act[1];
        const ch_uint<2> ar_req =
            select(ar_req0, ch_uint<2>(1_d), ch_uint<2>(0_d)) |
            select(ar_req1, ch_uint<2>(2_d), ch_uint<2>(0_d));
        const ch_uint<2> ar_lg = s.ar_last_grant;
        const auto ar_rr = chlib::round_robin_selector<kNumMasters>(ar_req, ar_lg);
        const ch_uint<kOwnerW> ar_idx =
            chlib::onehot_to_binary<kNumMasters>(ar_rr.grant);

        std::array<ch_stream<ArPayload>, kNumMasters> ar_in;
        for (unsigned i = 0; i < kNumMasters; ++i) {
            ar_in[i].as_master();
        }
        ar_in[0].payload = concat(RESP((*m_arprot[0]).impl()), ADDR((*m_araddr[0]).impl()));
        ar_in[0].valid = ar_req0;
        ar_in[1].payload = concat(RESP((*m_arprot[1]).impl()), ADDR((*m_araddr[1]).impl()));
        ar_in[1].valid = ar_req1;

        auto ar_mux = chlib::stream_mux<kNumMasters, ArPayload>(ar_in, ar_idx);
        const ch_uint<kRouteW> ar_target =
            decode_addr(bits<ADDR_WIDTH - 1, 0>(ar_mux.output_stream.payload));
        auto ar_demux = chlib::stream_demux<ArPayload, kNumRoutes>(ar_mux.output_stream,
                                                                   ar_target);
        std::array<ch_bool, kNumSlaves> rd_busy;
        for (unsigned j = 0; j < kNumSlaves; ++j) {
            rd_busy[j] = busy_of(rd_act, rd_route, j);
            ar_demux.output_streams[j].ready <<=
                ch_bool((*s_arready[j]).impl()) & !rd_busy[j];
        }
        for (unsigned j = 0; j < kNumSlaves; ++j) {
            *s_arvalid[j] = ar_demux.output_streams[j].valid & !rd_busy[j];
            *s_araddr[j] = bits<ADDR_WIDTH - 1, 0>(ar_demux.output_streams[j].payload);
            *s_arprot[j] =
                bits<ADDR_WIDTH + 1, ADDR_WIDTH>(ar_demux.output_streams[j].payload);
        }
        std::array<ch_bool, kNumMasters> arready_v;
        for (unsigned i = 0; i < kNumMasters; ++i) {
            arready_v[i] = ar_mux.input_streams[i].ready & !rd_act[i];
            *m_arready[i] = arready_v[i];
        }
        const ch_bool ar_fire = ar_mux.output_stream.valid & ar_mux.output_stream.ready;

        // =============================================================
        // 6. R 平面（与 B 同构，载荷 = {resp, data}）
        // =============================================================
        std::array<ch_stream<RPayload>, kNumSlaves> r_in;
        for (unsigned j = 0; j < kNumSlaves; ++j) {
            r_in[j].as_master();
            r_in[j].payload = concat(RESP((*s_rresp[j]).impl()), DATA((*s_rdata[j]).impl()));
            r_in[j].valid = ch_bool((*s_rvalid[j]).impl());
        }
        const std::array<chlib::StreamDemuxResult<RPayload, kNumMasters>, kNumSlaves>
            r_demux{
                chlib::stream_demux<RPayload, kNumMasters>(r_in[0],
                                                           owner_of(rd_act, rd_route, 0)),
                chlib::stream_demux<RPayload, kNumMasters>(r_in[1],
                                                           owner_of(rd_act, rd_route, 1)),
                chlib::stream_demux<RPayload, kNumMasters>(r_in[2],
                                                           owner_of(rd_act, rd_route, 2)),
                chlib::stream_demux<RPayload, kNumMasters>(r_in[3],
                                                           owner_of(rd_act, rd_route, 3)),
                chlib::stream_demux<RPayload, kNumMasters>(r_in[4],
                                                           owner_of(rd_act, rd_route, 4)),
                chlib::stream_demux<RPayload, kNumMasters>(r_in[5],
                                                           owner_of(rd_act, rd_route, 5))};
        for (unsigned j = 0; j < kNumSlaves; ++j) {
            *s_rready[j] = r_demux[j].input_stream.ready;
        }

        std::array<ch_stream<RPayload>, kNumMasters> err_r_s;
        for (unsigned i = 0; i < kNumMasters; ++i) {
            err_r_s[i].as_master();
            err_r_s[i].payload = concat(
                make_uint<2>(static_cast<unsigned>(AxiResp::DECERR)),
                make_uint<DATA_WIDTH>(0));
            err_r_s[i].valid = err_r_pend[i];
        }
        std::array<chlib::StreamMuxResult<kNumRoutes, RPayload>, kNumMasters> r_mux{
            chlib::stream_mux<kNumRoutes, RPayload>(
                std::array<ch_stream<RPayload>, kNumRoutes>{
                    r_demux[0].output_streams[0], r_demux[1].output_streams[0],
                    r_demux[2].output_streams[0], r_demux[3].output_streams[0],
                    r_demux[4].output_streams[0], r_demux[5].output_streams[0],
                    err_r_s[0]},
                rd_route[0]),
            chlib::stream_mux<kNumRoutes, RPayload>(
                std::array<ch_stream<RPayload>, kNumRoutes>{
                    r_demux[0].output_streams[1], r_demux[1].output_streams[1],
                    r_demux[2].output_streams[1], r_demux[3].output_streams[1],
                    r_demux[4].output_streams[1], r_demux[5].output_streams[1],
                    err_r_s[1]},
                rd_route[1])};
        std::array<ch_bool, kNumMasters> rvalid_v{rd_act[0] & r_mux[0].output_stream.valid,
                                                  rd_act[1] & r_mux[1].output_stream.valid};
        for (unsigned i = 0; i < kNumMasters; ++i) {
            *m_rvalid[i] = rvalid_v[i];
            *m_rresp[i] =
                select(rd_act[i],
                       bits<DATA_WIDTH + 1, DATA_WIDTH>(r_mux[i].output_stream.payload),
                       make_uint<2>(0));
            *m_rdata[i] = select(rd_act[i],
                                 bits<DATA_WIDTH - 1, 0>(r_mux[i].output_stream.payload),
                                 make_uint<DATA_WIDTH>(0));
            r_mux[i].output_stream.ready <<= ch_bool((*m_rready[i]).impl());
        }
        const std::array<ch_bool, kNumMasters> r_fire{
            rvalid_v[0] & ch_bool((*m_rready[0]).impl()),
            rvalid_v[1] & ch_bool((*m_rready[1]).impl())};

        // =============================================================
        // 7. 状态更新（全部 `select`；禁 C++ 控制流判断硬件值）
        // =============================================================
        for (unsigned i = 0; i < kNumMasters; ++i) {
            const ch_bool aw_mine = aw_fire & (aw_idx == make_uint<kOwnerW>(i));
            (*st_wr_active[i])->next =
                select(rst, ch_bool(false),
                       select(aw_mine, ch_bool(true),
                              select(b_fire[i], ch_bool(false), wr_act[i])));
            (*st_wr_route[i])->next = select(rst, make_uint<kRouteW>(0),
                                          select(aw_mine, aw_target, wr_route[i]));
            // ERR 写：W 被就地消费后置 deccerr 响应，保持到 master 接受
            const ch_bool err_w =
                w_fire[i] & (wr_route[i] == make_uint<kRouteW>(kErrRoute));
            (*st_err_b[i])->next =
                select(rst, ch_bool(false),
                       select(err_w, ch_bool(true),
                              select(b_fire[i], ch_bool(false), err_b_pend[i])));
        }
        s.aw_last_grant->next = select(rst, ch_uint<2>(0_d),
                                       select(aw_fire, aw_rr.grant, aw_lg));

        for (unsigned i = 0; i < kNumMasters; ++i) {
            const ch_bool ar_mine = ar_fire & (ar_idx == make_uint<kOwnerW>(i));
            (*st_rd_active[i])->next =
                select(rst, ch_bool(false),
                       select(ar_mine, ch_bool(true),
                              select(r_fire[i], ch_bool(false), rd_act[i])));
            (*st_rd_route[i])->next = select(rst, make_uint<kRouteW>(0),
                                          select(ar_mine, ar_target, rd_route[i]));
            const ch_bool err_ar =
                ar_mine & (ar_target == make_uint<kRouteW>(kErrRoute));
            (*st_err_r[i])->next =
                select(rst, ch_bool(false),
                       select(err_ar, ch_bool(true),
                              select(r_fire[i], ch_bool(false), err_r_pend[i])));
        }
        s.ar_last_grant->next = select(rst, ch_uint<2>(0_d),
                                       select(ar_fire, ar_rr.grant, ar_lg));
    }
};

}  // namespace axi4
