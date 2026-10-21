/**
 * @file axi4_full.h
 * @brief AXI4 全功能从设备控制器（5 通道 + 多 beat burst）
 *
 * 规范形态（与 `include/axi4/axi4_lite_slave.h` 的 `AxiLiteSlave` 及 6 个
 * AXI4-Lite peripheral **同源**，契约见
 * `openspec/specs/axi-slave-conformance/spec.md` 与
 * `openspec/specs/axi4-full-slave-burst/spec.md`）：
 *
 *   * `rst_n`：低有效复位，**在时钟沿生效**
 *     （`select(!io().rst_n, <init_val>, ...)` 落在 `ch_reg` 的 `next`
 *     表达式上、由时钟沿采样 —— **不是**异步复位）。
 *     ⚠️ 未接线的 `ch_in<ch_bool>` 读出为 0 = **永久复位**
 *     （零编译错误的静默破坏）→ 实例化点 MUST 显式接线。
 *   * AW→W **相位分离**：W 可在 AW 之后的任意周期被接受。
 *     ⚠️ `aw_done` 是寄存器（表达式中读到的是**当前值**），因此
 *     **同拍 AW+W 时该拍 W 不被接受** —— master MUST 保持 `WVALID`
 *     到 `WREADY`（AXI 合规 master 的义务）。这是规范形态的固有性质，
 *     **不是缺陷**。
 *   * **地址锁存**：AW/AR 握手时锁存地址，解码 MUST 基于**锁存**地址
 *     （AXI 只要求地址保持到对应握手，握手后 master 即可改写）。
 *   * **响应保持**：`bvalid` 保持到 `bready`，`rvalid` 保持到 `rready`。
 *   * 写 / 读方向**各自独立** outstanding：AXI4 的 AW/W/B 与 AR/R 是
 *     独立通道组，AXI4 允许读写并发 → **不使用**共享 `busy`
 *     （把本组件降级为读写互斥属于行为回退）。
 *   * 未映射地址 → `DECERR`(0b11)；burst 长度不符 / 不支持的 burst 类型
 *     → `SLVERR`(0b10)。
 *
 * burst 语义：
 *   * `awlen` / `arlen` 是「beat 数 − 1」的 AXI 编码 → 一笔事务 = `len + 1` 个 beat
 *   * `INCR`：每 beat 地址前进 `2^awsize` / `2^arsize` 字节
 *     （**查表**实现，不用运行期移位 —— `ch_uint::operator<<` 按右操作数的
 *     类型宽度推算结果宽度，运行期移位量会把结果宽度算错）
 *   * `FIXED`：每 beat 地址不变
 *   * `WRAP`：**不支持** → 以 `SLVERR` 明确拒绝（MUST NOT 静默当作 INCR）
 *   * 每一拍按**当拍** `wstrb` 重新展开字节道掩码（MUST NOT 在 AW 拍锁存）
 *   * ⚠️ **窄传输未实现**：`DATA_WIDTH == 64` 且 `awsize < 3` 时，
 *     相邻 beat 落到**同一个** 64 位寄存器（字节道重编号不在本组件范围）
 *
 * 限制：
 *   * 数据位宽 ≤ 64（`byte_lane.h` 封顶 64 + ADR-035 R3 的 Verilator 端口上限）
 *   * 寄存器数量固定 16（`NUM_REGS` 未 honor → `static_assert` 编译期暴露）
 *   * 最大 burst 长度 ≤ 16（`awlen` / `arlen` 端口宽度 4 bit 的物理上限）
 */

#pragma once

#include "axi4/byte_lane.h"
#include "ch.hpp"
#include "component.h"

#include <cstdint>

namespace axi4 {

using namespace ch::core;

#ifndef AXI4_AXIRESP_DEFINED
#define AXI4_AXIRESP_DEFINED
enum class AxiResp : uint8_t {
    OKAY   = 0b00,
    EXOKAY = 0b01,
    SLVERR = 0b10,
    DECERR = 0b11
};
#endif

enum class AxiBurst : uint8_t {
    FIXED = 0b00,
    INCR  = 0b01,
    WRAP  = 0b10
};

template <
    unsigned ADDR_WIDTH = 32,
    unsigned DATA_WIDTH = 32,
    unsigned ID_WIDTH = 4,
    unsigned NUM_REGS = 16,
    unsigned MAX_BURST_LEN = 16
>
class Axi4Slave : public ch::Component {
public:
    static constexpr unsigned NUM_BYTES = DATA_WIDTH / 8;

    // 实现固定提供 16 个寄存器（reg0..reg15）
    static constexpr unsigned NUM_IMPL_REGS = 16;

