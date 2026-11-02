// ⚠️⚠️ **路线 B（A5 的 H 批）第 3 批**（2026-11-02 · change
// `dual-backend-parity-route-b-fifo-2026-11`）
//
// ## 为什么这个文件【值得】存在（立项理由，不是凑数）
//
// `chlib::sync_fifo` 是**所有** FIFO 型组件的本体（`stream_fifo` /
// `StreamBuilder::queue` / 两个 cache 的队列都建在它上面），而它的
// **空/满门控（自保护）** 在 **verilator codegen 路径上从未被验证过**。
//
// ⚠️ **这个缺陷类是仓内真实发生过的**（ADR-045 / change
// `chlib-stream-semantics-2026-10`）：`sync_fifo` 此前对**写口**与**双指针**
// 都**没有**空/满门控 ⇒
//   · **满 FIFO 再写** ⇒ 写指针回绕 ⇒ **覆写已写入的数据**（静默损坏）；
//   · **空 FIFO 空读** ⇒ 读指针**每拍自由运行** ⇒ 与写指针失步 ⇒ 之后写入的
//     元素**永远读不出来**（`count` 归零，但读到的是错槽）。
// ⇒ ⇒ 这两条都**只在 interpreter 上被钉过**（`tests/chlib/test_fifo.cpp` 的
// `[fifo][sync_fifo][regression]` 组）⇒ ⇒ **codegen 侧零覆盖**。
// ⇒ ⇒ 而「指针门控」恰恰是**寄存器 + 组合条件**的混合体，正是两条实现路径
// 最容易分叉的地方。
//
// ⚠️ **为什么不用既有 `tests/test_stream_fifo_dual.cpp`**：它覆盖的是
// `stream_fifo`（**wrapper**，且读使能由外部驱动）⇒ ⇒ **不是** `sync_fifo`
// 本体 ⇒ ⇒ 本体的 `will_write` / `will_read` 门控从未在 verilator 下跑过。
//
// ## 为什么是【新增】而不是转换
//
// `tests/chlib/test_fifo.cpp` 是**裸 context + 直接调 `sync_fifo()`** 的形态
// （无 `ch_device` / 无顶层端口）⇒ ⇒ `DUAL_BACKEND_TEST_PAIR` **不适用**
// （macro 需要 `ch_device<DutType>`）⇒ ⇒ 必须**新建**一个带端口的 DUT 包一层。
// ⇒ 原文件 **MUST NOT 修改**。
//
// ## 期望值（**手推**，禁抄仿真输出）
//
// 语义前提（逐条来自 `include/chlib/fifo.h` 的实现，不是猜的）：
//   ① `will_write = wren & !full(count)` · `will_read = rden & !empty(count)`
//      ⇒ ⇒ **门控在写口与读指针上**（这正是本文件要钉的东西）；
//   ② `count` / `write_ptr` / `read_ptr` 都是 `ch_reg` ⇒ **同沿**推进；
//   ③ `q_async` 是 **0-cycle 组合**读（`aread`）⇒ 呈现**当前** `read_ptr` 指向的
//      槽 ⇒ ⇒ **沿后**读到的是「沿后指针」指向的元素；
//   ④ `memory.write` 在沿上写入（受 `will_write` 门控）。
//
// ⇒ ⇒ **读数 MUST 用「rden=0 采样」两步法**：`q_async` 在 pop 沿**之后**就
// 指向**下一个**队首（AGENTS.md 的 FIFO 协议）⇒ ⇒ 「要看的元素」必须在
// **pop 之前**采样 ⇒ 所以节奏是「rden=0 采样 → rden=1 推进 → rden=0 采样」。
//
// 深度 = `1 << ADDR_WIDTH` = **4**（`sync_fifo<8, 2>`）。
//
// ⚠️ **判据（N2′）**：断言数(with verilator) == **2 ×** 断言数(interpreter-only)。
#include "catch_amalgamated.hpp"

