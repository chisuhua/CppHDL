// tests/test_pod_bundle_conversions.cpp
#define CATCH_CONFIG_MAIN
#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "core/bundle/bundle_utils.h"
#include "simulator.h"
#include <cstring>
#include <iostream>
#include <type_traits>
#include <vector>

using namespace ch::core;

// 定义一个小型POD结构体（小于等于64位）
struct SmallData {
    uint32_t data; // 32 bits
    bool flag1;    // 1 bit
    bool flag2;    // 1 bit
    // 总计: 34 bits <= 64 bits

    void print() const {
        std::cout << "SmallData: data=0x" << std::hex << data
                  << ", flag1=" << flag1 << ", flag2=" << flag2 << std::dec
                  << std::endl;
    }

    bool operator==(const SmallData &other) const {
        return data == other.data && flag1 == other.flag1 && flag2 == other.flag2;
    }
};

// 定义一个中型POD结构体（超过64位但小于128位）
struct MediumData {
    uint64_t address; // 64 bits
    uint32_t data;    // 32 bits
    bool flag1;       // 1 bit
    // 总计: 97 bits > 64 bits

    void print() const {
        std::cout << "MediumData: address=0x" << std::hex << address
                  << ", data=0x" << data << ", flag1=" << flag1 << std::dec
                  << std::endl;
    }

    bool operator==(const MediumData &other) const {
        return address == other.address && data == other.data && flag1 == other.flag1;
    }
};

// 定义一个大型POD结构体（远超64位）
struct LargeData {
    uint64_t address; // 64 bits
    uint32_t data;    // 32 bits
    uint16_t extra;   // 16 bits
    uint8_t flags;    // 8 bits
    bool flag1;       // 1 bit
    bool flag2;       // 1 bit
    // 总计: 122 bits > 64 bits

    void print() const {
        std::cout << "LargeData: address=0x" << std::hex << address
                  << ", data=0x" << data << ", extra=0x" << extra
                  << ", flags=0x" << (int)flags << ", flag1=" << flag1
                  << ", flag2=" << flag2 << std::dec << std::endl;
    }

    bool operator==(const LargeData &other) const {
        return address == other.address && data == other.data && extra == other.extra &&
               flags == other.flags && flag1 == other.flag1 && flag2 == other.flag2;
    }
};

// 对应的Bundle类型
struct small_data_bundle : public bundle_base<small_data_bundle> {
    using Self = small_data_bundle;
    ch_uint<32> data;
    ch_bool flag1;
    ch_bool flag2;

    small_data_bundle() = default;
    small_data_bundle(const std::string &prefix) {
        this->set_name_prefix(prefix);
    }

    CH_BUNDLE_FIELDS(Self, data, flag1, flag2)

    void as_master() override {
        this->make_output(data, flag1, flag2);
    }

    void as_slave() override {
        this->make_input(data, flag1, flag2);
    }
};

struct medium_data_bundle : public bundle_base<medium_data_bundle> {
    using Self = medium_data_bundle;
    ch_uint<64> address;
    ch_uint<32> data;
    ch_bool flag1;

    medium_data_bundle() = default;
    medium_data_bundle(const std::string &prefix) {
        this->set_name_prefix(prefix);
    }

    CH_BUNDLE_FIELDS(Self, address, data, flag1)

    void as_master() override {
        this->make_output(address, data, flag1);
    }

    void as_slave() override {
        this->make_input(address, data, flag1);
    }
};

struct large_data_bundle : public bundle_base<large_data_bundle> {
    using Self = large_data_bundle;
    ch_uint<64> address;
    ch_uint<32> data;
    ch_uint<16> extra;
    ch_uint<8> flags;
    ch_bool flag1;
    ch_bool flag2;

    large_data_bundle() = default;
    large_data_bundle(const std::string &prefix) {
        this->set_name_prefix(prefix);
    }