    // ---- 模板参数 honor 化：未被 honor 的参数 MUST 在编译期暴露 ----
    // NUM_REGS 未 honor（固定 16 个寄存器）—— 与 `AxiLiteSlave` 同源的处理：
    // 用 static_assert 暴露，不静默接受（见 axi4-full-slave-burst spec 的
    // 「模板参数 MUST 被 honor」Requirement）。
    static_assert(NUM_REGS == NUM_IMPL_REGS,
                  "Axi4Slave: NUM_REGS is not honoured — the implementation "
                  "provides exactly 16 registers (reg0..reg15).");
    // MAX_BURST_LEN 真正生效（burst 长度上限，超限 → SLVERR）；
    // awlen/arlen 是 4 bit 端口 → 16 beat 是物理上限。
    static_assert(MAX_BURST_LEN >= 1 && MAX_BURST_LEN <= 16,
                  "Axi4Slave: MAX_BURST_LEN MUST be in [1, 16] (awlen/arlen "
                  "are 4-bit ports => 16 beats is the physical maximum).");
    // DATA_WIDTH ≤ 64：byte_lane.h 封顶 64 + Verilator 后端拒 >64 bit 端口
    // （ADR-035 R3）。
    static_assert(DATA_WIDTH >= 8 && DATA_WIDTH <= 64,
                  "Axi4Slave: DATA_WIDTH MUST be in [8, 64] (byte_lane.h caps "
                  "at 64; the Verilator backend rejects wider ports, "
                  "ADR-035 R3).");

    __io(
        ch_in<ch_uint<ADDR_WIDTH>> awaddr;
        ch_in<ch_uint<ID_WIDTH>> awid;
        ch_in<ch_uint<4>> awlen;
        ch_in<ch_uint<3>> awsize;
        ch_in<ch_uint<2>> awburst;
        ch_in<ch_bool> awlock;
        ch_in<ch_uint<4>> awcache;
        ch_in<ch_uint<2>> awprot;
        ch_in<ch_bool> awvalid;
        ch_out<ch_bool> awready;

        ch_in<ch_uint<DATA_WIDTH>> wdata;
        ch_in<ch_uint<ID_WIDTH>> wid;
        ch_in<ch_uint<NUM_BYTES>> wstrb;
        ch_in<ch_bool> wlast;
        ch_in<ch_bool> wvalid;
        ch_out<ch_bool> wready;

        ch_out<ch_uint<ID_WIDTH>> bid;
        ch_out<ch_uint<2>> bresp;
        ch_out<ch_bool> bvalid;
        ch_in<ch_bool> bready;

        ch_in<ch_uint<ADDR_WIDTH>> araddr;
        ch_in<ch_uint<ID_WIDTH>> arid;
        ch_in<ch_uint<4>> arlen;
        ch_in<ch_uint<3>> arsize;
        ch_in<ch_uint<2>> arburst;
        ch_in<ch_bool> arlock;
        ch_in<ch_uint<4>> arcache;
        ch_in<ch_uint<2>> arprot;
        ch_in<ch_bool> arvalid;
        ch_out<ch_bool> arready;

        ch_out<ch_uint<DATA_WIDTH>> rdata;
        ch_out<ch_uint<ID_WIDTH>> rid;
        ch_out<ch_uint<2>> rresp;
        ch_out<ch_bool> rlast;
        ch_out<ch_bool> rvalid;
        ch_in<ch_bool> rready;

        // 复位（低有效，在时钟沿生效）—— 必需端口，实例化点 MUST 显式接线
        ch_in<ch_bool> rst_n;
    )

    Axi4Slave(ch::Component* parent = nullptr, const std::string& name = "axi4_slave")
        : ch::Component(parent, name) {}

    void create_ports() override {
        new (io_storage_) io_type;
    }

    // 全部状态寄存器（复位 pattern 统一由 describe() 内的
    // `select(!io().rst_n, <init_val>, ...)` 给出）。
    //
    // ⚠️ 每个 ch_reg MUST 有且仅有一处 `->next` 赋值，且 MUST 包上复位 pattern
    //    —— 漏一个 = 该寄存器永远停在初值（静默破坏）。
    struct SlaveState {
        // ---- 写通道 ----
        ch_reg<ch_bool> aw_done{ch_bool(false)};                 // AW 已接收，等待 W beat
        ch_reg<ch_bool> write_done{ch_bool(false)};              // burst 已结束（wlast / beat 数用尽）
        ch_reg<ch_bool> b_valid{ch_bool(false)};                 // B 响应待取
        ch_reg<ch_uint<ADDR_WIDTH>> write_addr_cur{ch_uint<ADDR_WIDTH>(0_d)}; // 当前 beat 地址（AW 握手时锁存）
        ch_reg<ch_uint<4>> write_len_reg{ch_uint<4>(0_d)};       // 锁存的 awlen（beat 数 − 1）
        ch_reg<ch_uint<3>> write_size_reg{ch_uint<3>(0_d)};      // 锁存的 awsize
        ch_reg<ch_uint<2>> write_burst_reg{ch_uint<2>(0_d)};     // 锁存的 awburst
        ch_reg<ch_uint<ID_WIDTH>> write_id_reg{ch_uint<ID_WIDTH>(0_d)}; // 锁存的 awid
        ch_reg<ch_uint<5>> write_beat_cnt{ch_uint<5>(0_d)};      // 已接受的 W beat 数（0..16）
        ch_reg<ch_bool> write_rejected{ch_bool(false)};          // SLVERR：burst 类型不支持 / 超长
        ch_reg<ch_bool> write_len_err{ch_bool(false)};           // SLVERR：wlast 与 awlen 不符
        ch_reg<ch_bool> write_decerr{ch_bool(false)};            // DECERR：某 beat 落在窗口外

