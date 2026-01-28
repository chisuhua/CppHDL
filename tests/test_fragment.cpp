#define CATCH_CONFIG_MAIN
#include "bundle/axi_lite_bundle.h"
#include "bundle/axi_protocol.h" // 添加axi_protocol.h，其中包含get_field_type的定义
#include "bundle/fragment.h"
#include "bundle/stream_bundle.h"
#include "catch_amalgamated.hpp"
#include "chlib/switch.h"
#include "core/bool.h"
#include "core/bundle/bundle_base.h"
#include "core/bundle/bundle_meta.h"
#include "core/bundle/bundle_protocol.h" // 包含structural_string定义
#include "core/bundle/bundle_traits.h"
#include "core/context.h"
#include "core/traits.h"
#include "core/uint.h"
#include "simulator.h"
#include <array>
#include <memory>

using namespace ch::core;
using namespace chlib;

TEST_CASE("Fragment - BasicCreation", "[fragment][basic]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_fragment<ch_uint<8>> fragment;

    // 验证字段存在性
    STATIC_REQUIRE(has_field_named_v<ch_fragment<ch_uint<8>>,
                                     structural_string{"data_beat"}> == true);
    STATIC_REQUIRE(
        has_field_named_v<ch_fragment<ch_uint<8>>, structural_string{"last"}> ==
        true);

    // 验证字段类型
    using DataType = get_field_type_t<ch_fragment<ch_uint<8>>,
                                      structural_string{"data_beat"}>;
    using LastType =
        get_field_type_t<ch_fragment<ch_uint<8>>, structural_string{"last"}>;

    STATIC_REQUIRE(std::is_same_v<DataType, ch_uint<8>>);
    STATIC_REQUIRE(std::is_same_v<LastType, ch_bool>);
}

TEST_CASE("Fragment - DirectionControl", "[fragment][direction]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_fragment<ch_uint<8>> master_frag;
    ch_fragment<ch_uint<8>> slave_frag;

    master_frag.as_master();
    slave_frag.as_slave();

    REQUIRE(master_frag.get_role() == bundle_role::master);
    REQUIRE(slave_frag.get_role() == bundle_role::slave);
}

TEST_CASE("Fragment - WidthCalculation", "[fragment][width]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_fragment<ch_uint<8>> frag8;
    ch_fragment<ch_uint<16>> frag16;
    ch_fragment<ch_uint<32>> frag32;

    // fragment的总宽度 = 数据宽度 + last信号宽度(1位)
    REQUIRE(frag8.width() == 9);
    REQUIRE(frag16.width() == 17);
    REQUIRE(frag32.width() == 33);
}

TEST_CASE("Fragment - FieldAccess", "[fragment][fields]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_fragment<ch_uint<8>> fragment;

    // 验证字段存在性
    STATIC_REQUIRE(has_field_named_v<ch_fragment<ch_uint<8>>,
                                     structural_string{"data_beat"}> == true);
    STATIC_REQUIRE(
        has_field_named_v<ch_fragment<ch_uint<8>>, structural_string{"last"}> ==
        true);

    // 验证字段类型
    using DataType = get_field_type_t<ch_fragment<ch_uint<8>>,
                                      structural_string{"data_beat"}>;
    using LastType =
        get_field_type_t<ch_fragment<ch_uint<8>>, structural_string{"last"}>;

    STATIC_REQUIRE(std::is_same_v<DataType, ch_uint<8>>);
    STATIC_REQUIRE(std::is_same_v<LastType, ch_bool>);
}

// 创建一个用于测试fragment序列的组件
class FragmentSequenceTestComponent : public ch::Component {
public:
    __io(ch_out<ch_uint<8>> data_out; ch_out<ch_bool> last_out;
         ch_out<ch_bool> valid_out;)

        FragmentSequenceTestComponent(
            ch::Component *parent = nullptr,
            const std::string &name = "fragment_seq_test")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        // 创建多拍数据序列
        std::array<ch_uint<8>, 5> data = {1_d, 2_d, 3_d, 4_d, 5_d};
        auto fragments = fragment_sequence(data);

        // 使用寄存器来跟踪当前传输的是第几拍
        ch_reg<ch_uint<3>> counter(0_d);
        counter->next = counter + 1_d;

