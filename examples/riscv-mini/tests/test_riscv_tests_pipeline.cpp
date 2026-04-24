/**
 * @file test_riscv_tests_pipeline.cpp
 * @brief RISC-V RV32UI tests through Rv32iPipeline hardware (Phase F Wave 4)
 *
 * riscv-tests spec:
 * - Entry point: 0x80000000
 * - tohost: 0x80001000 (1=pass, 0=fail)
 * - Binary files: /workspace/project/riscv-tests/build/rv32ui/*.bin
 *
 * Architecture:
 *   PipelineTestTop (ch_device)
 *   ├── Rv32iPipeline (ch_module) - 5-stage pipeline
 *   ├── InstrTCM (ch_module) - instruction memory
 *   └── DataTCM (ch_module) - data memory
 */

#include "../../../tests/catch_amalgamated.hpp"
#include "ch.hpp"
#include "simulator.h"
#include "component.h"
#include "../../include/cpu/pipeline/rv32i_pipeline.h"
#include "../../include/cpu/pipeline/rv32i_tcm.h"

#include <cstdint>
#include <fstream>
#include <vector>
#include <string>
#include <iostream>
#include <iomanip>

using namespace ch::core;
using namespace riscv;

// ============================================================================
// Benchmark Result Structure
// ============================================================================
struct BenchmarkResult {
    std::string test_name;
    uint64_t cycles = 0;
    uint64_t instructions = 0;
    uint64_t branch_count = 0;
    uint64_t branch_mispredict = 0;
    uint64_t load_use_stalls = 0;
    uint64_t control_stalls = 0;
    bool passed = false;

    double cpi() const { return instructions > 0 ? (double)cycles / instructions : 0; }
    double ipc() const { return cycles > 0 ? (double)instructions / cycles : 0; }
    double branch_accuracy() const {
        return branch_count > 0 ? 1.0 - (double)branch_mispredict / branch_count : 1.0;
    }
    double load_use_stall_rate() const {
        return instructions > 0 ? (double)load_use_stalls / instructions * 100 : 0;
    }
};

// ============================================================================
// Constants
// ============================================================================
constexpr uint32_t RV32I_TOHOST_ADDR = 0x80001000;
constexpr uint32_t MAX_TICKS = 100000;

// ============================================================================
// Wrapper Component: Pipeline + ITCM + DTCM
// ============================================================================
// ch_module can only be used inside Component::describe()
// This wrapper provides the parent context for all ch_module instantiations

class PipelineTestTop : public ch::Component {
public:
    // ITCM write interface (for loading programs)
    __io(
        // ITCM write port
        ch_in<ch_uint<20>>  itcm_write_addr;
        ch_in<ch_uint<32>>  itcm_write_data;
        ch_in<ch_bool>       itcm_write_en;

        // DTCM read port (for checking tohost)
        ch_in<ch_uint<20>>  dtcm_read_addr;
        ch_out<ch_uint<32>> dtcm_read_data;

        // DTCM write port (for initializing tohost)
        ch_in<ch_uint<20>>  dtcm_write_addr;
        ch_in<ch_uint<32>>  dtcm_write_data;
        ch_in<ch_bool>       dtcm_write_en;

        // Performance counter outputs (exposed from pipeline)
        ch_out<ch_uint<48>> perf_cycles;
        ch_out<ch_uint<48>> perf_instructions;
        ch_out<ch_uint<32>> perf_branch_count;
        ch_out<ch_uint<32>> perf_branch_mispredict;
        ch_out<ch_uint<32>> perf_load_use_stalls;
        ch_out<ch_uint<32>> perf_control_stalls
    )

    PipelineTestTop(ch::Component* parent = nullptr, const std::string& name = "pipeline_top")
        : ch::Component(parent, name) {}

    void create_ports() override {
        new (io_storage_) io_type;
    }

    void describe() override {
        // All ch_module calls require Component::current() != nullptr
        // This works because we're inside PipelineTestTop::describe()
        ch::ch_module<Rv32iPipeline> pipeline{"pipeline"};
        ch::ch_module<InstrTCM<20, 32>> itcm{"itcm"};
        ch::ch_module<DataTCM<20, 32>> dtcm{"dtcm"};

        // Pipeline <-> ITCM
        itcm.io().addr <<= pipeline.io().instr_addr;
        pipeline.io().instr_data <<= itcm.io().data;
        pipeline.io().instr_ready <<= itcm.io().ready;

        // Pipeline <-> DTCM
        // Pipeline <-> DTCM (addr/data/write shared with test harness via select)
        dtcm.io().valid <<= ch_bool(true);
        pipeline.io().data_read_data <<= dtcm.io().rdata;
        pipeline.io().data_ready <<= dtcm.io().ready;

        // Release reset
        pipeline.io().rst <<= ch_bool(false);
        pipeline.io().clk <<= ch_bool(true);

        // Expose ITCM write and DTCM read to test harness
        itcm.io().write_addr <<= io().itcm_write_addr;
        itcm.io().write_data <<= io().itcm_write_data;
        itcm.io().write_en   <<= io().itcm_write_en;

        dtcm.io().addr <<= select(io().dtcm_write_en | pipeline.io().data_write_en,
                                  io().dtcm_write_addr,
                                  io().dtcm_read_addr);
        io().dtcm_read_data <<= dtcm.io().rdata;

        dtcm.io().wdata <<= select(io().dtcm_write_en, io().dtcm_write_data, pipeline.io().data_write_data);
        dtcm.io().write <<= io().dtcm_write_en | pipeline.io().data_write_en;

        io().perf_cycles <<= pipeline.io().perf_cycles;
        io().perf_instructions <<= pipeline.io().perf_instructions;
        io().perf_branch_count <<= pipeline.io().perf_branch_count;
        io().perf_branch_mispredict <<= pipeline.io().perf_branch_mispredict;
        io().perf_load_use_stalls <<= pipeline.io().perf_load_use_stalls;
        io().perf_control_stalls <<= pipeline.io().perf_control_stalls;
    }
};

