#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "codegen_dag.h"
#include "codegen_verilog.h"
#include "dual_backend_fixture.h"
#include "hier_corpus_fixture.h"
#include "module.h"
#include "simulator.h"
#include <cstdint>

using namespace ch;
using namespace ch::core;

// 定义一个简单的测试模块
template <unsigned N> class SimpleModule : public Component {
public:
    __io(ch_in<ch_uint<N>> in_port; ch_out<ch_uint<N>> out_port;)

        SimpleModule(Component *parent = nullptr,
                     const std::string &name = "SimpleModule")
        : Component(parent, name) {}

    void create_ports() override { new (this->io_storage_) io_type; }

    void describe() override {
        // 直接连接输入到输出
        // 使用新的显式operator<<=替换赋值操作
        io().out_port <<= io().in_port;
    }
};

TEST_CASE("CH_MODULE - Basic Instantiation", "[module][basic]") {
    class Top : public Component {
    public:
        __io()

            Top(Component *parent = nullptr, const std::string &name = "top")
            : Component(parent, name) {}

        void create_ports() override { new (this->io_storage_) io_type; }

        void describe() override {
            // 在父组件中创建子模块
            ch::ch_module<SimpleModule<4>> child_module{"child_module"};
        }
    };

    ch_device<Top> device;

    // 检查子模块是否创建成功
    REQUIRE(device.instance().child_count() == 1);
}

TEST_CASE("CH_MODULE - Code Generation", "[module][codegen]") {
    class Top : public Component {
    public:
        __io(ch_in<ch_uint<4>> in_data; ch_out<ch_uint<4>> out_data;)

            Top(Component *parent = nullptr, const std::string &name = "top")
            : Component(parent, name) {}

        void create_ports() override { new (this->io_storage_) io_type; }

        void describe() override {
            ch::ch_module<SimpleModule<4>> mod{"mod"};

            // 检查子模块是否创建成功
            REQUIRE(mod.io().in_port.impl() != nullptr);
            REQUIRE(mod.io().out_port.impl() != nullptr);
            // 连接端口: 顶层输入 -> 子模块输入，子模块输出 -> 顶层输出
            // 使用新的operator<<=替代connect函数调用
            mod.io().in_port <<= io().in_data;
            io().out_data <<= mod.io().out_port;
        }
    };

    ch_device<Top> device;

    // 测试代码生成功能
    REQUIRE_NOTHROW(toVerilog("test_module_codgen.v", device.context()));
    REQUIRE_NOTHROW(toDAG("test_module_codgen.dot", device.context()));
}

// dual-backend-test-coverage (Phase 1): CH_MODULE - Simulation Value
// Transfer driven through InterpreterBackend + VerilatorBackend.
// Pass-through SimpleModule<4> (Top.io().in_data → mod → Top.io().out_data).
class ModuleValueTransferTop : public Component {
public:
    __io(ch_in<ch_uint<4>> in_data; ch_out<ch_uint<4>> out_data;)

    ModuleValueTransferTop(Component *parent = nullptr,
                           const std::string &name = "top_xfer")
        : Component(parent, name) {}

    void create_ports() override { new (this->io_storage_) io_type; }

    void describe() override {
        ch::ch_module<SimpleModule<4>> mod{"mod"};
        mod.io().in_port <<= io().in_data;
        io().out_data <<= mod.io().out_port;
    }
};


DUAL_BACKEND_TEST_PAIR(
    ModuleValueTransferTop,
    "CH_MODULE - Simulation Value Transfer",
    "[module][simulation][dual]",
    [&]() {
        for (uint64_t i = 0; i < 16; ++i) {
            sim.set_input_value(dev.instance().io().in_data, i);
            sim.tick();
            REQUIRE(sim.get_value(dev.instance().io().out_data) == i);
        }
    });

