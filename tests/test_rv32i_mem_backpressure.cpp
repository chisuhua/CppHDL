// 测试：RV32I MEM 级反压（`data_ready` 真消费）
//
// ## 判据来源
//
// 已签批 spec `openspec/specs/rv32i-memory-timing/spec.md` 的
// **「MEM 级的反压接口 MUST 有可观测的语义」**（含 2 个 Scenario）：
//   ① 反压期间 MEM/WB 冻结 + 有效链自洽 + 向前级发 stall
//   ② 反压 MUST 有非零覆盖（存在能拉低该就绪信号的 DUT 与用例）
// + `ADR-069:176`：反压**不得**混进 `perf_load_use_stalls`。
//
// ## 为什么 DUT = `Rv32iPipeline`（不是核）
//
// `data_ready` 是 `Rv32iPipeline` 的输入（`rv32i_pipeline.h:69`），在核里接
// `dtcm.ready ≡ 1`（恒 1，永不反压）⇒ ⇒ 要「拉低它就绪信号」只能以
// `Rv32iPipeline` 为 DUT（spec 的对象也正是它：**「MEM 级暴露给上级的」**）。
//
// ## 手推期望值（禁抄仿真输出）
//
// 程序 = 重复 `addi x1, x1, 1`（`0x00108093`）⇒ 无 load-use ⇒ 流水线稳定后
// 每拍退休 1 条。固定跑 `kCycles` 拍：
//   · 无反压 ⇒ 退休 `R`
//   · 反压 `N` 拍（冻结型）⇒ 退休 `R - N`（少跑 N 拍等效进度）
// ⇒ ⇒ `R - R_stalled == N`（AC5）
//
// 反压期间「本应每拍变化的量」冻结：
//   · `dbg_wb_write_data`（x1 递增）· `dbg_exmem_alu_result` · `dbg_id_pc`
//   · `perf_instructions`

#include <catch_amalgamated.hpp>

#include <cstdint>
#include <cstdio>
#include <vector>

#include "ch.hpp"
#include "cpu/pipeline/rv32i_pipeline.h"
#include "simulator.h"

