// ⚠️⚠️ **路线 B（I 批）—— `chlib::stream_arbiter_priority` / `stream_arbiter_round_robin`
// 进双后端 parity**（2026-11-03）
//
// ## 为什么这批有价值
//
// ① **`stream_arbiter_round_robin` 含 `ch_reg`**（`last_grant_reg`）⇒ ⇒ 走
//    `always_ff` codegen 路径 ⇒ ⇒ 正是两后端最易分叉处；且它的**轮转反馈**
//    是 **ADR-055（2026-11-01）刚修好的**（修前恒传 `last_grant = 0_d`
//    ⇒ ⇒ 扫描起点恒为 1 ⇒ ⇒ **低位通道饿死**）⇒ ⇒ **修复后的语义从未被
//    双后端验证过**。
// ② `stream_arbiter_priority` 的**方向**在本仓是**反直觉**的 ⇒ 见下 ⇒ 必须钉住。
//
// ## 4 问
//
// ① **重复**：两个符号在全部 54 个 dual 文件里**各出现 0 次** ⇒ 无重复。
// ② **判别力**：变异 **7/7 全抓**（见下）。
// ③ **与后端无关**：**否** —— 真跑仿真 + 读顶层端口，正是要对比两后端。
// ④ **值得新写**：既有 `tests/test_stream_arbiter.cpp`（4 用例）与
//    `tests/chlib/test_stream.cpp` 都是**裸 context**（`ch_device=0`）⇒ ⇒
//    命中 **R6 排除项** ⇒ ⇒ **不可转换** ⇒ ⇒ 必须新写（wrapper DUT + 手推期望）。
//    ⚠️ 两个原文件 **MUST NOT 修改**。
//
// ## 🔴 本批钉住的两条「反直觉」语义
//
// ### ① 优先级方向 = **高位优先**（不是低位）
//
// `priority_encoder`（`include/chlib/combinational.h:22-32`）的循环是 `i = 0 → N-1`
// 且每次 `select(bit_at_i, current_value, result)` **覆盖**先前结果 ⇒ ⇒
// **最高**置位胜 ⇒ ⇒ `stream_arbiter_priority` 也是**高位优先**。
// ⚠️ **它的注释写「优先处理低位」⇒ 与实现相反**（独立立项
// `.rddf/improvements/priority-encoder-doc-contradicts-implementation-2026-11.md`）
// ⚠️ 且与同仓 `chlib::priority_selector`（**低位**优先）**方向相反**
// ⇒ ⇒ 「优先级」在本仓含义不唯一 ⇒ ⇒ 本文件按**当前实现**断言并留痕。
//
// ### ② 两个仲裁器的 `ready` 门控**不同**
//
// | | `stream_arbiter_priority` | `stream_arbiter_round_robin` |
// |---|---|---|
// | `ins[i].ready` | `(selected==i) & out.ready` ⇒ ⚠️ **不被 valid 门控** | `(selected==i) & out.ready & valid` ⇒ ✅ **被 valid 门控** |
// | 无请求时 | `selected = priority_encoder(0) = 0` ⇒ ⇒ **通道 0 仍拿到 ready** | `grant = 0` ⇒ `selected = 0` ⇒ ⇒ 但 valid=0 ⇒ ⇒ **全部 ready=0** |
//
// ⇒ ⇒ 「无请求 + 消费端就绪」这一拍上两者**行为不同** ⇒ ⇒ 是判别点。
//
// ## 期望值（**手推**，禁抄仿真输出）
//
// 模型（`/tmp/opencode/gen_arbiter_table.py`，独立 Python 实现）：
//
//     priority:  selected = priority_encoder(valid_vector)   # **最高**位胜
//                out.payload = payloads[selected] ; out.valid = (valid_vector != 0)
//                ins[i].ready = (selected==i) & out.ready     # ⚠️ 无 valid 门控
//
//     round_robin: grant = round_robin_selector(valid_vector, last_grant_reg).grant
//                  # 起点 = (last_idx + 1) % N（last_idx = onehot_to_binary(last_grant)）
//                  selected = priority_encoder(grant)          # grant=0 ⇒ selected=0
//                  ins[i].ready = (selected==i) & out.ready & valid   # ✅ 有 valid 门控
//                  last_grant_reg->next = select(valid, grant, last_grant_reg)
//
// ⚠️ **读模型**（`Simulator::tick`：边沿前组合求值 → 寄存器提交 → 边沿后组合求值）：
// **tick 之后读到的端口 = f(边沿后的寄存器, 本拍输入)** ⇒ ⇒ 对 round_robin 而言，
// 读到的选择用的是**边沿后**的 `last_grant_reg` ⇒ ⇒ 相位序列因此是
// 「**本拍的输入 + 下一拍的相位**」⇒ ⇒ 期望表按此生成（探针实测逐位吻合）。
//
// ⚠️ **`last_grant_reg` 的初值 MUST 是字面量零**（`stream.h:786`）：注释记录了
// 「`ch_reg` 的【表达式】初值在 Verilog codegen 侧失效 ⇒ ⇒ 静默回退 0」
// ⇒ ⇒ 本文件的 round_robin 期望值**依赖**这个字面量零 ⇒ ⇒ 它同时是一条
// **双后端一致性**判据。
//
// ## 激励
//
// `priority`（8 组）：无请求 · 单路 ×3 · **多路有效 ×3**（判别方向）· 消费端不就绪
// `round_robin`（8 组）：三路恒有效 ×3 · **无请求一拍**（相位 MUST 保持 ⇒ ADR-055
// 的「饿死复现」防线）· 再三路有效 ×3 · 消费端不就绪
//
// ⚠️ **陷阱 T3**：body 参数里**花括号内的逗号**会被预处理器当参数分隔
//    ⇒ ⇒ 拍表放**文件作用域**。
// ⚠️ **判据（N2′）**：断言数(with verilator) == **2 ×** 断言数(interpreter-only)。
#include "catch_amalgamated.hpp"
#include "dual_backend_fixture.h"

