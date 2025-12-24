// test/ch_mem_test.cpp
#include "ast/mem_port_impl.h"
#include "ast/memimpl.h"
#include "catch_amalgamated.hpp"
#include "core/bool.h"
#include "core/context.h"
#include "core/mem.h"
#include "core/reg.h"
#include "core/traits.h"
#include "core/uint.h"
#include <source_location>
#include <type_traits>
#include <vector>

using namespace ch::core;

// 测试辅助函数
template <unsigned N> using test_uint = ch_uint<N>;

// ---------- Width Trait Tests ----------

TEST_CASE("ch_width_impl: basic ch_mem types", "[mem][width][basic]") {
    STATIC_REQUIRE(ch_width_v<ch_mem<ch_uint<8>, 256>> == 8);
    STATIC_REQUIRE(ch_width_v<ch_mem<ch_uint<16>, 1024>> == 16);
    STATIC_REQUIRE(ch_width_v<ch_mem<ch_uint<32>, 65536>> == 32);

    // 测试const版本
    STATIC_REQUIRE(ch_width_v<const ch_mem<ch_uint<8>, 256>> == 8);
}

// ---------- Basic Construction Tests ----------

TEST_CASE("ch_mem: basic construction", "[mem][construction][basic]") {
    context ctx("test_ctx");
    ctx_swap swap(&ctx);

    SECTION("Default construction") {
        ch_mem<ch_uint<32>, 1024> mem("test_mem");
        REQUIRE(mem.impl() != nullptr);
        REQUIRE(mem.impl()->addr_width() == 10); // log2(1024) = 10
        REQUIRE(mem.impl()->data_width() == 32);
        REQUIRE(mem.impl()->depth() == 1024);
        REQUIRE(mem.impl()->is_rom() == false);
    }

    SECTION("Construction with vector init data") {
        std::vector<uint32_t> init_data = {0x12345678, 0xABCDEF00, 0xDEADBEEF};
        ch_mem<ch_uint<32>, 1024> mem(init_data, "test_mem");
        REQUIRE(mem.impl() != nullptr);
        REQUIRE(mem.impl()->init_data().size() == 3);
    }
    /*
     SECTION("Construction with array init data") {
         std::array<uint32_t, 3> init_data = {0x11111111, 0x22222222,
     0x33333333}; ch_mem<ch_uint<16>, 64> mem(init_data, "test_mem");
         REQUIRE(mem.impl() != nullptr);
         REQUIRE(mem.impl()->init_data().size() == 3);
     }
     */
}

// ---------- ROM Construction Tests ----------

TEST_CASE("ch_mem: ROM construction", "[mem][construction][rom]") {
    context ctx("test_ctx");
    ctx_swap swap(&ctx);

    SECTION("ROM from vector") {
        std::vector<uint32_t> rom_data = {0x12345678, 0xABCDEF00, 0xDEADBEEF};
        auto rom = ch_mem<ch_uint<32>, 1024>::make_rom(rom_data, "test_rom");
        REQUIRE(rom.impl() != nullptr);
        REQUIRE(rom.impl()->is_rom() == true);
        REQUIRE(rom.impl()->init_data().size() == 3);
    }

    SECTION("ROM from array") {
        std::array<uint16_t, 4> rom_data = {0x1111, 0x2222, 0x3333, 0x4444};
        auto rom = ch_mem<ch_uint<16>, 16>::make_rom(rom_data, "test_rom");
        REQUIRE(rom.impl() != nullptr);
        REQUIRE(rom.impl()->is_rom() == true);
    }
}

// ---------- Port Creation Tests ----------

// test/test_utils.h
class test_context : public context {
public:
    clockimpl *default_clock;
    resetimpl *default_reset;

    test_context(const std::string &name = "test_ctx") : context(name) {
        // 创建默认的测试时钟和复位
        default_clock = create_clock(sdata_type(0, 1), true, false, "test_clk");
        default_reset = create_reset(
            sdata_type(1, 1), resetimpl::reset_type::async_low, "test_rst");

        // 设置为当前时钟和复位
        set_current_clock(default_clock);
        set_current_reset(default_reset);
    }
};