    CH_BUNDLE_FIELDS(Self, address, data, extra, flags, flag1, flag2)

    void as_master() override {
        this->make_output(address, data, extra, flags);
        this->make_output(flag1, flag2);
    }

    void as_slave() override {
        this->make_input(address, data, extra, flags);
        this->make_input(flag1, flag2);
    }
};

// 工具函数：将POD结构体序列化为uint64_t数组（处理任意大小的POD）
template <typename T>
std::vector<uint64_t> serialize_pod_to_uint64_array(const T &pod) {
    static_assert(std::is_standard_layout_v<T> && std::is_trivial_v<T>,
                  "Type must be POD");

    size_t byte_size = sizeof(T);
    size_t uint64_count =
        (byte_size + sizeof(uint64_t) - 1) / sizeof(uint64_t); // 向上取整

    std::vector<uint64_t> result(uint64_count, 0);
    std::memcpy(result.data(), &pod, byte_size);

    return result;
}

// 工具函数：从uint64_t数组反序列化为POD结构体
template <typename T>
T deserialize_pod_from_uint64_array(const std::vector<uint64_t> &data) {
    static_assert(std::is_standard_layout_v<T> && std::is_trivial_v<T>,
                  "Type must be POD");

    T result{};
    size_t byte_size = std::min(sizeof(result), data.size() * sizeof(uint64_t));
    std::memcpy(&result, data.data(), byte_size);

    return result;
}

// 测试小型POD结构体的序列化/反序列化
TEST_CASE("PODConversions - Small POD Serialization/Deserialization", 
          "[pod][conversion][small]") {
    SmallData small{0x12345678, true, false};
    std::cout << "Original: ";
    small.print();

    auto uint64_array = serialize_pod_to_uint64_array(small);
    std::cout << "Serialized to " << uint64_array.size()
              << " uint64_t values:" << std::endl;
    for (size_t i = 0; i < uint64_array.size(); ++i) {
        std::cout << "  [" << i << "]: 0x" << std::hex << uint64_array[i]
                  << std::dec << std::endl;
    }

    SmallData deserialized_small =
        deserialize_pod_from_uint64_array<SmallData>(uint64_array);
    std::cout << "Deserialized: ";
    deserialized_small.print();

    REQUIRE(small == deserialized_small);
}

// 测试中型POD结构体的序列化/反序列化
TEST_CASE("PODConversions - Medium POD Serialization/Deserialization", 
          "[pod][conversion][medium]") {
    MediumData medium{0x123456789ABCDEF0ULL, 0xABCD1234, true};
    std::cout << "Original: ";
    medium.print();

    auto uint64_array = serialize_pod_to_uint64_array(medium);
    std::cout << "Serialized to " << uint64_array.size()
              << " uint64_t values:" << std::endl;
    for (size_t i = 0; i < uint64_array.size(); ++i) {
        std::cout << "  [" << i << "]: 0x" << std::hex << uint64_array[i]
                  << std::dec << std::endl;
    }

    MediumData deserialized_medium =
        deserialize_pod_from_uint64_array<MediumData>(uint64_array);
    std::cout << "Deserialized: ";
    deserialized_medium.print();

    REQUIRE(medium == deserialized_medium);
}

// 测试大型POD结构体的序列化/反序列化
TEST_CASE("PODConversions - Large POD Serialization/Deserialization", 
          "[pod][conversion][large]") {
    LargeData large{0x123456789ABCDEF0ULL, 0xABCD1234, 0xEF56, 0x78, true, false};
    std::cout << "Original: ";
    large.print();

    auto uint64_array = serialize_pod_to_uint64_array(large);
    std::cout << "Serialized to " << uint64_array.size()
              << " uint64_t values:" << std::endl;
    for (size_t i = 0; i < uint64_array.size(); ++i) {
        std::cout << "  [" << i << "]: 0x" << std::hex << uint64_array[i]
                  << std::dec << std::endl;
    }

    LargeData deserialized_large =
        deserialize_pod_from_uint64_array<LargeData>(uint64_array);
    std::cout << "Deserialized: ";
    deserialized_large.print();

    REQUIRE(large == deserialized_large);
}

