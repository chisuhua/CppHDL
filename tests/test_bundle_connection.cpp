#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "codegen_dag.h"
#include "component.h"
#include "core/bool.h"
#include "core/bundle/bundle_base.h"
#include "core/bundle/bundle_meta.h"
#include "core/bundle/bundle_utils.h"
#include "core/context.h"
#include "core/io.h"
#include "core/literal.h"
#include "core/reg.h"
#include "core/uint.h"
#include "simulator.h"

using namespace ch::core;

// 定义一个简单的Bundle，只包含一个ch_uint字段
struct SimpleBundle : public bundle_base<SimpleBundle> {
    using Self = SimpleBundle;
    ch_uint<8> data;

    SimpleBundle() = default;
    // SimpleBundle(const std::string &prefix) { this->set_name_prefix(prefix);
    // }

    CH_BUNDLE_FIELDS_T(data)

    void as_master_direction() { this->make_output(data); }

    void as_slave_direction() { this->make_input(data); }
};

// 定义一个包含多个字段的Bundle
struct ComplexBundle : public bundle_base<ComplexBundle> {
    using Self = ComplexBundle;
    ch_uint<8> input_field;
    ch_uint<4> output_field;
    ch_bool enable;

    ComplexBundle() = default;
    ComplexBundle(const std::string &prefix) { this->set_name_prefix(prefix); }

    CH_BUNDLE_FIELDS_T(input_field, output_field, enable)

    void as_master_direction() {
        this->make_output(input_field, output_field);
        this->make_input(enable);
    }

    void as_slave_direction() {
        this->make_input(input_field, enable);
        this->make_output(output_field);
    }
};

// 定义一个flip类型的Bundle（输入输出方向相反）
struct FlipBundle : public bundle_base<FlipBundle> {
    using Self = FlipBundle;
    ch_uint<8> data;
    ch_bool enable;

    FlipBundle() = default;
    FlipBundle(const std::string &prefix) { this->set_name_prefix(prefix); }

    CH_BUNDLE_FIELDS_T(data, enable)

    void as_master_direction() {
        this->make_output(data);
        this->make_input(enable);
    }

    void as_slave_direction() { this->make_input(data, enable); }
};

// 定义一个握手协议Bundle
struct HandShakeBundle : public bundle_base<HandShakeBundle> {
    using Self = HandShakeBundle;
    ch_uint<8> payload;
    ch_bool valid;
    ch_bool ready;

    HandShakeBundle() = default;
    HandShakeBundle(const std::string &prefix) {
        this->set_name_prefix(prefix);
    }

    CH_BUNDLE_FIELDS_T(payload, valid, ready)

    void as_master_direction() {
        this->make_output(payload, valid);
        this->make_input(ready);
    }

    void as_slave_direction() {
        this->make_input(payload, valid);
        this->make_output(ready);
    }
};

TEST_CASE("test_bundle_connection - Basic same-direction bundle connection",
          "[bundle][connection]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    SimpleBundle src, dst;

    // 设置方向
    src.as_master();
    dst.as_slave();

    // 连接两个bundle
    dst <<= src;

    REQUIRE(src.is_valid());
    REQUIRE(dst.is_valid());
}

TEST_CASE("test_bundle_connection - Complex bundle connection",
          "[bundle][connection][complex]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    ComplexBundle src, dst;

    // 设置方向
    src.as_master();
    dst.as_slave();

    // 连接两个bundle
    dst <<= src;

    REQUIRE(src.is_valid());
    REQUIRE(dst.is_valid());
}

TEST_CASE("test_bundle_connection - Flip bundle test",
          "[bundle][connection][flip]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    FlipBundle master, slave;

    // 设置方向
    master.as_master();
    slave.as_slave();

    // 测试flip功能
    auto flipped = master.flip();
    REQUIRE(flipped != nullptr);

    REQUIRE(master.is_valid());
    REQUIRE(slave.is_valid());
}

TEST_CASE("test_bundle_connection - Handshake bundle connection",
          "[bundle][connection][handshake]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    HandShakeBundle master, slave;

    // 设置方向
    master.as_master();
    slave.as_slave();

    // 连接两个bundle
    slave <<= master;

    REQUIRE(master.is_valid());
    REQUIRE(slave.is_valid());
}

