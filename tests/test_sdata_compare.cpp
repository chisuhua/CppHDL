#include "catch_amalgamated.hpp"
#include "core/types.h"

using namespace ch::core;

TEST_CASE("sdata_type and uint64_t comparison", "[sdata][comparison]") {
    // 创建一些测试数据
    sdata_type a(42, 8);  // 值为42，宽度为8位
    sdata_type b(100, 8); // 值为100，宽度为8位
    uint64_t c = 42;
    uint64_t d = 100;

    SECTION("sdata_type and uint64_t comparison") {
        // 相等比较
        REQUIRE(a == c);
        REQUIRE_FALSE(a == d);

        // 不等比较
        REQUIRE(a != d);
        REQUIRE_FALSE(a != c);

        // 小于比较
        REQUIRE(a < d);
        REQUIRE_FALSE(b < c);

        // 小于等于比较
        REQUIRE(a <= c);
        REQUIRE(a <= d);
        REQUIRE_FALSE(b <= c);

        // 大于比较
        REQUIRE(b > c);
        REQUIRE_FALSE(a > d);

        // 大于等于比较
        REQUIRE(b >= c);
        REQUIRE(a >= c);
        REQUIRE_FALSE(a >= d);
    }

    SECTION("uint64_t and sdata_type comparison") {
        // 相等比较
        REQUIRE(c == a);
        REQUIRE_FALSE(d == a);

        // 不等比较
        REQUIRE(d != a);
        REQUIRE_FALSE(c != a);

        // 小于比较
        REQUIRE(c < b);
        REQUIRE_FALSE(d < a);

        // 小于等于比较
        REQUIRE(c <= a);
        REQUIRE(c <= b);
        REQUIRE_FALSE(d <= a);

        // 大于比较
        REQUIRE(d > a);
        REQUIRE_FALSE(c > b);

        // 大于等于比较
        REQUIRE(d >= a);
        REQUIRE(c >= a);
        REQUIRE_FALSE(c >= b);
    }
}