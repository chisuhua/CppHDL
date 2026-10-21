/**
 * @file axi4_lite_slave.h
 * @brief 通用 AXI4-Lite Slave Controller（4 个寄存器）
 *
 * 规范形态（与 `include/axi4/peripherals/axi_gpio.h` 的四件套同源，
 * 见 `openspec/specs/axi-slave-conformance/spec.md`）：
 *
 *   * `rst_n`：低有效复位，**在时钟沿生效**（`select(!io().rst_n, <init>, ...)`
 *     落在 `ch_reg` 的 `next` 表达式上，由时钟沿采样）
 *   * AW→W **相位分离**：W 可在 AW 之后的任意周期被接受。
 *     注意 `aw_done` 是寄存器（表达式中读到的是**当前值**），因此
 *     **同拍 AW+W 时该拍 W 不被接受** —— master 必须保持 `WVALID` 直到
 *     `WREADY` 拉高（AXI 合规 master 的义务）。这与 6 个 AXI4-Lite peripheral
 *     及主 spec `dual-backend-test-coverage` 的「状态寄存器组与相位分离」一致。
 *   * **地址锁存**：AW/AR 握手时锁存地址；AXI 只要求地址保持到对应握手，
 *     握手后 master 即可改写 → 解码 MUST 基于锁存地址，否则写会落到错误寄存器
 *   * **响应保持**：`bvalid` 保持到 `BREADY`，`rvalid` 保持到 `RREADY`
 *     （组合单拍会让合规 master 永远等不到响应 → 死锁）
 *   * `busy` 只在事务**真正完成**（`b_valid && bready` / `r_valid && rready`）时释放
 *   * 未映射地址 → `DECERR`(0b11)；MUST NOT 别名到已实现寄存器。
 *     已映射但 `wstrb == 0` 的空写 → 响应仍为 `OKAY`(0b00)
 *   * 地址窗口：字节地址 `0x00..0x0C`（4 个寄存器），
 *     解码步长**固定 4 字节**（`addr >> 2`），与 `DATA_WIDTH` 无关
 */

#pragma once

#include "axi4/byte_lane.h"
#include "ch.hpp"
#include "component.h"

namespace axi4 {

using namespace ch::core;

template <unsigned ADDR_WIDTH = 32, unsigned DATA_WIDTH = 32, unsigned NUM_REGS = 4>
class AxiLiteSlave : public ch::Component {
public:
    // `NUM_REGS` 未被实现 honor（固定 4 个寄存器）——MUST 在编译期暴露，
    // MUST NOT 静默接受（仓库先例：axi_to_axilite.h:35 / byte_lane.h:42-43）
    static_assert(NUM_REGS == 4,
                  "AxiLiteSlave: NUM_REGS is not honoured — AxiLiteSlave implements "
                  "exactly 4 registers; use Axi4Slave for more.");

    __io(
        ch_in<ch_uint<ADDR_WIDTH>> awaddr;
        ch_in<ch_uint<2>> awprot;
        ch_in<ch_bool> awvalid;
        ch_out<ch_bool> awready;

        ch_in<ch_uint<DATA_WIDTH>> wdata;
        ch_in<ch_uint<DATA_WIDTH/8>> wstrb;
        ch_in<ch_bool> wvalid;
        ch_out<ch_bool> wready;

        ch_out<ch_uint<2>> bresp;
        ch_out<ch_bool> bvalid;
        ch_in<ch_bool> bready;

        ch_in<ch_uint<ADDR_WIDTH>> araddr;
        ch_in<ch_uint<2>> arprot;
        ch_in<ch_bool> arvalid;
        ch_out<ch_bool> arready;

        ch_out<ch_uint<DATA_WIDTH>> rdata;
        ch_out<ch_uint<2>> rresp;
        ch_out<ch_bool> rvalid;
        ch_in<ch_bool> rready;

        // 复位（低有效，在时钟沿生效）——必需端口，实例化点 MUST 显式接线
        ch_in<ch_bool> rst_n;
    )

    AxiLiteSlave(ch::Component* parent = nullptr, const std::string& name = "axi_lite_slave")
        : ch::Component(parent, name) {}

