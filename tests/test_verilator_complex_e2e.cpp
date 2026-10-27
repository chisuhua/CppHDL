// tests/test_verilator_complex_e2e.cpp
// Reference implementation for DUAL_BACKEND_TEST_PAIR macro
// (see openspec/specs/dual-backend-test-coverage/spec.md). Retain
// hand-crafted structure (spec R9).
//
// strengthen-verilator-e2e-coverage phase 7 (GREEN):
// 2 representative complex design fixtures — RV32I pipeline + SPI FSM —
// under VerilatorBackend simulation. Each fixture runs interpreter
// + verilator side-by-side and compares data_map_ end-state.
//
// Fixture 1 — Rv32iPipelineFixture:
//   ch_module<Rv32iPipeline> with 3 instructions in imem:
//     addi x1, x0, 1  ;  addi x2, x0, 2  ;  add x3, x1, x2
//   D-TCM stub (write=0). Observation: perf_instructions final == 3
//   (top-level ch_out, verilator-observable per design D1 invariant 4).
//
// Fixture 2 — SpiFsmFixture:
//   ch_state_machine with 4 states: IDLE -> START -> TRANSFER -> STOP
//   -> IDLE. Send 1 byte 0xA5 (8-bit shift register), advance 8
//   transfers; observe `dbg_state` ch_out for state-transition parity.
//
// Tagged [verilator][e2e][complex]. Skipped cleanly when verilator is
// not on PATH; CATCH_CONFIG_RUNNER keeps the executable exit 0 when
// all tests SKIP.
//
// TIMEOUT 600s (add_catch_test does not support TIMEOUT directly;
// set_tests_properties applied in CMakeLists.txt). Even with TIMEOUT
// 600, RV32I verilator compile can be slow on cold cache; if so the
// TEST_CASE SKIPs and the change is complete without archive blocking.
#include <unistd.h>
#include <cstdio>
#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "chlib/state_machine.h"
#include "component.h"
#include "core/context.h"
#include "core/eval_backend.h"
#include "core/interpreter_backend.h"
#include "core/verilator_backend.h"
#include "core/operators.h"
#include "cpu/pipeline/rv32i_pipeline.h"
#include "device.h"
#include "simulator.h"
#include <array>
#include <cstdlib>
#include <memory>
#include <string>
#include <sys/stat.h>

#define CATCH_CONFIG_RUNNER

using namespace ch;
using namespace ch::core;
using namespace ch::core::literals;

namespace {

bool tool_available(const char *cmd) {
    std::string which_cmd = std::string("command -v ") + cmd +
                            " >/dev/null 2>&1";
    return std::system(which_cmd.c_str()) == 0;
}

std::string make_temp_dir(const std::string &suffix) {
    char tmpl[] = "/tmp/cpphdl_vl_cx_XXXXXX";
    char *dir = ::mkdtemp(tmpl);
    if (!dir) return {};
    std::string result(dir);
    result += suffix;
    ::mkdir(result.c_str(), 0755);
    return result;
}

uint64_t read_port_output(Simulator &sim, ch::core::lnodeimpl *ln) {
    auto it = sim.data_map().find(ln->id());
    if (it == sim.data_map().end()) return UINT64_MAX;
    return static_cast<uint64_t>(it->second);
}

// ---------------------------------------------------------------------------
// SPI FSM fixture (ch_state_machine DSL).
// 4 states: IDLE(0), START(1), TRANSFER(2), STOP(3). Send 1 byte
// 0xA5 (8-bit shift register, MSB-first). Cycle through 8 transfers
// and a STOP. We expose:
//   - dbg_state: ch_uint<2> state register (top-level ch_out, so
//     verilator's port_access_ can read it)
//   - mosi_out: ch_bool that pulses high on each bit emitted
// ---------------------------------------------------------------------------
enum class SpiState : uint8_t {
    IDLE = 0,
    START = 1,
    TRANSFER = 2,
    STOP = 3,
};

class SpiFsmFixture : public ch::Component {
public:
    __io(
        ch_in<ch_bool>      start;
        ch_in<ch_uint<8>>   data_in;
        ch_in<ch_bool>      miso_in;
        ch_out<ch_uint<2>>  dbg_state;
        ch_out<ch_bool>     mosi_out;
        ch_out<ch_uint<3>>  dbg_bit_count;
    )
    SpiFsmFixture(ch::Component *parent = nullptr,
                  const std::string &name = "spi_fsm")
        : ch::Component(parent, name) {}
    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        // Bit counter — counts up to 7 (8 transfers total).
        ch_reg<ch_uint<3>> bit_count(0_d, "bit_count");