        // ---- 读通道 ----
        ch_reg<ch_bool> r_valid{ch_bool(false)};                 // AR 已接收，R beat 流式输出
        ch_reg<ch_uint<ADDR_WIDTH>> read_addr_cur{ch_uint<ADDR_WIDTH>(0_d)}; // 当前 beat 地址（AR 握手时锁存）
        ch_reg<ch_uint<4>> read_len_reg{ch_uint<4>(0_d)};        // 锁存的 arlen（beat 数 − 1）
        ch_reg<ch_uint<3>> read_size_reg{ch_uint<3>(0_d)};       // 锁存的 arsize
        ch_reg<ch_uint<2>> read_burst_reg{ch_uint<2>(0_d)};      // 锁存的 arburst
        ch_reg<ch_uint<ID_WIDTH>> read_id_reg{ch_uint<ID_WIDTH>(0_d)}; // 锁存的 arid
        ch_reg<ch_uint<5>> read_beat_cnt{ch_uint<5>(0_d)};       // 已接受的 R beat 数（0..16）
        ch_reg<ch_bool> read_rejected{ch_bool(false)};           // SLVERR：burst 类型不支持 / 超长
        ch_reg<ch_bool> read_decerr{ch_bool(false)};             // DECERR：某 beat 落在窗口外

        // ---- 寄存器文件 ----
        ch_reg<ch_uint<DATA_WIDTH>> reg0{ch_uint<DATA_WIDTH>(0_d)};
        ch_reg<ch_uint<DATA_WIDTH>> reg1{ch_uint<DATA_WIDTH>(0_d)};
        ch_reg<ch_uint<DATA_WIDTH>> reg2{ch_uint<DATA_WIDTH>(0_d)};
        ch_reg<ch_uint<DATA_WIDTH>> reg3{ch_uint<DATA_WIDTH>(0_d)};
        ch_reg<ch_uint<DATA_WIDTH>> reg4{ch_uint<DATA_WIDTH>(0_d)};
        ch_reg<ch_uint<DATA_WIDTH>> reg5{ch_uint<DATA_WIDTH>(0_d)};
        ch_reg<ch_uint<DATA_WIDTH>> reg6{ch_uint<DATA_WIDTH>(0_d)};
        ch_reg<ch_uint<DATA_WIDTH>> reg7{ch_uint<DATA_WIDTH>(0_d)};
        ch_reg<ch_uint<DATA_WIDTH>> reg8{ch_uint<DATA_WIDTH>(0_d)};
        ch_reg<ch_uint<DATA_WIDTH>> reg9{ch_uint<DATA_WIDTH>(0_d)};
        ch_reg<ch_uint<DATA_WIDTH>> reg10{ch_uint<DATA_WIDTH>(0_d)};
        ch_reg<ch_uint<DATA_WIDTH>> reg11{ch_uint<DATA_WIDTH>(0_d)};
        ch_reg<ch_uint<DATA_WIDTH>> reg12{ch_uint<DATA_WIDTH>(0_d)};
        ch_reg<ch_uint<DATA_WIDTH>> reg13{ch_uint<DATA_WIDTH>(0_d)};
        ch_reg<ch_uint<DATA_WIDTH>> reg14{ch_uint<DATA_WIDTH>(0_d)};
        ch_reg<ch_uint<DATA_WIDTH>> reg15{ch_uint<DATA_WIDTH>(0_d)};
    };

