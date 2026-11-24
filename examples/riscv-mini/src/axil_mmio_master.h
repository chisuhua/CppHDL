/**
 * @file axil_mmio_master.h
 * @brief 核侧 **AXI4-Lite 主设备**（MMIO 通道的核内一端）
 *
 * ## 职责
 *
 * 把核内 MEM 级的**单拍请求语义**（`preq` / `pwe` / `paddr` / `pwdata` /
 * `pstrbe`）翻译成一条 **AXI4-Lite 主口**（19 根）上的完整事务，并在事务
 * **完成的当拍**回一个 **`ready` 脉冲**。
 *
 * ## 三条硬约束（来源 = change `rv32i-soc-mmio-channel-2026-11` 的 T2.x）
 *
 * 1. **`ready` MUST 是「单拍完成脉冲」**（T2.3）
 *    ⇒ ⇒ 它进了核顶层的 `data_ready_calc` ⇒ ⇒ 若为**电平**，观测面的
 *    「store 提交事件」会在**整个事务期间**持续为高 ⇒ ⇒ 事件次数 = 事件时长
 *    之和 ⇒ ⇒ 判据（AC3 的计数终值）**灌水**。
 *    ⇒ ⇒ 本实现的 `ready` = 完成握手那一拍（`B` 或 `R` 的 handshake）。
 *
 * 2. **`awvalid` / `wvalid` / `arvalid` MUST 保持到各自握手**（AXI 协议）
 *    ⇒ ⇒ 且 `awaddr` / `wdata` / `wstrb` / `araddr` MUST 在 VALID 期间**稳定**
 *    ⇒ ⇒ 故它们由**发送当拍锁存**的寄存器驱动（**直接接输入端口会让它们随
 *    流水线变化** ⇒ ⇒ 违反 AXI，且违例是**静默**的：从设备并不检查）。
 *
 * 3. **`bready` / `rready` 恒 1**（本核的 MMIO 通道是**单事务**语义
 *    ⇒ ⇒ 不存在「暂不接受响应」的场景）⇒ ⇒ 因此完成时刻 = `bvalid` / `rvalid`
 *    出现的那一拍。
 *
 * ## 为什么 AW 与 W **同拍发起**却仍算「相位分离」
 *
 * AXI4-Lite 允许 AW 与 W 各自独立握手 ⇒ ⇒ 本 master **同拍拉高两者**，
 * 但**分别**等待各自的 READY（`aw_done` / `w_done` 两个标志）
 * ⇒ ⇒ 从设备可以任意顺序、任意拍数地接受它们（如 `AxiLiteGpio` 要求
 * W 在 AW 之后）⇒ ⇒ 本 master 不假定二者的先后。
 *
 * ## 复位
 *
 * `rst` **高有效**，直连核顶层的 `ext_rst` ⇒ ⇒ **不新增核端口**（G-9）。
 *
 * ⚠️ 硬件值 MUST NOT 参与 C++ 控制流（`AGENTS.md` 的最高频缺陷族）
 *   ⇒ ⇒ 本文件全部用 `select(...)` / `&` / `|` / `!`，**无** `if` / `?:`。
 * ⚠️ `&&` / `||` 在 `ch_bool` 上已弃用（ADR-036）⇒ ⇒ 本文件用 `&` / `|`。
 */

#pragma once

#include "ch.hpp"
#include "component.h"

// 与 `examples/riscv-mini/src/rv32i_core_top.h` 同款：本头是**核 IP 侧**的头，
// 其消费者（核顶层）已在全局打开 `ch::core`；此处同样打开 ⇒ ⇒ 本头**自包含**
// （`verify_header_self_contained.sh` 会单独编译它）。
using namespace ch::core;

namespace axil_mmio_detail {

/// 事务状态（**编码值不是契约** · 只在本文件与判据里使用）
inline constexpr uint64_t kIdle   = 0;
inline constexpr uint64_t kWrAddr = 1;  // 等 AW **与** W 两个握手
inline constexpr uint64_t kWrResp = 2;  // 等 B
inline constexpr uint64_t kRdAddr = 3;  // 等 AR 握手
inline constexpr uint64_t kRdResp = 4;  // 等 R

}  // namespace axil_mmio_detail

class AxilMmioMaster : public ch::Component {
public:
    __io(
        // ---------------- 请求面（核内 MEM 级驱动）----------------
        ch_in<ch_bool>      preq{"preq"};
        ch_in<ch_bool>      pwe{"pwe"};
        ch_in<ch_uint<32>>  paddr{"paddr"};
        ch_in<ch_uint<32>>  pwdata{"pwdata"};
        ch_in<ch_uint<4>>   pstrbe{"pstrbe"};
        /// **完成脉冲**（一次事务**恰好一拍**）—— 见文件头的约束 1
        ch_out<ch_bool>     ready{"ready"};
        /// 读数据（**保持寄存器** ⇒ ⇒ 完成脉冲之后仍可读，直到下一次读完成）
        ch_out<ch_uint<32>> prdata{"prdata"};

        // ---------------- AXI4-Lite 主口（19 根）----------------
        ch_out<ch_uint<32>> awaddr{"awaddr"};
        ch_out<ch_uint<2>>  awprot{"awprot"};
        ch_out<ch_bool>     awvalid{"awvalid"};
        ch_in<ch_bool>      awready{"awready"};

        ch_out<ch_uint<32>> wdata{"wdata"};
        ch_out<ch_uint<4>>  wstrb{"wstrb"};
        ch_out<ch_bool>     wvalid{"wvalid"};
        ch_in<ch_bool>      wready{"wready"};

        ch_in<ch_uint<2>>   bresp{"bresp"};
        ch_in<ch_bool>      bvalid{"bvalid"};
        ch_out<ch_bool>     bready{"bready"};

        ch_out<ch_uint<32>> araddr{"araddr"};
        ch_out<ch_uint<2>>  arprot{"arprot"};
        ch_out<ch_bool>     arvalid{"arvalid"};
        ch_in<ch_bool>      arready{"arready"};

        ch_in<ch_uint<32>>  rdata{"rdata"};
        ch_in<ch_uint<2>>   rresp{"rresp"};
        ch_in<ch_bool>      rvalid{"rvalid"};
        ch_out<ch_bool>     rready{"rready"};

        /// 复位（**高有效** · 接核顶层 `ext_rst`）
        ch_in<ch_bool>      rst{"rst"};
    )

