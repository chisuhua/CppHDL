// tests/test_port_name_parity.cpp
//
// `port-name-literal-parity-2026-11` **P1**：把「三探针」**逐字搬进仓**。
//
// ⚠️ **这些用例钉的是「当前行为」（现状钉），不是「目标行为」** ——
//    若 route (c)（根实例名清空 ⇒ 产出**裸** `prog_addr`）落地
//    ⇒ ⇒ 本组 `Ratchet*` MUST 被**改写**为目标语义
//    ⇒ ⇒ 详见 `.rddf/improvements/port-name-literal-parity-2026-11.md` 的 §A.1⑦。
//
// ⚠️ **⑤（目标钉「根端口裸名」）不进本文件** ⇒ ⇒ 落
//    `.rddf/checks/arch-residual-registry.md`（含解除条件 + 触发条件）
//    ⇒ ⇒ 理由：「**未运行的验证不可判**」⇒ ⇒ SKIP **零判别力**
//    ⇒ ⇒ 残差登记有 fail-closed 护栏 ⇒ ⇒ **会主动催办**。
//
// ⚠️ **断言层级上到「发射出的 module 文本」**（oracle 建议 3 硬要求 ①）——
//    只断言节点名 ⇒ ⇒ **命名层与产物层**可能分叉 ⇒ ⇒ 命名变了、产物没变 ⇒ ⇒ 漏判。
//    ⇒ ⇒ ①④ MUST 断言 `verilogwriter` 的输出文本。
//
// ⚠️ **exact-string 断言的顺序敏感性**：本文件的断言用**确切字符串**（如 `top_prog_addr`）
//    ⇒ ⇒ 它对**节点创建顺序**敏感 ⇒ ⇒ **fixture 只含本用例所需节点**
//    （多一个节点就可能让计数器变体漂到别的断言上）。
//
// ⚠️ **变异自证（MUST 是「正向变异」）**（metis 订正）：
//    ① 的变异 = 把 `__io` 的具名成员改回匿名 ⇒ ⇒ 必须转红
//    ② 的变异 = 令 `component.cpp` 的空名 fallback 保空（或给 `lnodeimpl` 加特判）⇒ ⇒ 必须转红
//    ③ 的变异 = 强制 `build_verilog_name_table` 的 counter 恒 0 ⇒ ⇒ 必须转红
//    ⇒ ⇒ **禁**「删掉断言语句」式的假变异。

#include "catch_amalgamated.hpp"

#include "ch.hpp"
#include "codegen_naming.h"
#include "codegen_verilog.h"
#include "component.h"
#include "device.h"

#include <algorithm>
#include <memory>
#include <sstream>
#include <string>

using namespace ch;
using namespace ch::core;

namespace {

// ── fixture：一个带**具名端口**的组件 ──────────────────────────────────
class Probe : public ch::Component {
public:
    __io(
        ch_in<ch_uint<32>> prog_addr{"prog_addr"};
    )

    Probe(ch::Component *parent = nullptr, const std::string &name = "top")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {}
};

// ── fixture：**无任何 `ch_in`** 的组件（退化用例 ⑥）────────────────────
class NoPort : public ch::Component {
public:
    NoPort(ch::Component *parent = nullptr, const std::string &name = "top")
        : ch::Component(parent, name) {}

    // ⚠️ **没有 `__io`** ⇒ ⇒ 连 `io_storage_` 都没有 ⇒ ⇒ 无端口可创建
    //   ⇒ ⇒ 这正是退化用例要的条件（「根无名 + 无任何 `ch_in`」）
    void describe() override {}
};

// ── fixture：**同名兄弟**的父组件（用例 ③）────────────────────────────
class DupParent : public ch::Component {
public:
    // ⚠️ 用 **`ch_module` 成员**（框架的**延迟 build** 机制走这条）
    //   ⇒ ⇒ `create_child`（在 `describe()` 里）**不会**触发子组件的
    //   `create_ports()` ⇒ ⇒ 子组件的端口**根本不会被创建**（实测）
    //   ⇒ ⇒ 且成员形态是仓内**既有**模式（ADR-059 D2′ 的延迟 build 就是为它设计的）
    // ⚠️ **两个实例都叫 `dup`** ⇒ ⇒ 两者端口的 base 相同 ⇒ ⇒ 计数器必须区分
    ch::ch_module<Probe> dup1{"dup"};
    ch::ch_module<Probe> dup2{"dup"};

    DupParent(ch::Component *parent = nullptr, const std::string &name = "top")
        : ch::Component(parent, name) {}

    // ⚠️ 父组件**没有** `__io` ⇒ ⇒ 无端口 ⇒ ⇒ 不需要 `create_ports` override

