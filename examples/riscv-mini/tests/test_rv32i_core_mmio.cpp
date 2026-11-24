/**
 * @file test_rv32i_core_mmio.cpp
 * @brief **核级（IP 级）MMIO 通道判据**（change `rv32i-ip-freeze-2026-11` · T1）
 *
 * ## 为什么要它
 *
 * 核级 TB（`tb_rv32i_soc.{cpp,sv}`）把 8 根 `axil_*` 输入**全部 tie 0**
 * （核级没有从设备）⇒ ⇒ 24 条核级判据里**没有一条**走 MMIO
 * ⇒ ⇒ 通道的行为只在 **SoC 级**（`examples/riscv-soc` 的 A1/A7/A9）被间接验证。
 * ⇒ ⇒ 「IP 可复用」的前提是它**自己的接口**被单独验证过 ⇒ ⇒ 本文件补这一格。
 *
 * ## 观测面（**不新增端口**）
 *
 * 读值用「**存回去**」变成可观测量：核的根输出 `obs_data_write_data`（#9）+
 * `obs_data_write_en`（#10 · **单拍脉冲**，S4a-0 起）⇒ ⇒ 观测到的 store 数据
 * == stub 回的 `rdata` 即证明读值正确。
 *
 * ## 期望值**手推**（禁抄仿真输出）
 *
 * ```
 * lui  x4, 0x80102   ⇒ x4 = 0x8010_2000（= kGpio.base · MMIO 段内）
 * lw   x6, 4(x4)     ⇒ MMIO 读（stub 回 rdata = 0x5A）⇒ 写回 x6
 * addi x7, x0, 1     ⇒ **非 MMIO 后继**（其 EX/MEM 地址 = 1）
 *                      ⇒ ⇒ 逼出「读数据 mux 用当拍地址选路径」的缺陷
 * sw   x6, 0(x4)     ⇒ MMIO 写（数据 = 读回来的值 0x5A）
 * jal  x0, 0         ⇒ 自环（停机）
 * ```
 *
 * ⇒ ⇒ 写回 **3**（x4 / x6 / x7）· store **1**（只有那条 `sw`）
 * ⇒ ⇒ store 的 **地址** = 0x8010_2000 · **数据** = **0x5A**
 * ⇒ ⇒ 总线上：读地址 = 0x8010_2004 · 写地址 = 0x8010_2000
 *
 * ## 判据（7 组 · 每组都有**反证**面 ⇒ ⇒ 恒绿判据一律不收）
 *
 * > ⚠️ **编号说明**：本文件的 `AC1` … `AC7` 是**判据组编号**（= 7 个 TEST_CASE）；
 * > change 提案的 `AC1` … `AC7` 是**验收条目**（其中 AC6 = 层次化等价 · AC7 = 零回归）⇒ ⇒
 * > **不同名同义**：本文件 AC1–AC5 ↔ 提案 AC1–AC5；本文件 **AC6（AXI 握手协议）/ AC7（慢从设备）**
 * > 是本批新增的两组，归提案的「T1 判据」范畴。
 *
 * 1. **AC1 读值**：store 事件的地址 / 数据 == 手推值
 * 2. **AC2 计数**：store == 1 · 写回 == 3（rd 与 data 各判一次）
 * 3. **AC3 事务依赖**：从设备**不应答** ⇒ 程序**永不完成** ⇒ ⇒ 证明 AC1 的「完成」
 *    不是「核自说自话」⇒ ⇒ 再从「应答 ⇒ 完成」两侧夹逼出「核真的在等从设备」
 * 4. **AC4 DECERR 不挂死**：`bresp = 3` ⇒ 程序照常完成
 * 5. **AC5 非 MMIO 零变化**：无 MMIO 的程序 ⇒ `axil_*` 请求线**从未拉高**
 *    （⇒ 证明 pre-cache 分流方向正确：字节地址 0 **不**被推进 MMIO 旁路）
 * 6. **AC6 AXI-Lite 协议**：VALID 保持到握手 · VALID 期间地址 / 数据稳定 ·
 *    **完成事件不得早于 B 响应** · 响应不早于请求 · 总线上的地址 == 手推值
 * 7. **AC7 慢从设备**（`ready_lat = 3`）：事务照样完成，且 VALID 窗口**真的跨多拍**
 *    （否则 AC6 的「保持」只覆盖 1 拍 ⇒ 判别力退化）
 *
 * ## 时序模型（`eval_combinational` + `tick` 各一次）
 *
 * `set_input_value` → `eval_combinational`（**算本拍组合**）→ 采样输出 →
 * `tick`（**本拍沿**）⇒ ⇒ 采样的是**本拍**的组合输出，不是下一拍
 * （`get_port_value` 只读 `data_map_` ⇒ ⇒ 不自己重算 ⇒ ⇒ 必须显式 `eval_combinational`）。
 *
 * ## stub 从设备（**响应式**）
 *
 * | 输入 | 规则 |
 * |---|---|
 * | `awready` / `wready` / `arready` | **每个事务独立**：「观察到该请求」后的第 `ready_lat + 1` 拍起拉高（请求线全撤 ⇒ 下一事务重新计时） |
 * | `bvalid` | 观察到 AW **与** W 握手后，下一拍给 1 拍；`bresp` = 配置值 |
 * | `rvalid` | 观察到 AR 握手后，下一拍给 1 拍；`rdata` = 配置值 |
 * | `dead = true` | **永不应答**（ready 恒 0 · valid 恒 0）⇒ ⇒ 反证档 |
 *
 * ⚠️ `ready_lat` 是**额外**延迟：stub MUST 先**观察**到请求才能应答 ⇒ ⇒ 天然晚 1 拍。
 */