// ============================================================================
// Helper: Load binary file into memory
// ============================================================================
std::vector<uint32_t> load_binary_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return {};

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<char> buffer(size);
    file.read(buffer.data(), size);

    while (buffer.size() % 4 != 0) buffer.push_back(0);

    std::vector<uint32_t> words;
    for (size_t i = 0; i < buffer.size(); i += 4) {
        uint32_t word = static_cast<uint8_t>(buffer[i]) |
                       (static_cast<uint8_t>(buffer[i+1]) << 8) |
                       (static_cast<uint8_t>(buffer[i+2]) << 16) |
                       (static_cast<uint8_t>(buffer[i+3]) << 24);
        words.push_back(word);
    }
    return words;
}

// ============================================================================
// Run single riscv-tests binary through hardware pipeline with metrics
// ============================================================================
bool run_riscv_test_pipeline(const std::string& test_name, const std::string& bin_filename, BenchmarkResult& result) {
    context ctx("rv32ui_" + test_name + "_pipeline");
    ctx_swap swap(&ctx);

    ch::ch_device<PipelineTestTop> top;
    Simulator sim(top.context());

    std::string bin_path = "/workspace/project/riscv-tests/build/rv32ui/" + bin_filename;
    std::vector<uint32_t> program = load_binary_file(bin_path);

    if (program.empty()) {
        std::cerr << "SKIP: Binary not found: " << bin_path << std::endl;
        return false;
    }

    for (size_t i = 0; i < program.size(); i++) {
        sim.set_input_value(top.instance().io().itcm_write_addr, static_cast<uint32_t>(i));
        sim.set_input_value(top.instance().io().itcm_write_data, program[i]);
        sim.set_input_value(top.instance().io().itcm_write_en, 1);
        sim.tick();
        sim.set_input_value(top.instance().io().itcm_write_en, 0);
    }

    sim.set_input_value(top.instance().io().dtcm_read_addr, RV32I_TOHOST_ADDR / 4);
    sim.set_input_value(top.instance().io().dtcm_write_addr, RV32I_TOHOST_ADDR / 4);
    sim.set_input_value(top.instance().io().dtcm_write_data, 0);
    sim.set_input_value(top.instance().io().dtcm_write_en, 1);
    sim.tick();
    sim.set_input_value(top.instance().io().dtcm_write_en, 0);

    uint32_t tohost_value = 0;
    bool test_complete = false;

    for (uint32_t tick = 0; tick < MAX_TICKS && !test_complete; tick++) {
        sim.tick();

        sim.set_input_value(top.instance().io().dtcm_read_addr, RV32I_TOHOST_ADDR / 4);
        sim.tick();
        auto val = sim.get_port_value(top.instance().io().dtcm_read_data);
        tohost_value = static_cast<uint64_t>(val);

        if (tohost_value != 0) {
            test_complete = true;
        }
    }

    result.test_name = test_name;
    result.passed = false;

    if (!test_complete) {
        std::cerr << "TIMEOUT: " << test_name << " (" << MAX_TICKS << " ticks)" << std::endl;
        return false;
    }

    if (tohost_value != 1) {
        std::cerr << "FAIL: " << test_name << " returned " << tohost_value << std::endl;
        return false;
    }

    result.cycles = static_cast<uint64_t>(sim.get_port_value(top.instance().io().perf_cycles));
    result.instructions = static_cast<uint64_t>(sim.get_port_value(top.instance().io().perf_instructions));
    result.branch_count = static_cast<uint64_t>(sim.get_port_value(top.instance().io().perf_branch_count));
    result.branch_mispredict = static_cast<uint64_t>(sim.get_port_value(top.instance().io().perf_branch_mispredict));
    result.load_use_stalls = static_cast<uint64_t>(sim.get_port_value(top.instance().io().perf_load_use_stalls));
    result.control_stalls = static_cast<uint64_t>(sim.get_port_value(top.instance().io().perf_control_stalls));
    result.passed = true;

    return true;
}

// ============================================================================
// Benchmark Report Printer
// ============================================================================
void print_benchmark_report(const std::string& name, const BenchmarkResult& r) {
    std::cout << std::setw(20) << name
              << std::setw(12) << r.cycles
              << std::setw(12) << r.instructions
              << std::setw(10) << std::fixed << std::setprecision(3) << r.cpi()
              << std::setw(10) << std::fixed << std::setprecision(3) << r.ipc()
              << std::setw(12) << std::fixed << std::setprecision(2)
              << (r.branch_count > 0 ? (double)r.branch_mispredict / r.branch_count * 100 : 0)
              << std::setw(10) << std::fixed << std::setprecision(2) << r.load_use_stall_rate()
              << (r.passed ? "  PASS" : "  FAIL") << "\n";
}

void print_aggregate_report(const std::vector<BenchmarkResult>& results) {
    uint64_t total_cycles = 0;
    uint64_t total_instructions = 0;
    uint64_t total_branch_count = 0;
    uint64_t total_branch_mispredict = 0;
    uint64_t total_load_use_stalls = 0;
    int passed_count = 0;

    for (const auto& r : results) {
        if (r.passed) {
            total_cycles += r.cycles;
            total_instructions += r.instructions;
            total_branch_count += r.branch_count;
            total_branch_mispredict += r.branch_mispredict;
            total_load_use_stalls += r.load_use_stalls;
            passed_count++;
        }
    }

    if (passed_count == 0) return;

    double avg_cpi = (double)total_cycles / total_instructions;
    double avg_ipc = (double)total_instructions / total_cycles;
    double branch_accuracy = total_branch_count > 0 ? 1.0 - (double)total_branch_mispredict / total_branch_count : 1.0;
    double load_use_rate = total_instructions > 0 ? (double)total_load_use_stalls / total_instructions * 100 : 0;

    std::cout << "\n========== Aggregate Benchmark Results ==========\n";
    std::cout << "Tests Passed: " << passed_count << "/" << results.size() << "\n";
    std::cout << "Total Cycles:      " << total_cycles << "\n";
    std::cout << "Total Instructions:" << total_instructions << "\n";
    std::cout << "Average CPI:       " << std::fixed << std::setprecision(3) << avg_cpi << "\n";
    std::cout << "Average IPC:       " << std::fixed << std::setprecision(3) << avg_ipc << "\n";
    std::cout << "Ideal CPI:         1.0\n";
    std::cout << "Pipeline Efficiency:" << (1.0 / avg_cpi * 100) << "%\n";
    std::cout << "Branch Accuracy:   " << std::fixed << std::setprecision(2) << (branch_accuracy * 100) << "%\n";
    std::cout << "Load-Use Stall Rate:" << std::fixed << std::setprecision(2) << load_use_rate << "%\n";
    std::cout << "==================================================\n\n";
}

