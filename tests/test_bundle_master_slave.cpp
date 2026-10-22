// tests/test_bundle_master_slave.cpp
//
// Bundle master/slave 连接的可执行回归护栏
// （OpenSpec change `bundle-master-slave-io-2026-10`，capability
//  `bundle-master-slave-io`）。
//
// 为什么需要它：`fifo_bundle` / `config_bundle` / `interrupt_bundle` 这三个
// 「普通信号字段」bundle，在此之前的**全部**使用点只有「打印宽度 / 打印角色 /
// 命名测试」—— **从未做过一次真实的 master/slave 连接**。
// 于是 `samples/fifo_bundle.cpp` 一直是红的（`empty=0` / exit=1）却长期无人发现：
//
//   - `as_master()` / `as_slave()` 把每个字段**重绑定**成「零字面量总线」的
//     **位切片** ⇒ 设计**只读不写**的字段恒为 0（FIFO 永远看不到 push/pop）
//   - 字段连接用 `operator=`（**节点重绑定**）⇒ 原驱动被孤儿化
//   - `connect()` 方向盲 ⇒ 把 slave 的输出用 master 的未驱动输入覆盖
//   - `Simulator` 的 bundle 字段读写走常量转换 / 重绑定 ⇒ 运行时字段读不到
//
// 本文件是那两个方向的**可执行锁**：TB → slave、slave → master。
//
// ⚠️ DUT MUST 用 **bundle 级**接线（`connect(master, slave)` 或 `master <<= slave`），
// MUST NOT 逐字段手工 `<<=` —— 否则「方向感知」/「方向指认」的回归不会被发现。

#include "catch_amalgamated.hpp"

#include "bundle/common_bundles.h"
#include "ch.hpp"
#include "component.h"
#include "core/context.h"
#include "device.h"
#include "simulator.h"

#include <cstdint>
#include <variant>
#include <vector>

using namespace ch;
using namespace ch::core;

namespace {

// ---------------------------------------------------------------------------
// DUT 1：bundle↔bundle 的 FIFO
//
// 字段语义按 `fifo_bundle`：data=写数据、push/pop=命令（master 驱动）、
// full/empty=状态（slave 驱动）。
//
// ⚠️ 该 DUT **必须**含：
//   - 多个字段（`fifo_bundle` 有 5 个）
//   - 至少一个 **offset > 0** 的字段（`push` 在 offset 2）—— 否则「切片覆写」
//     这类缺陷测不出来（单字段 bundle 会碰巧正确）
//   - 至少一个「**设计只读不写**」的字段（`push` / `pop`）—— 这正是 D0 的暴露点
// ---------------------------------------------------------------------------
class BundleFifo : public ch::Component {
public:
    fifo_bundle<ch_uint<8>> io;

    BundleFifo(ch::Component *parent = nullptr,
               const std::string &name = "bundle_fifo")
        : ch::Component(parent, name) {
        io.as_slave();
    }

    void create_ports() override {}

    void describe() override {
        // 读写指针（addr_width + 1 位用于空/满判定）
        ch_reg<ch_uint<3>> rd_ptr(0_d, "rd_ptr");
        ch_reg<ch_uint<3>> wr_ptr(0_d, "wr_ptr");

        ch_uint<2> rd_a = bits<1, 0>(rd_ptr);
        ch_uint<2> wr_a = bits<1, 0>(wr_ptr);

        // `push` / `pop` 是**只读**字段（设计从不给它们赋值）
        auto writing = io.push;
        auto reading = io.pop;

        rd_ptr->next = select(reading, rd_ptr + 1_b, rd_ptr);
        wr_ptr->next = select(writing, wr_ptr + 1_b, wr_ptr);

        ch_mem<ch_uint<8>, 4> mem("bundle_fifo_mem");
        mem.write(wr_a, io.data, writing);

        // `full` / `empty` 是**写**字段（设计驱动它们）
        io.empty <<= (rd_ptr == wr_ptr);
        io.full <<= ((wr_ptr + 1_b) == rd_ptr);

        // ⚠️ **读数据通路**（ADR-044）：`rdata` 是**独立**的读数据字段
        //    （master IN / slave OUT）。
        //    ⚠️ 用 `aread`（0-cycle 组合读）⇒ 与生成的 Verilog
        //    `assign dout = mem[rd_ptr];` 完全一致（见
        //    `sim-mem-read-timing-align-2026-10`）。
        //    ⚠️ FIFO 协议：`dout` 在 pop 沿**之后**指向**下一个**队首 ⇒
        //    消费者 MUST 在 pop 沿**之前**采样（ADR-044 裁决 5）。
        io.rdata <<= mem.aread(rd_a);
    }
};

// 顶层：master bundle ↔ slave bundle
class BundleFifoTop : public ch::Component {
public:
    fifo_bundle<ch_uint<8>> io_bundle;

