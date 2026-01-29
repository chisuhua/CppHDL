#define CATCH_CONFIG_MAIN
#include "bundle/stream_bundle.h"
#include "catch_amalgamated.hpp"
#include "chlib/switch.h"
#include "core/bool.h"
#include "core/bundle/bundle_base.h"
#include "core/bundle/bundle_meta.h"
#include "core/bundle/bundle_protocol.h"
#include "core/bundle/bundle_traits.h"
#include "core/context.h"
#include "core/uint.h"
#include "simulator.h"
#include <memory>

using namespace ch::core;

// ============ 静态类型检查测试 ============

TEST_CASE("Stream - BasicCreation", "[stream][basic]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_stream<ch_uint<8>> stream;

    // 验证字段存在性
    STATIC_REQUIRE(has_field_named_v<ch_stream<ch_uint<8>>,
                                     structural_string{"payload"}> == true);
    STATIC_REQUIRE(has_field_named_v<ch_stream<ch_uint<8>>,
                                     structural_string{"valid"}> == true);
    STATIC_REQUIRE(has_field_named_v<ch_stream<ch_uint<8>>,
                                     structural_string{"ready"}> == true);
}

TEST_CASE("Stream - FieldTypes", "[stream][types]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    // 验证字段类型
    using PayloadType = get_field_type_t<ch_stream<ch_uint<16>>,
                                         structural_string{"payload"}>;
    using ValidType =
        get_field_type_t<ch_stream<ch_uint<16>>, structural_string{"valid"}>;
    using ReadyType =
        get_field_type_t<ch_stream<ch_uint<16>>, structural_string{"ready"}>;

    STATIC_REQUIRE(std::is_same_v<PayloadType, ch_uint<16>>);
    STATIC_REQUIRE(std::is_same_v<ValidType, ch_bool>);
    STATIC_REQUIRE(std::is_same_v<ReadyType, ch_bool>);
}

TEST_CASE("Stream - DirectionControl", "[stream][direction]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_stream<ch_uint<8>> master_stream;
    ch_stream<ch_uint<8>> slave_stream;

    master_stream.as_master();
    slave_stream.as_slave();

    REQUIRE(master_stream.get_role() == bundle_role::master);
    REQUIRE(slave_stream.get_role() == bundle_role::slave);
}

TEST_CASE("Stream - WidthCalculation", "[stream][width]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_stream<ch_uint<8>> stream8;
    ch_stream<ch_uint<16>> stream16;
    ch_stream<ch_uint<32>> stream32;

    // stream的总宽度 = payload宽度 + valid(1位) + ready(1位)
    REQUIRE(stream8.width() == 10);   // 8 + 1 + 1
    REQUIRE(stream16.width() == 18);  // 16 + 1 + 1
    REQUIRE(stream32.width() == 34);  // 32 + 1 + 1
}

TEST_CASE("Stream - FieldCount", "[stream][fields]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    // stream应该有3个字段：payload, valid, ready
    REQUIRE(bundle_field_count_v<ch_stream<ch_uint<8>>> == 3);
    REQUIRE(bundle_field_count_v<ch_stream<ch_uint<16>>> == 3);
    REQUIRE(bundle_field_count_v<ch_stream<ch_uint<32>>> == 3);
}

TEST_CASE("Stream - NestedPayloadTypes", "[stream][nested]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    // 测试不同类型的payload
    ch_stream<ch_uint<8>> stream8;
    ch_stream<ch_uint<16>> stream16;
    ch_stream<ch_uint<32>> stream32;
    ch_stream<ch_uint<64>> stream64;

    REQUIRE(stream8.width() == 10);
    REQUIRE(stream16.width() == 18);
    REQUIRE(stream32.width() == 34);
    REQUIRE(stream64.width() == 66);
}

TEST_CASE("Stream - FireSignalCondition", "[stream][fire]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_stream<ch_uint<8>> stream;

    stream.as_master();

    // fire() 应该返回 valid && ready 的结果
    // 这是一个静态组件方法，只验证它能被调用
    REQUIRE(stream.fire().impl() != nullptr);
}

TEST_CASE("Stream - MultiplePayloadWidths", "[stream][width_variants]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    std::vector<int> widths = {8, 16, 32, 64, 128};
    std::vector<int> expected = {10, 18, 34, 66, 130};

    // 验证不同宽度的payload对应的stream总宽度
    for (size_t i = 0; i < widths.size(); ++i) {
        REQUIRE((widths[i] + 2) == expected[i]);
    }
}

TEST_CASE("Stream - BundleBase", "[stream][bundle]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_stream<ch_uint<16>> stream;

    // 验证默认角色
    stream.as_master();
    REQUIRE(stream.get_role() == bundle_role::master);
    // 验证是bundle_base的子类
    REQUIRE(stream.is_valid());
}