TEST_CASE("ch_mem: port creation", "[mem][ports][basic]") {
    test_context ctx("test_ctx");
    ctx_swap swap(&ctx);

    ch_mem<ch_uint<32>, 256> mem("test_mem");
    SECTION("Async read port creation") {
        ch_reg<ch_uint<8>> addr(0); // 8位地址足够访问256个位置
        auto read_port = mem.aread(addr, "async_read");

        REQUIRE(read_port.impl() != nullptr);
        REQUIRE(read_port.port_type() == mem_port_type::async_read);
        REQUIRE(read_port.has_addr() == true);
        REQUIRE(read_port.has_cd() == false); // 异步读端口没有时钟
    }

    SECTION("Sync read port creation") {
        ch_reg<ch_uint<8>> addr(0);
        ch_reg<ch_bool> enable(true);
        auto read_port = mem.sread(addr, enable, "sync_read");

        REQUIRE(read_port.impl() != nullptr);
        REQUIRE(read_port.port_type() == mem_port_type::sync_read);
        REQUIRE(read_port.has_addr() == true);
        REQUIRE(read_port.has_cd() == true); // 同步读端口有时钟
        REQUIRE(read_port.has_enable() == true);
    }

    SECTION("Write port creation") {
        ch_reg<ch_uint<8>> addr(0);
        ch_reg<ch_uint<32>> data(0x12345678);
        ch_reg<ch_bool> enable(true);
        auto write_port = mem.write(addr, data, enable, "write_port");

        REQUIRE(write_port.impl() != nullptr);
        REQUIRE(write_port.port_type() == mem_port_type::write);
        REQUIRE(write_port.has_addr() == true);
        REQUIRE(write_port.has_cd() == true);
        REQUIRE(write_port.wdata() != nullptr);
    }
}

// ---------- Memory Node Tests ----------

TEST_CASE("memimpl: basic functionality", "[mem][impl][basic]") {
    context ctx("test_ctx");
    ctx_swap swap(&ctx);

    SECTION("Memory node creation") {
        std::vector<sdata_type> init_data;
        init_data.emplace_back(0x12345678, 32);
        init_data.emplace_back(0xABCDEF00, 32);

        auto *mem_node = ctx.create_memory(8, 32, 256, 1, true, false,
                                           init_data, "test_mem");
        REQUIRE(mem_node != nullptr);
        REQUIRE(mem_node->addr_width() == 8);
        REQUIRE(mem_node->data_width() == 32);
        REQUIRE(mem_node->depth() == 256);
        REQUIRE(mem_node->has_byte_enable() == true);
        REQUIRE(mem_node->is_rom() == false);
        REQUIRE(mem_node->init_data().size() == 2);
    }

    SECTION("Memory node with no init data") {
        std::vector<sdata_type> empty_init;
        auto *mem_node = ctx.create_memory(10, 16, 1024, 1, false, true,
                                           empty_init, "test_rom");
        REQUIRE(mem_node != nullptr);
        REQUIRE(mem_node->is_rom() == true);
        REQUIRE(mem_node->init_data().empty() == true);
    }
}

// ---------- Port Node Tests ----------

TEST_CASE("mem_port_impl: node creation", "[mem][port][impl]") {
    context ctx("test_ctx");
    ctx_swap swap(&ctx);

    std::vector<sdata_type> init_data;
    auto *mem_node =
        ctx.create_memory(8, 32, 256, 1, true, false, init_data, "test_mem");
    REQUIRE(mem_node != nullptr);

    SECTION("Read port node creation") {
        // auto *addr_node = ctx.create_input(8, "addr");
        // auto *enable_node = ctx.create_literal(sdata_type(1, 1), "enable");
        // // auto* data_output = ctx.create_output(32, "data_out");
        // auto *data_output = ctx.create_node<proxyimpl>(
        //     32, "_data_proxy", std::source_location::current());

        // 创建一个内存对象来测试aread函数
        ch_mem<ch_uint<32>, 256> mem_obj("test_mem");
        ch_reg<ch_uint<8>> addr(0);

        // 使用aread函数创建读端口
        auto read_port = mem_obj.aread(addr, "async_read_test");

        REQUIRE(read_port.impl() != nullptr);
        REQUIRE(read_port.parent() == mem_obj.impl());
        REQUIRE(read_port.port_type() == mem_port_type::async_read);
        REQUIRE(read_port.has_addr() == true);
    }

    SECTION("Write port node creation") {
        auto *addr_node = ctx.create_input(8, "addr");
        auto *data_node = ctx.create_input(32, "wdata");
        auto *enable_node = ctx.create_literal(sdata_type(1, 1), "enable");

        auto *write_port =
            ctx.create_mem_write_port(mem_node, 1, 32, nullptr, addr_node,
                                      data_node, enable_node, "write_port");

        REQUIRE(write_port != nullptr);
        REQUIRE(write_port->parent() == mem_node);
        REQUIRE(write_port->port_id() == 1);
        REQUIRE(write_port->port_type() == mem_port_type::write);
        REQUIRE(write_port->wdata() == data_node);
    }
}

