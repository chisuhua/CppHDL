// tests/test_bundle_ecosystem.cpp
//
// bundle 生态「边角设施」的可执行回归护栏
// （OpenSpec change `bundle-ecosystem-hardening-2026-10`，
//   capability `bundle-master-slave-io`）。
//
// 为什么需要它：ADR-042（`bundle-master-slave-io-2026-10`）只覆盖了
// 「单个 bundle、叶子字段、master↔slave 直连」这条**主路径**；bundle 生态的
// **边角**设施（嵌套 bundle / `flip()` / 工厂函数 / 序列化 / `ch_stream` 的
// 连接语义 / prefix 命名）此前**既无契约、也无断言**，其中若干处还**静默失效**：
//
//   - prefix 命名机制是「空壳」：叶子节点名**从来**不带 prefix；
//   - 基类构造期对**尚未构造**的嵌套 bundle 成员调虚函数 ⇒ UB（segfault）；
//   - `flip()` 硬编码 `as_slave()`，不记忆原方向；
//   - `deserialize_bits_to_fields()` 把字段**重绑定**成位串切片；
//   - `ch_stream` 的自由 `operator<<=` 走**节点重绑定**（与其它 bundle 不一致）。
//
// ⚠️ 本文件的断言 MUST 是**行为级**的（驱动源 → 目标读到值），
//    MUST NOT 仅断言 `impl() != nullptr` 之类的结构性事实 ——
//    结构性断言在「节点重绑定」与「数据流边」两种语义下**都会通过** ⇒ 零信号。
//    ⇒ 凡涉及「连接语义」的用例，都额外带一条**能区分两种语义**的断言
//      （节点身份不同 / 存在 src 边）。
//
// ⚠️ **必须** include `chlib/stream_operators.h` —— 自由 `operator<<=` 只在
//    声明可见的 TU 里参与重载决议 ⇒ 不 include 它时，`ch_stream <<= ch_stream`
//    会落到**成员** `bundle_base::operator<<=`（逐字段连接）⇒ 本文件的
//    「自由运算符」用例就测不到自由运算符（实测：include 缺失时，
//    把自由运算符改回节点重绑定，本文件**全绿** ⇒ 零信号）。

#include "catch_amalgamated.hpp"

#include "bundle/axi_bundle.h"
#include "bundle/axi_lite_bundle.h"
#include "bundle/common_bundles.h"
#include "bundle/fragment.h"
#include "bundle/stream_bundle.h"
#include "ch.hpp"
#include "chlib/stream_operators.h"
#include "component.h"
#include "core/bundle/bundle_base.h"
#include "core/bundle/bundle_serialization.h"
#include "core/bundle/bundle_utils.h"
#include "core/context.h"
#include "device.h"
#include "simulator.h"

#include <cstdint>
#include <string>
#include <tuple>

using namespace ch;
using namespace ch::core;

// ============================================================================
// ⚠️ **编译期**断言：被移除的设施 MUST NOT 再存在
// ============================================================================
//
// `bundle_concat` / `bundle_slice_view`（以及它们的公开工厂 `bundle_cat()` /
// `bundle_slice()`）是 `core/bundle/bundle_operations.h` 的**全部**内容
// ⇒ 那个头文件消失 ⇒ 那两个类型与两个工厂就**不可能**再被引用。
//
// （用 `__has_include` 而不是运行时检查：类型不存在时**无法**在运行时引用它；
//   而头文件的存在性是**编译期**可判定的 ⇒ 断言在编译时就钉住，
//   任何「偷偷把头文件加回来」的行为都会让本文件**编译失败**。）
//
// ⚠️ 这**不是**「给被移除的设施补行为断言」—— 它断言的是「它们**不存在**」。
#if __has_include("core/bundle/bundle_operations.h")
#error                                                                        \
    "core/bundle/bundle_operations.h MUST have been removed (the bundle concat/slice facilities and their factories are gone)"
#endif

namespace {

// ---------------------------------------------------------------------------
// 含**嵌套 bundle** 字段的 bundle（嵌套字段是 `ch_stream`）
//
// ⚠️ **故意不**定义自己的 prefix 构造 ⇒ 走 `CH_BUNDLE_FIELDS_T` 里的
//    `using bundle_base<Self>::bundle_base;` ⇒ 继承**基类**的
//    `bundle_base(const std::string&)` ⇒ 这正是 L1 的 UB 路径
//    （基类构造期对尚未构造的嵌套 bundle 成员派发虚调用）。
// ---------------------------------------------------------------------------
struct WithNestedStream : public bundle_base<WithNestedStream> {
    using Self = WithNestedStream;
    ch_stream<ch_uint<8>> data_stream;
    ch_bool flag;