// ============================================================================
// B006: Reference Interpreter for Baseline Comparison
// ============================================================================
struct ReferenceResult {
    uint64_t cycles;
    uint64_t instructions;
};

class ReferenceInterpreter {
public:
    std::vector<uint32_t> memory;
    uint32_t pc;
    uint32_t regs[32];
    bool tohost_written;
    uint32_t tohost_value;
    uint64_t instruction_count;
    uint64_t cycle_count;

    ReferenceInterpreter() : pc(0x80000000), tohost_written(false), tohost_value(0), instruction_count(0), cycle_count(0) {
        memory.resize(16384, 0);
        for (int i = 0; i < 32; i++) regs[i] = 0;
    }

    bool load_binary(const std::string& path) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) return false;
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);
        std::vector<char> buffer(size);
        file.read(buffer.data(), size);
        size_t word_count = (buffer.size() + 3) / 4;
        memory.resize(word_count, 0);
        for (size_t i = 0; i + 3 < buffer.size(); i += 4) {
            memory[i/4] = static_cast<uint8_t>(buffer[i]) |
                        (static_cast<uint8_t>(buffer[i+1]) << 8) |
                        (static_cast<uint8_t>(buffer[i+2]) << 16) |
                        (static_cast<uint8_t>(buffer[i+3]) << 24);
        }
        return true;
    }

    uint32_t read_mem(uint32_t addr) {
        if (addr < 0x80000000 || addr >= 0x80040000) return 0;
        size_t index = (addr - 0x80000000) / 4;
        if (index >= memory.size()) return 0;
        return memory[index];
    }

    void write_mem(uint32_t addr, uint32_t val) {
        if (addr < 0x80000000 || addr >= 0x80040000) return;
        size_t index = (addr - 0x80000000) / 4;
        if (index >= memory.size()) return;
        memory[index] = val;
    }

    bool step() {
        uint32_t instr = read_mem(pc);
        if (instr == 0) { pc += 4; return true; }
        instruction_count++;

        uint8_t opcode = instr & 0x7F;
        uint8_t rd = (instr >> 7) & 0x1F;
        uint8_t funct3 = (instr >> 12) & 0x7;
        uint8_t rs1 = (instr >> 15) & 0x1F;
        uint8_t rs2 = (instr >> 20) & 0x1F;
        uint32_t imm = (instr >> 20) & 0xFFF;

        auto wr = [&](uint8_t r, uint32_t v) { if (r != 0) regs[r] = v; };

        if (opcode == 0x13) { // OP-IMM
            int32_t simm = (static_cast<int32_t>(imm) << 20) >> 20;
            switch (funct3) {
                case 0: wr(rd, regs[rs1] + simm); break;
                case 1: wr(rd, regs[rs1] << (imm & 0x1F)); break;
                case 2: wr(rd, (static_cast<int32_t>(regs[rs1]) < simm) ? 1 : 0); break;
                case 3: wr(rd, (regs[rs1] < static_cast<uint32_t>(simm)) ? 1 : 0); break;
                case 4: wr(rd, regs[rs1] ^ simm); break;
                case 5: wr(rd, (imm & 0x800) ? (static_cast<int32_t>(regs[rs1]) >> (imm & 0x1F)) : (regs[rs1] >> (imm & 0x1F))); break;
                case 6: wr(rd, regs[rs1] | simm); break;
                case 7: wr(rd, regs[rs1] & simm); break;
            }
            pc += 4;
        } else if (opcode == 0x33) { // OP
            switch (funct3) {
                case 0: wr(rd, (instr & 0x40000000) ? (regs[rs1] - regs[rs2]) : (regs[rs1] + regs[rs2])); break;
                case 1: wr(rd, regs[rs1] << (regs[rs2] & 0x1F)); break;
                case 2: wr(rd, (static_cast<int32_t>(regs[rs1]) < static_cast<int32_t>(regs[rs2])) ? 1 : 0); break;
                case 3: wr(rd, (regs[rs1] < regs[rs2]) ? 1 : 0); break;
                case 4: wr(rd, regs[rs1] ^ regs[rs2]); break;
                case 5: wr(rd, (instr & 0x40000000) ? (static_cast<int32_t>(regs[rs1]) >> (regs[rs2] & 0x1F)) : (regs[rs1] >> (regs[rs2] & 0x1F))); break;
                case 6: wr(rd, regs[rs1] | regs[rs2]); break;
                case 7: wr(rd, regs[rs1] & regs[rs2]); break;
            }
            pc += 4;
        } else if (opcode == 0x03) { // LOAD
            int32_t simm = (static_cast<int32_t>(imm) << 20) >> 20;
            uint32_t addr = regs[rs1] + simm;
            switch (funct3) {
                case 0: wr(rd, static_cast<int8_t>(read_mem(addr) & 0xFF)); break;
                case 1: wr(rd, static_cast<int16_t>(read_mem(addr) & 0xFFFF)); break;
                case 2: wr(rd, read_mem(addr)); break;
                case 4: wr(rd, read_mem(addr) & 0xFF); break;
                case 5: wr(rd, read_mem(addr) & 0xFFFF); break;
            }
            pc += 4;
        } else if (opcode == 0x23) { // STORE
            int32_t simm = ((instr >> 25) << 5) | ((instr >> 7) & 0x1F);
            simm = (simm << 20) >> 20;
            uint32_t addr = regs[rs1] + simm;
            switch (funct3) {
                case 0: write_mem(addr, (read_mem(addr) & ~0xFF) | (regs[rs2] & 0xFF)); break;
                case 1: write_mem(addr, (read_mem(addr) & ~0xFFFF) | (regs[rs2] & 0xFFFF)); break;
                case 2: write_mem(addr, regs[rs2]); break;
            }
            pc += 4;
        } else if (opcode == 0x63) { // BRANCH
            int32_t simm = ((instr >> 31) << 12) | ((instr >> 7) & 1) << 11 |
                          ((instr >> 25) & 0x3F) << 5 | ((instr >> 8) & 0xF) << 1;
            simm = (simm << 19) >> 19;
            bool take = false;
            switch (funct3) {
                case 0: take = (regs[rs1] == regs[rs2]); break;
                case 1: take = (regs[rs1] != regs[rs2]); break;
                case 4: take = (static_cast<int32_t>(regs[rs1]) < static_cast<int32_t>(regs[rs2])); break;
                case 5: take = (static_cast<int32_t>(regs[rs1]) >= static_cast<int32_t>(regs[rs2])); break;
                case 6: take = (regs[rs1] < regs[rs2]); break;
                case 7: take = (regs[rs1] >= regs[rs2]); break;
            }
            pc += take ? simm : 4;
        } else if (opcode == 0x6F) { // JAL
            int32_t simm = ((instr >> 31) << 20) | ((instr >> 21) & 0x3FF) << 1 |
                          ((instr >> 20) & 1) << 11 | ((instr >> 12) & 0xFF) << 12;
            simm = (simm << 11) >> 11;
            wr(rd, pc + 4);
            pc += simm;
        } else if (opcode == 0x67) { // JALR
            int32_t simm = (static_cast<int32_t>(imm) << 20) >> 20;
            uint32_t target = (regs[rs1] + simm) & ~1U;
            wr(rd, pc + 4);
            pc = target;
        } else if (opcode == 0x37) { // LUI
            wr(rd, instr & 0xFFFFF000);
            pc += 4;
        } else if (opcode == 0x17) { // AUIPC
            wr(rd, pc + (instr & 0xFFFFF000));
            pc += 4;
        } else if (opcode == 0x73) { // SYSTEM
            if (funct3 == 0 && rs2 == 0) return false;
            pc += 4;
        } else {
            pc += 4;
        }

        if (pc >= 0x80040000) return false;

        uint32_t th = read_mem(0x80001000);
        if (th != 0) { tohost_written = true; tohost_value = th; return false; }

        return instruction_count < 100000;
    }

    ReferenceResult run() {
        cycle_count = 0;
        instruction_count = 0;
        while (step()) { cycle_count++; }
        cycle_count++;
        return {cycle_count, instruction_count};
    }
};