    // ==================== 写通道（AW / W / B） ====================
    void build_write_channel(SlaveState &s) {
        // ---- AW 握手 ----
        // aw_done 为 0（无在途写事务）时接受新的 AW；
        // aw_done 在 B 握手完成前保持 → 写方向单 outstanding。
        auto aw_handshake = select(io().awvalid,
                                   select(s.aw_done, ch_bool(false), ch_bool(true)),
                                   ch_bool(false));
        io().awready = aw_handshake;

        // ---- W 握手（AW 之后的任意周期都可接受）----
        // ⚠️ aw_done 是寄存器 → 同拍 AW+W 时该拍 W 不被接受（AXI 合规 master
        //    的义务是保持 WVALID 到 WREADY）。
        // ⚠️ write_done 是寄存器 → burst 结束（wlast 到达 / beat 数用尽）后
        //    wready 撤销，本笔事务**不再接受**额外的 W beat
        //    （否则越界 beat 会静默写入寄存器）。
        auto w_handshake = select(io().wvalid,
                                  select(s.aw_done, select(s.write_done, ch_bool(false), ch_bool(true)),
                                         ch_bool(false)),
                                  ch_bool(false));
        io().wready = w_handshake;
        w_handshake_ = w_handshake;

        // ---- burst 元数据裁决（AW 握手拍，用**活**的 awburst / awlen）----
        // WRAP(0b10) 与保留编码(0b11) 不支持 → 明确拒绝（MUST NOT 静默当 INCR）
        auto burst_unsupported = select(
            io().awburst == ch_uint<2>(static_cast<unsigned>(AxiBurst::WRAP)),
            ch_bool(true),
            select(io().awburst == ch_uint<2>(3_d), ch_bool(true), ch_bool(false)));
        auto aw_beats_total = ch_uint<5>(io().awlen + ch_uint<5>(1_d));
        auto too_long = (aw_beats_total > ch_uint<5>(MAX_BURST_LEN));
        auto aw_reject = select(aw_handshake,
                                select(burst_unsupported | too_long, ch_bool(true), ch_bool(false)),
                                ch_bool(false));

        // ---- 当前 beat 的地址与解码（基于**锁存**地址）----
        decode_word(s.write_addr_cur, write_sel_, write_mapped_);

        // ---- 步长与地址推进 ----
        auto stride = burst_stride(s.write_size_reg);
        auto is_incr = (s.write_burst_reg ==
                        ch_uint<2>(static_cast<unsigned>(AxiBurst::INCR)));
        auto advance = select(is_incr, stride, ch_uint<ADDR_WIDTH>(0_d));

        // ---- beat 计数（**下一个值**）----
        // ⚠️ 「本拍是否为末拍」MUST 用下一个值判定（MUST NOT 用当前寄存器值
        //    比较 —— 仓库已登记该坑，见 include/axi4/AGENTS.md ANTI-PATTERNS）。
        auto next_cnt = select(w_handshake,
                                ch_uint<5>(s.write_beat_cnt + ch_uint<5>(1_d)),
                                ch_uint<5>(s.write_beat_cnt));
        auto beats_total = ch_uint<5>(s.write_len_reg + ch_uint<5>(1_d));
        auto expected_last = (next_cnt == beats_total);

        // burst 结束：预期末拍到达，或 master 提前拉了 wlast
        auto burst_end = select(w_handshake,
                                select(expected_last, ch_bool(true), io().wlast),
                                ch_bool(false));
        // 长度不符：**wlast 与预期末拍不一致**（提前 / 缺失）→ 异或
        //   - wlast 提前到达（wlast=1 但本拍不是预期末拍）
        //   - wlast 缺失（预期末拍到了但 wlast=0）
        auto len_mismatch = select(w_handshake,
                                   select(expected_last,
                                          select(io().wlast, ch_bool(false), ch_bool(true)),
                                          select(io().wlast, ch_bool(true), ch_bool(false))),
                                   ch_bool(false));

        // ---- B 响应 ----
        auto b_handshake = select(s.b_valid, io().bready, ch_bool(false));
        b_handshake_ = b_handshake;

        // ==================== 状态寄存器更新 ====================
        // aw_done：AW 握手置位，B 握手完成清零
        s.aw_done->next = select(!io().rst_n, ch_bool(false),
                                 select(aw_handshake, ch_bool(true),
                                        select(b_handshake, ch_bool(false), s.aw_done)));
        // write_done：burst 结束置位（AW 握手时清零）
        // → 之后的 W beat 不再被接受（wready 撤销），避免越界 beat 静默写入
        s.write_done->next = select(!io().rst_n, ch_bool(false),
                                    select(aw_handshake, ch_bool(false),
                                           select(burst_end, ch_bool(true), s.write_done)));
        // b_valid：burst 结束置位，B 握手清零
        // ⚠️ 置 1 条件 MUST 优先于清 0 条件（bready 常态拉高时否则 bvalid 永不拉高）
        s.b_valid->next = select(!io().rst_n, ch_bool(false),
                                 select(burst_end, ch_bool(true),
                                        select(b_handshake, ch_bool(false), s.b_valid)));
        // 当前 beat 地址：AW 握手时锁存，每个被接受的 W beat 后推进
        s.write_addr_cur->next = select(!io().rst_n, ch_uint<ADDR_WIDTH>(0_d),
                                        select(aw_handshake, io().awaddr,
                                               select(w_handshake,
                                                      ch_uint<ADDR_WIDTH>(s.write_addr_cur + advance),
                                                      s.write_addr_cur)));
        // burst 元数据锁存
        s.write_len_reg->next = select(!io().rst_n, ch_uint<4>(0_d),
                                        select(aw_handshake, io().awlen, s.write_len_reg));
        s.write_size_reg->next = select(!io().rst_n, ch_uint<3>(0_d),
                                        select(aw_handshake, io().awsize, s.write_size_reg));
        s.write_burst_reg->next = select(!io().rst_n, ch_uint<2>(0_d),
                                         select(aw_handshake, io().awburst, s.write_burst_reg));
        s.write_id_reg->next = select(!io().rst_n, ch_uint<ID_WIDTH>(0_d),
                                      select(aw_handshake, io().awid, s.write_id_reg));
        // beat 计数：AW 握手清零，每个被接受的 W beat +1
        s.write_beat_cnt->next = select(!io().rst_n, ch_uint<5>(0_d),
                                        select(aw_handshake, ch_uint<5>(0_d),
                                               select(w_handshake,
                                                      ch_uint<5>(s.write_beat_cnt + ch_uint<5>(1_d)),
                                                      s.write_beat_cnt)));
        // 错误标志：AW 握手时裁决的类型 / 长度问题；B 握手后清除
        s.write_rejected->next = select(!io().rst_n, ch_bool(false),
                                        select(aw_reject, ch_bool(true),
                                               select(b_handshake, ch_bool(false), s.write_rejected)));
        // 长度不符：已写入的 beat MUST NOT 被回滚（部分写入 + 错误响应是 AXI 允许的行为）
        s.write_len_err->next = select(!io().rst_n, ch_bool(false),
                                       select(len_mismatch, ch_bool(true),
                                              select(b_handshake, ch_bool(false), s.write_len_err)));
        // 越界 beat：该 beat 被抑制（寄存器保持原值），事务以 DECERR 结束
        s.write_decerr->next = select(!io().rst_n, ch_bool(false),
                                      select(select(w_handshake, !write_mapped_, ch_bool(false)),
                                             ch_bool(true),
                                             select(b_handshake, ch_bool(false), s.write_decerr)));
    }

