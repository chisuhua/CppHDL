// ⚠️⚠️ **路线 A（E/F 批）—— `riscv::HazardUnitComplete` 进双后端 parity**
// （2026-11-02）
//
// ## 为什么这个文件成本最低
//
// **零 DUT 复制**：`riscv::HazardUnitComplete`（`include/cpu/pipeline/rv32i_hazard_complete.h`）
// **本身就是带顶层端口的 `Component`**（18 个 `ch_in` + 7 个 `ch_out`，全部 ≤32 bit）
// ⇒ ⇒ 直接用库组件当 macro 的 DUT（与 `test_forwarding_dual.cpp` 同型，先例：
// `tests/test_blt_debug_behavior.cpp`）⇒ ⇒ **测的就是生产代码本身**。
//
// 4 问：① 无 dual 兄弟（`tests/chlib/test_hazard_complete.cpp` 是 interpreter-only，
// 且它**只断 1–2 个输出**）· ② 判别力见下变异表 · ③ 真跑仿真读顶层端口 ·
// ④ 纯组合（**无 `ch_reg`**）⇒ **1 拍即可**。
//
// ⚠️ **原文件 MUST NOT 修改**。
// ⚠️ **生产消费者 = 0**（流水线实际用的是 `riscv::HazardUnit`）⇒ ⇒ 本文件的价值在
//    「前推档位 × 优先级 × 停顿 × 冲刷」这套**组合语义契约**，不是「关键路径回归」。
//    **如实登记，不包装成关键路径**。
//
// ---
// ## 🔴 读实现时发现的两个**死端口**（本批不修，如实登记）
//
// `rv32i_hazard_complete.h:35-36` 声明了 `id_is_load` 与 `id_is_branch` **两个输入端口**，
// 但 `describe()`（`:80-192`）**从未读取它们**（实测：两个名字在全文各只出现 **1** 次，
// 即各自的声明处）⇒ ⇒ 属与 `HazardUnit` 被 ADR-047 D7 删掉的那 5 个「预留输入」**同类**的
// 死端口 ⇒ ⇒ 但它们**不在本批范围**（删端口 = 改公开 API ⇒ 需独立立项）。
// ⚠️ 本文件**仍然驱动**它们（未驱动端口读到的是**未定义值**，双后端下两侧未必相同 ⇒
//    即使当前无消费者，依赖「未驱动也无所谓」也是**碰巧通过**）。
//
// ## 期望值（**手推**，禁抄仿真输出）
//
// 语义逐条对照 `describe()`：
//
//     match<N>(a, rd, we)  = (a == rd) ? (we ? (a != 0) : 0) : 0      // 三个级各一份
//     src<N>               = match_EX ? 1 : match_MEM ? 2 : match_WB ? 3 : 0
//     data<N>              = src==1 ? ex_alu_result
//                          : src==2 ? mem_alu_result
//                          : src==3 ? wb_write_data
//                                    : rs<N>_data_reg
//     match_load<N>        = (mem_rd_addr == id_rs<N>_addr) ? (id_rs<N>_addr != 0) : 0
//     stall                = mem_is_load ? (match_load<1> ? 1 : match_load<2>) : 0
//     flush = pc_src       = ex_branch ? ex_branch_taken : 0
//
// ⇒ ⇒ **优先级 EX(1) > MEM(2) > WB(3) > REG(0)** 是本组件最值得钉的一条
// （它是「最近者胜」的编码约定；反过来就是错的数据）。
//
// ### 5 个数据源取**互不相同**的值（任何串源/漏接立刻可见）
//     ex_alu=0x11111111 · mem_alu=0x22222222 · wb=0x33333333
//     rs1_reg=0xAAAA0001 · rs2_reg=0xBBBB0002
//
// ⚠️ **陷阱 T3**：body 参数里**花括号内的逗号**会被预处理器当参数分隔 ⇒ 向量表放**文件作用域**。
// ⚠️ **MUST 每拍驱动全部 18 个输入端口**（含两个死端口，理由见上）。
// ⚠️ **判据（N2′）**：断言数(with verilator) == **2 ×** 断言数(interpreter-only)。
#include "catch_amalgamated.hpp"

#include "cpu/pipeline/rv32i_hazard_complete.h"
#include "dual_backend_fixture.h"
#include "simulator.h"

#include <cstdint>

