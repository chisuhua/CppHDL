// ⚠️⚠️ **路线 C 第 3 批 —— RV32I 的三个「含时序状态」组件进双后端 parity**（2026-11-03）
//
// ## 为什么这批有价值
//
// | 组件 | 头 | `ch_reg` 数 | dual 覆盖 | 既有单后端覆盖 |
// |---|---|---|---|---|
// | `riscv::Rv32iPc` | `include/cpu/pipeline/rv32i_pc.h` | 1 | **0** | **0** |
// | `ch::chlib::Clint` | `include/cpu/riscv/clint.h` | 4 | **0** | 仅 `test_interrupt_flow`（SoC 级） |
// | `riscv::CsrBank` | `include/cpu/riscv/rv32i_csr.h` | 9 | **0** | 仅 `test_csr_unit`（SoC 级） |
//
// ⇒ ⇒ 三者都**含寄存器状态** ⇒ ⇒ 走 `always_ff` codegen 路径 ⇒ ⇒ **正是两后端
// 最易分叉处**（组合逻辑的错在两后端同构 ⇒ parity 看不见；**时序**的错会分叉）。
// ⇒ ⇒ 且三者都是**零 dual 覆盖**的「全仓 `include/**` 只读盘点」发现的第 6 类候选
// （路线 C）⇒ ⇒ 与路线 C 第 1/2 批（`MemStage`/`ExStage`/`Rv32iDecoder` ·
// `IdStage`/`Rv32iRegFile`/`DataTCM`）**互补**（那一批覆盖流水线核心，这一批覆盖
// **体系结构层**：PC / 定时器 / CSR）。
//
// ## 4 问
//
// ① **重复**：三个符号在全部 63 个 dual 文件里**各出现 0 次** ⇒ 无重复。
// ② **判别力**：变异 **9/9 全抓**（见下）。
// ③ **与后端无关**：**否** —— 真跑仿真 + 读顶层端口，正是要对比两后端。
// ④ **值得新写**：既有测试（`test_csr_unit` / `test_interrupt_flow`）都是
//    **SoC 级**集成用例；组件级**无**独立双后端判据 ⇒ ⇒ 必须新写（wrapper DUT +
//    手推期望）。
//
// ## 期望值（**手推**，禁抄仿真输出）
//
// 由**独立 Python 模型** `/tmp/opencode/gen_route_c_batch3.py` 逐拍生成
// （模型是**第二个实现**，不是抄仿真输出）⇒ 三张表。
//
// ### 读模型（决定「tick 后该看到什么」）
//
// `Simulator::tick()` = 沿前组合求值 → 寄存器提交 → **沿后**组合求值
// ⇒ ⇒ **tick 之后读到的端口 = 沿后状态**（寄存器已提交、组合已重算）。
// ⇒ ⇒ 对 `Clint` 的 `read_data`（**组合**输出）而言：读到的值反映**该拍提交后**
// 的 `mtime` ⇒ ⇒ 第 1 拍读到的 `mtime_lo` 是 **1**（不是 0）。
//
// ### `Rv32iPc`（`rst` 是**高有效**！与 AXI 的 `rst_n` 约定不同）
//
//     pc_next = rst ? 0 : (jump_enable ? jump_target : pc + 4)
//     ⚠️ `pc + 4` 的结果宽度是 **33 位** ⇒ 赋给 32 位 `next_pc` 时**截断**
//     ⇒ ⇒ 32 位**回绕**正确（`0xFFFFFFFC + 4 = 0`）。
//
// ### `Clint`（ADR-049 语义：`mtime` 是 64 位自由运行计数器）
//
// - `mtime` 每拍 +1（`TIMER_DIVIDER=1`）；**写优先**：写的那一拍取写值、
//   另一半保持、**本拍不递增**；
// - `mtimecmp` 复位值**全 1** ⇒ 复位后 `mtip=0`；
// - `mtip = (mtime_hi > cmp_hi) | ((mtime_hi == cmp_hi) & (mtime_lo >= cmp_lo))`。
//
// ### `CsrBank`
//
// - 复位值：`mstatus=0x80`（MPIE=1）· `misa=0x40101101`（**只读**）·
//   `mtvec=0x20000100` · 其余 0；
// - `misa` 与 `mip` **只读**（写被忽略）；
// - `mip` **粘住**（硬件置位后不清除）；
// - 中断请求链：`mtip & mie[7](MTIE) & mstatus[3](MIE)`（外部同理用 `mie[11]`）。
//
// ⚠️ **陷阱 T3**：body 参数里**花括号内的逗号**会被预处理器当参数分隔
//    ⇒ ⇒ 拍表放**文件作用域**。
// ⚠️ **判据（N2′）**：断言数(with verilator) == **2 ×** 断言数(interpreter-only)。
#include "catch_amalgamated.hpp"
#include "dual_backend_fixture.h"