// dual-backend-test-coverage (Phase 1): CH_MODULE - Connection Between
// Child Modules driven through both backends. Two pass-through
// SimpleModule<4> chained: top.in → mod1 → mod2 → top.out.
class ModuleChainTop : public Component {
public:
    __io(ch_in<ch_uint<4>> in_data; ch_out<ch_uint<4>> out_data;)

    ModuleChainTop(Component *parent = nullptr,
                   const std::string &name = "top_chain")
        : Component(parent, name) {}

    void create_ports() override { new (this->io_storage_) io_type; }

    void describe() override {
        ch::ch_module<SimpleModule<4>> mod1{"mod1"};
        ch::ch_module<SimpleModule<4>> mod2{"mod2"};
        mod1.io().in_port <<= io().in_data;
        mod2.io().in_port <<= mod1.io().out_port;
        io().out_data <<= mod2.io().out_port;
    }
};

DUAL_BACKEND_TEST_PAIR(
    ModuleChainTop,
    "CH_MODULE - Connection Between Child Modules",
    "[module][child-connection][dual]",
    [&]() {
        for (uint64_t i = 0; i < 16; ++i) {
            sim.set_input_value(dev.instance().io().in_data, i);
            sim.tick();
            REQUIRE(sim.get_value(dev.instance().io().out_data) == i);
        }
    });

// dual-backend-test-coverage (Phase 1): CH_MODULE - Connection Between
// Different IO Directions (select-based DataProcessor chained) driven
// through both backends.
class ModuleDataProcessor : public Component {
public:
    __io(ch_in<ch_uint<4>> input; ch_out<ch_uint<4>> output;
         ch_in<bool> enable;)

    ModuleDataProcessor(Component *parent = nullptr,
                        const std::string &name = "DataProc")
        : Component(parent, name) {}

    void create_ports() override { new (this->io_storage_) io_type; }

    void describe() override {
        io().output <<= select(io().enable, io().input, 0_d);
    }
};

class ModuleIoDirectionTop : public Component {
public:
    __io(ch_in<ch_uint<4>> in_data; ch_out<ch_uint<4>> out_data;
         ch_in<bool> enable1; ch_in<bool> enable2;)

    ModuleIoDirectionTop(Component *parent = nullptr,
                         const std::string &name = "top_io_dir")
        : Component(parent, name) {}

    void create_ports() override { new (this->io_storage_) io_type; }

    void describe() override {
        ch::ch_module<ModuleDataProcessor> proc1{"proc1"};
        ch::ch_module<ModuleDataProcessor> proc2{"proc2"};
        proc1.io().input <<= io().in_data;
        proc1.io().enable <<= io().enable1;
        proc2.io().input <<= proc1.io().output;
        proc2.io().enable <<= io().enable2;
        io().out_data <<= proc2.io().output;
    }
};

DUAL_BACKEND_TEST_PAIR(
    ModuleIoDirectionTop,
    "CH_MODULE - Connection Between Different IO Directions",
    "[module][io-direction][dual]",
    [&]() {
        // Enable both processors — data passes through.
        sim.set_input_value(dev.instance().io().enable1, 1);
        sim.set_input_value(dev.instance().io().enable2, 1);
        for (uint64_t i = 0; i < 16; ++i) {
            sim.set_input_value(dev.instance().io().in_data, i);
            sim.tick();
            REQUIRE(sim.get_value(dev.instance().io().out_data) == i);
        }

        // proc2 disabled — outputs 0.
        sim.set_input_value(dev.instance().io().in_data, 5);
        sim.set_input_value(dev.instance().io().enable1, 1);
        sim.set_input_value(dev.instance().io().enable2, 0);
        sim.tick();
        REQUIRE(sim.get_value(dev.instance().io().out_data) == 0);

        // proc1 disabled — outputs 0.
        sim.set_input_value(dev.instance().io().enable1, 0);
        sim.set_input_value(dev.instance().io().enable2, 1);
        sim.tick();
        REQUIRE(sim.get_value(dev.instance().io().out_data) == 0);
    });