ReferenceResult run_reference_interpreter(const std::string& bin_filename) {
    ReferenceInterpreter interp;
    std::string path = "/workspace/project/riscv-tests/build/rv32ui/" + bin_filename;
    if (!interp.load_binary(path)) return {0, 0};
    return interp.run();
}

void print_reference_comparison(const std::string& name, const BenchmarkResult& hw, const ReferenceResult& ref) {
    if (ref.instructions == 0) return;
    double overhead = (double)hw.cycles / ref.cycles;
    double ref_cpi = (double)ref.cycles / ref.instructions;
    std::cout << std::setw(20) << name
              << " HW=" << std::setw(8) << hw.cycles
              << " Ref=" << std::setw(8) << ref.cycles
              << " Overhead=" << std::fixed << std::setprecision(3) << overhead << "x"
              << " (Ref CPI=" << std::fixed << std::setprecision(3) << ref_cpi << ")"
              << (hw.passed ? "  PASS" : "  FAIL") << "\n";
}

// ============================================================================
// Test Cases with Benchmark Metrics
// ============================================================================
TEST_CASE("rv32ui-p-simple benchmark", "[benchmark][pipeline][rv32ui][simple]") {
    BenchmarkResult result;
    bool passed = run_riscv_test_pipeline("simple", "simple.bin", result);
    print_benchmark_report("simple", result);
    if (!passed) {
        std::ifstream f("/workspace/project/riscv-tests/build/rv32ui/simple.bin");
        if (!f.is_open()) SKIP("Binary not available");
        FAIL("Test failed or timed out");
    }
    REQUIRE(result.passed == true);
}

TEST_CASE("rv32ui-p-add benchmark", "[benchmark][pipeline][rv32ui][add]") {
    BenchmarkResult result;
    bool passed = run_riscv_test_pipeline("add", "add.bin", result);
    print_benchmark_report("add", result);
    if (!passed) {
        std::ifstream f("/workspace/project/riscv-tests/build/rv32ui/add.bin");
        if (!f.is_open()) SKIP("Binary not available");
        FAIL("Test failed or timed out");
    }
    REQUIRE(result.passed == true);
}

TEST_CASE("rv32ui-p-addi benchmark", "[benchmark][pipeline][rv32ui][addi]") {
    BenchmarkResult result;
    bool passed = run_riscv_test_pipeline("addi", "addi.bin", result);
    print_benchmark_report("addi", result);
    if (!passed) {
        std::ifstream f("/workspace/project/riscv-tests/build/rv32ui/addi.bin");
        if (!f.is_open()) SKIP("Binary not available");
        FAIL("Test failed or timed out");
    }
    REQUIRE(result.passed == true);
}

TEST_CASE("rv32ui-p-and benchmark", "[benchmark][pipeline][rv32ui][and]") {
    BenchmarkResult result;
    bool passed = run_riscv_test_pipeline("and", "and.bin", result);
    print_benchmark_report("and", result);
    if (!passed) {
        std::ifstream f("/workspace/project/riscv-tests/build/rv32ui/and.bin");
        if (!f.is_open()) SKIP("Binary not available");
        FAIL("Test failed or timed out");
    }
    REQUIRE(result.passed == true);
}

