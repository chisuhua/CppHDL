// test_mem_timing.cpp
#define CATCH_CONFIG_MAIN
#include "catch_amalgamated.hpp"
#include "component.h"
#include "core/mem.h"
#include "core/reg.h"
#include "core/uint.h"
#include "core/bool.h"
#include "core/context.h"
#include "device.h"
#include "simulator.h"
#include "utils/logger.h"
#include <memory>
#include <vector>

using namespace ch;
using namespace ch::core;

// Component for testing single port synchronous memory read/write
class SinglePortMem : public ch::Component {
public:
    __io(
        ch_in<ch_uint<8>> addr_in;
        ch_in<ch_uint<32>> data_in;
        ch_in<ch_bool> we_in;  // Write enable
        ch_in<ch_bool> en_in;  // Enable for reads
        ch_out<ch_uint<32>> data_out;
        ch_out<ch_bool> done_out;
    )

    SinglePortMem(ch::Component *parent = nullptr,
                  const std::string &name = "single_port_mem")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_mem<ch_uint<32>, 256> mem("test_mem");
        
        // Write port
        auto write_port = mem.write(ch_uint<8>(io().addr_in.impl()), 
                                   ch_uint<32>(io().data_in.impl()), 
                                   ch_bool(io().we_in.impl()));
        
        // Read port (when not writing)
        auto read_port = mem.sread(ch_uint<8>(io().addr_in.impl()), ch_bool(io().en_in.impl()));
        
        // Output is either the read data or feedback from write
        io().data_out = select(io().we_in, io().data_in, read_port);
        io().done_out = io().en_in;  // Simple done signal
    }
};

// Component for testing dual-port memory (one read, one write)
class DualPortMem : public ch::Component {
public:
    __io(
        ch_in<ch_uint<8>> read_addr;
        ch_in<ch_uint<8>> write_addr;
        ch_in<ch_uint<32>> write_data;
        ch_in<ch_bool> write_enable;
        ch_out<ch_uint<32>> read_data;
        ch_out<ch_bool> read_valid;
    )

    DualPortMem(ch::Component *parent = nullptr,
                const std::string &name = "dual_port_mem")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_mem<ch_uint<32>, 256> mem("dual_port_mem");
        
        // Read port
        auto read_port = mem.sread(ch_uint<8>(io().read_addr.impl()), ch_bool(true));
        
        // Write port
        auto write_port = mem.write(ch_uint<8>(io().write_addr.impl()), 
                                   ch_uint<32>(io().write_data.impl()), 
                                   ch_bool(io().write_enable.impl()));
        
        io().read_data = read_port;
        io().read_valid = ch_bool(true);  // Always valid for this test
    }
};

// Component for testing memory with initialization data
class InitializedMem : public ch::Component {
public:
    __io(
        ch_in<ch_uint<8>> addr_in;
        ch_out<ch_uint<32>> data_out;
    )

    InitializedMem(ch::Component *parent = nullptr,
                   const std::string &name = "init_mem")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        // Initialize memory with some data
        std::vector<uint32_t> init_data = {0xDEADBEEF, 0x12345678, 0xABCDEF00, 0xFEDCBA98};
        ch_mem<ch_uint<32>, 256> mem(init_data, "init_mem");
        
        auto read_port = mem.aread(ch_uint<8>(io().addr_in.impl()));
        io().data_out = read_port;
    }
};

// Component for testing memory with multiple read ports
class MultiReadMem : public ch::Component {
public:
    __io(
        ch_in<ch_uint<8>> addr1_in;
        ch_in<ch_uint<8>> addr2_in;
        ch_out<ch_uint<32>> data1_out;
        ch_out<ch_uint<32>> data2_out;
    )

    MultiReadMem(ch::Component *parent = nullptr,
                 const std::string &name = "multi_read_mem")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_mem<ch_uint<32>, 256> mem("multi_read_mem");
        
        // Two read ports
        auto read_port1 = mem.aread(ch_uint<8>(io().addr1_in.impl()), "read1");
        auto read_port2 = mem.aread(ch_uint<8>(io().addr2_in.impl()), "read2");
        
        io().data1_out = read_port1;
        io().data2_out = read_port2;
    }
};

// Component for testing write-read dependency
class WriteReadDependency : public ch::Component {
public:
    __io(
        ch_in<ch_uint<8>> addr_in;
        ch_in<ch_uint<32>> data_in;
        ch_in<ch_bool> write_enable;
        ch_out<ch_uint<32>> data_out;
    )