    void create_ports() override {
        new (io_storage_) io_type;
    }

    void describe() override {
        // ---------------- 状态寄存器组（busy / aw_done / b_valid / r_valid）----------------
        ch_reg<ch_bool> busy(ch_bool(false));
        ch_reg<ch_bool> aw_done(ch_bool(false));   // AW 已接收，等待 W
        ch_reg<ch_bool> b_valid(ch_bool(false));   // W 已接收，B 响应待取
        ch_reg<ch_bool> r_valid(ch_bool(false));   // AR 已接收，R 响应待取

        // ---------------- 地址锁存寄存器 ----------------
        // AXI 只要求 AWADDR/ARADDR 保持到对应握手，握手后 master 即可改写它们；
        // W 可能延后多个周期才到达 → 地址必须在握手时锁存。
        ch_reg<ch_uint<ADDR_WIDTH>> addr_reg(0_d);
        ch_reg<ch_uint<ADDR_WIDTH>> ar_addr_reg(0_d);

        // ---------------- 寄存器文件 ----------------
        ch_reg<ch_uint<DATA_WIDTH>> reg0(0_d);
        ch_reg<ch_uint<DATA_WIDTH>> reg1(0_d);
        ch_reg<ch_uint<DATA_WIDTH>> reg2(0_d);
        ch_reg<ch_uint<DATA_WIDTH>> reg3(0_d);

        // ================= 写通道 =================
        // 写地址握手：busy 期间不接受新的 AW
        auto aw_handshake = select(io().awvalid, select(busy, ch_bool(false), ch_bool(true)), ch_bool(false));
        io().awready = aw_handshake;

        // 写数据握手：AW 完成后才接受 W（master 可延后一个或多个周期送 W）
        auto w_handshake = select(io().wvalid, aw_done, ch_bool(false));
        io().wready = w_handshake;

        // 写地址解码（步长固定 4 字节，基于**锁存**地址）
        // 窗口 = 字节地址 0x00..0x0C；窗口外 → DECERR（MUST NOT 用掩码截断，
        // 那会把 0x100 之类的地址别名到 reg0）
        auto addr_index = addr_reg >> ch_uint<ADDR_WIDTH>(2_d);
        auto sel0 = (addr_index == ch_uint<ADDR_WIDTH>(0_d));
        auto sel1 = (addr_index == ch_uint<ADDR_WIDTH>(1_d));
        auto sel2 = (addr_index == ch_uint<ADDR_WIDTH>(2_d));
        auto sel3 = (addr_index == ch_uint<ADDR_WIDTH>(3_d));
        auto write_mapped = sel0 | sel1 | sel2 | sel3;

        // 写响应：未映射地址 → DECERR(0b11)
        // （值的有效期仅限 bvalid == 1 的拍）
        io().bresp = select(write_mapped, ch_uint<2>(0_d), ch_uint<2>(3_d));

        // 写响应保持：b_valid 置位后保持到 BREADY
        io().bvalid = select(b_valid, ch_bool(true), ch_bool(false));

        // 写寄存器
        auto we0 = select(w_handshake, sel0, ch_bool(false));
        auto we1 = select(w_handshake, sel1, ch_bool(false));
        auto we2 = select(w_handshake, sel2, ch_bool(false));
        auto we3 = select(w_handshake, sel3, ch_bool(false));

        // 字节写：WSTRB 逐字节展开（语义见 include/axi4/AGENTS.md 字节道语义）
        const auto wstrb_mask =
            axi4::detail::byte_lane_mask<DATA_WIDTH>(io().wstrb);

        reg0->next = select(
            !io().rst_n, ch_uint<DATA_WIDTH>(0_d),
            select(we0,
                   axi4::detail::byte_lane_apply<DATA_WIDTH, DATA_WIDTH>(reg0, io().wdata, wstrb_mask),
                   reg0));
        reg1->next = select(
            !io().rst_n, ch_uint<DATA_WIDTH>(0_d),
            select(we1,
                   axi4::detail::byte_lane_apply<DATA_WIDTH, DATA_WIDTH>(reg1, io().wdata, wstrb_mask),
                   reg1));
        reg2->next = select(
            !io().rst_n, ch_uint<DATA_WIDTH>(0_d),
            select(we2,
                   axi4::detail::byte_lane_apply<DATA_WIDTH, DATA_WIDTH>(reg2, io().wdata, wstrb_mask),
                   reg2));
        reg3->next = select(
            !io().rst_n, ch_uint<DATA_WIDTH>(0_d),
            select(we3,
                   axi4::detail::byte_lane_apply<DATA_WIDTH, DATA_WIDTH>(reg3, io().wdata, wstrb_mask),
                   reg3));

        // ================= 读通道 =================
        auto ar_handshake = select(io().arvalid, select(busy, ch_bool(false), ch_bool(true)), ch_bool(false));
        io().arready = ar_handshake;

        // 读地址解码（基于**锁存**地址）
        auto ar_addr_index = ar_addr_reg >> ch_uint<ADDR_WIDTH>(2_d);
        auto read_sel0 = (ar_addr_index == ch_uint<ADDR_WIDTH>(0_d));
        auto read_sel1 = (ar_addr_index == ch_uint<ADDR_WIDTH>(1_d));
        auto read_sel2 = (ar_addr_index == ch_uint<ADDR_WIDTH>(2_d));
        auto read_sel3 = (ar_addr_index == ch_uint<ADDR_WIDTH>(3_d));
        auto read_mapped = read_sel0 | read_sel1 | read_sel2 | read_sel3;

        // 读响应：未映射地址 → DECERR(0b11)
        io().rresp = select(read_mapped, ch_uint<2>(0_d), ch_uint<2>(3_d));

        // 读数据（未映射地址 → 0，MUST NOT 呈现其它寄存器的值）
        ch_uint<DATA_WIDTH> read_val(0_d);
        read_val = select(read_sel0, reg0, read_val);
        read_val = select(read_sel1, reg1, read_val);
        read_val = select(read_sel2, reg2, read_val);
        read_val = select(read_sel3, reg3, read_val);
        io().rdata = read_val;

        // 读响应保持：r_valid 置位后保持到 RREADY
        io().rvalid = select(r_valid, ch_bool(true), ch_bool(false));

        // ================= 状态更新 =================
        // 事务真正完成的条件（不是裸 bready/rready）
        auto write_done = select(b_valid, io().bready, ch_bool(false));
        auto read_done = select(r_valid, io().rready, ch_bool(false));

        // ⚠️ 置 1 条件 MUST 优先于清 0 条件：
        // 若写成清 0 优先，bready 常态拉高时 b_valid 永远为 0 →
        // bvalid 永不拉高（master 死等）且 aw_done 永不清零（同一笔 W 被反复接受）。
        busy->next = select(!io().rst_n, ch_bool(false),
                            select(aw_handshake, ch_bool(true),
                                   select(ar_handshake, ch_bool(true),
                                          select(write_done, ch_bool(false),
                                                 select(read_done, ch_bool(false), busy)))));
        // aw_done 仅在 B 握手完成时清零
        // （若在裸 bready 上清零，master 常态拉高 BREADY 时 W 永远不会被接受）
        aw_done->next = select(!io().rst_n, ch_bool(false),
                               select(aw_handshake, ch_bool(true),
                                      select(write_done, ch_bool(false), aw_done)));
        b_valid->next = select(!io().rst_n, ch_bool(false),
                               select(w_handshake, ch_bool(true),
                                      select(io().bready, ch_bool(false), b_valid)));
        r_valid->next = select(!io().rst_n, ch_bool(false),
                               select(ar_handshake, ch_bool(true),
                                      select(io().rready, ch_bool(false), r_valid)));
        // 地址锁存（AW / AR 握手时采样）
        addr_reg->next = select(!io().rst_n, ch_uint<ADDR_WIDTH>(0_d),
                                select(aw_handshake, io().awaddr, addr_reg));
        ar_addr_reg->next = select(!io().rst_n, ch_uint<ADDR_WIDTH>(0_d),
                                   select(ar_handshake, io().araddr, ar_addr_reg));
    }
};

} // namespace axi4