#include "../../../tests/catch_amalgamated.hpp"
#include "ch.hpp"
#include "simulator.h"
#include "rv32i_core_top.h"

#include "cpu/riscv/rv32i_mem_map.h"
#include "cpu/riscv/rv32i_asm_encode.h"

#include <cstddef>
#include <cstdint>
#include <ios>
#include <memory>
#include <utility>
#include <vector>

using namespace ch;
using namespace ch::core;

using riscv::ADD;
using riscv::ADDI;
using riscv::JAL;
using riscv::LUI;
using riscv::LW;
using riscv::NOP;
using riscv::SW;

namespace {

// ---------------------------------------------------------------------------
// 常量（**全部派生或手推** ⇒ ⇒ 禁裸数字）
// ---------------------------------------------------------------------------

/// I-TCM 尾部填 NOP 的字数（G1 纪律：I-TCM 初值 0 = 未知 opcode ⇒ 流水线会冻结）
constexpr std::size_t kFillWords = 64;

/// 观测窗口（拍）
constexpr std::size_t kCycles = 200;

/// MMIO 程序的字数
constexpr std::size_t kMmioProgWords = 5;

/// **MMIO 程序**（5 字 · 手推见文件头）
constexpr uint32_t kMmioProgram[kMmioProgWords] = {
    LUI(4, 0x80102),  // 0  x4 = 0x8010_2000 = kGpio.base
    LW(6, 4, 4),      // 1  MMIO 读 ⇒ stub 回 0x5A
    ADDI(7, 0, 1),    // 2  非 MMIO 后继（EX/MEM 地址 = 1）
    SW(6, 4, 0),      // 3  MMIO 写（数据 = 读回来的值）
    JAL(0, 0),        // 4  自环
};

/// **手算编码**（独立于库编码器 ⇒ ⇒ 交叉校验证据 · 与 SoC 级 A9 同一组常量）
constexpr uint32_t kMmioHandEncoded[kMmioProgWords] = {
    0x80102237u,  // 0  lui  x4, 0x80102  imm=0x80102 · rd=4 · op=0110111
    0x00422303u,  // 1  lw   x6, 4(x4)   imm=4 · rs1=4 · funct3=010 · rd=6 · op=0000011
    0x00100393u,  // 2  addi x7, x0, 1   imm=1 · rs1=0 · funct3=000 · rd=7 · op=0010011
    0x00622023u,  // 3  sw   x6, 0(x4)   imm=0 · rs2=6 · rs1=4 · funct3=010 · op=0100011
    0x0000006Fu,  // 4  jal  x0, 0       imm=0 · rd=0 · op=1101111
};

static_assert(kMmioProgram[0] == kMmioHandEncoded[0], "字 0 编码与手算常量不符");
static_assert(kMmioProgram[1] == kMmioHandEncoded[1], "字 1 编码与手算常量不符");
static_assert(kMmioProgram[2] == kMmioHandEncoded[2], "字 2 编码与手算常量不符");
static_assert(kMmioProgram[3] == kMmioHandEncoded[3], "字 3 编码与手算常量不符");
static_assert(kMmioProgram[4] == kMmioHandEncoded[4], "字 4 编码与手算常量不符");

/// **非 MMIO 程序**（7 字 · 与核级 TB `tb_rv32i_soc.cpp` 同款）
constexpr std::size_t kPlainProgWords = 7;
constexpr uint32_t kPlainProgram[kPlainProgWords] = {
    ADDI(1, 0, 42), NOP, ADDI(2, 0, 11), NOP, ADD(3, 1, 2), SW(3, 0, 0),
    JAL(0, 0),
};

/// 手推的 MMIO 读地址（`lw x6, 4(x4)` ⇒ x4 + 4）
constexpr uint32_t kMmioReadAddr = 0x80102004u;
/// 手推的 MMIO 写地址（`sw x6, 0(x4)` ⇒ x4 + 0）
constexpr uint32_t kMmioWriteAddr = 0x80102000u;
/// 手推的读值（stub 回的 `rdata`）
constexpr uint64_t kStubRdata = 0x5Au;
/// 手推的 store 数据（== 读回来的值）
constexpr uint64_t kExpectStoreData = kStubRdata;
/// 手推的写回数（x4 / x6 / x7）
constexpr std::size_t kExpectWritebacks = 3;

/// ⚠️ **手推常量 × 库表**的交叉校验（编译期 ⇒ 零运行期成本）
static_assert(kMmioWriteAddr == static_cast<uint32_t>(riscv::kGpio.base),
              "MMIO 写地址 MUST == 库层 kGpio.base");
static_assert(kMmioReadAddr == static_cast<uint32_t>(riscv::kGpio.base) + 4u,
              "MMIO 读地址 MUST == 库层 kGpio.base + 4");
static_assert(riscv::in_mmio(kMmioReadAddr), "MMIO 读地址 MUST 落在 MMIO 段内");
static_assert(riscv::in_mmio(kMmioWriteAddr), "MMIO 写地址 MUST 落在 MMIO 段内");
static_assert(!riscv::in_mmio(0u),
              "字节地址 0 MUST NOT 落在 MMIO 段内（pre-cache 分流的方向判据）");

// ---------------------------------------------------------------------------
// stub 从设备 + 一次运行的观测
// ---------------------------------------------------------------------------

/// stub 的配置
struct StubCfg {
    /// ready 相对「首次观察到请求」的**额外**延迟拍数（0 = 尽快应答）
    int ready_lat = 0;
    /// **永不应答**（反证档）
    bool dead = false;
    /// 读数据（手推值）
    uint64_t rdata = kStubRdata;
    /// 写响应码（3 = DECERR）
    uint64_t bresp = 0;
    /// 读响应码（3 = DECERR）
    uint64_t rresp = 0;
};

/// 一次运行的观测结果
struct RunOut {
    // ── 事件 ──
    std::size_t stores = 0;                        ///< store 提交事件数
    std::uint64_t store_addr = 0;                  ///< 最后一拍 store 的地址
    std::uint64_t store_data = 0;                  ///< 最后一拍 store 的数据
    std::vector<std::int64_t> store_cycles;        ///< 每次 store 事件发生在哪一拍
    std::vector<std::pair<std::uint32_t, std::uint64_t>> writebacks;