        // 根据计数器选择当前输出哪个fragment
        io().data_out = switch_(counter, fragments[0].payload.data_beat,
                                case_(0_d, fragments[0].payload.data_beat),
                                case_(1_d, fragments[1].payload.data_beat),
                                case_(2_d, fragments[2].payload.data_beat),
                                case_(3_d, fragments[3].payload.data_beat),
                                case_(4_d, fragments[4].payload.data_beat));

        io().last_out = switch_(counter, fragments[0].payload.last,
                                case_(0_d, fragments[0].payload.last),
                                case_(1_d, fragments[1].payload.last),
                                case_(2_d, fragments[2].payload.last),
                                case_(3_d, fragments[3].payload.last),
                                case_(4_d, fragments[4].payload.last));

        io().valid_out = true; // 总是有效
    }
};

TEST_CASE("Fragment - MultiBeatSequence", "[fragment][multibeat]") {
    // 使用组件包装fragment序列以支持时序仿真
    ch_device<FragmentSequenceTestComponent> device;
    ch::Simulator simulator(device.context());

    // 第一拍 - 应该是数据1，last为false
    simulator.tick();
    auto data_val = simulator.get_value(device.instance().io().data_out);
    auto last_val = simulator.get_value(device.instance().io().last_out);
    auto valid_val = simulator.get_value(device.instance().io().valid_out);

    REQUIRE(static_cast<uint64_t>(data_val) == 1);
    REQUIRE(static_cast<uint64_t>(last_val) == 0); // 第一个不是last
    REQUIRE(static_cast<uint64_t>(valid_val) == 1);

    // 第二拍 - 应该是数据2，last为false
    simulator.tick();
    data_val = simulator.get_value(device.instance().io().data_out);
    last_val = simulator.get_value(device.instance().io().last_out);

    REQUIRE(static_cast<uint64_t>(data_val) == 2);
    REQUIRE(static_cast<uint64_t>(last_val) == 0); // 第二个不是last

    // 第三拍 - 应该是数据3，last为false
    simulator.tick();
    data_val = simulator.get_value(device.instance().io().data_out);
    last_val = simulator.get_value(device.instance().io().last_out);

    REQUIRE(static_cast<uint64_t>(data_val) == 3);
    REQUIRE(static_cast<uint64_t>(last_val) == 0); // 第三个不是last

    // 第四拍 - 应该是数据4，last为false
    simulator.tick();
    data_val = simulator.get_value(device.instance().io().data_out);
    last_val = simulator.get_value(device.instance().io().last_out);

    REQUIRE(static_cast<uint64_t>(data_val) == 4);
    REQUIRE(static_cast<uint64_t>(last_val) == 0); // 第四个不是last

    // 第五拍 - 应该是数据5，last为true
    simulator.tick();
    data_val = simulator.get_value(device.instance().io().data_out);
    last_val = simulator.get_value(device.instance().io().last_out);

    REQUIRE(static_cast<uint64_t>(data_val) == 5);
    REQUIRE(static_cast<uint64_t>(last_val) == 1); // 第五个是last
}

TEST_CASE("Fragment - ConversionFunctions", "[fragment][conversion]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    // 测试payload到fragment的转换
    ch_uint<8> payload(42_d);
    ch_bool is_last(true);

    auto fragment_flow = payload_to_fragment(payload, is_last);

    // 测试fragment到payload的转换
    ch_flow<ch_fragment<ch_uint<8>>> input_flow;
    input_flow.payload.data_beat = 24_d;
    input_flow.payload.last = false;
    input_flow.valid = true;

    auto payload_flow = fragment_to_payload(input_flow);

    // 使用Simulator进行仿真验证
    ch::Simulator simulator(ctx.get());

    // 验证转换结果
    auto data_val = simulator.get_value(fragment_flow.payload.data_beat);
    auto last_val = simulator.get_value(fragment_flow.payload.last);
    auto valid_val = simulator.get_value(fragment_flow.valid);

    REQUIRE(static_cast<uint64_t>(data_val) == 42);
    REQUIRE(static_cast<uint64_t>(last_val) == 1); // is_last为true
    REQUIRE(static_cast<uint64_t>(valid_val) == 1);

    // 验证反向转换结果
    auto payload_val = simulator.get_value(payload_flow.payload);
    auto payload_valid = simulator.get_value(payload_flow.valid);

    REQUIRE(static_cast<uint64_t>(payload_val) == 24);
    REQUIRE(static_cast<uint64_t>(payload_valid) == 1);
}