// ---------- Type Safety Tests ----------

TEST_CASE("ch_mem: type safety", "[mem][types][safety]") {
    context ctx("test_ctx");
    ctx_swap swap(&ctx);

    SECTION("Different data types") {
        ch_mem<ch_uint<8>, 256> mem8("mem8");
        ch_mem<ch_uint<16>, 256> mem16("mem16");
        ch_mem<ch_uint<32>, 256> mem32("mem32");

        REQUIRE(ch_width_v<decltype(mem8)> == 8);
        REQUIRE(ch_width_v<decltype(mem16)> == 16);
        REQUIRE(ch_width_v<decltype(mem32)> == 32);
    }

    SECTION("Different depths") {
        ch_mem<ch_uint<32>, 16> mem16("mem16");
        ch_mem<ch_uint<32>, 256> mem256("mem256");
        ch_mem<ch_uint<32>, 65536> mem64k("mem64k");

        REQUIRE(mem16.impl()->depth() == 16);
        REQUIRE(mem256.impl()->depth() == 256);
        REQUIRE(mem64k.impl()->depth() == 65536);
    }
}

// ---------- Port Connection Tests ----------

TEST_CASE("ch_mem: port connections", "[mem][ports][connections]") {
    context ctx("test_ctx");
    ctx_swap swap(&ctx);

    ch_mem<ch_uint<32>, 256> mem("test_mem");

    SECTION("Multiple read ports") {
        ch_reg<ch_uint<8>> addr(0);

        auto port1 = mem.aread(addr, "read1");
        auto port2 = mem.aread(addr, "read2");
        auto port3 = mem.sread(addr, ch_bool(true), "read3");

        REQUIRE(port1.impl() != nullptr);
        REQUIRE(port2.impl() != nullptr);
        REQUIRE(port3.impl() != nullptr);

        // 检查内存节点是否正确注册了端口
        REQUIRE(mem.impl()->read_ports().size() >= 3);
    }

    SECTION("Multiple write ports") {
        ch_reg<ch_uint<8>> addr1(0), addr2(1_d);
        ch_reg<ch_uint<32>> data1(0x11111111_h), data2(0x22222222_h);
        ch_reg<ch_bool> enable(true);

        auto write1 = mem.write(addr1, data1, enable, "write1");
        auto write2 = mem.write(addr2, data2, enable, "write2");

        REQUIRE(write1.impl() != nullptr);
        REQUIRE(write2.impl() != nullptr);

        // 检查内存节点是否正确注册了端口
        REQUIRE(mem.impl()->write_ports().size() >= 2);
    }
}

// ---------- Initialization Data Tests ----------