    // ==================== 读通道（AR / R） ====================
    void build_read_channel(SlaveState &s) {
        // ---- AR 握手 ----
        // r_valid 在最后一个 R beat 被接受前保持 → 读方向单 outstanding
        auto ar_handshake = select(io().arvalid,
                                   select(s.r_valid, ch_bool(false), ch_bool(true)),
                                   ch_bool(false));
        io().arready = ar_handshake;

        // ---- R 握手 ----
        auto r_handshake = select(s.r_valid, io().rready, ch_bool(false));

        // ---- burst 元数据裁决（AR 握手拍，用**活**的 arburst / arlen）----
        auto burst_unsupported = select(
            io().arburst == ch_uint<2>(static_cast<unsigned>(AxiBurst::WRAP)),
            ch_bool(true),
            select(io().arburst == ch_uint<2>(3_d), ch_bool(true), ch_bool(false)));
        auto ar_beats_total = ch_uint<5>(io().arlen + ch_uint<5>(1_d));
        auto too_long = (ar_beats_total > ch_uint<5>(MAX_BURST_LEN));
        auto ar_reject = select(ar_handshake,
                                select(burst_unsupported | too_long, ch_bool(true), ch_bool(false)),
                                ch_bool(false));

        // ---- 当前 beat 的地址与解码（基于**锁存**地址）----
        decode_word(s.read_addr_cur, read_sel_, read_mapped_);

        // ---- 读 mux（每一拍取**该拍对应地址**的寄存器值）----
        ch_uint<DATA_WIDTH> read_val(0_d);
        read_val = select(read_sel_[0], s.reg0, read_val);
        read_val = select(read_sel_[1], s.reg1, read_val);
        read_val = select(read_sel_[2], s.reg2, read_val);
        read_val = select(read_sel_[3], s.reg3, read_val);
        read_val = select(read_sel_[4], s.reg4, read_val);
        read_val = select(read_sel_[5], s.reg5, read_val);
        read_val = select(read_sel_[6], s.reg6, read_val);
        read_val = select(read_sel_[7], s.reg7, read_val);
        read_val = select(read_sel_[8], s.reg8, read_val);
        read_val = select(read_sel_[9], s.reg9, read_val);
        read_val = select(read_sel_[10], s.reg10, read_val);
        read_val = select(read_sel_[11], s.reg11, read_val);
        read_val = select(read_sel_[12], s.reg12, read_val);
        read_val = select(read_sel_[13], s.reg13, read_val);
        read_val = select(read_sel_[14], s.reg14, read_val);
        read_val = select(read_sel_[15], s.reg15, read_val);

        // ---- 末拍判定 ----
        // `rdata` 与 `rlast` 由**同一个** beat 索引（read_beat_cnt）驱动：
        // rdata 取 read_addr_cur 对应的寄存器，rlast 标记该索引是否为末拍。
        // ⚠️ 这里用「当前值」是**正确**的：若改用「下一个值」判定，
        //    在 rready=0（末拍尚未被 master 接受）时会漏掉 rlast。
        //    「下一个值」规则针对的是「用陈旧寄存器值判定 FSM 结束」——
        //    读侧不存在那种 FSM（流式输出，每拍一个 beat）。
        auto last_beat = (s.read_beat_cnt == s.read_len_reg);
        auto burst_end = select(r_handshake, last_beat, ch_bool(false));

        // ---- 步长与地址推进 ----
        auto stride = burst_stride(s.read_size_reg);
        auto is_incr = (s.read_burst_reg ==
                        ch_uint<2>(static_cast<unsigned>(AxiBurst::INCR)));
        auto advance = select(is_incr, stride, ch_uint<ADDR_WIDTH>(0_d));

        // ---- R 响应 ----
        auto unmapped = !read_mapped_;
        auto slverr = select(s.read_rejected, ch_bool(true), ch_bool(false));
        auto decerr = select(unmapped, ch_bool(true),
                             select(s.read_decerr, ch_bool(true), ch_bool(false)));

        io().rvalid = s.r_valid;
        io().rid = s.read_id_reg;
        io().rlast = select(s.r_valid, last_beat, ch_bool(false));
        // 越界 beat → rdata = 0（MUST NOT 呈现其它寄存器的值）
        io().rdata = select(slverr, ch_uint<DATA_WIDTH>(0_d),
                            select(unmapped, ch_uint<DATA_WIDTH>(0_d), read_val));
        io().rresp = select(slverr, ch_uint<2>(static_cast<unsigned>(AxiResp::SLVERR)),
                            select(decerr, ch_uint<2>(static_cast<unsigned>(AxiResp::DECERR)),
                                   ch_uint<2>(static_cast<unsigned>(AxiResp::OKAY))));

        // ==================== 状态寄存器更新 ====================
        // r_valid：AR 握手置位，末拍被接受后清零（流式：整个 burst 期间保持为 1）
        s.r_valid->next = select(!io().rst_n, ch_bool(false),
                                 select(ar_handshake, ch_bool(true),
                                        select(burst_end, ch_bool(false), s.r_valid)));
        // 当前 beat 地址：AR 握手时锁存，每个被接受的 R beat 后推进
        s.read_addr_cur->next = select(!io().rst_n, ch_uint<ADDR_WIDTH>(0_d),
                                       select(ar_handshake, io().araddr,
                                              select(r_handshake,
                                                     ch_uint<ADDR_WIDTH>(s.read_addr_cur + advance),
                                                     s.read_addr_cur)));
        // burst 元数据锁存
        s.read_len_reg->next = select(!io().rst_n, ch_uint<4>(0_d),
                                       select(ar_handshake, io().arlen, s.read_len_reg));
        s.read_size_reg->next = select(!io().rst_n, ch_uint<3>(0_d),
                                       select(ar_handshake, io().arsize, s.read_size_reg));
        s.read_burst_reg->next = select(!io().rst_n, ch_uint<2>(0_d),
                                        select(ar_handshake, io().arburst, s.read_burst_reg));
        s.read_id_reg->next = select(!io().rst_n, ch_uint<ID_WIDTH>(0_d),
                                     select(ar_handshake, io().arid, s.read_id_reg));
        // beat 计数：AR 握手清零，每个被接受的 R beat +1
        s.read_beat_cnt->next = select(!io().rst_n, ch_uint<5>(0_d),
                                       select(ar_handshake, ch_uint<5>(0_d),
                                              select(r_handshake,
                                                     ch_uint<5>(s.read_beat_cnt + ch_uint<5>(1_d)),
                                                     s.read_beat_cnt)));
        // 错误标志
        s.read_rejected->next = select(!io().rst_n, ch_bool(false),
                                       select(ar_reject, ch_bool(true),
                                              select(burst_end, ch_bool(false), s.read_rejected)));
        s.read_decerr->next = select(!io().rst_n, ch_bool(false),
                                     select(select(r_handshake, !read_mapped_, ch_bool(false)),
                                            ch_bool(true),
                                            select(burst_end, ch_bool(false), s.read_decerr)));
    }

