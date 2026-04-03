// #define CATCH_CONFIG_MAIN
// #include "bundle/stream_bundle.h"
// #include "catch_amalgamated.hpp"
// #include "chlib/stream_width_adapter.h"
// #include "chlib/stream.h"
// #include "core/context.h"
// #include "simulator.h"
// 
// using namespace ch::core;
// using namespace chlib;
// 
// // ============ Test: Stream Width Adapter - Structural Tests ============
// 
// TEST_CASE("Stream Width Adapter: Structural - narrow_to_wide output type", "[stream][width_adapter][structural]") {
//     auto ctx = std::make_unique<ch::core::context>("test_narrow_to_wide_struct");
//     ch::core::ctx_swap ctx_guard(ctx.get());
// 
//     ch_stream<ch_uint<8>> narrow_input;
//     auto result = stream_narrow_to_wide<ch_uint<32>, ch_uint<8>, 4>(narrow_input);
// 
//     // Verify output stream has correct structure
//     REQUIRE(result.output.payload.width == 32);
//     REQUIRE(result.output.valid.width == 1);
//     REQUIRE(result.output.ready.width == 1);
// }
// 
// TEST_CASE("Stream Width Adapter: Structural - wide_to_narrow output type", "[stream][width_adapter][structural]") {
//     auto ctx = std::make_unique<ch::core::context>("test_wide_to_narrow_struct");
//     ch::core::ctx_swap ctx_guard(ctx.get());
// 
//     ch_stream<ch_uint<32>> wide_input;
//     auto result = stream_wide_to_narrow<ch_uint<8>, ch_uint<32>, 4>(wide_input);
// 
//     // Verify output stream has correct structure
//     REQUIRE(result.output.payload.width == 8);
//     REQUIRE(result.output.valid.width == 1);
//     REQUIRE(result.output.ready.width == 1);
// }
// 
// // ============ Test: Stream Width Adapter - Different width ratios ============
// 
// TEST_CASE("Stream Width Adapter: Different ratios - 8-bit to 16-bit", "[stream][width_adapter][ratio]") {
//     auto ctx = std::make_unique<ch::core::context>("test_8_to_16");
//     ch::core::ctx_swap ctx_guard(ctx.get());
// 
//     ch_stream<ch_uint<8>> narrow_input;
//     auto result = stream_narrow_to_wide<ch_uint<16>, ch_uint<8>, 2>(narrow_input);
// 
//     // Verify output stream has correct structure
//     REQUIRE(result.output.payload.width == 16);
//     REQUIRE(result.output.valid.width == 1);
//     REQUIRE(result.output.ready.width == 1);
// }
// 
// TEST_CASE("Stream Width Adapter: Different ratios - 16-bit to 8-bit", "[stream][width_adapter][ratio]") {
//     auto ctx = std::make_unique<ch::core::context>("test_16_to_8");
//     ch::core::ctx_swap ctx_guard(ctx.get());
// 
//     ch_stream<ch_uint<16>> wide_input;
//     auto result = stream_wide_to_narrow<ch_uint<8>, ch_uint<16>, 2>(wide_input);
// 
//     // Verify output stream has correct structure
//     REQUIRE(result.output.payload.width == 8);
//     REQUIRE(result.output.valid.width == 1);
//     REQUIRE(result.output.ready.width == 1);
// }
// 
// // ============ Test: Stream Width Adapter - Different ratios ============
// 
// TEST_CASE("Stream Width Adapter: 4-bit to 32-bit", "[stream][width_adapter][ratio]") {
//     auto ctx = std::make_unique<ch::core::context>("test_4_to_32");
//     ch::core::ctx_swap ctx_guard(ctx.get());
// 
//     ch_stream<ch_uint<4>> narrow_input;
//     auto result = stream_narrow_to_wide<ch_uint<32>, ch_uint<4>, 8>(narrow_input);
// 
//     // Verify output stream has correct structure
//     REQUIRE(result.output.payload.width == 32);
//     REQUIRE(result.output.valid.width == 1);
//     REQUIRE(result.output.ready.width == 1);
// }
// 
// TEST_CASE("Stream Width Adapter: 64-bit to 8-bit", "[stream][width_adapter][ratio]") {
//     auto ctx = std::make_unique<ch::core::context>("test_64_to_8");
//     ch::core::ctx_swap ctx_guard(ctx.get());
// 
//     ch_stream<ch_uint<64>> wide_input;
//     auto result = stream_wide_to_narrow<ch_uint<8>, ch_uint<64>, 8>(wide_input);
// 
//     // Verify output stream has correct structure
//     REQUIRE(result.output.payload.width == 8);
//     REQUIRE(result.output.valid.width == 1);
//     REQUIRE(result.output.ready.width == 1);
// }
// 
// // ============ Test: Stream Width Adapter - Input type verification ============
// 
// TEST_CASE("Stream Width Adapter: Input stream preserved", "[stream][width_adapter][input]") {
//     auto ctx = std::make_unique<ch::core::context>("test_input_preserved");
//     ch::core::ctx_swap ctx_guard(ctx.get());
// 
//     // Test narrow to wide preserves input
//     {
//         ch_stream<ch_uint<8>> narrow_input;
//         auto result = stream_narrow_to_wide<ch_uint<32>, ch_uint<8>, 4>(narrow_input);
//         
//         REQUIRE(result.input.payload.width == 8);
//         REQUIRE(result.input.valid.width == 1);
//         REQUIRE(result.input.ready.width == 1);
//     }
//     
//     // Test wide to narrow preserves input
//     {
//         ch_stream<ch_uint<32>> wide_input;
//         auto result = stream_wide_to_narrow<ch_uint<8>, ch_uint<32>, 4>(wide_input);
//         
//         REQUIRE(result.input.payload.width == 32);
//         REQUIRE(result.input.valid.width == 1);
//         REQUIRE(result.input.ready.width == 1);
//     }
// }
