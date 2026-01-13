#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "codegen_dag.h"
#include "component.h"
#include "core/bool.h"
#include "core/context.h"
#include "core/io.h"
#include "core/mem.h"
#include "core/reg.h"
#include "core/uint.h"
#include "simulator.h"
#include <cstdint>

using namespace ch::core;

TEST_CASE("ch_mem sread port connection to ch_uint",
          "[mem][connection][sread]") {
    class MemSReadConnectionModule : public Component {
    public:
        __io(ch_in<ch_uint<4>> addr_in;   // 4位地址输入
             ch_in<ch_bool> enable_in;    // 使能信号输入
             ch_out<ch_uint<8>> data_out; // 8位数据输出
             )

            MemSReadConnectionModule(Component *parent = nullptr,
                                     const std::string &name = "mem_sread_conn")
            : Component(parent, name) {}

        void create_ports() override { new (this->io_storage_) io_type; }

        void describe() override {
            // 初始化存储器内容
            std::vector<uint64_t> init_data = {10,  20,  30,  40,  50,  60,
                                               70,  80,  90,  100, 110, 120,
                                               130, 140, 150, 160};
            // 创建一个16个条目，每个条目8位的存储器
            ch_mem<ch_uint<8>, 16> mem(init_data, "test_mem");

            ch_bool enable_in(1_b);
            enable_in <<= io().enable_in;

            // 创建同步读端口
            auto read_port = mem.sread(io().addr_in, enable_in, "sread_port");

            // 使用operator<<=连接读端口到输出
            io().data_out <<= read_port;
        }
    };

    ch_device<MemSReadConnectionModule> dev;
    Simulator sim(dev.context());

    auto addr_in = dev.io().addr_in;
    auto enable_in = dev.io().enable_in;
    auto data_out = dev.io().data_out;

    // 测试不同地址的读取
    std::vector<std::pair<uint64_t, uint64_t>> test_cases = {
        {0, 10},  // 地址0，应返回10
        {1, 20},  // 地址1，应返回20
        {2, 30},  // 地址2，应返回30
        {7, 80},  // 地址7，应返回80
        {15, 160} // 地址15，应返回160
    };

    for (const auto &test_case : test_cases) {
        uint64_t addr = test_case.first;
        uint64_t expected = test_case.second;

        sim.set_input_value(addr_in, addr);
        sim.set_input_value(enable_in, 1); // 使能读取
        sim.tick();
        sim.tick();

        uint64_t actual = static_cast<uint64_t>(sim.get_value(data_out));
        REQUIRE(actual == expected);
    }
}

TEST_CASE("ch_mem aread port connection to ch_uint",
          "[mem][connection][aread]") {
    class MemAReadConnectionModule : public Component {
    public:
        __io(ch_in<ch_uint<4>> addr_in;   // 4位地址输入
             ch_out<ch_uint<8>> data_out; // 8位数据输出
             )

            MemAReadConnectionModule(Component *parent = nullptr,
                                     const std::string &name = "mem_aread_conn")
            : Component(parent, name) {}

        void create_ports() override { new (this->io_storage_) io_type; }

        void describe() override {

            // 初始化存储器内容
            std::vector<uint64_t> init_data = {5,   15,  25,  35, 45,  55,
                                               65,  75,  85,  95, 105, 115,
                                               125, 135, 145, 155};

            // 创建一个16个条目，每个条目8位的存储器
            ch_mem<ch_uint<8>, 16> mem(init_data, "test_mem");

            // 创建异步读端口
            auto read_port = mem.aread(io().addr_in, "aread_port");

            // 使用operator<<=连接读端口到输出
            io().data_out <<= read_port;
        }
    };

    ch_device<MemAReadConnectionModule> dev;
    Simulator sim(dev.context());

    auto addr_in = dev.io().addr_in;
    auto data_out = dev.io().data_out;

    // 测试不同地址的读取
    std::vector<std::pair<uint64_t, uint64_t>> test_cases = {
        {0, 5},    // 地址0，应返回5
        {1, 15},   // 地址1，应返回15
        {3, 35},   // 地址3，应返回35
        {10, 105}, // 地址10，应返回105
        {15, 155}  // 地址15，应返回155
    };

    for (const auto &test_case : test_cases) {
        uint64_t addr = test_case.first;
        uint64_t expected = test_case.second;

        sim.set_input_value(addr_in, addr);
        sim.tick(); // 异步读取不需要时钟周期，但为了更新输出，仍然tick

        uint64_t actual = static_cast<uint64_t>(sim.get_value(data_out));
        REQUIRE(actual == expected);
    }
}

