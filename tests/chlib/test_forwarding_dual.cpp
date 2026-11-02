// ⚠️⚠️ **路线 B（新文件）—— `chlib::ForwardingMux` 的组件级语义进双后端 parity**
// （2026-11-02）
//
// ## 立项理由
//
// ① **组件级 dual 覆盖 = 0**：实测「含 `DUAL_BACKEND_TEST_PAIR(` 调用」的源文件
//    全集里，`chlib/forwarding.h` 只在 `tests/test_rv32i_pipeline_behavior.cpp`
//    与 `examples/riscv-mini/tests/test_rv32i_soc_dual_parity.cpp` 出现
//    ⇒ ⇒ 那两处覆盖的是 **SoC 级组合**（`riscv::HazardUnit` + `ForwardingMux` 的
//    组合 DUT），**不是 `ForwardingMux` 组件本身**。
// ② **既有单后端测试基本不可转换**：`tests/chlib/test_forwarding.cpp` 的 8 个用例
//    中有 **6 个**用的是**函数式**接口（`chlib::forward_detect<>` /
//    `chlib::forward_select<>` = 裸 context、无顶层端口）⇒ ⇒ 命中 R6 排除项
//    "bare-context tests" ⇒ 结构上不可转换；只有 `:213`（`ForwardingMux` 组件）
//    带顶层端口。
// ③ **本文件直接用库组件当 DUT（零 DUT 复制）** ⇒ 测的就是**生产代码本身**，
//    不是复刻品（仓内先例：`tests/test_blt_debug_behavior.cpp` 直接用
//    `riscv::BranchConditionUnit`）。
//
// ## 为什么 ForwardingMux 值得组件级双后端钉
//
// 它在 RV32I 的关键路径上，且档位 1 有一层**组件内部**的跳转判定
// （ADR-047 D4）：`is_jump` ⇒ 用 `jump_result`（返回地址 `pc+4`）而不是
// `ex_mem_alu_result`。⚠️ 这层判定**故意放在组件内部**（头注释明写）：
// 放在流水线装配里内联 ⇒ **测试台无法驱动** ⇒ 只能靠"实施者自己重建一份等价
// 逻辑"来测 ⇒ 测的是**副本** ⇒ 对**真实接线零信号** ⇒ **假绿**。
// ⇒ ⇒ **组件级**判据是唯一能钉住它的形态。
//
// ## 期望值（**手推**，禁抄仿真输出）
//
// 语义（逐字对照 `include/chlib/forwarding.h::ForwardingMux::describe()`）：
//
//     ex_mem_selected = is_jump ? jump_result : ex_mem_alu_result
//     alu_input_a    = forward_a == REG(0) ? rs1_data_reg
//                     : forward_a == EX(1)  ? ex_mem_selected
//                                             : mem_wb_result
//     alu_input_b    = 同上（用 rs2_data_reg / forward_b）
//
// ⚠️ `forward_a == 3` **未定义**（档位只有 0/1/2）⇒ ⇒ 本文件**不**断言它
//    （与 onehot 文件同一处置原则：不把未定义行为冻结成断言）。
//
// ⚠️ **向量表放在文件作用域**（陷阱 T3）：`DUAL_BACKEND_TEST_PAIR` 的 body 参数里
//    **花括号内的逗号**会被预处理器当成**参数分隔**。
//
// ⚠️ **MUST 驱动全部 7 个输入端口**：仓内 ANTI-PATTERNS 已登记「未驱动端口读到的是
//    **未定义值**」，双后端下两侧的未定义值未必相同 ⇒ ⇒ 依赖默认值 = 碰巧通过。
//
// ⚠️ **判据（N2′）**：断言数(with verilator) == **2 ×** 断言数(interpreter-only)。
#include "catch_amalgamated.hpp"

#include "chlib/forwarding.h"
#include "component.h"
#include "core/bool.h"
#include "core/context.h"
#include "core/uint.h"
#include "device.h"
#include "dual_backend_fixture.h"
#include "simulator.h"

#include <cstdint>

using namespace ch;
using namespace ch::core;

namespace {

// 5 个数据源取**互不相同**的值 ⇒ 任何串源/漏接都立刻可见。
constexpr uint64_t kRs1DataReg = 0x11111111ULL;
constexpr uint64_t kRs2DataReg = 0x22222222ULL;
constexpr uint64_t kExMemAlu = 0x33333333ULL;
constexpr uint64_t kJumpResult = 0x44444444ULL;
constexpr uint64_t kMemWbResult = 0x55555555ULL;

struct FwdCase {
    uint64_t forward_a;
    uint64_t forward_b;
    uint64_t is_jump;
    uint64_t expect_a;
    uint64_t expect_b;
};
const FwdCase kCases[] = {
    // 档位 0/0：两路都取寄存器堆
    {0, 0, 0, kRs1DataReg, kRs2DataReg},
    // 档位 1/2：a 取 EX/MEM（**非跳转** ⇒ ALU 结果），b 取 MEM/WB
    {1, 2, 0, kExMemAlu, kMemWbResult},
    // 档位 1/1 + **is_jump=1** ⇒ 两路都取**跳转返回地址**（ADR-047 D4 的核心）
    {1, 1, 1, kJumpResult, kJumpResult},
    // 档位 2/0：a 取 MEM/WB，b 取寄存器堆
    {2, 0, 0, kMemWbResult, kRs2DataReg},
    // 档位 0/2 + is_jump=1（is_jump 只影响档位 1 ⇒ 档位 0 不受影响）
    {0, 2, 1, kRs1DataReg, kMemWbResult},
};

}  // namespace

// ───────────── pair 1：`chlib::ForwardingMux<32>` 三档位 + 跳转返回地址 ─────────────
DUAL_BACKEND_TEST_PAIR(
    chlib::ForwardingMux<32>,
    "chlib ForwardingMux：三档位数据源选择 + is_jump 取返回地址（两后端一致）",
    "[chlib][forwarding][mux][dual]",
    [&]() {
        auto &io = dev.instance().io();
        for (const auto &tc : kCases) {
            // 全部 7 个输入端口**逐个显式驱动**
            sim.set_input_value(io.forward_a, tc.forward_a);
            sim.set_input_value(io.forward_b, tc.forward_b);
            sim.set_input_value(io.rs1_data_reg, kRs1DataReg);
            sim.set_input_value(io.rs2_data_reg, kRs2DataReg);
            sim.set_input_value(io.ex_mem_alu_result, kExMemAlu);
            sim.set_input_value(io.jump_result, kJumpResult);
            sim.set_input_value(io.mem_wb_result, kMemWbResult);
            sim.set_input_value(io.is_jump, tc.is_jump);
            sim.tick();
            INFO("forward_a=" << tc.forward_a << " forward_b=" << tc.forward_b
                              << " is_jump=" << tc.is_jump);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.alu_input_a)) ==
                    tc.expect_a);
            REQUIRE(static_cast<uint64_t>(sim.get_port_value(io.alu_input_b)) ==
                    tc.expect_b);
        }
    });
