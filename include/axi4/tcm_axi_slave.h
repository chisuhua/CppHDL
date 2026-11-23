/**
 * @file tcm_axi_slave.h
 * @brief 把 `DataTCM` / `InstrTCM` 包成 **AXI4 full slave**（支持 burst）
 *
 * ## 为什么需要它（2026-11-23 · S3 · change `rv32i-dcache-burst-2026-11`）
 *
 * G-7 裁决 = **full AXI4 主干** ⇒ `DCacheSync` 的 AXI 主口需要一个对端。
 * 仓内的 `Axi4Slave` 是**通用寄存器 slave**（测试用），`AxiLiteSlave` 是 Lite 且
 * 注释明确 `NUM_REGS` 不被兑现 ⇒ ⇒ **没有现成的「内存型 AXI4 full slave」**。
 *
 * 本组件把 TCM 的「简化单口」（`addr` 字地址 / `write` / `wdata` / `strbe` / `valid`
 * → `ready` / `rdata`）包成 AXI4 full：
 * - **读**：`ARLEN` 递增 burst ⇒ 逐 beat 发 TCM 读（`sread` **1 拍**）⇒ 逐 beat 回 `R`
 * - **写**：`AWLEN` 递增 burst + `W` 的 `wstrb` ⇒ 逐 beat 写 TCM
 *
 * ## 已知限制（**如实登记**，不假装完整）
 *
 * - **无 ID 处理**：`arid` / `awid` 被忽略 ⇒ 不支持 out-of-order（单口 TCM 也不需要）
 * - **读与写串行化**：`r_active` 期间不接 `AW`（反之亦然）⇒ 单口 TCM 的必然
 * - **`ARBURST` / `AWBURST` 只支持 INCR(01)** ⇒ 其它值按 INCR 处理（TCM 是线性内存）
 * - **`ARSIZE` / `AWSIZE` 只支持 2（4 字节）** ⇒ 与 RV32I 的字宽一致
 * - **`r_resp` / `b_resp` 恒 OKAY**（`0`）⇒ 地址越界由 TCM 自身截断（非本组件的语义）
 *
 * ⚠️ 判据见 `tests/test_tcm_axi_slave.cpp`（AC6：`ARLEN=3` 读 4 字 / `AWLEN=3` 写 4 字，
 * 地址与值全部**手推**）。
 */
#pragma once

#include "ch.hpp"
#include "component.h"

namespace axi4 {

using namespace ch::core;

/**
 * @tparam ADDR_WIDTH  AXI 侧字节地址宽度（默认 32）
 * @tparam DATA_WIDTH  数据宽度（默认 32 · 与 RV32I 一致）
 * @tparam TCM_AW      TCM 的**字地址**宽度（默认 12 ⇒ 默认 4096 字）
 * @tparam ID_WIDTH    AXI ID 宽度（默认 4 · 本实现忽略其值）
 */
template <unsigned ADDR_WIDTH = 32, unsigned DATA_WIDTH = 32,
          unsigned TCM_AW = 12, unsigned ID_WIDTH = 4>
class TcmAxiSlave : public ch::Component {
public:
    __io(
        // ---- AR ----
        ch_in<ch_uint<ADDR_WIDTH>> ar_addr;
        ch_in<ch_uint<ID_WIDTH>>   ar_id;
        ch_in<ch_uint<4>>          ar_len;
        ch_in<ch_uint<3>>          ar_size;
        ch_in<ch_uint<2>>          ar_burst;
        ch_in<ch_bool>             ar_valid;
        ch_out<ch_bool>            ar_ready;

        // ---- R ----
        ch_out<ch_uint<DATA_WIDTH>> r_data;
        ch_out<ch_uint<ID_WIDTH>>   r_id;
        ch_out<ch_uint<2>>          r_resp;
        ch_out<ch_bool>             r_last;
        ch_out<ch_bool>             r_valid;
        ch_in<ch_bool>              r_ready;

        // ---- AW ----
        ch_in<ch_uint<ADDR_WIDTH>> aw_addr;
        ch_in<ch_uint<ID_WIDTH>>   aw_id;
        ch_in<ch_uint<4>>          aw_len;
        ch_in<ch_uint<3>>          aw_size;
        ch_in<ch_uint<2>>          aw_burst;
        ch_in<ch_bool>             aw_valid;
        ch_out<ch_bool>            aw_ready;

        // ---- W ----
        ch_in<ch_uint<DATA_WIDTH>> w_data;
        ch_in<ch_uint<DATA_WIDTH / 8>> w_strb;
        ch_in<ch_bool>             w_last;
        ch_in<ch_bool>             w_valid;
        ch_out<ch_bool>            w_ready;

        // ---- B ----
        ch_out<ch_uint<ID_WIDTH>> bid;
        ch_out<ch_uint<2>>        bresp;
        ch_out<ch_bool>           bvalid;
        ch_in<ch_bool>            bready;

        // ---- TCM 侧（**字地址** · 单口简化握手）----
        ch_out<ch_uint<TCM_AW>>    tcm_addr;
        ch_out<ch_bool>            tcm_write;
        ch_out<ch_uint<DATA_WIDTH>> tcm_wdata;
        ch_out<ch_uint<4>>         tcm_strbe;
        ch_out<ch_bool>            tcm_valid;
        ch_in<ch_bool>             tcm_ready;
        ch_in<ch_uint<DATA_WIDTH>> tcm_rdata;
    )