        // Shift register — drives mosi_out (MSB-first).
        ch_reg<ch_uint<8>> shift(0_d, "shift");

        // ch_state_machine.
        chlib::ch_state_machine<SpiState, 4> sm;

        // IDLE: wait for start. On start, load shift + reset bit_count,
        // transition to START.
        sm.state(SpiState::IDLE).on_active([&] {
            // The ch_in<ch_bool> port has no direct ch_bool conversion
            // (see io.h:232 only ch_uint has it). We drive a ch_bool
            // reg from the port via select(); the reg updates at
            // posedge and becomes the transition_when condition.
            ch_reg<ch_bool> start_latch(ch_bool(false), "start_latch");
            start_latch->next = io().start;
            ch_bool start_now = start_latch;
            sm.transition_when(start_now, SpiState::START);
            // When the start signal is asserted (in IDLE), capture
            // data_in into shift and reset bit_count.
            shift->next = select(start_now, io().data_in, shift);
            bit_count->next = select(start_now, make_literal<0>(),
                                     bit_count);
        });

        // START: pulse 1 cycle, then advance to TRANSFER.
        sm.state(SpiState::START).on_active([&] {
            sm.transition_when(ch_bool(true), SpiState::TRANSFER);
        });

        // TRANSFER: emit current MSB, advance shift + bit_count. After
        // bit_count == 7, transition to STOP.
        sm.state(SpiState::TRANSFER).on_active([&] {
            ch_bool last_bit = (bit_count == make_literal<7>());
            // Stay for 1 cycle.
            sm.transition_when(ch_bool(true),
                               last_bit ? SpiState::STOP
                                        : SpiState::TRANSFER);
            bit_count->next = bit_count + 1_d;
            shift->next = shift << 1_d;
        });

        // STOP: pulse 1 cycle, then back to IDLE.
        sm.state(SpiState::STOP).on_active([&] {
            sm.transition_when(ch_bool(true), SpiState::IDLE);
        });

        sm.set_entry(SpiState::IDLE);
        sm.build();

        // Output assignments.
        io().dbg_state = sm.current_state_uint();
        // mosi_out: in TRANSFER, top bit of shift (MSB). In other
        // states, 0. We extract the MSB via bit_select<7>(shift).
        ch_bool in_transfer = sm.is_in(SpiState::TRANSFER);
        ch_bool msb = bit_select<7>(shift);
        io().mosi_out = select(in_transfer, msb, ch_bool(false));
        io().dbg_bit_count = bit_count;
    }
};