TEST_CASE("ch_mem read port connection with ch_uint variable",
          "[mem][connection][read_port][ch_uint]") {
    class MemReadWithVariableModule : public Component {
    public:
        __io(ch_in<ch_uint<3>> addr_in; // 3位地址输入 (支持8个条目)
             ch_in<ch_bool> clk_in; ch_in<ch_bool> rst_in;
             ch_out<ch_uint<16>> data_out; // 16位数据输出
             )

            MemReadWithVariableModule(
                Component *parent = nullptr,
                const std::string &name = "mem_read_var_conn")
            : Component(parent, name) {}

        void create_ports() override { new (this->io_storage_) io_type; }

        void describe() override {

            // 初始化存储器内容
            std::vector<uint64_t> init_data = {100, 200, 300, 400,
                                               500, 600, 700, 800};

            // 创建一个8个条目，每个条目16位的存储器
            ch_mem<ch_uint<16>, 8> mem(init_data, "test_mem");
            // 创建同步读端口
            auto read_port =
                mem.sread(io().addr_in, ch_bool(1_b), "sread_port");

            // 创建一个ch_uint变量来接收读端口的值
            ch_uint<16> read_data(0_d);
            read_data <<= read_port;

            // 使用operator <<= 连接到输出
            io().data_out <<= read_data;
        }
    };

    ch_device<MemReadWithVariableModule> dev;
    Simulator sim(dev.context());

    auto addr_in = dev.io().addr_in;
    auto data_out = dev.io().data_out;

    // 测试不同地址的读取
    std::vector<std::pair<uint64_t, uint64_t>> test_cases = {
        {0, 100}, // 地址0，应返回100
        {1, 200}, // 地址1，应返回200
        {2, 300}, // 地址2，应返回300
        {4, 500}, // 地址4，应返回500
        {7, 800}  // 地址7，应返回800
    };

    for (const auto &test_case : test_cases) {
        uint64_t addr = test_case.first;
        uint64_t expected = test_case.second;

        sim.set_input_value(addr_in, addr);
        sim.tick();

        int i = 0;
        toDAG("mem" + std::to_string(i) + ".dot", dev.context(), sim);
        i++;

        uint64_t actual = static_cast<uint64_t>(sim.get_value(data_out));
        REQUIRE(actual == expected);
    }
}

TEST_CASE("ch_mem sread port to ch_reg connection",
          "[mem][connection][sread][ch_reg]") {
    class MemSReadToRegModule : public Component {
    public:
        __io(ch_in<ch_uint<4>> addr_in; // 4位地址输入
             ch_in<ch_bool> clk_in; ch_in<ch_bool> rst_in;
             ch_out<ch_uint<8>> data_out; // 8位数据输出
             )

            MemSReadToRegModule(Component *parent = nullptr,
                                const std::string &name = "mem_sread_reg_conn")
            : Component(parent, name) {}

        void create_ports() override { new (this->io_storage_) io_type; }

        void describe() override {

            // 初始化存储器内容
            std::vector<uint64_t> init_data = {11,  22,  33,  44,  55,  66,
                                               77,  88,  99,  111, 122, 133,
                                               144, 155, 166, 177};

            // 创建一个16个条目，每个条目8位的存储器
            ch_mem<ch_uint<8>, 16> mem(init_data, "test_mem");
            // 创建同步读端口
            auto read_port =
                mem.sread(io().addr_in, ch_bool(1_b), "sread_port");

            // 创建寄存器来存储读取的数据
            ch_reg<ch_uint<8>> read_reg(0_d, "read_reg");

            // 使用operator<<=连接读端口到寄存器
            read_reg <<= read_port;

            // 将寄存器输出连接到模块输出
            io().data_out <<= read_reg;
        }
    };

    ch_device<MemSReadToRegModule> dev;
    Simulator sim(dev.context());

    auto addr_in = dev.io().addr_in;
    auto data_out = dev.io().data_out;

    // 测试不同地址的读取
    std::vector<std::pair<uint64_t, uint64_t>> test_cases = {
        {0, 11},   // 地址0，应返回11
        {1, 22},   // 地址1，应返回22
        {5, 66},   // 地址5，应返回66
        {10, 122}, // 地址10，应返回122
        {15, 177}  // 地址15，应返回177
    };

    for (const auto &test_case : test_cases) {
        uint64_t addr = test_case.first;
        uint64_t expected = test_case.second;

        sim.set_input_value(addr_in, addr);
        sim.tick();
        sim.tick();

        uint64_t actual = static_cast<uint64_t>(sim.get_value(data_out));
        REQUIRE(actual == expected);
    }
}