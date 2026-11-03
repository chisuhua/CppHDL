// ⚠️⚠️ **ADR-065 的行为判据 —— chlib 仲裁器族的两处「建图期求值」缺陷**
// （`stream_arbiter_lock` + `round_robin_arbiter`，双后端 parity）
//
// ## 为什么这个文件存在
//
// 两个组件此前都**静默给出错误结果**，而既有测试**零判别力**：
//
// | 组件 | 修复前 | 既有测试 |
// |---|---|---|
// | `stream_arbiter_lock` | 选择逻辑用**普通 C++ `if/else`** ⇒ 条件量是硬件值 ⇒ **建图期恒假** ⇒ 恒选通道 0，且**输出别的通道的 payload**（静默数据损坏） | `tests/test_stream_arbiter.cpp` 只断 `.width` + 1 条 `valid==true` ⇒ `selected`/`payload` **从未被断言** |
// | `round_robin_arbiter` | `UpdatePriority` 外层 `if (result.valid)` ⇒ 同样建图期恒假 ⇒ `next_ptr` **恒 0** ⇒ 不是轮转，而是「固定从最低位开始」⇒ 持续有效的低位通道**饿死**高位通道 | `tests/chlib/test_selector_arbiter.cpp` 全裸 `context` ⇒ 动态推进**不可观测** |
//
// ⇒ 修好后 MUST 有判据（ADR-065 D5/D6），否则「修完仍是假绿」。
//
// ## 手推期望（**禁抄仿真输出**）
//
// ### `stream_arbiter_lock` 的规格（ADR-065 D3/D4）
//
//     selected    = locked ? locked_channel
//                          : (has_valid ? priority_encoder(valid_vector) : locked_channel)
//     out.valid   = valid_vector[selected]          # ⚠️ **选中通道**的 valid（不是「任一」）
//     out.payload = payloads[selected]
//     ins[i].ready = (selected == i) & out.ready
//     locked<=1 & locked_channel<=prio   WHEN !locked & has_valid      # 授予即锁
//     locked<=0                          WHEN out.valid & out.ready    # fire 释放
//     ⚠️ `priority_encoder` 是**高位优先**（见 `test_stream_arbiter_dual.cpp` 的留痕）
//
// ### `round_robin_arbiter` 的规格（ADR-065 D2）
//
//     grant    = 从指针开始循环扫描的首个置位请求（one-hot）
//     next_ptr = 被授予位 + 1（环绕）；无请求 ⇒ 保持
//     ⚠️ **读模型**：`tick()` = 沿前组合 → 寄存器提交 → **沿后**组合
//        ⇒ `get_port_value` 读到的是「**指针已前进之后**」的组合值
//        ⇒ 因此 N 拍全 1 请求下 `grant` 序列是 bit1,bit2,…,bit0,… 而不是从 bit0 起。
//
// ⚠️ **判据（N2′）**：断言数(with verilator) == 2 × 断言数(interpreter-only)。
#include "catch_amalgamated.hpp"
#include "dual_backend_fixture.h"

#include "bundle/stream_bundle.h"
#include "ch.hpp"
#include "chlib/selector_arbiter.h"
#include "chlib/stream.h"
#include "chlib/stream_arbiter.h"
#include "component.h"
#include "core/context.h"
#include "simulator.h"

#include <array>
#include <cstdint>

using namespace ch;
using namespace ch::core;
using namespace chlib;

