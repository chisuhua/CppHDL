#include "catch_amalgamated.hpp"
#include "core/reg.h"
#include "core/uint.h"
#include "core/context.h"
#include "ast/ast_nodes.h"

using namespace ch::core;

TEST_CASE("Register-Proxy Explicit Linking", "[reg][linking]") {
    context ctx("test_ctx");
    ctx_swap swap(&ctx);
    
    // Create a register
    ch_reg<ch_uint<8>> reg(0_d, "test_reg");
    
    // Get the register node implementation
    auto* reg_impl = static_cast<regimpl*>(reg.impl());
    
    // Verify the register node exists
    REQUIRE(reg_impl != nullptr);
    REQUIRE(reg_impl->type() == lnodetype::type_reg);
    
    // Verify the proxy node exists and is linked
    auto* proxy = reg_impl->get_proxy();
    REQUIRE(proxy != nullptr);
    REQUIRE(proxy->type() == lnodetype::type_proxy);
    
    // Verify the proxy node is correctly sized
    REQUIRE(proxy->size() == 8);
    
    // Verify the names are correct
    REQUIRE(reg_impl->name() == "test_reg");
    REQUIRE(proxy->name() == "_test_reg");
}

TEST_CASE("Register Next Assignment with Explicit Linking", "[reg][next][linking]") {
    context ctx("test_ctx");
    ctx_swap swap(&ctx);
    
    // Create registers
    ch_reg<ch_uint<8>> reg_a(0_d, "reg_a");
    ch_reg<ch_uint<8>> reg_b(0_d, "reg_b");
    
    // Get register implementations
    auto* reg_a_impl = static_cast<regimpl*>(reg_a.impl());
    auto* reg_b_impl = static_cast<regimpl*>(reg_b.impl());
    
    // Verify both registers have proxy links
    REQUIRE(reg_a_impl->get_proxy() != nullptr);
    REQUIRE(reg_b_impl->get_proxy() != nullptr);
    
    // Assign reg_a's next value to reg_b
    reg_a->next = reg_b;
    
    // Verify the next value is set correctly
    REQUIRE(reg_a_impl->get_next() != nullptr);
    REQUIRE(reg_a_impl->get_next() == reg_b_impl->get_proxy());
}