    WriteReadDependency(ch::Component *parent = nullptr,
                        const std::string &name = "wr_dep_mem")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_mem<ch_uint<32>, 256> mem("wr_dep_mem");
        
        // Write port
        auto write_port = mem.write(ch_uint<8>(io().addr_in.impl()), 
                                   ch_uint<32>(io().data_in.impl()), 
                                   ch_bool(io().write_enable.impl()));
        
        // Read port at same address - this tests write-read forwarding
        auto read_port = mem.sread(ch_uint<8>(io().addr_in.impl()), ch_bool(true));
        
        io().data_out = read_port;
    }
};

TEST_CASE("Memory Timing - Single Port Memory Write/Read", "[mem][timing][single-port]") {
    ch_device<SinglePortMem> device;
    Simulator simulator(device.context());

    // Initialize memory by writing to address 0
    simulator.set_input_value(device.instance().io().addr_in, static_cast<uint64_t>(0));
    simulator.set_input_value(device.instance().io().data_in, static_cast<uint64_t>(0x12345678));
    simulator.set_input_value(device.instance().io().we_in, static_cast<uint64_t>(1));  // Write enable
    simulator.set_input_value(device.instance().io().en_in, static_cast<uint64_t>(0));  // Disable read

    // Tick 1: Write data to memory
    simulator.tick();

    // Now read from the same address
    simulator.set_input_value(device.instance().io().we_in, static_cast<uint64_t>(0));  // Read mode
    simulator.set_input_value(device.instance().io().en_in, static_cast<uint64_t>(1));  // Enable read
    simulator.tick();

    // Check if we read the same data back
    auto read_value = simulator.get_port_value(device.instance().io().data_out);
    REQUIRE(static_cast<uint64_t>(read_value) == 0x12345678);

    // Write different data to same address
    simulator.set_input_value(device.instance().io().data_in, static_cast<uint64_t>(0xABCDEF00));
    simulator.set_input_value(device.instance().io().we_in, static_cast<uint64_t>(1));  // Write mode
    simulator.set_input_value(device.instance().io().en_in, static_cast<uint64_t>(0));  // Disable read
    simulator.tick();

    // Read again
    simulator.set_input_value(device.instance().io().we_in, static_cast<uint64_t>(0));  // Read mode
    simulator.set_input_value(device.instance().io().en_in, static_cast<uint64_t>(1));  // Enable read
    simulator.tick();

    read_value = simulator.get_port_value(device.instance().io().data_out);
    REQUIRE(static_cast<uint64_t>(read_value) == 0xABCDEF00);
}

TEST_CASE("Memory Timing - Dual Port Memory", "[mem][timing][dual-port]") {
    ch_device<DualPortMem> device;
    Simulator simulator(device.context());

    // Write data to address 5
    simulator.set_input_value(device.instance().io().write_addr, static_cast<uint64_t>(5));
    simulator.set_input_value(device.instance().io().write_data, static_cast<uint64_t>(0x12345678));
    simulator.set_input_value(device.instance().io().write_enable, static_cast<uint64_t>(1));

    // Read from address 5
    simulator.set_input_value(device.instance().io().read_addr, static_cast<uint64_t>(5));

    // Tick 1: Perform write
    simulator.tick();

    // Read the value back
    auto read_value = simulator.get_port_value(device.instance().io().read_data);
    REQUIRE(static_cast<uint64_t>(read_value) == 0x12345678);

    // Write different data to address 5
    simulator.set_input_value(device.instance().io().write_data, static_cast<uint64_t>(0xFEDCBA98));
    simulator.set_input_value(device.instance().io().write_enable, static_cast<uint64_t>(1));
    simulator.tick();

    // Read again
    read_value = simulator.get_port_value(device.instance().io().read_data);
    REQUIRE(static_cast<uint64_t>(read_value) == 0xFEDCBA98);

    // Test writing to different address while reading from another
    simulator.set_input_value(device.instance().io().write_addr, static_cast<uint64_t>(10));
    simulator.set_input_value(device.instance().io().write_data, static_cast<uint64_t>(0xAAAAAAAA));
    simulator.set_input_value(device.instance().io().read_addr, static_cast<uint64_t>(5));
    simulator.tick();

    // Address 5 should still have the old value
    read_value = simulator.get_port_value(device.instance().io().read_data);
    REQUIRE(static_cast<uint64_t>(read_value) == 0xFEDCBA98);

    // Now read from address 10
    simulator.set_input_value(device.instance().io().read_addr, static_cast<uint64_t>(10));
    simulator.tick();
    read_value = simulator.get_port_value(device.instance().io().read_data);
    REQUIRE(static_cast<uint64_t>(read_value) == 0xAAAAAAAA);
}