namespace {

constexpr unsigned kN = 3;              // lock arbiter 的通道数
constexpr uint64_t kPayload[3] = {0x11, 0x22, 0x33};

// ---------------------------------------------------------------------------
// 拍表（⚠️ 陷阱 T3：花括号内的逗号会被宏当参数分隔 ⇒ 表 MUST 放文件作用域）
// 字段顺序：{v0, v1, v2, out_ready | selected, out_valid, out_payload,
//            r0_ready, r1_ready, r2_ready}
// ---------------------------------------------------------------------------
struct LockCase {
    uint64_t v0, v1, v2, out_ready;
    uint64_t selected, out_valid, out_payload;
    uint64_t r0, r1, r2;
};

// pair 1：唯一有效通道 = k（k = 0/1/2）⇒ selected/payload/ready 三者都要对
const LockCase kOneHotCases[] = {
    // ch0 唯一有效（自己就绪）⇒ 选中 0，payload 0x11
    {1, 0, 0, 1, 0, 1, 0x11, 1, 0, 0},
    // ch1 唯一有效 ⇒ 选中 1（修复前会选 0 且给出 0x11）
    {0, 1, 0, 1, 1, 1, 0x22, 0, 1, 0},
    // ch2 唯一有效 ⇒ 选中 2
    {0, 0, 1, 1, 2, 1, 0x33, 0, 0, 1},
};

// pair 2：锁存保持 —— ch1 先被锁（下游 not ready），随后 ch0/ch2 变有效
//         ⇒ selected MUST 仍是 1（不被「更高优先」的 ch2 抢走）
const LockCase kHoldCases[] = {
    {0, 1, 0, 0, 1, 1, 0x22, 0, 1, 0}, // 锁到 ch1
    {0, 1, 1, 0, 1, 1, 0x22, 0, 1, 0}, // ch2 也有效 ⇒ 仍 1
    {1, 1, 1, 0, 1, 1, 0x22, 0, 1, 0}, // ch0 也有效 ⇒ 仍 1
};

// pair 3：释放 —— ch1 有效 + 下游 ready ⇒ fire 释放 ⇒ 之后按优先编码（高位优先）选 2
// ⚠️ **读模型**：`tick()` 的最后一步是**沿后**组合求值 ⇒ **fire 那一拍**读到的
//    `selected` 已经是「**释放后**」的仲裁结果（`locked=0` ⇒ 取 `prio_sel`）。
//    ⇒ ⇒ 「fire 期间仍选 1」这件事**不可在 tick 后观测**（它发生在沿前组合里，
//    由 `valid & ready` 决定是否释放）⇒ ⇒ 本表断言的是**释放确实发生了**
//    （第 3 拍起选 2，而非继续锁着 1）。
const LockCase kReleaseCases[] = {
    {0, 1, 0, 0, 1, 1, 0x22, 0, 1, 0}, // 锁到 ch1
    {0, 1, 1, 0, 1, 1, 0x22, 0, 1, 0}, // 保持（更高优先的 ch2 变有效也不换人）
    {0, 1, 1, 1, 2, 1, 0x33, 0, 0, 1}, // fire ⇒ 释放 ⇒ 沿后已改选 2
    {0, 0, 1, 1, 2, 1, 0x33, 0, 0, 1}, // 继续选 2
    {0, 0, 0, 1, 2, 0, 0x33, 0, 0, 0}, // ch2 撤 ⇒ valid 落（selected 保持）
};

// pair 4：锁定通道撤销 valid、其它通道有效 ⇒ out.valid MUST 为 0（**不串数据**）
const LockCase kLockedInvalidCases[] = {
    {1, 0, 0, 0, 0, 1, 0x11, 1, 0, 0}, // 锁到 ch0
    {0, 0, 1, 0, 0, 0, 0x11, 0, 0, 0}, // ch0 撤 + ch2 有效 ⇒ **valid=0**（旧实现给 1 + 0x33）
    {0, 0, 0, 0, 0, 0, 0x11, 0, 0, 0}, // 全无请求 ⇒ valid=0
};

// ---------------------------------------------------------------------------
// 拍表：`round_robin_arbiter<4>`  —— {request | grant, valid, next_ptr}
// ---------------------------------------------------------------------------
struct RrCase {
    uint64_t request;
    uint64_t grant, valid, next_ptr;
};

// pair 5：全 1 请求 ⇒ 指针每拍 +1，grant 跟着轮转（8 拍覆盖两整圈）
const RrCase kRrAllCases[] = {
    {0xF, 0x2, 1, 1}, {0xF, 0x4, 1, 2}, {0xF, 0x8, 1, 3}, {0xF, 0x1, 1, 0},
    {0xF, 0x2, 1, 1}, {0xF, 0x4, 1, 2}, {0xF, 0x8, 1, 3}, {0xF, 0x1, 1, 0},
};

// pair 6：偏请求（bits 0 与 3）⇒ 两者**交替**被服务（修复前恒选 bit0 ⇒ 饿死 bit3）
const RrCase kRrSkewCases[] = {
    {0x9, 0x8, 1, 1}, // ptr=0 ⇒ 授予 bit0 ⇒ ptr→1 ⇒ 沿后从 1 起 = bit3
    {0x9, 0x1, 1, 0}, // ptr=1 ⇒ 授予 bit3 ⇒ ptr→0 ⇒ 沿后从 0 起 = bit0
    {0x9, 0x8, 1, 1},
    {0x9, 0x1, 1, 0},
};

// pair 7：无请求 ⇒ grant=0 / valid=0 / next_ptr **保持**
const RrCase kRrIdleCases[] = {
    {0xF, 0x2, 1, 1}, // 先动起来（ptr → 1）
    {0x0, 0x0, 0, 1}, // 无请求 ⇒ 指针保持 1
    {0x0, 0x0, 0, 1},
    // 恢复请求：ptr=1 起扫 ⇒ 授予 bit3 ⇒ ptr→0 ⇒ **沿后**从 0 起 = bit0
    {0x9, 0x1, 1, 0},
};

// ---------------------------------------------------------------------------
// DUT Ⓐ：`stream_arbiter_lock<ch_uint<8>, 3>`
// ---------------------------------------------------------------------------
class LockArbiterDut : public ch::Component {
public:
    __io(ch_in<ch_bool> v0;
         ch_in<ch_bool> v1;
         ch_in<ch_bool> v2;
         ch_in<ch_uint<8>> p0;
         ch_in<ch_uint<8>> p1;
         ch_in<ch_uint<8>> p2;
         ch_in<ch_bool> consumer_ready;
         ch_out<ch_uint<2>> selected;
         ch_out<ch_bool> out_valid;
         ch_out<ch_uint<8>> out_payload;
         ch_out<ch_bool> r0_ready;
         ch_out<ch_bool> r1_ready;
         ch_out<ch_bool> r2_ready;)

