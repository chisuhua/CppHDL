// samples/advanced_pod_bundle_serialization.cpp
//
// 演示：把一个**超过 uint64_t 宽度**的 POD 结构体与等价的 bundle 互转。
//
// ⚠️ API 漂移记录（2026-10-22）：
// ① `CH_BUNDLE_FIELDS(Self, a, b, …)` 宏已被 **ADR-043** 移除
//    ⇒ 当前用 `CH_BUNDLE_FIELDS_T(a, b, …)`（不带 `Self` 参数）。
// ② 十六进制字面量带 `_d` 后缀不被 `lit_dec` 接受
//    ⇒ 十六进制一律用 `_h`（`lit_hex::is_escape` 认 `'x'`）。
// ③ `bundle.to_bits()` 不是 bundle 的公开 API ⇒ 改用 `bundle.width()`
//    与逐字段的 `get_bundle_field_value()`。
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

    // 测试3: Bundle字段数量和宽度
    std::cout << "\n=== Test 3: Bundle Field Count and Width ===" << std::endl;

    // ⚠️ 建 bundle 需要**活跃 context**（字段节点是在构造时建立的）
    ch::core::context ctx("advanced_pod_bundle_serialization");
    ch::core::ctx_swap swap(&ctx);

    very_large_data_bundle bundle;
    bundle.part1 = ch_uint<64>(0x1111111111111111_h);
    bundle.part2 = ch_uint<32>(0x22222222_h);
    bundle.part3 = ch_uint<16>(0x3333_h);
    bundle.part4 = ch_uint<8>(0x44_h);
    bundle.flag = ch_bool(true);
    bundle.ready = ch_bool(false);

    std::cout << "Field count: " << bundle_field_count_v<very_large_data_bundle>
              << std::endl;
    std::cout << "Bundle width: " << bundle.width() << " bits" << std::endl;

    // 测试4: Bundle字段访问
    std::cout << "\n=== Test 4: Bundle Field Access ===" << std::endl;
    auto fields = bundle.__bundle_fields();
    std::cout << "Number of fields in tuple: " << std::tuple_size_v<decltype(fields)> << std::endl;

    // 测试5: Bundle方向设置
    std::cout << "\n=== Test 5: Bundle Direction Setting ===" << std::endl;
    bundle.as_master();
    std::cout << "Bundle set as master, role is now: " << static_cast<int>(bundle.get_role()) << std::endl;

    bundle.as_slave();
    std::cout << "Bundle set as slave, role is now: " << static_cast<int>(bundle.get_role()) << std::endl;

    // 测试6: 逐字段读回（经 Simulator 的槽位 API）
    // ⚠️ bundle 字段的值存在 Simulator 的 data_map_ 槽位里 ⇒ MUST 经
    //    `get_bundle_field_value()` 读，**MUST NOT** 直接把字段当宿主值用。
    std::cout << "\n=== Test 6: Per-field Readback via Simulator ===" << std::endl;
    ch::Simulator sim(&ctx);
    sim.tick();

    std::cout << "  part1 = 0x" << std::hex
              << sim.get_bundle_field_value(bundle.part1) << std::dec << std::endl;
    std::cout << "  part2 = 0x" << std::hex
              << sim.get_bundle_field_value(bundle.part2) << std::dec << std::endl;
    std::cout << "  part3 = 0x" << std::hex
              << sim.get_bundle_field_value(bundle.part3) << std::dec << std::endl;
    std::cout << "  part4 = 0x" << std::hex
              << sim.get_bundle_field_value(bundle.part4) << std::dec << std::endl;
    std::cout << "  flag  = " << sim.get_bundle_field_value(bundle.flag)
              << std::endl;

    // 自检：写入 bundle 的字段值必须能被逐字段读回
    if (sim.get_bundle_field_value(bundle.part1) != 0x1111111111111111ULL ||
        sim.get_bundle_field_value(bundle.part2) != 0x22222222ULL ||
        sim.get_bundle_field_value(bundle.part3) != 0x3333U ||
        sim.get_bundle_field_value(bundle.part4) != 0x44U ||
        sim.get_bundle_field_value(bundle.flag) != 1) {
        std::cerr << "ERROR: per-field readback does not match what was written"
                  << std::endl;
        return 1;
    }

    std::cout << "\nDemo completed successfully!" << std::endl;

    return 0;
}
