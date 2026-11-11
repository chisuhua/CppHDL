// tests/test_port_name_named_ctor.cpp
//
// `port-naming-ratchet-2026-11` **N1** 的判据：__io 内**具名构造**的用法约定 + 端到端探针。
//
// ⚠️ **为什么必须单独成文件**（不并入 `test_component_naming.cpp`）：
//   后者判的是「**组件路径**的拼接口径」（ADR-059 D3/D4）⇒ ⇒ 判别力 = 前缀重复。
//   本文件判的是「**端口名**的来源」⇒ ⇒ 判别力 = 产物端口名 ⇒ ⇒ 两者是**不同的缺陷族**
//   ⇒ ⇒ 混在一起会让「哪个缺陷族转红」不可归因。
//
// ⚠️ **判别力**（本文件 MUST 抓住的具体缺陷）：
//   具名构造的**调用点实测 = 0**
//   （`grep -rnE 'ch_(in|out)<[^>]*>[[:space:]]+[a-z_][a-z0-9_]*\{"' include/ examples/`
//   ⇒ **0 命中**）⇒ ⇒ 这条路径**从未被端到端跑过**⇒ ⇒ 两种失败形态
//   ①「构造在 `__io` 内**不生效**」②「生效了但**名字没进图**」**必须先被排除**
//   ⇒ ⇒ 否则 N3（名字稳定性）建在沙上。
//   ⇒ ⇒ 本文件的判据 = **产物端口名**（不是节点名）—— 因为 manifest 的 `name` 字段
//   来自**产物**，不是来自节点 ⇒ ⇒ 判据必须落在产物上。
//
// ⚠️ **用法约定（三条，缺一不可）**：
//   ① `__io` 内**成员声明**带 NSDMI：`ch_in<ch_bool> prog_we{"prog_we"};`
//   ② **必须** override `create_ports()` 并 `new (io_storage_) io_type;`
//      —— 否则 `io_storage_` 永不初始化 ⇒ ⇒ **端口根本不会被创建**
//      （实测：`ch_device<未 override 者>` ⇒ 端口不存在 ⇒ 读 `impl()` 是未定义行为）
//   ③ **名字串 = 唯一真源** ⇒ ⇒ 标识符**不参与**命名 ⇒ ⇒ MUST 与标识符**逐字相同**
//      ⇒ ⇒ 这一条由 `scripts/verify_port_naming.sh` 的 P1 静态护栏兜住
//      （理由见下「反向判据」）。

#include "catch_amalgamated.hpp"

#include "ch.hpp"
#include "codegen_verilog.h"
#include "component.h"
#include "simulator.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace ch::core;

namespace {

// ── DUT：三个具名端口（input bool / output word / input byte）──────────────
class NamedPorts : public ch::Component {
public:
    __io(
        ch_in<ch_bool>      prog_we{"prog_we"};
        ch_out<ch_uint<32>> prog_addr{"prog_addr"};
        ch_in<ch_uint<8>>   prog_len{"prog_len"};
    )

    NamedPorts(ch::Component *parent = nullptr, const std::string &name = "named")
        : ch::Component(parent, name) {}

    // ⚠️ 用法约定 ②：**必须** override 并 `new (io_storage_) io_type;`
    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {}
};

// ── DUT：故意让「标识符 ≠ 名字串」⇒ ⇒ 观察**哪个**胜出 ──────────────────
// ⚠️ 这是**反向判据**：若哪天框架改成「标识符优先」⇒ ⇒ 本用例转红 ⇒ ⇒ 说明
//   「名字串 = 唯一真源」这条约定**失效** ⇒ ⇒ N2 的静态护栏 P1 也就失去依据。
class MismatchedName : public ch::Component {
public:
    __io(
        ch_in<ch_bool>      mismatched{"totally_different"};
    )

