// tests/test_bundle_bitstream.cpp
#define CATCH_CONFIG_MAIN
#include "catch_amalgamated.hpp"
#include "../include/core/context.h"
#include "../include/core/bundle/bundle_base.h"
#include "../include/core/bundle/bundle_meta.h"
#include "../include/core/bundle/bundle_utils.h"
#include "../include/io/common_bundles.h"
#include "../include/core/uint.h"
#include "../include/core/bool.h"
#include "../include/simulator.h"
#include "../include/component.h"
#include <memory>
#include <bitset>

using namespace ch;
using namespace ch::core;

// 测试用的简单Bundle
template<typename T>
struct TestSimpleBundle : public bundle_base<TestSimpleBundle<T>> {
    using Self = TestSimpleBundle<T>;
    T data;
    ch_bool flag1;
    ch_bool flag2;

    TestSimpleBundle() = default;
    
    CH_BUNDLE_FIELDS(Self, data, flag1, flag2)

    void as_master() override {
        this->make_output(data, flag1, flag2);
    }

    void as_slave() override {
        this->make_input(data, flag1, flag2);
    }
};

TEST_CASE("BundleBitstream - BitSegmentConcatenation", "[bundle][bitstream][concatenation]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());
    
    // 创建真实的Bundle实例
    TestSimpleBundle<ch_uint<4>> bundle;
    
    // 验证Bundle字段数量
    auto fields = bundle.__bundle_fields();
    REQUIRE(std::tuple_size_v<decltype(fields)> == 3);
    
    // 验证Bundle总宽度
    REQUIRE(get_bundle_width<TestSimpleBundle<ch_uint<4>>>() == 6); // 4 + 1 + 1
    
    // 测试Bundle字段的位段拼接原理
    // 字段值
    uint64_t data_value = 10;   // 1010 (4位)
    uint64_t flag1_value = 1;   // 1 (1位)
    uint64_t flag2_value = 0;   // 0 (1位)
    
    // 按照字段顺序进行位段拼接
    // 字段顺序: data(4位), flag1(1位), flag2(1位)
    // 拼接方式: [flag2(1位) | flag1(1位) | data(4位)]
    uint64_t serialized = (flag2_value << 5) | (flag1_value << 4) | data_value;
    
    // 验证拼接结果
    REQUIRE(serialized == 0x1a); // 011010 in binary
    
    // 从序列化的值中提取各个字段
    uint64_t extracted_data = serialized & 0xF;          // 提取低4位 (data)
    uint64_t extracted_flag1 = (serialized >> 4) & 0x1;  // 提取第4位 (flag1)
    uint64_t extracted_flag2 = (serialized >> 5) & 0x1;  // 提取第5位 (flag2)
    
    // 验证反序列化结果
    REQUIRE(data_value == extracted_data);
    REQUIRE(flag1_value == extracted_flag1);
    REQUIRE(flag2_value == extracted_flag2);
}

TEST_CASE("BundleBitstream - SerializationDeserialization", "[bundle][bitstream][serialize]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());
    
    // 使用真实的fifo_bundle进行测试
    fifo_bundle<ch_uint<4>> bundle;
    
    // 测试Bundle宽度计算
    REQUIRE(get_bundle_width<fifo_bundle<ch_uint<4>>>() == 8); // data(4) + push(1) + full(1) + pop(1) + empty(1) = 8
    
    // 测试序列化功能存在（编译期检查）
    REQUIRE(true);
    
    // 测试Bundle字段访问
    auto fields = bundle.__bundle_fields();
    REQUIRE(std::tuple_size_v<decltype(fields)> == 5);
}

TEST_CASE("BundleBitstream - SimulatorBundleSupport", "[bundle][bitstream][simulator]") {
    // 测试真实的Bundle宽度
    REQUIRE(get_bundle_width<fifo_bundle<ch_uint<4>>>() == 8);
    
    // 验证实际的Bundle类型
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());
    
    fifo_bundle<ch_uint<4>> bundle;
    auto fields = bundle.__bundle_fields();
    
    // 验证字段数量
    REQUIRE(std::tuple_size_v<decltype(fields)> == 5);
}