// ---------------------------------------------------------------------------
// RV32I 5-stage pipeline fixture.
//
// Loads 3 short RV32I instructions into a 4-word ch_mem imem (sread,
// posedge-aligned for verilator parity per verilator-e2e-coverage spec):
//   addi x1, x0, 1   => 0x00000094  (I-type: imm=1, rs1=0, rd=1, op=0x13)
//   addi x2, x0, 2   => 0x00000113  (I-type: imm=2, rs1=0, rd=2, op=0x13)
//   add  x3, x1, x2  => 0x002081B3  (R-type: rs2=2, rs1=1, rd=3, op=0x33)
//   <padding>        => 0x00000013  (addi x0, x0, 0)
//
// D-TCM stub: data_write_en=0, data_read_data=0 (3 short ALU instructions
// never issue loads/stores so the stub is observationally inert).
//
// Observation point: io().perf_instructions (top-level ch_out<ch_uint<48>>;
// verilator-observable per ADR-035 §M6). After 20 cycles the 3
// instructions must have committed; perf_instructions == 3 on both backends.
//
// The TEST_CASE feeds `cpu.io().instr_data` directly from the testbench
// (mirrors an external imem that is observable to the testbench but not
// verilator-deep-internal). This is the same shape used by the
// riscv-mini stage tests in examples/riscv-mini/tests/.
// ---------------------------------------------------------------------------
class Rv32iPipelineFixture : public ch::Component {
public:
    __io(
        ch_out<ch_uint<48>> perf_instructions;
        // ⚠️ 写回观测（2026-10-25，change rv32i-pipeline-commit-path-2026-10）：
        //    `perf_instructions == N` 是**可被伪事件满足**的断言（钉不住行为）
        //    ⇒ 必须能**看到寄存器堆的写回值**才能断言 ISA 语义。
        ch_out<ch_bool>     wb_write_en;
        ch_out<ch_uint<32>> wb_write_data;
        ch_out<ch_uint<5>>  wb_rd_addr;
    )
    Rv32iPipelineFixture(ch::Component *parent = nullptr,
                         const std::string &name = "rv32i_top")
        : ch::Component(parent, name) {}
    void create_ports() override { new (io_storage_) io_type; }
    void describe() override {
        ch::ch_module<riscv::Rv32iPipeline> cpu{"cpu"};

        // Internal imem: a 16-word ch_mem holding the **3 effective test
        // instructions + 3 NOPs** (6 words total) + 10 padding words.
        // The IF stage pc_reg resets to 0x80000000 (if_stage.h:66); we map
        // that PC range to our 16-word array via the low **4** bits of
        // (pc >> 2) — i.e. PC[6:15] = word index in the 16-word array
        // (`bits<5,2>` ⇒ 4 bits ⇒ all 16 words reachable), padded with
        // NOPs for cycles that fetch beyond program[5].
        //
        // NOTE (verilator-e2e-coverage spec §Requirement 1 invariant
        // 3): uses ch_mem sread (posedge aligned) — aread would be
        // 0-cycle comb in Verilog but posedge in Simulator, breaking
        // trace parity. sread is safe for both backends.
        // ⚠️ 16 字（原来 4 字）：程序 MUST **无数据冒险** ——
        //    lib 版 `HazardUnit` **不接数据前推**
        //    （include/cpu/pipeline/hazard_unit.h: `io().rs1_data = io().rs1_data_raw`）
        //    ⇒ 生产者→消费者之间必须插 NOP，否则读到的是**上一条**的值。
        //    程序（**6 字**：3 条有效指令 + 3 条 NOP；其余 10 个字为 NOP 填充）：
        //      0: addi x1, x0, 1   = 0x00100093
        //      1: addi x2, x0, 2   = 0x00200113
        //      2,3,4: NOP          （等 x1/x2 写回：生产者 WB 在 ID+3 拍）
        //      5: add  x3, x1, x2  = 0x002081B3   ⇒ x3 == 3
        //      6..15: NOP
        //    ⇒ x3 == 3 是**ISA 正确**的值，可被外部观测（写回数据）验证。
        //    ⚠️ NOP 数 = 3：没有前推时，生产者的写回发生在 ID+3 拍
        //    （EX→MEM→WB）⇒ 消费者至少要隔 3 条指令。
        ch_mem<ch_uint<32>, 16> imem{
            std::array<uint32_t, 16>{0x00100093u, 0x00200113u,
                                     0x00000013u, 0x00000013u,
                                     0x00000013u,
                                     0x002081B3u, 0x00000013u,
                                     0x00000013u, 0x00000013u,
                                     0x00000013u, 0x00000013u,
                                     0x00000013u, 0x00000013u,
                                     0x00000013u, 0x00000013u},
            "imem"};

        // instr_data is sread from imem[pc[6:2]] (the low **5** address
        // bits after shifting right by 2) ⇒ 16 个字里选 1 个；
        // ch_bool(true) always enables the read (no gating).
        //
        // ⚠️⚠️ 位宽 MUST 与 `ch_mem` 的容量**一致**（2026-10-25，第 15 轮 critic
        //    SHOULD_FIX #2 —— **第 14 轮我误判了这条 NIT，现纠正**）：
        //    `bits<MSB,LSB>` 是**闭区间** ⇒ 宽度 = MSB−LSB+1 ⇒
        //    `bits<4,2>` 只有 **3 位** ⇒ 只可达 **8 个字** ⇒ 与
        //    `ch_mem<ch_uint<32>, 16>` **不匹配**（后 8 个字不可达 ⇒ 程序
        //    放过 word 7 就会静默回绕）。
        //    ⇒ 正确写法：`bits<5,2>`（4 位 ⇒ 16 字 ✔）。
        //
        // ⚠️ **必须把 read_port 直接接到端口**（2026-10-25 实测修复）：
        //    此前先落进一个 `ch_uint<4>` 中间变量 ⇒ 宽度不匹配（read_port 的
        //    元素类型是 `ch_uint<32>`）⇒ 取到的**不是读数据**（实测恒为 3）
        //    ⇒ 这个 fixture 的 imem **从来没有真正工作过**，而旧断言
        //    `perf_instructions == 12` 钉的是**垃圾值**（"绿 ≠ 对"）。
        //    先例：`include/cpu/pipeline/rv32i_tcm.h` 的 `InstrTCM` 也是
        //    直接 `io().data <<= read_data;`。
        auto word_sel = imem.sread(bits<5, 2>(cpu.io().instr_addr),
                                   ch_bool(true));

        // Drive the pipeline.
        cpu.io().instr_data <<= word_sel;
        cpu.io().instr_ready <<= ch_bool(true);

        // Data memory stub — no loads/stores in this fixture.
        // ⚠️ **不要**驱动 `data_write_en`（2026-10-25，第 14 轮 critic NIT）：
        //    它是 `Rv32iPipeline` 的**输出**端口（由流水线自己驱动
        //    `io().data_write_en <<= exmem_is_store`）⇒ 父组件驱动子组件的
        //    输出端口是**反向**连接（写法可疑，语义未定义）。
        //    ⇒ 本 fixture 不需要它：该固件**无 store** ⇒ 流水线侧恒 0。
        cpu.io().data_read_data <<= ch_uint<32>(0_d);
        cpu.io().data_ready <<= ch_bool(true);

        // Reset held low (3 short instructions start at PC=0x80000000
        // directly without a reset cycle).
        cpu.io().rst <<= ch_bool(false);

        // Expose perf_instructions + 写回（ISA 语义断言的观测面）
        io().perf_instructions <<= cpu.io().perf_instructions;
        io().wb_write_en <<= cpu.io().dbg_wb_write_en;
        io().wb_write_data <<= cpu.io().dbg_wb_write_data;
        io().wb_rd_addr <<= cpu.io().dbg_wb_rd_addr;
    }
};

} // namespace

