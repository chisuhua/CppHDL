// samples/large_pod_bundle_example.cpp
//
// 演示：用 `uint64_t` 数组承载**超过 64 位**的 POD 数据，并逐字段灌进 bundle。
//
// ⚠️ API 漂移记录（2026-10-22）：
// ① `CH_BUNDLE_FIELDS(Self, …)` 宏已被 **ADR-043** 移除
//    ⇒ 当前用 `CH_BUNDLE_FIELDS_T(…)`。
// ② `Simulator::set_bundle_field_value()` 的第二参**不再是 vector**，
//    而是**单个 uint64_t 值**（该值内部的位段由 `offset`/`width` 选出）
//    ⇒ 跨 uint64 边界的字段必须**由调用方自己**先从数组里切出来。
// ③ bundle 字段的值在 Simulator 的 data_map_ 槽位里 ⇒ 读回 MUST 用
//    `get_bundle_field_value()`，MUST NOT 把字段当宿主值直接 static_cast。
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

    // ⚠️ ADR-043 之后是 `CH_BUNDLE_FIELDS_T(...)`（不再传 `Self`）
    CH_BUNDLE_FIELDS_T(part1, part2, part3, part4, flag, ready)

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

// 创建一个模块来测试大尺寸Bundle
//
// ⚠️ **为什么驱动信号是独立的 `ch_uint` 成员而不是 bundle 字段**：
// ① `Simulator::set_bundle_field_value()` 是 **private**（只有
//    `set_bundle_value()` 这一打包式入口是 public，而它在总宽 > 64 位时
//    **显式拒绝** ⇒ 122 位的 bundle **没有**公开的逐字段写入口。
// ② `Simulator::set_input_value()` 只有 `ch_in<T>` 与 `ch_uint<N>` 两个
//    重载 ⇒ TB 侧用 `ch_uint` 成员承载输入，再在 `describe()` 里接到
//    bundle 字段上（节点重绑定 ⇒ bundle 字段直接读该驱动节点的值）。
class TestVeryLargeDataModule : public ch::Component {
public:
    // TB 驱动的输入
    ch_uint<64> in_part1 = 0_d;
    ch_uint<32> in_part2 = 0_d;
    ch_uint<16> in_part3 = 0_d;
    ch_uint<8> in_part4 = 0_d;
    ch_uint<1> in_flag = 0_d;

    very_large_data_bundle io;

    TestVeryLargeDataModule(
        ch::Component *parent = nullptr,
        const std::string &name = "test_very_large_module")
        : ch::Component(parent, name) {
        io.as_slave_direction();
    }

    void create_ports() override {}

    void describe() override {
        io.part1 = in_part1;
        io.part2 = in_part2;
        io.part3 = in_part3;
        io.part4 = in_part4;
        io.flag = in_flag;   // ch_bool ← ch_uint<1>（隐式转换）
        io.ready = io.flag;
    }
};

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
              << (large_match ? "yes" : "no") << std::endl;

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

    // 使用辅助函数把 POD 的位段灌进 bundle 的各个字段
    //
    // ⚠️ **POD 是按字节 memcpy 进 uint64_t 数组的（不是位紧凑打包）** ⇒
    //    字段的位偏移必须按**字节布局**算，不能按「121 位连续流」算：
    //      part1 → arr[0] bit 0..63
    //      part2 → arr[1] bit 0..31
    //      part3 → arr[1] bit 32..47
    //      part4 → arr[1] bit 48..55
    //      flag  → arr[1] bit 56
    //    （旧 sample 用的 0/64/96/112/120 是「连续位流」偏移 ⇒ 对
    //      memcpy 布局是错的 ⇒ part2 起全部读回 0。）
    auto extract = [](const std::vector<uint64_t> &arr, unsigned word,
                      unsigned offset, unsigned width) -> uint64_t {
        // ⚠️ width == 64 时 `1ULL << 64` 是 UB ⇒ MUST 特判
        if (width >= 64) {
            return (offset == 0) ? arr[word] : 0;
        }
        return (arr[word] >> offset) & ((1ULL << width) - 1);
    };
    very_large_sim.set_input_value(test_very_large_device.instance().in_part1,
                                   extract(very_large_uint64_array, 0, 0, 64));
    very_large_sim.set_input_value(test_very_large_device.instance().in_part2,
                                   extract(very_large_uint64_array, 1, 0, 32));
    very_large_sim.set_input_value(test_very_large_device.instance().in_part3,
                                   extract(very_large_uint64_array, 1, 32, 16));
    very_large_sim.set_input_value(test_very_large_device.instance().in_part4,
                                   extract(very_large_uint64_array, 1, 48, 8));
    very_large_sim.set_input_value(test_very_large_device.instance().in_flag,
                                   extract(very_large_uint64_array, 1, 56, 1));

    very_large_sim.tick();

    // 获取Bundle值
    // ⚠️ MUST 用 `get_bundle_field_value()`（槽位读）—— 直接把字段
    //    static_cast 成宿主值拿到的是**节点**，不是仿真后的值。
    std::vector<uint64_t> result_array(2, 0); // 需要2个uint64_t存储121位数据
    // ⚠️ 写回时 MUST 用**与写入完全相同**的字节布局（见上面的 extract 说明），
    //    否则 round-trip 断言会因布局不一致而假失败。
    result_array[0] |=
        very_large_sim.get_bundle_field_value(
            test_very_large_device.instance().io.part1);
    result_array[1] |=
        very_large_sim.get_bundle_field_value(
            test_very_large_device.instance().io.part2);
    result_array[1] |=
        (very_large_sim.get_bundle_field_value(
             test_very_large_device.instance().io.part3)
         << 32);
    result_array[1] |=
        (very_large_sim.get_bundle_field_value(
             test_very_large_device.instance().io.part4)
         << 48);
    result_array[1] |=
        (very_large_sim.get_bundle_field_value(
             test_very_large_device.instance().io.flag)
         << 56);

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
    std::cout << "Full conversion match: " << (full_match ? "yes" : "no")
              << std::endl;

    // 自检：逐字段灌进去的值必须能完整还原成原 POD
    if (!large_match || !full_match) {
        std::cerr << "ERROR: POD <-> bundle round-trip mismatch" << std::endl;
        return 1;
    }

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