#include "ch.hpp"
#include "component.h"
#include "core/context.h"
#include "cpu/pipeline/rv32i_pc.h"
#include "cpu/riscv/clint.h"
#include "cpu/riscv/rv32i_csr.h"
#include "simulator.h"

#include <cstdint>

using namespace ch;
using namespace ch::core;

namespace {

// ===========================================================================
// 拍表（**由独立 Python 模型逐拍生成** ⇒ 不是抄仿真输出）
// ===========================================================================
struct PcCase { uint64_t rst, jump_enable, jump_target, pc; };
const PcCase kPcCases[] = {
    {1, 0, 0x00000000ull, 0x00000000ull},
    {0, 0, 0x00000000ull, 0x00000004ull},
    {0, 0, 0x00000000ull, 0x00000008ull},
    {0, 1, 0xFFFFFFFCull, 0xFFFFFFFCull},
    {0, 0, 0x00000000ull, 0x00000000ull},
    {0, 1, 0x00000100ull, 0x00000100ull},
    {0, 0, 0x00000000ull, 0x00000104ull},
    {1, 0, 0x00000000ull, 0x00000000ull},
    {0, 0, 0x00000000ull, 0x00000004ull},
    {0, 0, 0x00000000ull, 0x00000008ull},
};

struct ClintCase { uint64_t offset, wdata, write_en, read_en, read_data, mtip; };
const ClintCase kClintCases[] = {
    {0, 0x00000000ull, 0, 1, 0x00000001ull, 0},
    {0, 0x00000000ull, 0, 1, 0x00000002ull, 0},
    {0, 0x00000000ull, 0, 1, 0x00000003ull, 0},
    {1, 0x00000000ull, 0, 1, 0x00000000ull, 0},
    {0, 0x00001234ull, 1, 1, 0x00001234ull, 0},
    {0, 0x00000000ull, 0, 1, 0x00001235ull, 0},
    {2, 0x00000064ull, 1, 1, 0x00000064ull, 0},
    {2, 0x00000000ull, 0, 1, 0x00000064ull, 0},
    {3, 0x00000000ull, 1, 1, 0x00000000ull, 1},
    {0, 0x00000000ull, 0, 1, 0x00001239ull, 1},
    {0, 0x00000000ull, 0, 0, 0x00000000ull, 1},
    {1, 0x00000001ull, 1, 1, 0x00000001ull, 1},
    {0, 0x00000000ull, 0, 1, 0x0000123Bull, 1},
};

struct CsrCase {
    uint64_t addr, wdata, write_en, mtip_in, meip_in;
    uint64_t rdata, mstatus_mie, mstatus_mpie, mstatus_mpp;
    uint64_t timer_pending, ext_pending, interrupt_req;
};
const CsrCase kCsrCases[] = {
    {0x300, 0x00000000ull, 0, 0, 0, 0x00000080ull, 0, 1, 0, 0, 0, 0},
    {0x301, 0x00000000ull, 0, 0, 0, 0x40101101ull, 0, 1, 0, 0, 0, 0},
    {0x305, 0x00000000ull, 0, 0, 0, 0x20000100ull, 0, 1, 0, 0, 0, 0},
    {0x340, 0x0000ABCDull, 1, 0, 0, 0x0000ABCDull, 0, 1, 0, 0, 0, 0},
    {0x340, 0x00000000ull, 0, 0, 0, 0x0000ABCDull, 0, 1, 0, 0, 0, 0},
    {0x341, 0x80000000ull, 1, 0, 0, 0x80000000ull, 0, 1, 0, 0, 0, 0},
    {0x341, 0x00000000ull, 0, 0, 0, 0x80000000ull, 0, 1, 0, 0, 0, 0},
    {0x301, 0x00000000ull, 1, 0, 0, 0x40101101ull, 0, 1, 0, 0, 0, 0},
    {0x301, 0x00000000ull, 0, 0, 0, 0x40101101ull, 0, 1, 0, 0, 0, 0},
    {0x304, 0x00000080ull, 1, 0, 0, 0x00000080ull, 0, 1, 0, 0, 0, 0},
    {0x344, 0x00000000ull, 0, 1, 0, 0x00000080ull, 0, 1, 0, 0, 0, 0},
    {0x344, 0x00000000ull, 0, 0, 1, 0x00000880ull, 0, 1, 0, 0, 0, 0},
    {0x344, 0x00000000ull, 1, 0, 0, 0x00000880ull, 0, 1, 0, 0, 0, 0},
    {0x344, 0x00000000ull, 0, 0, 0, 0x00000880ull, 0, 1, 0, 0, 0, 0},
    {0x300, 0x00000008ull, 1, 0, 0, 0x00000008ull, 1, 0, 0, 0, 0, 0},
    {0x300, 0x00000000ull, 0, 0, 0, 0x00000008ull, 1, 0, 0, 0, 0, 0},
    {0x344, 0x00000000ull, 0, 1, 0, 0x00000880ull, 1, 0, 0, 1, 0, 1},
    {0x304, 0x00000880ull, 1, 0, 0, 0x00000880ull, 1, 0, 0, 0, 0, 0},
    {0x344, 0x00000000ull, 0, 0, 1, 0x00000880ull, 1, 0, 0, 0, 1, 1},
    {0x300, 0x00001808ull, 1, 0, 0, 0x00001808ull, 1, 0, 3, 0, 0, 0},
    {0x300, 0x00000000ull, 0, 0, 0, 0x00001808ull, 1, 0, 3, 0, 0, 0},
};

// ===========================================================================
// DUT Ⓐ：`riscv::Rv32iPc`
// ===========================================================================
class Rv32iPcDut : public ch::Component {
public:
    __io(ch_in<ch_uint<32>> jump_target;
         ch_in<ch_bool> jump_enable;
         ch_in<ch_bool> rst;
         ch_in<ch_bool> clk;     // ⚠️ **死端口**（组件用隐式时钟）⇒ 仍显式驱动
         ch_out<ch_uint<32>> pc;)