    // ── AXI 观测面 ──
    std::uint64_t axil_req_cycles = 0;             ///< 任一请求线为高的拍数
    std::uint64_t awvalid_cycles = 0;              ///< `awvalid` 为高的拍数
    std::uint64_t arvalid_cycles = 0;              ///< `arvalid` 为高的拍数
    std::uint64_t first_awaddr = 0;                ///< 首个 `awvalid` 拍的地址
    std::uint64_t first_araddr = 0;                ///< 首个 `arvalid` 拍的地址
    bool saw_awvalid = false;
    bool saw_arvalid = false;
    bool saw_bvalid = false;
    bool saw_rvalid = false;
    std::int64_t first_awvalid = -1;
    std::int64_t first_arvalid = -1;
    std::int64_t first_bvalid = -1;
    std::int64_t first_rvalid = -1;
    bool arvalid_at_end = false;                   ///< 最后一拍 `arvalid` 仍为高

    // ── 协议违例（AXI4-Lite）──
    bool valid_dropped = false;                    ///< VALID 在 READY 之前被撤
    bool payload_changed = false;                 ///< VALID 期间地址 / 数据变了
    bool resp_before_req = false;                 ///< 响应早于请求
};

/// 跑一个程序并采集观测
///
/// ⚠️ **采样口径**：`eval_combinational` 先于采样 ⇒ ⇒ 取到**本拍**的组合值。
RunOut run_core(const std::vector<std::uint32_t> &prog, const StubCfg &cfg) {
    auto dev = std::make_unique<ch::ch_device<Rv32iCoreTop>>();
    Simulator sim(dev->context());
    auto &io = dev->instance().io();

    RunOut out;

    // ── stub 的内部状态 ──
    // 「当前未被接受的请求」起始拍（-1 = 无）⇒ ⇒ `ready_lat` 对**每个事务**独立生效
    // （否则只有**首个**请求等，后续事务的 VALID 窗口退化成 1 拍 ⇒ AC7 抓不住）
    std::int64_t pending_since = -1;
    std::int64_t b_due = -1;
    std::int64_t r_due = -1;
    bool aw_fired = false;   // AW 已握手（AXI 允许 AW / W 先后到达）
    bool w_fired = false;    // W 已握手

    // ready 的拉高条件（**额外**延迟 ⇒ 天然比「观察到的请求」晚 1 拍）
    auto ready_now = [&](std::int64_t cycle) {
        return !cfg.dead && pending_since >= 0 && (cycle - pending_since) > cfg.ready_lat;
    };
    // 驱动从设备输入（**本拍**）
    auto drive = [&](std::int64_t cycle) {
        const bool rdy = ready_now(cycle);
        sim.set_input_value(io.axil_awready, rdy ? 1u : 0u);
        sim.set_input_value(io.axil_wready, rdy ? 1u : 0u);
        sim.set_input_value(io.axil_arready, rdy ? 1u : 0u);
        sim.set_input_value(io.axil_bvalid, (cycle == b_due) ? 1u : 0u);
        sim.set_input_value(io.axil_rvalid, (cycle == r_due) ? 1u : 0u);
        sim.set_input_value(io.axil_bresp, cfg.bresp);
        sim.set_input_value(io.axil_rresp, cfg.rresp);
        // 🔴 `rdata` **只在响应拍**是真的（AXI：`rdata` 仅在 `rvalid=1` 时有效）
        //    ⇒ ⇒ 其余拍给**每拍都变**的垃圾值 ⇒ ⇒ 「提前锁存 `rdata`」的实现必拿到垃圾
        //    ⇒ ⇒ 否则（全程恒 0x5A）该回归**结构性不可见**（critic 轮次 1 · SHOULD_FIX-3①）。
        const std::uint64_t rdata_now = (cycle == r_due)
            ? cfg.rdata
            : (0xA5A50000u + static_cast<std::uint64_t>(cycle));
        sim.set_input_value(io.axil_rdata, rdata_now);
    };

    auto get = [&](const auto &port) {
        return static_cast<std::uint64_t>(sim.get_port_value(port));
    };

    // ── 装载程序（装载期 `ext_rst` 保持 1 ⇒ 性能计数器不被装载污染）──
    sim.set_input_value(io.ext_rst, 1u);
    sim.set_input_value(io.prog_we, 1u);
    for (std::size_t i = 0; i < prog.size(); ++i) {
        drive(-1000);
        sim.set_input_value(io.prog_addr, static_cast<std::uint64_t>(i));
        sim.set_input_value(io.prog_data, prog[i]);
        sim.tick(1);
    }
    for (std::size_t i = prog.size(); i < kFillWords; ++i) {
        drive(-1000);
        sim.set_input_value(io.prog_addr, static_cast<std::uint64_t>(i));
        sim.set_input_value(io.prog_data, NOP);
        sim.tick(1);
    }
    sim.set_input_value(io.prog_we, 0u);
    sim.set_input_value(io.prog_addr, 0u);
    sim.set_input_value(io.prog_data, 0u);

    // ── 释放复位 ──
    sim.set_input_value(io.ext_rst, 0u);

    // ── 上一拍的快照（协议检查用）──
    bool prev_awvalid = false, prev_wvalid = false, prev_arvalid = false;
    bool prev_awready = false, prev_wready = false, prev_arready = false;
    bool saw_ar_before = false, saw_aw_before = false, saw_w_before = false;
    std::uint64_t prev_awaddr = 0, prev_wdata = 0, prev_wstrb = 0, prev_araddr = 0;

    for (std::int64_t cycle = 0; cycle < static_cast<std::int64_t>(kCycles); ++cycle) {
        drive(cycle);
        sim.eval_combinational();

        const bool awvalid = get(io.axil_awvalid) != 0u;
        const bool wvalid = get(io.axil_wvalid) != 0u;
        const bool arvalid = get(io.axil_arvalid) != 0u;
        const bool bvalid = get(io.axil_bvalid) != 0u;
        const bool rvalid = get(io.axil_rvalid) != 0u;
        const std::uint64_t awaddr = get(io.axil_awaddr);
        const std::uint64_t wdata = get(io.axil_wdata);
        const std::uint64_t wstrb = get(io.axil_wstrb);
        const std::uint64_t araddr = get(io.axil_araddr);
        const bool rdy = ready_now(cycle);

        // ── 事件 ──
        if (get(io.obs_data_write_en) != 0u) {
            ++out.stores;
            out.store_addr = get(io.obs_data_addr);
            out.store_data = get(io.obs_data_write_data);
            out.store_cycles.push_back(cycle);
        }
        if (get(io.dbg_wb_write_en) != 0u) {
            out.writebacks.emplace_back(
                static_cast<std::uint32_t>(get(io.dbg_wb_rd_addr)),
                get(io.dbg_wb_write_data));
        }

        // ── AXI 观测面 ──
        if (awvalid || wvalid || arvalid) ++out.axil_req_cycles;
        if (awvalid) {
            ++out.awvalid_cycles;
            if (!out.saw_awvalid) {
                out.saw_awvalid = true;
                out.first_awvalid = cycle;
                out.first_awaddr = awaddr;
            }
        }
        if (arvalid) {
            ++out.arvalid_cycles;
            if (!out.saw_arvalid) {
                out.saw_arvalid = true;
                out.first_arvalid = cycle;
                out.first_araddr = araddr;
            }
        }
        if (bvalid && !out.saw_bvalid) {
            out.saw_bvalid = true;
            out.first_bvalid = cycle;
        }
        if (rvalid && !out.saw_rvalid) {
            out.saw_rvalid = true;
            out.first_rvalid = cycle;
        }
        out.arvalid_at_end = arvalid;

        // ── 协议违例 ──
        // ① VALID MUST 保持到握手（READY 之前不得撤）
        if (prev_awvalid && !prev_awready && !awvalid) out.valid_dropped = true;
        if (prev_wvalid && !prev_wready && !wvalid) out.valid_dropped = true;
        if (prev_arvalid && !prev_arready && !arvalid) out.valid_dropped = true;
        // ② VALID 期间地址 / 数据 MUST 稳定
        if (prev_awvalid && awvalid && awaddr != prev_awaddr) out.payload_changed = true;
        if (prev_wvalid && wvalid && (wdata != prev_wdata || wstrb != prev_wstrb)) {
            out.payload_changed = true;
        }
        if (prev_arvalid && arvalid && araddr != prev_araddr) out.payload_changed = true;
        // ③ **激励良构性**（⚠️ 这一条判的是 **stub 自己**，不是 DUT —— 见 AC6d 的注释）：
        //    `rvalid` / `bvalid` 是本 stub **驱动的输入** ⇒ ⇒ 「响应早于请求」只会来自
        //    **模型写坏**（`r_due` / `b_due` 在收到请求前就被排上）⇒ ⇒ 若模型坏了，
        //    AC1 会**空洞地**通过（核在未被要求时就拿到数据）⇒ ⇒ 故必须自查。
        if (rvalid && !saw_ar_before) out.resp_before_req = true;
        if (bvalid && !(saw_aw_before && saw_w_before)) out.resp_before_req = true;

        // ── stub 状态更新（基于**本拍**观察到的握手）──
        // `pending_since`：请求线全撤 ⇒ 视为「上一事务已结束」⇒ 下一事务重新计时
        if (!(awvalid || wvalid || arvalid)) {
            pending_since = -1;
        } else if (pending_since < 0) {
            pending_since = cycle;
        }
        if (awvalid && rdy) aw_fired = true;
        if (wvalid && rdy) w_fired = true;
        // AXI 允许 AW / W 先后到达 ⇒ ⇒ 两者**都**握手后才给 B（文件头表格如此声明）
        if (aw_fired && w_fired && b_due < 0) b_due = cycle + 1;
        if (arvalid && rdy) r_due = cycle + 1;

        prev_awvalid = awvalid;
        prev_wvalid = wvalid;
        prev_arvalid = arvalid;
        prev_awready = rdy;
        prev_wready = rdy;
        prev_arready = rdy;
        prev_awaddr = awaddr;
        prev_wdata = wdata;
        prev_wstrb = wstrb;
        prev_araddr = araddr;
        saw_ar_before = saw_ar_before || arvalid;
        saw_aw_before = saw_aw_before || awvalid;
        saw_w_before = saw_w_before || wvalid;

        sim.tick();
    }

    return out;
}

/// MMIO 程序（vector 形态）
std::vector<std::uint32_t> mmio_program() {
    return std::vector<std::uint32_t>(kMmioProgram, kMmioProgram + kMmioProgWords);
}

/// 非 MMIO 程序（vector 形态）
std::vector<std::uint32_t> plain_program() {
    return std::vector<std::uint32_t>(kPlainProgram, kPlainProgram + kPlainProgWords);
}

/// **响应式从设备**档的运行结果（AC1 / AC2 / AC6 三个用例**同配置** ⇒ ⇒ 复用一次运行）
///
/// ⚠️ 函数内静态缓存：仿真**确定性**（无时间/随机源）⇒ ⇒ 复用不削弱判别力；
///    没有它时 ctest 下要跑 3 次整机建图（~40s → ~15s）。
/// ⚠️ `RunOut` 只持**纯数据**（不持端口 / lambda）⇒ ⇒ 器件析构后仍安全。
const RunOut &responsive_run() {
    static const RunOut out = run_core(mmio_program(), StubCfg{});
    return out;
}

}  // namespace

