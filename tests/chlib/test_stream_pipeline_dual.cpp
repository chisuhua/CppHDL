// ⚠️⚠️ **路线 B（G 批）—— `chlib` 三个 stream pipe 进双后端 parity**（2026-11-03）
//
// ## 为什么这批价值最高
//
// ① **`stream_pipeline.h` 的三个 pipe 在 ADR-055（2026-11-01）里刚被重写**，
//    且重写**修掉了三个真缺陷**（m2s 的「下游一停顿就丢数据」· s2m 的
//    「寄存的是上游自己的 ready ⇒ 方向反了的自环」· half 的
//    「toggle 由传输驱动 ⇒ 无数据时死锁」）⇒ ⇒ **重写后的语义从未被
//    双后端验证过** ⇒ ⇒ 生成的 Verilog（`always_ff` + `select` 链）与
//    interpreter 是否逐周期一致 = **完全未知**。
// ② **含时序状态**（`ch_reg`）⇒ ⇒ 走的是 `always_ff` codegen 路径
//    ⇒ ⇒ 正是两后端最容易分叉的地方（对比：纯组合的组合子没有这条风险）。
// ③ **有生产消费者**：`include/bundle/stream_bundle_member_inlines.h` 的
//    `m2sPipe()` / `s2mPipe()` / `halfPipe()` 三个成员函数都建在它们之上。
//
// ## 4 问
//
// ① **重复**：全仓 54 个 dual 文件里 `stream_m2s_pipe` / `stream_s2m_pipe` /
//    `stream_half_pipe` 各出现 **0** 次 ⇒ 无重复。
// ② **判别力**：7 条变异（见文件末尾的判别力清单）⇒ 见下。
// ③ **与后端无关**：**否** —— 真跑仿真 + 读顶层端口，正是要对比两后端。
// ④ **值得新写**：既有测试 `tests/test_stream_pipeline.cpp`（12 用例 / 68 断言）
//    **全是裸 context**（`ch_device=0`、无顶层端口）⇒ ⇒ 命中 **R6 排除项**
//    ⇒ ⇒ **结构上不可转换** ⇒ ⇒ 必须新写（自带 wrapper DUT + 手推期望）。
//    ⚠️ 原文件 **MUST NOT 修改**。
//
// ## DUT 形态（wrapper）
//
// 三个 pipe 都是**函数式组合子**（接收 `ch_stream<T>`，返回 result struct），
// **不是** `Component` ⇒ ⇒ 需要一层**端口 wrapper**（本文件的 3 个 DUT 类）。
// ⚠️ **不是「DUT 复制」**：wrapper 只做「端口 ↔ stream 字段」的适配，
//    被测的语义（`accept` / `valid_reg` / `payload_reg` / `toggle_reg` /
//    `out_free` / `ready_reg`）**全部**在库组件里 ⇒ ⇒ **测的就是生产代码本身**。
//    先例：`tests/chlib/test_stream_builder_backpressure_dual.cpp`（同型 wrapper）。
//
// ## ⚠️ 读模型（`Simulator::tick()` 的精确顺序 · `src/simulator.cpp:308-328`）
//
//     1) eval_combinational()   ← **边沿前**
//     2) 寄存器提交（用边沿前的组合值）
//     3) eval_combinational()   ← **边沿后**
//
// ⇒ ⇒ **tick 之后读到的端口 = 边沿后的状态**（与 `ch_reg` 同约定）⇒ ⇒
// 「tick 后即可见」是**框架约定**，不是 pipe 的语义 ⇒ ⇒ 既有单后端测试
// （`tests/test_stream_pipeline.cpp:30-33`：tick 一次即见 `0x42`）与本文件一致。
//
// ## 期望值（**手推**，禁抄仿真输出）
//
// 模型由 `/tmp/opencode/gen_pipe_table.py`（独立 Python 实现）逐拍生成：
//
//     m2s:  out_free_pre = (!pre_valid) or R
//           accept       = out_free_pre and in_valid
//           post_valid   = accept or (pre_valid and not R)
//           post_payload = in_payload if accept else pre_payload
//           观测: out_valid=post_valid  out_payload=post_payload
//                 in_ready = (!post_valid) or R      ← 用**边沿后**的寄存器重算
//
//     s2m:  post_ready   = R                        （ready_reg <- output.ready）
//           观测: out_valid=in_valid  out_payload=in_payload
//                 in_ready = post_ready
//
//     half: out_free_pre = (!pre_valid) or R
//           accept       = pre_toggle and out_free_pre
//           take         = accept and in_valid
//           post_toggle  = not pre_toggle
//           post_valid   = take or (pre_valid and not R)
//           post_payload = in_payload if take else pre_payload
//           观测: out_valid=post_valid  out_payload=post_payload
//                 in_ready = post_toggle and ((!post_valid) or R)
//
// ### 激励（8 拍，三个 pipe 共用）
//
//     c1: (in_valid=0, payload=0x00, consumer_ready=0)   ← **空缓冲 + 消费端不就绪**
//     c2: (1, 0xA1, 1)   c3: (0, 0x00, 1)
//     c4: (1, 0xB2, 0)   c5: (1, 0xC3, 0)   c6: (1, 0xC3, 1)
//     c7: (0, 0x00, 1)   c8: (1, 0xD4, 1)
//
// ⚠️ **c1 是 m2s 与 s2m 的判别点**：空缓冲 + 消费端不就绪 ⇒
//    **m2s** 的 `in_ready` = `(!post_valid) or R` = **1**（缓冲还有空位 ⇒ 上游能推）
//    而 **s2m** 的 `in_ready` = **R** = **0**（ready 是寄存的 ⇒ 上游被挡住）
//    ⇒ ⇒ 若把 s2m 的 `in_ready` 误改成组合直通 `output.ready`，c1 会读到 1 ⇒ 转红。
//
// ### 三个 pipe 各自钉住的语义
//
// | 维度 | m2s | s2m | half |
// |---|---|---|---|
// | **1 拍延迟**（`out_valid` 在捕获拍的**下一次读**才为 1） | ✅ | ❌（payload/valid 组合直通） | ✅ |
// | **下游停顿 ⇒ 数据保持**（`out_payload` 不变 + `out_valid` 保持 1） | ✅ c4→c5 | ❌（无缓冲） | ✅ c5→c6 |
// | **缓冲满 ⇒ 上游被挡住**（`in_ready`=0） | ✅ c4/c5 | ❌ | ✅ c5/c6 |
// | **半带宽**（隔拍才捕获） | ❌ | ❌ | ✅（toggle 自由运行 ⇒ 隔拍） |
// | **ready 的来源** | `!valid_reg or output.ready`（组合） | `output.ready`（寄存） | `toggle & out_free`（组合） |
//
// ⚠️ **如实登记的判别力边界（不是缺陷，是读模型的固有限制）**：
// ① **s2m 的「ready 被寄存」这条在读模型下与「组合直通」不可区分**
//    （两者都观测到**本拍**的 R）⇒ ⇒ 本文件钉的是「`in_ready` **跟随**消费端
//    ready 且**非零**」，**不是**「它被寄存了」⇒ ⇒ 要钉「寄存」需**另一个观察点**
//    （例如在两次 tick 之间改 R 再读，但 `get_port_value` 只读 `data_map_`，
//    组合值不会自动重算 ⇒ ⇒ 需要框架级的 eval 触发点 ⇒ **独立议题**）。
// ② **m2s / s2m 的「1 拍延迟」在端口读值上与 `ch_reg` 同约定**
//    （tick 后即可见）⇒ ⇒ 本文件钉的是「c1 时 `out_valid` 仍为 0、c2 才为 1」
//    这个**跨拍的相对关系**，而非绝对的「晚一拍」。
//
// ⚠️ **陷阱 T3**：body 参数里**花括号内的逗号**会被预处理器当参数分隔
//    ⇒ ⇒ 拍表放**文件作用域**。
// ⚠️ **判据（N2′）**：断言数(with verilator) == **2 ×** 断言数(interpreter-only)。
#include "catch_amalgamated.hpp"
#include "dual_backend_fixture.h"