    BundleFifoTop(ch::Component *parent = nullptr,
                   const std::string &name = "bundle_fifo_top")
        : ch::Component(parent, name) {
        io_bundle.as_master();
    }

    void create_ports() override {}

    void describe() override {
        ch::ch_module<BundleFifo> fifo_inst{"fifo_inst"};
        // ⚠️ bundle 级接线（不是逐字段 <<=）
        connect(io_bundle, fifo_inst.instance().io);
    }
};

// DUT 2：**未连接**的 standalone bundle —— 覆盖 `set_bundle_value()` /
// `get_bundle_value()` 的槽位读写语义。
//
// ⚠️ 已连接字段的值由设计每拍求值产生，TB 的写入会被下一拍覆写 ⇒
// 「TB 写入被设计观察到」这条**只能**在未连接的 bundle 上断言。
class StandaloneBundle : public ch::Component {
public:
    fifo_bundle<ch_uint<8>> io;

    StandaloneBundle(ch::Component *parent = nullptr,
                     const std::string &name = "standalone_bundle")
        : ch::Component(parent, name) {
        io.as_master();
    }

    void create_ports() override {}
    void describe() override {}
};

// 总宽正好 64 位的 bundle（单个 64 位字段）—— 用于「64 位字段的位段提取」
struct Bundle64 : public bundle_base<Bundle64> {
    using Self = Bundle64;
    ch_uint<64> lo;

    Bundle64() = default;
    CH_BUNDLE_FIELDS_T(lo)

    void as_master_direction() { this->make_output(lo); }
    void as_slave_direction() { this->make_input(lo); }
};

// 总宽 128 位（> 64）的 bundle —— 用于「打包式 API 的宽度约束」
struct BundleWide : public bundle_base<BundleWide> {
    using Self = BundleWide;
    ch_uint<64> lo;
    ch_uint<64> hi;

    BundleWide() = default;
    CH_BUNDLE_FIELDS_T(lo, hi)

    void as_master_direction() { this->make_output(lo, hi); }
    void as_slave_direction() { this->make_input(lo, hi); }
};

// 承载宽度 bundle 的组件（bundle 作为成员，字段节点仍进设计图）
class Bundle64Comp : public ch::Component {
public:
    Bundle64 io;
    Bundle64Comp(ch::Component *parent = nullptr,
                 const std::string &name = "bundle64_comp")
        : ch::Component(parent, name) {
        io.as_master();
    }
    void create_ports() override {}
    void describe() override {}
};

class BundleWideComp : public ch::Component {
public:
    BundleWide io;
    BundleWideComp(ch::Component *parent = nullptr,
                   const std::string &name = "bundle_wide_comp")
        : ch::Component(parent, name) {
        io.as_master();
    }
    void create_ports() override {}
    void describe() override {}
};

// `fifo_bundle` 的位布局：data(8) push(1) full(1) pop(1) empty(1) rdata(8)
// → push 在 bit 8、full 在 bit 9、pop 在 bit 10、empty 在 bit 11
// ⚠️ `rdata` **追加在末尾**（ADR-044 裁决 2）⇒ 既有 5 个偏移**完全不变**。
constexpr unsigned kFifoDataBits = 8;
constexpr unsigned kFifoPushBit = 8;
constexpr unsigned kFifoFullBit = 9;
constexpr unsigned kFifoPopBit = 10;
constexpr unsigned kFifoEmptyBit = 11;
constexpr unsigned kFifoRdataBit = 12;       // rdata 起始 bit
constexpr unsigned kFifoRdataBits = 8;       // rdata 宽度
constexpr uint64_t kFifoRdataMask = (1ULL << kFifoRdataBits) - 1;

uint64_t field_bit(uint64_t packed, unsigned bit) {
    return (packed >> bit) & 0x1ULL;
}

}  // anonymous namespace

// ============================================================================
// DUT 1：master ↔ slave 的跨边界值传播
// ============================================================================

