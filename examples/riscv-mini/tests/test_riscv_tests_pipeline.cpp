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

using namespace ch::core;
using namespace riscv;

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
// Run single riscv-tests binary through hardware pipeline
// ============================================================================
bool run_riscv_test_pipeline(const std::string& test_name, const std::string& bin_filename) {
    context ctx("rv32ui_" + test_name + "_pipeline");
    ctx_swap swap(&ctx);

    // ch_device wraps the PipelineTestTop Component
    // This provides the parent context for all internal ch_module instances
    ch::ch_device<PipelineTestTop> top;
    Simulator sim(top.context());

    // Load test binary
    std::string bin_path = "/workspace/project/riscv-tests/build/rv32ui/" + bin_filename;
    std::vector<uint32_t> program = load_binary_file(bin_path);

    if (program.empty()) {
        std::cerr << "SKIP: Binary not found: " << bin_path << std::endl;
        return false;
    }

    // Load program into ITCM via exposed write interface
    for (size_t i = 0; i < program.size(); i++) {
        sim.set_input_value(top.instance().io().itcm_write_addr, static_cast<uint32_t>(i));
        sim.set_input_value(top.instance().io().itcm_write_data, program[i]);
        sim.set_input_value(top.instance().io().itcm_write_en, 1);
        sim.tick();
        sim.set_input_value(top.instance().io().itcm_write_en, 0);
    }

    // Initialize tohost to 0 in DTCM
    sim.set_input_value(top.instance().io().dtcm_read_addr, RV32I_TOHOST_ADDR / 4);
    sim.set_input_value(top.instance().io().dtcm_write_addr, RV32I_TOHOST_ADDR / 4);
    sim.set_input_value(top.instance().io().dtcm_write_data, 0);
    sim.set_input_value(top.instance().io().dtcm_write_en, 1);
    sim.tick();
    sim.set_input_value(top.instance().io().dtcm_write_en, 0);

    // Run until tohost changes or max ticks
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

    if (!test_complete) {
        std::cerr << "TIMEOUT: " << test_name << " (" << MAX_TICKS << " ticks)" << std::endl;
        return false;
    }

    if (tohost_value != 1) {
        std::cerr << "FAIL: " << test_name << " returned " << tohost_value << std::endl;
        return false;
    }

    std::cout << "PASS: " << test_name << std::endl;
    return true;
}

// ============================================================================
// Test Cases
// ============================================================================
TEST_CASE("rv32ui-p-simple through pipeline", "[pipeline][rv32ui][simple]") {
    bool passed = run_riscv_test_pipeline("simple", "simple.bin");
    if (!passed) {
        std::ifstream f("/workspace/project/riscv-tests/build/rv32ui/simple.bin");
        if (!f.is_open()) SKIP("Binary not available");
        FAIL("Test failed or timed out");
    }
    SUCCEED("Test passed through hardware pipeline");
}

TEST_CASE("rv32ui-p-add through pipeline", "[pipeline][rv32ui][add]") {
    bool passed = run_riscv_test_pipeline("add", "add.bin");
    if (!passed) {
        std::ifstream f("/workspace/project/riscv-tests/build/rv32ui/add.bin");
        if (!f.is_open()) SKIP("Binary not available");
        FAIL("Test failed or timed out");
    }
    SUCCEED("Test passed through hardware pipeline");
}

TEST_CASE("rv32ui-p-addi through pipeline", "[pipeline][rv32ui][addi]") {
    bool passed = run_riscv_test_pipeline("addi", "addi.bin");
    if (!passed) {
        std::ifstream f("/workspace/project/riscv-tests/build/rv32ui/addi.bin");
        if (!f.is_open()) SKIP("Binary not available");
        FAIL("Test failed or timed out");
    }
    SUCCEED("Test passed through hardware pipeline");
}