// ---------------------------------------------------------------------------
// 判据
// ---------------------------------------------------------------------------

TEST_CASE("rv32i core ip: the MMIO read value reaches the data path "
          "(through a non-MMIO successor)",
          "[rv32i][mmio][ip]") {
    const RunOut &out = responsive_run();  // 响应式 · ready_lat = 0 · rdata = 0x5A

    INFO("stores=" << out.stores << " store_addr=0x" << std::hex << out.store_addr
                   << " store_data=0x" << out.store_data << std::dec
                   << " writebacks=" << out.writebacks.size());

    // ── AC1a：store 事件**恰好一次** ──
    REQUIRE(out.stores == 1u);
    // ── AC1b：store 的地址 == 手推的 MMIO 写地址 ──
    REQUIRE(out.store_addr == kMmioWriteAddr);
    // ── AC1c：store 的数据 == stub 回的读值（0x5A）⇒ ⇒ 读值真的走通了数据通路 ──
    REQUIRE(out.store_data == kExpectStoreData);
}

TEST_CASE("rv32i core ip: MMIO transaction counts (store events / writebacks)",
          "[rv32i][mmio][ip]") {
    const RunOut &out = responsive_run();

    INFO("stores=" << out.stores << " writebacks=" << out.writebacks.size());

    // ── AC2a：store 事件恰好 1（只有那条 `sw`）──
    REQUIRE(out.stores == 1u);
    // ── AC2b：写回恰好 3（x4 / x6 / x7 · `jal x0` 不写回）──
    REQUIRE(out.writebacks.size() == kExpectWritebacks);

    // ── AC2c：每个 rd 的**数据** == 手推值（rd 出现 + 值正确，各判一次）──
    struct Expect {
        std::uint32_t rd;
        std::uint64_t data;
    };
    const Expect expects[3] = {
        {4u, static_cast<std::uint64_t>(kMmioWriteAddr)},  // x4 = 0x8010_2000
        {6u, kStubRdata},                                   // x6 = 0x5A（MMIO 读回）
        {7u, 1u},                                          // x7 = 1
    };
    for (const auto &e : expects) {
        bool rd_seen = false;
        bool data_seen = false;
        for (const auto &wb : out.writebacks) {
            if (wb.first != e.rd) continue;
            rd_seen = true;
            if (wb.second == e.data) data_seen = true;
        }
        INFO("rd=x" << e.rd << " 期望 data=0x" << std::hex << e.data << std::dec);
        REQUIRE(rd_seen);
        REQUIRE(data_seen);
    }
}

