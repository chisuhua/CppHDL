/**
 * UART TX Example - SpinalHDL Port using State Machine DSL
 * 
 * CppHDL 移植版本 - 使用状态机 DSL
 */

#include "chlib/state_machine.h"
#include "ch.hpp"
#include "component.h"
#include "module.h"
#include "simulator.h"
#include "codegen_verilog.h"
#include <iostream>
#include <vector>

using namespace ch;
using namespace ch::core;
using namespace chlib;

// UART TX 状态定义
enum class UartTxState : uint8_t {
    IDLE,     // 空闲状态
    START,    // 发送起始位
    DATA,     // 发送数据位
    STOP      // 发送停止位
};

// ============================================================================
// UART TX 模块（简化版 - 固定波特率）
// ============================================================================

class UartTx : public ch::Component {
public:
    // IO 端口定义
    __io(
        ch_in<ch_bool> frame_start;     // 帧开始信号
        ch_in<ch_uint<8>> data;         // 发送数据
        ch_out<ch_bool> uart_tx;        // UART 发送输出
        ch_out<ch_bool> busy;           // 忙标志
    )
    
    UartTx(ch::Component* parent = nullptr, 
           const std::string& name = "uart_tx")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        // ⚠️ **本模块的写法必须遵守 3 条承重约束**（详见
        //    openspec/changes/archive/2026-10-29-uart-tx-fsm-live-logic-2026-10/design.md）：
        //  ① `if (ch_bool)` / `if (ch_uint == lit)` / 三元 `? :` 在**建图期**求值
        //     ⇒ 恒走「假」分支（`ch_bool`/`ch_uint` 的 `operator uint64_t()` 只对
        //     **const 节点**返回真值 ⇒ `src/lnode/bool.cpp:44-49`）
        //     ⇒ 条件逻辑 MUST 用 `transition_when` / `select`（硬件原语）；
        //  ② `reg->next` 是 **last-wins**（`regimpl::set_next` = `set_src(0,…)`）
        //     ⇒ **每个寄存器 MUST 只有一个写入点**；
        //  ③ `ch_out` 的 `<<=` 也是覆盖 ⇒ **每个输出 MUST 只驱动一次**。

        // 波特率计数器 (16 位)：0..433 ⇒ 每位 434 拍
        ch_reg<ch_uint<16>> baud_counter(0_d);

        // 位计数器 (3 位，0-7)
        ch_reg<ch_uint<3>> bit_counter(0_d);

        // 移位寄存器 (9 位：bit0 = 起始位，bit1..bit8 = 8 个数据位 LSB-first)
        ch_reg<ch_uint<9>> shift_reg(511_d);  // 空闲状态为全 1

        // 创建状态机
        ch_state_machine<UartTxState, 4> sm;

        // ---- 输入端口 → 硬件值的**唯一**合法途径 --------------------------
        // ⚠️ `port<ch_bool, input_direction>` **没有** `ch_bool` 转换
        // （`include/core/io.h:232`：只有 `ch_uint` 有）⇒ ⇒ **不可**直接
        //    `transition_when(io().frame_start, …)`（实测：`ch_bool(port)` 与
        //    `ch_uint<1>(port)` 都**编译不过**）⇒ ⇒ MUST 经**寄存器**中转。
        //    代价：`frame_start` 脉冲被**延后 1 拍**看到（UART 不在意）。
        ch_reg<ch_bool> start_latch(ch_bool(false));
        start_latch->next = io().frame_start;
        ch_bool frame_start = start_latch;          // 当前值（上一拍采到的）

        // `port<ch_uint<8>>` → `ch_uint<8>` 有转换（`io.h:232`）⇒ 可直接取
        ch_uint<8> data_v = io().data;

        // ---- 硬件条件（**不是** C++ 条件 ⇒ 建图期只建节点，运行时求值）----
        // 波特率周期到（434 拍）
        ch_bool baud_done = (baud_counter == ch_uint<16>(433_d));
        // 最后一个数据位
        ch_bool bit_last = (bit_counter == ch_uint<3>(7_d));

        // 状态：IDLE —— 空闲，等待 frame_start
        sm.state(UartTxState::IDLE).on_active([&] {
            sm.transition_when(frame_start, UartTxState::START);
        });

        // 状态：START —— 发送起始位（0），等一个波特周期
        sm.state(UartTxState::START).on_active([&] {
            sm.transition_when(baud_done, UartTxState::DATA);
        });