#include "bundle/stream_bundle.h"
#include "ch.hpp"
#include "chlib/stream.h"
#include "component.h"
#include "core/context.h"
#include "simulator.h"

#include <cstdint>

using namespace ch;
using namespace ch::core;
using namespace chlib;

namespace {

constexpr unsigned kN = 3;

// 三个通道的 payload（互不相同 ⇒ 串源立刻可见）
constexpr uint64_t kP0 = 0xA0;
constexpr uint64_t kP1 = 0xA1;
constexpr uint64_t kP2 = 0xA2;

// ── 拍表：{in_valid 掩码, consumer_ready, selected, out_valid, out_payload,
//          r0_ready, r1_ready, r2_ready} ──
struct ArbiterCase {
    uint64_t in_valid, consumer_ready;
    uint64_t selected, out_valid, out_payload;
    uint64_t r0_ready, r1_ready, r2_ready;
};

const ArbiterCase kPriority[] = {
    // ⚠️ 无请求 + 消费端就绪 ⇒ priority 的 ready **不被 valid 门控** ⇒ 通道 0 拿到 ready
    {0b000, 1, 0, 0, kP0, 1, 0, 0},
    {0b001, 1, 0, 1, kP0, 1, 0, 0},
    {0b010, 1, 1, 1, kP1, 0, 1, 0},
    {0b100, 1, 2, 1, kP2, 0, 0, 1},
    // ⚠️ 多路有效 ⇒ **高位**优先（判别方向）
    {0b011, 1, 1, 1, kP1, 0, 1, 0},
    {0b101, 1, 2, 1, kP2, 0, 0, 1},
    {0b111, 1, 2, 1, kP2, 0, 0, 1},
    // ⚠️ 消费端不就绪 ⇒ 全部 ready=0
    {0b011, 0, 1, 1, kP1, 0, 0, 0},
};

const ArbiterCase kRoundRobin[] = {
    // ⚠️ 读模型 ⇒ 读到的选择用**边沿后**的相位 ⇒ 序列为 2,0,1,...
    {0b111, 1, 2, 1, kP2, 0, 0, 1},
    {0b111, 1, 0, 1, kP0, 1, 0, 0},
    {0b111, 1, 1, 1, kP1, 0, 1, 0},
    // ⚠️ 无请求一拍 ⇒ 相位 MUST 保持（ADR-055 的「饿死复现」防线）⇒ 且 valid=0
    //    ⇒ **全部 ready=0**（round_robin 的 ready **被** valid 门控 ⇒ 与 priority 不同）
    {0b000, 1, 0, 0, kP0, 0, 0, 0},
    {0b111, 1, 2, 1, kP2, 0, 0, 1},
    {0b111, 1, 0, 1, kP0, 1, 0, 0},
    {0b111, 1, 1, 1, kP1, 0, 1, 0},
    // ⚠️ 消费端不就绪 ⇒ 全部 ready=0
    {0b111, 0, 2, 1, kP2, 0, 0, 0},
};

// ---------------------------------------------------------------------------
// DUT Ⓐ：`stream_arbiter_priority`（纯组合）
// ---------------------------------------------------------------------------
class PriorityArbiterDut : public ch::Component {
public:
    __io(ch_in<ch_bool> in0_valid;
         ch_in<ch_uint<8>> in0_payload;
         ch_in<ch_bool> in1_valid;
         ch_in<ch_uint<8>> in1_payload;
         ch_in<ch_bool> in2_valid;
         ch_in<ch_uint<8>> in2_payload;
         ch_in<ch_bool> consumer_ready;
         ch_out<ch_bool> out_valid;
         ch_out<ch_uint<8>> out_payload;
         ch_out<ch_uint<2>> selected;
         ch_out<ch_bool> r0_ready;
         ch_out<ch_bool> r1_ready;
         ch_out<ch_bool> r2_ready;)

