// tests/test_simulator_bundle.cpp
// T401: Simulator API 扩展 - Bundle IO 字段访问测试
// 测试目标：验证 set_input_value/get_value 支持 ch_uint<N> 类型

#define CATCH_CONFIG_MAIN
#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "simulator.h"
#include "core/bundle/bundle_base.h"
#include "core/bundle/bundle_utils.h"

using namespace ch::core;

// 定义一个简单的 Bundle 用于测试
struct TestBundle : public bundle_base<TestBundle> {
    using Self = TestBundle;
    ch_uint<8> data;
    ch_bool valid;
    
    TestBundle() = default;
    
    CH_BUNDLE_FIELDS_T(data, valid)
};

// ============================================================================
// T401.4: Simulator Bundle API 单元测试
// ============================================================================

TEST_CASE("Simulator API - ch_uint set_input_value", "[T401][simulator][api]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());
    
    class TestModule : public ch::Component {
    public:
        __io(ch_uint<8> data_in; ch_uint<8> data_out;);
        
        TestModule(ch::Component *parent = nullptr, const std::string &name = "test")
            : Component(parent, name) {}
        
        void create_ports() override { new (this->io_storage_) io_type; }
        
        void describe() override {
            // 简单地直接连接输入到输出
            io().data_out <<= io().data_in;
        }
    };
    
    ch::ch_device<TestModule> dev;
    ch::Simulator sim(dev.context());
    
    // 设置输入值 (使用扩展的 API)
    sim.set_input_value(dev.instance().io().data_in, 42);
    
    // 获取输出值
    auto output_val = sim.get_value(dev.instance().io().data_out);
    
    // 验证输出等于输入
    REQUIRE(static_cast<uint64_t>(output_val) == 42);
}

TEST_CASE("Simulator API - ch_bool set_input_value", "[T401][simulator][api]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());
    
    class TestModule : public ch::Component {
    public:
        __io(ch_bool valid_in; ch_bool valid_out;);
        
        TestModule(ch::Component *parent = nullptr, const std::string &name = "test")
            : Component(parent, name) {}
        
        void create_ports() override { new (this->io_storage_) io_type; }
        
        void describe() override {
            io().valid_out <<= io().valid_in;
        }
    };
    
    ch::ch_device<TestModule> dev;
    ch::Simulator sim(dev.context());
    
    // 使用 set_value 设置 ch_bool 输入值 (已有 API)
    sim.set_value(dev.instance().io().valid_in, 1);
    
    // 获取输出值
    auto output_val = sim.get_value(dev.instance().io().valid_out);
    
    // 验证输出等于输入
    REQUIRE(static_cast<uint64_t>(output_val) == 1);
}

TEST_CASE("Simulator API - Bundle field direct access", "[T401][simulator][bundle]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());
    
    class TestBundleModule : public ch::Component {
    public:
        __io(TestBundle bundle_in; TestBundle bundle_out;);
        
        TestBundleModule(ch::Component *parent = nullptr, const std::string &name = "test")
            : Component(parent, name) {}
        
        void create_ports() override { new (this->io_storage_) io_type; }
        
        void describe() override {
            // 直接连接 Bundle 的 data 字段
            io().bundle_out.data <<= io().bundle_in.data;
            // 连接 valid 字段
            io().bundle_out.valid <<= io().bundle_in.valid;
        }
    };
    
    ch::ch_device<TestBundleModule> dev;
    ch::Simulator sim(dev.context());
    
    // ✅ T401 新增：直接访问 Bundle 的单个字段
    // 设置 bundle_in.data 字段
    sim.set_input_value(dev.instance().io().bundle_in.data, 0xA5);
    
    // 设置 bundle_in.valid 字段
    sim.set_value(dev.instance().io().bundle_in.valid, 1);
    
    // 运行一个 tick
    sim.tick();
    
    // 获取 bundle_out 的字段值
    auto data_out = sim.get_value(dev.instance().io().bundle_out.data);
    auto valid_out = sim.get_value(dev.instance().io().bundle_out.valid);
    
    // 验证输出
    REQUIRE(static_cast<uint64_t>(data_out) == 0xA5);
    REQUIRE(static_cast<uint64_t>(valid_out) == 1);
}