    LockArbiterDut(ch::Component *parent = nullptr,
                   const std::string &name = "lock_arbiter_dut")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        std::array<ch_stream<ch_uint<8>>, kN> ins;
        // ⚠️ 接线纪律：先 `as_master()` ⇒ `ready` 才有节点（组合子要驱动它）
        for (auto &s : ins) {
            s.as_master();
        }
        ins[0].valid = ch_bool(io().v0.impl());
        ins[1].valid = ch_bool(io().v1.impl());
        ins[2].valid = ch_bool(io().v2.impl());
        ins[0].payload = io().p0;
        ins[1].payload = io().p1;
        ins[2].payload = io().p2;

        auto r = stream_arbiter_lock<ch_uint<8>, kN>(ins);

        io().selected = r.selected;
        io().out_valid = r.output_stream.valid;
        io().out_payload = r.output_stream.payload;
        io().r0_ready = r.input_streams[0].ready;
        io().r1_ready = r.input_streams[1].ready;
        io().r2_ready = r.input_streams[2].ready;
        // 消费端 ready ⇒ 覆盖**默认恒就绪**驱动（ADR-054 形态 E）
        ch_bool consumer(ch_bool(io().consumer_ready.impl()));
        r.output_stream.ready <<= consumer;
    }
};

// ---------------------------------------------------------------------------
// DUT Ⓑ：`round_robin_arbiter<4>`（含 `ch_reg` ⇒ 走 `always_ff` 路径）
// ---------------------------------------------------------------------------
class RoundRobinArbiterDut : public ch::Component {
public:
    __io(ch_in<ch_uint<4>> request;
         ch_out<ch_uint<4>> grant;
         ch_out<ch_bool> valid;
         ch_out<ch_uint<4>> next_ptr;)