TEST_CASE("bundle master/slave: TB 驱动的 push 到达 slave 侧",
          "[bundle][master-slave][read-timing]") {
    ch_device<BundleFifoTop> dev;
    Simulator sim(dev.context());

    constexpr uint64_t kPushBit = 1ULL << kFifoPushBit;
    constexpr uint64_t kPopBit = 1ULL << kFifoPopBit;

    // ⚠️ 先断言初值 empty == 1 —— 否则「push 后 empty == 0」这条断言在
    // 「连接本来就坏」的情况下也会**空洞地通过**（empty 恒为 0）
    sim.set_bundle_value(dev.instance().io_bundle, 0);
    sim.tick();
    uint64_t packed = sim.get_bundle_value(dev.instance().io_bundle);
    REQUIRE(field_bit(packed, kFifoEmptyBit) == 1);

    // TB 推入一个元素：push=1, data=0x5A
    sim.set_bundle_value(dev.instance().io_bundle, kPushBit | 0x5A);
    sim.tick();

    // slave 侧的 FIFO 真的写入了 ⇒ master 侧看到的 empty 变 0
    // ⇒ 这条断言要求 TB 的 push **真的到达**了 slave（`push` 是只读字段，
    //    正是 D0「只读字段恒为 0」缺陷的暴露点）
    packed = sim.get_bundle_value(dev.instance().io_bundle);
    REQUIRE(field_bit(packed, kFifoEmptyBit) == 0);

    // TB 弹出一个元素：pop=1 ⇒ rd_ptr 前进 ⇒ empty 回到 1
    // ⇒ 证明 pop 也到达了 slave（`pop` 同样是只读字段）
    sim.set_bundle_value(dev.instance().io_bundle, kPopBit);
    sim.tick();
    packed = sim.get_bundle_value(dev.instance().io_bundle);
    REQUIRE(field_bit(packed, kFifoEmptyBit) == 1);
}

TEST_CASE("bundle master/slave: **读数据通路** —— 推入的元素真的被读回",
          "[bundle][master-slave][read-path]") {
    // ⚠️ ADR-044 的核心断言：**读数据必须真的到达 master 侧**。
    //    ⚠️ 这条断言在 `rdata` 存在**之前**是**无法表达**的（`data` 是
    //    写数据、master 侧是它的驱动方）⇒ 「只读字段恒为 0」这类缺陷
    //    根本测不出来（这正是本 change 要治的病）。
    ch_device<BundleFifoTop> dev;
    Simulator sim(dev.context());

    constexpr uint64_t kPushBit = 1ULL << kFifoPushBit;
    constexpr uint64_t kPopBit = 1ULL << kFifoPopBit;

    // 初始：空 ⇒ `rdata` 是 don't-care（ADR-044 裁决 4）⇒ **不**断言它的值
    sim.set_bundle_value(dev.instance().io_bundle, 0);
    sim.tick();
    uint64_t packed = sim.get_bundle_value(dev.instance().io_bundle);
    REQUIRE(field_bit(packed, kFifoEmptyBit) == 1);

    // ---- 推入第 1 个元素 0x5A ----
    sim.set_bundle_value(dev.instance().io_bundle, kPushBit | 0x5A);
    sim.tick();
    packed = sim.get_bundle_value(dev.instance().io_bundle);

    // 非空 ⇒ `rdata` 有效 ⇒ MUST 是**刚推入的那个值**
    REQUIRE(field_bit(packed, kFifoEmptyBit) == 0);
    REQUIRE(((packed >> kFifoRdataBit) & kFifoRdataMask) == 0x5A);

    // ---- 再推入第 2 个元素 0x3C ----
    // ⚠️ 0-cycle `aread` ⇒ `dout` 跟随 `rd_ptr` ⇒ 此时 `rd_ptr` 仍指向
    //    **队首**（第 1 个元素）⇒ `rdata` 仍是 0x5A。
    sim.set_bundle_value(dev.instance().io_bundle, kPushBit | 0x3C);
    sim.tick();
    packed = sim.get_bundle_value(dev.instance().io_bundle);
    REQUIRE(((packed >> kFifoRdataBit) & kFifoRdataMask) == 0x5A);

    // ---- 弹出第 1 个元素 ----
    // ⚠️ ADR-044 裁决 5：pop 沿**之后** `dout` 指向**下一个**队首 ⇒
    //    弹出 0x5A 之后，`rdata` MUST 变成 **0x3C**（下一个队首）。
    sim.set_bundle_value(dev.instance().io_bundle, kPopBit);
    sim.tick();
    packed = sim.get_bundle_value(dev.instance().io_bundle);
    REQUIRE(((packed >> kFifoRdataBit) & kFifoRdataMask) == 0x3C);
    REQUIRE(field_bit(packed, kFifoEmptyBit) == 0);
}

