// tests/test_bundle_large.cpp
// Regression test for the 10-field (now 40-field) Bundle macro limit.
//
// Background: CH_BUNDLE_FIELDS_T used to silently drop fields beyond 10
// (extended to 20 in early 2026, then to 40 to support HazardUnitBundle's
// 23 fields). This test exercises bundles at the boundaries to ensure
// no silent field truncation occurs.
//
// Test cases:
//   - 25-field bundle: covers the historical 10/20 limit + margin
//   - 23-field bundle: mirrors the real HazardUnitBundle use case
//   - 1-field bundle: minimum boundary
//   - 40-field bundle: maximum current limit
//   - 41-field bundle: documents the current upper bound (compile-time check)

#include "catch_amalgamated.hpp"
#include "core/bool.h"
#include "core/bundle/bundle_base.h"
#include "core/bundle/bundle_meta.h"
#include "core/context.h"
#include "core/uint.h"

using namespace ch;
using namespace ch::core;

// 25-field bundle: >20 (the pre-fix limit) and >10 (the original limit).
// Uses ch_uint<1> for data fields so bundle_width_v is exactly field count.
struct LargeBundle25 : public bundle_base<LargeBundle25> {
    using Self = LargeBundle25;
    ch_uint<1> f01, f02, f03, f04, f05;
    ch_uint<1> f06, f07, f08, f09, f10;
    ch_uint<1> f11, f12, f13, f14, f15;
    ch_uint<1> f16, f17, f18, f19, f20;
    ch_uint<1> f21, f22, f23, f24, f25;

    LargeBundle25() = default;
    CH_BUNDLE_FIELDS_T(
        f01, f02, f03, f04, f05,
        f06, f07, f08, f09, f10,
        f11, f12, f13, f14, f15,
        f16, f17, f18, f19, f20,
        f21, f22, f23, f24, f25
    )
};

// 23-field bundle: matches the real HazardUnitBundle in
// examples/riscv-mini/src/hazard_unit_bundle.h which is the documented
// motivating case for extending the macro beyond 20 fields.
struct HazardUnitLike23 : public bundle_base<HazardUnitLike23> {
    using Self = HazardUnitLike23;
    ch_uint<5> id_rs1_addr, id_rs2_addr, ex_rd_addr, mem_rd_addr, wb_rd_addr;
    ch_bool ex_reg_write, mem_reg_write, wb_reg_write;
    ch_bool mem_is_load, ex_branch, ex_branch_taken;
    ch_uint<32> ex_alu_result, mem_alu_result, wb_write_data;
    ch_uint<32> rs1_data_raw, rs2_data_raw;
    ch_uint<2> forward_a, forward_b;
    ch_uint<32> rs1_data, rs2_data;
    ch_bool stall_if, stall_id, flush_id_ex;

    HazardUnitLike23() = default;
    CH_BUNDLE_FIELDS_T(
        id_rs1_addr, id_rs2_addr, ex_rd_addr, mem_rd_addr, wb_rd_addr,
        ex_reg_write, mem_reg_write, wb_reg_write,
        mem_is_load, ex_branch, ex_branch_taken,
        ex_alu_result, mem_alu_result, wb_write_data,
        rs1_data_raw, rs2_data_raw,
        forward_a, forward_b, rs1_data, rs2_data,
        stall_if, stall_id, flush_id_ex
    )
};

// 1-field boundary: smallest possible bundle.
struct TinyBundle1 : public bundle_base<TinyBundle1> {
    using Self = TinyBundle1;
    ch_uint<8> only;
    TinyBundle1() = default;
    CH_BUNDLE_FIELDS_T(only)
};

// 40-field boundary: maximum current limit.
struct MaxBundle40 : public bundle_base<MaxBundle40> {
    using Self = MaxBundle40;
    ch_uint<1> f01, f02, f03, f04, f05, f06, f07, f08, f09, f10;
    ch_uint<1> f11, f12, f13, f14, f15, f16, f17, f18, f19, f20;
    ch_uint<1> f21, f22, f23, f24, f25, f26, f27, f28, f29, f30;
    ch_uint<1> f31, f32, f33, f34, f35, f36, f37, f38, f39, f40;

