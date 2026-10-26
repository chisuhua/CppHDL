/**
 * @file axi4_lite_matrix.h
 * @brief AXI4-Lite 2x2 Matrix
 *
 * 连接 2 个主设备到 2 个从设备，支持：
 * - 写 / 读**各自独立**的 round-robin 仲裁（AW / AR 各一套指针）
 * - 路由在 AW / AR 握手当拍**锁存**（写侧与读侧各自独立）
 * - 编译期可配置的地址窗口映射 + 互连层自应答的 DECERR error responder
 * - 低有效 `rst_n`（**在时钟沿生效**），全部状态寄存器带复位 pattern
 *
 * 契约：`openspec/specs/axi-interconnect-routing/spec.md`
 *
 * ⚠️ **AXI4-Lite 无 `arlen`**（AXI4-Lite 是单拍协议）⇒ error responder 的读侧
 * 恒返回 **1 个 beat**（不是 AXI4 Full 的 `arlen + 1`）。
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
    typename Map = AxiLiteDefaultMatrixMap
>
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

        // 复位（低有效，**在时钟沿生效** —— 配 `select(!io().rst_n, <init>, ...)`）。
        // `rst_n` 是**必需端口**，实例化点 MUST 显式接线；未接线读出为 0 = **永久复位**
        // （零编译错误的静默破坏：编译通过、运行时不报错，但状态永远停在初值）。
        // 互连层是 rst_n 链路的**中间节点**，MUST 与下游 slave 在同一复位窗口内复位。
        ch_in<ch_bool> rst_n;
    )

    AxiLiteMatrix2x2(ch::Component* parent = nullptr, const std::string& name = "axi_matrix_2x2")
        : ch::Component(parent, name) {}

    void create_ports() override {
        new (io_storage_) io_type;
    }

    // ==================== 状态 ====================
    // 全部 `ch_reg` MUST 有 `select(!io().rst_n, <init>, ...)` 复位 pattern
    // （低有效，**在时钟沿生效** —— 落在 `->next` 表达式上由时钟沿采样，**不是**异步复位）；
    // `<init>` MUST 带该寄存器的**显式位宽**。
    //
    // 设计要点：
    //   * 路由目标在 **AW / AR 握手当拍**锁存 ⇒ W / B / R 通道 MUST 用**锁存值**转发，
    //     MUST NOT 用活的 `awaddr` / `araddr`（否则 master 握手后改写地址会改错落点）。
    //   * 握手**决策**（`s*_awvalid` / `s*_arvalid` 的正向转发、`m*_awready` /
    //     `m*_arready` 的反查）MUST 用**当拍**地址解码 + **当拍** grant
    //     （AXI 要求 master 在 `awvalid && !awready` 期间保持地址稳定 ⇒ 握手窗口内的
    //     「活地址」是安全的）。
    //   * 写 / 读 MUST 各自独立仲裁（AXI4-Lite 的 AW/W/B 与 AR/R 是独立通道组）。
    struct MatrixState {
        ch_reg<ch_bool> aw_ptr;  // 写仲裁指针：0 ⇒ m0 优先
        ch_reg<ch_bool> ar_ptr;  // 读仲裁指针：0 ⇒ m0 优先

        // 写侧：AW 握手当拍锁存（route：0 ⇒ s0，1 ⇒ s1）
        ch_reg<ch_bool> wr_route_m0, wr_route_m1;
        ch_reg<ch_bool> wr_mapped_m0, wr_mapped_m1;
        ch_reg<ch_bool> wr_active_m0, wr_active_m1;

        // 读侧：AR 握手当拍锁存
        ch_reg<ch_bool> rd_route_m0, rd_route_m1;
        ch_reg<ch_bool> rd_mapped_m0, rd_mapped_m1;
        ch_reg<ch_bool> rd_active_m0, rd_active_m1;

        // error responder（写）：未映射地址由互连层自己应答
        ch_reg<ch_bool> err_w_busy;   // 已被一笔未映射写占用（AW 已握手）
        ch_reg<ch_bool> err_w_valid;  // bvalid，保持到 bready

        // error responder（读）：未映射读返回 1 个 DECERR beat（AXI4-Lite 无 arlen）
        ch_reg<ch_bool> err_r_valid;

        MatrixState()
            : aw_ptr(false), ar_ptr(false),
              wr_route_m0(false), wr_route_m1(false),
              wr_mapped_m0(false), wr_mapped_m1(false),
              wr_active_m0(false), wr_active_m1(false),
              rd_route_m0(false), rd_route_m1(false),
              rd_mapped_m0(false), rd_mapped_m1(false),
              rd_active_m0(false), rd_active_m1(false),
              err_w_busy(false), err_w_valid(false),
              err_r_valid(false) {}
    };

    // -------- 2 选 1 正向转发（**单次赋值** —— MUST NOT 写成「输出读自己」的自环，
    //          那会让拓扑排序报 `Cycle detected in IR graph`）--------
    // `V` 独立推导：实参是 `ch_in<T>`（端口），T 只由 `ch_out<T>` 与 `dflt` 决定
    // ⇒ 避免「deduced conflicting types for T」。
    template <typename T, typename V>
    static void fwd2(ch_out<T> &dst, const ch_bool &s0, const ch_bool &s1,
                     const V &v0, const V &v1, const T &dflt) {
        dst = select(s0, v0, select(s1, v1, dflt));
    }

    // -------- 按 1 bit 索引 2 选 1（反向通道用**锁存路由**反查）--------
    template <typename V>
    static auto idx2(const ch_bool &idx, const V &v0, const V &v1) {
        return select(idx, v1, v0);
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

    // -------- 当拍地址解码：`target` = 命中的 slave（1 ⇒ s1），`mapped` = 是否落在窗口内 --------
    void decode_addr(const ch_uint<ADDR_WIDTH> &addr, ch_bool &target, ch_bool &mapped) {
        static_assert(axi_window_is_valid(Map::windows[0]),
                      "地址窗口 0 非法：size MUST 非 0 且为 2 的幂，base MUST 按 size 对齐");
        static_assert(axi_window_is_valid(Map::windows[1]),
                      "地址窗口 1 非法：size MUST 非 0 且为 2 的幂，base MUST 按 size 对齐");
        static_assert(axi_window_fits(Map::windows[0], ADDR_WIDTH),
                      "地址窗口 0 超出 ADDR_WIDTH 位地址空间：base 的高位会被静默丢掉 "
                      "⇒ 别名到低地址（与低窗口双命中 ⇒ 静默错路由）");
        static_assert(axi_window_fits(Map::windows[1], ADDR_WIDTH),
                      "地址窗口 1 超出 ADDR_WIDTH 位地址空间：base 的高位会被静默丢掉 "
                      "⇒ 别名到低地址（与低窗口双命中 ⇒ 静默错路由）");
        const ch_bool h0 = window_hit(addr, Map::windows[0].base, Map::windows[0].size);
        const ch_bool h1 = window_hit(addr, Map::windows[1].base, Map::windows[1].size);
        mapped = h0 | h1;
        target = select(h0, ch_bool(false), h1);  // s0 优先（窗口重叠时）
    }

    // -------- round-robin 仲裁（2 master）：在途保持授权 + 指针指向无请求者时让位 --------
    // 请求实参是 `ch_in<ch_bool>`（端口）⇒ 用模板参数独立推导（与 `fwd2` 同理：
    // 直接写 `const ch_bool &` 会因「端口 → 基础类型」不可隐式转换而编译失败）。
    template <typename R0, typename R1>
    void arbitrate(const ch_bool &ptr, const R0 &req0, const R1 &req1,
                   const ch_bool &act0, const ch_bool &act1,
                   ch_bool &grant0, ch_bool &grant1) {
        // ptr == false ⇒ 优先 m0；ptr == true ⇒ 优先 m1
        const ch_bool prefer0 = !ptr;
        // 旋转到「以优先方为起点」的请求顺序
        const ch_bool q0 = select(prefer0, req0, req1);
        const ch_bool q1 = select(prefer0, req1, req0);
        // 旋转空间里的优先编码：q0 优先 ⇒ 优先方有请求时独占（**让位**在 q0=0 时发生）
        const ch_bool g0 = q0;
        const ch_bool g1 = select(q0, ch_bool(false), q1);
        // 旋转回真实 master 序号
        const ch_bool rr0 = select(prefer0, g0, g1);
        const ch_bool rr1 = select(prefer0, g1, g0);
        // 在途事务的 master MUST 保持授权（active 优先于轮询）
        const ch_bool any_act = act0 | act1;
        grant0 = select(any_act, act0, rr0);
        grant1 = select(any_act, act1, rr1);
    }

    void describe() override {
        MatrixState s;

        // ================= 1. 每 master 的**当拍**地址解码 =================
        // 写侧 MUST 用 awaddr、读侧 MUST 用 araddr（此前读通道复用写地址 ⇒ 读事务按写地址路由）
        ch_bool aw_target_m0, aw_target_m1, aw_mapped_m0, aw_mapped_m1;
        ch_bool ar_target_m0, ar_target_m1, ar_mapped_m0, ar_mapped_m1;
        decode_addr(io().m0_awaddr, aw_target_m0, aw_mapped_m0);
        decode_addr(io().m1_awaddr, aw_target_m1, aw_mapped_m1);
        decode_addr(io().m0_araddr, ar_target_m0, ar_mapped_m0);
        decode_addr(io().m1_araddr, ar_target_m1, ar_mapped_m1);

        // ================= 2. 仲裁（AW / AR 各自独立）=================
        // 请求取自该通道**自己的** valid；轮转只由该方向**事务真正完成**触发
        ch_bool aw_grant_m0, aw_grant_m1;
        arbitrate(s.aw_ptr, io().m0_awvalid, io().m1_awvalid,
                  s.wr_active_m0, s.wr_active_m1, aw_grant_m0, aw_grant_m1);
        ch_bool ar_grant_m0, ar_grant_m1;
        arbitrate(s.ar_ptr, io().m0_arvalid, io().m1_arvalid,
                  s.rd_active_m0, s.rd_active_m1, ar_grant_m0, ar_grant_m1);

        // ================= 3. error responder 的接受能力 =================
        const ch_bool err_w_can_accept = (!s.err_w_busy) & (!s.err_w_valid);
        const ch_bool err_r_can_accept = (!s.err_r_valid);

        // ================= 4. 转发选择 =================
        // 正向（AW / AR）：当拍地址解码 + 当拍 grant + 已映射
        const ch_bool aw_sel_s0_m0 = (!s.wr_active_m0) & aw_grant_m0 & aw_mapped_m0 & (!aw_target_m0);
        const ch_bool aw_sel_s1_m0 = (!s.wr_active_m0) & aw_grant_m0 & aw_mapped_m0 & aw_target_m0;
        const ch_bool aw_sel_s0_m1 = (!s.wr_active_m1) & aw_grant_m1 & aw_mapped_m1 & (!aw_target_m1);
        const ch_bool aw_sel_s1_m1 = (!s.wr_active_m1) & aw_grant_m1 & aw_mapped_m1 & aw_target_m1;
        const ch_bool ar_sel_s0_m0 = (!s.rd_active_m0) & ar_grant_m0 & ar_mapped_m0 & (!ar_target_m0);
        const ch_bool ar_sel_s1_m0 = (!s.rd_active_m0) & ar_grant_m0 & ar_mapped_m0 & ar_target_m0;
        const ch_bool ar_sel_s0_m1 = (!s.rd_active_m1) & ar_grant_m1 & ar_mapped_m1 & (!ar_target_m1);
        const ch_bool ar_sel_s1_m1 = (!s.rd_active_m1) & ar_grant_m1 & ar_mapped_m1 & ar_target_m1;

        // 反向（W / B 用写路由锁存；R 用读路由锁存）—— MUST NOT 用活的地址或活的 grant
        const ch_bool wr_sel_s0_m0 = s.wr_active_m0 & s.wr_mapped_m0 & (!s.wr_route_m0);
        const ch_bool wr_sel_s1_m0 = s.wr_active_m0 & s.wr_mapped_m0 & s.wr_route_m0;
        const ch_bool wr_sel_s0_m1 = s.wr_active_m1 & s.wr_mapped_m1 & (!s.wr_route_m1);
        const ch_bool wr_sel_s1_m1 = s.wr_active_m1 & s.wr_mapped_m1 & s.wr_route_m1;
        const ch_bool rd_sel_s0_m0 = s.rd_active_m0 & s.rd_mapped_m0 & (!s.rd_route_m0);
        const ch_bool rd_sel_s1_m0 = s.rd_active_m0 & s.rd_mapped_m0 & s.rd_route_m0;
        const ch_bool rd_sel_s0_m1 = s.rd_active_m1 & s.rd_mapped_m1 & (!s.rd_route_m1);
        const ch_bool rd_sel_s1_m1 = s.rd_active_m1 & s.rd_mapped_m1 & s.rd_route_m1;

        // ================= 5. 正向通道 =================
        // ---- AW ----
        fwd2(io().s0_awaddr, aw_sel_s0_m0, aw_sel_s0_m1, io().m0_awaddr, io().m1_awaddr,
             ch_uint<ADDR_WIDTH>(0_d));
        fwd2(io().s1_awaddr, aw_sel_s1_m0, aw_sel_s1_m1, io().m0_awaddr, io().m1_awaddr,
             ch_uint<ADDR_WIDTH>(0_d));
        io().s0_awvalid = (aw_sel_s0_m0 & io().m0_awvalid) | (aw_sel_s0_m1 & io().m1_awvalid);
        io().s1_awvalid = (aw_sel_s1_m0 & io().m0_awvalid) | (aw_sel_s1_m1 & io().m1_awvalid);
        fwd2(io().s0_awprot, aw_sel_s0_m0, aw_sel_s0_m1, io().m0_awprot, io().m1_awprot,
             ch_uint<2>(0_d));
        fwd2(io().s1_awprot, aw_sel_s1_m0, aw_sel_s1_m1, io().m0_awprot, io().m1_awprot,
             ch_uint<2>(0_d));

        // ---- W（按**锁存的写路由**转发）----
        fwd2(io().s0_wdata, wr_sel_s0_m0, wr_sel_s0_m1, io().m0_wdata, io().m1_wdata,
             ch_uint<DATA_WIDTH>(0_d));
        fwd2(io().s1_wdata, wr_sel_s1_m0, wr_sel_s1_m1, io().m0_wdata, io().m1_wdata,
             ch_uint<DATA_WIDTH>(0_d));
        fwd2(io().s0_wstrb, wr_sel_s0_m0, wr_sel_s0_m1, io().m0_wstrb, io().m1_wstrb,
             ch_uint<DATA_WIDTH / 8>(0_d));
        fwd2(io().s1_wstrb, wr_sel_s1_m0, wr_sel_s1_m1, io().m0_wstrb, io().m1_wstrb,
             ch_uint<DATA_WIDTH / 8>(0_d));
        io().s0_wvalid = (wr_sel_s0_m0 & io().m0_wvalid) | (wr_sel_s0_m1 & io().m1_wvalid);
        io().s1_wvalid = (wr_sel_s1_m0 & io().m0_wvalid) | (wr_sel_s1_m1 & io().m1_wvalid);

        // ---- AR ----
        fwd2(io().s0_araddr, ar_sel_s0_m0, ar_sel_s0_m1, io().m0_araddr, io().m1_araddr,
             ch_uint<ADDR_WIDTH>(0_d));
        fwd2(io().s1_araddr, ar_sel_s1_m0, ar_sel_s1_m1, io().m0_araddr, io().m1_araddr,
             ch_uint<ADDR_WIDTH>(0_d));
        io().s0_arvalid = (ar_sel_s0_m0 & io().m0_arvalid) | (ar_sel_s0_m1 & io().m1_arvalid);
        io().s1_arvalid = (ar_sel_s1_m0 & io().m0_arvalid) | (ar_sel_s1_m1 & io().m1_arvalid);
        fwd2(io().s0_arprot, ar_sel_s0_m0, ar_sel_s0_m1, io().m0_arprot, io().m1_arprot,
             ch_uint<2>(0_d));
        fwd2(io().s1_arprot, ar_sel_s1_m0, ar_sel_s1_m1, io().m0_arprot, io().m1_arprot,
             ch_uint<2>(0_d));

        // ================= 6. 握手决策（当拍地址 + 当拍 grant + 在途门控）=================
        // 在途期间 MUST 为 0 —— 否则 master 认为 AW/AR 被接受而互连悄悄吞掉（AW 静默丢失）
        const ch_bool m0_awready = (!s.wr_active_m0) & select(aw_grant_m0,
                                 select(aw_mapped_m0,
                                        idx2(aw_target_m0, io().s0_awready, io().s1_awready),
                                        err_w_can_accept),
                                 ch_bool(false));
        const ch_bool m1_awready = (!s.wr_active_m1) & select(aw_grant_m1,
                                 select(aw_mapped_m1,
                                        idx2(aw_target_m1, io().s0_awready, io().s1_awready),
                                        err_w_can_accept),
                                 ch_bool(false));
        io().m0_awready = m0_awready;
        io().m1_awready = m1_awready;

        const ch_bool m0_wready = s.wr_active_m0 & select(s.wr_mapped_m0,
                                idx2(s.wr_route_m0, io().s0_wready, io().s1_wready),
                                !s.err_w_valid);
        const ch_bool m1_wready = s.wr_active_m1 & select(s.wr_mapped_m1,
                                idx2(s.wr_route_m1, io().s0_wready, io().s1_wready),
                                !s.err_w_valid);
        io().m0_wready = m0_wready;
        io().m1_wready = m1_wready;

        const ch_bool m0_arready = (!s.rd_active_m0) & select(ar_grant_m0,
                                 select(ar_mapped_m0,
                                        idx2(ar_target_m0, io().s0_arready, io().s1_arready),
                                        err_r_can_accept),
                                 ch_bool(false));
        const ch_bool m1_arready = (!s.rd_active_m1) & select(ar_grant_m1,
                                 select(ar_mapped_m1,
                                        idx2(ar_target_m1, io().s0_arready, io().s1_arready),
                                        err_r_can_accept),
                                 ch_bool(false));
        io().m0_arready = m0_arready;
        io().m1_arready = m1_arready;

        // ================= 7. 反向通道：按**锁存路由**反查 =================
        const ch_uint<2> DECERR_V(static_cast<unsigned>(AxiResp::DECERR));

        // ---- m0 ----
        const ch_bool m0_bvalid = s.wr_active_m0 & select(s.wr_mapped_m0,
                                idx2(s.wr_route_m0, io().s0_bvalid, io().s1_bvalid),
                                s.err_w_valid);
        io().m0_bvalid = m0_bvalid;
        // ⚠️ 响应载荷 MUST 也按**在途标志**门控 —— 否则本 master 没有在途写事务时，
        //    `bresp` 仍会镜像某个从设备的当前值（可能是**另一个 master 正在读/写**
        //    的那个从设备）⇒ 表面上像「响应串到了别人身上」。
        //    契约（spec §「每个 slave 的响应 MUST 回到发起它的 master」）：
        //    该响应 MUST **只**出现在发起它的 master 的 `bvalid` / `bresp` 上。
        io().m0_bresp = select(s.wr_active_m0,
                               select(s.wr_mapped_m0,
                                      idx2(s.wr_route_m0, io().s0_bresp, io().s1_bresp),
                                      DECERR_V),
                               ch_uint<2>(0_d));
        const ch_bool m0_rvalid = s.rd_active_m0 & select(s.rd_mapped_m0,
                                idx2(s.rd_route_m0, io().s0_rvalid, io().s1_rvalid),
                                s.err_r_valid);
        io().m0_rvalid = m0_rvalid;
        io().m0_rdata = select(s.rd_active_m0,
                               select(s.rd_mapped_m0,
                                      idx2(s.rd_route_m0, io().s0_rdata, io().s1_rdata),
                                      ch_uint<DATA_WIDTH>(0_d)),
                               ch_uint<DATA_WIDTH>(0_d));
        io().m0_rresp = select(s.rd_active_m0,
                               select(s.rd_mapped_m0,
                                      idx2(s.rd_route_m0, io().s0_rresp, io().s1_rresp),
                                      DECERR_V),
                               ch_uint<2>(0_d));

        // ---- m1 ----
        const ch_bool m1_bvalid = s.wr_active_m1 & select(s.wr_mapped_m1,
                                idx2(s.wr_route_m1, io().s0_bvalid, io().s1_bvalid),
                                s.err_w_valid);
        io().m1_bvalid = m1_bvalid;
        io().m1_bresp = select(s.wr_active_m1,
                               select(s.wr_mapped_m1,
                                      idx2(s.wr_route_m1, io().s0_bresp, io().s1_bresp),
                                      DECERR_V),
                               ch_uint<2>(0_d));
        const ch_bool m1_rvalid = s.rd_active_m1 & select(s.rd_mapped_m1,
                                idx2(s.rd_route_m1, io().s0_rvalid, io().s1_rvalid),
                                s.err_r_valid);
        io().m1_rvalid = m1_rvalid;
        io().m1_rdata = select(s.rd_active_m1,
                               select(s.rd_mapped_m1,
                                      idx2(s.rd_route_m1, io().s0_rdata, io().s1_rdata),
                                      ch_uint<DATA_WIDTH>(0_d)),
                               ch_uint<DATA_WIDTH>(0_d));
        io().m1_rresp = select(s.rd_active_m1,
                               select(s.rd_mapped_m1,
                                      idx2(s.rd_route_m1, io().s0_rresp, io().s1_rresp),
                                      DECERR_V),
                               ch_uint<2>(0_d));

        // ---- 从设备侧的反向 ready（按**锁存路由**反查来源 master）----
        // ⚠️ MUST NOT 用**写**通道的 grant（旧实现用 `grant_m0_s0` ⇒ 读事务的
        // `rready` 走错通道 ⇒ 读响应永不被接受）。
        io().s0_bready = (wr_sel_s0_m0 & io().m0_bready) | (wr_sel_s0_m1 & io().m1_bready);
        io().s1_bready = (wr_sel_s1_m0 & io().m0_bready) | (wr_sel_s1_m1 & io().m1_bready);
        io().s0_rready = (rd_sel_s0_m0 & io().m0_rready) | (rd_sel_s0_m1 & io().m1_rready);
        io().s1_rready = (rd_sel_s1_m0 & io().m0_rready) | (rd_sel_s1_m1 & io().m1_rready);

        // ================= 8. 握手脉冲 =================
        const ch_bool aw_hs_m0 = io().m0_awvalid & m0_awready;
        const ch_bool aw_hs_m1 = io().m1_awvalid & m1_awready;
        const ch_bool ar_hs_m0 = io().m0_arvalid & m0_arready;
        const ch_bool ar_hs_m1 = io().m1_arvalid & m1_arready;
        const ch_bool b_hs_m0 = m0_bvalid & io().m0_bready;
        const ch_bool b_hs_m1 = m1_bvalid & io().m1_bready;
        const ch_bool r_hs_m0 = m0_rvalid & io().m0_rready;
        const ch_bool r_hs_m1 = m1_rvalid & io().m1_rready;

        // ================= 9. 写侧状态：路由锁存 + 在途标志 + 仲裁指针 =================
        s.wr_route_m0->next = select(!io().rst_n, ch_bool(false),
                              select(aw_hs_m0, aw_target_m0, s.wr_route_m0));
        s.wr_route_m1->next = select(!io().rst_n, ch_bool(false),
                              select(aw_hs_m1, aw_target_m1, s.wr_route_m1));
        s.wr_mapped_m0->next = select(!io().rst_n, ch_bool(false),
                               select(aw_hs_m0, aw_mapped_m0, s.wr_mapped_m0));
        s.wr_mapped_m1->next = select(!io().rst_n, ch_bool(false),
                               select(aw_hs_m1, aw_mapped_m1, s.wr_mapped_m1));
        s.wr_active_m0->next = select(!io().rst_n, ch_bool(false),
                               select(aw_hs_m0, ch_bool(true),
                               select(b_hs_m0, ch_bool(false), s.wr_active_m0)));
        s.wr_active_m1->next = select(!io().rst_n, ch_bool(false),
                               select(aw_hs_m1, ch_bool(true),
                               select(b_hs_m1, ch_bool(false), s.wr_active_m1)));

        // 轮转条件 MUST 是**该方向事务真正完成**
        const ch_bool wr_done = (s.wr_active_m0 & b_hs_m0) | (s.wr_active_m1 & b_hs_m1);
        s.aw_ptr->next = select(!io().rst_n, ch_bool(false),
                         select(wr_done, !s.aw_ptr, s.aw_ptr));

        // ================= 10. 读侧状态 =================
        s.rd_route_m0->next = select(!io().rst_n, ch_bool(false),
                              select(ar_hs_m0, ar_target_m0, s.rd_route_m0));
        s.rd_route_m1->next = select(!io().rst_n, ch_bool(false),
                              select(ar_hs_m1, ar_target_m1, s.rd_route_m1));
        s.rd_mapped_m0->next = select(!io().rst_n, ch_bool(false),
                               select(ar_hs_m0, ar_mapped_m0, s.rd_mapped_m0));
        s.rd_mapped_m1->next = select(!io().rst_n, ch_bool(false),
                               select(ar_hs_m1, ar_mapped_m1, s.rd_mapped_m1));
        s.rd_active_m0->next = select(!io().rst_n, ch_bool(false),
                               select(ar_hs_m0, ch_bool(true),
                               select(r_hs_m0, ch_bool(false), s.rd_active_m0)));
        s.rd_active_m1->next = select(!io().rst_n, ch_bool(false),
                               select(ar_hs_m1, ch_bool(true),
                               select(r_hs_m1, ch_bool(false), s.rd_active_m1)));

        const ch_bool rd_done = (s.rd_active_m0 & r_hs_m0) | (s.rd_active_m1 & r_hs_m1);
        s.ar_ptr->next = select(!io().rst_n, ch_bool(false),
                         select(rd_done, !s.ar_ptr, s.ar_ptr));

        // ================= 11. error responder（写）=================
        // 未映射写：互连层自己接受 AW/W 并返回 DECERR；`bvalid` MUST 是**寄存器**
        // 且保持到 `bready`（组合单拍脉冲会让合规 master 永久死锁）
        const ch_bool err_aw_hs = (aw_hs_m0 & (!aw_mapped_m0)) | (aw_hs_m1 & (!aw_mapped_m1));
        const ch_bool err_w_hs = (io().m0_wvalid & m0_wready & (!s.wr_mapped_m0)) |
                                 (io().m1_wvalid & m1_wready & (!s.wr_mapped_m1));
        const ch_bool err_b_hs = s.err_w_valid &
                                 ((s.wr_active_m0 & (!s.wr_mapped_m0) & io().m0_bready) |
                                  (s.wr_active_m1 & (!s.wr_mapped_m1) & io().m1_bready));
        s.err_w_busy->next = select(!io().rst_n, ch_bool(false),
                              select(err_aw_hs, ch_bool(true),
                              select(err_b_hs, ch_bool(false), s.err_w_busy)));
        s.err_w_valid->next = select(!io().rst_n, ch_bool(false),
                               select(err_w_hs, ch_bool(true),
                               select(err_b_hs, ch_bool(false), s.err_w_valid)));

        // ================= 12. error responder（读）=================
        // AXI4-Lite **无 arlen** ⇒ 恒返回 **1 个 beat**（每拍 rresp=DECERR）
        const ch_bool err_ar_hs = (ar_hs_m0 & (!ar_mapped_m0)) | (ar_hs_m1 & (!ar_mapped_m1));
        const ch_bool err_r_hs = s.err_r_valid &
                                 ((s.rd_active_m0 & (!s.rd_mapped_m0) & io().m0_rready) |
                                  (s.rd_active_m1 & (!s.rd_mapped_m1) & io().m1_rready));
        s.err_r_valid->next = select(!io().rst_n, ch_bool(false),
                               select(err_ar_hs, ch_bool(true),
                               select(err_r_hs, ch_bool(false), s.err_r_valid)));
    }
};

}  // namespace axi4
