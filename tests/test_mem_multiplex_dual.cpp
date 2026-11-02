// ⚠️⚠️ **路线 A（D 批）第 3 个文件 —— `ch_mem` 的两个**新 codegen 形态**进双后端 parity**
// （2026-11-02）
//
// ## 为什么是这个文件（D 批剩 1 的「可转性 4 问」结论）
//
// `tests/test_mem_timing.cpp` 的 7 个用例逐条过 4 问后，**只有 2 个形态**带来新覆盖：
//
// | 原用例 | 4 问结论 |
// |---|---|
// | `:535` sread clears its value while the enable is low | ❌ **重复** —— 禁读拍清零语义**已被** `tests/test_mem_sread_disable_dual.cpp` 覆盖 |
// | `:389` / `:435` 写后读 / 顺序写读 | ❌ **重复** —— **已被** `tests/test_mem_comb_read_dual.cpp`（跨拍写后读 + 同拍写后读 + `we=0` 不写）覆盖 |
// | `:306` Initialized Memory（aread + init） | ❌ **重复** —— aread → `ch_out` 的数据通路**已被** `test_mem_connection_dual.cpp` 的 pair 2 覆盖 |
// | `:182` Single Port | ⚠️ **部分重复 + 一处新形态** —— 它的 **`select()` 里嵌读端口**（`io().data_out = select(io().we_in, io().data_in, read_port);`）是**任何既有 dual 文件都没有的 codegen 路径** ⇒ ⇒ 本文件 pin 它 |
// | `:358` Multi-Read Port（**同一 mem 上两个读端口**） | ✅ **新形态** —— 既有 dual 文件里每个 DUT **只有一个**读端口 ⇒ ⇒ 两个 proxy 同时驱动同一 mem 的 codegen 路径从未被验证 ⇒ ⇒ 本文件 pin 它 |
// | `:231` Dual Port | ⚠️ 读写地址分离 —— 与上面的「两个读端口」部分重叠，本文件不重复做 |
//
// ## 🔴 T2：**本文件的两个 DUT 对「0-cycle vs 1-cycle」零判别力**（如实登记）
//
// 地址来自 **TB 驱动的输入端口** ⇒ `tick()` 前就已写入 `data_map_` ⇒ 沿上采样到的
// 已是新地址 ⇒ 「0-cycle 读新地址」与「1-cycle 沿上采样新地址」**观测值相同**
// ⇒ ⇒ **本文件对「0/1-cycle 采样语义」不判别**（时序由
// `test_mem_read_timing_dual.cpp` 的**寄存器驱动**形态负责 ⇒ ⇒ **不重复**）。
// ⚠️ **但 N4 变异实测把上面这句的原始表述（「零判别力」）证伪了** —— 见下。
//
// ### 🔴 N4：`aread` 被发射成 `always_ff` ⇒ **转红**（结论与预测相反，已查清机制）
//
// 变异：`src/codegen_verilog.cpp` 的 `if (port_type() == async_read)` → `if (false)`
// （即**所有**读端口都走 `always_ff` 分支）。
// **实测**：pair 2 三个向量**全部**读到 **0**（期望 0xC3 / 0xF6 / 0xA1）。
//
// **机制（三步实证，不是推断）**：
//   ① 把该变异下生成的 `top.v` 用**独立简易 harness** 驱动（同 rst 脉冲 + 同
//      clk 0→1→0 序列）⇒ **读到正确值 0xc3 / 0x90 / 0xa1 / 0x3a** ⇒
//      ⇒ **生成的 Verilog 语义本身没错**（含 dangling-else：
//      `if (!rst) if (1'b1) proxy <= mem[a]; else proxy <= 0;` 的 `else` 绑**内层** `if`）；
//   ② 所以错在后端驱动路径。`Simulator::tick()` 里 `eval()`（进而
//      `eval_sequential()` → Vtop 时钟 0→1→0）**只在 `default_clock_instr_` 存在时**才调用；
//   ③ 而 `src/simulator_init.cpp:153` 有一行：
//      `if (sequential_instr_list_.empty()) default_clock_instr_ = nullptr;`
//      ⇒ ⇒ **pair 2 的 DUT 没有任何时序节点**（只有 `aread` + `ch_out`）
//      ⇒ ⇒ 时序表为空 ⇒ 时钟**从不翻转** ⇒ `always_ff` **永不触发** ⇒ 读出 0。
//   ⇒ 这也解释了为什么 pair 1（用 `sread` ⇒ 时序表非空）在同一个变异下**完好**。
//
// ✅ **因此 N4 是一个真判别力的变异**（它抓到的是「codegen 发射 `always_ff`，而
// Simulator 把该构造归类为组合」这一类**静默 0**）⇒ ⇒ 本文件**保留**对它的回归能力。
// 🔴 **记为框架层不变量（待独立立项）**：
//    「Verilog codegen 为该设计发射了任一 `always_ff`」 ⟺
//    「Simulator 的 `sequential_instr_list_` 非空（⇒ 时钟会 toggle）」
//    两者一旦漂移 ⇒ 生成的时序逻辑**静默不工作**（读 0，无任何报错）。
//    ⇒ 当前是否有其它构造也漂移**未审计** ⇒ 独立立项，不在本文件范围。
//
// ⇒ ⇒ 本文件的判别面是另外三条：
//   ① **mux 的操作数是读端口的 data proxy**（不是那个**虚构**的 read_port 节点）
//      ⇒ 若 codegen 引用了 read_port ⇒ 生成**未声明信号** ⇒ Verilator 编译失败
//      ⇒ 且 interpreter 侧读的是 proxy ⇒ **两后端分叉**；
//   ② **写使能被尊重**（`we=0` 时不得写）；
//   ③ **同一 mem 上的两个读端口**各自独立解码地址（不串线）。
//
// ## 期望值（**手推**，禁抄仿真输出）
//
// 初始化（16 × 8bit，两两不同）：[0]=0xA1 [1]=0xB2 [2]=0xC3 [3]=0xD4 [4]=0xE5
// [5]=0xF6 [6]=0x07 [7]=0x18 [8]=0x29 [9]=0x3A [10]=0x4B [11]=0x5C [12]=0x6D
// [13]=0x7E [14]=0x8F [15]=0x90
//
// ### pair 1（mux 嵌读端口 + 写使能）
//   语义：`data_out = we_in ? data_in : <读端口的 data proxy>`
//   （`we_in=1` ⇒ 走**写数据**；`we_in=0` ⇒ 走**读到的 mem 内容**）
//   step1: we=0, addr=3, data=0x00        ⇒ 0xD4（mem[3] 的初值）
//   step2: we=1, addr=3, data=0x5A        ⇒ 0x5A（mux 走**写数据**分支）
//   step3: we=0, addr=3, data=0x00        ⇒ **0x5A**（step2 的写已落地 ⇒ 读到 0x5A）
//   step4: we=0, addr=3, data=**0x77**    ⇒ **0x5A**（we=0 ⇒ 写被忽略 ⇒ 值不变）
//          ⚠️ 这一步同时判别「写使能被忽略」与「mux 分支接反」两种失败模式。
//   step5: we=0, addr=5                   ⇒ 0xF6（mem[5] 的初值）
//   step6: we=1, addr=5, data=0xC3        ⇒ 0xC3（mux 走写数据分支）
//
// ### pair 2（同一 mem 上两个读端口）
//   语义：`data1_out = mem[addr1_in]`、`data2_out = mem[addr2_in]`（两个 aread，0-cycle）
//   ⚠️ 两个地址**刻意互异** ⇒ 任何 proxy 接反 / 串线都立刻可见。
//
// ⚠️ **向量表放在文件作用域**（陷阱 T3）：`DUAL_BACKEND_TEST_PAIR` 的 body 参数里
//    **花括号内的逗号**会被预处理器当成**参数分隔**。
//
// ⚠️ **MUST 驱动全部输入端口**（仓内 ANTI-PATTERNS 已登记「未驱动端口读到的是未定义值」）。
//
// ⚠️ **判据（N2′）**：断言数(with verilator) == **2 ×** 断言数(interpreter-only)。
#include "catch_amalgamated.hpp"