    MismatchedName(ch::Component *parent = nullptr, const std::string &name = "mism")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {}
};

// ── 产物读取工具 ────────────────────────────────────────────────────────
std::string read_file(const std::string &path) {
    std::ifstream f(path);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::string emit_flat(ch::Component &root, const std::string &path) {
    ch::toVerilog(path, root.context());
    return read_file(path);
}

}   // namespace

// ============================================================================
// 判据 1：具名构造在 `__io` 内**生效**（节点名 = <组件路径>.<显式名>）
// ============================================================================
TEST_CASE("port naming: 具名构造在 __io 内 MUST 生效（节点名带显式名）",
          "[port][naming][named-ctor]") {
    ch::ch_device<NamedPorts> dev;
    auto &io = dev.instance().io();

    REQUIRE(io.prog_we.is_valid());
    REQUIRE(io.prog_addr.is_valid());
    REQUIRE(io.prog_len.is_valid());

    // 节点名 = 组件路径 + "." + 显式名（**不是** `unnamed_input`）
    const std::string we_name = io.prog_we.impl()->name();
    CAPTURE(we_name);
    REQUIRE(we_name == "top.prog_we");
    REQUIRE(io.prog_addr.impl()->name() == "top.prog_addr");
    REQUIRE(io.prog_len.impl()->name() == "top.prog_len");

    // 反向：MUST NOT 落到默认名
    REQUIRE(we_name.find("unnamed_input") == std::string::npos);
}

// ============================================================================
// 判据 2（**核心**）：**产物端口名** MUST 是显式名（不是 unnamed_input）
// ============================================================================
// ⚠️ 为什么判据落在**产物**而不是节点：
//   manifest 的 `name` 字段来自**产物** ⇒ ⇒ 若显式名只进了节点、没进产物
//   ⇒ ⇒ manifest 的 `name` 仍然是 `top_unnamed_input` ⇒ ⇒ 工具链**拿不到**可读的端口名
//   ⇒ ⇒ 用户诉求「端口名不可用」**根本没被解决**。
TEST_CASE("port naming: 具名构造 MUST 进产物端口表（不是 unnamed_input）",
          "[port][naming][named-ctor]") {
    const std::string v_path = "test_port_name_named_ctor.v";
    std::string v;
    {
        ch::ch_device<NamedPorts> dev;
        v = emit_flat(dev.instance(), v_path);
    }
    REQUIRE_FALSE(v.empty());

    // 顶层端口表 MUST 含显式名（扁平产物 = 单 module 全局命名空间 ⇒ 前缀 `top_`）
    REQUIRE(v.find("input  top_prog_we") != std::string::npos);
    REQUIRE(v.find("output [31:0] top_prog_addr") != std::string::npos);
    REQUIRE(v.find("top_prog_len") != std::string::npos);

    // 🔴 反向判据：MUST NOT 出现默认名
    REQUIRE(v.find("top_unnamed_input") == std::string::npos);
    REQUIRE(v.find("unnamed_input") == std::string::npos);

    // 端口名**逐字**等于 `<前缀><显式名>`（不是 `<前缀><显式名>_1` 之类）
    // ⚠️ 「零序号」由 N3 的判据钉（用**期望字符串比对**，不用「名字不含 `_N`」）
    REQUIRE(v.find("top_prog_we_1") == std::string::npos);
    REQUIRE(v.find("top_prog_addr_1") == std::string::npos);
}

// ============================================================================
// 判据 3（**反向**）：标识符 ≠ 名字串 ⇒ ⇒ **名字串胜出**（唯一真源）
// ============================================================================
// ⚠️ 理由（oracle 根因 F-C）：框架**无反射** ⇒ ⇒ 运行期唯一存活的是**显式命名串**
//   ⇒ ⇒ manifest 的 `cpp_member` **只能是它** ⇒ ⇒ 「端口名可解析到 C++ 成员名」
//   **必须**由静态护栏兜（`scripts/verify_port_naming.sh` 的 P1 判「标识符 == 名字串」）
//   ⇒ ⇒ 若本用例转红（标识符开始胜出），那条护栏就失去依据 ⇒ ⇒ MUST 同步改约定。
TEST_CASE("port naming: 名字串 MUST 胜出（标识符不参与命名）", "[port][naming][named-ctor]") {
    ch::ch_device<MismatchedName> dev;
    const std::string n = dev.instance().io().mismatched.impl()->name();
    CAPTURE(n);
    // 名字**串**胜出 ⇒ ⇒ 节点名以 `totally_different` 结尾（**不是** `mismatched`）
    REQUIRE(n == "top.totally_different");
}

// ============================================================================
// 判据 4：不同组件实例 ⇒ 端口名带**各自的组件路径**（跨组件消歧）
// ============================================================================
// ⚠️ 为什么重要（N4 的理由）：扁平产物是**单 module 全局命名空间**
//   ⇒ ⇒ 前缀/路径是**唯一消歧手段** ⇒ ⇒ 「顶层端口名 == C++ 成员名」对多数端口
//   **结构上不可满足** ⇒ ⇒ 用户诉求的正确归宿 = 「名字可解析到 C++ 成员」
//   （manifest `cpp_member`）+ 「名字稳定」，**不是**「字面同名」。
TEST_CASE("port naming: 端口名 MUST 带组件路径（跨组件消歧）", "[port][naming][named-ctor]") {
    ch::ch_device<NamedPorts> dev;
    const std::string we_name = dev.instance().io().prog_we.impl()->name();
    CAPTURE(we_name);
    // 组件路径前缀（`top` = `ch_device` 的根实例名）⇒ ⇒ 端口名**含**它
    REQUIRE(we_name.rfind("top.", 0) == 0);
    // ⚠️ 「顶层端口名 == C++ 成员名」**不成立**（`top_prog_we` ≠ `prog_we`）
    //   ⇒ ⇒ 这不是缺陷，是**契约**（N4 / R1-a：MUST 可**解析**到成员名，不是**字面相同**）
    REQUIRE(we_name != "prog_we");
}

// ============================================================================
// 判据 5（**N3 核心**）：显式命名端口的名字 MUST 对「图的无关改动」**稳定**
// ============================================================================
// ⚠️ **为什么这是 N3 要修的缺陷**：
//   **未具名**端口的 Verilog 名带一个**序号**，而序号 = 「eval-list 里排在它前面的
//   **同名**节点个数」⇒ ⇒ **加一个无关节点**就可能让**后面**的端口全体**后移一位**
//   ⇒ ⇒ 「按名引用」的外部资产（TB / wrapper / 文档）**静默失配**。
//   ⇒ ⇒ **显式命名**的端口 MUST **绕开**计数器 ⇒ ⇒ 名字只由「组件路径 + 用户串」决定。
//
// ⚠️ **怎么造出「无关改动」**：
//   在同一组件里放一个**同名**的内部寄存器（`ch_reg<...> prog_we{"prog_we"}`）
//   ⇒ ⇒ 它的节点名也是 `top.prog_we` ⇒ ⇒ **base 相同** ⇒ ⇒ 计数器会抢名。
//   ⇒ ⇒ 修复前：端口被挤成 `top_prog_we_1`（**不稳定**）；修复后：端口仍是 `top_prog_we`。
//
// ⚠️ **判据用「期望字符串比对」**（metis 订正 ②）：
//   「零序号」MUST NOT 靠「名字不含 `_N`」判定（**合法成员名本身可以以 `_2` 结尾**
//   ⇒ ⇒ 那种判据不可判）⇒ ⇒ MUST 比对**确切期望串**：
//   端口名 == `top_prog_we`（命中）**且** `top_prog_we_1`（计数器变体）不存在。
namespace {

// 与 `NamedPorts` 同款端口，但**多一个同名内部寄存器** ⇒ ⇒ base 相同 ⇒ ⇒ 抢名
//
// ⚠️ **为什么同名寄存器 MUST 是「成员」而不是 `describe()` 里的局部变量**：
//   · **成员**的节点在**成员初始化阶段**创建 ⇒ ⇒ **早于** `create_ports()`
//     ⇒ ⇒ 端口节点**排在它后面** ⇒ ⇒ 计数器会把它挤到 `top_prog_we_1`
//     ⇒ ⇒ **修复前**端口名 = `top_prog_we_1`（**不稳定**）；修复后 = `top_prog_we`；
//   · `describe()` 里的局部 `ch_reg` 在**端口之后**创建 ⇒ ⇒ 端口本来就拿 counter 0
//     ⇒ ⇒ **该形态没有判别力**（实测：把它放进 `describe()` ⇒ 变异不转红）。
class NamedPortsWithCollision : public ch::Component {
public:
    __io(
        ch_in<ch_bool>      prog_we{"prog_we"};
        ch_out<ch_uint<32>> prog_addr{"prog_addr"};
    )