TEST_CASE("rv32i core ip: the core really waits for the slave "
          "(a slave that never answers ⇒ the program never completes)",
          "[rv32i][mmio][ip]") {
    StubCfg cfg;
    cfg.dead = true;  // 反证档：永不应答
    const RunOut out = run_core(mmio_program(), cfg);

    INFO("axil_req_cycles=" << out.axil_req_cycles
                            << " arvalid_cycles=" << out.arvalid_cycles
                            << " stores=" << out.stores
                            << " writebacks=" << out.writebacks.size());

    // ── AC3a：请求**确实发出去了**（否则本用例是空洞的）──
    REQUIRE(out.axil_req_cycles > 0u);
    REQUIRE(out.saw_arvalid);
    // ── AC3b：程序**没有完成**（store 未提交 · 写回不足 3）⇒ ⇒ 核真的在等从设备 ──
    REQUIRE(out.stores == 0u);
    REQUIRE(out.writebacks.size() < kExpectWritebacks);
    // ── AC3c：VALID 保持到握手（AXI 协议）⇒ ⇒ 最后一拍 `arvalid` 仍为高 ──
    REQUIRE(out.arvalid_at_end);
    // ── AC3d：没有响应 —— ⚠️ **这判的是 stub 的 `dead` 配置**（`rvalid` / `bvalid` 是
    //    `ch_in` ⇒ ⇒ DUT **物理上无法**驱动它们）⇒ ⇒ 它是 AC3b 的**前提自检**
    //    （若 stub 反而给了响应，AC3b 的「不完成」就不再归因于「核在等」）。──
    REQUIRE_FALSE(out.saw_rvalid);
    REQUIRE_FALSE(out.saw_bvalid);
}