TEST_CASE("ch_mem: initialization data handling", "[mem][init][data]") {
    context ctx("test_ctx");
    ctx_swap swap(&ctx);

    SECTION("Vector initialization") {
        std::vector<uint16_t> init_values = {0x1234, 0x5678, 0x9ABC, 0xDEF0};
        ch_mem<ch_uint<16>, 1024> mem(init_values, "init_mem");

        const auto &init_data = mem.impl()->init_data();
        REQUIRE(init_data.size() == 4);

        for (size_t i = 0; i < init_values.size(); ++i) {
            REQUIRE(init_data[i].bitwidth() == 16);
            REQUIRE(static_cast<uint64_t>(init_data[i]) == init_values[i]);
        }
    }

    SECTION("Array initialization") {
        std::array<uint32_t, 3> init_values = {0x11111111, 0x22222222,
                                               0x33333333};
        ch_mem<ch_uint<32>, 64> mem(init_values, "init_mem");

        const auto &init_data = mem.impl()->init_data();
        REQUIRE(init_data.size() == 3);

        for (size_t i = 0; i < init_values.size(); ++i) {
            REQUIRE(init_data[i].bitwidth() == 32);
            REQUIRE(static_cast<uint64_t>(init_data[i]) == init_values[i]);
        }
    }

    SECTION("Empty initialization") {
        ch_mem<ch_uint<32>, 1024> mem("empty_mem");
        const auto &init_data = mem.impl()->init_data();
        REQUIRE(init_data.empty() == true);
    }
}

// ---------- Error Handling Tests ----------

TEST_CASE("ch_mem: error handling", "[mem][error][handling]") {
    context ctx("test_ctx");
    ctx_swap swap(&ctx);

    SECTION("Write to ROM should compile but not be callable") {
        std::vector<uint32_t> rom_data = {0x12345678, 0xABCDEF00};
        auto rom = ch_mem<ch_uint<32>, 1024>::make_rom(rom_data, "test_rom");

        // ROM construction should work
        REQUIRE(rom.impl() != nullptr);
        REQUIRE(rom.impl()->is_rom() == true);

        // Note: Write operations are deleted at compile time for ROM
        // This test just verifies ROM construction works
    }

    SECTION("Port creation with different address widths") {
        ch_mem<ch_uint<32>, 65536> large_mem("large_mem"); // 需要16位地址
        ch_mem<ch_uint<32>, 256> small_mem("small_mem");   // 需要8位地址

        ch_reg<ch_uint<16>> large_addr(0);
        ch_reg<ch_uint<8>> small_addr(0);

        // 这些应该能正常编译和工作
        auto large_port = large_mem.aread(large_addr);
        auto small_port = small_mem.aread(small_addr);

        REQUIRE(large_port.impl() != nullptr);
        REQUIRE(small_port.impl() != nullptr);
    }
}

// test/ch_mem_test.cpp (额外测试)
TEST_CASE("ch_mem: advanced port scenarios", "[mem][ports][advanced]") {
    test_context ctx("test_ctx");
    ctx_swap swap(&ctx);

    ch_mem<ch_uint<32>, 64> mem("test_mem");

    SECTION("Multiple ports on same memory") {
        ch_reg<ch_uint<6>> addr1(0);
        ch_reg<ch_uint<6>> addr2(1);
        ch_reg<ch_uint<32>> data(0x12345678);
        ch_reg<ch_bool> enable(true);

        // 创建多个读端口
        auto read1 = mem.aread(addr1, "read1");
        auto read2 = mem.sread(addr2, enable, "read2");

        // 创建写端口
        auto write1 = mem.write(addr1, data, enable, "write1");

        REQUIRE(read1.impl() != nullptr);
        REQUIRE(read2.impl() != nullptr);
        REQUIRE(write1.impl() != nullptr);

        // 检查内存节点是否正确注册了端口
        REQUIRE(mem.impl()->read_ports().size() >= 2);
        REQUIRE(mem.impl()->write_ports().size() >= 1);
    }

    SECTION("Port with literal enable") {
        ch_reg<ch_uint<6>> addr(0);

        // 使用字面值作为使能信号
        auto read_port = mem.sread(addr, ch_bool(true), "sync_read_literal");

        REQUIRE(read_port.impl() != nullptr);
        REQUIRE(read_port.port_type() == mem_port_type::sync_read);
        // 当使能为常量1时，可能被优化掉
        // REQUIRE(read_port.impl()->has_enable() == false); // 可选检查
    }

    SECTION("Port with different address widths") {
        // 测试自动推导的地址宽度
        STATIC_REQUIRE(ch_mem<ch_uint<32>, 64>::addr_width == 6);
        STATIC_REQUIRE(ch_mem<ch_uint<32>, 256>::addr_width == 8);
        STATIC_REQUIRE(ch_mem<ch_uint<32>, 65536>::addr_width == 16);
    }
}