    // ==================== 寄存器文件（写通道落点） ====================
    void build_data_channels(SlaveState &s) {
        // 字节写：WSTRB 逐 beat 展开（语义见 include/axi4/AGENTS.md 字节道语义）。
        // 掩码在 W 握手拍按**当拍** io().wstrb 现场展开 —— MUST NOT 只在 AW 拍展开一次。
        const auto wstrb_mask =
            axi4::detail::byte_lane_mask<DATA_WIDTH>(io().wstrb);

        // 被拒绝的 burst（类型不支持 / 超长）→ 不产生任何写入
        auto write_enable = select(s.write_rejected, ch_bool(false), w_handshake_);

        auto we0 = select(write_enable, write_sel_[0], ch_bool(false));
        auto we1 = select(write_enable, write_sel_[1], ch_bool(false));
        auto we2 = select(write_enable, write_sel_[2], ch_bool(false));
        auto we3 = select(write_enable, write_sel_[3], ch_bool(false));
        auto we4 = select(write_enable, write_sel_[4], ch_bool(false));
        auto we5 = select(write_enable, write_sel_[5], ch_bool(false));
        auto we6 = select(write_enable, write_sel_[6], ch_bool(false));
        auto we7 = select(write_enable, write_sel_[7], ch_bool(false));
        auto we8 = select(write_enable, write_sel_[8], ch_bool(false));
        auto we9 = select(write_enable, write_sel_[9], ch_bool(false));
        auto we10 = select(write_enable, write_sel_[10], ch_bool(false));
        auto we11 = select(write_enable, write_sel_[11], ch_bool(false));
        auto we12 = select(write_enable, write_sel_[12], ch_bool(false));
        auto we13 = select(write_enable, write_sel_[13], ch_bool(false));
        auto we14 = select(write_enable, write_sel_[14], ch_bool(false));
        auto we15 = select(write_enable, write_sel_[15], ch_bool(false));

        s.reg0->next = select(!io().rst_n, ch_uint<DATA_WIDTH>(0_d),
                              select(we0,
                                     axi4::detail::byte_lane_apply<DATA_WIDTH, DATA_WIDTH>(s.reg0, io().wdata, wstrb_mask),
                                     s.reg0));
        s.reg1->next = select(!io().rst_n, ch_uint<DATA_WIDTH>(0_d),
                              select(we1,
                                     axi4::detail::byte_lane_apply<DATA_WIDTH, DATA_WIDTH>(s.reg1, io().wdata, wstrb_mask),
                                     s.reg1));
        s.reg2->next = select(!io().rst_n, ch_uint<DATA_WIDTH>(0_d),
                              select(we2,
                                     axi4::detail::byte_lane_apply<DATA_WIDTH, DATA_WIDTH>(s.reg2, io().wdata, wstrb_mask),
                                     s.reg2));
        s.reg3->next = select(!io().rst_n, ch_uint<DATA_WIDTH>(0_d),
                              select(we3,
                                     axi4::detail::byte_lane_apply<DATA_WIDTH, DATA_WIDTH>(s.reg3, io().wdata, wstrb_mask),
                                     s.reg3));
        s.reg4->next = select(!io().rst_n, ch_uint<DATA_WIDTH>(0_d),
                              select(we4,
                                     axi4::detail::byte_lane_apply<DATA_WIDTH, DATA_WIDTH>(s.reg4, io().wdata, wstrb_mask),
                                     s.reg4));
        s.reg5->next = select(!io().rst_n, ch_uint<DATA_WIDTH>(0_d),
                              select(we5,
                                     axi4::detail::byte_lane_apply<DATA_WIDTH, DATA_WIDTH>(s.reg5, io().wdata, wstrb_mask),
                                     s.reg5));
        s.reg6->next = select(!io().rst_n, ch_uint<DATA_WIDTH>(0_d),
                              select(we6,
                                     axi4::detail::byte_lane_apply<DATA_WIDTH, DATA_WIDTH>(s.reg6, io().wdata, wstrb_mask),
                                     s.reg6));
        s.reg7->next = select(!io().rst_n, ch_uint<DATA_WIDTH>(0_d),
                              select(we7,
                                     axi4::detail::byte_lane_apply<DATA_WIDTH, DATA_WIDTH>(s.reg7, io().wdata, wstrb_mask),
                                     s.reg7));
        s.reg8->next = select(!io().rst_n, ch_uint<DATA_WIDTH>(0_d),
                              select(we8,
                                     axi4::detail::byte_lane_apply<DATA_WIDTH, DATA_WIDTH>(s.reg8, io().wdata, wstrb_mask),
                                     s.reg8));
        s.reg9->next = select(!io().rst_n, ch_uint<DATA_WIDTH>(0_d),
                              select(we9,
                                     axi4::detail::byte_lane_apply<DATA_WIDTH, DATA_WIDTH>(s.reg9, io().wdata, wstrb_mask),
                                     s.reg9));
        s.reg10->next = select(!io().rst_n, ch_uint<DATA_WIDTH>(0_d),
                               select(we10,
                                      axi4::detail::byte_lane_apply<DATA_WIDTH, DATA_WIDTH>(s.reg10, io().wdata, wstrb_mask),
                                      s.reg10));
        s.reg11->next = select(!io().rst_n, ch_uint<DATA_WIDTH>(0_d),
                               select(we11,
                                      axi4::detail::byte_lane_apply<DATA_WIDTH, DATA_WIDTH>(s.reg11, io().wdata, wstrb_mask),
                                      s.reg11));
        s.reg12->next = select(!io().rst_n, ch_uint<DATA_WIDTH>(0_d),
                               select(we12,
                                      axi4::detail::byte_lane_apply<DATA_WIDTH, DATA_WIDTH>(s.reg12, io().wdata, wstrb_mask),
                                      s.reg12));
        s.reg13->next = select(!io().rst_n, ch_uint<DATA_WIDTH>(0_d),
                               select(we13,
                                      axi4::detail::byte_lane_apply<DATA_WIDTH, DATA_WIDTH>(s.reg13, io().wdata, wstrb_mask),
                                      s.reg13));
        s.reg14->next = select(!io().rst_n, ch_uint<DATA_WIDTH>(0_d),
                               select(we14,
                                      axi4::detail::byte_lane_apply<DATA_WIDTH, DATA_WIDTH>(s.reg14, io().wdata, wstrb_mask),
                                      s.reg14));
        s.reg15->next = select(!io().rst_n, ch_uint<DATA_WIDTH>(0_d),
                               select(we15,
                                      axi4::detail::byte_lane_apply<DATA_WIDTH, DATA_WIDTH>(s.reg15, io().wdata, wstrb_mask),
                                      s.reg15));
    }