    TcmAxiSlave(ch::Component* parent = nullptr,
                const std::string& name = "tcm_axi_slave")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        // =====================================================================
        // 读通路：AR 握手 ⇒ 逐 beat 发 TCM 读（sread 1 拍）⇒ 逐 beat 回 R
        // =====================================================================
        ch_reg<ch_bool>     r_active{false, "r_active"};
        ch_reg<ch_bool>     r_pending{false, "r_pending"};  // TCM 读已发出、数据已到
        ch_reg<ch_uint<ADDR_WIDTH>> r_addr{0_d, "r_addr"};  // 当前 beat 的**字节**地址
        ch_reg<ch_uint<4>>  r_left{0_d, "r_left"};          // 剩余 beat 数（0 = 最后一个）
        ch_reg<ch_uint<ID_WIDTH>> r_id_reg{0_d, "r_id_reg"};

        ch_bool ar_fire = io().ar_valid & io().ar_ready;
        io().ar_ready <<= !r_active & !io().aw_valid;  // 单口：与写互斥（读优先）

        // 发 TCM 读（每 beat 一次；`r_pending=0` ⇒ 上一 beat 已被取走）
        ch_bool tcm_rd_valid = r_active & !r_pending;
        ch_bool r_fire = io().r_valid & io().r_ready;
        ch_bool r_done = r_fire & io().r_last;

        r_active->next = select(ar_fire, ch_bool(true),
                                select(r_done, ch_bool(false), r_active));
        r_pending->next = select(tcm_rd_valid, ch_bool(true),
                                 select(r_fire, ch_bool(false), r_pending));
        r_id_reg->next = select(ar_fire, io().ar_id, r_id_reg);

        // ⚠️ `ARLEN` 是 **beat 数 − 1**（`3` = 4 beat）
        r_left->next = select(ar_fire, io().ar_len,
                              select(r_fire,
                                     select(r_left == ch_uint<4>(0_d), ch_uint<4>(0_d),
                                            r_left - ch_uint<4>(1_d)),
                                     r_left));
        auto r_addr_next = select(ar_fire, io().ar_addr,
                                  select(r_fire, r_addr + ch_uint<ADDR_WIDTH>(4_d), r_addr));
        r_addr->next = r_addr_next;

        io().r_valid <<= r_active & r_pending;
        io().r_data  <<= io().tcm_rdata;
        io().r_last  <<= (r_left == ch_uint<4>(0_d));
        io().r_resp  <<= ch_uint<2>(0_d);   // OKAY
        io().r_id    <<= r_id_reg;

        // =====================================================================
        // 写通路：AW 握手 ⇒ W 逐 beat 写 TCM ⇒ 最后一个 beat 回 B
        // =====================================================================
        ch_reg<ch_bool>     w_active{false, "w_active"};
        ch_reg<ch_uint<ADDR_WIDTH>> w_addr{0_d, "w_addr"};
        ch_reg<ch_uint<ID_WIDTH>> w_id_reg{0_d, "w_id_reg"};

        ch_bool aw_fire = io().aw_valid & io().aw_ready;
        io().aw_ready <<= !r_active & !w_active;

        ch_bool w_fire = io().w_valid & io().w_ready;
        io().w_ready <<= w_active;

        w_active->next = select(aw_fire, ch_bool(true),
                                select(w_fire & io().w_last, ch_bool(false), w_active));
        w_addr->next = select(aw_fire, io().aw_addr,
                              select(w_fire, w_addr + ch_uint<ADDR_WIDTH>(4_d), w_addr));
        w_id_reg->next = select(aw_fire, io().aw_id, w_id_reg);

        // TCM 写（每 beat 一次；与读互斥 ⇒ 读优先）
        auto tcm_wr_valid = w_active & w_fire & !r_active;

        // =====================================================================
        // TCM 单口（读优先）
        // =====================================================================
        io().tcm_valid <<= tcm_rd_valid | tcm_wr_valid;
        io().tcm_write <<= tcm_wr_valid;
        io().tcm_addr  <<= select(tcm_wr_valid,
                                  bits<TCM_AW - 1, 0>(w_addr >> ch_uint<2>(2_d)),
                                  bits<TCM_AW - 1, 0>(r_addr >> ch_uint<2>(2_d)));
        io().tcm_wdata <<= io().w_data;
        io().tcm_strbe <<= io().w_strb;

        // =====================================================================
        // B 通道
        // =====================================================================
        io().bvalid <<= w_active & w_fire & io().w_last;
        io().bresp  <<= ch_uint<2>(0_d);   // OKAY
        io().bid    <<= w_id_reg;
    }
};

} // namespace axi4