// ============================================================================
// SpiFsm: assert state register parity after walking IDLE → START →
// TRANSFER → STOP → IDLE once.
// ============================================================================
TEST_CASE("ComplexE2E - SpiFsmStateTransitionTraceParity",
          "[verilator][e2e][complex][spi-fsm][m5]") {
    if (!tool_available("verilator")) {
        SKIP("verilator not on PATH");
    }

    auto dev_a = std::make_unique<ch::ch_device<SpiFsmFixture>>();
    auto dev_b = std::make_unique<ch::ch_device<SpiFsmFixture>>();
    auto sim_a = std::make_unique<Simulator>(dev_a->context());
    auto sim_b = std::make_unique<Simulator>(dev_b->context());

    sim_a->set_backend(std::make_unique<InterpreterBackend>());
    ch::data_map_t data_map_b;
    auto workdir = make_temp_dir("_cx");
    REQUIRE_FALSE(workdir.empty());
    auto backend = std::make_unique<VerilatorBackend>(workdir);
    REQUIRE(backend->initialize(dev_b->context(), data_map_b));
    if (backend->port_access_snapshot().empty()) {
        SKIP("verilator dlopen did not populate port_access_");
    }
    sim_b->set_backend(std::move(backend));

    auto *state_a = dev_a->instance().io().dbg_state.impl();
    auto *state_b = dev_b->instance().io().dbg_state.impl();
    auto *count_a = dev_a->instance().io().dbg_bit_count.impl();
    auto *count_b = dev_b->instance().io().dbg_bit_count.impl();

    // Walk 32 cycles; toggle start on cycle 0 only.
    for (uint32_t cycle = 0; cycle < 32; ++cycle) {
        bool start_now = (cycle == 0);
        auto drive = [&](ch::Simulator &sim,
                         ch::ch_device<SpiFsmFixture> &dev) {
            sim.set_input_value(dev.instance().io().start,
                                start_now ? 1u : 0u);
            sim.set_input_value(dev.instance().io().data_in,
                                static_cast<uint64_t>(0xA5));
            sim.set_input_value(dev.instance().io().miso_in, 0u);
        };
        drive(*sim_a, *dev_a);
        drive(*sim_b, *dev_b);
        sim_a->tick(1);
        sim_b->tick(1);
        uint64_t sa = read_port_output(*sim_a, state_a);
        uint64_t sb = read_port_output(*sim_b, state_b);
        uint64_t ba = read_port_output(*sim_a, count_a);
        uint64_t bb = read_port_output(*sim_b, count_b);
        UNSCOPED_INFO("cycle=" << cycle
                     << " state:interp=" << sa << " vl=" << sb
                     << " count:interp=" << ba << " vl=" << bb);
        REQUIRE(sa == sb);
        REQUIRE(ba == bb);
    }
}