#include "component.h"
#include "core/bool.h"
#include "core/context.h"
#include "core/mem.h"
#include "core/uint.h"
#include "device.h"
#include "dual_backend_fixture.h"
#include "simulator.h"

#include <cstdint>
#include <vector>

using namespace ch;
using namespace ch::core;

namespace {

// ── DUT 1：`select()` 的操作数里嵌**读端口**（mux-over-read-port）──
class MuxOverReadPortDut : public ch::Component {
public:
    __io(ch_in<ch_uint<4>> addr_in; ch_in<ch_uint<8>> data_in;
         ch_in<ch_bool> we_in; ch_out<ch_uint<8>> data_out;)

    MuxOverReadPortDut(ch::Component *parent = nullptr,
                       const std::string &name = "mux_over_read_port")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        const std::vector<uint64_t> init = {0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6,
                                            0x07, 0x18, 0x29, 0x3A, 0x4B, 0x5C,
                                            0x6D, 0x7E, 0x8F, 0x90};
        ch_mem<ch_uint<8>, 16> mem(init, "mux_read_mem");

        // sread 端口（1-cycle）；⚠️ **它的 data proxy 被用作 mux 的一个操作数**
        auto read_port =
            mem.sread(ch_uint<4>(io().addr_in.impl()), ch_bool(true),
                      "sread_port");