namespace {

constexpr uint32_t kBasePc    = 0x80000000u;
constexpr uint32_t kNop       = 0x00000013u;  // addi x0, x0, 0
constexpr uint32_t kAddiX1Inc = 0x00108093u;  // addi x1, x1, 1（WB 每次都写递增的 x1）
constexpr uint32_t kLw        = 0x00002083u;  // lw   x1, 0(x0)
constexpr uint32_t kAddUse    = 0x00100133u;  // add  x2, x0, x1（load-use 消费者）
constexpr uint32_t kBeqSelf   = 0x00000063u;  // beq  x0, x0, 0（恒 taken · 自循环）

struct RunResult {
    std::vector<uint64_t> wb_en;      // 每拍 tick 前 dbg_wb_write_en
    std::vector<uint64_t> wb_data;    // 每拍 tick 前 dbg_wb_write_data
    std::vector<uint64_t> wb_rd;
    std::vector<uint64_t> exmem_alu;  // 每拍 tick 前 dbg_exmem_alu_result
    std::vector<uint64_t> exmem_valid; // 每拍 tick 前 dbg_exmem_valid
    std::vector<uint64_t> id_pc;      // 每拍 tick 前 dbg_id_pc
    std::vector<uint64_t> instrs;     // 每拍 tick 前 perf_instructions
    std::vector<uint64_t> branches;   // 每拍 tick 前 perf_branch_count
    std::vector<uint64_t> ctrl;       // 每拍 tick 前 perf_control_stalls
    std::vector<uint64_t> mispred;    // 每拍 tick 前 perf_branch_mispredict
    std::vector<uint64_t> ex_branch;  // 每拍 tick 前 dbg_id_is_branch（**EX 级**视图）
    uint64_t final_cycles   = 0;
    uint64_t final_instrs   = 0;
    uint64_t final_load_use = 0;
    uint64_t final_branches = 0;
    uint64_t final_ctrl_stalls = 0;
    uint64_t final_mispredict  = 0;
};

RunResult run_pipeline(int cycles, int stall_from, int stall_len,
                       const std::vector<uint32_t>& prog) {
    ch::ch_device<riscv::Rv32iPipeline> dut;
    ch::Simulator sim(dut.context());
    auto& io = dut.instance().io();

    // 复位（2 拍，保持输入有确定值）
    sim.set_input_value(io.rst, 1);
    sim.set_input_value(io.instr_data, kNop);
    sim.set_input_value(io.instr_ready, 1);
    sim.set_input_value(io.data_ready, 1);
    sim.set_input_value(io.data_read_data, 0);
    sim.tick(2);
    sim.set_input_value(io.rst, 0);

    auto fetch = [&](uint32_t pc) -> uint32_t {
        if (pc < kBasePc) return kNop;
        uint64_t idx = (pc - kBasePc) / 4u;
        return (idx < prog.size()) ? prog[idx] : kNop;
    };

    RunResult r;
    r.wb_en.reserve(cycles);
    r.wb_data.reserve(cycles);
    r.wb_rd.reserve(cycles);
    r.exmem_alu.reserve(cycles);
    r.exmem_valid.reserve(cycles);
    r.id_pc.reserve(cycles);
    r.instrs.reserve(cycles);
    r.branches.reserve(cycles);
    r.ctrl.reserve(cycles);
    r.mispred.reserve(cycles);
    r.ex_branch.reserve(cycles);

    for (int c = 0; c < cycles; ++c) {
        const uint64_t pc = static_cast<uint64_t>(sim.get_port_value(io.instr_addr));
        sim.set_input_value(io.instr_data, fetch(static_cast<uint32_t>(pc)));
        sim.set_input_value(io.instr_ready, 1);
        sim.set_input_value(io.data_ready,
                            (c >= stall_from && c < stall_from + stall_len) ? 0 : 1);
        sim.set_input_value(io.data_read_data, 0);

        // 观察：tick **前**的寄存器状态（反压当拍的 tick 才会冻结它）
        r.wb_en.push_back(static_cast<uint64_t>(sim.get_port_value(io.dbg_wb_write_en)));
        r.wb_data.push_back(static_cast<uint64_t>(sim.get_port_value(io.dbg_wb_write_data)));
        r.wb_rd.push_back(static_cast<uint64_t>(sim.get_port_value(io.dbg_wb_rd_addr)));
        r.exmem_alu.push_back(static_cast<uint64_t>(sim.get_port_value(io.dbg_exmem_alu_result)));
        r.exmem_valid.push_back(static_cast<uint64_t>(sim.get_port_value(io.dbg_exmem_valid)));
        r.id_pc.push_back(static_cast<uint64_t>(sim.get_port_value(io.dbg_id_pc)));
        r.instrs.push_back(static_cast<uint64_t>(sim.get_port_value(io.perf_instructions)));
        r.branches.push_back(static_cast<uint64_t>(sim.get_port_value(io.perf_branch_count)));
        r.ctrl.push_back(static_cast<uint64_t>(sim.get_port_value(io.perf_control_stalls)));
        r.mispred.push_back(static_cast<uint64_t>(sim.get_port_value(io.perf_branch_mispredict)));
        r.ex_branch.push_back(static_cast<uint64_t>(sim.get_port_value(io.dbg_id_is_branch)));

        sim.tick();
    }

    r.final_cycles   = static_cast<uint64_t>(sim.get_port_value(io.perf_cycles));
    r.final_instrs   = static_cast<uint64_t>(sim.get_port_value(io.perf_instructions));
    r.final_load_use = static_cast<uint64_t>(sim.get_port_value(io.perf_load_use_stalls));
    r.final_branches = static_cast<uint64_t>(sim.get_port_value(io.perf_branch_count));
    r.final_ctrl_stalls = static_cast<uint64_t>(sim.get_port_value(io.perf_control_stalls));
    r.final_mispredict = static_cast<uint64_t>(sim.get_port_value(io.perf_branch_mispredict));
    return r;
}

std::vector<uint32_t> alu_program() { return std::vector<uint32_t>(40, kAddiX1Inc); }

} // namespace

TEST_CASE("Rv32iPipeline 反压：MEM/WB 冻结（spec Scenario 1）", "[cpu][pipeline][backpressure]") {
    constexpr int kCycles = 30, kStallFrom = 12, kStallLen = 3;
    auto r = run_pipeline(kCycles, kStallFrom, kStallLen, alu_program());

    // tick 前读到的是「上一沿的结果」⇒ 反压生效后的稳定值出现在 [from+1, from+len]
    const uint64_t base = r.wb_data[kStallFrom];
    for (int c = kStallFrom + 1; c <= kStallFrom + kStallLen; ++c) {
        INFO("cycle " << c << " wb_data=" << r.wb_data[c] << " base=" << base);
        REQUIRE(r.wb_data[c] == base);
        REQUIRE(r.wb_rd[c] == r.wb_rd[kStallFrom]);
    }
}