TEST_CASE("rv32i core ip: a DECERR response does not hang the program",
          "[rv32i][mmio][ip]") {
    StubCfg cfg;
    cfg.bresp = 3u;  // DECERR
    cfg.rresp = 3u;  // DECERR
    const RunOut out = run_core(mmio_program(), cfg);

    INFO("stores=" << out.stores << " writebacks=" << out.writebacks.size());

    // ── AC4a：程序照常完成（错误响应不得挂死流水线）──
    REQUIRE(out.stores == 1u);
    REQUIRE(out.writebacks.size() == kExpectWritebacks);
    // ── AC4b：读值仍正确（错误码不影响数据）──
    REQUIRE(out.store_data == kExpectStoreData);
}

TEST_CASE("rv32i core ip: a non-MMIO program never raises an axil request",
          "[rv32i][mmio][ip]") {
    StubCfg cfg;
    const RunOut out = run_core(plain_program(), cfg);

    INFO("axil_req_cycles=" << out.axil_req_cycles << " stores=" << out.stores
                            << " store_addr=0x" << std::hex << out.store_addr
                            << " store_data=0x" << out.store_data << std::dec
                            << " writebacks=" << out.writebacks.size());

    // ── AC5a：MMIO 请求线**从未拉高**（⇒ pre-cache 分流方向正确）──
    REQUIRE(out.axil_req_cycles == 0u);
    REQUIRE_FALSE(out.saw_arvalid);
    REQUIRE_FALSE(out.saw_awvalid);
    // ── AC5b：非 MMIO 路径的行为**零变化**（store 到字节地址 0 · 数据 53）──
    REQUIRE(out.stores == 1u);
    REQUIRE(out.store_addr == 0u);
    REQUIRE(out.store_data == 53u);
    REQUIRE(out.writebacks.size() == 3u);
}