namespace {

// 5 个数据源（互不相同 ⇒ 串源可见）
constexpr uint64_t kExAlu = 0x11111111ULL;
constexpr uint64_t kMemAlu = 0x22222222ULL;
constexpr uint64_t kWbData = 0x33333333ULL;
constexpr uint64_t kRs1Reg = 0xAAAA0001ULL;
constexpr uint64_t kRs2Reg = 0xBBBB0002ULL;

struct HazardCase {
    // 输入（18 个；id_is_load / id_is_branch 是死端口，恒 0）
    uint64_t id_rs1, id_rs2;
    uint64_t ex_rd, ex_we, ex_branch, ex_branch_taken;
    uint64_t mem_rd, mem_we, mem_is_load;
    uint64_t wb_rd, wb_we;
    // 输出（7 个）
    uint64_t rs1_src, rs2_src;
    uint64_t rs1_data, rs2_data;
    uint64_t stall, flush, pc_src;
};

const HazardCase kCases[] = {
    // 全不动：无任何匹配 ⇒ 两侧都取寄存器堆
    {1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, kRs1Reg, kRs2Reg, 0, 0, 0},
    // RS1 命中 EX ⇒ 档位 1、取 ex_alu
    {5, 2, 5, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, kExAlu, kRs2Reg, 0, 0, 0},
    // RS1 命中 MEM ⇒ 档位 2、取 mem_alu
    {6, 2, 0, 0, 0, 0, 6, 1, 0, 0, 0, 2, 0, kMemAlu, kRs2Reg, 0, 0, 0},
    // RS1 命中 WB ⇒ 档位 3、取 wb_write_data
    {7, 2, 0, 0, 0, 0, 0, 0, 0, 7, 1, 3, 0, kWbData, kRs2Reg, 0, 0, 0},
    // ⚠️ 三处全命中 ⇒ **EX 胜**（优先级判据）
    {8, 2, 8, 1, 0, 0, 8, 1, 0, 8, 1, 1, 0, kExAlu, kRs2Reg, 0, 0, 0},
    // ⚠️ EX 写使能=0（不命中）⇒ **MEM > WB**
    {9, 2, 9, 0, 0, 0, 9, 1, 0, 9, 1, 2, 0, kMemAlu, kRs2Reg, 0, 0, 0},
    // 仅 WB 命中（EX/MEM 同 rd 但 we=0）⇒ 档位 3
    {10, 2, 0, 0, 0, 0, 10, 0, 0, 10, 1, 3, 0, kWbData, kRs2Reg, 0, 0, 0},
    // ⚠️ x0 不前推（三处 rd 都是 0 且 we=1）⇒ 两路都回寄存器堆
    {0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, kRs1Reg, kRs2Reg, 0, 0, 0},
    // 写使能门控：rd 相同但 we=0 ⇒ 不命中
    {11, 2, 11, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, kRs1Reg, kRs2Reg, 0, 0, 0},
    // ⚠️ RS2 独立命中 MEM ⇒ rs2 档位 2、rs1 不受影响
    {3, 12, 0, 0, 0, 0, 12, 1, 0, 0, 0, 0, 2, kRs1Reg, kMemAlu, 0, 0, 0},
    // load-use：MEM 级是 load 且 rd == id_rs1 ⇒ stall
    {13, 2, 0, 0, 0, 0, 13, 0, 1, 0, 0, 0, 0, kRs1Reg, kRs2Reg, 1, 0, 0},
    // load-use：rd == id_rs2 ⇒ stall
    {3, 14, 0, 0, 0, 0, 14, 0, 1, 0, 0, 0, 0, kRs1Reg, kRs2Reg, 1, 0, 0},
    // ⚠️ load-use 的 x0 保护：rd==id_rs1==0 ⇒ **不** stall
    {0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, kRs1Reg, kRs2Reg, 0, 0, 0},
    // branch taken ⇒ flush = pc_src = 1
    {1, 2, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, kRs1Reg, kRs2Reg, 0, 1, 1},
    // branch not taken ⇒ 不冲刷
    {1, 2, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, kRs1Reg, kRs2Reg, 0, 0, 0},
    // ⚠️ 非 branch 但 taken=1 ⇒ **不**冲刷（taken 被 ex_branch 门控）
    {1, 2, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, kRs1Reg, kRs2Reg, 0, 0, 0},
};

}  // namespace

DUAL_BACKEND_TEST_PAIR(
    riscv::HazardUnitComplete,
    "HazardUnitComplete：前推档位/优先级 + load-use 停顿 + 分支冲刷（纯组合，两后端一致）",
    "[chlib][hazard][forwarding][stall][flush][dual]",
    [&]() {
        auto &io = dev.instance().io();
        for (const auto &tc : kCases) {
            // ⚠️ 18 个输入端口**逐个显式驱动**（含两个死端口 `id_is_load` / `id_is_branch`）
            sim.set_input_value(io.id_rs1_addr, tc.id_rs1);
            sim.set_input_value(io.id_rs2_addr, tc.id_rs2);
            sim.set_input_value(io.id_is_load, 0);
            sim.set_input_value(io.id_is_branch, 0);
            sim.set_input_value(io.ex_rd_addr, tc.ex_rd);
            sim.set_input_value(io.ex_reg_write, tc.ex_we);
            sim.set_input_value(io.ex_branch, tc.ex_branch);
            sim.set_input_value(io.ex_branch_taken, tc.ex_branch_taken);
            sim.set_input_value(io.mem_rd_addr, tc.mem_rd);
            sim.set_input_value(io.mem_reg_write, tc.mem_we);
            sim.set_input_value(io.mem_is_load, tc.mem_is_load);
            sim.set_input_value(io.wb_rd_addr, tc.wb_rd);
            sim.set_input_value(io.wb_reg_write, tc.wb_we);
            sim.set_input_value(io.ex_alu_result, kExAlu);
            sim.set_input_value(io.mem_alu_result, kMemAlu);
            sim.set_input_value(io.wb_write_data, kWbData);
            sim.set_input_value(io.rs1_data_reg, kRs1Reg);
            sim.set_input_value(io.rs2_data_reg, kRs2Reg);
            sim.tick();
            INFO("id_rs1=" << tc.id_rs1 << " id_rs2=" << tc.id_rs2
                           << " ex_rd=" << tc.ex_rd << "/we" << tc.ex_we
                           << " mem_rd=" << tc.mem_rd << "/we" << tc.mem_we
                           << "/load" << tc.mem_is_load
                           << " wb_rd=" << tc.wb_rd << "/we" << tc.wb_we
                           << " ex_branch=" << tc.ex_branch << "/taken"
                           << tc.ex_branch_taken);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.rs1_src)) ==
                    tc.rs1_src);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.rs2_src)) ==
                    tc.rs2_src);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.rs1_data)) ==
                    tc.rs1_data);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.rs2_data)) ==
                    tc.rs2_data);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.stall)) ==
                    tc.stall);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.flush)) ==
                    tc.flush);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.pc_src)) ==
                    tc.pc_src);
        }
    });