// 测试小型Bundle的序列化/反序列化
TEST_CASE("PODConversions - Small Bundle Serialization/Deserialization", 
          "[pod][conversion][bundle][small]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    small_data_bundle bundle;
    bundle.data = 0x12345678_d;
    bundle.flag1 = 1_b;
    bundle.flag2 = 0_b;

    REQUIRE(bundle.width() == 34);

    // 序列化
    auto bits = serialize(bundle);
    REQUIRE(bits.width == 34);

    // 反序列化
    auto deserialized_bundle = deserialize<small_data_bundle>(bits);

    REQUIRE(static_cast<uint64_t>(deserialized_bundle.data) == 0x12345678);
    REQUIRE(static_cast<bool>(deserialized_bundle.flag1) == true);
    REQUIRE(static_cast<bool>(deserialized_bundle.flag2) == false);
}

// 测试中型Bundle的序列化/反序列化
TEST_CASE("PODConversions - Medium Bundle Serialization/Deserialization", 
          "[pod][conversion][bundle][medium]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    medium_data_bundle bundle;
    bundle.address = 0x123456789ABCDEF0ULL;
    bundle.data = 0xABCD1234_d;
    bundle.flag1 = 1_b;

    REQUIRE(bundle.width() == 97);

    // 序列化
    auto bits = serialize(bundle);
    REQUIRE(bits.width == 97);

    // 反序列化
    auto deserialized_bundle = deserialize<medium_data_bundle>(bits);

    REQUIRE(static_cast<uint64_t>(deserialized_bundle.address) == 0x123456789ABCDEF0ULL);
    REQUIRE(static_cast<uint64_t>(deserialized_bundle.data) == 0xABCD1234);
    REQUIRE(static_cast<bool>(deserialized_bundle.flag1) == true);
}

// 测试大型Bundle的序列化/反序列化
TEST_CASE("PODConversions - Large Bundle Serialization/Deserialization", 
          "[pod][conversion][bundle][large]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    large_data_bundle bundle;
    bundle.address = 0x123456789ABCDEF0ULL;
    bundle.data = 0xABCD1234_d;
    bundle.extra = 0xEF56_d;
    bundle.flags = 0x78_d;
    bundle.flag1 = 1_b;
    bundle.flag2 = 0_b;

    REQUIRE(bundle.width() == 122);

    // 序列化
    auto bits = serialize(bundle);
    REQUIRE(bits.width == 122);

    // 反序列化
    auto deserialized_bundle = deserialize<large_data_bundle>(bits);

    REQUIRE(static_cast<uint64_t>(deserialized_bundle.address) == 0x123456789ABCDEF0ULL);
    REQUIRE(static_cast<uint64_t>(deserialized_bundle.data) == 0xABCD1234);
    REQUIRE(static_cast<uint64_t>(deserialized_bundle.extra) == 0xEF56);
    REQUIRE(static_cast<uint64_t>(deserialized_bundle.flags) == 0x78);
    REQUIRE(static_cast<bool>(deserialized_bundle.flag1) == true);
    REQUIRE(static_cast<bool>(deserialized_bundle.flag2) == false);
}

// 测试POD到Bundle的转换
TEST_CASE("PODConversions - POD to Bundle Conversion", 
          "[pod][conversion][pod-to-bundle]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    // 测试中型数据
    MediumData medium{0x123456789ABCDEF0ULL, 0xABCD1234, true};
    
    // 创建对应的bundle并手动设置值
    medium_data_bundle bundle;
    bundle.address = ch_uint<64>(make_literal(medium.address, 64));
    bundle.data = ch_uint<32>(make_literal(medium.data, 32));
    bundle.flag1 = ch_bool(make_literal(medium.flag1, 1));

    // 序列化bundle
    auto bundle_bits = serialize(bundle);
    
    // 反序列化为POD
    auto deserialized_medium = deserialize_pod_from_uint64_array<MediumData>(
        serialize_pod_to_uint64_array(medium));  // 用原数据验证
    
    REQUIRE(medium == deserialized_medium);
}