    CH_BUNDLE_FIELDS_T(data_stream, flag)

    void as_master_direction() {
        this->make_output(flag);
        this->data_stream.as_master();
    }

    void as_slave_direction() {
        this->make_input(flag);
        this->data_stream.as_slave();
    }
};

// ---------------------------------------------------------------------------
// 两个普通叶子字段的 bundle —— 用于「反序列化路径」的用例
// ---------------------------------------------------------------------------
struct TwoFieldBundle : public bundle_base<TwoFieldBundle> {
    using Self = TwoFieldBundle;
    ch_uint<8> data;
    ch_bool flag;

    TwoFieldBundle() = default;

    CH_BUNDLE_FIELDS_T(data, flag)

    void as_master_direction() { this->make_output(data, flag); }
    void as_slave_direction() { this->make_input(data, flag); }
};

// 承载 TwoFieldBundle 的组件（bundle 作为成员 ⇒ 字段节点进设计图）
class TwoFieldDut : public ch::Component {
public:
    TwoFieldBundle io;

    TwoFieldDut(ch::Component *parent = nullptr,
                const std::string &name = "two_field_dut")
        : ch::Component(parent, name) {
        io.as_master();
    }

    void create_ports() override {}
    void describe() override {}
};

// ---------------------------------------------------------------------------
// DUT：`ch_stream` 的连接（数据流边 vs 节点重绑定）
// ---------------------------------------------------------------------------
//
// 设计侧用寄存器驱动 `src.payload`；`dst <<= src` 是被测的连接。
// ⚠️ 「行为级」断言：dst 的 payload MUST 读到源计数器的值。
class StreamConnDut : public ch::Component {
public:
    ch_stream<ch_uint<8>> src;
    ch_stream<ch_uint<8>> dst;

    StreamConnDut(ch::Component *parent = nullptr,
                  const std::string &name = "stream_conn")
        : ch::Component(parent, name) {
        src.as_master();
        dst.as_slave();
    }

    void create_ports() override {}

    void describe() override {
        ch_reg<ch_uint<8>> counter(0_d, "counter");
        counter->next = counter + 1_b;
        // 设计侧驱动源字段
        src.payload = counter;
        src.valid = 1_b;
        // ← 被测的连接
        dst <<= src;
    }
};

}  // anonymous namespace

// ============================================================================
// 1. prefix 命名（L11：prefix 机制此前是「空壳」）
// ============================================================================

TEST_CASE("bundle: 带 prefix 构造的 bundle 的字段节点名 MUST 带 prefix",
          "[bundle][ecosystem][prefix]") {
    context ctx("bundle_prefix");
    ctx_swap swap(&ctx);

    fifo_bundle<ch_uint<8>> b("top.fifo");
    b.as_master();

    REQUIRE(b.data.impl() != nullptr);
    REQUIRE(b.data.impl()->name() == "top.fifo.data");
    REQUIRE(b.push.impl()->name() == "top.fifo.push");
    REQUIRE(b.full.impl()->name() == "top.fifo.full");
    REQUIRE(b.pop.impl()->name() == "top.fifo.pop");
    REQUIRE(b.empty.impl()->name() == "top.fifo.empty");
}

TEST_CASE("bundle: 命名 MUST 幂等（as_master 后 as_slave 不重复拼接 prefix）",
          "[bundle][ecosystem][prefix]") {
    context ctx("bundle_prefix_idem");
    ctx_swap swap(&ctx);

    fifo_bundle<ch_uint<8>> b("pfx");
    b.as_master();

    auto *node = b.data.impl();
    const std::string name_before = node->name();
    REQUIRE(name_before == "pfx.data");

    b.as_slave();

    // 节点身份 MUST 不变
    REQUIRE(b.data.impl() == node);
    // 名字 MUST NOT 变成 "pfx.pfx.data"
    REQUIRE(b.data.impl()->name() == name_before);
    REQUIRE(b.data.impl()->name().find("pfx.pfx") == std::string::npos);
}

