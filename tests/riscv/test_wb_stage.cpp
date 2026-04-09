/**
 * @file test_wb_stage.cpp
 * @brief RISC-V WB 级 (写回级) 单元测试
 * 
 * 测试 WB 阶段组件结构
 * 
 * 作者：DevMate
 * 创建日期：2026-04-09
 */

#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "riscv/stages/wb_stage.h"

using namespace ch::core;
using namespace riscv;

/**
 * WB 级组件实例化测试
 */
TEST_CASE("WB Stage - Component Instantiation", "[wb_stage][component]") {
    context ctx("wb_test");
    ctx_swap swap(&ctx);
    
    class WbTop : public ch::Component {
    public:
        __io()
        WbTop(ch::Component* parent = nullptr, const std::string& name = "wb_top")
            : ch::Component(parent, name) { wb_stage_ = new WbStage(this, "wb"); }
        void create_ports() override { new (io_storage_) io_type; }
        void describe() override {}
    private:
        WbStage* wb_stage_;
    };
    
    ch_device<WbTop> device;
    REQUIRE(device.context() != nullptr);
    REQUIRE(device.instance().child_count() == 1);
}