#include "bundle/stream_bundle.h"
#include "ch.hpp"
#include "chlib/stream_pipeline.h"
#include "component.h"
#include "core/context.h"
#include "simulator.h"

#include <cstdint>

using namespace ch;
using namespace ch::core;
using namespace chlib;

namespace {

// ── 拍表（8 拍，三个 pipe 共用；期望值见文件头的模型）──
// {in_valid, in_payload, consumer_ready, in_ready, out_valid, out_payload}
struct PipeStep {
    uint64_t in_valid, in_payload, consumer_ready;
    uint64_t in_ready, out_valid, out_payload;
};

const PipeStep kM2S[] = {
    {0, 0x00, 0, 1, 0, 0x00},  // c1 空缓冲 + 消费端不就绪 ⇒ in_ready=1（m2s 专属）
    {1, 0xA1, 1, 1, 1, 0xA1},  // c2 捕获 0xA1 ⇒ 下一次读可见
    {0, 0x00, 1, 1, 0, 0xA1},  // c3 被消费掉 ⇒ valid 归 0，payload 仍留在寄存器
    {1, 0xB2, 0, 0, 1, 0xB2},  // c4 消费端停顿 ⇒ 捕获 0xB2，in_ready 拉低
    {1, 0xC3, 0, 0, 1, 0xB2},  // c5 ⚠️ 数据**保持** 0xB2（不丢！）+ in_ready=0
    {1, 0xC3, 1, 1, 1, 0xC3},  // c6 消费端恢复 ⇒ 0xC3 入缓冲
    {0, 0x00, 1, 1, 0, 0xC3},  // c7 又被消费掉
    {1, 0xD4, 1, 1, 1, 0xD4},  // c8 捕获 0xD4
};

const PipeStep kS2M[] = {
    {0, 0x00, 0, 0, 0, 0x00},  // c1 ⚠️ in_ready=0（ready 是**寄存**的 R）
    {1, 0xA1, 1, 1, 1, 0xA1},  // c2 payload/valid **组合直通**（0 拍）
    {0, 0x00, 1, 1, 0, 0x00},  // c3
    {1, 0xB2, 0, 0, 1, 0xB2},  // c4 in_ready 跟随 R=0
    {1, 0xC3, 0, 0, 1, 0xC3},  // c5
    {1, 0xC3, 1, 1, 1, 0xC3},  // c6
    {0, 0x00, 1, 1, 0, 0x00},  // c7
    {1, 0xD4, 1, 1, 1, 0xD4},  // c8
};

const PipeStep kHALF[] = {
    {0, 0x00, 0, 1, 0, 0x00},  // c1 post_toggle=1 ⇒ in_ready=1（隔拍相位）
    {1, 0xA1, 1, 0, 1, 0xA1},  // c2 ⚠️ toggle=1 ⇒ **这一拍才捕获**
    {0, 0x00, 1, 1, 0, 0xA1},  // c3 被消费掉
    {1, 0xB2, 0, 0, 1, 0xB2},  // c4 捕获 0xB2
    {1, 0xC3, 0, 0, 1, 0xB2},  // c5 ⚠️ 数据**保持** 0xB2 + in_ready=0
    {1, 0xC3, 1, 0, 1, 0xC3},  // c6 消费端恢复 ⇒ 0xC3 入缓冲
    {0, 0x00, 1, 1, 0, 0xC3},  // c7 又被消费掉
    {1, 0xD4, 1, 0, 1, 0xD4},  // c8 捕获 0xD4
};

// ---------------------------------------------------------------------------
// 三个 wrapper DUT（只做「端口 ↔ stream 字段」适配；语义全在库组件里）
// ---------------------------------------------------------------------------

// `stream_m2s_pipe`：master→slave，valid/payload 各延迟 1 拍
class M2SPipeDut : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> in_payload;
         ch_in<ch_bool> in_valid;
         ch_in<ch_bool> consumer_ready;
         ch_out<ch_uint<8>> out_payload;
         ch_out<ch_bool> out_valid;
         ch_out<ch_bool> in_ready;)

    M2SPipeDut(ch::Component *parent = nullptr,
               const std::string &name = "m2s_pipe_dut")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_stream<ch_uint<8>> src;
        // ⚠️ **接线纪律**：先 `as_master()` ⇒ `src.ready` 才有节点 ⇒ 否则
        //    `detail::drive_upstream_ready` 会**响亮拒绝**（`no_node`）
        src.as_master();
        src.payload = io().in_payload;
        src.valid = ch_bool(io().in_valid.impl());

        auto r = stream_m2s_pipe(src);

        io().out_payload = r.output.payload;
        io().out_valid = r.output.valid;
        // ⚠️ **上游反压的观测点** = 组合子驱动的那个节点（`src.ready`）
        io().in_ready = src.ready;
        // ⚠️ 消费端 ready ⇒ 覆盖 pipe 的**默认恒就绪**驱动（ADR-054 形态 E）
        ch_bool consumer(ch_bool(io().consumer_ready.impl()));
        r.output.ready <<= consumer;
    }
};

