#include "catch_amalgamated.hpp"
#include "core/reg.h"
#include "core/uint.h"
#include "core/context.h"
#include "core/operators.h"
#include "ast/ast_nodes.h"

using namespace ch::core;

TEST_CASE("User Tracking for Register Nodes", "[user_tracking][reg]") {
    context ctx("test_ctx");
    ctx_swap swap(&ctx);
    
    // Create registers
    ch_reg<ch_uint<8>> reg_a(0_d, "reg_a");
    ch_reg<ch_uint<8>> reg_b(0_d, "reg_b");
    
    // Get register implementations
    auto* reg_a_proxy = reg_a.impl();
    auto* reg_b_proxy = reg_b.impl();
    
    auto* reg_a_impl = static_cast<regimpl*>(reg_a_proxy->src(0));
    auto* reg_b_impl = static_cast<regimpl*>(reg_b_proxy->src(0));
    
    // Registers have an initial value (literal), so they should have one user
    REQUIRE(reg_a_impl->get_users().size() == 1);
    REQUIRE(reg_b_impl->get_users().size() == 1);
    REQUIRE(reg_a_proxy->get_users().empty());
    REQUIRE(reg_b_proxy->get_users().empty());
    
    // Create an operation that uses reg_a and reg_b
    auto result = reg_a + reg_b;
    auto* result_proxy = result.impl();
    auto* result_impl = static_cast<opimpl*>(result_proxy->src(0));
    
    // Verify that reg_a and reg_b proxies now have users
    REQUIRE(reg_a_proxy->get_users().size() == 1);
    REQUIRE(reg_b_proxy->get_users().size() == 1);
    
    // Verify that the users are the operation nodes
    REQUIRE(reg_a_proxy->get_users()[0] == result_impl);
    REQUIRE(reg_b_proxy->get_users()[0] == result_impl);
    
    // The operation should not have users yet (except for its proxy)
    // The proxy node should be the only user of the operation node
    REQUIRE(result_impl->get_users().size() == 1);
    REQUIRE(result_impl->get_users()[0] == result_proxy);
    REQUIRE(result_proxy->get_users().empty());
}

TEST_CASE("User Tracking for Operation Nodes", "[user_tracking][op]") {
    context ctx("test_ctx");
    ctx_swap swap(&ctx);
    
    // Create uint values
    ch_uint<8> val_a(5_d);
    ch_uint<8> val_b(3_d);
    
    // Create an operation
    auto result = val_a + val_b;
    auto* result_proxy = result.impl();
    auto* result_impl = static_cast<opimpl*>(result_proxy->src(0));
    
    // Verify source tracking
    REQUIRE(result_impl->lhs() == val_a.impl());
    REQUIRE(result_impl->rhs() == val_b.impl());
    
    // Verify user tracking
    REQUIRE(val_a.impl()->get_users().size() == 1);
    REQUIRE(val_b.impl()->get_users().size() == 1);
    REQUIRE(val_a.impl()->get_users()[0] == result_impl);
    REQUIRE(val_b.impl()->get_users()[0] == result_impl);
    
    // The operation should not have users yet (except for its proxy)
    // The proxy node should be the only user of the operation node
    REQUIRE(result_impl->get_users().size() == 1);
    REQUIRE(result_impl->get_users()[0] == result_proxy);
    REQUIRE(result_proxy->get_users().empty());
    
    // Create another operation using the result
    ch_uint<8> val_c(2_d);
    auto final_result = result + val_c;
    auto* final_proxy = final_result.impl();
    auto* final_impl = static_cast<opimpl*>(final_proxy->src(0));
    
    // Verify updated user tracking
    REQUIRE(result_proxy->get_users().size() == 1);
    REQUIRE(val_c.impl()->get_users().size() == 1);
    REQUIRE(result_proxy->get_users()[0] == final_impl);
    REQUIRE(val_c.impl()->get_users()[0] == final_impl);
    
    // The final operation should not have users yet (except for its proxy)
    REQUIRE(final_impl->get_users().size() == 1);
    REQUIRE(final_impl->get_users()[0] == final_proxy);
    REQUIRE(final_proxy->get_users().empty());
}