TEST_CASE("rv32i core ip: the axil channel obeys the AXI4-Lite handshake rules",
          "[rv32i][mmio][ip]") {
    const RunOut &out = responsive_run();

    INFO("awvalid_cycles=" << out.awvalid_cycles
                            << " arvalid_cycles=" << out.arvalid_cycles
                            << " first_arvalid=" << out.first_arvalid
                            << " first_rvalid=" << out.first_rvalid
                            << " first_awvalid=" << out.first_awvalid
                            << " first_bvalid=" << out.first_bvalid);

    // ── AC6a：读 / 写事务**都发生过**（否则本用例空洞）──
    REQUIRE(out.saw_arvalid);
    REQUIRE(out.saw_awvalid);
    REQUIRE(out.saw_rvalid);
    REQUIRE(out.saw_bvalid);
    // ── AC6b：VALID 在 READY 之前不得被撤 ──
    REQUIRE_FALSE(out.valid_dropped);
    // ── AC6c：VALID 期间地址 / 数据不得变 ──
    REQUIRE_FALSE(out.payload_changed);
    // ── AC6d：响应不得早于请求 —— ⚠️ **这条判的是 stub 的良构性，不是 DUT**：
    //    `rvalid` / `bvalid` 由本文件的 stub 驱动 ⇒ ⇒ 该断言防的是「模型在未被请求时
    //    就给了响应」⇒ ⇒ 那会让 AC1 **空洞地**通过（核没被要求就拿到 0x5A）。
    //    ⇒ ⇒ **DUT 侧**的责任由 AC6b（VALID 保持 · 读的是核驱动的 `awvalid`/`wvalid`/
    //    `arvalid`）/ AC6c（载荷稳定）/ AC6f（完成事件不早于 B）承担。
    REQUIRE_FALSE(out.resp_before_req);
    // ⚠️ 下面两条**结构性恒真**（stub 只在观察到请求后才排响应 ⇒ 唯一转红路径 =
    //    「完全没有请求」，已被 AC6a 覆盖）⇒ ⇒ 它们是**激励良构性**，不是 DUT 判据。
    REQUIRE(out.first_rvalid > out.first_arvalid);
    REQUIRE(out.first_bvalid > out.first_awvalid);
    // ── AC6e：总线上的地址 == 手推值（⇒ 分流判据用的就是 MMIO 段）──
    REQUIRE(out.first_araddr == kMmioReadAddr);
    REQUIRE(out.first_awaddr == kMmioWriteAddr);
    // ── AC6f：**完成事件 MUST NOT 早于 B 响应**（DUT 侧 · 有判别力）──
    //    `obs_data_write_en` 是 store 提交事件；若实现「W 握手即完成」（不等 `bvalid`）
    //    ⇒ ⇒ 事件会**早于** B 一拍 ⇒ ⇒ 必红（现行实现等 `in_wr_resp & b_fire` ⇒ 同拍）。
    REQUIRE_FALSE(out.store_cycles.empty());
    for (const auto c : out.store_cycles) {
        INFO("store event cycle=" << c << " first_bvalid=" << out.first_bvalid);
        REQUIRE(c >= out.first_bvalid);
    }
}