// `stream_s2m_pipe`：slave→master，payload/valid 组合直通（0 拍），ready 寄存 1 拍
class S2MPipeDut : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> in_payload;
         ch_in<ch_bool> in_valid;
         ch_in<ch_bool> consumer_ready;
         ch_out<ch_uint<8>> out_payload;
         ch_out<ch_bool> out_valid;
         ch_out<ch_bool> in_ready;)

    S2MPipeDut(ch::Component *parent = nullptr,
               const std::string &name = "s2m_pipe_dut")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_stream<ch_uint<8>> src;
        src.as_master();
        src.payload = io().in_payload;
        src.valid = ch_bool(io().in_valid.impl());

        auto r = stream_s2m_pipe(src);

        io().out_payload = r.output.payload;
        io().out_valid = r.output.valid;
        io().in_ready = src.ready;
        ch_bool consumer(ch_bool(io().consumer_ready.impl()));
        r.output.ready <<= consumer;
    }
};

// `stream_half_pipe`：全寄存 + 隔拍才捕获（半带宽）
class HalfPipeDut : public ch::Component {
public:
    __io(ch_in<ch_uint<8>> in_payload;
         ch_in<ch_bool> in_valid;
         ch_in<ch_bool> consumer_ready;
         ch_out<ch_uint<8>> out_payload;
         ch_out<ch_bool> out_valid;
         ch_out<ch_bool> in_ready;)