TEST_CASE("bundle: 拷贝构造 MUST 保留 prefix", "[bundle][ecosystem][prefix]") {
    context ctx("bundle_prefix_copy");
    ctx_swap swap(&ctx);

    fifo_bundle<ch_uint<8>> orig("cpfx");
    // 拷贝发生在**角色设定之前** ⇒ 副本 MUST 自己带上 prefix
    fifo_bundle<ch_uint<8>> copy(orig);
    copy.as_master();

    REQUIRE(copy.data.impl() != nullptr);
    REQUIRE(copy.data.impl()->name() == "cpfx.data");
}

TEST_CASE("bundle: 事后改 prefix MUST NOT 改名已建立的节点",
          "[bundle][ecosystem][prefix]") {
    context ctx("bundle_prefix_late");
    ctx_swap swap(&ctx);

    fifo_bundle<ch_uint<8>> b("first");
    b.as_master();

    auto *node = b.data.impl();
    const std::string name_before = node->name();
    REQUIRE(name_before == "first.data");

    b.set_name_prefix("second");

    // 已建立的节点 MUST 保持不变（改名会孤儿化已建立的数据流边）
    REQUIRE(b.data.impl() == node);
    REQUIRE(b.data.impl()->name() == name_before);
}

TEST_CASE("bundle: 角色从未设定的 bundle MUST NOT 有字段节点",
          "[bundle][ecosystem][prefix]") {
    context ctx("bundle_prefix_norole");
    ctx_swap swap(&ctx);

    fifo_bundle<ch_uint<8>> b("nopfx");

    // 现有语义（被 tests/test_bundle_node_management.cpp 钉住）
    REQUIRE(b.data.impl() == nullptr);
    REQUIRE(b.push.impl() == nullptr);
    REQUIRE(b.empty.impl() == nullptr);
}

TEST_CASE("bundle: 嵌套 bundle 的叶子节点名 MUST 含完整层级前缀",
          "[bundle][ecosystem][prefix][nested]") {
    context ctx("nested_prefix");
    ctx_swap swap(&ctx);

    // ⚠️ 该构造走**基类**的 prefix ctor ⇒ 构造期 MUST NOT 触碰派生类成员
    //    （否则对尚未构造的嵌套 bundle 成员派发虚调用 ⇒ UB / segfault）
    WithNestedStream nested("outer");
    nested.as_master();

    REQUIRE(nested.flag.impl() != nullptr);
    REQUIRE(nested.flag.impl()->name() == "outer.flag");

    REQUIRE(nested.data_stream.payload.impl() != nullptr);
    REQUIRE(nested.data_stream.payload.impl()->name() ==
            "outer.data_stream.payload");
    REQUIRE(nested.data_stream.valid.impl()->name() ==
            "outer.data_stream.valid");
    REQUIRE(nested.data_stream.ready.impl()->name() == "outer.data_stream.ready");
}

// ============================================================================
// 2. `flip()` 的方向语义
// ============================================================================

TEST_CASE("bundle: flip() MUST 返回方向取反的副本", "[bundle][ecosystem][flip]") {
    context ctx("bundle_flip");
    ctx_swap swap(&ctx);

    fifo_bundle<ch_uint<8>> m;
    m.as_master();
    auto flipped_master = m.flip();
    REQUIRE(flipped_master != nullptr);
    REQUIRE(flipped_master->get_role() == bundle_role::slave);
    // 方向元数据 MUST 一并取反
    REQUIRE(std::holds_alternative<input_direction>(
        flipped_master->data.direction()));

    fifo_bundle<ch_uint<8>> s;
    s.as_slave();
    auto flipped_slave = s.flip();
    REQUIRE(flipped_slave != nullptr);
    REQUIRE(flipped_slave->get_role() == bundle_role::master);
    REQUIRE(std::holds_alternative<output_direction>(
        flipped_slave->data.direction()));
}

TEST_CASE("bundle: 未设角色的 bundle flip MUST 走 unknown→slave",
          "[bundle][ecosystem][flip]") {
    context ctx("bundle_flip_unknown");
    ctx_swap swap(&ctx);

    // 从未调用过 as_master() / as_slave()
    fifo_bundle<ch_uint<8>> u;
    REQUIRE(u.get_role() == bundle_role::unknown);

    auto flipped = u.flip();
    REQUIRE(flipped != nullptr);
    REQUIRE(flipped->get_role() == bundle_role::slave);
}

// ============================================================================
// 3. `master()` / `slave()` 工厂的节点独立性
// ============================================================================

