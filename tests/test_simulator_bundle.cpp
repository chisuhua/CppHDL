// tests/test_simulator_bundle.cpp
// T401: Simulator API 扩展 - Bundle IO 字段直接访问测试

#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "component.h"
#include "core/bundle/bundle_base.h"
#include "core/io.h"
#include "simulator.h"

using namespace ch::core;

// 定义一个简单的 Bundle 用于测试
struct SimpleBundle : public bundle_base<SimpleBundle> {
    using Self = SimpleBundle;
    ch_uint<8> data;
    
    SimpleBundle() = default;
    
    CH_BUNDLE_FIELDS_T(data)
};

// ============================================================================
// T401: Simulator API扩展测试 - 验证Component的IO字段访问
// ============================================================================

TEST_CASE("Simulator API - Component IO field access", "[T401][simulator]") {
    // 创建包含 Bundle IO 的 Component
    class TestBundleIOComponent : public Component {
    public:
        __io(SimpleBundle data_in; SimpleBundle data_out;);
        
        TestBundleIOComponent(Component *parent = nullptr, const std::string &name = "test")
            : Component(parent, name) {}
        
        void create_ports() override { new (this->io_storage_) io_type; }
        
        void describe() override {
            // 直接连接输入到输出
            io().data_out.data <<= io().data_in.data;
        }
    };
    
    ch::ch_device<TestBundleIOComponent> dev;
    ch::Simulator sim(dev.context());
    
    // T401: 设置 Bundle 字段的值
    sim.set_input_value(dev.instance().io().data_in.data, 0xA5);
    
    sim.tick();
    
    // T401: 读取字段值
    auto val = sim.get_value(dev.instance().io().data_out.data);
    
    REQUIRE(static_cast<uint64_t>(val) == 0xA5);
}

TEST_CASE("Simulator API - get_input_value", "[T401][simulator]") {
    class TestComp : public Component {
    public:
        __io(ch_in<ch_uint<8>> data_in; ch_out<ch_uint<8>> data_out;);
        
        TestComp(Component *parent = nullptr, const std::string &name = "test")
            : Component(parent, name) {}
        
        void create_ports() override { new (this->io_storage_) io_type; }
        
        void describe() override {
            io().data_out <<= io().data_in;
        }
    };
    
    ch::ch_device<TestComp> dev;
    ch::Simulator sim(dev.context());
    
    const uint64_t TEST_VALUE = 0x42;
    
    // 设置值
    sim.set_input_value(dev.instance().io().data_in, TEST_VALUE);
    
    // T401.3: 测试 get_input_value
    auto read_val = sim.get_input_value(dev.instance().io().data_in);
    
    REQUIRE(static_cast<uint64_t>(read_val) == TEST_VALUE);
}

TEST_CASE("Simulator API - Multiple component fields", "[T401][simulator]") {
    class MultiIOComp : public Component {
    public:
        __io(ch_in<ch_uint<8>> field1; ch_in<ch_uint<8>> field2;
             ch_out<ch_uint<8>> out1; ch_out<ch_uint<8>> out2;);
        
        MultiIOComp(Component *parent = nullptr, const std::string &name = "test")
            : Component(parent, name) {}
        
        void create_ports() override { new (this->io_storage_) io_type; }
        
        void describe() override {
            io().out1 <<= io().field1;
            io().out2 <<= io().field2;
        }
    };
    
    ch::ch_device<MultiIOComp> dev;
    ch::Simulator sim(dev.context());
    
    // 设置两个不同的字段
    sim.set_input_value(dev.instance().io().field1, 0xAA);
    sim.set_input_value(dev.instance().io().field2, 0x55);
    
    sim.tick();
    
    // 验证各自的值
    REQUIRE(static_cast<uint64_t>(sim.get_value(dev.instance().io().out1)) == 0xAA);
    REQUIRE(static_cast<uint64_t>(sim.get_value(dev.instance().io().out2)) == 0x55);
}