TEST_CASE("Rv32iPipeline 反压：EX/MEM 与 M2 冻结", "[cpu][pipeline][backpressure]") {
    constexpr int kCycles = 30, kStallFrom = 12, kStallLen = 3;
    auto r = run_pipeline(kCycles, kStallFrom, kStallLen, alu_program());

    const uint64_t base = r.exmem_alu[kStallFrom];
    const uint64_t vbase = r.exmem_valid[kStallFrom];
    for (int c = kStallFrom + 1; c <= kStallFrom + kStallLen; ++c) {
        INFO("cycle " << c << " exmem_alu=" << r.exmem_alu[c] << " base=" << base
                      << " exmem_valid=" << r.exmem_valid[c] << " vbase=" << vbase);
        REQUIRE(r.exmem_alu[c] == base);
        // AC2 字面要求：`dbg_exmem_valid` 同样逐拍不变（冻结 ⇒ 不因气泡/新指令而变）
        REQUIRE(r.exmem_valid[c] == vbase);
    }
}

TEST_CASE("Rv32iPipeline 反压：向前级 stall（PC 冻结 + 退休不增）",
          "[cpu][pipeline][backpressure]") {
    constexpr int kCycles = 30, kStallFrom = 12, kStallLen = 3;
    auto r = run_pipeline(kCycles, kStallFrom, kStallLen, alu_program());

    const uint64_t pc0 = r.id_pc[kStallFrom];
    const uint64_t i0  = r.instrs[kStallFrom];
    for (int c = kStallFrom + 1; c <= kStallFrom + kStallLen; ++c) {
        INFO("cycle " << c << " id_pc=" << r.id_pc[c] << " pc0=" << pc0
                      << " instrs=" << r.instrs[c] << " i0=" << i0);
        REQUIRE(r.id_pc[c] == pc0);
        REQUIRE(r.instrs[c] == i0);
    }
}

TEST_CASE("Rv32iPipeline 反压：有效链自洽（退休数 / 写回序列等价）",
          "[cpu][pipeline][backpressure]") {
    constexpr int kCycles = 60, kStallFrom = 15, kStallLen = 4;
    auto prog = alu_program();
    auto plain = run_pipeline(kCycles, -1, 0, prog);
    auto stall = run_pipeline(kCycles, kStallFrom, kStallLen, prog);

    // ① 固定拍数下，反压 N 拍 ⇒ 少退休 **N** 条（手推：冻结型 ⇒ 停 N 拍）
    REQUIRE(plain.final_instrs >= stall.final_instrs);
    REQUIRE(plain.final_instrs - stall.final_instrs ==
            static_cast<uint64_t>(kStallLen));

    // ② 有效链自洽：反压 run 的**写回事件序列** MUST 是 plain 的前缀
    //    （反压只让同一序列「晚到」，**不得**多写 / 错写 / 丢写）
    //    ⚠️ 用「事件」而非「拍」：冻结期间 `write_en` 保持 1 且数据不变
    //    ⇒ ⇒ 连续相同 = **同一条指令停住**（不是多写）⇒ 去重后才是事件。
    auto events = [](const RunResult& r) {
        std::vector<std::pair<uint64_t, uint64_t>> out;
        for (size_t i = 0; i < r.wb_en.size(); ++i) {
            if (r.wb_en[i] == 0) continue;
            std::pair<uint64_t, uint64_t> e{r.wb_rd[i], r.wb_data[i]};
            if (!out.empty() && out.back() == e) continue;  // 冻结期间的同一条
            out.push_back(e);
        }
        return out;
    };
    auto e_plain = events(plain);
    auto e_stall = events(stall);
    REQUIRE(e_stall.size() <= e_plain.size());
    for (size_t i = 0; i < e_stall.size(); ++i) {
        INFO("writeback event #" << i << " plain_rd=" << e_plain[i].first
                                 << " plain_data=" << e_plain[i].second
                                 << " stalled_rd=" << e_stall[i].first
                                 << " stalled_data=" << e_stall[i].second);
        REQUIRE(e_stall[i] == e_plain[i]);
    }
}

TEST_CASE("Rv32iPipeline 反压：覆盖自证（去掉拉低驱动 ⇒ 判据必失败）",
          "[cpu][pipeline][backpressure]") {
    // ⚠️ **覆盖自证**：本用例的判据 = 「反压让 PC 冻结」。
    //    若把 `data_ready` 恒驱动 1（= 不反压）⇒ 该判据 MUST 失败
    //    ⇒ ⇒ 证明判据对「反压通路零覆盖」有判别力（spec Scenario 2 的要求）。
    constexpr int kCycles = 30, kStallFrom = 12, kStallLen = 3;
    auto no_stall = run_pipeline(kCycles, -1, 0, alu_program());

    bool pc_frozen = true;
    for (int c = kStallFrom + 1; c <= kStallFrom + kStallLen; ++c) {
        if (no_stall.id_pc[c] != no_stall.id_pc[kStallFrom]) { pc_frozen = false; break; }
    }
    // 无反压时 PC **不会**冻结 ⇒ 「PC 冻结」这个判据不成立
    REQUIRE_FALSE(pc_frozen);
}