    RoundRobinArbiterDut(ch::Component *parent = nullptr,
                         const std::string &name = "round_robin_arbiter_dut")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        auto r = round_robin_arbiter<4>(io().request);
        io().grant = r.grant;
        io().valid = r.valid;
        io().next_ptr = r.next_ptr;
    }
};

}  // namespace

// ===========================================================================
// pair 1：选择正确 + 数据不串（唯一有效通道 k，三组）
// ===========================================================================
DUAL_BACKEND_TEST_PAIR(
    LockArbiterDut,
    "stream_arbiter_lock：唯一有效通道 k 时 selected/payload/ready 三者都对"
    "（修复前恒选通道 0 且输出通道 0 的 payload —— 静默数据损坏）",
    "[chlib][stream][arbiter][lock][select][dual][adr065]",
    [&]() {
        auto &io = dev.instance().io();
        for (const auto &tc : kOneHotCases) {
            sim.set_input_value(io.v0, tc.v0);
            sim.set_input_value(io.v1, tc.v1);
            sim.set_input_value(io.v2, tc.v2);
            sim.set_input_value(io.p0, kPayload[0]);
            sim.set_input_value(io.p1, kPayload[1]);
            sim.set_input_value(io.p2, kPayload[2]);
            sim.set_input_value(io.consumer_ready, tc.out_ready);
            sim.tick();
            INFO("v=" << tc.v0 << tc.v1 << tc.v2 << " ready=" << tc.out_ready);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.selected)) ==
                    tc.selected);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.out_valid)) ==
                    tc.out_valid);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.out_payload)) ==
                    tc.out_payload);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.r0_ready)) == tc.r0);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.r1_ready)) == tc.r1);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.r2_ready)) == tc.r2);
        }
    });

// ===========================================================================
// pair 2：锁存保持 —— 授予后 MUST 保持到事务完成
// ===========================================================================
DUAL_BACKEND_TEST_PAIR(
    LockArbiterDut,
    "stream_arbiter_lock：授予后锁存保持（更高的通道变有效也不换人）"
    "（头注释承诺的「stays selected until the transaction completes」）",
    "[chlib][stream][arbiter][lock][hold][dual][adr065]",
    [&]() {
        auto &io = dev.instance().io();
        for (const auto &tc : kHoldCases) {
            sim.set_input_value(io.v0, tc.v0);
            sim.set_input_value(io.v1, tc.v1);
            sim.set_input_value(io.v2, tc.v2);
            sim.set_input_value(io.p0, kPayload[0]);
            sim.set_input_value(io.p1, kPayload[1]);
            sim.set_input_value(io.p2, kPayload[2]);
            sim.set_input_value(io.consumer_ready, tc.out_ready);
            sim.tick();
            INFO("v=" << tc.v0 << tc.v1 << tc.v2 << " ready=" << tc.out_ready);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.selected)) ==
                    tc.selected);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.out_payload)) ==
                    tc.out_payload);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.out_valid)) ==
                    tc.out_valid);
        }
    });

// ===========================================================================
// pair 3：释放 —— `output.fire` 后下一拍可换通道
// ===========================================================================
DUAL_BACKEND_TEST_PAIR(
    LockArbiterDut,
    "stream_arbiter_lock：output.fire 之后释放锁 ⇒ 下一拍按优先编码重新仲裁",
    "[chlib][stream][arbiter][lock][release][dual][adr065]",
    [&]() {
        auto &io = dev.instance().io();
        for (const auto &tc : kReleaseCases) {
            sim.set_input_value(io.v0, tc.v0);
            sim.set_input_value(io.v1, tc.v1);
            sim.set_input_value(io.v2, tc.v2);
            sim.set_input_value(io.p0, kPayload[0]);
            sim.set_input_value(io.p1, kPayload[1]);
            sim.set_input_value(io.p2, kPayload[2]);
            sim.set_input_value(io.consumer_ready, tc.out_ready);
            sim.tick();
            INFO("v=" << tc.v0 << tc.v1 << tc.v2 << " ready=" << tc.out_ready);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.selected)) ==
                    tc.selected);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.out_valid)) ==
                    tc.out_valid);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.out_payload)) ==
                    tc.out_payload);
        }
    });

