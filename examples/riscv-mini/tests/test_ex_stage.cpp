/**
 * @file test_ex_stage.cpp
 * @brief RISC-V EX 级 (执行级) 单元测试
 * 
 * 测试 EX 阶段组件结构、ALU 操作码定义
 * 
 * 作者：DevMate
 * 创建日期：2026-04-09
 */

#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "../src/stages/ex_stage.h"

using namespace ch::core;
using namespace riscv;

/**
 * ALU 操作码定义测试
 */
TEST_CASE("EX Stage - ALU OpCode Definitions", "[ex_stage][constants]") {
    REQUIRE(ALU_OP_ADD == 0);
    REQUIRE(ALU_OP_SUB == 1);
    REQUIRE(ALU_OP_AND == 2);
    REQUIRE(ALU_OP_OR == 3);
    REQUIRE(ALU_OP_XOR == 4);
    REQUIRE(ALU_OP_SLL == 5);
    REQUIRE(ALU_OP_SRL == 6);
    REQUIRE(ALU_OP_SRA == 7);
    REQUIRE(ALU_OP_SLT == 8);
    REQUIRE(ALU_OP_SLTU == 9);
}

/**
 * EX 级组件实例化测试
 */
TEST_CASE("EX Stage - Component Instantiation", "[ex_stage][component]") {
    context ctx("ex_test");
    ctx_swap swap(&ctx);
    
    class ExTop : public ch::Component {
    public:
        __io()
        ExTop(ch::Component* parent = nullptr, const std::string& name = "ex_top")
            : ch::Component(parent, name) { ex_stage_ = new ExStage(this, "ex"); }
        void create_ports() override { new (io_storage_) io_type; }
        void describe() override {}
    private:
        ExStage* ex_stage_;
    };
    
    ch_device<ExTop> device;
    REQUIRE(device.context() != nullptr);
    REQUIRE(device.instance().child_count() == 1);
}