#include "chlib/fifo.h"
#include "component.h"
#include "core/bool.h"
#include "core/context.h"
#include "core/uint.h"
#include "device.h"
#include "dual_backend_fixture.h"
#include "simulator.h"

using namespace ch;
using namespace ch::core;
using namespace chlib;

namespace {

/// DUT：`sync_fifo<8, 2>`（深度 **4**）包一层带端口的组件。
///
/// ⚠️ **`q_async`（0-cycle 组合）+ `count` / `empty` / `full`** 全部作为端口
///    引出 ⇒ 观测面不依赖内部节点（verilator 侧看不到内部）。
class SyncFifoDut : public ch::Component {
public:
    __io(ch_in<ch_uint<1>> wren; ch_in<ch_uint<8>> din; ch_in<ch_uint<1>> rden;
         ch_out<ch_uint<8>> dout; ch_out<ch_uint<3>> count;
         ch_out<ch_bool> empty; ch_out<ch_bool> full;)

    SyncFifoDut(ch::Component *parent = nullptr,
                const std::string &name = "sync_fifo_dut")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        SyncFifoResult<8, 2> f = sync_fifo<8, 2>(io().wren, io().din, io().rden);
        io().dout = f.q_async;   // ⚠️ 0-cycle 组合视图（**不是** 1-cycle 的 `q`）
        io().count = f.count;
        io().empty = f.empty;
        io().full = f.full;
    }
};

// ⚠️ **用例表 MUST 放在文件作用域**：`DUAL_BACKEND_TEST_PAIR` 的 body 参数里
//    **花括号内的逗号**会被预处理器当成**参数分隔**（只有**圆括号**才保护）
//    ⇒ ⇒ 数组初始化的逗号会让宏收到 11 / 13 个参数 ⇒ 编译失败。
//
// 手推金标（深度 4；写入 0x11/0x22/0x33/0x44）
const uint64_t kWriteVals[4] = {0x11, 0x22, 0x33, 0x44};
// pair 1：每次 pop 沿**之后** `q_async` 呈现的队首
const uint64_t kAfterPopP1[3] = {0x22, 0x33, 0x44};
const uint64_t kCountP1[3] = {3, 2, 1};
// pair 2：排空 4 个（最后一次弹完读指针回绕到 0 ⇒ 队首又回到 mem[0] = 0x11）
const uint64_t kAfterPopP2[4] = {0x22, 0x33, 0x44, 0x11};
const uint64_t kCountP2[4] = {3, 2, 1, 0};

}  // namespace

// ───────────────────── pair 1：顺序 + 数据完整性 ─────────────────────
DUAL_BACKEND_TEST_PAIR(
    SyncFifoDut,
    "sync_fifo - 写满 4 个再按序读出（顺序与数据完整性）",
    "[chlib][fifo][sync_fifo][dual][route_b]",
    [&]() {
        auto &io = dev.instance().io();
        // ⚠️ 空时 `dout` 是 mem 未初始化内容 ⇒ **MUST NOT** 断言（don't-care）

        // 逐拍写入 0x11 / 0x22 / 0x33 / 0x44（rden 恒 0）
        for (int i = 0; i < 4; ++i) {
            sim.set_input_value(io.wren, 1u);
            sim.set_input_value(io.rden, 0u);
            sim.set_input_value(io.din, kWriteVals[i]);
            sim.tick();
            INFO("write tick " << (i + 1));
            // 手推：`q_async` 是 0-cycle ⇒ 沿后立即呈现 mem[rp=0] = 第一个元素
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.dout)) == 0x11u);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.count)) ==
                    static_cast<uint64_t>(i + 1));
        }
        // 满：count == 4 ⇒ full = 1 · empty = 0
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.full)) == 1u);
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.empty)) == 0u);

        // 停写一拍，采样队首（rden=0 ⇒ 指针不动）
        sim.set_input_value(io.wren, 0u);
        sim.set_input_value(io.rden, 0u);
        sim.tick();
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.dout)) == 0x11u);
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.count)) == 4u);

        // 逐个弹出：每弹一次 → 停一拍采样（pop 沿后 `q_async` 已是**下一个**队首）
        for (int i = 0; i < 3; ++i) {
            sim.set_input_value(io.wren, 0u);
            sim.set_input_value(io.rden, 1u);
            sim.tick();
            INFO("pop tick " << (i + 1));
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.dout)) ==
                    kAfterPopP1[i]);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.count)) ==
                    kCountP1[i]);
        }
        // 弹出最后一个 ⇒ count 归零 ⇒ empty = 1
        sim.set_input_value(io.rden, 1u);
        sim.tick();
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.count)) == 0u);
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.empty)) == 1u);
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.full)) == 0u);
    });