// ===========================================================================
// pair 4：锁定通道撤销 valid ⇒ out.valid=0（**MUST NOT** 换人 / 串数据）
// ===========================================================================
DUAL_BACKEND_TEST_PAIR(
    LockArbiterDut,
    "stream_arbiter_lock：锁定通道撤销 valid 而其它通道有效 ⇒ out.valid=0"
    "（MUST NOT 返回其它通道的数据）",
    "[chlib][stream][arbiter][lock][valid_gate][dual][adr065]",
    [&]() {
        auto &io = dev.instance().io();
        for (const auto &tc : kLockedInvalidCases) {
            sim.set_input_value(io.v0, tc.v0);
            sim.set_input_value(io.v1, tc.v1);
            sim.set_input_value(io.v2, tc.v2);
            sim.set_input_value(io.p0, kPayload[0]);
            sim.set_input_value(io.p1, kPayload[1]);
            sim.set_input_value(io.p2, kPayload[2]);
            sim.set_input_value(io.consumer_ready, tc.out_ready);
            sim.tick();
            INFO("v=" << tc.v0 << tc.v1 << tc.v2 << " ready=" << tc.out_ready);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.out_valid)) ==
                    tc.out_valid);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.selected)) ==
                    tc.selected);
        }
    });

// ===========================================================================
// pair 5：round_robin_arbiter 全 1 请求 ⇒ 真轮转（8 拍）
// ===========================================================================
DUAL_BACKEND_TEST_PAIR(
    RoundRobinArbiterDut,
    "round_robin_arbiter：全 1 请求下 grant 轮转 + next_ptr 每拍 +1"
    "（修复前 next_ptr 恒 0 ⇒ 固定最低位优先 ⇒ 高位通道饿死）",
    "[chlib][selector][round_robin_arbiter][rotation][dual][adr065]",
    [&]() {
        auto &io = dev.instance().io();
        for (const auto &tc : kRrAllCases) {
            sim.set_input_value(io.request, tc.request);
            sim.tick();
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.grant)) ==
                    tc.grant);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.valid)) ==
                    tc.valid);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.next_ptr)) ==
                    tc.next_ptr);
        }
    });

// ===========================================================================
// pair 6：偏请求 ⇒ 两个请求者都被服务（无饿死）
// ===========================================================================
DUAL_BACKEND_TEST_PAIR(
    RoundRobinArbiterDut,
    "round_robin_arbiter：偏请求（bits 0 与 3）下两请求者交替被服务（无饿死）",
    "[chlib][selector][round_robin_arbiter][fairness][dual][adr065]",
    [&]() {
        auto &io = dev.instance().io();
        for (const auto &tc : kRrSkewCases) {
            sim.set_input_value(io.request, tc.request);
            sim.tick();
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.grant)) ==
                    tc.grant);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.next_ptr)) ==
                    tc.next_ptr);
        }
    });

// ===========================================================================
// pair 7：无请求 ⇒ grant=0 / valid=0 / 指针保持
// ===========================================================================
DUAL_BACKEND_TEST_PAIR(
    RoundRobinArbiterDut,
    "round_robin_arbiter：无请求 ⇒ grant=0 且 next_ptr 保持（不空转）",
    "[chlib][selector][round_robin_arbiter][idle][dual][adr065]",
    [&]() {
        auto &io = dev.instance().io();
        for (const auto &tc : kRrIdleCases) {
            sim.set_input_value(io.request, tc.request);
            sim.tick();
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.grant)) ==
                    tc.grant);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.valid)) ==
                    tc.valid);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.next_ptr)) ==
                    tc.next_ptr);
        }
    });