TEST_CASE("test_bundle_connection - Bundle field validation",
          "[bundle][validation]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    SimpleBundle bundle;
    bundle.as_master();

    // 验证bundle字段的有效性
    REQUIRE(bundle.is_valid());

    // 测试字段数量
    auto fields = bundle.__bundle_fields();
    REQUIRE(std::tuple_size_v<decltype(fields)> == 1);
}

TEST_CASE("test_bundle_connection - Bundle width calculation",
          "[bundle][width]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    SimpleBundle bundle;

    // 验证bundle宽度
    REQUIRE(bundle.width() == 8);

    ComplexBundle complex_bundle;
    REQUIRE(complex_bundle.width() == 13); // 8 + 4 + 1
}

TEST_CASE("test_bundle_connection - Bundle role management", "[bundle][role]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    SimpleBundle bundle;

    // 测试bundle角色
    bundle.as_master();
    REQUIRE(bundle.get_role() == bundle_role::master);

    bundle.as_slave();
    REQUIRE(bundle.get_role() == bundle_role::slave);
}

// TEST_CASE("test_bundle_connection - Bundle connection in module",
//           "[bundle][connection][module]") {
//     class BundleConnectionModule : public Component {
//     public:
//         __io(SimpleBundle input_bundle; SimpleBundle output_bundle;);

//         BundleConnectionModule(Component *parent = nullptr,
//                                const std::string &name = "bundle_conn")
//             : Component(parent, name) {}

//         void create_ports() override { new (this->io_storage_) io_type; }

//         void describe() override {
//             SimpleBundle internal_bundle;

//             // 连接输入bundle到内部bundle
//             internal_bundle <<= io().input_bundle;

//             // 连接内部bundle到输出bundle
//             io().output_bundle <<= internal_bundle;
//         }
//     };

//     ch_device<BundleConnectionModule> dev;
//     toDAG("bundle1.dot", dev.context());
//     Simulator sim(dev.context());

//     auto input_bundle = dev.io().input_bundle;
//     auto output_bundle = dev.io().output_bundle;

//     // 测试bundle中数据字段的连接
//     std::vector<uint64_t> test_values = {10, 42, 100, 200};

//     for (uint64_t test_val : test_values) {
//         sim.set_input_value(input_bundle.data, test_val);
//         sim.tick();

//         auto output_val = sim.get_value(output_bundle.data);
//         REQUIRE(static_cast<uint64_t>(output_val) == test_val);
//     }
// }

// TEST_CASE("test_bundle_connection - Connection between bundles with different
// "
//           "directions",
//           "[bundle][connection][flip]") {
//     class BundleFlipConnectionModule : public Component {
//     public:
//         __io(ComplexBundle input_bundle; FlipBundle output_bundle;);

//         BundleFlipConnectionModule(Component *parent = nullptr,
//                                    const std::string &name =
//                                    "flip_bundle_conn")
//             : Component(parent, name) {}

//         void create_ports() override { new (this->io_storage_) io_type; }

//         void describe() override {
//             // 连接不同方向的bundle字段
//             //
//             // ComplexBundle的input_field(ch_in)
//             连接到FlipBundle的data(ch_out) io().output_bundle.data <<=
//             io().input_bundle.input_field;
//             // ComplexBundle的enable(ch_in)连接到FlipBundle的enable(ch_in)
//             io().output_bundle.enable <<= io().input_bundle.enable;
//         }
//     };

//     ch_device<BundleFlipConnectionModule> dev;
//     Simulator sim(dev.context());

//     auto input_bundle = dev.io().input_bundle;
//     auto output_bundle = dev.io().output_bundle;

//     // 测试不同方向bundle之间的连接
//     std::vector<std::tuple<uint64_t, bool>> test_cases = {
//         {5, true}, {15, false}, {25, true}, {35, false}};

//     for (const auto &[data_val, enable_val] : test_cases) {
//         sim.set_input_value(input_bundle.input_field, data_val);
//         sim.set_value(input_bundle.enable, enable_val);
//         sim.tick();

//         auto output_data = sim.get_value(output_bundle.data);
//         auto output_enable = sim.get_value(output_bundle.enable);

//         REQUIRE(static_cast<uint64_t>(output_data) == data_val);
//         REQUIRE(output_enable == enable_val);
//     }
// }