    void describe() override {}
};

// ── 工具：发射出的 module 文本（扁平产物）──────────────────────────────
std::string emit_module_text(context *ctx) {
    std::ostringstream oss;
    verilogwriter writer(ctx);
    writer.print(oss);
    return oss.str();
}

// ── 工具：命名表（节点 → Verilog 名）──────────────────────────────────
std::unordered_map<const lnodeimpl *, std::string> name_table(context *ctx) {
    return build_verilog_name_table(ctx->get_eval_list());
}

}   // namespace

// ============================================================================
// `Ratchet*` ①：__io 内具名构造生效 ⇒ 节点名 `top.prog_addr` /
//                Verilog 名 `top_prog_addr`
// ============================================================================
// ⚠️ **N5(c) 原始问句**：「把 `ch_device` 的根实例名清空后，`lnodeimpl` 的
//    `parent == nullptr` 分支是否产出**裸** `prog_addr`？」
//    ⇒ ⇒ 本用例钉的是**直接构造**（根名 `"top"`）下的**现状**：
//    节点名 = `<组件路径>.<显式名>`，Verilog 名 = `<前缀>_<显式名>`。
TEST_CASE("Ratchet 1: 具名构造生效 ⇒ 节点名 top.prog_addr / Verilog 名 top_prog_addr",
          "[port][naming][parity][ratchet]") {
    auto ctx = std::make_unique<ch::core::context>("ratchet1");
    ch::core::ctx_swap swap(ctx.get());

    Probe probe(nullptr, "top");
    probe.create_ports();

    // ── 命名层 ────────────────────────────────────────────────────────
    const std::string node_name = probe.io().prog_addr.impl()->name();
    CAPTURE(node_name);
    REQUIRE(node_name == "top.prog_addr");

    // ── 产物层（**必须**断言发射文本 ⇒ ⇒ 命名层与产物层不能分叉）──────
    const auto table = name_table(ctx.get());
    const auto it = table.find(probe.io().prog_addr.impl());
    REQUIRE(it != table.end());
    REQUIRE(it->second == "top_prog_addr");

    const std::string v = emit_module_text(ctx.get());
    REQUIRE(v.find("top_prog_addr") != std::string::npos);
    // ⚠️ 反向：MUST NOT 落到默认名
    REQUIRE(v.find("top_unnamed_input") == std::string::npos);
}

// ============================================================================
// `Ratchet*` ②：根实例名置空 ⇒ 节点名 `unnamed.prog_addr`
//                （**不是** `prog_addr`，**也不是** `.prog_addr`）
// ============================================================================
// ⚠️ **N5(c) 原始问句**：「根实例名置空 ⇒ 产出的是 `.prog_addr` → `_prog_addr` 吗？」
//    ⇒ ⇒ **实测 = 第三形态 `unnamed.prog_addr`** ⇒ ⇒ 那条回退条件**永不命中**
//    ⇒ ⇒ ⇒ 这就是 route (c) **在当前命名机制下不可实现**的直接证据。
//    ⇒ ⇒ ⚠️ 本用例钉的是**现状**（现状钉）⇒ ⇒ 若 route (c) 落地 ⇒ ⇒ MUST 改写。
TEST_CASE("Ratchet 2: 根实例名置空 ⇒ 节点名 unnamed.prog_addr（不是裸名）",
          "[port][naming][parity][ratchet]") {
    auto ctx = std::make_unique<ch::core::context>("ratchet2");
    ch::core::ctx_swap swap(ctx.get());

    // ⚠️ **空名** ⇒ ⇒ `local_name_` 被归一成 `"unnamed"`（`src/component.cpp:66`）
    Probe probe(nullptr, "");
    probe.create_ports();

    const std::string node_name = probe.io().prog_addr.impl()->name();
    CAPTURE(node_name);
    REQUIRE(node_name == "unnamed.prog_addr");
    // ⚠️ **反向**：MUST NOT 是裸名，也 MUST NOT 是 `.prog_addr`（前导点）
    REQUIRE(node_name != "prog_addr");
    REQUIRE(node_name != ".prog_addr");
    REQUIRE(node_name.rfind("unnamed.", 0) == 0);
}