        // 状态：DATA —— 发送 8 个数据位（LSB first）
        sm.state(UartTxState::DATA).on_active([&] {
            // ⚠️ **MUST 与 `baud_done` 合取**：`transition_when` 的条件是
            //    **电平敏感**的（每拍求值、为真即走）⇒ 若只用 `bit_last`
            //    ⇒ 它在 `bit_counter` 变 7 的那一刻就持续为真 ⇒ **第 8 个数据位
            //    只发 1 拍**（而非 434 拍）⇒ 停止位提前 433 拍。
            //    ⇒ 与 `baud_done` 合取 ⇒ 「满一个波特周期」才转移
            //    （与 START/STOP 的写法同构）。
            sm.transition_when(bit_last & baud_done, UartTxState::STOP);
        });

        // 状态：STOP —— 发送停止位（1），等一个波特周期后回 IDLE
        sm.state(UartTxState::STOP).on_active([&] {
            sm.transition_when(baud_done, UartTxState::IDLE);
        });

        // 设置入口状态并构建
        sm.set_entry(UartTxState::IDLE);
        sm.build();

        // ====================================================================
        // ⚠️ 以下是**唯一的**寄存器 next 写入点与输出驱动点（约束 ② / ③）
        //    —— MUST 写在 `sm.build()` **之后**（`build()` 内部会执行各状态的
        //    `on_active`；若在 `on_active` 里写 `reg->next` ⇒ 多状态覆盖 ⇒
        //    last-wins ⇒ 只有最后发射的那个状态生效）。
        // ====================================================================

        // 波特率计数器：IDLE 保持；START/DATA/STOP 每拍 +1，到期清零
        baud_counter->next = select(
            sm.is_in(UartTxState::START) | sm.is_in(UartTxState::DATA) |
                sm.is_in(UartTxState::STOP),
            select(baud_done, ch_uint<16>(0_d), baud_counter + ch_uint<16>(1_d)),
            baud_counter);

        // 位计数器：START 到期清零；DATA 到期时按「是否最后一位」决定清零 / +1
        bit_counter->next = select(
            sm.is_in(UartTxState::DATA) & baud_done,
            select(bit_last, ch_uint<3>(0_d), bit_counter + ch_uint<3>(1_d)),
            select(sm.is_in(UartTxState::START) & baud_done,
                   ch_uint<3>(0_d), bit_counter));

        // 移位寄存器：
        //   IDLE 且 frame_start ⇒ 载入 {data, 起始位 0}（bit0 = 起始位）
        //   DATA 且波特到期     ⇒ 右移一位（最低位补 1；⚠️ 该补位**不参与**
        //                          输出 —— 停止位来自输出 `select` 的常量分支）
        //   STOP 且波特到期     ⇒ 恢复空闲全 1
        shift_reg->next = select(
            sm.is_in(UartTxState::DATA) & baud_done,
            (shift_reg >> ch_uint<9>(1_d)) | ch_uint<9>(1_d),
            select(sm.is_in(UartTxState::STOP) & baud_done,
                   ch_uint<9>(511_d),
                   select(sm.is_in(UartTxState::IDLE) & frame_start,
                          (data_v << ch_uint<9>(1_d)) | ch_uint<9>(0_d),
                          shift_reg)));

        // 输出 uart_tx：**只驱动一次**（约束 ③）
        //   IDLE = 1（空闲高）/ START = 0（起始位）/ DATA = 数据位 / STOP = 1（停止位）
        //   ⚠️ 数据位取 `bit1`（= `(shift_reg >> 1) & 1`）⇒ 完整发出 bit1..bit8
        //      （原写法取 `bit0` = 起始位 ⇒ 少发 `data[7]` ⇒ off-by-one）
        io().uart_tx <<= select(
            sm.is_in(UartTxState::START),
            ch_bool(false),
            select(sm.is_in(UartTxState::DATA),
                   ((shift_reg >> ch_uint<9>(1_d)) & ch_uint<9>(1_d)) !=
                       ch_uint<9>(0_d),
                   ch_bool(true)));

        // 输出 busy：**只驱动一次** —— START/DATA 期间为 1，IDLE/STOP 为 0
        io().busy <<= select(
            sm.is_in(UartTxState::START) | sm.is_in(UartTxState::DATA),
            ch_bool(true), ch_bool(false));
    }
};

// ============================================================================
// 顶层模块（用于测试）
// ============================================================================

class UartTxTop : public ch::Component {
public:
    __io(
        ch_in<ch_bool> frame_start;
        ch_in<ch_uint<8>> data;
        ch_out<ch_bool> uart_tx;
        ch_out<ch_bool> busy;
    )
    
    UartTxTop(ch::Component* parent = nullptr, const std::string& name = "uart_tx_top")
        : ch::Component(parent, name) {}
    
    void create_ports() override {
        new (io_storage_) io_type;
    }
    
