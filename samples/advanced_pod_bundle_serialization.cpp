// samples/advanced_pod_bundle_serialization.cpp
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
        // 分多次调用make_output，因为最多支持4个参数
        this->make_output(part1, part2, part3, part4);
        this->make_output(flag);
        this->make_input(ready);
    }

    void as_slave_direction() {
        // 分多次调用make_input
        this->make_input(part1, part2, part3, part4);
        this->make_input(flag);
        this->make_output(ready);
    }
};

int main() {
    std::cout << "Advanced POD to Bundle Serialization Demo" << std::endl;
    std::cout << "========================================" << std::endl;

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

    // 使用新的序列化工具函数
    auto uint64_array = ch::core::detail::serialize_pod_to_uint64_array(large);
    std::cout << "Serialized to " << uint64_array.size()
              << " uint64_t values:" << std::endl;
    for (size_t i = 0; i < uint64_array.size(); ++i) {
        std::cout << "  [" << i << "]: 0x" << std::hex << uint64_array[i]
                  << std::dec << std::endl;
    }

    VeryLargeData deserialized_large =
        ch::core::detail::deserialize_pod_from_uint64_array<VeryLargeData>(
            uint64_array);
    std::cout << "Deserialized: ";
    deserialized_large.print();

    std::cout << "Equal? " << (large == deserialized_large ? "Yes" : "No")
              << std::endl;

    // 测试3: Bundle序列化
    std::cout << "\n=== Test 3: Bundle Serialization ===" << std::endl;
    very_large_data_bundle bundle;
    bundle.part1 = 0x1111111111111111_d;
    bundle.part2 = 0x22222222_d;
    bundle.part3 = 0x3333_d;
    bundle.part4 = 0x44_d;
    bundle.flag = true;
    bundle.ready = false;

    auto bundle_bits = bundle.to_bits();
    std::cout << "Bundle serialized to bits: 0x" << std::hex
              << bundle_bits.value() << std::dec << std::endl;

    // 测试4: Bundle字段数量和宽度
    std::cout << "\n=== Test 4: Bundle Field Count and Width ===" << std::endl;
    std::cout << "Field count: " << bundle_field_count_v<very_large_data_bundle>
              << std::endl;
    std::cout << "Bundle width: " << bundle.width() << " bits" << std::endl;

    // 测试5: Bundle字段访问
    std::cout << "\n=== Test 5: Bundle Field Access ===" << std::endl;
    auto fields = bundle.__bundle_fields();
    std::cout << "Number of fields in tuple: " << std::tuple_size_v<decltype(fields)> << std::endl;

    // 测试6: Bundle方向设置
    std::cout << "\n=== Test 6: Bundle Direction Setting ===" << std::endl;
    bundle.as_master();
    std::cout << "Bundle set as master, role is now: " << static_cast<int>(bundle.get_role()) << std::endl;

    bundle.as_slave();
    std::cout << "Bundle set as slave, role is now: " << static_cast<int>(bundle.get_role()) << std::endl;

    std::cout << "\nDemo completed successfully!" << std::endl;

    return 0;
}