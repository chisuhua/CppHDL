#include "catch_amalgamated.hpp"

#include "utils/source_info.h"
#include <iostream>

using namespace ch::utils;

// 测试source_location的基本功能
TEST_CASE("source_location basic functionality", "[source_info]") {
    SECTION("Construction from std::source_location") {
        source_location loc;
        // 由于现在必须使用 std::source_location 构造，无法直接测试空值
        REQUIRE(loc.file_name() != nullptr); // file_name 应该不为nullptr
    }

    SECTION(
        "Parameterized construction is no longer supported, using default") {
        source_location loc;                 // 使用默认方式构造
        REQUIRE(loc.file_name() != nullptr); // file_name 应该不为nullptr
    }
}

// 测试source_info的基本功能
TEST_CASE("source_info basic functionality", "[source_info]") {
    SECTION("Default construction") {
        source_info info;
        REQUIRE(info.empty() == false);
        REQUIRE(info.hasName() == false);
        REQUIRE(info.hasLocation() == true);
    }

    SECTION("Construction with location") {
        source_location loc; // 使用默认方式构造
        source_info info(loc);
        REQUIRE(info.hasLocation() == true);
        REQUIRE(info.hasName() == false);
        REQUIRE(info.sloc().file_name() != nullptr);
    }

    SECTION("Construction with name") {
        source_info info(source_location(), "test_name");
        REQUIRE(info.hasName() == true);
        REQUIRE(info.hasLocation() == true); // 现在总是有位置信息
        REQUIRE(info.name() == "test_name");
    }
}

// 测试CH_SLOC宏功能
TEST_CASE("CH_SLOC macro functionality", "[source_info]") {
    CH_SLOC; // 使用宏定义一个source_location

    REQUIRE(sloc.file_name() != nullptr); // 文件名不应为空指针
    REQUIRE(sloc.line() > 0);             // 行号应大于0
}

// 测试CH_SRC_INFO宏功能
TEST_CASE("CH_SRC_INFO macro functionality", "[source_info]") {
    CH_SRC_INFO; // 使用宏定义一个source_info

    // 检查是否有位置信息
    REQUIRE(srcinfo.hasLocation() == true);

    // 检查srcinfo变量本身没有名称（因为CH_SRC_INFO宏定义中srcinfo不是变量名）
    REQUIRE(srcinfo.hasName() == false);
}

// 测试sloc_arg和srcinfo_arg模板
TEST_CASE("sloc_arg and srcinfo_arg templates", "[source_info]") {
    SECTION("sloc_arg construction") {
        int test_value = 100;
        CH_SLOC;

        sloc_arg<int> arg(test_value, sloc);
        REQUIRE(arg.data == test_value);
        REQUIRE(arg.sloc.file_name() != nullptr);
    }

    SECTION("srcinfo_arg construction with default") {
        int test_value = 200;
        CH_SRC_INFO;

        srcinfo_arg<int> arg(test_value, srcinfo);
        REQUIRE(arg.data == test_value);
        REQUIRE(arg.srcinfo.hasLocation() == true);
    }
}

// 测试输出流操作符
TEST_CASE("Stream output operators", "[source_info]") {
    SECTION("source_location stream output") {
        source_location loc; // 使用默认方式构造
        std::ostringstream oss;
        oss << loc;
        std::string output = oss.str();
        REQUIRE(output.find(std::string(loc.file_name())) != std::string::npos);
    }

    SECTION("source_info stream output") {
        source_location loc; // 使用默认方式构造
        source_info info(loc, "test_name");
        std::ostringstream oss;
        oss << info;
        std::string output = oss.str();
        REQUIRE(output.find("'test_name'") != std::string::npos);
    }

    SECTION("source_info stream output without name") {
        source_location loc; // 使用默认方式构造
        source_info info(loc);
        std::ostringstream oss;
        oss << info;
        std::string output = oss.str();
        REQUIRE(output.find(std::string(loc.file_name())) != std::string::npos);
    }
}