// ============================================================================
// `Ratchet*` ③：同名兄弟 ⇒ 端口名含 `top_dup_prog_addr` 与 `top_dup_prog_addr_1`
// ============================================================================
// ⚠️ **N5(c) 原始问句**：「同名兄弟的端口名会不会撞？」
//    ⇒ ⇒ **会撞** ⇒ ⇒ 计数器必须区分 ⇒ ⇒ 这正是「去前缀后唯一性论证」的
//    核心难点（`port-name-literal-parity` 的 E1）。
TEST_CASE("Ratchet 3: 同名兄弟 ⇒ 端口名含 top_dup_prog_addr 与 top_dup_prog_addr_1",
          "[port][naming][parity][ratchet]") {
    auto ctx = std::make_unique<ch::core::context>("ratchet3");
    ch::core::ctx_swap swap(ctx.get());

    DupParent parent(nullptr, "top");
    parent.create_ports();
    parent.describe();

    REQUIRE(parent.child_count() == 2);

    const auto table = name_table(ctx.get());
    std::vector<std::string> names;
    for (const auto &child : parent.children()) {
        // ⚠️ `Component` 基类**没有** `io()`（它是各派生类自己的）⇒ ⇒ 向下转型
        const auto *probe = static_cast<const Probe *>(child.get());
        const auto it = table.find(probe->io().prog_addr.impl());
        REQUIRE(it != table.end());
        names.push_back(it->second);
    }
    std::sort(names.begin(), names.end());
    CAPTURE(names[0]);
    CAPTURE(names[1]);
    REQUIRE(names.size() == 2);
    REQUIRE(names[0] == "top_dup_prog_addr");
    REQUIRE(names[1] == "top_dup_prog_addr_1");
}

// ============================================================================
// `Device*` ④：`ch::ch_device<T>` 路径 ⇒ 与 ① 的直接构造**等价**
// ============================================================================
// ⚠️ **为什么必须有它**（oracle 建议 3 硬要求 · metis finding 21）：
//   ① 固化的是**直接构造**路径 ⇒ ⇒ 若**只**固化它 ⇒ ⇒ **交付路径**
//   （`include/device.h:28` 的真实 `"top"` 根名）**从未被覆盖**
//   ⇒ ⇒ 下游拿到的产品与固化的事实**可能不是同一路径**。
TEST_CASE("Device 4: ch_device 路径 MUST 与直接构造等价（覆盖真实交付路径）",
          "[port][naming][parity][device]") {
    // ── ① 的直接构造 ──────────────────────────────────────────────────
    std::string direct_node;
    std::string direct_verilog_name;
    std::string direct_module;
    {
        auto ctx = std::make_unique<ch::core::context>("device4_direct");
        ch::core::ctx_swap swap(ctx.get());
        Probe probe(nullptr, "top");
        probe.create_ports();
        direct_node = probe.io().prog_addr.impl()->name();
        const auto table = name_table(ctx.get());
        const auto it = table.find(probe.io().prog_addr.impl());
        REQUIRE(it != table.end());
        direct_verilog_name = it->second;
        direct_module = emit_module_text(ctx.get());
    }

    // ── ④ 的 `ch_device` 真实路径 ─────────────────────────────────────
    std::string device_node;
    std::string device_verilog_name;
    std::string device_module;
    {
        ch::ch_device<Probe> dev;
        device_node = dev.instance().io().prog_addr.impl()->name();
        const auto table = name_table(dev.context());
        const auto it = table.find(dev.instance().io().prog_addr.impl());
        REQUIRE(it != table.end());
        device_verilog_name = it->second;
        device_module = emit_module_text(dev.context());
    }

    // ⚠️ **等价**：节点名 / Verilog 名 / 产物文本里的端口名 MUST 一致
    REQUIRE(device_node == direct_node);
    REQUIRE(device_verilog_name == direct_verilog_name);
    REQUIRE(device_node == "top.prog_addr");
    REQUIRE(device_verilog_name == "top_prog_addr");
    REQUIRE(device_module.find("top_prog_addr") != std::string::npos);
    REQUIRE(direct_module.find("top_prog_addr") != std::string::npos);
}

// ============================================================================
// 退化用例 ⑥：根无名 + 无任何 `ch_in` ⇒ 命名表为空 ⇒ 不崩且表为空
// ============================================================================
TEST_CASE("退化 6: 根无名 + 无端口 ⇒ 命名表为空且不崩", "[port][naming][parity][degenerate]") {
    auto ctx = std::make_unique<ch::core::context>("degenerate6");
    ch::core::ctx_swap swap(ctx.get());

    NoPort probe(nullptr, "");   // ⚠️ **根无名**
    probe.create_ports();

    // ⚠️ 不崩（能走到这里就是「不崩」）
    const auto table = name_table(ctx.get());
    // ⚠️ **「命名表为空」的准确口径**：该 ctx **恒有** `default_clock` /
    //   `default_reset`（`src/core/context.cpp:114-119` ⇒ ctx 构造即建）
    //   ⇒ ⇒ 「空」只能指「**没有端口条目**」⇒ ⇒ 逐条查表里**没有** input/output。
    size_t port_entries = 0;
    for (const auto &kv : table) {
        const auto t = kv.first->type();
        if (t == ch::core::lnodetype::type_input ||
            t == ch::core::lnodetype::type_output) {
            ++port_entries;
        }
    }
    REQUIRE(port_entries == 0);

    // 发射也不崩（空模块）
    const std::string v = emit_module_text(ctx.get());
    REQUIRE(v.find("module top") != std::string::npos);
}
