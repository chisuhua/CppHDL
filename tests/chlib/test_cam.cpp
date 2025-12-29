#include "catch_amalgamated.hpp"
#include "chlib/memory.h"
#include "core/context.h"
#include "core/literal.h"
#include "simulator.h"
#include <memory>

using namespace ch::core;
using namespace chlib;

TEST_CASE("Memory: CAM (Content Addressable Memory)", "[memory][cam]") {
    auto ctx = std::make_unique<ch::core::context>("test_cam");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Basic CAM write and search operations") {
        Cam<3, 8> cam("test_cam");
        
        ch_bool clk = true;
        ch_uint<8> data1 = 0x12_d;
        ch_uint<8> data2 = 0x34_d;
        ch_uint<3> addr1 = 0_d;
        ch_uint<3> addr2 = 1_d;
        
        // Write data to CAM
        cam.write(clk, addr1, data1, true);
        cam.write(clk, addr2, data2, true);
        
        // Search for data1
        CamResult<3, 8> result1 = cam.search(clk, data1, true);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(simulator.get_value(result1.hit) == true);
        REQUIRE(simulator.get_value(result1.addr) == 0);
        REQUIRE(simulator.get_value(result1.data) == 0x12);
        
        // Search for data2
        CamResult<3, 8> result2 = cam.search(clk, data2, true);
        sim.tick();
        
        REQUIRE(simulator.get_value(result2.hit) == true);
        REQUIRE(simulator.get_value(result2.addr) == 1);
        REQUIRE(simulator.get_value(result2.data) == 0x34);
        
        // Search for non-existent data
        ch_uint<8> non_existent = 0x56_d;
        CamResult<3, 8> result3 = cam.search(clk, non_existent, true);
        sim.tick();
        
        REQUIRE(simulator.get_value(result3.hit) == false);
    }
    
    SECTION("CAM invalidate operation") {
        Cam<2, 8> cam("test_cam_invalidate");
        
        ch_bool clk = true;
        ch_uint<8> data1 = 0xAB_d;
        ch_uint<2> addr1 = 0_d;
        
        // Write data to CAM
        cam.write(clk, addr1, data1, true);
        
        // Verify it exists
        CamResult<2, 8> result1 = cam.search(clk, data1, true);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(simulator.get_value(result1.hit) == true);
        REQUIRE(simulator.get_value(result1.addr) == 0);
        REQUIRE(simulator.get_value(result1.data) == 0xAB);
        
        // Invalidate the entry
        cam.invalidate(clk, addr1, true);
        
        // Try to search for the same data - should not find it
        CamResult<2, 8> result2 = cam.search(clk, data1, true);
        sim.tick();
        
        REQUIRE(simulator.get_value(result2.hit) == false);
    }
}

TEST_CASE("Memory: TCAM (Ternary Content Addressable Memory)", "[memory][tcam]") {
    auto ctx = std::make_unique<ch::core::context>("test_tcam");
    ch::core::ctx_swap ctx_swapper(ctx.get());

    SECTION("Basic TCAM write and search operations") {
        Tcam<3, 8> tcam("test_tcam");
        
        ch_bool clk = true;
        ch_uint<8> data1 = 0xF0_d;    // 11110000
        ch_uint<8> mask1 = 0xF0_d;    // Mask for upper 4 bits
        ch_uint<3> addr1 = 0_d;
        
        // Write data and mask to TCAM
        tcam.write(clk, addr1, data1, mask1, true);
        
        // Search for a value that matches the masked pattern
        ch_uint<8> search_data = 0xF5_d;  // 11110101 - matches upper 4 bits of data1 with mask1
        TcamResult<3, 8> result1 = tcam.search(clk, search_data);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(simulator.get_value(result1.hit) == true);
        REQUIRE(simulator.get_value(result1.addr) == 0);
        REQUIRE(simulator.get_value(result1.data) == 0xF0);
        
        // Search for a value that does not match
        ch_uint<8> search_data2 = 0x0F_d;  // 00001111 - does not match upper 4 bits
        TcamResult<3, 8> result2 = tcam.search(clk, search_data2);
        sim.tick();
        
        REQUIRE(simulator.get_value(result2.hit) == false);
    }
    
    SECTION("TCAM with wildcard pattern") {
        Tcam<3, 8> tcam("test_tcam_wildcard");
        
        ch_bool clk = true;
        ch_uint<8> data1 = 0xFF_d;    // 11111111
        ch_uint<8> mask1 = 0x00_d;    // Wildcard - all bits are don't care
        ch_uint<3> addr1 = 0_d;
        
        // Write wildcard entry to TCAM
        tcam.write(clk, addr1, data1, mask1, true);
        
        // Search for any value - should match wildcard
        ch_uint<8> search_data = 0x55_d;
        TcamResult<3, 8> result1 = tcam.search(clk, search_data);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(simulator.get_value(result1.hit) == true);
        REQUIRE(simulator.get_value(result1.addr) == 0);
        REQUIRE(simulator.get_value(result1.data) == 0xFF);
    }
    
    SECTION("TCAM invalidate operation") {
        Tcam<2, 8> tcam("test_tcam_invalidate");
        
        ch_bool clk = true;
        ch_uint<8> data1 = 0xCC_d;
        ch_uint<8> mask1 = 0xFF_d;
        ch_uint<2> addr1 = 0_d;
        
        // Write data to TCAM
        tcam.write(clk, addr1, data1, mask1, true);
        
        // Verify it exists
        ch_uint<8> search_data = 0xCC_d;
        TcamResult<2, 8> result1 = tcam.search(clk, search_data);
        
        ch::Simulator sim(ctx.get());
        sim.tick();
        
        REQUIRE(simulator.get_value(result1.hit) == true);
        REQUIRE(simulator.get_value(result1.addr) == 0);
        REQUIRE(simulator.get_value(result1.data) == 0xCC);
        
        // Invalidate the entry
        tcam.invalidate(clk, addr1, true);
        
        // Try to search for the same data - should not find it
        TcamResult<2, 8> result2 = tcam.search(clk, search_data);
        sim.tick();
        
        REQUIRE(simulator.get_value(result2.hit) == false);
    }
}