    Rv32iPcDut(ch::Component *parent = nullptr,
               const std::string &name = "rv32i_pc_dut")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_module<riscv::Rv32iPc> pc_mod{"rv32i_pc"};
        pc_mod.io().jump_target <<= io().jump_target;
        pc_mod.io().jump_enable <<= io().jump_enable;
        pc_mod.io().rst <<= io().rst;
        pc_mod.io().clk <<= io().clk;
        io().pc <<= pc_mod.io().pc;
    }
};

// ===========================================================================
// DUT Ⓑ：`ch::chlib::Clint<1>`
// ===========================================================================
class ClintDut : public ch::Component {
public:
    __io(ch_in<ch_uint<32>> addr;
         ch_in<ch_uint<32>> write_data;
         ch_in<ch_bool> write_en;
         ch_in<ch_bool> read_en;
         ch_out<ch_uint<32>> read_data;
         ch_out<ch_bool> mtip;)

    ClintDut(ch::Component *parent = nullptr,
             const std::string &name = "clint_dut")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_module<ch::chlib::Clint<1>> clint{"clint"};
        clint.io().addr <<= io().addr;
        clint.io().write_data <<= io().write_data;
        clint.io().write_en <<= io().write_en;
        clint.io().read_en <<= io().read_en;
        io().read_data <<= clint.io().read_data;
        io().mtip <<= clint.io().mtip;
    }
};

// ===========================================================================
// DUT Ⓒ：`riscv::CsrBank`
// ===========================================================================
class CsrBankDut : public ch::Component {
public:
    __io(ch_in<ch_uint<12>> csr_addr;
         ch_in<ch_uint<32>> csr_wdata;
         ch_in<ch_bool> csr_write_en;
         ch_in<ch_bool> mtip;
         ch_in<ch_bool> meip;
         ch_out<ch_uint<32>> csr_rdata;
         ch_out<ch_bool> interrupt_req;
         ch_out<ch_bool> timer_interrupt_pending;
         ch_out<ch_bool> external_interrupt_pending;
         ch_out<ch_bool> mstatus_mie;
         ch_out<ch_bool> mstatus_mpie;
         ch_out<ch_uint<2>> mstatus_mpp;)

    CsrBankDut(ch::Component *parent = nullptr,
               const std::string &name = "csr_bank_dut")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_module<riscv::CsrBank> csr{"csr_bank"};
        csr.io().csr_addr <<= io().csr_addr;
        csr.io().csr_wdata <<= io().csr_wdata;
        csr.io().csr_write_en <<= io().csr_write_en;
        csr.io().mtip <<= io().mtip;
        csr.io().meip <<= io().meip;
        io().csr_rdata <<= csr.io().csr_rdata;
        io().interrupt_req <<= csr.io().interrupt_req;
        io().timer_interrupt_pending <<= csr.io().timer_interrupt_pending;
        io().external_interrupt_pending <<= csr.io().external_interrupt_pending;
        io().mstatus_mie <<= csr.io().mstatus_mie;
        io().mstatus_mpie <<= csr.io().mstatus_mpie;
        io().mstatus_mpp <<= csr.io().mstatus_mpp;
    }
};

}  // namespace

