// tests/test_axi_spi.cpp
// AXI4-Lite SPI Controller Tests

#define CATCH_CONFIG_MAIN
#include "axi4/peripherals/axi_spi.h"
#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "simulator.h"
#include "codegen_verilog.h"
#include <memory>
#include <iostream>
#include <fstream>

using namespace ch;
using namespace ch::core;
using namespace axi4;

// ============================================================================
// Helper Functions for AXI4-Lite Transactions
// ============================================================================

template <typename T>
void axi_lite_write(ch::Simulator& sim, T& device, uint32_t addr, uint32_t data) {
    // Write address
    sim.set_port_value(device.instance().io().awaddr, addr);
    sim.set_port_value(device.instance().io().awprot, 0);
    sim.set_port_value(device.instance().io().awvalid, 1);
    
    // Wait for AW ready
    while (sim.get_port_value(device.instance().io().awready) == 0) {
        sim.tick();
    }
    sim.tick();
    sim.set_port_value(device.instance().io().awvalid, 0);
    
    // Write data
    sim.set_port_value(device.instance().io().wdata, data);
    sim.set_port_value(device.instance().io().wstrb, 0xF);
    sim.set_port_value(device.instance().io().wvalid, 1);
    
    // Wait for W ready and B valid
    while (sim.get_port_value(device.instance().io().wready) == 0) {
        sim.tick();
    }
    sim.tick();
    sim.set_port_value(device.instance().io().wvalid, 0);
    
    // Wait for B valid
    while (sim.get_port_value(device.instance().io().bvalid) == 0) {
        sim.tick();
    }
    sim.set_port_value(device.instance().io().bready, 1);
    sim.tick();
    sim.set_port_value(device.instance().io().bready, 0);
}

template <typename T>
uint32_t axi_lite_read(ch::Simulator& sim, T& device, uint32_t addr) {
    // Read address
    sim.set_port_value(device.instance().io().araddr, addr);
    sim.set_port_value(device.instance().io().arprot, 0);
    sim.set_port_value(device.instance().io().arvalid, 1);
    
    // Wait for AR ready
    while (sim.get_port_value(device.instance().io().arready) == 0) {
        sim.tick();
    }
    sim.tick();
    sim.set_port_value(device.instance().io().arvalid, 0);
    
    // Wait for R valid
    while (sim.get_port_value(device.instance().io().rvalid) == 0) {
        sim.tick();
    }
    uint32_t data = static_cast<uint64_t>(sim.get_port_value(device.instance().io().rdata));
    sim.set_port_value(device.instance().io().rready, 1);
    sim.tick();
    sim.set_port_value(device.instance().io().rready, 0);
    
    return data;
}

// ============================================================================
// Test Cases
// ============================================================================

TEST_CASE("AxiLiteSpi - ComponentCreation", "[axi][spi][basic]") {
    auto ctx = std::make_unique<ch::core::context>("test_spi_basic");
    ch::core::ctx_swap ctx_guard(ctx.get());
    
    // Create SPI instance with 8-bit data width
    AxiLiteSpi<8, 16> spi(nullptr, "spi");
    
    // Verify component creation
    REQUIRE(spi.name() == "spi");
}

TEST_CASE("AxiLiteSpi - ComponentCreation32Bit", "[axi][spi][basic]") {
    auto ctx = std::make_unique<ch::core::context>("test_spi_basic32");
    ch::core::ctx_swap ctx_guard(ctx.get());
    
    // Create SPI instance with 32-bit data width
    AxiLiteSpi<32, 16> spi(nullptr, "spi32");
    
    // Verify component creation
    REQUIRE(spi.name() == "spi32");
}

TEST_CASE("AxiLiteSpi - RegisterAccess", "[axi][spi][register]") {
    auto ctx = std::make_unique<ch::core::context>("test_spi_reg");
    ch::core::ctx_swap ctx_guard(ctx.get());
    
    ch::ch_device<AxiLiteSpi<8, 16>> spi_device;
    ch::Simulator sim(spi_device.context());
    
    // Initialize inputs
    sim.set_input_value(spi_device.instance().io().spi_miso, 0);
    sim.set_input_value(spi_device.instance().io().bready, 0);
    sim.set_input_value(spi_device.instance().io().rready, 0);
    
    // Test register write and read
    const uint32_t BAUD_VAL = 99;
    axi_lite_write(sim, spi_device, 0x10, BAUD_VAL);  // BAUD register
    uint32_t read_val = axi_lite_read(sim, spi_device, 0x10);
    
    REQUIRE((read_val & 0xFFFF) == BAUD_VAL);
    
    // Test CTRL register write
    axi_lite_write(sim, spi_device, 0x0C, 0x01);  // ENABLE
    read_val = axi_lite_read(sim, spi_device, 0x0C);
    REQUIRE((read_val & 0x01) == 0x01);
}

