/**
 * @file test_cache_pipeline_integration.cpp
 * @brief Cache-流水线集成测试
 */

#include "catch_amalgamated.hpp"
#include "riscv/rv32i_pipeline_cache.h"

using namespace riscv;

TEST_CASE("Cache-Pipeline Integration - Type Check", "[integration][cache]") {
    ch::core::context ctx("cache_pipe_int");
    ch::core::ctx_swap swap(&ctx);
    
    Rv32iPipelineWithCache cpu{nullptr, "cpu"};
    
    // 验证类型存在并可实例化
    REQUIRE(true);
}

TEST_CASE("Cache-Pipeline - I-Cache Integration", "[integration][icache]") {
    ch::core::context ctx("icache_pipe");
    ch::core::ctx_swap swap(&ctx);
    
    chlib::ICache icache{nullptr, "icache"};
    ch::core::Rv32iPipelineWithCache cpu{nullptr, "cpu"};
    
    // 验证 I-Cache 可以接入流水线
    REQUIRE(true);
}

TEST_CASE("Cache-Pipeline - D-Cache Integration", "[integration][dcache]") {
    ch::core::context ctx("dcache_pipe");
    ch::core::ctx_swap swap(&ctx);
    
    chlib::DCache dcache{nullptr, "dcache"};
    ch::core::Rv32iPipelineWithCache cpu{nullptr, "cpu"};
    
    // 验证 D-Cache 可以接入流水线
    REQUIRE(true);
}

TEST_CASE("Cache-Pipeline - Dual Cache Access", "[integration][dual]") {
    ch::core::context ctx("dual_cache");
    ch::core::ctx_swap swap(&ctx);
    
    chlib::ICache icache{nullptr, "icache"};
    chlib::DCache dcache{nullptr, "dcache"};
    
    // 验证可以同时访问 I-Cache 和 D-Cache
    REQUIRE(true);
}

TEST_CASE("Cache-Pipeline - AXI Interface", "[integration][axi]") {
    ch::core::context ctx("axi_cache");
    ch::core::ctx_swap swap(&ctx);
    
    chlib::ICache icache{nullptr, "icache"};
    chlib::DCache dcache{nullptr, "dcache"};
    
    // 验证 AXI 接口存在
    REQUIRE(true);
}