// ============================================================================
// Rv32iPipelineFixture — RV32I 3-instruction trace parity.
//
// Follow-up to strengthen-verilator-e2e-coverage D8 (b) (2026-10-14
// archive). With verilator-reset-initial-value-mismatch archived
// (2026-10-14), the t=0 reset semantics are now correct on both
// backends and this fixture can run a real e2e.
//
// Drives 3 short RV32I instructions through the 5-stage pipeline
// (IF/ID/EX/MEM/WB) and verifies perf_instructions == 3 on both
// backends. imem is internal (ch_mem sread, posedge-aligned for
// verilator parity per verilator-e2e-coverage spec §Requirement 1
// invariant 3).
// ============================================================================
TEST_CASE("ComplexE2E - Rv32iShortInstructionTraceParity",
          "[verilator][e2e][complex][rv32i][m5]") {
    if (!tool_available("verilator")) {
        SKIP("verilator not on PATH");
    }

    auto dev_a = std::make_unique<ch::ch_device<Rv32iPipelineFixture>>();
    auto dev_b = std::make_unique<ch::ch_device<Rv32iPipelineFixture>>();
    auto sim_a = std::make_unique<Simulator>(dev_a->context());
    auto sim_b = std::make_unique<Simulator>(dev_b->context());

    sim_a->set_backend(std::make_unique<InterpreterBackend>());
    ch::data_map_t data_map_b;
    auto workdir = make_temp_dir("_cx_rv32i");
    REQUIRE_FALSE(workdir.empty());
    auto backend = std::make_unique<VerilatorBackend>(workdir);
    REQUIRE(backend->initialize(dev_b->context(), data_map_b));
    if (backend->port_access_snapshot().empty()) {
        SKIP("verilator dlopen did not populate port_access_");
    }
    sim_b->set_backend(std::move(backend));

    auto *perf_a = dev_a->instance().io().perf_instructions.impl();
    auto *perf_b = dev_b->instance().io().perf_instructions.impl();
    auto *wb_en_a = dev_a->instance().io().wb_write_en.impl();
    auto *wb_data_a = dev_a->instance().io().wb_write_data.impl();
    auto *wb_rd_a = dev_a->instance().io().wb_rd_addr.impl();
    auto *wb_en_b = dev_b->instance().io().wb_write_en.impl();
    auto *wb_data_b = dev_b->instance().io().wb_write_data.impl();
    auto *wb_rd_b = dev_b->instance().io().wb_rd_addr.impl();

    // 观测「x3 的写回」：两后端都 MUST 出现 (rd=3, data=3) —— ISA 语义断言
    bool saw_x3_interp = false;
    bool saw_x3_verilator = false;
    for (int cycle = 0; cycle < 20; ++cycle) {
        sim_a->tick(1);
        sim_b->tick(1);
        if (read_port_output(*sim_a, wb_en_a) == 1 &&
            read_port_output(*sim_a, wb_rd_a) == 3 &&
            read_port_output(*sim_a, wb_data_a) == 3) {
            saw_x3_interp = true;
        }
        if (read_port_output(*sim_b, wb_en_b) == 1 &&
            read_port_output(*sim_b, wb_rd_b) == 3 &&
            read_port_output(*sim_b, wb_data_b) == 3) {
            saw_x3_verilator = true;
        }
    }

    uint64_t perf_a_val = read_port_output(*sim_a, perf_a);
    uint64_t perf_b_val = read_port_output(*sim_b, perf_b);
    UNSCOPED_INFO("perf_instructions interp=" << perf_a_val
                 << " verilator=" << perf_b_val);

    // ⚠️ **语义断言**（2026-10-25，change rv32i-pipeline-commit-path-2026-10）：
    //    固件 = `addi x1,x0,1; addi x2,x0,2; NOP; NOP; NOP; add x3,x1,x2` ⇒
    //    **x3 == 3** 是 ISA 正确值（固件无数据冒险：生产者与消费者之间有 NOP，
    //    因为 lib 版 HazardUnit 不接数据前推）。
    //    ⇒ 「计数器 == N」这类断言**不能**替代它：计数器可被伪事件满足
    //    （此前 `perf_instructions == 12` 钉的是**坏行为** —— 当时
    //    `mem_valid_out = is_load & mem_valid` ⇒ 退休数统计的是 load 次数）。
    REQUIRE(saw_x3_interp);
    REQUIRE(saw_x3_verilator);

    // 退休指令数：20 拍 − **3 拍填充**（IF/ID → ID/EX → EX/MEM → MEM/WB
    // 依次就绪；退休发生在 MEM 级 ⇒ 3 拍后才有第一条退休）− **1 条填充期指令**
    // = **16**
    //
    // ⚠️ 修复前这里是 **18** —— 那是**多算了 1**：「绿而错」的又一例：
    //    当时 EX/MEM 的 valid 取自 **ID 级**的 valid（而数据取自 **EX 级**
    //    的结果）⇒ **有效位与数据错开一级** ⇒ 填充期产生了一次
    //    「数据无效但 valid 为真」的**假退休**。
    //    （2026-10-25 改成 `exmem_valid <<= select(exmem_en, valid_out, false)`
    //    ⇒ 有效位与数据同源 ⇒ 假退休消失。）
    //    ⚠️ 更早之前是 **12** —— 那时流水线每 11 拍被 flush 重启，退休语义
    //    也完全是错的（「退休 = load 数据有效」）。
    //
    // ⚠️ **2026-10-28 订正 17 → 16**（change `rv32i-load-byte-lane-2026-10`
    //    顺带修的**既有红**）：ADR-048 D6 的 **`primed` 门控**
    //    （`stages/if_stage.h`：复位后首个非复位周期结束前，`if_id_update_en`
    //    被 `primed` 门掉 + `flush` ⇒ **填充期取到的那条指令不进 EX
    //    ⇒ 不退休**）⇒ 退休数比「纯填充拍数」**少 1**。
    //    ⇒ ADR-048 落地时**更新了它自己那个** SoC 用例的金标（179 → 178），
    //      却**漏了本用例** ⇒ 本断言自那时起一直是红的。
    //    ⚠️ 归因：**与本 change 的主题无关**（本用例的固件是
    //      `addi/addi/NOP×3/add`，**零** `LB`/`LH` ⇒ 本 change 的
    //      `sext` 修复对它**零影响**）⇒ 已实测确认：在**本 change 之前**
    //      （HEAD `6d28264`）本断言**同样**失败（`16 == 17`）。
    //    ⇒ 按 ZERO-DEBT「Red → fix immediately」顺带收口。
    REQUIRE(perf_a_val == perf_b_val);
    REQUIRE(perf_a_val == 16);
}

// Custom main(): exit 0 when verilator is missing.
int main(int argc, char *argv[]) {
    if (!tool_available("verilator")) {
        std::fprintf(stderr,
                     "test_verilator_complex_e2e: verilator not installed; "
                     "skipping all e2e tests.\n");
        return 0;
    }
    Catch::Session session;
    session.applyCommandLine(argc, argv);
    return session.run();
}