TEST_CASE("rv32ui-p-andi benchmark", "[benchmark][pipeline][rv32ui][andi]") {
    BenchmarkResult result;
    bool passed = run_riscv_test_pipeline("andi", "andi.bin", result);
    print_benchmark_report("andi", result);
    if (!passed) {
        std::ifstream f("/workspace/project/riscv-tests/build/rv32ui/andi.bin");
        if (!f.is_open()) SKIP("Binary not available");
        FAIL("Test failed or timed out");
    }
    REQUIRE(result.passed == true);
}

TEST_CASE("rv32ui-p-auipc benchmark", "[benchmark][pipeline][rv32ui][auipc]") {
    BenchmarkResult result;
    bool passed = run_riscv_test_pipeline("auipc", "auipc.bin", result);
    print_benchmark_report("auipc", result);
    if (!passed) {
        std::ifstream f("/workspace/project/riscv-tests/build/rv32ui/auipc.bin");
        if (!f.is_open()) SKIP("Binary not available");
        FAIL("Test failed or timed out");
    }
    REQUIRE(result.passed == true);
}

TEST_CASE("rv32ui-p-beq benchmark", "[benchmark][pipeline][rv32ui][beq]") {
    BenchmarkResult result;
    bool passed = run_riscv_test_pipeline("beq", "beq.bin", result);
    print_benchmark_report("beq", result);
    if (!passed) {
        std::ifstream f("/workspace/project/riscv-tests/build/rv32ui/beq.bin");
        if (!f.is_open()) SKIP("Binary not available");
        FAIL("Test failed or timed out");
    }
    REQUIRE(result.passed == true);
}

TEST_CASE("rv32ui-p-bge benchmark", "[benchmark][pipeline][rv32ui][bge]") {
    BenchmarkResult result;
    bool passed = run_riscv_test_pipeline("bge", "bge.bin", result);
    print_benchmark_report("bge", result);
    if (!passed) {
        std::ifstream f("/workspace/project/riscv-tests/build/rv32ui/bge.bin");
        if (!f.is_open()) SKIP("Binary not available");
        FAIL("Test failed or timed out");
    }
    REQUIRE(result.passed == true);
}

TEST_CASE("rv32ui-p-bgeu benchmark", "[benchmark][pipeline][rv32ui][bgeu]") {
    BenchmarkResult result;
    bool passed = run_riscv_test_pipeline("bgeu", "bgeu.bin", result);
    print_benchmark_report("bgeu", result);
    if (!passed) {
        std::ifstream f("/workspace/project/riscv-tests/build/rv32ui/bgeu.bin");
        if (!f.is_open()) SKIP("Binary not available");
        FAIL("Test failed or timed out");
    }
    REQUIRE(result.passed == true);
}

TEST_CASE("rv32ui-p-blt benchmark", "[benchmark][pipeline][rv32ui][blt]") {
    BenchmarkResult result;
    bool passed = run_riscv_test_pipeline("blt", "blt.bin", result);
    print_benchmark_report("blt", result);
    if (!passed) {
        std::ifstream f("/workspace/project/riscv-tests/build/rv32ui/blt.bin");
        if (!f.is_open()) SKIP("Binary not available");
        FAIL("Test failed or timed out");
    }
    REQUIRE(result.passed == true);
}

TEST_CASE("rv32ui-p-bltu benchmark", "[benchmark][pipeline][rv32ui][bltu]") {
    BenchmarkResult result;
    bool passed = run_riscv_test_pipeline("bltu", "bltu.bin", result);
    print_benchmark_report("bltu", result);
    if (!passed) {
        std::ifstream f("/workspace/project/riscv-tests/build/rv32ui/bltu.bin");
        if (!f.is_open()) SKIP("Binary not available");
        FAIL("Test failed or timed out");
    }
    REQUIRE(result.passed == true);
}

TEST_CASE("rv32ui-p-bne benchmark", "[benchmark][pipeline][rv32ui][bne]") {
    BenchmarkResult result;
    bool passed = run_riscv_test_pipeline("bne", "bne.bin", result);
    print_benchmark_report("bne", result);
    if (!passed) {
        std::ifstream f("/workspace/project/riscv-tests/build/rv32ui/bne.bin");
        if (!f.is_open()) SKIP("Binary not available");
        FAIL("Test failed or timed out");
    }
    REQUIRE(result.passed == true);
}

TEST_CASE("rv32ui-p-fence_i benchmark", "[benchmark][pipeline][rv32ui][fence_i]") {
    BenchmarkResult result;
    bool passed = run_riscv_test_pipeline("fence_i", "fence_i.bin", result);
    print_benchmark_report("fence_i", result);
    if (!passed) {
        std::ifstream f("/workspace/project/riscv-tests/build/rv32ui/fence_i.bin");
        if (!f.is_open()) SKIP("Binary not available");
        FAIL("Test failed or timed out");
    }
    REQUIRE(result.passed == true);
}

TEST_CASE("rv32ui-p-jal benchmark", "[benchmark][pipeline][rv32ui][jal]") {
    BenchmarkResult result;
    bool passed = run_riscv_test_pipeline("jal", "jal.bin", result);
    print_benchmark_report("jal", result);
    if (!passed) {
        std::ifstream f("/workspace/project/riscv-tests/build/rv32ui/jal.bin");
        if (!f.is_open()) SKIP("Binary not available");
        FAIL("Test failed or timed out");
    }
    REQUIRE(result.passed == true);
}

TEST_CASE("rv32ui-p-jalr benchmark", "[benchmark][pipeline][rv32ui][jalr]") {
    BenchmarkResult result;
    bool passed = run_riscv_test_pipeline("jalr", "jalr.bin", result);
    print_benchmark_report("jalr", result);
    if (!passed) {
        std::ifstream f("/workspace/project/riscv-tests/build/rv32ui/jalr.bin");
        if (!f.is_open()) SKIP("Binary not available");
        FAIL("Test failed or timed out");
    }
    REQUIRE(result.passed == true);
}