TEST_CASE("bundle master/slave: slave 产生的 empty 到达 master 侧",
          "[bundle][master-slave][read-timing]") {
    ch_device<BundleFifoTop> dev;
    Simulator sim(dev.context());

    constexpr uint64_t kPushBit = 1ULL << kFifoPushBit;

    // 初始：空（slave 驱动的 empty 穿过 bundle 到达 master）
    sim.set_bundle_value(dev.instance().io_bundle, 0);
    sim.tick();
    uint64_t packed = sim.get_bundle_value(dev.instance().io_bundle);
    REQUIRE(field_bit(packed, kFifoEmptyBit) == 1);

    // 推入一个元素 → empty 应变 0
    sim.set_bundle_value(dev.instance().io_bundle, kPushBit | 0x11);
    sim.tick();
    packed = sim.get_bundle_value(dev.instance().io_bundle);
    REQUIRE(field_bit(packed, kFifoEmptyBit) == 0);

    // ⚠️ `full` 也是 slave 驱动的字段 ⇒ 它同样必须能穿过 bundle 到达 master
    // （本 DUT 深度 4，推 1 个元素不会满 ⇒ 期望 full == 0）
    REQUIRE(field_bit(packed, kFifoFullBit) == 0);
}

TEST_CASE("bundle master/slave: offset 非零的字段（push）能正确传播",
          "[bundle][master-slave][read-timing]") {
    // `push` 在 bundle 位布局的 offset 8（非 0）⇒ 若连接被实现成「取源节点
    // 某个位区间的切片」，传播的值就会错位。
    ch_device<BundleFifoTop> dev;
    Simulator sim(dev.context());

    constexpr uint64_t kPushBit = 1ULL << kFifoPushBit;

    sim.set_bundle_value(dev.instance().io_bundle, 0);
    sim.tick();

    // push=1 → slave 侧写入 → empty 翻转 ⇒ 证明 push 这一位传对了
    sim.set_bundle_value(dev.instance().io_bundle, kPushBit);
    sim.tick();
    uint64_t packed = sim.get_bundle_value(dev.instance().io_bundle);
    REQUIRE(field_bit(packed, kFifoEmptyBit) == 0);
}

// ============================================================================
// 方向指认 —— 必须**直接**断言方向元数据
// ============================================================================
//
// ⚠️ 为什么连接级测试**测不出**方向指认错误：两个方向相同的字段会落进
// 「第三分支」（src 驱动 dst）⇒ 接线结果恰好也是对的。
// ⇒ 「`push` 放错边」这类缺陷 MUST 由**方向元数据**的断言来钉。
// （实测：把 `fifo_bundle` 的 `push` 改回 master IN / slave OUT，连接级用例
//   **全绿** —— 只有本用例能抓到。）
TEST_CASE("bundle: master/slave 的字段方向指认与协议语义一致",
          "[bundle][master-slave][direction]") {
    context ctx("bundle_direction");
    ctx_swap swap(&ctx);

    fifo_bundle<ch_uint<8>> master;
    fifo_bundle<ch_uint<8>> slave;
    master.as_master();
    slave.as_slave();

    // master = 发起方：data（写数据）/ push / pop 由 master 驱动
    REQUIRE(std::holds_alternative<output_direction>(master.data.direction()));
    REQUIRE(std::holds_alternative<output_direction>(master.push.direction()));
    REQUIRE(std::holds_alternative<output_direction>(master.pop.direction()));
    // master = 响应方读状态：full / empty 由 slave 驱动
    REQUIRE(std::holds_alternative<input_direction>(master.full.direction()));
    REQUIRE(std::holds_alternative<input_direction>(master.empty.direction()));
    // ⚠️ ADR-044：`rdata`（读数据）是 master 的**输入**（由 slave 驱动）
    REQUIRE(std::holds_alternative<input_direction>(master.rdata.direction()));

    // slave 侧 MUST 与 master 侧**互为镜像**
    REQUIRE(std::holds_alternative<input_direction>(slave.data.direction()));
    REQUIRE(std::holds_alternative<input_direction>(slave.push.direction()));
    REQUIRE(std::holds_alternative<input_direction>(slave.pop.direction()));
    REQUIRE(std::holds_alternative<output_direction>(slave.full.direction()));
    REQUIRE(std::holds_alternative<output_direction>(slave.empty.direction()));
    // ⚠️ ADR-044：`rdata`（读数据）是 slave 的**输出**（与 master 侧镜像）
    REQUIRE(std::holds_alternative<output_direction>(slave.rdata.direction()));
}