TEST_CASE("bundle: master()/slave() 工厂 MUST 产出字段节点独立的 bundle",
          "[bundle][ecosystem][factory]") {
    context ctx("bundle_factory");
    ctx_swap swap(&ctx);

    fifo_bundle<ch_uint<8>> src;
    src.as_master();

    auto m = master(src);
    REQUIRE(m.get_role() == bundle_role::master);
    // 字段节点 MUST 独立（不与源共享）
    REQUIRE(m.data.impl() != nullptr);
    REQUIRE(m.data.impl() != src.data.impl());
    REQUIRE(m.push.impl() != src.push.impl());
    REQUIRE(m.empty.impl() != src.empty.impl());
    // 总线节点 MUST 仍共享（拷贝构造语义 —— 既有测试钉住）
    REQUIRE(m.impl() == src.impl());

    auto s = slave(src);
    REQUIRE(s.get_role() == bundle_role::slave);
    REQUIRE(s.data.impl() != src.data.impl());
    REQUIRE(s.impl() == src.impl());
}

// ============================================================================
// 4. 反序列化路径（`deserialize_bits_to_fields()`）
// ============================================================================

TEST_CASE("bundle: deserialize MUST 产出字段一等节点（不是位串切片）",
          "[bundle][ecosystem][deserialize]") {
    context ctx("bundle_deserialize");
    ctx_swap swap(&ctx);

    ch_uint<16> bits = ch_uint<16>(0x00A5);
    auto b = deserialize<TwoFieldBundle>(bits);

    REQUIRE(b.data.impl() != nullptr);
    REQUIRE(b.flag.impl() != nullptr);

    // ⚠️ **区分性断言**：字段节点 MUST 是**值代理**（自己的节点），
    //    MUST NOT 是位串的 `bits` / `bit_select` 切片节点。
    //    （旧实现把字段**重绑定**成切片 ⇒ 节点类型是 `type_op`。）
    REQUIRE(b.data.impl()->type() == lnodetype::type_proxy);
    REQUIRE(b.flag.impl()->type() == lnodetype::type_proxy);

    // 且字段的驱动源 MUST NOT 是源位串本身（切片才是驱动源）
    REQUIRE(b.data.impl()->num_srcs() == 1);
    REQUIRE(b.data.impl()->src(0) != bits.impl());
    REQUIRE(b.flag.impl()->num_srcs() == 1);
    REQUIRE(b.flag.impl()->src(0) != bits.impl());
}

TEST_CASE("bundle: deserialize 产出的字段 MUST 可独立寻址且值正确",
          "[bundle][ecosystem][deserialize]") {
    ch_device<TwoFieldDut> dev;

    // ⚠️ `ch_device` 的构造结束时会把 context **换回外层** ⇒ 要在**设备的
    //    context** 里建节点，MUST 显式再 swap 一次（否则字面量建到别的
    //    context 里 ⇒ 切片拿不到源 ⇒ 字段恒为 0）。
    {
        ctx_swap device_ctx(dev.context());
        // 把位串反序列化进 DUT 的 bundle
        // （data = bits 7..0 = 0xA5；flag = bit 8 = 1）
        //
        // ⚠️ MUST 在 **Simulator 构造之前**反序列化 —— Simulator 的构造会
        //    初始化求值表（`context::get_eval_list()`）；之后新建的节点
        //    （切片 + 边）不在表里 ⇒ 字段的值不会被求值。
        ch_uint<16> bits = ch_uint<16>(0x01A5);
        deserialize_bits_to_fields(bits, dev.instance().io);
    }

    Simulator sim(dev.context());
    sim.tick();

    // 字段 MUST 在 data_map 中有槽位（可寻址）⇒ 不得出现
    // 「Value not found for signal node ID」告警
    REQUIRE(sim.get_signal_node_value(dev.instance().io.data.impl()) == 0xA5);
    REQUIRE(sim.get_signal_node_value(dev.instance().io.flag.impl()) == 1);
}

// ============================================================================
// 5. `ch_stream` 的连接语义
// ============================================================================