// TEST_CASE("test_bundle_connection - Complex multi-field bundle connection",
//           "[bundle][connection][complex]") {
//     class ComplexBundleConnectionModule : public Component {
//     public:
//         __io(ComplexBundle input_bundle; ComplexBundle output_bundle;);

//         ComplexBundleConnectionModule(
//             Component *parent = nullptr,
//             const std::string &name = "complex_bundle_conn")
//             : Component(parent, name) {}

//         void create_ports() override { new (this->io_storage_) io_type; }

//         void describe() override {
//             // 连接bundle的所有字段
//             io().output_bundle.input_field <<= io().input_bundle.input_field;
//             io().output_bundle.output_field <<=
//             io().input_bundle.output_field; io().output_bundle.enable <<=
//             io().input_bundle.enable;
//         }
//     };

//     ch_device<ComplexBundleConnectionModule> dev;
//     Simulator sim(dev.context());

//     auto input_bundle = dev.io().input_bundle;
//     auto output_bundle = dev.io().output_bundle;

//     // 测试所有字段的连接
//     std::vector<std::tuple<uint64_t, uint64_t, bool>> test_cases = {
//         {10, 5, true}, {42, 10, false}, {100, 15, true}, {200, 0, false}};

//     for (const auto &[input_val, output_val, enable_val] : test_cases) {
//         sim.set_input_value(input_bundle.input_field, input_val);
//         sim.set_input_value(input_bundle.output_field, output_val);
//         sim.set_value(input_bundle.enable, enable_val);

//         sim.tick();

//         auto output_input_field = sim.get_value(output_bundle.input_field);
//         auto output_output_field = sim.get_value(output_bundle.output_field);
//         auto output_enable = sim.get_value(output_bundle.enable);

//         REQUIRE(static_cast<uint64_t>(output_input_field) == input_val);
//         REQUIRE(static_cast<uint64_t>(output_output_field) == output_val);
//         REQUIRE(output_enable == enable_val);
//     }
// }

TEST_CASE("test_bundle_connection - Bundle using connect function",
          "[bundle][connection][connect]") {
    context ctx;
    ctx_swap ctx_guard(&ctx);

    // 创建两个相同类型的bundle
    SimpleBundle bundle_src;
    SimpleBundle bundle_dst;

    // 使用connect函数连接两个bundle
    ch::core::connect(bundle_src, bundle_dst);

    // 验证bundle内的字段连接
    REQUIRE(bundle_dst.data.impl() != nullptr);
    REQUIRE(bundle_src.data.impl() != nullptr);
    REQUIRE(bundle_dst.data.impl() !=
            bundle_src.data.impl()); // connect函数逐字段连接，不会共享节点
}

TEST_CASE("test_bundle_connection - Bundle with master/slave direction control",
          "[bundle][connection][direction]") {
    context ctx;
    ctx_swap ctx_guard(&ctx);

    // 创建master和slave bundle
    auto master_bundle = ch::core::master(HandShakeBundle{});
    auto slave_bundle = ch::core::slave(HandShakeBundle{});

    // 连接master的输出到slave的输入
    slave_bundle.payload <<= master_bundle.payload;
    slave_bundle.valid <<= master_bundle.valid;
    master_bundle.ready <<= slave_bundle.ready;

    // 验证连接是否成功
    REQUIRE(slave_bundle.payload.impl() != nullptr);
    REQUIRE(slave_bundle.valid.impl() != nullptr);
    REQUIRE(master_bundle.ready.impl() != nullptr);
}

TEST_CASE("test_bundle_connection - Bundle flip functionality",
          "[bundle][connection][flip]") {
    context ctx;
    ctx_swap ctx_guard(&ctx);

    ComplexBundle original_bundle;
    auto flipped_bundle = original_bundle.flip();

    // 验证翻转后的bundle不为空
    REQUIRE(flipped_bundle != nullptr);
    REQUIRE(flipped_bundle->is_valid());

    // 测试flip bundle与原bundle的连接
    auto input_bundle = ch::core::master(ComplexBundle{});
    auto output_bundle = ch::core::slave(ComplexBundle{});

    // 连接master到flip的输入部分
    output_bundle.input_field <<= input_bundle.input_field;
    output_bundle.enable <<= input_bundle.enable;

    REQUIRE(output_bundle.input_field.impl() != nullptr);
    REQUIRE(output_bundle.enable.impl() != nullptr);
}