    // ==================== 响应通道（B） ====================
    void build_response_logic(SlaveState &s) {
        auto slverr = select(s.write_rejected, ch_bool(true),
                             select(s.write_len_err, ch_bool(true), ch_bool(false)));
        auto decerr = select(s.write_decerr, ch_bool(true), ch_bool(false));

        io().bvalid = s.b_valid;
        io().bid = s.write_id_reg;
        io().bresp = select(slverr,
                            ch_uint<2>(static_cast<unsigned>(AxiResp::SLVERR)),
                            select(decerr,
                                   ch_uint<2>(static_cast<unsigned>(AxiResp::DECERR)),
                                   ch_uint<2>(static_cast<unsigned>(AxiResp::OKAY))));
        (void)b_handshake_;
    }

    void describe() override {
        SlaveState s;

        build_write_channel(s);
        build_read_channel(s);
        build_data_channels(s);
        build_response_logic(s);
    }

private:
    // 地址解码：步长固定 4 字节（`addr >> 2`），窗口 = 字节地址 0x00..0x3C
    // （16 个寄存器 × 4 字节）。
    // ⚠️ MUST NOT 用掩码截断索引（那会把 0x100 之类的地址别名到 reg0）
    //    —— 窗口外一律判为未映射（→ DECERR）。
    void decode_word(ch_uint<ADDR_WIDTH> addr, ch_bool (&sel)[16],
                     ch_bool &mapped) {
        auto index = addr >> ch_uint<ADDR_WIDTH>(2_d);
        for (unsigned i = 0; i < 16; ++i) {
            sel[i] = (index == ch_uint<ADDR_WIDTH>(static_cast<std::uint64_t>(i)));
        }
        mapped = sel[0];
        for (unsigned i = 1; i < 16; ++i) {
            mapped = mapped | sel[i];
        }
    }