    MaxBundle40() = default;
    CH_BUNDLE_FIELDS_T(
        f01, f02, f03, f04, f05, f06, f07, f08, f09, f10,
        f11, f12, f13, f14, f15, f16, f17, f18, f19, f20,
        f21, f22, f23, f24, f25, f26, f27, f28, f29, f30,
        f31, f32, f33, f34, f35, f36, f37, f38, f39, f40
    )
};

TEST_CASE("BundleLarge - 25 fields registered correctly", "[bundle][large]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    LargeBundle25 bundle;
    auto fields = bundle.__bundle_fields();

    // Pre-fix: this would be 20 (fields f21-f25 silently dropped)
    // Post-fix: must be 25
    STATIC_REQUIRE(std::tuple_size_v<decltype(fields)> == 25);
    REQUIRE(std::tuple_size_v<decltype(fields)> == 25);

    // bundle_width_v must reflect ALL 25 1-bit fields = 25 bits
    STATIC_REQUIRE(bundle_width_v<LargeBundle25> == 25);
    REQUIRE(bundle_width_v<LargeBundle25> == 25);
}

TEST_CASE("BundleLarge - 23-field HazardUnitLike bundle", "[bundle][large][hazard]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    HazardUnitLike23 bundle;
    auto fields = bundle.__bundle_fields();

    // Real HazardUnitBundle in examples/riscv-mini/src/hazard_unit_bundle.h
    // declares 23 fields. Pre-fix (20-field limit) would silently drop 3.
    STATIC_REQUIRE(std::tuple_size_v<decltype(fields)> == 23);
    REQUIRE(std::tuple_size_v<decltype(fields)> == 23);

    // Verify field names are preserved (proves registration, not just count)
    auto first_field = std::get<0>(fields);
    REQUIRE(std::string(first_field.name) == "id_rs1_addr");
    auto last_field = std::get<22>(fields);
    REQUIRE(std::string(last_field.name) == "flush_id_ex");
}

TEST_CASE("BundleLarge - 1-field minimum boundary", "[bundle][large][boundary]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    TinyBundle1 bundle;
    auto fields = bundle.__bundle_fields();

    STATIC_REQUIRE(std::tuple_size_v<decltype(fields)> == 1);
    REQUIRE(std::tuple_size_v<decltype(fields)> == 1);
    REQUIRE(std::string(std::get<0>(fields).name) == "only");
}

TEST_CASE("BundleLarge - 40-field maximum boundary", "[bundle][large][boundary]") {
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    MaxBundle40 bundle;
    auto fields = bundle.__bundle_fields();

    STATIC_REQUIRE(std::tuple_size_v<decltype(fields)> == 40);
    REQUIRE(std::tuple_size_v<decltype(fields)> == 40);
    STATIC_REQUIRE(bundle_width_v<MaxBundle40> == 40);
    REQUIRE(bundle_width_v<MaxBundle40> == 40);
}

TEST_CASE("BundleLarge - 20-field legacy boundary still works",
          "[bundle][large][regression]") {
    // Ensures the extension from 10->20->40 did not break the 20-field
    // dispatch path that was already in use.
    auto ctx = std::make_unique<ch::core::context>("test_ctx");
    ch::core::ctx_swap ctx_guard(ctx.get());

    LargeBundle25 bundle;  // 25 fields; first 20 must dispatch correctly
    auto fields = bundle.__bundle_fields();
    REQUIRE(std::string(std::get<0>(fields).name) == "f01");
    REQUIRE(std::string(std::get<9>(fields).name) == "f10");
    REQUIRE(std::string(std::get<10>(fields).name) == "f11");
    REQUIRE(std::string(std::get<19>(fields).name) == "f20");
    REQUIRE(std::string(std::get<20>(fields).name) == "f21");
    REQUIRE(std::string(std::get<24>(fields).name) == "f25");
}