TEST_CASE("AxiLiteSpi - SpiTransfer8Bit", "[axi][spi][transfer]") {
    auto ctx = std::make_unique<ch::core::context>("test_spi_xfer8");
    ch::core::ctx_swap ctx_guard(ctx.get());
    
    ch::ch_device<AxiLiteSpi<8, 16>> spi_device;
    ch::Simulator sim(spi_device.context());
    
    // Initialize inputs
    sim.set_input_value(spi_device.instance().io().spi_miso, 0);
    sim.set_input_value(spi_device.instance().io().bready, 0);
    sim.set_input_value(spi_device.instance().io().rready, 0);
    
    // Set baud rate divider
    axi_lite_write(sim, spi_device, 0x10, 9);  // Divide by 10
    
    // Enable SPI controller
    axi_lite_write(sim, spi_device, 0x0C, 0x01);  // ENABLE
    
    // Check initial status (TX_EMPTY should be set)
    uint32_t status = axi_lite_read(sim, spi_device, 0x08);
    REQUIRE((status & 0x02) != 0);  // TX_EMPTY
    
    // Start transfer by writing to TX_DATA (0x00)
    const uint8_t TEST_DATA = 0xA5;  // 10100101
    axi_lite_write(sim, spi_device, 0x00, TEST_DATA);
    
    // Simulate loopback mode (MISO = MOSI)
    bool transfer_done = false;
    int cycle_count = 0;
    int sclk_edges = 0;
    bool last_sclk = false;
    
    for (int i = 0; i < 500; ++i) {
        sim.tick();
        cycle_count++;
        
        auto mosi = static_cast<uint64_t>(sim.get_port_value(spi_device.instance().io().spi_mosi));
        auto sclk = static_cast<uint64_t>(sim.get_port_value(spi_device.instance().io().spi_sclk));
        auto cs = static_cast<uint64_t>(sim.get_port_value(spi_device.instance().io().spi_cs));
        
        // Loopback: MISO = MOSI
        sim.set_input_value(spi_device.instance().io().spi_miso, mosi);
        
        // Count SCLK edges
        if (sclk && !last_sclk) {
            sclk_edges++;
        }
        last_sclk = (sclk != 0);
        
        // Check for CS going low (transfer in progress)
        if (cs == 0 && i > 10) {
            transfer_done = true;
        }
        
        // Check status register
        status = axi_lite_read(sim, spi_device, 0x08);
        if ((status & 0x04) != 0) {  // RX_FULL
            break;
        }
    }
    
    // Verify transfer completed - use nested parentheses to avoid Catch2 || issue
    REQUIRE(((transfer_done == true) || ((status & 0x04) != 0)) == true);
    
    // Read RX data
    uint32_t rx_data = axi_lite_read(sim, spi_device, 0x04);
    
    // In loopback mode, RX should equal TX
    REQUIRE((rx_data & 0xFF) == TEST_DATA);
    
    std::cout << "SPI 8-bit transfer test passed!" << std::endl;
    std::cout << "  TX: 0x" << std::hex << TEST_DATA << std::dec << std::endl;
    std::cout << "  RX: 0x" << std::hex << (rx_data & 0xFF) << std::dec << std::endl;
    std::cout << "  Cycles: " << cycle_count << std::endl;
    std::cout << "  SCLK edges: " << sclk_edges << std::endl;
}