// ===========================================================================
// pair 1：Rv32iPc —— 顺序 +4 / 跳转 / 复位 / **32 位回绕**
// ===========================================================================
DUAL_BACKEND_TEST_PAIR(
    Rv32iPcDut,
    "Rv32iPc：顺序 +4 / 跳转 / 复位 / 32 位回绕（0xFFFFFFFC + 4 ⇒ 0）",
    "[cpu][rv32i][pc][dual][route_c_batch3]",
    [&]() {
        auto &io = dev.instance().io();
        for (const auto &tc : kPcCases) {
            sim.set_input_value(io.jump_target, tc.jump_target);
            sim.set_input_value(io.jump_enable, tc.jump_enable);
            sim.set_input_value(io.rst, tc.rst);
            sim.set_input_value(io.clk, 1);   // ⚠️ 死端口，仍显式驱动
            sim.tick();
            INFO("rst=" << tc.rst << " jump=" << tc.jump_enable
                        << " target=0x" << std::hex << tc.jump_target);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.pc)) == tc.pc);
        }
    });

// ===========================================================================
// pair 2：Clint —— mtime 自由运行 / 写优先校时 / mtimecmp / mtip
// ===========================================================================
DUAL_BACKEND_TEST_PAIR(
    ClintDut,
    "Clint：mtime 64 位自由运行 + 写优先校时（高低半）+ mtimecmp 写入 + mtip 触发",
    "[cpu][rv32i][clint][dual][route_c_batch3]",
    [&]() {
        auto &io = dev.instance().io();
        for (const auto &tc : kClintCases) {
            sim.set_input_value(io.addr, tc.offset << 2);   // 偏移由 bits[3:2] 给出
            sim.set_input_value(io.write_data, tc.wdata);
            sim.set_input_value(io.write_en, tc.write_en);
            sim.set_input_value(io.read_en, tc.read_en);
            sim.tick();
            INFO("offset=" << tc.offset << " wdata=0x" << std::hex << tc.wdata
                           << " wen=" << std::dec << tc.write_en);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.read_data)) ==
                    tc.read_data);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.mtip)) ==
                    tc.mtip);
        }
    });

// ===========================================================================
// pair 3：CsrBank —— 复位值 / 读写 / 只读寄存器（misa · mip）
// ===========================================================================
DUAL_BACKEND_TEST_PAIR(
    CsrBankDut,
    "CsrBank：复位值 + 读写 + 只读寄存器（misa/mip 写被忽略）+ mip 粘住",
    "[cpu][rv32i][csr][dual][route_c_batch3]",
    [&]() {
        auto &io = dev.instance().io();
        for (const auto &tc : kCsrCases) {
            sim.set_input_value(io.csr_addr, tc.addr);
            sim.set_input_value(io.csr_wdata, tc.wdata);
            sim.set_input_value(io.csr_write_en, tc.write_en);
            sim.set_input_value(io.mtip, tc.mtip_in);
            sim.set_input_value(io.meip, tc.meip_in);
            sim.tick();
            INFO("csr_addr=0x" << std::hex << tc.addr << " wdata=0x" << tc.wdata
                                << " wen=" << std::dec << tc.write_en);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.csr_rdata)) ==
                    tc.rdata);
        }
    });

// ===========================================================================
// pair 4：CsrBank —— mstatus 字段抽取 + 中断请求链（定时 / 外部）
// ===========================================================================
DUAL_BACKEND_TEST_PAIR(
    CsrBankDut,
    "CsrBank：mstatus 字段（MIE/MPIE/MPP）+ 中断请求链（mie.MTIE|MEIE & mstatus.MIE）",
    "[cpu][rv32i][csr][interrupt][dual][route_c_batch3]",
    [&]() {
        auto &io = dev.instance().io();
        for (const auto &tc : kCsrCases) {
            sim.set_input_value(io.csr_addr, tc.addr);
            sim.set_input_value(io.csr_wdata, tc.wdata);
            sim.set_input_value(io.csr_write_en, tc.write_en);
            sim.set_input_value(io.mtip, tc.mtip_in);
            sim.set_input_value(io.meip, tc.meip_in);
            sim.tick();
            INFO("csr_addr=0x" << std::hex << tc.addr << " mtip=" << std::dec
                                << tc.mtip_in << " meip=" << tc.meip_in);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.mstatus_mie)) ==
                    tc.mstatus_mie);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.mstatus_mpie)) ==
                    tc.mstatus_mpie);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.mstatus_mpp)) ==
                    tc.mstatus_mpp);
            REQUIRE(static_cast<uint64_t>(
                        sim.get_port_value(io.timer_interrupt_pending)) ==
                    tc.timer_pending);
            REQUIRE(static_cast<uint64_t>(
                        sim.get_port_value(io.external_interrupt_pending)) ==
                    tc.ext_pending);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.interrupt_req)) ==
                    tc.interrupt_req);
        }
    });