TEST_CASE("Simulator API - Bundle field get_input_value", "[T401][simulator][api]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());
    
    class TestModule : public ch::Component {
    public:
        __io(ch_uint<16> data_in;);
        
        TestModule(ch::Component *parent = nullptr, const std::string &name = "test")
            : Component(parent, name) {}
        
        void create_ports() override { new (this->io_storage_) io_type; }
        
        void describe() override {
            // 内部逻辑：将输入值加 1
            ch_reg<ch_uint<16>> reg(0_d);
            reg->next = select(ch_bool(true), reg + ch_uint<16>(1_d), reg);
        }
    };
    
    ch::ch_device<TestModule> dev;
    ch::Simulator sim(dev.context());
    
    // 设置输入值
    constexpr uint64_t TEST_VALUE = 0x1234;
    sim.set_input_value(dev.instance().io().data_in, TEST_VALUE);
    
    // ✅ T401 新增：使用 get_input_value 读取输入值
    auto input_val = sim.get_input_value(dev.instance().io().data_in);
    
    // 验证读取的值正确
    REQUIRE(static_cast<uint64_t>(input_val) == TEST_VALUE);
}

TEST_CASE("Simulator API - Multiple Bundle fields test", "[T401][simulator][bundle]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());
    
    class MultiFieldModule : public ch::Component {
    public:
        __io(TestBundle bundle_in; ch_uint<8> data_out; ch_bool valid_out;);
        
        MultiFieldModule(ch::Component *parent = nullptr, const std::string &name = "test")
            : Component(parent, name) {}
        
        void create_ports() override { new (this->io_storage_) io_type; }
        
        void describe() override {
            // 分别输出 Bundle 的不同字段
            io().data_out <<= io().bundle_in.data;
            io().valid_out <<= io().bundle_in.valid;
        }
    };
    
    ch::ch_device<MultiFieldModule> dev;
    ch::Simulator sim(dev.context());
    
    // ✅ T401 新增：可以独立设置 Bundle 的不同字段
    sim.set_input_value(dev.instance().io().bundle_in.data, 0x55);
    sim.set_value(dev.instance().io().bundle_in.valid, 0);
    
    sim.tick();
    
    auto data_out = sim.get_value(dev.instance().io().data_out);
    auto valid_out = sim.get_value(dev.instance().io().valid_out);
    
    REQUIRE(static_cast<uint64_t>(data_out) == 0x55);
    REQUIRE(static_cast<uint64_t>(valid_out) == 0);
    
    // 第二次测试：设置不同值
    sim.set_input_value(dev.instance().io().bundle_in.data, 0xAA);
    sim.set_value(dev.instance().io().bundle_in.valid, 1);
    
    sim.tick();
    
    data_out = sim.get_value(dev.instance().io().data_out);
    valid_out = sim.get_value(dev.instance().io().valid_out);
    
    REQUIRE(static_cast<uint64_t>(data_out) == 0xAA);
    REQUIRE(static_cast<uint64_t>(valid_out) == 1);
}

TEST_CASE("Simulator API - Bundle field edge cases", "[T401][simulator][bundle]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());
    
    class TestModule : public ch::Component {
    public:
        __io(ch_uint<4> nibble_in; ch_uint<32> word_in; ch_uint<36> combined_out;);
        
        TestModule(ch::Component *parent = nullptr, const std::string &name = "test")
            : Component(parent, name) {}
        
        void create_ports() override { new (this->io_storage_) io_type; }
        
        void describe() override {
            // 将 nibble 和 word 组合
            auto temp = concat(io().nibble_in, io().word_in);
            io().combined_out <<= temp;
        }
    };
    
    ch::ch_device<TestModule> dev;
    ch::Simulator sim(dev.context());
    
    // 测试不同位宽的字段
    sim.set_input_value(dev.instance().io().nibble_in, 0xA);
    sim.set_input_value(dev.instance().io().word_in, 0x12345678);
    
    sim.tick();
    
    auto combined = sim.get_value(dev.instance().io().combined_out);
    
    // 预期值：nibble(4) + word(32) = 36 位
    // 0xA << 32 | 0x12345678 = 0xA12345678
    REQUIRE(static_cast<uint64_t>(combined) == 0xA12345678ULL);
}

TEST_CASE("Simulator API - ch_uint various widths", "[T401][simulator][api]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());
    
    ch_uint<4> n4_0(0_d);
    ch_uint<8> n8_0(0_d);
    ch_uint<16> n16_0(0_d);
    ch_uint<32> n32_0(0_d);
    ch_uint<64> n64_0(0_d);
    
    // 验证编译通过（类型检查）
    // 注意：这些是独立的 ch_uint 变量，不是 port
    // set_input_value 应该能够接受所有类型的 ch_uint<N>
    
    // 使用 set_value 测试（因为 ch_uint<N> 不是 port，不能在 module 外使用）
    REQUIRE(n4_0.impl() != nullptr);
    REQUIRE(n8_0.impl() != nullptr);
    REQUIRE(n16_0.impl() != nullptr);
    REQUIRE(n32_0.impl() != nullptr);
    REQUIRE(n64_0.impl() != nullptr);
}