    void describe() override {
        ch::ch_module<UartTx> uart_tx_inst{"uart_tx_inst"};
        
        uart_tx_inst.io().frame_start <<= io().frame_start;
        uart_tx_inst.io().data <<= io().data;
        io().uart_tx <<= uart_tx_inst.io().uart_tx;
        io().busy <<= uart_tx_inst.io().busy;
    }
};

// ============================================================================
// 主函数 - 仿真测试
// ============================================================================

int main() {
    std::cout << "CppHDL vs SpinalHDL - UART TX Example (State Machine DSL)" << std::endl;
    std::cout << "========================================================" << std::endl;
    
    {
        ch_device<UartTxTop> device;
        Simulator simulator(device.context());
        
        std::cout << "\n=== UART TX Simulation ===" << std::endl;
        
        // 初始化
        // ⚠️ **必须**用 `set_input_value` 驱动：`frame_start` / `data` 是**顶层**
        // `ch_in`（`describe()` 把它们接线给子模块 ⇒ 顶层侧**无内部驱动**
        // ⇒ 该由 TB 驱动）⇒ 而 `io().X = value` 对 `ch_in` **无 `operator=`**
        // ⇒ 走 `CHERROR("Cannot assign value to input port!")` + **什么都不做**
        // ⇒ **端口保持未驱动**（实测：这条路径每次运行打 4 条 ERROR）。
        // ⚠️ 值侧 MUST 用**纯整数**：`ch_bool` 的 `operator uint64_t` 是
        //    **explicit** ⇒ `set_input_value(port, ch_bool(false))` 编译不过
        //    （与仓内既有写法一致：`tests/test_axi4_full_burst.cpp:187`）。
        simulator.set_input_value(device.instance().io().frame_start, 0);
        // ⚠️ **数据 MUST 用判别性值 `0xA5`**（`10100101` ⇒ `bit7 = 1`）：
        //    `0x55`（`01010101` ⇒ `bit7 = 0`）与「起始位 0」相同 ⇒ **抓不到**
        //    「少发 data[7]」的 off-by-one ⇒ ⇒ 用 `0x55` 的断言是**零判别力**的。
        constexpr uint64_t TEST_DATA = 0xA5;  // 165
        simulator.set_input_value(device.instance().io().data, TEST_DATA);

        // 空闲几个周期
        for (int i = 0; i < 5; i++) {
            simulator.tick();
        }

        // 启动发送
        std::cout << "Starting UART TX with data 0xA5 (165)..." << std::endl;
        simulator.set_input_value(device.instance().io().frame_start, 1);
        simulator.tick();
        simulator.set_input_value(device.instance().io().frame_start, 0);

        // 仿真足够周期以完成一帧（10 位 * 434 波特率周期 + 余量）
        constexpr unsigned FRAME_CYCLES = 4500;
        // 波特率分频：每位 434 拍（`baud_counter` 0..433 ⇒ 到期清零）
        constexpr int BAUD_DIV = 434;
        // ⚠️ **`busy` 高电平的总拍数**（**手推**）：`busy` 高 = START（起始位）
        //    + DATA（8 个数据位）= 9 位 × 434 = **3906** 拍
        constexpr long long EXPECT_BUSY_HIGH = 9LL * BAUD_DIV;

        std::cout << "Simulating " << FRAME_CYCLES << " cycles..." << std::endl;

        // ---- 自检：以 `busy` 上升沿为帧起点，按波特周期采样 `uart_tx` ----
        // ⚠️ 期望值**手推**（不得抄仿真输出）：
        //    帧 = 起始位(0) + 8 个数据位(LSB first) + 停止位(1)
        //    `0xA5 = 10100101b` ⇒ LSB-first = 1,0,1,0,0,1,0,1
        //    ⇒ 期望序列 = {0, 1,0,1,0,0,1,0,1, 1}
        const std::vector<uint64_t> expected = {
            0,                    // 起始位
            1, 0, 1, 0, 0, 1, 0, 1,  // 0xA5 的 8 位（LSB first）
            1                     // 停止位
        };

        std::vector<uint64_t> sampled;
        long long t0 = -1;   // 帧起点（busy 首次为 1 的那一拍）
        bool idle_restored = false;
        // ⚠️ **`busy` 高电平的总拍数**（**必需**：采样网格对「位段长度」零判别力 ——
        //    每位样本落在**位段末尾** ⇒ 位段被截短时采样值仍可能正确）
        //    **手推**：`busy` 高 = START（起始位 1 位）+ DATA（8 个数据位）
        //    = (1 + 8) × 434 = **3906** 拍。
        //    ⇒ 若「最后一个数据位只发 1 拍」（`transition_when` 条件漏 `baud_done`
        //    ⇒ 电平敏感、`bit_last` 一变真就转移）⇒ 实测会是 **3473** 拍
        //    ⇒ 本断言即可抓住。
        long long busy_high = 0;

        for (unsigned i = 0; i < FRAME_CYCLES; i++) {
            simulator.tick();

            auto uart_tx_val = simulator.get_value(device.instance().io().uart_tx);
            auto busy_val = simulator.get_value(device.instance().io().busy);
            const uint64_t tx = static_cast<uint64_t>(uart_tx_val);
            const uint64_t busy = static_cast<uint64_t>(busy_val);

            if (t0 < 0) {
                // 找 `busy` 上升沿 = 进 START 的那一拍 = 起始位的开始
                if (busy != 0) {
                    t0 = static_cast<long long>(i);
                    // ⚠️ 起始位就在**这一拍**（rel = 0）⇒ MUST 立即采样
                    //    （否则漏掉第 0 位 ⇒ 只采到 9 位）
                    sampled.push_back(tx);
                    busy_high = 1;   // 这一拍 `busy` 已为 1
                }
            } else {
                if (busy != 0) {
                    busy_high++;
                }
                const long long rel = static_cast<long long>(i) - t0;
                // 每 BAUD_DIV 拍采一次（相对帧起点）
                if (rel % BAUD_DIV == 0 &&
                    rel / BAUD_DIV < static_cast<long long>(expected.size())) {
                    sampled.push_back(tx);
                }
                // 帧结束（10 位之后）MUST 回到 IDLE：busy = 0 且 uart_tx = 1
                if (rel == static_cast<long long>(expected.size()) * BAUD_DIV) {
                    idle_restored = (busy == 0) && (tx == 1);
                }
            }

            // 打印关键信号
            if (i % 500 == 0 || i < 20) {
                std::cout << "  Cycle " << i << ": uart_tx=" << tx
                          << ", busy=" << busy << std::endl;
            }
        }

        // ---- 自检判定（不匹配 ⇒ 退出码非 0 ⇒ 运行护栏能抓到）----
        bool ok = true;
        if (t0 < 0) {
            std::cout << "\n[FAIL] 未观测到 `busy` 上升沿 ⇒ 状态机从未启动"
                      << std::endl;
            ok = false;
        } else if (sampled.size() != expected.size()) {
            std::cout << "\n[FAIL] 采样位数 " << sampled.size() << " ≠ 期望 "
                      << expected.size() << std::endl;
            ok = false;
        } else {
            for (size_t k = 0; k < expected.size(); k++) {
                if (sampled[k] != expected[k]) {
                    std::cout << "\n[FAIL] 第 " << k << " 位：期望 uart_tx="
                              << expected[k] << "，实测 " << sampled[k]
                              << std::endl;
                    ok = false;
                }
            }
        }
        if (!idle_restored) {
            std::cout << "\n[FAIL] 帧结束后未回到 IDLE（busy=0 且 uart_tx=1）"
                      << std::endl;
            ok = false;
        }
        // ⚠️ **`busy` 高电平总拍数**（手推 = (1 起始位 + 8 数据位) × 434 = 3906）
        //    ⇒ 这条断言对「位段长度」有判别力（采样网格**没有**：每位样本落在
        //    位段末尾 ⇒ 位段被截短时采样值仍可能正确）
        if (busy_high != EXPECT_BUSY_HIGH) {
            std::cout << "\n[FAIL] `busy` 高电平拍数 " << busy_high
                      << " ≠ 期望 " << EXPECT_BUSY_HIGH
                      << "（= (1 起始位 + 8 数据位) × " << BAUD_DIV
                      << "）⇒ 某位的长度不对" << std::endl;
            ok = false;
        }
        if (ok) {
            std::cout << "\n[PASS] 帧序列 = 起始位(0) + 0xA5 的 8 位 LSB-first"
                      << " + 停止位(1)，每 " << BAUD_DIV << " 拍一位；"
                      << "`busy` 高电平 " << busy_high << " 拍；帧后已回 IDLE"
                      << std::endl;
        }

        std::cout << "\n=== Simulation Complete ===" << std::endl;

        // 生成 Verilog
        std::cout << "\n=== Generating Verilog ===" << std::endl;
        toVerilog("uart_tx.v", device.context());
        std::cout << "Verilog generated: uart_tx.v" << std::endl;

        // ⚠️ **自检失败 MUST 返回非 0** —— 运行护栏（`run_all_ported_tests.sh`）
        //    只查**退出码** ⇒ 返回 0 会让「行为退化」的 example 被判 PASS
        //    （这正是本 example 长期「假绿」的机制）。
        if (!ok) {
            std::cout << "\nUART TX self-check FAILED" << std::endl;
            return 1;
        }
    }

    std::cout << "\nProgram completed successfully" << std::endl;

    return 0;
}