TEST_CASE("rv32ui-p-lb benchmark", "[benchmark][pipeline][rv32ui][lb]") {
    BenchmarkResult result;
    bool passed = run_riscv_test_pipeline("lb", "lb.bin", result);
    print_benchmark_report("lb", result);
    if (!passed) {
        std::ifstream f("/workspace/project/riscv-tests/build/rv32ui/lb.bin");
        if (!f.is_open()) SKIP("Binary not available");
        FAIL("Test failed or timed out");
    }
    REQUIRE(result.passed == true);
}

TEST_CASE("rv32ui-p-lbu benchmark", "[benchmark][pipeline][rv32ui][lbu]") {
    BenchmarkResult result;
    bool passed = run_riscv_test_pipeline("lbu", "lbu.bin", result);
    print_benchmark_report("lbu", result);
    if (!passed) {
        std::ifstream f("/workspace/project/riscv-tests/build/rv32ui/lbu.bin");
        if (!f.is_open()) SKIP("Binary not available");
        FAIL("Test failed or timed out");
    }
    REQUIRE(result.passed == true);
}

TEST_CASE("rv32ui-p-lh benchmark", "[benchmark][pipeline][rv32ui][lh]") {
    BenchmarkResult result;
    bool passed = run_riscv_test_pipeline("lh", "lh.bin", result);
    print_benchmark_report("lh", result);
    if (!passed) {
        std::ifstream f("/workspace/project/riscv-tests/build/rv32ui/lh.bin");
        if (!f.is_open()) SKIP("Binary not available");
        FAIL("Test failed or timed out");
    }
    REQUIRE(result.passed == true);
}

TEST_CASE("rv32ui-p-lhu benchmark", "[benchmark][pipeline][rv32ui][lhu]") {
    BenchmarkResult result;
    bool passed = run_riscv_test_pipeline("lhu", "lhu.bin", result);
    print_benchmark_report("lhu", result);
    if (!passed) {
        std::ifstream f("/workspace/project/riscv-tests/build/rv32ui/lhu.bin");
        if (!f.is_open()) SKIP("Binary not available");
        FAIL("Test failed or timed out");
    }
    REQUIRE(result.passed == true);
}

TEST_CASE("rv32ui-p-lui benchmark", "[benchmark][pipeline][rv32ui][lui]") {
    BenchmarkResult result;
    bool passed = run_riscv_test_pipeline("lui", "lui.bin", result);
    print_benchmark_report("lui", result);
    if (!passed) {
        std::ifstream f("/workspace/project/riscv-tests/build/rv32ui/lui.bin");
        if (!f.is_open()) SKIP("Binary not available");
        FAIL("Test failed or timed out");
    }
    REQUIRE(result.passed == true);
}

TEST_CASE("rv32ui-p-lw benchmark", "[benchmark][pipeline][rv32ui][lw]") {
    BenchmarkResult result;
    bool passed = run_riscv_test_pipeline("lw", "lw.bin", result);
    print_benchmark_report("lw", result);
    if (!passed) {
        std::ifstream f("/workspace/project/riscv-tests/build/rv32ui/lw.bin");
        if (!f.is_open()) SKIP("Binary not available");
        FAIL("Test failed or timed out");
    }
    REQUIRE(result.passed == true);
}

TEST_CASE("rv32ui-p-ma_data benchmark", "[benchmark][pipeline][rv32ui][ma_data]") {
    BenchmarkResult result;
    bool passed = run_riscv_test_pipeline("ma_data", "ma_data.bin", result);
    print_benchmark_report("ma_data", result);
    if (!passed) {
        std::ifstream f("/workspace/project/riscv-tests/build/rv32ui/ma_data.bin");
        if (!f.is_open()) SKIP("Binary not available");
        FAIL("Test failed or timed out");
    }
    REQUIRE(result.passed == true);
}

TEST_CASE("rv32ui-p-or benchmark", "[benchmark][pipeline][rv32ui][or]") {
    BenchmarkResult result;
    bool passed = run_riscv_test_pipeline("or", "or.bin", result);
    print_benchmark_report("or", result);
    if (!passed) {
        std::ifstream f("/workspace/project/riscv-tests/build/rv32ui/or.bin");
        if (!f.is_open()) SKIP("Binary not available");
        FAIL("Test failed or timed out");
    }
    REQUIRE(result.passed == true);
}

TEST_CASE("rv32ui-p-ori benchmark", "[benchmark][pipeline][rv32ui][ori]") {
    BenchmarkResult result;
    bool passed = run_riscv_test_pipeline("ori", "ori.bin", result);
    print_benchmark_report("ori", result);
    if (!passed) {
        std::ifstream f("/workspace/project/riscv-tests/build/rv32ui/ori.bin");
        if (!f.is_open()) SKIP("Binary not available");
        FAIL("Test failed or timed out");
    }
    REQUIRE(result.passed == true);
}

TEST_CASE("rv32ui-p-sb benchmark", "[benchmark][pipeline][rv32ui][sb]") {
    BenchmarkResult result;
    bool passed = run_riscv_test_pipeline("sb", "sb.bin", result);
    print_benchmark_report("sb", result);
    if (!passed) {
        std::ifstream f("/workspace/project/riscv-tests/build/rv32ui/sb.bin");
        if (!f.is_open()) SKIP("Binary not available");
        FAIL("Test failed or timed out");
    }
    REQUIRE(result.passed == true);
}

TEST_CASE("rv32ui-p-sh benchmark", "[benchmark][pipeline][rv32ui][sh]") {
    BenchmarkResult result;
    bool passed = run_riscv_test_pipeline("sh", "sh.bin", result);
    print_benchmark_report("sh", result);
    if (!passed) {
        std::ifstream f("/workspace/project/riscv-tests/build/rv32ui/sh.bin");
        if (!f.is_open()) SKIP("Binary not available");
        FAIL("Test failed or timed out");
    }
    REQUIRE(result.passed == true);
}