// ═══════════════════════════════════════════════════════════════════════════
// 语料级层次化回归 · **产物级**（残留 1b · T0）
//
// ⚠️ **为什么这些 DUT 需要它**：本文件的 DUT 是**真实语料** ⇒ ⇒ 它们带**真实拓扑**
//    （子组件 + 真实端口 + 真实位宽）⇒ ⇒ 层次化路径在它们身上**结构上**与 T3-005 的
//    5 个**专用** DUT 不同 ⇒ ⇒ ⇒ 「层次化路径只被专用 DUT 覆盖」的缺口**正是**这里。
//
// ⚠️ **判据在共享 fixture 里**（`tests/hier_corpus_fixture.h`）⇒ ⇒ 这里只负责
//    「**用哪些真实 DUT**」这个事实（★ 它们的类在本文件的匿名命名空间内 ⇒ ⇒ 外部
//    引用不到）。
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("corpus hierarchy regression (product level): ModuleValueTransferTop",
          "[hierarchy][corpus][t0][module]") {
    const std::string dir = ch_test::make_temp_dir("_corpus_module_value_transfer_top");
    REQUIRE_FALSE(dir.empty());
    const auto r = ch_test::hier_corpus::check_product_level<ModuleValueTransferTop>("module_value_transfer_top", dir);
    INFO("error: " << r.error);
    REQUIRE(r.ok);
    REQUIRE(r.module_count >= 2u);       // ★ ForceLayer 下真的分层了
    REQUIRE(r.output_port_count >= 1u);   // ★ 顶层有可观测的 output
    REQUIRE(r.deterministic);             // ★ 两次发射逐字节相同
    REQUIRE(r.differs_from_flat);         // ★ 分层真的改变了产物
    // ⚠️ Catch2 的 `REQUIRE` **不支持** `||` ⇒ ⇒ 先算布尔再断言（★ 仓内已登记的坑）
    const bool lint_accounted = r.lint_skipped || r.lint_ok;
    REQUIRE(lint_accounted);  // ★ verilator 不可用 ⇒ 显式 skip
}

TEST_CASE("corpus hierarchy regression (product level): ModuleChainTop",
          "[hierarchy][corpus][t0][module]") {
    const std::string dir = ch_test::make_temp_dir("_corpus_module_chain_top");
    REQUIRE_FALSE(dir.empty());
    const auto r = ch_test::hier_corpus::check_product_level<ModuleChainTop>("module_chain_top", dir);
    INFO("error: " << r.error);
    REQUIRE(r.ok);
    REQUIRE(r.module_count >= 2u);       // ★ ForceLayer 下真的分层了
    REQUIRE(r.output_port_count >= 1u);   // ★ 顶层有可观测的 output
    REQUIRE(r.deterministic);             // ★ 两次发射逐字节相同
    REQUIRE(r.differs_from_flat);         // ★ 分层真的改变了产物
    // ⚠️ Catch2 的 `REQUIRE` **不支持** `||` ⇒ ⇒ 先算布尔再断言（★ 仓内已登记的坑）
    const bool lint_accounted = r.lint_skipped || r.lint_ok;
    REQUIRE(lint_accounted);  // ★ verilator 不可用 ⇒ 显式 skip
}

TEST_CASE("corpus hierarchy regression (product level): ModuleIoDirectionTop",
          "[hierarchy][corpus][t0][module]") {
    const std::string dir = ch_test::make_temp_dir("_corpus_module_io_direction_top");
    REQUIRE_FALSE(dir.empty());
    const auto r = ch_test::hier_corpus::check_product_level<ModuleIoDirectionTop>("module_io_direction_top", dir);
    INFO("error: " << r.error);
    REQUIRE(r.ok);
    REQUIRE(r.module_count >= 2u);       // ★ ForceLayer 下真的分层了
    REQUIRE(r.output_port_count >= 1u);   // ★ 顶层有可观测的 output
    REQUIRE(r.deterministic);             // ★ 两次发射逐字节相同
    REQUIRE(r.differs_from_flat);         // ★ 分层真的改变了产物
    // ⚠️ Catch2 的 `REQUIRE` **不支持** `||` ⇒ ⇒ 先算布尔再断言（★ 仓内已登记的坑）
    const bool lint_accounted = r.lint_skipped || r.lint_ok;
    REQUIRE(lint_accounted);  // ★ verilator 不可用 ⇒ 显式 skip
}

