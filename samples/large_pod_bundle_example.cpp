// samples/large_pod_bundle_example.cpp
#include "bundle/common_bundles.h"
#include "ch.hpp"
#include "codegen_verilog.h"
#include "component.h"
#include "core/bundle/bundle_utils.h"
#include "module.h"
#include "simulator.h"
#include <bitset>
#include <cstring>
#include <iostream>
#include <type_traits>
#include <vector>

using namespace ch::core;

// 定义一个超过uint64_t大小的POD结构体 (128位)
struct VeryLargeData {
    uint64_t part1; // 64 bits
    uint32_t part2; // 32 bits
    uint16_t part3; // 16 bits
    uint8_t part4;  // 8 bits
    bool flag;      // 1 bit
    // 总共: 121 bits > 64 bits, 需要使用两个uint64_t来表示

    void print() const {
        std::cout << "VeryLargeData: part1=0x" << std::hex << part1
                  << ", part2=0x" << part2 << ", part3=0x" << part3
                  << ", part4=0x" << (int)part4 << ", flag=" << flag << std::dec
                  << std::endl;
    }

    bool operator==(const VeryLargeData &other) const {
        return part1 == other.part1 && part2 == other.part2 &&
               part3 == other.part3 && part4 == other.part4 &&
               flag == other.flag;
    }
};

// 对应的Bundle类型
struct very_large_data_bundle : public bundle_base<very_large_data_bundle> {
    using Self = very_large_data_bundle;
    ch_uint<64> part1;
    ch_uint<32> part2;
    ch_uint<16> part3;
    ch_uint<8> part4;
    ch_bool flag;
    ch_bool ready;

    very_large_data_bundle() = default;
    very_large_data_bundle(const std::string &prefix) {
        this->set_name_prefix(prefix);
    }

    CH_BUNDLE_FIELDS(Self, part1, part2, part3, part4, flag, ready)

    void as_master_direction() {
        this->make_output(part1, part2, part3, part4);
        this->make_output(flag);
        this->make_input(ready);
    }

    void as_slave_direction() {
        this->make_input(part1, part2, part3, part4);
        this->make_input(flag);
        this->make_output(ready);
    }
};

// 扩展的工具函数：将POD结构体序列化为uint64_t数组（处理任意大小的POD）
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

// 扩展的工具函数：从uint64_t数组反序列化为POD结构体
template <typename T>
T deserialize_pod_from_uint64_array(const std::vector<uint64_t> &data) {
    static_assert(std::is_standard_layout_v<T> && std::is_trivial_v<T>,
                  "Type must be POD");

    T result{};
    size_t byte_size = std::min(sizeof(result), data.size() * sizeof(uint64_t));
    std::memcpy(&result, data.data(), byte_size);

    return result;
}