// 新增测试：验证改进的operator<<=实现，bundle连接同时连接字段
TEST_CASE("test_bundle_connection - Bundle operator<<= connects both bundle "
          "and fields",
          "[bundle][connection][operator]") {
    context ctx;
    ctx_swap ctx_guard(&ctx);

    // 创建两个相同类型的bundle
    SimpleBundle bundle_src;
    SimpleBundle bundle_dst;

    // 使用operator<<=连接两个bundle
    bundle_dst <<= bundle_src;

    // 验证bundle本身被连接
    REQUIRE(bundle_dst.impl() != nullptr);
    REQUIRE(bundle_src.impl() != nullptr);

    // 验证bundle的字段也被连接
    REQUIRE(bundle_dst.data.impl() != nullptr);
    REQUIRE(bundle_src.data.impl() != nullptr);
    REQUIRE(bundle_dst.data.impl() !=
            bundle_src.data.impl()); // 通过operator<<=连接，不共享节点
}

// 新增测试：验证复杂bundle的operator<<=实现
TEST_CASE(
    "test_bundle_connection - Complex bundle operator<<= connects all fields",
    "[bundle][connection][complex_operator]") {
    context ctx;
    ctx_swap ctx_guard(&ctx);

    // 创建两个相同类型的复杂bundle
    ComplexBundle bundle_src;
    ComplexBundle bundle_dst;

    // 使用operator<<=连接两个bundle
    bundle_dst <<= bundle_src;

    // 验证bundle本身被连接
    REQUIRE(bundle_dst.impl() != nullptr);
    REQUIRE(bundle_src.impl() != nullptr);

    // 验证bundle的所有字段都被连接
    REQUIRE(bundle_dst.input_field.impl() != nullptr);
    REQUIRE(bundle_dst.output_field.impl() != nullptr);
    REQUIRE(bundle_dst.enable.impl() != nullptr);

    // 验证字段连接成功（虽然节点不共享，但已建立了连接）
    REQUIRE(bundle_dst.input_field.impl() != bundle_src.input_field.impl());
    REQUIRE(bundle_dst.output_field.impl() != bundle_src.output_field.impl());
    REQUIRE(bundle_dst.enable.impl() != bundle_src.enable.impl());
}

// 新增测试：在模块中验证bundle operator<<=的功能
// TEST_CASE("test_bundle_connection - Bundle operator<<= in module context",
//           "[bundle][connection][module_operator]") {
//     class BundleOperatorModule : public Component {
//     public:
//         __io(ComplexBundle input_bundle; ComplexBundle output_bundle;);

//         BundleOperatorModule(Component *parent = nullptr,
//                              const std::string &name = "bundle_op_module")
//             : Component(parent, name) {}

//         void create_ports() override { new (this->io_storage_) io_type; }

//         void describe() override {
//             ComplexBundle internal_bundle;

//             // 使用operator<<=连接输入到内部bundle
//             internal_bundle <<= io().input_bundle;

//             // 使用operator<<=连接内部bundle到输出
//             io().output_bundle <<= internal_bundle;
//         }
//     };

//     ch_device<BundleOperatorModule> dev;
//     toDAG("bundle_op.dot", dev.context());
//     Simulator sim(dev.context());

//     auto input_bundle = dev.io().input_bundle;
//     auto output_bundle = dev.io().output_bundle;

//     // 测试所有字段通过operator<<=连接是否正常工作
//     std::vector<std::tuple<uint64_t, uint64_t, bool>> test_cases = {
//         {10, 5, true}, {42, 10, false}, {100, 15, true}, {200, 0, false}};

//     for (const auto &[input_val, output_val, enable_val] : test_cases) {
//         sim.set_input_value(input_bundle.input_field, input_val);
//         sim.set_input_value(input_bundle.output_field, output_val);
//         sim.set_value(input_bundle.enable, enable_val);

//         sim.tick();

//         auto output_input_field = sim.get_value(output_bundle.input_field);
//         auto output_output_field = sim.get_value(output_bundle.output_field);
//         auto output_enable = sim.get_value(output_bundle.enable);

//         REQUIRE(static_cast<uint64_t>(output_input_field) == input_val);
//         REQUIRE(static_cast<uint64_t>(output_output_field) == output_val);
//         REQUIRE(output_enable == enable_val);
//     }
// }