    // ⚠️ **成员** ⇒ ⇒ 节点名 `top.prog_we` ⇒ 与端口**同 base**
    //   ⇒ ⇒ 且它在**端口之前**创建 ⇒ ⇒ 计数器会抢名（修复前端口被挤成 `_1`）
    ch_reg<ch_bool> prog_we_internal{false, "prog_we"};

    NamedPortsWithCollision(ch::Component *parent = nullptr,
                            const std::string &name = "named")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        io().prog_addr <<= io().prog_we;
    }
};

}   // namespace

TEST_CASE("port naming: 显式命名端口的名字 MUST 对无关改动稳定（N3 · 绕开计数器）",
          "[port][naming][named-ctor]") {
    const std::string v_path = "test_port_name_named_ctor_stable.v";

    // ── A：基准设计（无同名内部节点）────────────────────────────────────
    std::string v_a;
    {
        ch::ch_device<NamedPorts> dev;
        v_a = emit_flat(dev.instance(), v_path);
    }
    // ── B：加了**同名**内部节点（无关改动）─────────────────────────────
    std::string v_b;
    {
        ch::ch_device<NamedPortsWithCollision> dev;
        v_b = emit_flat(dev.instance(), v_path);
    }
    REQUIRE_FALSE(v_a.empty());
    REQUIRE_FALSE(v_b.empty());

    // ⚠️ **期望字符串比对**（不是「名字不含 `_N`」）
    // A：端口名 == `top_prog_we`，且**没有**计数器变体
    REQUIRE(v_a.find("input  top_prog_we") != std::string::npos);
    REQUIRE(v_a.find("input  top_prog_we_1") == std::string::npos);
    REQUIRE(v_a.find("input  top_prog_we_2") == std::string::npos);

    // B：**关键** ⇒ ⇒ 加了同名内部节点后，**端口**的名字 MUST **一字不变**
    REQUIRE(v_b.find("input  top_prog_we") != std::string::npos);
    // 🔴 反向：MUST NOT 出现「端口被挤到计数器变体上」的形态
    REQUIRE(v_b.find("input  top_prog_we_1") == std::string::npos);
    REQUIRE(v_b.find("input  top_prog_we_2") == std::string::npos);

    // ⚠️ **同名内部寄存器**（`ch_reg<...> prog_we`）**理应**拿到 `top_prog_we_1`
    //   ⇒ ⇒ 它「让出」了 `top_prog_we` 给端口 ⇒ ⇒ 这正是「显式名绕开计数器」的效果
    //   ⇒ ⇒ ⚠️ **不可**断言「整个产物里没有 `top_prog_we_1`」（那是**内部节点**的名字）
    REQUIRE(v_b.find("top_prog_we_1") != std::string::npos);
}