TEST_CASE("Fragment - UtilityFunctions", "[fragment][utilities]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ch_flow<ch_fragment<ch_uint<8>>> flow;
    flow.payload.data_beat = 100_d;
    flow.payload.last = true;
    flow.valid = true;

    ch::Simulator sim(ctx.get());

    // 测试is_last_fragment
    auto last_result = is_last_fragment(flow);
    REQUIRE(sim.get_value(last_result) == true);

    // 测试get_last_signal
    auto last_signal = get_last_signal(flow);
    REQUIRE(sim.get_value(last_signal) == true);

    // 测试get_fragment_data
    auto data = get_fragment_data(flow);
    REQUIRE(sim.get_value(data) == 100);
}

// 定义一个包含AXI-Lite操作的复杂Fragment数据类型
struct AXIFragmentData {
    ch_uint<32> address; // 地址
    ch_uint<32> data;    // 数据
    ch_bool write;       // 操作类型：true=写，false=读
};

// AXI-Lite Bundle的Fragment包装
using AXIFragment = ch_fragment<AXIFragmentData>;

TEST_CASE("Fragment - AXILiteIntegration", "[fragment][axi][integration]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    // 创建AXI-Lite接口
    axi_lite_bundle<32, 32> axi_if;
    axi_if.as_master();

    // 创建一个复杂的fragment，包含AXI-Lite相关数据
    AXIFragment axi_fragment;
    axi_fragment.as_master();

    REQUIRE(axi_fragment.is_valid());

    // 验证字段存在
    STATIC_REQUIRE(
        has_field_named_v<AXIFragment, structural_string{"data_beat"}> == true);
    STATIC_REQUIRE(has_field_named_v<AXIFragment, structural_string{"last"}> ==
                   true);

    // 验证嵌套字段
    using DataBeatType =
        get_field_type_t<AXIFragment, structural_string{"data_beat"}>;
    STATIC_REQUIRE(std::is_same_v<DataBeatType, AXIFragmentData>);

    // // 验证AXIFragmentData的字段
    // STATIC_REQUIRE(
    //     has_field_named_v<DataBeatType, structural_string{"address"}> ==
    //     true);
    // STATIC_REQUIRE(has_field_named_v<DataBeatType, structural_string{"data"}>
    // ==
    //                true);
    // STATIC_REQUIRE(
    //     has_field_named_v<DataBeatType, structural_string{"write"}> == true);

    ch::Simulator sim(ctx.get());

    // 测试设置fragment数据
    sim.set_value(axi_fragment.data_beat.address, 0x1000);
    sim.set_value(axi_fragment.data_beat.data, 0xABCD);
    sim.set_value(axi_fragment.data_beat.write, true);
    sim.set_value(axi_fragment.last, true);

    auto address = sim.get_value(axi_fragment.data_beat.address);
    auto data = sim.get_value(axi_fragment.data_beat.data);
    auto write = sim.get_value(axi_fragment.data_beat.write);
    auto last = sim.get_value(axi_fragment.last == true);

    // 验证数据设置成功
    REQUIRE(address == 0x1000);
    REQUIRE(data == 0xABCD);
    REQUIRE(write == true);
    REQUIRE(last == true);

    // 计算复合fragment的宽度
    // address(32) + data(32) + write(1) = 65位数据 + 1位last = 66位
    REQUIRE(axi_fragment.width() == 66);
}