    AxilMmioMaster(ch::Component* parent = nullptr, const std::string& name = "axil_mmio_master")
        : ch::Component(parent, name) {}

    void create_ports() override {
        new (io_storage_) io_type;
    }

    void describe() override {
        using namespace ch::core;
        using namespace axil_mmio_detail;

        ch_reg<ch_uint<3>>  state(0_d);
        // AW / W **各自**的握手完成标志（相位分离：不假定先后）
        ch_reg<ch_bool>     aw_done(ch_bool(false));
        ch_reg<ch_bool>     w_done(ch_bool(false));
        // 发送当拍锁存（AXI 要求 VALID 期间地址 / 数据稳定）
        ch_reg<ch_uint<32>> addr_reg(0_d);
        ch_reg<ch_uint<32>> wdata_reg(0_d);
        ch_reg<ch_uint<4>>  strbe_reg(0_d);
        // 读数据保持寄存器
        ch_reg<ch_uint<32>> rdata_reg(0_d);

        auto in_idle    = (state == ch_uint<3>(kIdle));
        auto in_wr_addr = (state == ch_uint<3>(kWrAddr));
        auto in_wr_resp = (state == ch_uint<3>(kWrResp));
        auto in_rd_addr = (state == ch_uint<3>(kRdAddr));
        auto in_rd_resp = (state == ch_uint<3>(kRdResp));

        // **只在 IDLE 采样请求** ⇒ ⇒ 事务期间的 `preq` 电平（流水线被
        // `ready=0` 冻结时它恒高）**不会**被当成第二个事务
        auto start = in_idle & io().preq;

        // 组合 VALID：**握手当拍仍为高**，握手后下一拍撤销（AXI 合规）
        auto awvalid_c = in_wr_addr & !aw_done;
        auto wvalid_c  = in_wr_addr & !w_done;
        auto arvalid_c = in_rd_addr;

        auto aw_fire = awvalid_c & io().awready;
        auto w_fire  = wvalid_c & io().wready;
        // `bready` / `rready` 恒 1 ⇒ ⇒ 完成时刻 = VALID 出现的那一拍
        auto ar_fire = arvalid_c & io().arready;
        auto b_fire  = io().bvalid;
        auto r_fire  = io().rvalid;

        auto wr_complete = (aw_done | aw_fire) & (w_done | w_fire);

        // ---------------- 端口驱动 ----------------
        io().awvalid = awvalid_c;
        io().awaddr  = addr_reg;
        io().awprot  = ch_uint<2>(0_d);

        io().wvalid  = wvalid_c;
        io().wdata   = wdata_reg;
        io().wstrb   = strbe_reg;

        io().bready  = ch_bool(true);

        io().arvalid = arvalid_c;
        io().araddr  = addr_reg;
        io().arprot  = ch_uint<2>(0_d);

        io().rready  = ch_bool(true);

        io().prdata  = rdata_reg;

        // **完成脉冲**：B / R 握手那一拍 ⇒ ⇒ 恰好一拍（事务时长无关）
        io().ready = (in_wr_resp & b_fire) | (in_rd_resp & r_fire);

        // ---------------- 状态迁移 ----------------
        state->next = select(io().rst, ch_uint<3>(kIdle),
                      select(in_idle, select(start,
                                            select(io().pwe,
                                                   ch_uint<3>(kWrAddr),
                                                   ch_uint<3>(kRdAddr)),
                                            ch_uint<3>(kIdle)),
                      select(in_wr_addr, select(wr_complete,
                                                ch_uint<3>(kWrResp),
                                                ch_uint<3>(kWrAddr)),
                      select(in_wr_resp, select(b_fire,
                                                ch_uint<3>(kIdle),
                                                ch_uint<3>(kWrResp)),
                      select(in_rd_addr, select(ar_fire,
                                                ch_uint<3>(kRdResp),
                                                ch_uint<3>(kRdAddr)),
                      select(r_fire,
                             ch_uint<3>(kIdle),
                             ch_uint<3>(kRdResp)))))));

        // ---------------- 寄存器更新 ----------------
        aw_done->next = select(io().rst, ch_bool(false),
                        select(in_wr_addr, aw_done | aw_fire, ch_bool(false)));
        w_done->next  = select(io().rst, ch_bool(false),
                        select(in_wr_addr, w_done | w_fire, ch_bool(false)));

        addr_reg->next   = select(io().rst, ch_uint<32>(0_d),
                           select(start, io().paddr, addr_reg));
        wdata_reg->next  = select(io().rst, ch_uint<32>(0_d),
                           select(start, io().pwdata, wdata_reg));
        strbe_reg->next  = select(io().rst, ch_uint<4>(0_d),
                           select(start, io().pstrbe, strbe_reg));
        rdata_reg->next  = select(io().rst, ch_uint<32>(0_d),
                           select(r_fire, io().rdata, rdata_reg));
    }
};
