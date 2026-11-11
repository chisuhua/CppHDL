// tests/test_rtl_manifest.cpp
//
// **端口清单（manifest）** 的判据 —— capability `rtl-emit-portability`
// 的 schema `rtl-emit-manifest-v1`（`rtl-emit-product-tooling` **C4** / **N5(a)** / **C3**）。
//
// ⚠️ **判别力**（本文件 MUST 抓住的具体缺陷）：
//   ① **字段缺失 / 语义错位** ⇒ ⇒ schema 的 10 个字段 MUST **全部**在场且语义正确
//      （尤其 `driven`（**有驱动源**）与 `effective`（**能到达 `ch_out`**）**不是**一回事
//      —— 实测踩坑：用 `get_users()` 判「被消费」⇒ ⇒ **恒为 true** ⇒ ⇒ 零信息量）；
//   ② **C3 悬空输入诊断缺失** ⇒ ⇒ 子组件里**没人接**的 `ch_in` MUST 被点名；
//   ③ **清单不确定性** ⇒ ⇒ 同一设计两次生成 MUST **逐字节相同**（C5 的 F2 判据）。
//
// ⚠️ **期望值 MUST 手推**（禁抄仿真输出）。

#include "catch_amalgamated.hpp"

#include "ch.hpp"
#include "component.h"
#include "rtl_emit/manifest.h"

#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace ch::core;

namespace {

// ── DUT：子组件（一个**被接上**的端口 + 一个**悬空**的端口）────────────
class Leaf : public ch::Component {
public:
    __io(
        ch_in<ch_uint<8>>  data_in{"data_in"};    // ⇒ **被接上** ⇒ 无诊断
        ch_in<ch_uint<8>>  data_hole{"data_hole"}; // ⇒ ⚠️ **没人接** ⇒ MUST 被诊断
        ch_out<ch_uint<8>> data_out{"data_out"};
    )

    Leaf(ch::Component *parent = nullptr, const std::string &name = "leaf")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        io().data_out <<= io().data_in;
    }
};

// ── DUT：顶层（3 个具名端口）──────────────────────────────────────────
class Top : public ch::Component {
public:
    __io(
        ch_in<ch_bool>      prog_we{"prog_we"};     // ⚠️ **没人用** ⇒ driven=false / effective=false
        ch_in<ch_uint<8>>   prog_data{"prog_data"}; // ⇒ 接到 leaf ⇒ effective=true
        ch_out<ch_uint<8>>  result{"result"};       // ⇒ 输出
    )

    ch::ch_module<Leaf> leaf{"leaf"};

    Top(ch::Component *parent = nullptr, const std::string &name = "top")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        leaf.io().data_in <<= io().prog_data;
        io().result <<= leaf.io().data_out;
    }
};

// ── 工具：建一个**建好**的顶层组件（裸组件 ⇒ 必须显式 `build()`）──────
struct Built {
    std::unique_ptr<ch::core::context> ctx;
    std::unique_ptr<ch::Component> root;
};

Built build_top() {
    Built b;
    b.ctx = std::make_unique<ch::core::context>("manifest_test");
    {
        ctx_swap swap(b.ctx.get());
        b.root = std::make_unique<Top>(nullptr, "top");
        // ⚠️ **必须** `build()`：裸组件没人替它调 ⇒ ⇒ 不调 ⇒ 端口根本不建
        b.root->build(b.ctx.get());
    }
    return b;
}

const ch::rtl_emit::RtlPortEntry *find_port(const ch::rtl_emit::RtlManifest &m,
                                           const std::string &name) {
    for (const auto &p : m.ports) {
        if (p.name == name) {
            return &p;
        }
    }
    return nullptr;
}

std::string to_json(const ch::rtl_emit::RtlManifest &m) {
    std::ostringstream oss;
    ch::rtl_emit::write_rtl_manifest_json(m, oss);
    return oss.str();
}

}   // namespace

// ============================================================================
// 判据 1：schema 的 **10 个字段** MUST 全部在场且语义正确
// ============================================================================
TEST_CASE("manifest: 10 个字段 MUST 全部在场且语义正确", "[rtl][manifest]") {
    Built b = build_top();
    const auto m = ch::rtl_emit::build_rtl_manifest(*b.root, b.ctx.get());

    REQUIRE(m.schema == "rtl-emit-manifest-v1");
    REQUIRE(m.root == "top");
    REQUIRE_FALSE(m.ports.empty());

    // ── `top_prog_we`：**没人用** ⇒ driven=false（无驱动源）+ effective=false ──
    const auto *we = find_port(m, "top_prog_we");
    REQUIRE(we != nullptr);
    REQUIRE(we->dir == "input");
    REQUIRE(we->width == 1);
    REQUIRE(we->role == "data");
    // 🔴 `driven` = **有驱动源**（不是「有使用者」⇒ ⇒ 用后者会**恒为 true**）
    REQUIRE(we->driven == false);
    // 🔴 `effective` = **能到达**某个 `ch_out` ⇒ ⇒ 没人用 ⇒ ⇒ 到不了
    REQUIRE(we->effective == false);
    REQUIRE(we->owner == "top");
    REQUIRE(we->is_top_io == true);
    REQUIRE(we->cpp_member == "prog_we");
    // `semantic` 在 v1 **无生产者** ⇒ ⇒ 恒 null
    REQUIRE_FALSE(we->semantic.has_value());

    // ── `top_prog_data`：**没人接**（顶层 input 天然无内部驱动）但**值能到输出** ──
    const auto *pd = find_port(m, "top_prog_data");
    REQUIRE(pd != nullptr);
    REQUIRE(pd->driven == false);       // ⚠️ 顶层 input 没有内部 driver
    REQUIRE(pd->effective == true);     // ⇒ 但它的值能到 `top_result`
    REQUIRE(pd->cpp_member == "prog_data");

    // ── `top_leaf_data_in`：**被接上** ⇒ driven=true + effective=true ──
    const auto *din = find_port(m, "top_leaf_data_in");
    REQUIRE(din != nullptr);
    REQUIRE(din->driven == true);
    REQUIRE(din->effective == true);
    REQUIRE(din->owner == "top.leaf");
    REQUIRE(din->is_top_io == false);   // ⚠️ **不是**根组件自有 IO
    REQUIRE(din->cpp_member == "data_in");

    // ── `top_result`：输出 ⇒ driven=true（有驱动源）+ effective=true（它就是可观测面）──
    const auto *rs = find_port(m, "top_result");
    REQUIRE(rs != nullptr);
    REQUIRE(rs->dir == "output");
    REQUIRE(rs->driven == true);
    REQUIRE(rs->effective == true);
    REQUIRE(rs->is_top_io == true);
}