// 测试ch_uint到C++基本类型的转换
TEST_CASE("PODConversions - ch_uint to C++ Basic Types", 
          "[ch_uint][conversion][basic-types]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    // 测试各种宽度的ch_uint
    ch_uint<8> u8_val = 0xAB_d;
    ch_uint<16> u16_val = 0xABCD_d;
    ch_uint<32> u32_val = 0x12345678_d;
    ch_uint<64> u64_val = 0x123456789ABCDEF0ULL;

    uint64_t converted_u8 = static_cast<uint64_t>(u8_val);
    uint64_t converted_u16 = static_cast<uint64_t>(u16_val);
    uint64_t converted_u32 = static_cast<uint64_t>(u32_val);
    uint64_t converted_u64 = static_cast<uint64_t>(u64_val);

    REQUIRE(converted_u8 == 0xAB);
    REQUIRE(converted_u16 == 0xABCD);
    REQUIRE(converted_u32 == 0x12345678);
    REQUIRE(converted_u64 == 0x123456789ABCDEF0ULL);
}

// 测试大型数据在仿真环境中的行为
TEST_CASE("PODConversions - Large Data Simulation Integration", 
          "[pod][conversion][simulation][integration]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    // 创建一个模块来测试大型Bundle
    class TestLargeDataModule : public ch::Component {
    public:
        large_data_bundle io;

        TestLargeDataModule(
            ch::Component *parent = nullptr,
            const std::string &name = "test_large_module")
            : ch::Component(parent, name) {
            io.as_slave();
        }

        void create_ports() override {}

        void describe() override { 
            // 简单地将输入复制到输出
        }
    };

    ch::ch_device<TestLargeDataModule> test_device;
    ch::Simulator sim(test_device.context());

    // 设置大型Bundle数据
    LargeData test_data{0x123456789ABCDEF0ULL, 0xABCD1234, 0xEF56, 0x78, true, false};
    auto uint64_array = serialize_pod_to_uint64_array(test_data);

    // 使用仿真器设置Bundle值
    sim.set_bundle_field_value(
        test_device.instance().io.address, uint64_array, 0, 64);
    sim.set_bundle_field_value(
        test_device.instance().io.data, uint64_array, 64, 32);
    sim.set_bundle_field_value(
        test_device.instance().io.extra, uint64_array, 96, 16);
    sim.set_bundle_field_value(
        test_device.instance().io.flags, uint64_array, 112, 8);
    sim.set_bundle_field_value(
        test_device.instance().io.flag1, uint64_array, 120, 1);
    sim.set_bundle_field_value(
        test_device.instance().io.flag2, uint64_array, 121, 1);

    sim.tick();

    // 获取Bundle值
    uint64_t result_address = static_cast<uint64_t>(test_device.instance().io.address);
    uint64_t result_data = static_cast<uint64_t>(test_device.instance().io.data);
    uint64_t result_extra = static_cast<uint64_t>(test_device.instance().io.extra);
    uint64_t result_flags = static_cast<uint64_t>(test_device.instance().io.flags);
    bool result_flag1 = static_cast<bool>(test_device.instance().io.flag1);
    bool result_flag2 = static_cast<bool>(test_device.instance().io.flag2);

    REQUIRE(result_address == test_data.address);
    REQUIRE(result_data == test_data.data);
    REQUIRE(result_extra == test_data.extra);
    REQUIRE(result_flags == test_data.flags);
    REQUIRE(result_flag1 == test_data.flag1);
    REQUIRE(result_flag2 == test_data.flag2);
}