TEST_CASE("bundle: 字段节点身份在方向切换前后 MUST 不变",
          "[bundle][master-slave][node-identity]") {
    context ctx("bundle_node_identity");
    ctx_swap swap(&ctx);

    fifo_bundle<ch_uint<8>> b;
    b.as_master();

    // 记录每个字段的节点身份
    auto *data_node = b.data.impl();
    auto *push_node = b.push.impl();
    auto *pop_node = b.pop.impl();
    auto *full_node = b.full.impl();
    auto *empty_node = b.empty.impl();

    REQUIRE(data_node != nullptr);
    REQUIRE(push_node != nullptr);

    // 切换方向
    b.as_slave();

    // 节点身份 MUST 不变（否则已建立的数据流边会指向被孤儿化的旧节点）
    REQUIRE(b.data.impl() == data_node);
    REQUIRE(b.push.impl() == push_node);
    REQUIRE(b.pop.impl() == pop_node);
    REQUIRE(b.full.impl() == full_node);
    REQUIRE(b.empty.impl() == empty_node);
}

// ============================================================================
// 打包式 API 的宽度约束
// ============================================================================

TEST_CASE("bundle: 64 位宽字段的位段提取 MUST NOT 触发位移溢出",
          "[bundle][master-slave][width]") {
    ch_device<Bundle64Comp> dev;
    Simulator sim(dev.context());

    // 旧实现的位段提取是 `(1ULL << width) - 1` ⇒ width == 64 时是
    // `1ULL << 64` = **未定义行为**。
    // ⚠️ 本用例锁的是**值往返**（正确的实现必须精确往返）；
    // 它**不是** UB 探测器 —— GCC 对常量 `1ULL << 64` 折叠为 0 ⇒ 掩码恰好
    // 正确 ⇒ 值断言无法暴露该 UB。UB 本身靠**实现**避免（`width >= 64`
    // 特判），本用例保证该特判不被误删。
    constexpr uint64_t kValue = 0x0123456789ABCDEFull;
    sim.set_bundle_value(dev.instance().io, kValue);
    REQUIRE(sim.get_bundle_value(dev.instance().io) == kValue);
}

TEST_CASE("bundle: 总宽 > 64 位的打包式 API MUST 显式拒绝而非静默截断",
          "[bundle][master-slave][width]") {
    ch_device<BundleWideComp> dev;
    Simulator sim(dev.context());

    auto *lo_node = dev.instance().io.lo.impl();
    REQUIRE(lo_node != nullptr);

    // 先用**逐字段**的槽位 API 写一个已知值（打包式 API 对它不可用）
    constexpr uint64_t kKnown = 0x1111111111111111ull;
    sim.set_signal_node_value(lo_node, kKnown);
    REQUIRE(sim.get_signal_node_value(lo_node) == kKnown);

    // 打包式 API MUST 被拒绝 ⇒ 字段的值 MUST 不变（而不是被截断后的值覆盖）
    sim.set_bundle_value(dev.instance().io, 0xFFFF);
    REQUIRE(sim.get_signal_node_value(lo_node) == kKnown);
}

// ============================================================================
// DUT 2：未连接 bundle 的槽位读写
// ============================================================================

TEST_CASE("bundle: 未连接 bundle 的字段值可写可读",
          "[bundle][master-slave][slot-access]") {
    ch_device<StandaloneBundle> dev;
    Simulator sim(dev.context());

    // `data` 字段在 offset 0（8 位）⇒ 写 0x5A 必须能读回 0x5A
    sim.set_bundle_value(dev.instance().io, 0x5A);
    uint64_t read_back = sim.get_bundle_value(dev.instance().io);
    REQUIRE((read_back & 0xFF) == 0x5A);

    // `push` 字段在 offset 8 ⇒ 写它也必须能读回
    sim.set_bundle_value(dev.instance().io, 1ULL << kFifoPushBit);
    read_back = sim.get_bundle_value(dev.instance().io);
    REQUIRE(field_bit(read_back, kFifoPushBit) == 1);
}