TEST_CASE("Rv32iPipeline 反压：口径分档（不混进 perf_load_use_stalls）",
          "[cpu][pipeline][backpressure]") {
    // 程序含 load-use（`lw` 紧跟 `add`）⇒ 基线 run 的 load_use_stalls > 0
    constexpr int kCycles = 40;
    // ⚠️ **4 条 NOP 前缀**：流水线启动（IF/ID 装载 + skid 填充）要占几拍 ⇒
    //    若 load 紧贴第 1 条，消费者进 ID 时 load 已过 EX 级 ⇒ 检测点错过
    //    （实测：无前缀时 `perf_load_use_stalls == 0`）。
    std::vector<uint32_t> prog = {
        kNop, kNop, kNop, kNop, kLw, kAddUse,
        kNop, kNop, kNop, kNop, kNop, kNop,
    };
    auto plain = run_pipeline(kCycles, -1, 0, prog);
    // 反压放在**启动段**（c=1..6），**不与** load-use 解除段（c≈7..10）重叠
    auto stall = run_pipeline(kCycles, 1, 6, prog);

    // ① 基线确有 load-use 停顿（否则本判据无判别力 ⇒ 平凡成立）
    REQUIRE(plain.final_load_use == 4u);   // 手推：ADR-047 修订后 load-use = 4 拍
    // ② 反压 **不得** 改变 load-use 计数器（`ADR-069:176` 的分档要求）
    //    ⇒ ⇒ 反压 6 拍混进去的话会变成 10 ⇒ 本判据抓到
    INFO("plain load_use=" << plain.final_load_use
                           << " stalled load_use=" << stall.final_load_use);
    REQUIRE(stall.final_load_use == plain.final_load_use);
}

TEST_CASE("Rv32iPipeline 反压：同族计数器（分支/控制停顿/误预测）不得被污染",
          "[cpu][pipeline][backpressure]") {
    // ⚠️ 本用例治的是**同族缺陷**（与 `perf_instructions` 的重复计数同型）：
    //    反压时 EX 级指令**多拍驻留** ⇒ `id_stage.io().is_branch`（ID/EX 寄存，
    //    冻结保持 1）· `ex_stage.branch_taken` · `hazard.flush` 全部稳定为 1
    //    ⇒ ⇒ 不门控就**每停一拍各 +1**。
    //
    // ⚠️ **反压起点 MUST 选在「EX 级确有分支」的那一拍**（实测：若反压开始时
    //    EX 级是气泡，气泡驻留 ⇒ 三个计数器本来就不增 ⇒ 判据**平凡成立**、
    //    无判别力 —— 变异验证曾因此在「去掉门控」时仍全绿）。
    constexpr int kCycles = 30, kStallLen = 8;
    std::vector<uint32_t> prog(16, kBeqSelf);
    auto plain = run_pipeline(kCycles, -1, 0, prog);

    int br_at = -1;
    for (int c = 1; c < kCycles; ++c) {
        if (plain.ex_branch[c] != 0) { br_at = c; break; }
    }
    REQUIRE(br_at >= 1);            // 本程序确有分支进 EX（否则判据退化）
    // ⚠️ 计数器是**寄存**的（+1 在下一拍可见）⇒ 不能断言 `branches[br_at] > 0`；
    //    改看 `br_at + 1`（分支已被计数的那拍）。
    REQUIRE(plain.branches[br_at + 1] > plain.branches[br_at]);

    auto r = run_pipeline(kCycles, br_at, kStallLen, prog);
    const uint64_t b0 = r.branches[br_at];
    const uint64_t c0 = r.ctrl[br_at];
    const uint64_t m0 = r.mispred[br_at];
    for (int c = br_at + 1; c <= br_at + kStallLen; ++c) {
        INFO("cycle " << c << " branches=" << r.branches[c] << " (base " << b0 << ")"
                      << " ctrl=" << r.ctrl[c] << " (base " << c0 << ")"
                      << " mispred=" << r.mispred[c] << " (base " << m0 << ")");
        REQUIRE(r.branches[c] == b0);
        REQUIRE(r.ctrl[c] == c0);
        REQUIRE(r.mispred[c] == m0);
    }
}