TEST_CASE("rv32ui-p-sll benchmark", "[benchmark][pipeline][rv32ui][sll]") {
    BenchmarkResult result;
    bool passed = run_riscv_test_pipeline("sll", "sll.bin", result);
    print_benchmark_report("sll", result);
    if (!passed) {
        std::ifstream f("/workspace/project/riscv-tests/build/rv32ui/sll.bin");
        if (!f.is_open()) SKIP("Binary not available");
        FAIL("Test failed or timed out");
    }
    REQUIRE(result.passed == true);
}

TEST_CASE("rv32ui-p-slli benchmark", "[benchmark][pipeline][rv32ui][slli]") {
    BenchmarkResult result;
    bool passed = run_riscv_test_pipeline("slli", "slli.bin", result);
    print_benchmark_report("slli", result);
    if (!passed) {
        std::ifstream f("/workspace/project/riscv-tests/build/rv32ui/slli.bin");
        if (!f.is_open()) SKIP("Binary not available");
        FAIL("Test failed or timed out");
    }
    REQUIRE(result.passed == true);
}

TEST_CASE("rv32ui-p-slt benchmark", "[benchmark][pipeline][rv32ui][slt]") {
    BenchmarkResult result;
    bool passed = run_riscv_test_pipeline("slt", "slt.bin", result);
    print_benchmark_report("slt", result);
    if (!passed) {
        std::ifstream f("/workspace/project/riscv-tests/build/rv32ui/slt.bin");
        if (!f.is_open()) SKIP("Binary not available");
        FAIL("Test failed or timed out");
    }
    REQUIRE(result.passed == true);
}

TEST_CASE("rv32ui-p-slti benchmark", "[benchmark][pipeline][rv32ui][slti]") {
    BenchmarkResult result;
    bool passed = run_riscv_test_pipeline("slti", "slti.bin", result);
    print_benchmark_report("slti", result);
    if (!passed) {
        std::ifstream f("/workspace/project/riscv-tests/build/rv32ui/slti.bin");
        if (!f.is_open()) SKIP("Binary not available");
        FAIL("Test failed or timed out");
    }
    REQUIRE(result.passed == true);
}

TEST_CASE("rv32ui-p-sltiu benchmark", "[benchmark][pipeline][rv32ui][sltiu]") {
    BenchmarkResult result;
    bool passed = run_riscv_test_pipeline("sltiu", "sltiu.bin", result);
    print_benchmark_report("sltiu", result);
    if (!passed) {
        std::ifstream f("/workspace/project/riscv-tests/build/rv32ui/sltiu.bin");
        if (!f.is_open()) SKIP("Binary not available");
        FAIL("Test failed or timed out");
    }
    REQUIRE(result.passed == true);
}

TEST_CASE("rv32ui-p-sltu benchmark", "[benchmark][pipeline][rv32ui][sltu]") {
    BenchmarkResult result;
    bool passed = run_riscv_test_pipeline("sltu", "sltu.bin", result);
    print_benchmark_report("sltu", result);
    if (!passed) {
        std::ifstream f("/workspace/project/riscv-tests/build/rv32ui/sltu.bin");
        if (!f.is_open()) SKIP("Binary not available");
        FAIL("Test failed or timed out");
    }
    REQUIRE(result.passed == true);
}

TEST_CASE("rv32ui-p-sra benchmark", "[benchmark][pipeline][rv32ui][sra]") {
    BenchmarkResult result;
    bool passed = run_riscv_test_pipeline("sra", "sra.bin", result);
    print_benchmark_report("sra", result);
    if (!passed) {
        std::ifstream f("/workspace/project/riscv-tests/build/rv32ui/sra.bin");
        if (!f.is_open()) SKIP("Binary not available");
        FAIL("Test failed or timed out");
    }
    REQUIRE(result.passed == true);
}

TEST_CASE("rv32ui-p-srai benchmark", "[benchmark][pipeline][rv32ui][srai]") {
    BenchmarkResult result;
    bool passed = run_riscv_test_pipeline("srai", "srai.bin", result);
    print_benchmark_report("srai", result);
    if (!passed) {
        std::ifstream f("/workspace/project/riscv-tests/build/rv32ui/srai.bin");
        if (!f.is_open()) SKIP("Binary not available");
        FAIL("Test failed or timed out");
    }
    REQUIRE(result.passed == true);
}

TEST_CASE("rv32ui-p-srl benchmark", "[benchmark][pipeline][rv32ui][srl]") {
    BenchmarkResult result;
    bool passed = run_riscv_test_pipeline("srl", "srl.bin", result);
    print_benchmark_report("srl", result);
    if (!passed) {
        std::ifstream f("/workspace/project/riscv-tests/build/rv32ui/srl.bin");
        if (!f.is_open()) SKIP("Binary not available");
        FAIL("Test failed or timed out");
    }
    REQUIRE(result.passed == true);
}

TEST_CASE("rv32ui-p-srli benchmark", "[benchmark][pipeline][rv32ui][srli]") {
    BenchmarkResult result;
    bool passed = run_riscv_test_pipeline("srli", "srli.bin", result);
    print_benchmark_report("srli", result);
    if (!passed) {
        std::ifstream f("/workspace/project/riscv-tests/build/rv32ui/srli.bin");
        if (!f.is_open()) SKIP("Binary not available");
        FAIL("Test failed or timed out");
    }
    REQUIRE(result.passed == true);
}

TEST_CASE("rv32ui-p-st_ld benchmark", "[benchmark][pipeline][rv32ui][st_ld]") {
    BenchmarkResult result;
    bool passed = run_riscv_test_pipeline("st_ld", "st_ld.bin", result);
    print_benchmark_report("st_ld", result);
    if (!passed) {
        std::ifstream f("/workspace/project/riscv-tests/build/rv32ui/st_ld.bin");
        if (!f.is_open()) SKIP("Binary not available");
        FAIL("Test failed or timed out");
    }
    REQUIRE(result.passed == true);
}