// ───────────────────── pair 2：空/满门控（自保护） ─────────────────────
DUAL_BACKEND_TEST_PAIR(
    SyncFifoDut,
    "sync_fifo - 满写 MUST NOT 覆盖 / 空读 MUST NOT 移动指针（自保护）",
    "[chlib][fifo][sync_fifo][dual][self_protect][route_b]",
    [&]() {
        auto &io = dev.instance().io();
        // 先填满 4 个：0x11 / 0x22 / 0x33 / 0x44
        for (int i = 0; i < 4; ++i) {
            sim.set_input_value(io.wren, 1u);
            sim.set_input_value(io.rden, 0u);
            sim.set_input_value(io.din, kWriteVals[i]);
            sim.tick();
        }
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.count)) == 4u);

        // **满写两次** ⇒ MUST 被丢弃（`will_write = wren & !full`）
        // ⚠️ 判别性：若写口用**裸 `wren`** ⇒ mem[0] 被 0x99 覆写 ⇒ `dout` 变 0x99
        for (int i = 0; i < 2; ++i) {
            sim.set_input_value(io.wren, 1u);
            sim.set_input_value(io.rden, 0u);
            sim.set_input_value(io.din, 0x99u);
            sim.tick();
            INFO("overflow write " << (i + 1));
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.count)) == 4u);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.dout)) == 0x11u);
        }

        // 排空：逐拍弹（沿后 `q_async` = 下一个队首）
        for (int i = 0; i < 4; ++i) {
            sim.set_input_value(io.wren, 0u);
            sim.set_input_value(io.rden, 1u);
            sim.tick();
            INFO("drain " << (i + 1));
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.count)) == kCountP2[i]);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.dout)) ==
                    kAfterPopP2[i]);
        }

        // **空读两拍** ⇒ MUST 不移动读指针（`will_read = rden & !empty`）
        // ⚠️ 判别性：若读指针自由运行 ⇒ 之后写入的元素会落在错的槽 ⇒ 下面读不回 0x55
        for (int i = 0; i < 2; ++i) {
            sim.set_input_value(io.wren, 0u);
            sim.set_input_value(io.rden, 1u);
            sim.tick();
            INFO("empty read " << (i + 1));
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.count)) == 0u);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.empty)) == 1u);
        }

        // 空读之后写入 ⇒ MUST 能被原样读回（证明**双指针仍对齐**）
        sim.set_input_value(io.wren, 1u);
        sim.set_input_value(io.rden, 0u);
        sim.set_input_value(io.din, 0x55u);
        sim.tick();
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.count)) == 1u);
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.dout)) == 0x55u);

        // 再写一个 0x66 ⇒ 队列 [0x55, 0x66]
        sim.set_input_value(io.din, 0x66u);
        sim.tick();
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.count)) == 2u);
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.dout)) == 0x55u);

        // 弹一个 ⇒ 队首变成 0x66（顺序正确 ⇒ 写指针也未失步）
        sim.set_input_value(io.wren, 0u);
        sim.set_input_value(io.rden, 1u);
        sim.tick();
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.count)) == 1u);
        REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.dout)) == 0x66u);
    });