TEST_CASE("Memory Timing - Initialized Memory", "[mem][timing][initialized]") {
    ch_device<InitializedMem> device;
    Simulator simulator(device.context());

    // Read from initialized addresses
    simulator.set_input_value(device.instance().io().addr_in, static_cast<uint64_t>(0));
    simulator.tick();
    auto read_value = simulator.get_port_value(device.instance().io().data_out);
    REQUIRE(static_cast<uint64_t>(read_value) == 0xDEADBEEF);

    simulator.set_input_value(device.instance().io().addr_in, static_cast<uint64_t>(1));
    simulator.tick();
    read_value = simulator.get_port_value(device.instance().io().data_out);
    REQUIRE(static_cast<uint64_t>(read_value) == 0x12345678);

    simulator.set_input_value(device.instance().io().addr_in, static_cast<uint64_t>(2));
    simulator.tick();
    read_value = simulator.get_port_value(device.instance().io().data_out);
    REQUIRE(static_cast<uint64_t>(read_value) == 0xABCDEF00);

    simulator.set_input_value(device.instance().io().addr_in, static_cast<uint64_t>(3));
    simulator.tick();
    read_value = simulator.get_port_value(device.instance().io().data_out);
    REQUIRE(static_cast<uint64_t>(read_value) == 0xFEDCBA98);

    // Now write to address 0 and verify it changes
    ch_device<SinglePortMem> write_device;
    Simulator write_simulator(write_device.context());
    
    write_simulator.set_input_value(write_device.instance().io().addr_in, static_cast<uint64_t>(0));
    write_simulator.set_input_value(write_device.instance().io().data_in, static_cast<uint64_t>(0x99999999));
    write_simulator.set_input_value(write_device.instance().io().we_in, static_cast<uint64_t>(1));
    write_simulator.set_input_value(write_device.instance().io().en_in, static_cast<uint64_t>(1));
    write_simulator.tick();

    write_simulator.set_input_value(write_device.instance().io().we_in, static_cast<uint64_t>(0));
    write_simulator.tick();

    read_value = write_simulator.get_port_value(write_device.instance().io().data_out);
    REQUIRE(static_cast<uint64_t>(read_value) == 0x99999999);
}

TEST_CASE("Memory Timing - Multi-Read Port Memory", "[mem][timing][multi-read]") {
    ch_device<MultiReadMem> device;
    Simulator simulator(device.context());

    // First, initialize some data using the single port memory component
    ch_device<SinglePortMem> init_device;
    Simulator init_simulator(init_device.context());

    // Write some test data to addresses 0, 1, 2, 3
    for (int i = 0; i < 4; i++) {
        init_simulator.set_input_value(init_device.instance().io().addr_in, static_cast<uint64_t>(i));
        init_simulator.set_input_value(init_device.instance().io().data_in, static_cast<uint64_t>(0x1000 + i));
        init_simulator.set_input_value(init_device.instance().io().we_in, static_cast<uint64_t>(1));
        init_simulator.set_input_value(init_device.instance().io().en_in, static_cast<uint64_t>(1));
        init_simulator.tick();
        init_simulator.set_input_value(init_device.instance().io().we_in, static_cast<uint64_t>(0));
        init_simulator.tick();
    }

    // Now test reading from multiple ports simultaneously with our multi-read component
    simulator.set_input_value(device.instance().io().addr1_in, static_cast<uint64_t>(0));
    simulator.set_input_value(device.instance().io().addr2_in, static_cast<uint64_t>(1));
    simulator.tick();

    auto data1 = simulator.get_port_value(device.instance().io().data1_out);
    auto data2 = simulator.get_port_value(device.instance().io().data2_out);
    REQUIRE(static_cast<uint64_t>(data1) == 0x1000);
    REQUIRE(static_cast<uint64_t>(data2) == 0x1001);

    // Test different addresses
    simulator.set_input_value(device.instance().io().addr1_in, static_cast<uint64_t>(2));
    simulator.set_input_value(device.instance().io().addr2_in, static_cast<uint64_t>(3));
    simulator.tick();

    data1 = simulator.get_port_value(device.instance().io().data1_out);
    data2 = simulator.get_port_value(device.instance().io().data2_out);
    REQUIRE(static_cast<uint64_t>(data1) == 0x1002);
    REQUIRE(static_cast<uint64_t>(data2) == 0x1003);
}

