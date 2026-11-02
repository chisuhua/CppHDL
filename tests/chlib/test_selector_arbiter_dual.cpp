// ⚠️⚠️ **路线 B（H 批）—— `chlib::priority_selector` / `round_robin_selector` 进双后端 parity**
// （2026-11-03）
//
// ## 为什么选这两个（而不是同文件的 `round_robin_arbiter`）
//
// `include/chlib/selector_arbiter.h` 里三个组合子，本批**只取两个干净的**：
//
// | 组合子 | 状态 | 理由 |
// |---|---|---|
// | `priority_selector` | ✅ 本批 | **纯组合**（无 `ch_reg`、无 C++ `if`）⇒ 语义确定 |
// | `round_robin_selector` | ✅ 本批 | 同上；**且有生产消费者**（`include/chlib/stream.h` 的 `stream_arbiter_round_robin` 调它） |
// | `round_robin_arbiter` | 🔴 **不做** | `UpdatePriority` 的 `if (result.valid)` **建图期求值** ⇒ 恒假 ⇒ **`next_ptr` 恒 0 ⇒ 不轮转**（实测）⇒ **带真缺陷** ⇒ ⇒ dual parity 会给缺陷**盖章** ⇒ ⇒ **先修缺陷再覆盖**（独立立项 `.rddf/improvements/selector-round-robin-not-rotating-2026-11.md`） |
//
// ## 4 问
//
// ① **重复**：`priority_selector` / `round_robin_selector` 在全仓 54 个 dual 文件里
//    **各出现 0 次** ⇒ 无重复。
// ② **判别力**：见下（6 条变异）⇒ **关键**：优先级方向与轮转起点**只有多位置位时
//    才可观测** ⇒ ⇒ 激励 MUST 含**多位同时置位**的向量（既有单后端测试也有这类向量）。
// ③ **与后端无关**：**否** —— 真跑仿真 + 读顶层端口，正是要对比两后端。
// ④ **值得新写**：既有 `tests/chlib/test_selector_priority.cpp`（4 用例）与
//    `tests/chlib/test_selector_round_robin.cpp`（2 用例）**全是裸 context**
//    （`ch_device=0`、无顶层端口）⇒ ⇒ 命中 **R6 排除项** ⇒ ⇒ **不可转换**
//    ⇒ ⇒ 必须新写（自带 wrapper DUT + 手推期望）。
//    ⚠️ 两个原文件 **MUST NOT 修改**。
//
// ## DUT 形态（wrapper）
//
// 两个组合子都是**函数式**（返回 result struct），**不是** `Component`
// ⇒ ⇒ 需要一层「端口 ↔ 结果字段」的适配 wrapper。
// ⚠️ **不是「DUT 复制」**：wrapper 只做适配，**被测的 `select` 链全部在库组件里**
// ⇒ ⇒ 测的就是生产代码本身。
//
// ## 期望值（**手推**，禁抄仿真输出）
//
// 模型（`/tmp/opencode/gen_selector_table.py`，独立 Python 实现）：
//
//     priority_selector<N>(request):
//         grant = 0 ; valid = false
//         for i in 0..N-1:                       ← 低位 → 高位
//             grant = (bit_i(request) and not valid) ? (1 << i) : grant
//             valid = bit_i(request) or valid
//         ⇒ grant = **最低**置位胜；valid = (request != 0)
//
//     round_robin_selector<N>(request, last_grant):   ← last_grant 是 one-hot
//         has_last  = last_grant != 0
//         last_idx  = onehot_to_binary(last_grant) if has_last else 0
//         start_pos = (last_idx + 1) % N
//         grant = 0 ; valid = false
//         for i in 0..N-1:
//             pos = (start_pos + i) % N          ← **环绕**扫描
//             grant = (bit_pos(request) and not valid) ? (1 << pos) : grant
//             valid = bit_pos(request) or valid
//
// ⚠️ **位宽自洽性**：`compute_idx_width(4)` = `bit_width(3)` = **2**
// ⇒ ⇒ 「2 位加减」在数值上**恒等于 mod 4** ⇒ ⇒ `(start_pos + i) % 4` 的环绕语义
// **无歧义**（无论加法结果被扩到 2 位还是 3 位，mod 4 的结果相同）。
//
// ### ⚠️ 判别力要害：**单位置位时两种语义给出同一个 grant**
//
// `priority_selector` 的「低位优先」**只有多位同时置位时才可观测**
// ⇒ ⇒ 激励 MUST 含 `0b1010`（⇒ grant `0b0010`）、`0b1111`（⇒ grant `0b0001`）。
// `round_robin_selector` 的「起点 = 上次授予位置 + 1」同理：
// `request=0b1111` + `last_grant=0b0001` ⇒ 起点 1 ⇒ grant `0b0010`
// ⇒ ⇒ 若把起点错写成恒 0 ⇒ grant 会是 `0b0001` ⇒ 转红。
//
// ### 激励
//
// `priority`（7 组）：0 / 单点位0 / 位1 / 位3 / **位1+位3** / **四位全置** / 位1+位2
// `round_robin`（10 组）：无请求 · 起点 0 的三种单点 · **起点 1** · **起点 2** ·
//                        **环绕（last=pos3 ⇒ 起点 0）** · **起点 1 + 全请求** ·
//                        **环绕扫描（start=2 ⇒ 扫 2,3,0,1）** · **再环绕（start=3）**
//
// ⚠️ **陷阱 T3**：body 参数里**花括号内的逗号**会被预处理器当参数分隔
//    ⇒ ⇒ 拍表放**文件作用域**。
// ⚠️ **判据（N2′）**：断言数(with verilator) == **2 ×** 断言数(interpreter-only)。
#include "catch_amalgamated.hpp"
#include "dual_backend_fixture.h"