// ═══════════════════════════════════════════════════════════════════════════
// 语料级层次化回归 · **行为级**（残留 1b · T1）
//
// ⚠️ **T1 的选样依据 = 「结构类」而非「个数」**：每类取**一个**能代表该结构的 DUT
//    ⇒ ⇒ 类之间**互不重复** ⇒ ⇒ 覆盖面按**结构**计（★ 数值计会诱导「凑数」）。
//    ⚠️ **可用的结构类**（★ planner 普查 ⇒ 实施期实测**订正**过）：
//    · C1'「双子组件」⇒ `ModuleIoDirectionTop`（两个 `ModuleDataProcessor`）
//    · C2 「兄弟跨模块边 + **同类型多实例**」⇒ `ModuleChainTop`
//      （★ 发射器按 `sanitize(module_type_name)` 分组并编号 ⇒ ⇒ 产出 `<Type>` /
//       `<Type>_1` **两份同体 module** ⇒ ⇒ 这是 C2 的真正判别点）
//    · C3「root 持 `ch_mem` + 子组件带 AXI 端口」⇒ `ICachePrefetch`（在 examples 侧）
//    ⚠️ **「双子组件**各自带寄存器**」这一类在既有语料里**没有可用样本** ⇒ ⇒
//    ⇒ 唯一候选 `ImmConsistencyDut` **不可**层次化（★ 它的 `equal` 输出依赖两个子
//    组件的**非端口**内部线网 ⇒ ⇒ 需隐式端口合成 ⇒ ⇒ 发射器不支持）⇒ ⇒ 记 backlog。
// ═══════════════════════════════════════════════════════════════════════════

TEST_CASE("corpus hierarchy regression (behaviour level): ModuleChainTop - sibling edge + same-type multi-instance",
          "[hierarchy][corpus][t1][module][sibling-edge][multi-instance]") {
    // ⚠️ **verilator 不可用 ⇒ WARN-skip**（★ 与 `DUAL_BACKEND_TEST_PAIR` 的既有约定
    //    同款 ⇒ ⇒ 否则 macOS/Windows 的 `-DBUILD_VERILATOR=OFF` 本地开发会**新增硬
    //    失败**；⇒ ⇒ ★ 与本 change 的 T0（`lint_skipped` 显式分支）**自洽**）
    if (!ch_test::tool_available("verilator")) {
        WARN("verilator not on PATH; corpus hierarchy regression (behaviour level) skipped");
        return;
    }
    const std::string dir = ch_test::make_temp_dir("_corpus_t1_chain");
    REQUIRE_FALSE(dir.empty());
    ch::ch_device<ModuleChainTop> dev;

    const auto products =
        ch::hier_eq::emit(dev.instance(), dir, ch::Granularity::ForceLayer);
    REQUIRE(products.ok);

    std::vector<ch::hier_eq::DrivenInput> inputs;
    inputs.push_back(ch::hier_eq::DrivenInput{
        "in_data", [&dev](ch::Simulator &s, std::uint64_t v) {
            s.set_input_value(dev.instance().io().in_data, v);
        }});

    std::vector<ch::hier_eq::Observation> observations;
    observations.push_back(ch::hier_eq::make_observation(
        "out_data", dev.instance().io().out_data,
        [&dev](ch::Simulator &s) -> std::uint64_t {
            return static_cast<std::uint64_t>(s.get_port_value(dev.instance().io().out_data));
        }));

    // ⚠️ 激励 MUST **逐拍变化** ⇒ ⇒ 否则观测恒定 ⇒ ⇒ 被 `no-varying-signal` 拒绝
    const std::vector<std::vector<std::uint64_t>> stim = {{0}, {1}, {2}, {3}, {4}, {5},
                                                             {6}, {7}, {8}, {9}, {10}, {11},
                                                             {12}, {13}, {14}, {15}};
    const auto r = ch::hier_eq::compare(dev.context(), dir, products.hier_path, products.ports,
                                        inputs, observations, stim);
    INFO("error: " << r.error);
    INFO("first mismatch: " << r.first_mismatch);
    REQUIRE(r.ok);
    REQUIRE(r.equal);
    REQUIRE(r.mismatch_count == 0u);
}

