#include "catch_amalgamated.hpp"
#include "chlib/axi4lite.h"
#include "core/context.h"
#include "core/literal.h"
#include "simulator.h"
#include <memory>

using namespace ch::core;
using namespace chlib;

TEST_CASE("AXI4-Lite: Memory Slave Basic Write", "[axi4lite][slave]") {
    auto ctx = std::make_unique<ch::core::context>("test_axi4lite_slave_write");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Basic write transaction") {
        Axi4LiteMemorySlave<8, 32> slave("test_slave");
        
        ch_bool clk = false;
        ch_bool rst = true;
        
        Axi4LiteSlave<8, 32> axi_in;
        axi_in.aw.awaddr = 0x10_d;
        axi_in.aw.awprot = 0_d;
        axi_in.aw.awvalid = false;
        axi_in.aw.awready = false;  // Will be driven by slave
        
        axi_in.w.wdata = 0x12345678_d;
        axi_in.w.wstrb = 0xF_d;  // All bytes enabled
        axi_in.w.wlast = true;
        axi_in.w.wvalid = false;
        axi_in.w.wready = false;  // Will be driven by slave
        
        axi_in.b.bresp = 0_d;    // Will be driven by slave
        axi_in.b.bvalid = false; // Will be driven by slave
        axi_in.b.bready = false;
        
        axi_in.ar.araddr = 0x00_d;
        axi_in.ar.arprot = 0_d;
        axi_in.ar.arvalid = false;
        axi_in.ar.arready = false;  // Will be driven by slave
        
        axi_in.r.rdata = 0_d;    // Will be driven by slave
        axi_in.r.rresp = 0_d;    // Will be driven by slave
        axi_in.r.rlast = false;  // Will be driven by slave
        axi_in.r.rvalid = false; // Will be driven by slave
        axi_in.r.rready = false;
        
        ch::Simulator sim(ctx.get());
        
        // Reset
        clk = false;
        rst = true;
        axi_in = slave.process(clk, rst, axi_in);
        sim.tick();
        
        rst = false;
        clk = true;
        axi_in = slave.process(clk, rst, axi_in);
        sim.tick();
        
        // Start write address phase
        axi_in.aw.awvalid = true;
        axi_in.w.wvalid = true;
        
        // First tick - address and data presented
        axi_in = slave.process(clk, rst, axi_in);
        REQUIRE(simulator.get_value(axi_in.aw.awready) == true);  // Slave should accept address
        REQUIRE(simulator.get_value(axi_in.w.wready) == true);   // Slave should accept data
        
        // Complete the write transaction
        clk = false;
        axi_in = slave.process(clk, rst, axi_in);
        sim.tick();
        
        clk = true;
        axi_in.b.bready = true;  // Master ready to accept response
        axi_in = slave.process(clk, rst, axi_in);
        
        // Should have completed write transaction
        REQUIRE(simulator.get_value(axi_in.b.bvalid) == true);   // Response valid
        REQUIRE(simulator.get_value(axi_in.b.bresp) == 0);       // OKAY response
    }
}

TEST_CASE("AXI4-Lite: Simple Master Write", "[axi4lite][master]") {
    auto ctx = std::make_unique<ch::core::context>("test_axi4lite_master_write");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Basic master write transaction") {
        Axi4LiteSimpleMaster<8, 32> master("test_master");
        
        ch_bool clk = false;
        ch_bool rst = true;
        ch_bool start = false;
        ch_bool write_op = true;
        ch_uint<8> addr = 0x20_d;
        ch_uint<32> data = 0xAABBCCDD_d;
        
        ch::Simulator sim(ctx.get());
        
        // Reset
        clk = false;
        rst = true;
        Axi4LiteMaster<8, 32> axi_out = master.process(clk, rst, start, write_op, addr, data);
        sim.tick();
        
        rst = false;
        clk = true;
        axi_out = master.process(clk, rst, start, write_op, addr, data);
        sim.tick();
        
        // Start the transaction
        start = true;
        clk = false;
        axi_out = master.process(clk, rst, start, write_op, addr, data);
        sim.tick();
        
        clk = true;
        axi_out = master.process(clk, rst, start, write_op, addr, data);
        
        // Should be in write address phase
        REQUIRE(simulator.get_value(axi_out.aw.awvalid) == true);
        REQUIRE(simulator.get_value(axi_out.aw.awaddr) == 0x20);
        
        // Simulate slave ready
        // In a real system, we would connect the master to a slave
        // For this test, we just verify the master outputs
        REQUIRE(simulator.get_value(axi_out.w.wvalid) == false);  // Should wait for awready
    }
}

TEST_CASE("AXI4-Lite: Simple Master Read", "[axi4lite][master]") {
    auto ctx = std::make_unique<ch::core::context>("test_axi4lite_master_read");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Basic master read transaction") {
        Axi4LiteSimpleMaster<8, 32> master("test_master");
        
        ch_bool clk = false;
        ch_bool rst = true;
        ch_bool start = false;
        ch_bool write_op = false;  // Read operation
        ch_uint<8> addr = 0x30_d;
        ch_uint<32> data = 0x0_d;  // Not used for read
        
        ch::Simulator sim(ctx.get());
        
        // Reset
        clk = false;
        rst = true;
        Axi4LiteMaster<8, 32> axi_out = master.process(clk, rst, start, write_op, addr, data);
        sim.tick();
        
        rst = false;
        clk = true;
        axi_out = master.process(clk, rst, start, write_op, addr, data);
        sim.tick();
        
        // Start the transaction
        start = true;
        clk = false;
        axi_out = master.process(clk, rst, start, write_op, addr, data);
        sim.tick();
        
        clk = true;
        axi_out = master.process(clk, rst, start, write_op, addr, data);
        
        // Should be in read address phase
        REQUIRE(simulator.get_value(axi_out.ar.arvalid) == true);
        REQUIRE(simulator.get_value(axi_out.ar.araddr) == 0x30);
        REQUIRE(simulator.get_value(axi_out.r.rready) == true);  // Master ready to receive read data
    }
}

TEST_CASE("AXI4-Lite: Master Transaction Done", "[axi4lite][master]") {
    auto ctx = std::make_unique<ch::core::context>("test_axi4lite_transaction_done");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Verify transaction done signal") {
        Axi4LiteSimpleMaster<8, 32> master("test_master");
        
        ch_bool clk = false;
        ch_bool rst = true;
        ch_bool start = false;
        ch_bool write_op = true;
        ch_uint<8> addr = 0x40_d;
        ch_uint<32> data = 0xDEADBEEF_d;
        
        ch::Simulator sim(ctx.get());
        
        // Reset
        clk = false;
        rst = true;
        Axi4LiteMaster<8, 32> axi_out = master.process(clk, rst, start, write_op, addr, data);
        sim.tick();
        
        REQUIRE(simulator.get_value(master.is_transaction_done()) == false);
        
        rst = false;
        clk = true;
        axi_out = master.process(clk, rst, start, write_op, addr, data);
        sim.tick();
        
        // Start transaction
        start = true;
        clk = false;
        axi_out = master.process(clk, rst, start, write_op, addr, data);
        sim.tick();
        
        clk = true;
        axi_out = master.process(clk, rst, start, write_op, addr, data);
        sim.tick();
        
        // Should not be done yet
        REQUIRE(simulator.get_value(master.is_transaction_done()) == false);
    }
}