int main() {
    std::cout << "Handling POD Types Larger Than uint64_t with Bundles"
              << std::endl;
    std::cout << "==================================================="
              << std::endl;

    // 测试1: POD结构体大小和内存布局
    std::cout << "\n=== Test 1: POD Structure Analysis ===" << std::endl;
    std::cout << "VeryLargeData size: " << sizeof(VeryLargeData) << " bytes ("
              << (sizeof(VeryLargeData) * 8) << " bits)" << std::endl;

    // 测试2: 大尺寸POD序列化/反序列化
    std::cout
        << "\n=== Test 2: Very Large POD Serialization/Deserialization ==="
        << std::endl;
    VeryLargeData large{0x123456789ABCDEF0ULL, 0xABCD1234, 0xEF56, 0x78, true};
    std::cout << "Original: ";
    large.print();

    auto uint64_array = serialize_pod_to_uint64_array(large);
    std::cout << "Serialized to " << uint64_array.size()
              << " uint64_t values:" << std::endl;
    for (size_t i = 0; i < uint64_array.size(); ++i) {
        std::cout << "  [" << i << "]: 0x" << std::hex << uint64_array[i]
                  << std::dec << std::endl;
    }

    VeryLargeData deserialized_large =
        deserialize_pod_from_uint64_array<VeryLargeData>(uint64_array);
    std::cout << "Deserialized: ";
    deserialized_large.print();

    bool large_match = (large == deserialized_large);
    std::cout << "Serialization/Deserialization match: "
              << (large_match ? "✓" : "✗") << std::endl;

    // 测试3: Bundle创建和分析
    std::cout << "\n=== Test 3: Very Large Bundle Creation and Analysis ==="
              << std::endl;
    auto ctx = std::make_unique<ch::core::context>("demo_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    very_large_data_bundle large_bundle;
    large_bundle.as_slave(); // 设置方向用于测试
    std::cout << "VeryLargeDataBundle width: " << large_bundle.width()
              << " bits" << std::endl;

    // 测试4: 通过仿真器设置大尺寸Bundle值
    std::cout
        << "\n=== Test 4: Very Large Bundle Value Setting via Simulator ==="
        << std::endl;

    // 创建一个模块来测试大尺寸Bundle
    class TestVeryLargeDataModule : public ch::Component {
    public:
        very_large_data_bundle io;

        TestVeryLargeDataModule(
            ch::Component *parent = nullptr,
            const std::string &name = "test_very_large_module")
            : ch::Component(parent, name) {
            io.as_slave_direction();
        }

        void create_ports() override {}

        void describe() override { io.ready = io.flag; }
    };

    ch::ch_device<TestVeryLargeDataModule> test_very_large_device;
    ch::Simulator very_large_sim(test_very_large_device.context());

    // 使用大尺寸POD数据设置Bundle
    VeryLargeData test_very_large_data{0x123456789ABCDEF0ULL, 0xABCD1234,
                                       0xEF56, 0x78, true};
    std::cout << "Setting Very Large Bundle with POD data: ";
    test_very_large_data.print();

    auto very_large_uint64_array =
        serialize_pod_to_uint64_array(test_very_large_data);
    std::cout << "POD serialized to " << very_large_uint64_array.size()
              << " uint64_t values" << std::endl;

    // 通过仿真器设置Bundle值（使用完整数组）
    std::cout << "Using full uint64_t array to set bundle values" << std::endl;

    // 使用新的辅助函数设置Bundle值
    very_large_sim.set_bundle_field_value(
        test_very_large_device.instance().io.part1, very_large_uint64_array, 0,
        64);
    very_large_sim.set_bundle_field_value(
        test_very_large_device.instance().io.part2, very_large_uint64_array, 64,
        32);
    very_large_sim.set_bundle_field_value(
        test_very_large_device.instance().io.part3, very_large_uint64_array, 96,
        16);
    very_large_sim.set_bundle_field_value(
        test_very_large_device.instance().io.part4, very_large_uint64_array,
        112, 8);
    very_large_sim.set_bundle_field_value(
        test_very_large_device.instance().io.flag, very_large_uint64_array, 120,
        1);

    very_large_sim.tick();

    // 获取Bundle值
    std::vector<uint64_t> result_array(2, 0); // 需要2个uint64_t存储121位数据
    // 直接访问Bundle字段的值
    result_array[0] |=
        static_cast<uint64_t>(test_very_large_device.instance().io.part1);
    result_array[0] |=
        (static_cast<uint64_t>(test_very_large_device.instance().io.part2)
         << 32);
    result_array[1] |=
        static_cast<uint64_t>(test_very_large_device.instance().io.part3);
    result_array[1] |=
        (static_cast<uint64_t>(test_very_large_device.instance().io.part4)
         << 16);
    result_array[1] |=
        (static_cast<uint64_t>(test_very_large_device.instance().io.flag)
         << 24);

    std::cout << "Bundle values from simulator:" << std::endl;
    for (size_t i = 0; i < result_array.size(); ++i) {
        std::cout << "  [" << i << "]: 0x" << std::hex << result_array[i]
                  << std::dec << std::endl;
    }

    // 将Bundle值转换回POD
    VeryLargeData result_very_large_data =
        deserialize_pod_from_uint64_array<VeryLargeData>(result_array);
    std::cout << "Result POD data: ";
    result_very_large_data.print();

    bool full_match = (test_very_large_data == result_very_large_data);
    std::cout << "Full conversion match: " << (full_match ? "✓" : "✗")
              << std::endl;

    std::cout << "\nDemo completed successfully!" << std::endl;
    std::cout << "This demonstrates how to handle POD structs larger than "
                 "uint64_t with Bundle types."
              << std::endl;
    std::cout << "Key techniques:" << std::endl;
    std::cout << "1. Use uint64_t arrays to represent large POD data"
              << std::endl;
    std::cout
        << "2. Handle bit field extraction that spans multiple uint64_t values"
        << std::endl;
    std::cout
        << "3. Use specialized Simulator functions to set/get Bundle fields"
        << std::endl;

    return 0;
}