TEST_CASE("Memory Timing - Write-Read Dependency", "[mem][timing][dependency]") {
    ch_device<WriteReadDependency> device;
    Simulator simulator(device.context());

    // Write data to address 10
    simulator.set_input_value(device.instance().io().addr_in, static_cast<uint64_t>(10));
    simulator.set_input_value(device.instance().io().data_in, static_cast<uint64_t>(0x55555555));
    simulator.set_input_value(device.instance().io().write_enable, static_cast<uint64_t>(1));
    simulator.tick();

    // Try to read from the same address in the same cycle (depends on implementation)
    // In most memory implementations, this would return the old value
    auto read_value = simulator.get_port_value(device.instance().io().data_out);
    // For synchronous memory, we expect to see the old value initially
    // The new value should be available after another tick

    // Change write enable to 0 to perform a read
    simulator.set_input_value(device.instance().io().write_enable, static_cast<uint64_t>(0));
    simulator.tick();

    // Now we should see the written value
    read_value = simulator.get_port_value(device.instance().io().data_out);
    REQUIRE(static_cast<uint64_t>(read_value) == 0x55555555);

    // Write a new value to the same address
    simulator.set_input_value(device.instance().io().data_in, static_cast<uint64_t>(0xAAAAAAAA));
    simulator.set_input_value(device.instance().io().write_enable, static_cast<uint64_t>(1));
    simulator.tick();

    // Read again
    simulator.set_input_value(device.instance().io().write_enable, static_cast<uint64_t>(0));
    simulator.tick();

    read_value = simulator.get_port_value(device.instance().io().data_out);
    REQUIRE(static_cast<uint64_t>(read_value) == 0xAAAAAAAA);
}

TEST_CASE("Memory Timing - Sequential Writes and Reads", "[mem][timing][sequential]") {
    ch_device<SinglePortMem> device;
    Simulator simulator(device.context());

    // Sequentially write values to addresses 0-9
    for (int addr = 0; addr < 10; addr++) {
        simulator.set_input_value(device.instance().io().addr_in, static_cast<uint64_t>(addr));
        simulator.set_input_value(device.instance().io().data_in, static_cast<uint64_t>(0x2000 + addr));
        simulator.set_input_value(device.instance().io().we_in, static_cast<uint64_t>(1));
        simulator.set_input_value(device.instance().io().en_in, static_cast<uint64_t>(0));
        simulator.tick();
    }

    // Now read back all values in sequence
    for (int addr = 0; addr < 10; addr++) {
        simulator.set_input_value(device.instance().io().addr_in, static_cast<uint64_t>(addr));
        simulator.set_input_value(device.instance().io().we_in, static_cast<uint64_t>(0));
        simulator.set_input_value(device.instance().io().en_in, static_cast<uint64_t>(1));
        simulator.tick();
        
        auto read_value = simulator.get_port_value(device.instance().io().data_out);
        REQUIRE(static_cast<uint64_t>(read_value) == static_cast<uint64_t>(0x2000 + addr));
    }

    // Write to all addresses again with new values
    for (int addr = 0; addr < 10; addr++) {
        simulator.set_input_value(device.instance().io().addr_in, static_cast<uint64_t>(addr));
        simulator.set_input_value(device.instance().io().data_in, static_cast<uint64_t>(0x3000 + addr));
        simulator.set_input_value(device.instance().io().we_in, static_cast<uint64_t>(1));
        simulator.set_input_value(device.instance().io().en_in, static_cast<uint64_t>(0));
        simulator.tick();
    }

    // Read back again to verify new values
    for (int addr = 0; addr < 10; addr++) {
        simulator.set_input_value(device.instance().io().addr_in, static_cast<uint64_t>(addr));
        simulator.set_input_value(device.instance().io().we_in, static_cast<uint64_t>(0));
        simulator.set_input_value(device.instance().io().en_in, static_cast<uint64_t>(1));
        simulator.tick();
        
        auto read_value = simulator.get_port_value(device.instance().io().data_out);
        REQUIRE(static_cast<uint64_t>(read_value) == static_cast<uint64_t>(0x3000 + addr));
    }
}