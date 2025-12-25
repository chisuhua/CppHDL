// tests/test_fifo.cpp
#define CATCH_CONFIG_MAIN
#include "catch_amalgamated.hpp"
#include "component.h"
#include "core/bool.h"
#include "core/context.h"
#include "core/mem.h"
#include "core/reg.h"
#include "core/uint.h"
#include "device.h"
#include "module.h"
#include "simulator.h"
#include <memory>

using namespace ch;
using namespace ch::core;

// 辅助函数
constexpr bool ispow2(unsigned n) { return n > 0 && (n & (n - 1)) == 0; }

constexpr unsigned log2ceil(unsigned n) {
    if (n <= 1)
        return 0;
    unsigned result = 0;
    while (n > 1) {
        result++;
        n >>= 1;
    }
    return result;
}

template <typename T, unsigned N> class FiFo : public ch::Component {
public:
    static_assert(ispow2(N), "FIFO size must be power of 2");
    static constexpr unsigned addr_width = log2ceil(N);

    __io(ch_in<T> din; ch_in<ch_bool> push; ch_in<ch_bool> pop; ch_out<T> dout;
         ch_out<ch_bool> empty; ch_out<ch_bool> full;)

        FiFo(ch::Component *parent = nullptr, const std::string &name = "fifo")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        // 创建读写指针寄存器（addr_width + 1 位用于满/空检测）
        ch_reg<ch_uint<addr_width + 1>> rd_ptr(0, "rd_ptr");
        ch_reg<ch_uint<addr_width + 1>> wr_ptr(0, "wr_ptr");

        // 提取地址位（低 addr_width 位）
        ch_uint<addr_width> rd_a, wr_a;

        if constexpr (addr_width == 1) {
            // 特殊处理1位地址的情况
            auto rd_bit = bit_select<0>(rd_ptr);
            auto wr_bit = bit_select<0>(wr_ptr);
            rd_a = zext<decltype(rd_bit), addr_width>(rd_bit);
            wr_a = zext<decltype(wr_bit), addr_width>(wr_bit);
        } else {
            // 一般情况使用bits提取
            rd_a = bits<decltype(rd_ptr), addr_width - 1, 0>(rd_ptr);
            wr_a = bits<decltype(wr_ptr), addr_width - 1, 0>(wr_ptr);
        }

        // 控制信号
        auto reading = io().pop;
        auto writing = io().push;

        // 指针更新逻辑
        rd_ptr->next = select(reading, rd_ptr + 1, rd_ptr);
        wr_ptr->next = select(writing, wr_ptr + 1, wr_ptr);

        // 创建内存
        ch_mem<T, N> mem("fifo_mem");

        // 写操作
        mem.write(wr_a, io().din, writing);

        // 读操作（异步读取）
        auto data_out = mem.aread(rd_a);
        io().dout = data_out;

        // 状态信号 - 使用更标准的FIFO空/满检测
        // 空状态：读指针等于写指针
        io().empty = (rd_ptr == wr_ptr);
        // 满状态：写指针+1等于读指针
        auto wr_plus_one = wr_ptr + 1;
        io().full = (wr_plus_one == rd_ptr);
    }
};

TEST_CASE("FIFO - Basic Memory Functionality", "[fifo][memory]") {
    auto ctx = std::make_unique<context>("fifo_mem_test");
    ctx_swap ctx_guard(ctx.get());

    // Test basic memory creation and functionality
    ch_mem<ch_uint<8>, 4> memory("test_mem");
    REQUIRE(memory.impl() != nullptr);
    REQUIRE(memory.impl()->depth() == 4);
    REQUIRE(memory.impl()->data_width() == 8);

    // Test register creation
    ch_reg<ch_uint<8>> data_reg(0_d);
    REQUIRE(data_reg.impl() != nullptr);

    ch_reg<ch_uint<2>> addr_reg(0_d);
    REQUIRE(addr_reg.impl() != nullptr);

    ch_reg<ch_bool> enable_reg(0_b);
    REQUIRE(enable_reg.impl() != nullptr);
}

TEST_CASE("FIFO - Pointer Logic", "[fifo][pointers]") {
    auto ctx = std::make_unique<context>("fifo_ptr_test");
    ctx_swap ctx_guard(ctx.get());

    // Test pointer register creation and operations
    ch_reg<ch_uint<2>> rd_ptr(0_d, "rd_ptr");
    ch_reg<ch_uint<2>> wr_ptr(0_d, "wr_ptr");

    REQUIRE(rd_ptr.impl() != nullptr);
    REQUIRE(wr_ptr.impl() != nullptr);

    // Test basic operations
    auto rd_plus_one = rd_ptr + 1_d;
    auto wr_plus_one = wr_ptr + 1_d;

    REQUIRE(rd_plus_one.impl() != nullptr);
    REQUIRE(wr_plus_one.impl() != nullptr);

    // Test comparison operations
    auto empty_cond = (rd_ptr == wr_ptr);
    auto full_cond = (wr_plus_one == rd_ptr);

    REQUIRE(empty_cond.impl() != nullptr);
    REQUIRE(full_cond.impl() != nullptr);
}

TEST_CASE("FIFO - Component Interface", "[fifo][interface]") {
    // Test that we can declare the FIFO component type
    REQUIRE(std::is_class_v<FiFo<ch_uint<2>, 2>>);

    // Test template constraints
    REQUIRE(ispow2(2) == true);
    REQUIRE(ispow2(4) == true);
    REQUIRE(ispow2(3) == false);

    // Test address width calculation
    REQUIRE(log2ceil(1) == 0);
    REQUIRE(log2ceil(2) == 1);
    REQUIRE(log2ceil(4) == 2);

    // Test that the FIFO has the correct IO structure
    // We can't instantiate it without a parent component, but we can check the
    // template
    using fifo_type = FiFo<ch_uint<4>, 8>;
    constexpr unsigned expected_addr_width = 3; // log2(8)
    STATIC_REQUIRE(fifo_type::addr_width == expected_addr_width);
}

TEST_CASE("FIFO - Memory Operations Test", "[fifo][mem-op]") {
    context ctx("mem_op_test");
    ctx_swap swap(&ctx);

    // Test memory operations in isolation
    ch_mem<ch_uint<2>, 4> memory("test_mem");

    // Create registers for testing
    ch_reg<ch_uint<2>> addr(0_d);
    ch_reg<ch_uint<2>> data(0_d);
    ch_reg<ch_bool> enable(0_b);

    // Test that we can create read and write ports
    auto read_result = memory.aread(addr);
    memory.write(addr, data, enable);

    REQUIRE(read_result.impl() != nullptr);
}

// Simple component for testing register behavior
class SimpleRegister : public ch::Component {
public:
    __io(ch_out<ch_uint<4>> out; ch_in<ch_uint<4>> in;)

        SimpleRegister(ch::Component *parent = nullptr,
                       const std::string &name = "simple_reg")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_reg<ch_uint<4>> reg(0);
        reg->next = io().in;
        io().out = reg;
    }
};

TEST_CASE("Register - Basic Functionality", "[reg][basic]") {
    context ctx("reg_test");
    ctx_swap swap(&ctx);

    // Test register creation
    ch_reg<ch_uint<4>> reg(0_d);
    REQUIRE(reg.impl() != nullptr);

    // Test register operations
    ch_uint<4> value(5_d);
    reg->next = value;
    // next_assignment_proxy doesn't have impl() method, so we skip this check
}

TEST_CASE("Register - Component Interface", "[reg][interface]") {
    // Test that we can declare register-based components
    REQUIRE(std::is_class_v<SimpleRegister>);
}
