/**
 * @file test_mem_stage.cpp
 * @brief RISC-V MEM 级 (访存级) 单元测试
 * 
 * 测试 MEM 阶段组件结构
 * 
 * 作者：DevMate
 * 创建日期：2026-04-09
 */

#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "riscv/stages/mem_stage.h"

using namespace ch::core;
using namespace riscv;

/**
 * MEM 级组件实例化测试
 */
TEST_CASE("MEM Stage - Component Instantiation", "[mem_stage][component]") {
    context ctx("mem_test");
    ctx_swap swap(&ctx);
    
    class MemTop : public ch::Component {
    public:
        __io()
        MemTop(ch::Component* parent = nullptr, const std::string& name = "mem_top")
            : ch::Component(parent, name) { mem_stage_ = new MemStage(this, "mem"); }
        void create_ports() override { new (io_storage_) io_type; }
        void describe() override {}
    private:
        MemStage* mem_stage_;
    };
    
    ch_device<MemTop> device;
    REQUIRE(device.context() != nullptr);
    REQUIRE(device.instance().child_count() == 1);
}