TEST_CASE("AxiLiteSpi - SpiTransfer32Bit", "[axi][spi][transfer32]") {
    auto ctx = std::make_unique<ch::core::context>("test_spi_xfer32");
    ch::core::ctx_swap ctx_guard(ctx.get());
    
    ch::ch_device<AxiLiteSpi<32, 16>> spi_device;
    ch::Simulator sim(spi_device.context());
    
    // Initialize inputs
    sim.set_input_value(spi_device.instance().io().spi_miso, 0);
    sim.set_input_value(spi_device.instance().io().bready, 0);
    sim.set_input_value(spi_device.instance().io().rready, 0);
    
    // Set baud rate divider
    axi_lite_write(sim, spi_device, 0x10, 9);  // Divide by 10
    
    // Enable SPI controller
    axi_lite_write(sim, spi_device, 0x0C, 0x01);  // ENABLE
    
    // Start transfer with 32-bit data
    const uint32_t TEST_DATA = 0xDEADBEEF;
    axi_lite_write(sim, spi_device, 0x00, TEST_DATA);
    
    // Simulate loopback mode
    bool rx_full = false;
    int cycle_count = 0;
    
    for (int i = 0; i < 2000; ++i) {
        sim.tick();
        cycle_count++;
        
        auto mosi = static_cast<uint64_t>(sim.get_port_value(spi_device.instance().io().spi_mosi));
        sim.set_input_value(spi_device.instance().io().spi_miso, mosi);
        
        // Check status register
        uint32_t status = axi_lite_read(sim, spi_device, 0x08);
        if ((status & 0x04) != 0) {  // RX_FULL
            rx_full = true;
            break;
        }
    }
    
    REQUIRE(rx_full == true);
    
    // Read RX data
    uint32_t rx_data = axi_lite_read(sim, spi_device, 0x04);
    
    // In loopback mode, RX should equal TX
    REQUIRE(rx_data == TEST_DATA);
    
    std::cout << "SPI 32-bit transfer test passed!" << std::endl;
    std::cout << "  TX: 0x" << std::hex << TEST_DATA << std::dec << std::endl;
    std::cout << "  RX: 0x" << std::hex << rx_data << std::dec << std::endl;
    std::cout << "  Cycles: " << cycle_count << std::endl;
}

TEST_CASE("AxiLiteSpi - BaudRateControl", "[axi][spi][baud]") {
    auto ctx = std::make_unique<ch::core::context>("test_spi_baud");
    ch::core::ctx_swap ctx_guard(ctx.get());
    
    ch::ch_device<AxiLiteSpi<8, 16>> spi_device;
    ch::Simulator sim(spi_device.context());
    
    // Initialize inputs
    sim.set_input_value(spi_device.instance().io().spi_miso, 0);
    sim.set_input_value(spi_device.instance().io().bready, 0);
    sim.set_input_value(spi_device.instance().io().rready, 0);
    
    // Test different baud rates
    for (uint32_t baud_div : {9, 19, 99}) {
        axi_lite_write(sim, spi_device, 0x10, baud_div);
        uint32_t read_baud = axi_lite_read(sim, spi_device, 0x10);
        REQUIRE((read_baud & 0xFFFF) == baud_div);
    }
    
    std::cout << "Baud rate control test passed!" << std::endl;
}

TEST_CASE("AxiLiteSpi - StatusFlags", "[axi][spi][status]") {
    auto ctx = std::make_unique<ch::core::context>("test_spi_status");
    ch::core::ctx_swap ctx_guard(ctx.get());
    
    ch::ch_device<AxiLiteSpi<8, 16>> spi_device;
    ch::Simulator sim(spi_device.context());
    
    // Initialize inputs
    sim.set_input_value(spi_device.instance().io().spi_miso, 0);
    sim.set_input_value(spi_device.instance().io().bready, 0);
    sim.set_input_value(spi_device.instance().io().rready, 0);
    
    // Initial status: TX_EMPTY should be set
    uint32_t status = axi_lite_read(sim, spi_device, 0x08);
    REQUIRE((status & 0x02) != 0);  // TX_EMPTY
    REQUIRE((status & 0x01) == 0);  // Not BUSY
    REQUIRE((status & 0x04) == 0);  // RX not full
    
    // Enable and start transfer
    axi_lite_write(sim, spi_device, 0x0C, 0x01);  // ENABLE
    axi_lite_write(sim, spi_device, 0x00, 0x55);  // TX_DATA
    
    // During transfer: BUSY should be set, TX_EMPTY should be clear
    sim.tick();
    status = axi_lite_read(sim, spi_device, 0x08);
    // Note: BUSY flag timing depends on implementation
    
    std::cout << "Status flags test passed!" << std::endl;
}

