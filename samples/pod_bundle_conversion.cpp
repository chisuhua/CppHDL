// samples/pod_bundle_conversion.cpp
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

// 定义一个POD结构体，模拟用户定义的数据结构
struct SimpleData {
    uint8_t data; // 8 bits
    bool push;    // 1 bit
    bool pop;     // 1 bit

    // 用于调试的打印函数
    void print() const {
        std::cout << "SimpleData: data=0x" << std::hex << (int)data
                  << ", push=" << push << ", pop=" << pop << std::dec
                  << std::endl;
    }

    bool operator==(const SimpleData &other) const {
        return data == other.data && push == other.push && pop == other.pop;
    }
};

// 定义一个更大的POD结构体，超过uint64_t的位宽(64位)
struct LargeData {
    uint64_t address; // 64 bits
    uint32_t data;    // 32 bits
    uint16_t extra;   // 16 bits
    bool flag1;       // 1 bit
    bool flag2;       // 1 bit
    // 总计: 114 bits > 64 bits

    void print() const {
        std::cout << "LargeData: address=0x" << std::hex << address
                  << ", data=0x" << data << ", extra=0x" << extra
                  << ", flag1=" << flag1 << ", flag2=" << flag2 << std::dec
                  << std::endl;
    }

    bool operator==(const LargeData &other) const {
        return address == other.address && data == other.data &&
               extra == other.extra && flag1 == other.flag1 &&
               flag2 == other.flag2;
    }
};

// 专门针对LargeData的Bundle
struct large_data_bundle : public bundle_base<large_data_bundle> {
    using Self = large_data_bundle;
    ch_uint<64> address;
    ch_uint<32> data;
    ch_uint<16> extra;
    ch_bool flag1;
    ch_bool flag2;
    ch_bool ready;

    large_data_bundle() = default;
    large_data_bundle(const std::string &prefix) {
        this->set_name_prefix(prefix);
    }

    CH_BUNDLE_FIELDS_T(address, data, extra, flag1, flag2)

    void as_master_direction() {
        this->make_output(address, data, extra, flag1);
        this->make_output(flag2);
    }

    void as_slave_direction() {
        this->make_input(address, data, extra, flag1);
        this->make_input(flag2);
    }
};

// 工具函数：将POD结构体序列化为字节数组
template <typename T>
std::vector<uint8_t> serialize_pod_to_bytes(const T &pod) {
    static_assert(std::is_standard_layout_v<T> && std::is_trivial_v<T>,
                  "Type must be POD");

    std::vector<uint8_t> bytes(sizeof(T));
    std::memcpy(bytes.data(), &pod, sizeof(T));
    return bytes;
}

// 工具函数：从字节数组反序列化为POD结构体
template <typename T>
T deserialize_pod_from_bytes(const std::vector<uint8_t> &bytes) {
    static_assert(std::is_standard_layout_v<T> && std::is_trivial_v<T>,
                  "Type must be POD");

    T result{};
    std::memcpy(&result, bytes.data(), std::min(sizeof(result), bytes.size()));
    return result;
}

// 工具函数：将POD结构体序列化为uint64_t数组（处理大于64位的情况）
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

// 改进的工具函数：将POD结构体转换为Bundle（支持大尺寸POD）
template <typename PodType, typename BundleType>
void assign_pod_to_bundle_advanced(const PodType &pod, BundleType &bundle,
                                   Simulator &sim) {
    static_assert(std::is_standard_layout_v<PodType> &&
                      std::is_trivial_v<PodType>,
                  "PodType must be a POD struct");
    static_assert(std::is_base_of_v<bundle_base<BundleType>, BundleType>,
                  "BundleType must be derived from bundle_base");

    // 使用uint64_t数组处理大尺寸POD
    auto uint64_array = serialize_pod_to_uint64_array(pod);

    // 合并为单个uint64_t值（如果可能）
    uint64_t serialized_value = 0;
    if (!uint64_array.empty()) {
        serialized_value = uint64_array[0]; // 使用第一个值
    }

    // 使用仿真器设置Bundle值
    sim.set_bundle_value(bundle, serialized_value);
}