TEST_CASE("corpus hierarchy regression (behaviour level): ModuleIoDirectionTop - two child modules",
          "[hierarchy][corpus][t1][module][two-children]") {
    // ⚠️ **verilator 不可用 ⇒ WARN-skip**（★ 与 `DUAL_BACKEND_TEST_PAIR` 的既有约定
    //    同款 ⇒ ⇒ 否则 macOS/Windows 的 `-DBUILD_VERILATOR=OFF` 本地开发会**新增硬
    //    失败**；⇒ ⇒ ★ 与本 change 的 T0（`lint_skipped` 显式分支）**自洽**）
    if (!ch_test::tool_available("verilator")) {
        WARN("verilator not on PATH; corpus hierarchy regression (behaviour level) skipped");
        return;
    }
    const std::string dir = ch_test::make_temp_dir("_corpus_t1_io_dir");
    REQUIRE_FALSE(dir.empty());
    ch::ch_device<ModuleIoDirectionTop> dev;

    const auto products =
        ch::hier_eq::emit(dev.instance(), dir, ch::Granularity::ForceLayer);
    REQUIRE(products.ok);

    std::vector<ch::hier_eq::DrivenInput> inputs;
    inputs.push_back(ch::hier_eq::DrivenInput{
        "in_data", [&dev](ch::Simulator &s, std::uint64_t v) {
            s.set_input_value(dev.instance().io().in_data, v);
        }});
    inputs.push_back(ch::hier_eq::DrivenInput{
        "enable1", [&dev](ch::Simulator &s, std::uint64_t v) {
            s.set_input_value(dev.instance().io().enable1, v);
        }});
    inputs.push_back(ch::hier_eq::DrivenInput{
        "enable2", [&dev](ch::Simulator &s, std::uint64_t v) {
            s.set_input_value(dev.instance().io().enable2, v);
        }});

    std::vector<ch::hier_eq::Observation> observations;
    observations.push_back(ch::hier_eq::make_observation(
        "out_data", dev.instance().io().out_data,
        [&dev](ch::Simulator &s) -> std::uint64_t {
            return static_cast<std::uint64_t>(s.get_port_value(dev.instance().io().out_data));
        }));

    // ⚠️ 激励逐拍变化（★ 且**跨两个子组件的使能** ⇒ ⇒ 覆盖「select 的两路」）
    const std::vector<std::vector<std::uint64_t>> stim = {{0, 1, 0}, {1, 1, 0}, {2, 0, 1},
                                                            {3, 0, 1}, {4, 1, 1}, {5, 0, 0},
                                                            {6, 1, 0}, {7, 0, 1}, {8, 1, 1},
                                                            {9, 0, 0}, {10, 1, 0}, {11, 0, 1}};
    const auto r = ch::hier_eq::compare(dev.context(), dir, products.hier_path, products.ports,
                                        inputs, observations, stim);
    INFO("error: " << r.error);
    INFO("first mismatch: " << r.first_mismatch);
    REQUIRE(r.ok);
    REQUIRE(r.equal);
    REQUIRE(r.mismatch_count == 0u);
}