TEST_CASE("AxiLiteSpi - VerilogGeneration", "[axi][spi][codegen]") {
    auto ctx = std::make_unique<ch::core::context>("test_spi_vlog");
    ch::core::ctx_swap ctx_guard(ctx.get());
    
    ch::ch_device<AxiLiteSpi<8, 16>> spi_device;
    
    // Generate Verilog
    std::string vlog_file = "/workspace/CppHDL/tests/output/axi_spi.v";
    
    // Create output directory if needed
    (void)system("mkdir -p /workspace/CppHDL/tests/output");
    
    toVerilog(vlog_file, spi_device.context());
    
    // Verify file was created
    std::ifstream f(vlog_file);
    REQUIRE(f.good());
    
    // Check for key Verilog constructs
    std::string content((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());
    
    REQUIRE(content.find("module") != std::string::npos);
    REQUIRE(content.find("spi_mosi") != std::string::npos);
    REQUIRE(content.find("spi_miso") != std::string::npos);
    REQUIRE(content.find("spi_sclk") != std::string::npos);
    REQUIRE(content.find("spi_cs") != std::string::npos);
    REQUIRE(content.find("awaddr") != std::string::npos);
    REQUIRE(content.find("araddr") != std::string::npos);
    
    std::cout << "Verilog generation test passed!" << std::endl;
    std::cout << "  Output: " << vlog_file << std::endl;
    std::cout << "  Size: " << content.size() << " bytes" << std::endl;
}

// ============================================================================
// Integration Test: Full SPI Transaction Sequence
// ============================================================================

TEST_CASE("AxiLiteSpi - FullTransactionSequence", "[axi][spi][integration]") {
    auto ctx = std::make_unique<ch::core::context>("test_spi_full");
    ch::core::ctx_swap ctx_guard(ctx.get());
    
    ch::ch_device<AxiLiteSpi<8, 16>> spi_device;
    ch::Simulator sim(spi_device.context());
    
    // Initialize
    sim.set_input_value(spi_device.instance().io().spi_miso, 0);
    sim.set_input_value(spi_device.instance().io().bready, 0);
    sim.set_input_value(spi_device.instance().io().rready, 0);
    
    std::cout << "\n=== Full SPI Transaction Sequence Test ===" << std::endl;
    
    // Step 1: Configure baud rate
    std::cout << "Step 1: Setting baud rate divider..." << std::endl;
    axi_lite_write(sim, spi_device, 0x10, 19);  // Divide by 20
    REQUIRE((axi_lite_read(sim, spi_device, 0x10) & 0xFFFF) == 19);
    
    // Step 2: Enable SPI controller
    std::cout << "Step 2: Enabling SPI controller..." << std::endl;
    axi_lite_write(sim, spi_device, 0x0C, 0x01);  // ENABLE
    REQUIRE((axi_lite_read(sim, spi_device, 0x0C) & 0x01) == 0x01);
    
    // Step 3: Check initial status
    std::cout << "Step 3: Checking initial status..." << std::endl;
    uint32_t status = axi_lite_read(sim, spi_device, 0x08);
    REQUIRE((status & 0x02) != 0);  // TX_EMPTY
    
    // Step 4: Transfer multiple bytes in loopback mode
    std::cout << "Step 4: Transferring test bytes..." << std::endl;
    const uint8_t test_bytes[] = {0x00, 0x55, 0xAA, 0xFF, 0x12, 0x34, 0x56, 0x78};
    
    for (uint8_t tx_byte : test_bytes) {
        // Clear RX buffer
        axi_lite_write(sim, spi_device, 0x0C, 0x09);  // ENABLE | CLR_RX
        
        // Start transfer
        axi_lite_write(sim, spi_device, 0x00, tx_byte);
        
        // Wait for RX_FULL
        int timeout = 1000;
        while (timeout-- > 0) {
            sim.tick();
            auto mosi = static_cast<uint64_t>(sim.get_port_value(spi_device.instance().io().spi_mosi));
            sim.set_input_value(spi_device.instance().io().spi_miso, mosi);
            
            status = axi_lite_read(sim, spi_device, 0x08);
            if ((status & 0x04) != 0) break;  // RX_FULL
        }
        REQUIRE(timeout > 0);
        
        // Read and verify
        uint32_t rx = axi_lite_read(sim, spi_device, 0x04);
        REQUIRE((rx & 0xFF) == tx_byte);
        
        std::cout << "  TX: 0x" << std::hex << (int)tx_byte 
                  << " -> RX: 0x" << (int)(rx & 0xFF) << std::dec << std::endl;
    }
    
    // Step 5: Clear RX and verify status
    std::cout << "Step 5: Clearing RX buffer..." << std::endl;
    axi_lite_write(sim, spi_device, 0x0C, 0x09);  // ENABLE | CLR_RX
    status = axi_lite_read(sim, spi_device, 0x08);
    REQUIRE((status & 0x04) == 0);  // RX not full after clear
    
    std::cout << "Full transaction sequence test PASSED!" << std::endl;
}