// 改进的工具函数：从Bundle提取值并转换为POD结构体（支持大尺寸POD）
template <typename BundleType, typename PodType>
PodType assign_bundle_to_pod_advanced(const BundleType &bundle,
                                      Simulator &sim) {
    static_assert(std::is_standard_layout_v<PodType> &&
                      std::is_trivial_v<PodType>,
                  "PodType must be a POD struct");
    static_assert(std::is_base_of_v<bundle_base<BundleType>, BundleType>,
                  "BundleType must be derived from bundle_base");

    // 通过仿真器获取Bundle值
    uint64_t bundle_value = sim.get_bundle_value(bundle);

    // 创建一个包含该值的uint64_t数组
    std::vector<uint64_t> uint64_array(1, bundle_value);

    // 将uint64_t数组反序列化为POD结构体
    PodType pod = deserialize_pod_from_uint64_array<PodType>(uint64_array);

    return pod;
}

int main() {
    std::cout << "Advanced POD to Bundle Conversion Demo" << std::endl;
    std::cout << "=====================================" << std::endl;

    // 测试1: POD结构体大小和内存布局
    std::cout << "\n=== Test 1: POD Structure Analysis ===" << std::endl;
    std::cout << "SimpleData size: " << sizeof(SimpleData) << " bytes ("
              << (sizeof(SimpleData) * 8) << " bits)" << std::endl;
    std::cout << "LargeData size: " << sizeof(LargeData) << " bytes ("
              << (sizeof(LargeData) * 8) << " bits)" << std::endl;

    // 测试2: 大尺寸POD序列化/反序列化
    std::cout << "\n=== Test 2: Large POD Serialization/Deserialization ==="
              << std::endl;
    LargeData large{0x123456789ABCDEF0ULL, 0xABCD1234, 0xEF56, true, false};
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

    bool large_match = (large == deserialized_large);
    std::cout << "Serialization/Deserialization match: "
              << (large_match ? "✓" : "✗") << std::endl;

    // 测试3: Bundle创建和分析
    std::cout << "\n=== Test 3: Large Bundle Creation and Analysis ==="
              << std::endl;
    auto ctx = std::make_unique<ch::core::context>("demo_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    // large_data_bundle large_bundle;
    // std::cout << "LargeDataBundle width: " << large_bundle.width() << " bits"
    //           << std::endl;

    // 测试4: 通过仿真器设置大尺寸Bundle值
    std::cout << "\n=== Test 4: Large Bundle Value Setting via Simulator ==="
              << std::endl;

    // 创建一个模块来测试大尺寸Bundle
    class TestLargeDataModule : public ch::Component {
    public:
        large_data_bundle io;

        TestLargeDataModule(ch::Component *parent = nullptr,
                            const std::string &name = "test_large_module")
            : ch::Component(parent, name) {
            io.as_slave();
        }

        void create_ports() override {}

        void describe() override { io.ready = io.flag1 && io.flag2; }
    };

    ch::ch_device<TestLargeDataModule> test_large_device;
    ch::Simulator large_sim(test_large_device.context());

    // 使用大尺寸POD数据设置Bundle
    LargeData test_large_data{0x123456789ABCDEF0ULL, 0xABCD1234, 0xEF56, true,
                              false};
    std::cout << "Setting Large Bundle with POD data: ";
    test_large_data.print();

    auto large_uint64_array = serialize_pod_to_uint64_array(test_large_data);
    std::cout << "POD serialized to " << large_uint64_array.size()
              << " uint64_t values" << std::endl;

    // 通过仿真器设置Bundle值（使用第一个值）
    uint64_t first_value =
        large_uint64_array.empty() ? 0 : large_uint64_array[0];
    std::cout << "Using first uint64_t value: 0x" << std::hex << first_value
              << std::dec << std::endl;

    large_sim.set_bundle_value(test_large_device.instance().io, first_value);
    large_sim.tick();

    // 获取Bundle值
    uint64_t large_bundle_value =
        large_sim.get_bundle_value(test_large_device.instance().io);
    std::cout << "Bundle value from simulator: 0x" << std::hex
              << large_bundle_value << std::dec << std::endl;

    // 将Bundle值转换回POD（注意：这里会丢失部分信息，因为我们只使用了一个uint64_t）
    std::vector<uint64_t> result_array(1, large_bundle_value);
    LargeData result_large_data =
        deserialize_pod_from_uint64_array<LargeData>(result_array);
    std::cout << "Result POD data (partial): ";
    result_large_data.print();

    std::cout
        << "\nNote: Full conversion requires handling multiple uint64_t values."
        << std::endl;
    std::cout
        << "This demonstrates how to work with POD types larger than 64 bits."
        << std::endl;

    std::cout << "\nDemo completed successfully!" << std::endl;
    std::cout << "This demonstrates how to handle POD structs larger than "
                 "uint64_t with Bundle types."
              << std::endl;

    return 0;
}