#include "ch.hpp"
#include "chlib/selector_arbiter.h"
#include "component.h"
#include "core/context.h"
#include "simulator.h"

#include <cstdint>

using namespace ch;
using namespace ch::core;
using namespace chlib;

namespace {

constexpr unsigned kN = 4;

// ── `priority_selector<4>` 的向量（request, grant, valid）──
struct PriorityCase {
    uint64_t request, grant, valid;
};
const PriorityCase kPriority[] = {
    {0b0000, 0b0000, 0},  // 无请求 ⇒ 无授予、无效
    {0b0001, 0b0001, 1},  // 单点位 0
    {0b0010, 0b0010, 1},  // 单点位 1
    {0b1000, 0b1000, 1},  // 单点位 3
    {0b1010, 0b0010, 1},  // ⚠️ 位1+位3 ⇒ **最低**置位（bit1）胜 ⇒ 判别「低位优先」
    {0b1111, 0b0001, 1},  // ⚠️ 四位全置 ⇒ bit0 胜
    {0b0110, 0b0010, 1},  // 位1+位2 ⇒ bit1 胜
};

// ── `round_robin_selector<4>` 的向量（request, last_grant, grant, valid）──
struct RoundRobinCase {
    uint64_t request, last_grant, grant, valid;
};
const RoundRobinCase kRoundRobin[] = {
    {0b0000, 0b0000, 0b0000, 0},  // 无请求
    {0b0001, 0b0000, 0b0001, 1},  // 无上次 ⇒ 起点 0 ⇒ 位 0
    {0b0010, 0b0000, 0b0010, 1},  // 无上次 ⇒ 起点 0 ⇒ 扫到位 1
    {0b0100, 0b0000, 0b0100, 1},  // 无上次 ⇒ 起点 0 ⇒ 扫到位 2
    {0b0010, 0b0001, 0b0010, 1},  // last=pos0 ⇒ 起点 1
    {0b0100, 0b0010, 0b0100, 1},  // last=pos1 ⇒ 起点 2
    {0b0001, 0b1000, 0b0001, 1},  // ⚠️ last=pos3 ⇒ 起点 (3+1)%4 = 0（**环绕**）
    {0b1111, 0b0001, 0b0010, 1},  // ⚠️ last=pos0 ⇒ 起点 1 ⇒ 首个置位是 pos1
    {0b1001, 0b0010, 0b1000, 1},  // ⚠️ last=pos1 ⇒ 起点 2 ⇒ 扫 2,3,0,1 ⇒ pos3
    {0b0110, 0b0100, 0b0010, 1},  // ⚠️ last=pos2 ⇒ 起点 3 ⇒ 扫 3,0,1,2 ⇒ pos1
};

// ---------------------------------------------------------------------------
// DUT Ⓐ：`priority_selector<4>`
// ---------------------------------------------------------------------------
class PrioritySelectorDut : public ch::Component {
public:
    __io(ch_in<ch_uint<kN>> request;
         ch_out<ch_uint<kN>> grant;
         ch_out<ch_bool> valid;)

    PrioritySelectorDut(ch::Component *parent = nullptr,
                        const std::string &name = "priority_selector_dut")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_uint<kN> req(io().request.impl());
        auto r = priority_selector<kN>(req);
        io().grant = r.grant;
        io().valid = r.valid;
    }
};

// ---------------------------------------------------------------------------
// DUT Ⓑ：`round_robin_selector<4>`
// ---------------------------------------------------------------------------
class RoundRobinSelectorDut : public ch::Component {
public:
    __io(ch_in<ch_uint<kN>> request;
         ch_in<ch_uint<kN>> last_grant;
         ch_out<ch_uint<kN>> grant;
         ch_out<ch_bool> valid;)

    RoundRobinSelectorDut(ch::Component *parent = nullptr,
                          const std::string &name = "round_robin_selector_dut")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        ch_uint<kN> req(io().request.impl());
        ch_uint<kN> last(io().last_grant.impl());
        auto r = round_robin_selector<kN>(req, last);
        io().grant = r.grant;
        io().valid = r.valid;
    }
};

}  // namespace

// ===========================================================================
// pair 1：`priority_selector` —— **低位优先**（多位同时置位时才可观测）
// ===========================================================================
DUAL_BACKEND_TEST_PAIR(
    PrioritySelectorDut,
    "priority_selector：最低置位优先（多位同时置位时）/ 无请求时无授予",
    "[chlib][selector][priority][grant][dual]",
    [&]() {
        auto &io = dev.instance().io();
        for (const auto &tc : kPriority) {
            sim.set_input_value(io.request, tc.request);
            sim.tick();
            INFO("request=0b" << std::hex << tc.request);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.grant)) ==
                    tc.grant);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.valid)) ==
                    tc.valid);
        }
    });

// ===========================================================================
// pair 2：`round_robin_selector` —— 起点 = 上次授予位置 + 1（环绕扫描）
// ===========================================================================
DUAL_BACKEND_TEST_PAIR(
    RoundRobinSelectorDut,
    "round_robin_selector：起点 = 上次授予 + 1（环绕扫描）/ 全请求时取起点后的首个置位",
    "[chlib][selector][round_robin][grant][dual]",
    [&]() {
        auto &io = dev.instance().io();
        for (const auto &tc : kRoundRobin) {
            sim.set_input_value(io.request, tc.request);
            sim.set_input_value(io.last_grant, tc.last_grant);
            sim.tick();
            INFO("request=0b" << std::hex << tc.request
                              << " last_grant=0b" << tc.last_grant);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.grant)) ==
                    tc.grant);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.valid)) ==
                    tc.valid);
        }
    });