        // ⚠️ **关键形态**：mux 的 false 分支操作数 = 读端口
        //    （`read_port::impl()` 返回的是 **data proxy**，不是那个虚构的
        //    read_port 节点 ⇒ ⇒ codegen 必须引用 proxy 那个**已声明**的信号）
        io().data_out <<= select(io().we_in, io().data_in, read_port);

        // 写口（地址与读口共用；`we_in` 为写使能）
        mem.write(ch_uint<4>(io().addr_in.impl()),
                  ch_uint<8>(io().data_in.impl()), ch_bool(io().we_in.impl()));
    }
};

// ── DUT 2：**同一 mem 上两个读端口** ──
class MultiReadPortDut : public ch::Component {
public:
    __io(ch_in<ch_uint<4>> addr1_in; ch_in<ch_uint<4>> addr2_in;
         ch_out<ch_uint<8>> data1_out; ch_out<ch_uint<8>> data2_out;)

    MultiReadPortDut(ch::Component *parent = nullptr,
                     const std::string &name = "multi_read_port")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        const std::vector<uint64_t> init = {0xA1, 0xB2, 0xC3, 0xD4, 0xE5, 0xF6,
                                            0x07, 0x18, 0x29, 0x3A, 0x4B, 0x5C,
                                            0x6D, 0x7E, 0x8F, 0x90};
        ch_mem<ch_uint<8>, 16> mem(init, "multi_read_mem");

        // 两个 aread 端口（0-cycle），各自独立解码地址
        io().data1_out <<= mem.aread(ch_uint<4>(io().addr1_in.impl()), "read1");
        io().data2_out <<= mem.aread(ch_uint<4>(io().addr2_in.impl()), "read2");
    }
};

// pair 1 的步骤表（手推，见文件头「期望值」段）
struct MuxStep {
    uint64_t addr;
    uint64_t data;
    uint64_t we;
    uint64_t expect;
};
const MuxStep kMuxSteps[] = {
    {0x3, 0x00, 0, 0xD4}, // we=0 ⇒ 读 mem[3] 初值
    {0x3, 0x5A, 1, 0x5A}, // we=1 ⇒ mux 走写数据分支
    {0x3, 0x00, 0, 0x5A}, // 上一拍的写已落地 ⇒ 读到 0x5A
    {0x3, 0x77, 0, 0x5A}, // ⚠️ we=0 ⇒ 写被忽略 ⇒ 值不变
    {0x5, 0x00, 0, 0xF6}, // 换地址
    {0x5, 0xC3, 1, 0xC3}, // we=1 ⇒ mux 走写数据分支
};

// pair 2 的期望表（两个地址互异 ⇒ 抓 proxy 接反 / 串线）
struct MultiReadExpect {
    uint64_t addr1;
    uint64_t addr2;
    uint64_t expect1;
    uint64_t expect2;
};
const MultiReadExpect kMultiRead[] = {
    {0x2, 0xF, 0xC3, 0x90},
    {0x5, 0x3, 0xF6, 0xD4},
    {0x0, 0x0, 0xA1, 0xA1},
    {0x9, 0x6, 0x3A, 0x07},
};

}  // namespace

// ───────────── pair 1：mux 的操作数里嵌读端口 + 写使能 ─────────────
DUAL_BACKEND_TEST_PAIR(
    MuxOverReadPortDut,
    "ch_mem 读端口作为 mux 操作数：mux 接线 + 写使能被尊重（两后端一致）",
    "[mem][multiplex][read-port][dual]",
    [&]() {
        auto &io = dev.instance().io();
        for (const auto &tc : kMuxSteps) {
            sim.set_input_value(io.addr_in, tc.addr);
            sim.set_input_value(io.data_in, tc.data);
            sim.set_input_value(io.we_in, tc.we);
            sim.tick();
            INFO("addr=" << tc.addr << " data=0x" << std::hex << tc.data
                         << " we=" << tc.we);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.data_out)) ==
                    tc.expect);
        }
    });

// ───────────── pair 2：同一 mem 上两个读端口 ─────────────
DUAL_BACKEND_TEST_PAIR(
    MultiReadPortDut,
    "ch_mem 同一 mem 上两个读端口：各自独立解码地址（两后端一致）",
    "[mem][multi-read-port][dual]",
    [&]() {
        auto &io = dev.instance().io();
        for (const auto &tc : kMultiRead) {
            sim.set_input_value(io.addr1_in, tc.addr1);
            sim.set_input_value(io.addr2_in, tc.addr2);
            sim.tick();
            INFO("addr1=" << tc.addr1 << " addr2=" << tc.addr2);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.data1_out)) ==
                    tc.expect1);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.data2_out)) ==
                    tc.expect2);
        }
    });