TEST_CASE("bundle: ch_stream 的连接 MUST 建立数据流边（不是节点重绑定）",
          "[bundle][ecosystem][stream]") {
    context ctx("stream_connect");
    ctx_swap swap(&ctx);

    ch_stream<ch_uint<8>> source;
    ch_stream<ch_uint<8>> sink;
    source.as_master();
    sink.as_slave();

    sink <<= source;

    // ⚠️ **区分性断言**：节点重绑定 ⇒ 两侧是**同一个**节点；
    //    数据流边 ⇒ 不同的节点 + 一条 src 边。
    REQUIRE(sink.payload.impl() != source.payload.impl());
    REQUIRE(sink.valid.impl() != source.valid.impl());
    REQUIRE(sink.payload.impl()->num_srcs() == 1);
    REQUIRE(sink.payload.impl()->src(0) == source.payload.impl());
    REQUIRE(sink.valid.impl()->num_srcs() == 1);
    REQUIRE(sink.valid.impl()->src(0) == source.valid.impl());
    // ready 反向流动
    REQUIRE(source.ready.impl()->num_srcs() == 1);
    REQUIRE(source.ready.impl()->src(0) == sink.ready.impl());
}

TEST_CASE("bundle: ch_stream 的连接 MUST 让值跨边界传播",
          "[bundle][ecosystem][stream][behavior]") {
    ch_device<StreamConnDut> dev;
    Simulator sim(dev.context());

    sim.tick();

    // 行为级：dst 的 payload MUST 读到源计数器的值（tick 后 counter == 1）
    // ⇒ 「值传播」在节点重绑定与数据流边两种语义下**都会**通过 ⇒
    //    真正的区分性断言在上面的用例里（节点身份 + src 边）。
    REQUIRE(sim.get_signal_node_value(dev.instance().dst.payload.impl()) == 1);
}

TEST_CASE("bundle: 嵌套 stream 字段的连接 MUST 同样走数据流边",
          "[bundle][ecosystem][stream][nested]") {
    context ctx("nested_stream_connect");
    ctx_swap swap(&ctx);

    WithNestedStream a;
    WithNestedStream b;
    a.as_master();
    b.as_slave();

    // bundle 级连接 ⇒ 递归到嵌套的 ch_stream 字段
    b <<= a;

    REQUIRE(b.data_stream.payload.impl() != a.data_stream.payload.impl());
    REQUIRE(b.data_stream.payload.impl()->num_srcs() == 1);
    REQUIRE(b.data_stream.payload.impl()->src(0) ==
            a.data_stream.payload.impl());
    REQUIRE(b.data_stream.valid.impl() != a.data_stream.valid.impl());
    REQUIRE(b.data_stream.valid.impl()->src(0) == a.data_stream.valid.impl());
}

// ============================================================================
// 6. 已移除的设施
// ============================================================================

TEST_CASE("bundle: 公开派生类清单 MUST NOT 含已移除的拼接/切片类型",
          "[bundle][ecosystem][inventory]") {
    // ⚠️ 「不存在」这件事只能**编译期**断言（见文件顶部的 `__has_include`
    //    守卫）—— 类型不存在时无法在运行时引用它。
    //
    // 本用例锁定**库内**的 bundle 派生类清单（`include/` 下的 18 个；
    // 另有 2 个在 `examples/riscv-mini/src/` ⇒ 全仓 20 个）。
    using LibraryBundleTypes = std::tuple<
        axi_addr_channel<32>, axi_write_data_channel<32>,
        axi_write_resp_channel, axi_write_channel<32, 32>,
        axi_lite_aw_channel<32>, axi_lite_w_channel<32>, axi_lite_b_channel,
        axi_lite_ar_channel<32>, axi_lite_r_channel<32>,
        axi_lite_write_interface<32, 32>, axi_lite_read_interface<32, 32>,
        axi_lite_bundle<32, 32>, fifo_bundle<ch_uint<8>>, interrupt_bundle,
        config_bundle<32, 32>, ch_flow<ch_uint<8>>,
        ch_fragment<ch_uint<8>>, ch_stream<ch_uint<8>>>;

    STATIC_REQUIRE(std::tuple_size_v<LibraryBundleTypes> == 18);
    STATIC_REQUIRE(is_bundle_v<std::tuple_element_t<
                       0, LibraryBundleTypes>>);
    STATIC_REQUIRE(is_bundle_v<std::tuple_element_t<
                       17, LibraryBundleTypes>>);

    // 被移除的两个类型**不在**清单里 ⇒ 全仓清单 22 → 20
    // （18 库内 + 2 examples/riscv-mini）
    REQUIRE(bundle_field_count_v<fifo_bundle<ch_uint<8>>> == 6);
}