// ---------------------------------------------------------------------------
// AC7：**慢从设备**档（`ready_lat = 3`）
// ---------------------------------------------------------------------------
//
// 为什么需要它（critic 轮次 1 · SHOULD_FIX-5）：
// 其余用例都是 `ready_lat = 0` ⇒ ⇒ `ready` 在首次请求后 1 拍就拉高 ⇒ ⇒
// 「VALID MUST 保持到握手」（AC6b）只覆盖 **1 拍**窗口 ⇒ ⇒ 「N>1 拍后才撤 VALID」
// 的变异**逃逸**。⇒ ⇒ 本档把应答推后 3 拍 ⇒ ⇒ VALID 窗口变成多拍。
//
// ⚠️ 期望值手推：从设备**仍然应答** ⇒ ⇒ 事务照样完成 ⇒ 程序照样完成（store 1 / 写回 3）；
//    而 `arvalid` / `awvalid` 的**高位拍数** MUST ≥ `ready_lat`（证明窗口真的跨多拍）。

TEST_CASE("rv32i core ip: a slow slave still completes and VALID is held across the wait",
          "[rv32i][mmio][ip]") {
    StubCfg cfg;
    cfg.ready_lat = 3;  // 从设备「思考」3 拍后才接受地址
    const RunOut out = run_core(mmio_program(), cfg);

    INFO("arvalid_cycles=" << out.arvalid_cycles << " awvalid_cycles=" << out.awvalid_cycles
                           << " stores=" << out.stores
                           << " writebacks=" << out.writebacks.size());

    // ── AC7a：慢从设备照样完成（⇒ 核不假定从设备当拍就绪）──
    REQUIRE(out.stores == 1u);
    REQUIRE(out.store_data == kExpectStoreData);
    REQUIRE(out.writebacks.size() == kExpectWritebacks);
    // ── AC7b：VALID 在多拍等待期间**未撤** · 载荷**未变** ──
    REQUIRE_FALSE(out.valid_dropped);
    REQUIRE_FALSE(out.payload_changed);
    // ── AC7c：窗口**真的跨多拍**（否则 AC6b 的判别力退化成 1 拍）──
    REQUIRE(out.arvalid_cycles >= static_cast<std::uint64_t>(cfg.ready_lat));
    REQUIRE(out.awvalid_cycles >= static_cast<std::uint64_t>(cfg.ready_lat));
    // ── AC7d：地址仍 == 手推值（长时间等待期间不得漂移）──
    REQUIRE(out.first_araddr == kMmioReadAddr);
    REQUIRE(out.first_awaddr == kMmioWriteAddr);
}