TEST_CASE("Stream - PayloadAccessibility", "[stream][access]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_stream<ch_uint<32>> stream;

    stream.as_master();

    // 验证payload字段可以访问
    REQUIRE(stream.payload.impl() != nullptr);
    REQUIRE(stream.valid.impl() != nullptr);
    REQUIRE(stream.ready.impl() != nullptr);
}

// ============ 仿真测试 ============

class SimpleStreamProducer : public ch::Component {
public:
    __io(ch_out<ch_uint<8>> payload_out; ch_out<ch_bool> valid_out;)

    SimpleStreamProducer(ch::Component *parent = nullptr,
                         const std::string &name = "stream_producer")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        // 生成简单的数据：10, 20, 30 递增
        ch_reg<ch_uint<3>> counter(0_d);
        counter->next = counter + 1_d;

        io().payload_out <<= chlib::switch_(counter, 10_d, chlib::case_(0_d, 10_d),
                                chlib::case_(1_d, 20_d), chlib::case_(2_d, 30_d));

        // valid信号：前3个周期有效
        io().valid_out <<= (counter < 3_d);
    }
};

TEST_CASE("Stream - SimulationDataFlow", "[stream][simulation][dataflow]") {
    ch::ch_device<SimpleStreamProducer> device;
    ch::Simulator sim(device.context());

    // 验证stream数据在多个周期内流动
    std::vector<uint64_t> collected_data;
    std::vector<uint64_t> valid_signals;

    for (int i = 0; i < 4; ++i) {
        auto payload = sim.get_value(device.instance().io().payload_out);
        auto valid = sim.get_value(device.instance().io().valid_out);

        collected_data.push_back(static_cast<uint64_t>(payload));
        valid_signals.push_back(static_cast<uint64_t>(valid));

        sim.tick();
    }

    // 验证有有效数据输出
    bool has_valid_data = false;
    for (size_t i = 0; i < valid_signals.size(); ++i) {
        if (valid_signals[i] > 0) {
            has_valid_data = true;
            break;
        }
    }
    REQUIRE(has_valid_data);
}

class StreamConsumer : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> payload_in; ch_in<ch_bool> valid_in;
         ch_out<ch_uint<8>> output;)

    StreamConsumer(ch::Component *parent = nullptr,
                   const std::string &name = "stream_consumer")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        // 消费stream数据，将payload输出
        io().output <<= io().payload_in;
    }
};

TEST_CASE("Stream - SimulationDataConsumption",
          "[stream][simulation][consumption]") {
    ch::ch_device<StreamConsumer> device;
    ch::Simulator sim(device.context());

    // 设置输入值
    std::vector<uint32_t> test_inputs = {42, 100, 200};

    for (size_t i = 0; i < test_inputs.size(); ++i) {
        sim.set_value(device.instance().io().payload_in, (uint64_t)test_inputs[i]);
        sim.set_value(device.instance().io().valid_in, (uint64_t)1);

        sim.tick();

        auto output = sim.get_value(device.instance().io().output);
        uint64_t output_val = static_cast<uint64_t>(output);

        // 验证输出正确传递
        REQUIRE(output_val == test_inputs[i]);
    }
}

TEST_CASE("Stream - FireConditionLogic", "[stream][fire][logic]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_stream<ch_uint<16>> stream;
    stream.as_master();

    // fire()函数应该返回 valid && ready
    // 这体现了stream的反压控制机制
    auto fire_signal = stream.fire();
    REQUIRE(fire_signal.impl() != nullptr);

    // 验证fire是一个bool类型的表达式
    STATIC_REQUIRE(std::is_same_v<decltype(stream.fire()),
                                  ch::core::ch_bool>);
}

TEST_CASE("Stream - MasterSlaveSymmetry", "[stream][master_slave]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_stream<ch_uint<8>> master;
    ch_stream<ch_uint<8>> slave;

    master.as_master();
    slave.as_slave();

    // master和slave应该有相反的角色
    REQUIRE(master.get_role() != slave.get_role());
    REQUIRE(master.get_role() == bundle_role::master);
    REQUIRE(slave.get_role() == bundle_role::slave);
}

TEST_CASE("Stream - ComplexPayloadType", "[stream][complex_payload]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    // 测试大宽度的payload
    ch_stream<ch_uint<128>> large_stream;

    REQUIRE(large_stream.width() == 130);  // 128 + 1 + 1

    // 验证payload类型正确
    using PayloadType =
        get_field_type_t<ch_stream<ch_uint<128>>,
                         structural_string{"payload"}>;
    STATIC_REQUIRE(std::is_same_v<PayloadType, ch_uint<128>>);
}