    PriorityArbiterDut(ch::Component *parent = nullptr,
                       const std::string &name = "priority_arbiter_dut")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        std::array<ch_stream<ch_uint<8>>, kN> ins;
        // ⚠️ **接线纪律**：先 `as_master()` ⇒ `ready` 才有节点（组合子要驱动它）
        for (auto &s : ins) {
            s.as_master();
        }
        ins[0].valid = ch_bool(io().in0_valid.impl());
        ins[0].payload = io().in0_payload;
        ins[1].valid = ch_bool(io().in1_valid.impl());
        ins[1].payload = io().in1_payload;
        ins[2].valid = ch_bool(io().in2_valid.impl());
        ins[2].payload = io().in2_payload;

        auto r = stream_arbiter_priority<ch_uint<8>, kN>(ins);

        io().out_valid = r.output_stream.valid;
        io().out_payload = r.output_stream.payload;
        io().selected = r.selected;
        // ⚠️ **上游反压的观测点**：组合子用 `<<=` 驱动 ⇒ 调用方真收到
        io().r0_ready = ins[0].ready;
        io().r1_ready = ins[1].ready;
        io().r2_ready = ins[2].ready;
        // 消费端 ready ⇒ 覆盖**默认恒就绪**驱动（ADR-054 形态 E）
        ch_bool consumer(ch_bool(io().consumer_ready.impl()));
        r.output_stream.ready <<= consumer;
    }
};

// ---------------------------------------------------------------------------
// DUT Ⓑ：`stream_arbiter_round_robin`（含 `ch_reg` ⇒ 走 `always_ff` 路径）
// ---------------------------------------------------------------------------
class RoundRobinArbiterDut : public ch::Component {
public:
    __io(ch_in<ch_bool> in0_valid;
         ch_in<ch_uint<8>> in0_payload;
         ch_in<ch_bool> in1_valid;
         ch_in<ch_uint<8>> in1_payload;
         ch_in<ch_bool> in2_valid;
         ch_in<ch_uint<8>> in2_payload;
         ch_in<ch_bool> consumer_ready;
         ch_out<ch_bool> out_valid;
         ch_out<ch_uint<8>> out_payload;
         ch_out<ch_uint<2>> selected;
         ch_out<ch_bool> r0_ready;
         ch_out<ch_bool> r1_ready;
         ch_out<ch_bool> r2_ready;)