TEST_CASE("Fragment - AXILiteTransactionSequence",
          "[fragment][axi][sequence]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    // 创建AXI-Lite事务序列
    std::array<AXIFragmentData, 4> transactions = {{
        {0x1000_h, 0x1234_h, true}, // 写入0x1234到地址0x1000
        {0x2000_h, 0x5678_h, true}, // 写入0x5678到地址0x2000
        {0x1000_h, 0x0_h, false},   // 从地址0x1000读取
        {0x2000_h, 0x0_h, false}    // 从地址0x2000读取
    }};

    // 创建fragment序列
    std::array<ch_flow<AXIFragment>, 4> axi_fragments;
    for (size_t i = 0; i < 4; ++i) {
        axi_fragments[i].payload.data_beat.address = transactions[i].address;
        axi_fragments[i].payload.data_beat.data = transactions[i].data;
        axi_fragments[i].payload.data_beat.write = transactions[i].write;
        axi_fragments[i].payload.last = (i == 3); // 最后一个是last
        axi_fragments[i].valid = true;
    }

    // 验证序列
    REQUIRE(axi_fragments[0].payload.data_beat.address == 0x1000_h);
    REQUIRE(axi_fragments[0].payload.data_beat.data == 0x1234_h);
    REQUIRE(axi_fragments[0].payload.data_beat.write == true);
    REQUIRE(axi_fragments[0].payload.last == false);

    REQUIRE(axi_fragments[3].payload.data_beat.address == 0x2000_h);
    REQUIRE(axi_fragments[3].payload.data_beat.data == 0x0_h);
    REQUIRE(axi_fragments[3].payload.data_beat.write == false);
    REQUIRE(axi_fragments[3].payload.last == true); // 最后一个
}

TEST_CASE("Fragment - StreamIntegration", "[fragment][stream][integration]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    // 创建一个fragment化的流
    ch_stream<ch_fragment<ch_uint<16>>> fragment_stream;
    fragment_stream.as_master();

    REQUIRE(fragment_stream.is_valid());
    REQUIRE(bundle_field_count_v<ch_stream<ch_fragment<ch_uint<16>>>> ==
            3); // payload, valid, ready

    // 验证嵌套字段结构
    STATIC_REQUIRE(has_field_named_v<ch_stream<ch_fragment<ch_uint<16>>>,
                                     structural_string{"payload"}> == true);
    STATIC_REQUIRE(has_field_named_v<ch_stream<ch_fragment<ch_uint<16>>>,
                                     structural_string{"valid"}> == true);
    STATIC_REQUIRE(has_field_named_v<ch_stream<ch_fragment<ch_uint<16>>>,
                                     structural_string{"ready"}> == true);

    // payload是ch_fragment<ch_uint<16>>类型，应包含data_beat和last
    using PayloadType = get_field_type_t<ch_stream<ch_fragment<ch_uint<16>>>,
                                         structural_string{"payload"}>;
    STATIC_REQUIRE(std::is_same_v<PayloadType, ch_fragment<ch_uint<16>>>);

    // 验证payload的字段
    STATIC_REQUIRE(
        has_field_named_v<PayloadType, structural_string{"data_beat"}> == true);
    STATIC_REQUIRE(has_field_named_v<PayloadType, structural_string{"last"}> ==
                   true);
}

TEST_CASE("Fragment - NestedUsage", "[fragment][nested]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    // 创建嵌套fragment的示例
    ch_fragment<ch_fragment<ch_uint<8>>> nested_fragment;
    nested_fragment.as_master();

    REQUIRE(nested_fragment.is_valid());

    // 验证外层fragment的字段
    STATIC_REQUIRE(has_field_named_v<ch_fragment<ch_fragment<ch_uint<8>>>,
                                     structural_string{"data_beat"}> == true);
    STATIC_REQUIRE(has_field_named_v<ch_fragment<ch_fragment<ch_uint<8>>>,
                                     structural_string{"last"}> == true);

    // 外层data_beat的类型是ch_fragment<ch_uint<8>>
    using OuterDataType = get_field_type_t<ch_fragment<ch_fragment<ch_uint<8>>>,
                                           structural_string{"data_beat"}>;
    STATIC_REQUIRE(std::is_same_v<OuterDataType, ch_fragment<ch_uint<8>>>);

    // 内层fragment也应有相应的字段
    STATIC_REQUIRE(
        has_field_named_v<OuterDataType, structural_string{"data_beat"}> ==
        true);
    STATIC_REQUIRE(
        has_field_named_v<OuterDataType, structural_string{"last"}> == true);
}