// ============================================================================
// 判据 2：**`driven` 与 `effective` MUST 是两个独立字段**（不可互相顶替）
// ============================================================================
// ⚠️ 理由（schema 的 `ext_clk` 样例）：「有连线但无功能」这一类
//   **单布尔表达不了** ⇒ ⇒ 必须拆两字段。
TEST_CASE("manifest: driven 与 effective MUST 是两个独立字段", "[rtl][manifest]") {
    Built b = build_top();
    const auto m = ch::rtl_emit::build_rtl_manifest(*b.root, b.ctx.get());

    // `top_prog_data`：driven=false 但 effective=true ⇒ ⇒ **两字段不同向**
    const auto *pd = find_port(m, "top_prog_data");
    REQUIRE(pd != nullptr);
    REQUIRE(pd->driven != pd->effective);

    // `top_leaf_data_in`：两者都 true ⇒ ⇒ 反向：两字段**不恒等**
    const auto *din = find_port(m, "top_leaf_data_in");
    REQUIRE(din != nullptr);
    REQUIRE(din->driven == true);
    REQUIRE(din->effective == true);
}

// ============================================================================
// 判据 3（C3）：子组件里**没人接**的 `ch_in` MUST 被诊断点名
// ============================================================================
TEST_CASE("manifest: 悬空输入 MUST 被诊断点名（只诊断·不动端口面）", "[rtl][manifest]") {
    Built b = build_top();
    const auto m = ch::rtl_emit::build_rtl_manifest(*b.root, b.ctx.get());

    REQUIRE_FALSE(m.diagnostics.empty());
    const ch::rtl_emit::RtlDiagnostic *hole = nullptr;
    for (const auto &d : m.diagnostics) {
        if (d.kind == "dangling_input") {
            hole = &d;
            break;
        }
    }
    REQUIRE(hole != nullptr);
    // ⚠️ MUST **点名**：端口名 + 归属组件
    REQUIRE(hole->port.find("data_hole") != std::string::npos);
    REQUIRE(hole->owner == "top.leaf");

    // 🔴 反向：**被接上**的 `data_in` MUST **不在**诊断里
    for (const auto &d : m.diagnostics) {
        REQUIRE(d.port.find("data_in") == std::string::npos);
    }
}

// ============================================================================
// 判据 4：**确定性**（C5 的 F2 判据）—— 同一设计两次生成 MUST 逐字节相同
// ============================================================================
TEST_CASE("manifest: 清单 MUST 确定性（两次生成逐字节相同）", "[rtl][manifest]") {
    Built b1 = build_top();
    const auto m1 = ch::rtl_emit::build_rtl_manifest(*b1.root, b1.ctx.get());
    const std::string j1 = to_json(m1);

    Built b2 = build_top();
    const auto m2 = ch::rtl_emit::build_rtl_manifest(*b2.root, b2.ctx.get());
    const std::string j2 = to_json(m2);

    REQUIRE(j1 == j2);
    // ⚠️ 端口**按名字排序** ⇒ ⇒ 清单本身也是有序的
    for (std::size_t i = 1; i < m1.ports.size(); ++i) {
        REQUIRE(m1.ports[i - 1].name < m1.ports[i].name);
    }
}

// ============================================================================
// 判据 5：JSON 形状 MUST 含全部 10 个字段名（消费方按**字段名**取）
// ============================================================================
TEST_CASE("manifest: JSON MUST 含 schema 的全部字段名", "[rtl][manifest]") {
    Built b = build_top();
    const auto m = ch::rtl_emit::build_rtl_manifest(*b.root, b.ctx.get());
    const std::string j = to_json(m);

    for (const char *field : {"\"name\"", "\"dir\"", "\"width\"", "\"role\"",
                              "\"driven\"", "\"effective\"", "\"owner\"",
                              "\"is_top_io\"", "\"cpp_member\"", "\"semantic\""}) {
        CAPTURE(field);
        REQUIRE(j.find(field) != std::string::npos);
    }
    REQUIRE(j.find("\"schema\": \"rtl-emit-manifest-v1\"") != std::string::npos);
    REQUIRE(j.find("\"diagnostics\"") != std::string::npos);
}