TEST_CASE("rv32ui-p-sub benchmark", "[benchmark][pipeline][rv32ui][sub]") {
    BenchmarkResult result;
    bool passed = run_riscv_test_pipeline("sub", "sub.bin", result);
    print_benchmark_report("sub", result);
    if (!passed) {
        std::ifstream f("/workspace/project/riscv-tests/build/rv32ui/sub.bin");
        if (!f.is_open()) SKIP("Binary not available");
        FAIL("Test failed or timed out");
    }
    REQUIRE(result.passed == true);
}

TEST_CASE("rv32ui-p-sw benchmark", "[benchmark][pipeline][rv32ui][sw]") {
    BenchmarkResult result;
    bool passed = run_riscv_test_pipeline("sw", "sw.bin", result);
    print_benchmark_report("sw", result);
    if (!passed) {
        std::ifstream f("/workspace/project/riscv-tests/build/rv32ui/sw.bin");
        if (!f.is_open()) SKIP("Binary not available");
        FAIL("Test failed or timed out");
    }
    REQUIRE(result.passed == true);
}

TEST_CASE("rv32ui-p-xor benchmark", "[benchmark][pipeline][rv32ui][xor]") {
    BenchmarkResult result;
    bool passed = run_riscv_test_pipeline("xor", "xor.bin", result);
    print_benchmark_report("xor", result);
    if (!passed) {
        std::ifstream f("/workspace/project/riscv-tests/build/rv32ui/xor.bin");
        if (!f.is_open()) SKIP("Binary not available");
        FAIL("Test failed or timed out");
    }
    REQUIRE(result.passed == true);
}

TEST_CASE("rv32ui-p-xori benchmark", "[benchmark][pipeline][rv32ui][xori]") {
    BenchmarkResult result;
    bool passed = run_riscv_test_pipeline("xori", "xori.bin", result);
    print_benchmark_report("xori", result);
    if (!passed) {
        std::ifstream f("/workspace/project/riscv-tests/build/rv32ui/xori.bin");
        if (!f.is_open()) SKIP("Binary not available");
        FAIL("Test failed or timed out");
    }
    REQUIRE(result.passed == true);
}

// ============================================================================
// B006 Reference Comparison Tests
// ============================================================================
TEST_CASE("rv32ui-ref-simple vs HW", "[benchmark][reference][simple]") {
    BenchmarkResult hw;
    bool hw_passed = run_riscv_test_pipeline("simple", "simple.bin", hw);
    ReferenceResult ref = run_reference_interpreter("simple.bin");

    std::cout << "\n--- Reference Comparison: simple ---\n";
    print_reference_comparison("simple", hw, ref);

    if (!hw_passed) {
        std::ifstream f("/workspace/project/riscv-tests/build/rv32ui/simple.bin");
        if (!f.is_open()) SKIP("Binary not available");
    }
    REQUIRE(hw_passed);
}

TEST_CASE("rv32ui-ref-add vs HW", "[benchmark][reference][add]") {
    BenchmarkResult hw;
    bool hw_passed = run_riscv_test_pipeline("add", "add.bin", hw);
    ReferenceResult ref = run_reference_interpreter("add.bin");

    std::cout << "\n--- Reference Comparison: add ---\n";
    print_reference_comparison("add", hw, ref);

    if (!hw_passed) {
        std::ifstream f("/workspace/project/riscv-tests/build/rv32ui/add.bin");
        if (!f.is_open()) SKIP("Binary not available");
    }
    REQUIRE(hw_passed);
}

TEST_CASE("rv32ui-ref-beq vs HW", "[benchmark][reference][branch]") {
    BenchmarkResult hw;
    bool hw_passed = run_riscv_test_pipeline("beq", "beq.bin", hw);
    ReferenceResult ref = run_reference_interpreter("beq.bin");

    std::cout << "\n--- Reference Comparison: beq ---\n";
    print_reference_comparison("beq", hw, ref);

    if (!hw_passed) {
        std::ifstream f("/workspace/project/riscv-tests/build/rv32ui/beq.bin");
        if (!f.is_open()) SKIP("Binary not available");
    }
    REQUIRE(hw_passed);
}

TEST_CASE("rv32ui-ref-lw vs HW", "[benchmark][reference][memory]") {
    BenchmarkResult hw;
    bool hw_passed = run_riscv_test_pipeline("lw", "lw.bin", hw);
    ReferenceResult ref = run_reference_interpreter("lw.bin");

    std::cout << "\n--- Reference Comparison: lw ---\n";
    print_reference_comparison("lw", hw, ref);

    if (!hw_passed) {
        std::ifstream f("/workspace/project/riscv-tests/build/rv32ui/lw.bin");
        if (!f.is_open()) SKIP("Binary not available");
    }
    REQUIRE(hw_passed);
}

// ============================================================================
// B005/B007: Aggregate Benchmark Report Test
// ============================================================================
TEST_CASE("rv32ui-p-aggregate benchmark report", "[benchmark][aggregate]") {
    std::vector<BenchmarkResult> all_results;

    // List of representative RV32UI tests (reduced set for faster execution)
    std::vector<std::pair<std::string, std::string>> tests = {
        {"simple", "simple.bin"}, {"add", "add.bin"}, {"lw", "lw.bin"}
    };

    std::cout << "\n========== Running RV32UI Benchmark Suite ==========\n";
    std::cout << std::setw(20) << "Test"
              << std::setw(12) << "Cycles"
              << std::setw(12) << "Instrs"
              << std::setw(10) << "CPI"
              << std::setw(10) << "IPC"
              << std::setw(12) << "BrAcc%"
              << std::setw(10) << "Stall%"
              << "\n";
    std::cout << "--------------------------------------------------------\n";

    for (const auto& [name, bin] : tests) {
        BenchmarkResult result;
        bool passed = run_riscv_test_pipeline(name, bin, result);
        result.passed = passed;
        print_benchmark_report(name, result);
        all_results.push_back(result);
    }

    print_aggregate_report(all_results);
}
