/**
 * @file test_riscv_tests_pipeline.cpp
 * @brief RISC-V RV32UI tests through Rv32iPipeline hardware (Phase F Wave 4)
 */

#include "../../../tests/catch_amalgamated.hpp"
#include "ch.hpp"
#include "simulator.h"
#include "../../include/cpu/pipeline/rv32i_pipeline.h"
#include "../../include/cpu/pipeline/rv32i_tcm.h"

#include <cstdint>
#include <fstream>
#include <vector>
#include <string>
#include <iostream>

using namespace ch::core;
using namespace riscv;

constexpr uint32_t RV32I_TOHOST_ADDR = 0x80001000;
constexpr uint32_t MAX_TICKS = 100000;

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

bool run_riscv_test_pipeline(const std::string& test_name, const std::string& bin_filename) {
    context ctx("rv32ui_" + test_name + "_pipeline");
    ctx_swap swap(&ctx);
    
    ch::ch_device<Rv32iPipeline> pipeline;
    ch::ch_device<InstrTCM<20, 32>> itcm;
    ch::ch_device<DataTCM<20, 32>> dtcm;
    
    Simulator sim;
    
    std::string bin_path = "/workspace/project/riscv-tests/build/rv32ui-" + bin_filename;
    std::vector<uint32_t> program = load_binary_file(bin_path);
    
    if (program.empty()) {
        std::cerr << "SKIP: Binary not found or empty: " << bin_path << std::endl;
        return false;
    }
    
    for (size_t i = 0; i < program.size(); i++) {
        sim.set_input_value(itcm.instance().io().write_addr, static_cast<uint32_t>(i));
        sim.set_input_value(itcm.instance().io().write_data, program[i]);
        sim.set_input_value(itcm.instance().io().write_en, 1);
        sim.tick();
        sim.set_input_value(itcm.instance().io().write_en, 0);
    }
    
    sim.set_input_value(dtcm.instance().io().write_addr, RV32I_TOHOST_ADDR / 4);
    sim.set_input_value(dtcm.instance().io().write_data, 0);
    sim.set_input_value(dtcm.instance().io().write_en, 1);
    sim.tick();
    sim.set_input_value(dtcm.instance().io().write_en, 0);
    
    sim.set_input_value(pipeline.instance().io().rst, 1);
    sim.tick();
    sim.set_input_value(pipeline.instance().io().rst, 0);
    
    uint32_t tohost_value = 0;
    bool test_complete = false;
    
    for (uint32_t tick = 0; tick < MAX_TICKS && !test_complete; tick++) {
        sim.tick();
        
        sim.set_input_value(dtcm.instance().io().addr, RV32I_TOHOST_ADDR / 4);
        sim.tick();
        auto val = sim.get_port_value(dtcm.instance().io().data);
        tohost_value = static_cast<uint32_t>(val);
        
        if (tohost_value != 0) {
            test_complete = true;
        }
    }
    
    if (!test_complete) {
        std::cerr << "TIMEOUT: " << test_name << " did not complete in " << MAX_TICKS << " ticks" << std::endl;
        return false;
    }
    
    if (tohost_value != 1) {
        std::cerr << "FAIL: " << test_name << " returned " << tohost_value << " (expected 1)" << std::endl;
        return false;
    }
    
    std::cout << "PASS: " << test_name << std::endl;
    return true;
}

TEST_CASE("rv32ui-p-simple through pipeline", "[pipeline][rv32ui][simple]") {
    bool passed = run_riscv_test_pipeline("simple", "rv32ui-p-simple.bin");
    if (!passed) {
        std::ifstream f("/workspace/project/riscv-tests/build/rv32ui-p-simple.bin");
        if (!f.is_open()) SKIP("Binary not available");
        FAIL("Test failed or timed out");
    }
    SUCCEED("Test passed through hardware pipeline");
}

TEST_CASE("rv32ui-p-add through pipeline", "[pipeline][rv32ui][add]") {
    bool passed = run_riscv_test_pipeline("add", "rv32ui-p-add.bin");
    if (!passed) {
        std::ifstream f("/workspace/project/riscv-tests/build/rv32ui-p-add.bin");
        if (!f.is_open()) SKIP("Binary not available");
        FAIL("Test failed or timed out");
    }
    SUCCEED("Test passed through hardware pipeline");
}

TEST_CASE("rv32ui-p-addi through pipeline", "[pipeline][rv32ui][addi]") {
    bool passed = run_riscv_test_pipeline("addi", "rv32ui-p-addi.bin");
    if (!passed) {
        std::ifstream f("/workspace/project/riscv-tests/build/rv32ui-p-addi.bin");
        if (!f.is_open()) SKIP("Binary not available");
        FAIL("Test failed or timed out");
    }
    SUCCEED("Test passed through hardware pipeline");
}