    HalfPipeDut(ch::Component *parent = nullptr,
                const std::string &name = "half_pipe_dut")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_stream<ch_uint<8>> src;
        src.as_master();
        src.payload = io().in_payload;
        src.valid = ch_bool(io().in_valid.impl());

        auto r = stream_half_pipe(src);

        io().out_payload = r.output.payload;
        io().out_valid = r.output.valid;
        io().in_ready = src.ready;
        ch_bool consumer(ch_bool(io().consumer_ready.impl()));
        r.output.ready <<= consumer;
    }
};

}  // namespace

// ===========================================================================
// pair 1：`stream_m2s_pipe` —— 1 拍延迟 + 停顿保持 + 缓冲满挡住上游
// ===========================================================================
DUAL_BACKEND_TEST_PAIR(
    M2SPipeDut,
    "stream_m2s_pipe：1 拍延迟 / 下游停顿时数据保持 / 缓冲满则上游被挡住",
    "[chlib][stream][pipeline][m2s][ready][dual]",
    [&]() {
        auto &io = dev.instance().io();
        for (const auto &tc : kM2S) {
            sim.set_input_value(io.in_valid, tc.in_valid);
            sim.set_input_value(io.in_payload, tc.in_payload);
            sim.set_input_value(io.consumer_ready, tc.consumer_ready);
            sim.tick();
            INFO("in_valid=" << tc.in_valid << " in_payload=0x" << std::hex
                             << tc.in_payload << std::dec
                             << " consumer_ready=" << tc.consumer_ready);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.in_ready)) ==
                    tc.in_ready);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.out_valid)) ==
                    tc.out_valid);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.out_payload)) ==
                    tc.out_payload);
        }
    });

// ===========================================================================
// pair 2：`stream_s2m_pipe` —— payload/valid 组合直通 + ready 跟随消费端
// ===========================================================================
DUAL_BACKEND_TEST_PAIR(
    S2MPipeDut,
    "stream_s2m_pipe：payload/valid 组合直通（0 拍）/ in_ready 跟随消费端 ready",
    "[chlib][stream][pipeline][s2m][ready][dual]",
    [&]() {
        auto &io = dev.instance().io();
        for (const auto &tc : kS2M) {
            sim.set_input_value(io.in_valid, tc.in_valid);
            sim.set_input_value(io.in_payload, tc.in_payload);
            sim.set_input_value(io.consumer_ready, tc.consumer_ready);
            sim.tick();
            INFO("in_valid=" << tc.in_valid << " in_payload=0x" << std::hex
                             << tc.in_payload << std::dec
                             << " consumer_ready=" << tc.consumer_ready);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.in_ready)) ==
                    tc.in_ready);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.out_valid)) ==
                    tc.out_valid);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.out_payload)) ==
                    tc.out_payload);
        }
    });

// ===========================================================================
// pair 3：`stream_half_pipe` —— 隔拍捕获（半带宽）+ 停顿保持
// ===========================================================================
DUAL_BACKEND_TEST_PAIR(
    HalfPipeDut,
    "stream_half_pipe：隔拍才捕获（半带宽）/ 下游停顿时数据保持 / 满则上游被挡住",
    "[chlib][stream][pipeline][half][ready][dual]",
    [&]() {
        auto &io = dev.instance().io();
        for (const auto &tc : kHALF) {
            sim.set_input_value(io.in_valid, tc.in_valid);
            sim.set_input_value(io.in_payload, tc.in_payload);
            sim.set_input_value(io.consumer_ready, tc.consumer_ready);
            sim.tick();
            INFO("in_valid=" << tc.in_valid << " in_payload=0x" << std::hex
                             << tc.in_payload << std::dec
                             << " consumer_ready=" << tc.consumer_ready);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.in_ready)) ==
                    tc.in_ready);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.out_valid)) ==
                    tc.out_valid);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.out_payload)) ==
                    tc.out_payload);
        }
    });