    // `awsize` / `arsize` → 字节步长（查表）。
    // ⚠️ MUST NOT 用运行期移位算 `2^awsize` —— `ch_uint::operator<<` 按右操作数
    //    的**类型宽度**推算结果宽度，运行期移位量会把结果宽度算错
    //    （见 include/axi4/AGENTS.md ANTI-PATTERNS）。
    static ch_uint<ADDR_WIDTH> burst_stride(ch_uint<3> size) {
        ch_uint<ADDR_WIDTH> stride(0_d);
        stride = select(size == ch_uint<3>(0_d), ch_uint<ADDR_WIDTH>(1_d), stride);
        stride = select(size == ch_uint<3>(1_d), ch_uint<ADDR_WIDTH>(2_d), stride);
        stride = select(size == ch_uint<3>(2_d), ch_uint<ADDR_WIDTH>(4_d), stride);
        stride = select(size == ch_uint<3>(3_d), ch_uint<ADDR_WIDTH>(8_d), stride);
        stride = select(size == ch_uint<3>(4_d), ch_uint<ADDR_WIDTH>(16_d), stride);
        stride = select(size == ch_uint<3>(5_d), ch_uint<ADDR_WIDTH>(32_d), stride);
        stride = select(size == ch_uint<3>(6_d), ch_uint<ADDR_WIDTH>(64_d), stride);
        stride = select(size == ch_uint<3>(7_d), ch_uint<ADDR_WIDTH>(128_d), stride);
        return stride;
    }

    // describe() 内 helper 间传递的组合信号
    ch_bool w_handshake_{ch_bool(false)};
    ch_bool b_handshake_{ch_bool(false)};
    ch_bool write_sel_[16];
    ch_bool write_mapped_{ch_bool(false)};
    ch_bool read_sel_[16];
    ch_bool read_mapped_{ch_bool(false)};
};

} // namespace axi4
