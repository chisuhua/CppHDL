#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "simulator.h"
#include "riscv/rv32i_branch_predict.h"

using namespace ch;
using namespace ch::core;
using namespace riscv;

TEST_CASE("Debug BLT", "[debug]") {
    ch::ch_device<BranchConditionUnit> unit;
    ch::Simulator sim(unit.context());
    
    sim.set_input_value(unit.instance().io().branch_type, 4);
    sim.set_input_value(unit.instance().io().rs1_data, 10);
    sim.set_input_value(unit.instance().io().rs2_data, 20);
    
    sim.tick();
    
    auto condition = static_cast<uint64_t>(sim.get_port_value(unit.instance().io().branch_condition));  // ch_uint<1>
    auto rs1_val = static_cast<uint64_t>(sim.get_port_value(unit.instance().io().rs1_data));
    auto rs2_val = static_cast<uint64_t>(sim.get_port_value(unit.instance().io().rs2_data));
    auto rs1_sign = rs1_val >> 31;
    auto rs2_sign = rs2_val >> 31;
    
    std::cout << "branch_type=" << 4 << std::endl;
    std::cout << "rs1_data=" << rs1_val << ", rs2_data=" << rs2_val << std::endl;
    std::cout << "rs1_sign=" << rs1_sign << ", rs2_sign=" << rs2_sign << std::endl;
    std::cout << "condition=" << condition << std::endl;
    std::cout << "Expected: condition=1 (because 10 < 20)" << std::endl;
    
    // 测试无符号比较
    std::cout << "Direct C++ comparison: (10 < 20) = " << (10 < 20) << std::endl;
    
    REQUIRE(condition == 1);
}