    RoundRobinArbiterDut(ch::Component *parent = nullptr,
                         const std::string &name = "round_robin_arbiter_dut")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        std::array<ch_stream<ch_uint<8>>, kN> ins;
        for (auto &s : ins) {
            s.as_master();
        }
        ins[0].valid = ch_bool(io().in0_valid.impl());
        ins[0].payload = io().in0_payload;
        ins[1].valid = ch_bool(io().in1_valid.impl());
        ins[1].payload = io().in1_payload;
        ins[2].valid = ch_bool(io().in2_valid.impl());
        ins[2].payload = io().in2_payload;

        auto r = stream_arbiter_round_robin<ch_uint<8>, kN>(ins);

        io().out_valid = r.output_stream.valid;
        io().out_payload = r.output_stream.payload;
        io().selected = r.selected;
        io().r0_ready = ins[0].ready;
        io().r1_ready = ins[1].ready;
        io().r2_ready = ins[2].ready;
        ch_bool consumer(ch_bool(io().consumer_ready.impl()));
        r.output_stream.ready <<= consumer;
    }
};

}  // namespace

// ===========================================================================
// pair 1：`stream_arbiter_priority` —— **高位优先** + ready **不被 valid 门控**
// ===========================================================================
DUAL_BACKEND_TEST_PAIR(
    PriorityArbiterDut,
    "stream_arbiter_priority：高位优先（多路有效时）/ 只有被选中通道 ready / 无请求时通道 0 仍 ready",
    "[chlib][stream][arbiter][priority][grant][ready][dual]",
    [&]() {
        auto &io = dev.instance().io();
        for (const auto &tc : kPriority) {
            sim.set_input_value(io.in0_valid, (tc.in_valid >> 0) & 1u);
            sim.set_input_value(io.in1_valid, (tc.in_valid >> 1) & 1u);
            sim.set_input_value(io.in2_valid, (tc.in_valid >> 2) & 1u);
            sim.set_input_value(io.in0_payload, kP0);
            sim.set_input_value(io.in1_payload, kP1);
            sim.set_input_value(io.in2_payload, kP2);
            sim.set_input_value(io.consumer_ready, tc.consumer_ready);
            sim.tick();
            INFO("in_valid=0b" << std::hex << tc.in_valid << std::dec
                               << " consumer_ready=" << tc.consumer_ready);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.selected)) ==
                    tc.selected);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.out_valid)) ==
                    tc.out_valid);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.out_payload)) ==
                    tc.out_payload);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.r0_ready)) ==
                    tc.r0_ready);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.r1_ready)) ==
                    tc.r1_ready);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.r2_ready)) ==
                    tc.r2_ready);
        }
    });

// ===========================================================================
// pair 2：`stream_arbiter_round_robin` —— 轮转 + 无请求时相位保持
// ===========================================================================
DUAL_BACKEND_TEST_PAIR(
    RoundRobinArbiterDut,
    "stream_arbiter_round_robin：真轮转（序列 2,0,1…）/ 无请求时相位保持 / ready 被 valid 门控",
    "[chlib][stream][arbiter][round_robin][grant][ready][dual]",
    [&]() {
        auto &io = dev.instance().io();
        for (const auto &tc : kRoundRobin) {
            sim.set_input_value(io.in0_valid, (tc.in_valid >> 0) & 1u);
            sim.set_input_value(io.in1_valid, (tc.in_valid >> 1) & 1u);
            sim.set_input_value(io.in2_valid, (tc.in_valid >> 2) & 1u);
            sim.set_input_value(io.in0_payload, kP0);
            sim.set_input_value(io.in1_payload, kP1);
            sim.set_input_value(io.in2_payload, kP2);
            sim.set_input_value(io.consumer_ready, tc.consumer_ready);
            sim.tick();
            INFO("in_valid=0b" << std::hex << tc.in_valid << std::dec
                               << " consumer_ready=" << tc.consumer_ready);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.selected)) ==
                    tc.selected);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.out_valid)) ==
                    tc.out_valid);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.out_payload)) ==
                    tc.out_payload);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.r0_ready)) ==
                    tc.r0_ready);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.r1_ready)) ==
                    tc.r1_ready);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.r2_ready)) ==
                    tc.r2_ready);
        }
    });
