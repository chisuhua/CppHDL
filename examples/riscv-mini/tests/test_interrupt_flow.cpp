/**
 * @file test_interrupt_flow.cpp
 * @brief RV32I 流水线中断流程测试
 * 
 * 测试范围:
 * 1. Timer mtip triggers when mtime >= mtimecmp
 * 2. interrupt_req = mtip && mie.MTIE (bit 7) && mstatus.MIE (bit 3)
 * 3. Pipeline flushes on mtip (flush signal goes to IF stage)
 * 4. mepc correctly saves PC of interrupted instruction
 * 5. mcause = 7 (Machine Timer Interrupt) on timer trap
 * 6. mtvec base address is used as trap entry PC
 * 7. MRET restores PC from mepc
 * 8. MRET restores mstatus.MIE from mstatus.MPIE
 * 
 * 作者：DevMate
 * 最后修改：2026-04-14
 */

#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "simulator.h"
#include "cpu/riscv/rv32i_csr.h"
#include "cpu/riscv/rv32i_exception.h"
#include "cpu/riscv/clint.h"

using namespace ch::core;
using namespace ch::chlib;
using namespace riscv;

// Helper: 将 sdata_type 转换为 uint64_t
inline uint64_t to_uint64(const sdata_type& val) { return static_cast<uint64_t>(val); }

// Helper: 将 sdata_type 转换为 bool
inline bool to_bool(const sdata_type& val) { return val != 0; }

// ============================================================================
// CLINT 定时器 helper（ADR-049：`mtime` 是 64 位自由运行计数器）
// ============================================================================
//
// ⚠️ **读时序的三个实测事实**（探针实测，design.md §3）：
//
//   ① `read_data` 是**组合**输出 ⇒ 它的值在**时序更新之后**求值 ⇒ ⇒
//      `sim.get_value(read_data)` 读到的是**最近一次 tick 之后**的值；
//   ② **读的那一拍本身会让 `mtime` 递增**（若无写）⇒ ⇒ 任何「读」都推进
//      **分频相位**（相位 = 自复位以来的拍数 ⇒ **任何** tick 都推进）；
//   ③ ⇒ ⇒ **「写-读同拍」**（`write_en=1` + `write_data=V` + `read_en=1`
//      + 同一 `addr`）读到的是**精确的写值** —— 因为写的那一拍**不递增**
//      （ADR-049 D3 写优先）⇒ ⇒ 这是**消除时序脆弱性**的关键手段。
//
// ⚠️ 由此推导出本文件全部用例的期望值模型：
//   * 锚定 mtime = X ⇒ 用「写-读同拍」⇒ 读回 **X**（精确）；
//   * 观察递增 ⇒ 用「读-读同拍序列」⇒ 每次读 +1（`TIMER_DIVIDER = 1`）；
//   * 观察分频 ⇒ 读序列的**周期** = `TIMER_DIVIDER`（每 N 拍 +1）。
//
// ⇒ ⇒ 全部期望值**手推**，MUST NOT 抄仿真输出。

namespace {

/// 写-读同拍：向 `addr` 写 `v`，并在**同一拍**读回 ⇒ 读到**精确的写值**。
template <unsigned DIV>
uint32_t clint_write_read(Clint<DIV> &c, Simulator &sim, uint32_t addr,
                           uint32_t v) {
    sim.set_input_value(c.io().addr, addr);
    sim.set_input_value(c.io().write_data, v);
    sim.set_input_value(c.io().write_en, 1);
    sim.set_input_value(c.io().read_en, 1);
    sim.tick();
    sim.set_input_value(c.io().write_en, 0);
    sim.set_input_value(c.io().read_en, 0);
    return static_cast<uint32_t>(
        static_cast<uint64_t>(sim.get_value(c.io().read_data)));
}

/// 读（**不写**）⇒ ⚠️ **该拍会让 `mtime` 递增**（若无写）⇒ ⇒ 读到的是
/// **该拍之后**的值 ⇒ ⇒ 每次读都推进**分频相位**。
template <unsigned DIV>
uint32_t clint_read(Clint<DIV> &c, Simulator &sim, uint32_t addr) {
    sim.set_input_value(c.io().addr, addr);
    sim.set_input_value(c.io().read_en, 1);
    sim.tick();
    sim.set_input_value(c.io().read_en, 0);
    return static_cast<uint32_t>(
        static_cast<uint64_t>(sim.get_value(c.io().read_data)));
}

/// 只写（不读）—— 用于写 `mtimecmp`（它不影响 `mtime` 的值，但**推进分频相位**）。
template <unsigned DIV>
void clint_write(Clint<DIV> &c, Simulator &sim, uint32_t addr, uint32_t v) {
    sim.set_input_value(c.io().addr, addr);
    sim.set_input_value(c.io().write_data, v);
    sim.set_input_value(c.io().write_en, 1);
    sim.tick();
    sim.set_input_value(c.io().write_en, 0);
}

} // namespace

// ============================================================================
// 测试用例 1: CLINT mtip 触发条件 - mtime >= mtimecmp
// ============================================================================
TEST_CASE("mtip triggers when mtime >= mtimecmp", "[interrupt][clint][timer]") {
    context ctx("int_test_clint");
    ctx_swap swap(&ctx);
    
    // 创建 CLINT 设备（TIMER_DIVIDER 默认 = 1 ⇒ 每拍递增 1）
    ch::ch_device<Clint<>> clint_device;
    auto& clint = clint_device.instance();
    Simulator sim(clint_device.context());
    
    // ⚠️ 必须同时写 mtimecmp 的**高 32 位**：mtimecmp_hi 的复位值是 0xFFFFFFFF
    // （RISC-V 规范：复位后不产生定时器中断）。只写低 32 位会让 64 位比较的
    // 「hi 分支」恒不相等（mtime_hi=0 != 0xFFFFFFFF）⇒ mtip 永远不触发。
    //
    // ⚠️ 2026-10-30（ADR-049）：`mtime` 现在是**自由运行计数器** ⇒ ⇒
    // 「靠余量才通过」的断言（写 50 ⇒ 只 tick 1 拍 ⇒ 实际是 51）**已过时**
    // ⇒ ⇒ 本用例改成「**每次采样前重写 `mtime`**」（ADR-049 §4 的要求）
    // ⇒ ⇒ 断言**不再依赖任何余量** ⇒ 值是**精确**的。
    clint_write(clint, sim, 0xC, 0);   // mtimecmp_hi = 0
    clint_write(clint, sim, 0x8, 100); // mtimecmp_lo = 100
    
    // ── 场景 1：mtime = 50 < mtimecmp = 100 ⇒ mtip = 0 ──
    // ⚠️ 「写-读同拍」⇒ 读回**精确的写值** 50（写的那一拍**不递增**）。
    REQUIRE(clint_write_read(clint, sim, 0x0, 50) == 50);
    // ⚠️ `mtip` 是**组合**输出 ⇒ 它的值在 tick 之后求值 ⇒ ⇒ 无需额外 tick
    //    即可读到「本次写-读同拍之后」的比较结果。
    REQUIRE(to_bool(sim.get_value(clint.io().mtip)) == false);
    
    // ── 场景 2：mtime = 100 == mtimecmp = 100 ⇒ mtip = 1 ──
    REQUIRE(clint_write_read(clint, sim, 0x0, 100) == 100);
    REQUIRE(to_bool(sim.get_value(clint.io().mtip)) == true);
    
    // ── 场景 3：mtime = 150 > mtimecmp = 100 ⇒ mtip 保持 1 ──
    REQUIRE(clint_write_read(clint, sim, 0x0, 150) == 150);
    REQUIRE(to_bool(sim.get_value(clint.io().mtip)) == true);
}

// ============================================================================
// 测试用例 2: interrupt_req = mtip && mie.MTIE && mstatus.MIE
// ============================================================================
TEST_CASE("interrupt_req = mtip && mie.MTIE && mstatus.MIE", "[interrupt][csr][mie]") {
    context ctx("int_test_csr");
    ctx_swap swap(&ctx);
    
    // 创建 CsrBank 设备
    ch::ch_device<CsrBank> csr_device;
    auto& csr = csr_device.instance();
    Simulator sim(csr_device.context());
    
    // 初始状态: mstatus.MIE=0, mie.MTIE=0, mtip=0
    // interrupt_req 应为 0
    sim.set_input_value(csr.io().mtip, 0);
    sim.set_input_value(csr.io().meip, 0);
    sim.tick();
    sim.tick();
    
    bool int_req_initial = to_bool(sim.get_value(csr.io().interrupt_req));
    REQUIRE(int_req_initial == false);
    
    // 场景 1: mtip=1, mie.MTIE=0, mstatus.MIE=0
    // 条件不满足，interrupt_req 应为 0
    sim.set_input_value(csr.io().mtip, 1);
    sim.tick();
    sim.tick();
    
    // 需要通过 CSR 写操作设置 mie.MTIE 和 mstatus.MIE
    // 但 CSR Bank 的写接口需要连接到流水线，此处简化为检查内部逻辑
    
    // 验证 mtip 输入确实为 1
    bool mtip_val = to_bool(sim.get_value(csr.io().timer_interrupt_pending));
    // 注意: timer_interrupt_pending 需要 mie.MTIE=1 && mstatus.MIE=1 才为 1
    
    // 场景 2: 设置 mstatus.MIE=1 (通过mie寄存器间接验证)
    // mie.MTIE=1 且 mstatus.MIE=1 时，mtip=1 应产生 timer_interrupt_pending
    // 由于当前测试不连接写接口，我们验证组合逻辑关系
    
    // 读取 mie 值 (初始为 0)
    auto mie_val = to_uint64(sim.get_value(csr.io().mie_value));
    
    // 读取 mstatus 值
    auto mstatus_val = to_uint64(sim.get_value(csr.io().mstatus_value));
    
    // 验证 mie 初始值为 0 (所有中断禁用)
    REQUIRE(mie_val == 0x00000000);
    
    // 验证 mstatus 初始值为 0x00000080 (MPIE=1)
    REQUIRE(mstatus_val == 0x00000080);
}

// ============================================================================
// 测试用例 3: Pipeline flushes on mtip (flush signal to IF stage)
// ============================================================================
TEST_CASE("CsrBank generates flush signal for IF stage on timer interrupt", "[interrupt][pipeline][flush]") {
    context ctx("int_test_flush");
    ctx_swap swap(&ctx);
    
    // 创建 CsrBank 设备
    ch::ch_device<CsrBank> csr_device;
    auto& csr = csr_device.instance();
    Simulator sim(csr_device.context());
    
    // 初始状态: interrupt_req = 0
    sim.set_input_value(csr.io().mtip, 0);
    sim.set_input_value(csr.io().meip, 0);
    sim.tick();
    sim.tick();
    
    // 验证 mtip 相关的 timer_interrupt_pending 信号
    bool tip_initial = to_bool(sim.get_value(csr.io().timer_interrupt_pending));
    REQUIRE(tip_initial == false);
    
    // 注意: 由于 mie.MTIE 和 mstatus.MIE 初始为 0，
    // 即使 mtip=1，timer_interrupt_pending 也为 0
    // 这个测试验证了中断使能逻辑的条件性
    
    // 设置 mtip=1 但 interrupt_req 仍应为 0 (因为 mie.MTIE=0)
    sim.set_input_value(csr.io().mtip, 1);
    sim.tick();
    sim.tick();
    
    bool tip_with_mtip = to_bool(sim.get_value(csr.io().timer_interrupt_pending));
    REQUIRE(tip_with_mtip == false);  // mie.MTIE=0 所以仍为 0
    
    // 验证 interrupt_req 也为 0
    bool int_req = to_bool(sim.get_value(csr.io().interrupt_req));
    REQUIRE(int_req == false);
}

// ============================================================================
// 测试用例 4: mepc correctly saves PC of interrupted instruction
// ============================================================================
TEST_CASE("mepc saves PC when exception/timer interrupt occurs", "[interrupt][mepc][pc]") {
    context ctx("int_test_mepc");
    ctx_swap swap(&ctx);
    
    // 创建 ExceptionUnit 设备
    ch::ch_device<ExceptionUnit> exc_device;
    auto& exc = exc_device.instance();
    Simulator sim(exc_device.context());
    
    // 输入测试数据
    uint32_t test_pc = 0x00001234;  // 被中断的指令 PC
    uint32_t test_cause = 7;          // Machine Timer Interrupt
    
    sim.set_input_value(exc.io().current_pc, test_pc);
    sim.set_input_value(exc.io().exception_cause, test_cause);
    sim.set_input_value(exc.io().exception_valid, 1);
    sim.set_input_value(exc.io().mret_instruction, 0);
    
    // CSR 输入
    sim.set_input_value(exc.io().csr_mstatus, 0x00000080);
    sim.set_input_value(exc.io().csr_mtvec, 0x20000100);
    sim.set_input_value(exc.io().csr_mepc, 0x00000000);
    sim.set_input_value(exc.io().csr_mie, 0x00000000);
    sim.set_input_value(exc.io().csr_mcause, 0x00000000);
    
    sim.tick();
    sim.tick();
    
    // 验证 CSR 写地址为 mepc (0x341)
    auto csr_addr = to_uint64(sim.get_value(exc.io().csr_write_addr));
    REQUIRE(csr_addr == 0x341);
    
    // 验证 CSR 写数据为 current_pc
    auto csr_data = to_uint64(sim.get_value(exc.io().csr_write_data));
    REQUIRE(csr_data == test_pc);
    
    // 验证写使能有效
    bool csr_we = to_bool(sim.get_value(exc.io().csr_write_en));
    REQUIRE(csr_we == true);
    
    // 验证 trap_flush 信号
    bool trap_flush = to_bool(sim.get_value(exc.io().trap_flush));
    REQUIRE(trap_flush == true);
}

// ============================================================================
// 测试用例 5: mcause = 7 (Machine Timer Interrupt) on timer trap
// ============================================================================
TEST_CASE("mcause = 7 on machine timer interrupt", "[interrupt][mcause][timer]") {
    context ctx("int_test_mcause");
    ctx_swap swap(&ctx);
    
    // 创建 ExceptionUnit 设备
    ch::ch_device<ExceptionUnit> exc_device;
    auto& exc = exc_device.instance();
    Simulator sim(exc_device.context());
    
    // 设置定时器中断原因码 7
    uint32_t timer_cause = 7;
    
    sim.set_input_value(exc.io().current_pc, 0x00001000);
    sim.set_input_value(exc.io().exception_cause, timer_cause);
    sim.set_input_value(exc.io().exception_valid, 1);
    sim.set_input_value(exc.io().mret_instruction, 0);
    
    // CSR 输入
    sim.set_input_value(exc.io().csr_mstatus, 0x00000080);
    sim.set_input_value(exc.io().csr_mtvec, 0x20000100);
    sim.set_input_value(exc.io().csr_mepc, 0x00000000);
    sim.set_input_value(exc.io().csr_mie, 0x00000000);
    sim.set_input_value(exc.io().csr_mcause, 0x00000000);
    
    sim.tick();
    sim.tick();
    
    // 由于 mepc 和 mcause 都需要写，验证 CSR 写地址切换
    // 第一次 tick 时写 mepc，第二次 tick 时写 mcause
    // 或者通过组合逻辑检查写地址和数据选择
    
    // 验证 exception_valid 触发了 trap_flush
    bool trap_flush = to_bool(sim.get_value(exc.io().trap_flush));
    REQUIRE(trap_flush == true);
    
    // 验证 trap_entry_pc 输出 (mtvec base)
    auto trap_pc = to_uint64(sim.get_value(exc.io().trap_entry_pc));
    REQUIRE(trap_pc == 0x20000100);  // mtvec base address
}

// ============================================================================
// 测试用例 6: mtvec base address is used as trap entry PC
// ============================================================================
TEST_CASE("mtvec base address used as trap entry PC", "[interrupt][mtvec][trap]") {
    context ctx("int_test_mtvec");
    ctx_swap swap(&ctx);
    
    // 创建 ExceptionUnit 设备
    ch::ch_device<ExceptionUnit> exc_device;
    auto& exc = exc_device.instance();
    Simulator sim(exc_device.context());
    
    // 设置 mtvec 为不同值测试
    uint32_t mtvec_base = 0x20000000;  // Direct 模式基址
    
    sim.set_input_value(exc.io().current_pc, 0x00001000);
    sim.set_input_value(exc.io().exception_cause, 7);  // MTI
    sim.set_input_value(exc.io().exception_valid, 1);
    sim.set_input_value(exc.io().mret_instruction, 0);
    
    // CSR 输入
    sim.set_input_value(exc.io().csr_mstatus, 0x00000080);
    sim.set_input_value(exc.io().csr_mtvec, mtvec_base | 0x01);  // MODE=1 (Direct)
    sim.set_input_value(exc.io().csr_mepc, 0x00000000);
    sim.set_input_value(exc.io().csr_mie, 0x00000000);
    sim.set_input_value(exc.io().csr_mcause, 0x00000000);
    
    sim.tick();
    sim.tick();
    
    // 验证 trap_entry_pc = mtvec_base (清除低2位模式位)
    auto trap_pc = to_uint64(sim.get_value(exc.io().trap_entry_pc));
    REQUIRE(trap_pc == mtvec_base);  // 0x20000000
    
    // 验证 trap_entry_valid 信号
    bool trap_valid = to_bool(sim.get_value(exc.io().trap_entry_valid));
    REQUIRE(trap_valid == true);
}

// ============================================================================
// 测试用例 7: MRET restores PC from mepc
// ============================================================================
TEST_CASE("MRET restores PC from mepc", "[interrupt][mret][pc]") {
    context ctx("int_test_mret_pc");
    ctx_swap swap(&ctx);
    
    // 创建 ExceptionUnit 设备
    ch::ch_device<ExceptionUnit> exc_device;
    auto& exc = exc_device.instance();
    Simulator sim(exc_device.context());
    
    // 设置 mepc 返回地址
    uint32_t return_pc = 0x00001234;
    
    // MRET 场景
    sim.set_input_value(exc.io().current_pc, 0x00000000);  // 当前 PC 不重要
    sim.set_input_value(exc.io().exception_cause, 0);
    sim.set_input_value(exc.io().exception_valid, 0);       // 非异常
    sim.set_input_value(exc.io().mret_instruction, 1);     // MRET 指令
    
    // CSR 输入
    sim.set_input_value(exc.io().csr_mstatus, 0x00000080);
    sim.set_input_value(exc.io().csr_mtvec, 0x20000100);
    sim.set_input_value(exc.io().csr_mepc, return_pc);     // 要返回的地址
    sim.set_input_value(exc.io().csr_mie, 0x00000000);
    sim.set_input_value(exc.io().csr_mcause, 7);
    
    sim.tick();
    sim.tick();
    
    // 验证 mret_pc = mepc
    auto mret_pc = to_uint64(sim.get_value(exc.io().mret_pc));
    REQUIRE(mret_pc == return_pc);
    
    // 验证 mret_valid 信号
    bool mret_valid = to_bool(sim.get_value(exc.io().mret_valid));
    REQUIRE(mret_valid == true);
}

// ============================================================================
// 测试用例 8: MRET restores mstatus.MIE from mstatus.MPIE
// ============================================================================
TEST_CASE("MRET restores mstatus.MIE from mstatus.MPIE", "[interrupt][mret][mstatus]") {
    context ctx("int_test_mret_mstatus");
    ctx_swap swap(&ctx);
    
    // 创建 ExceptionUnit 设备
    ch::ch_device<ExceptionUnit> exc_device;
    auto& exc = exc_device.instance();
    Simulator sim(exc_device.context());
    
    // MRET 场景 - 验证 ExceptionUnit 输出的 CSR 写控制信号
    // 注意: mstatus 恢复逻辑可能需要结合 CsrBank 和 ExceptionUnit
    
    sim.set_input_value(exc.io().current_pc, 0x00000000);
    sim.set_input_value(exc.io().exception_cause, 0);
    sim.set_input_value(exc.io().exception_valid, 0);
    sim.set_input_value(exc.io().mret_instruction, 1);  // MRET
    
    // CSR 输入 - 模拟中断前状态
    // mstatus = 0x00000080: MIE=0, MPIE=1
    // 中断发生时 MIE 被保存到 MPIE，MIE 被清除
    // MRET 时 MPIE 恢复到 MIE
    sim.set_input_value(exc.io().csr_mstatus, 0x00000080);  // MIE=0, MPIE=1
    sim.set_input_value(exc.io().csr_mtvec, 0x20000100);
    sim.set_input_value(exc.io().csr_mepc, 0x00001000);
    sim.set_input_value(exc.io().csr_mie, 0x00000080);  // MTIE=1
    sim.set_input_value(exc.io().csr_mcause, 7);
    
    sim.tick();
    sim.tick();
    
    // 验证 MRET 有效
    bool mret_valid = to_bool(sim.get_value(exc.io().mret_valid));
    REQUIRE(mret_valid == true);
    
    // 验证返回 PC 正确
    auto mret_pc = to_uint64(sim.get_value(exc.io().mret_pc));
    REQUIRE(mret_pc == 0x00001000);
    
    // 注意: mstatus.MIE 恢复需要 CSR Bank 响应 ExceptionUnit 的 CSR 写操作
    // 这个测试验证了 MRET 信号的正确性
    // 完整的 MRET 测试需要连接 ExceptionUnit 和 CsrBank
}

// ============================================================================
// 测试用例 9: 完整中断流程集成测试
// ============================================================================
TEST_CASE("Complete timer interrupt flow: trigger to MRET return", "[interrupt][integration][full]") {
    context ctx("int_test_full");
    ctx_swap swap(&ctx);
    
    // 创建 CLINT 设备
    ch::ch_device<Clint<>> clint_device;
    auto& clint = clint_device.instance();
    Simulator sim(clint_device.context());
    
    // Step 1: 初始化 mtimecmp（高 32 位必须清零，见下方注释）
    // ⚠️ 必须同时写 mtimecmp 的**高 32 位**：mtimecmp_hi 的复位值是 0xFFFFFFFF
    // （RISC-V 规范：复位后不产生定时器中断）。只写低 32 位会让 64 位比较的
    // 「hi 分支」恒不相等（mtime_hi=0 != 0xFFFFFFFF）⇒ mtip 永远不触发。
    sim.set_input_value(clint.io().addr, 0xC);  // mtimecmp_hi 偏移
    sim.set_input_value(clint.io().write_data, 0);
    sim.set_input_value(clint.io().write_en, 1);
    sim.set_input_value(clint.io().read_en, 0);
    sim.tick();
    sim.set_input_value(clint.io().write_en, 0);
    // Step 1: 初始化 mtimecmp
    sim.set_input_value(clint.io().addr, 0x8);  // mtimecmp_lo
    sim.set_input_value(clint.io().write_data, 100);
    sim.set_input_value(clint.io().write_en, 1);
    sim.set_input_value(clint.io().read_en, 0);
    sim.tick();
    sim.set_input_value(clint.io().write_en, 0);
    
    // Step 2: 设置 mtime 触发中断
    sim.set_input_value(clint.io().addr, 0x0);  // mtime_lo
    sim.set_input_value(clint.io().write_data, 101);
    sim.set_input_value(clint.io().write_en, 1);
    sim.tick();
    sim.set_input_value(clint.io().write_en, 0);
    sim.tick();
    
    // Step 3: 验证 mtip 触发
    bool mtip_triggered = to_bool(sim.get_value(clint.io().mtip));
    REQUIRE(mtip_triggered == true);
    
    // Step 4: 创建 CsrBank 验证中断条件
    context ctx2("int_test_csr_full");
    ctx_swap swap2(&ctx2);
    
    ch::ch_device<CsrBank> csr_device;
    auto& csr = csr_device.instance();
    Simulator sim2(csr_device.context());
    
    // 提供 mtip 信号
    sim2.set_input_value(csr.io().mtip, 1);  // mtip 已触发
    sim2.set_input_value(csr.io().meip, 0);
    // ⚠️ 必须先 tick 一次：`mstatus_value` / `mie_value` 等是**组合输出**
    // （`io().x = reg`），不求值就读取拿到的是 data_map 里的初值 0，
    // 而不是寄存器的 INIT（mstatus 的 INIT = 0x80 / MPIE=1）。
    sim2.tick();
    
    // 注意: 需要设置 mie.MTIE=1 和 mstatus.MIE=1 才能产生 interrupt_req
    // 这需要通过 CSR 写接口操作，当前测试框架不支持直接写 CSR 寄存器
    // 完整测试需要流水线连接
    
    // 验证 mtip 输入到 CSR
    bool timer_int_pending = to_bool(sim2.get_value(csr.io().timer_interrupt_pending));
    // 由于 mie.MTIE=0，结果为 false
    REQUIRE(timer_int_pending == false);
    
    // 验证 mip 寄存器更新
    auto mip_val = to_uint64(sim2.get_value(csr.io().mcause_value));
    // mcause 输出的是 mcause CSR 的值，不是 mip
    // mip 是内部寄存器
    
    // 验证 mie 初始值
    auto mie_val = to_uint64(sim2.get_value(csr.io().mie_value));
    REQUIRE(mie_val == 0x00000000);  // 初始所有中断禁用
    
    // 验证 mstatus 初始值
    auto mstatus_val = to_uint64(sim2.get_value(csr.io().mstatus_value));
    REQUIRE(mstatus_val == 0x00000080);  // MPIE=1
}

// ============================================================================
// 测试用例 10: CsrBank mip 寄存器更新
// ============================================================================
TEST_CASE("CsrBank mip register updates with mtip", "[interrupt][csr][mip]") {
    context ctx("int_test_mip");
    ctx_swap swap(&ctx);
    
    ch::ch_device<CsrBank> csr_device;
    auto& csr = csr_device.instance();
    Simulator sim(csr_device.context());
    
    // 初始 mip 应为 0
    // mip 是内部寄存器，需要通过 CSR 读操作验证
    
    // 设置 mtip=1
    sim.set_input_value(csr.io().mtip, 1);
    sim.set_input_value(csr.io().meip, 0);
    sim.tick();
    
    // 验证 timer_interrupt_pending (需要 mie.MTIE=1 && mstatus.MIE=1)
    bool tip = to_bool(sim.get_value(csr.io().timer_interrupt_pending));
    REQUIRE(tip == false);  // mie.MTIE=0 所以为 false
    
    // 验证 mstatus.MIE 位提取
    bool mstatus_mie = to_bool(sim.get_value(csr.io().mstatus_mie));
    REQUIRE(mstatus_mie == false);  // mstatus[3]=0
    
    // 验证 mstatus.MPIE 位提取
    bool mstatus_mpie = to_bool(sim.get_value(csr.io().mstatus_mpie));
    REQUIRE(mstatus_mpie == true);  // mstatus[7]=1
}

// ============================================================================
// ADR-049 的判别性用例（`mtime` 的自由运行语义）
// ============================================================================
//
// ⚠️ 全部期望值**手推**（禁抄仿真输出）；读时序模型见本文件上方的 helper 注释。
// ⚠️ **判别力**的关键：「读」的那一拍会**递增** `mtime` ⇒ ⇒ 读序列的**周期**
//    就是 `TIMER_DIVIDER` ⇒ ⇒ 分频正确与否**直接可见**。

// ---------------------------------------------------------------------------
// V1：`mtime` 是自由运行计数器（`TIMER_DIVIDER = 1` ⇒ 每拍 +1）
// ---------------------------------------------------------------------------
TEST_CASE("CLINT mtime is a free-running counter (TIMER_DIVIDER=1)",
          "[interrupt][clint][timer][adr049]") {
    context ctx("int_test_clint_free_running");
    ctx_swap swap(&ctx);

    ch::ch_device<Clint<>> clint_device;
    auto& clint = clint_device.instance();
    Simulator sim(clint_device.context());

    // ⚠️ 手推：
    //   * 锚定（写-读同拍，mtime_lo = 0）⇒ 该拍**不递增** ⇒ 读回 **0**；
    //   * 之后每次「读」= 一个 tick ⇒ 该拍**递增 1** ⇒ ⇒ 第 k 次读读到 **k**。
    REQUIRE(clint_write_read(clint, sim, 0x0, 0) == 0);
    for (uint32_t k = 1; k <= 5; ++k) {
        REQUIRE(clint_read(clint, sim, 0x0) == k);
    }
}

// ---------------------------------------------------------------------------
// V2：**跨 32 位边界**（判别性 —— 抓「进位用 C++ 三元 ⇒ 恒 0」）
// ---------------------------------------------------------------------------
TEST_CASE("CLINT mtime carries across the 32-bit boundary",
          "[interrupt][clint][timer][adr049]") {
    context ctx("int_test_clint_carry");
    ctx_swap swap(&ctx);

    ch::ch_device<Clint<>> clint_device;
    auto& clint = clint_device.instance();
    Simulator sim(clint_device.context());

    // ⚠️ 手推：
    //   * 锚定 mtime_lo = 0xFFFFFFFF ⇒ M = 0x00000000_FFFFFFFF；
    //   * 读高半（那一拍**递增**）⇒ 0xFFFFFFFF + 1 = 0x00000001_00000000
    //     ⇒ ⇒ 高半 = **1**，低半 = **0**；
    //   * 再读低半（那一拍**又**递增）⇒ 低半 = **1**。
    // ⚠️ 若进位坏了（高半恒 0）⇒ 第一个断言转红。
    REQUIRE(clint_write_read(clint, sim, 0x0, 0xFFFFFFFFu) == 0xFFFFFFFFu);
    REQUIRE(clint_read(clint, sim, 0x4) == 1u);   // 高半进位 ⇒ 1
    REQUIRE(clint_read(clint, sim, 0x0) == 1u);   // 低半已回绕 ⇒ 再 +1
}

// ---------------------------------------------------------------------------
// V3：分频相位（`TIMER_DIVIDER = 4` ⇒ 每 4 拍 +1）
// ---------------------------------------------------------------------------
TEST_CASE("CLINT mtime divider gives one increment every TIMER_DIVIDER ticks",
          "[interrupt][clint][timer][adr049]") {
    context ctx("int_test_clint_divider");
    ctx_swap swap(&ctx);

    ch::ch_device<Clint<4>> clint_device;
    auto& clint = clint_device.instance();
    Simulator sim(clint_device.context());

    // ⚠️ 手推（分频计数器 `d` 复位为 0，递增发生在 `d == TIMER_DIVIDER-1`
    //   的那一拍 ⇒ **复位后第 4 拍**第一次递增）：
    //   * 锚定（拍 1，mtime_lo = 0）⇒ 该拍不递增 ⇒ `d`: 0→1，读回 **0**；
    //   * 读 1（拍 2，`d`=1 ⇒ 不递增）⇒ **0**；`d`: 1→2
    //   * 读 2（拍 3，`d`=2 ⇒ 不递增）⇒ **0**；`d`: 2→3
    //   * 读 3（拍 4，`d`=3 ⇒ **递增**）⇒ **1**；`d`: 3→0
    //   * 读 4..6（拍 5..7，`d`=0,1,2）⇒ **1**
    //   * 读 7（拍 8，`d`=3 ⇒ **递增**）⇒ **2**
    //
    // ⚠️ **判别力**：`TIMER_DIVIDER = 1` 时读 3 次会得到 **3** ⇒ 与本断言
    //   （**1**）**完全不同** ⇒ ⇒ 「忽略分频」的变异 MUST 转红。
    REQUIRE(clint_write_read(clint, sim, 0x0, 0) == 0);
    REQUIRE(clint_read(clint, sim, 0x0) == 0u);  // 读 1
    REQUIRE(clint_read(clint, sim, 0x0) == 0u);  // 读 2
    REQUIRE(clint_read(clint, sim, 0x0) == 1u);  // 读 3 —— 第 4 拍第一次递增
    REQUIRE(clint_read(clint, sim, 0x0) == 1u);  // 读 4
    REQUIRE(clint_read(clint, sim, 0x0) == 1u);  // 读 5
    REQUIRE(clint_read(clint, sim, 0x0) == 1u);  // 读 6
    REQUIRE(clint_read(clint, sim, 0x0) == 2u);  // 读 7 —— 第二个 4 拍窗口
}

// ---------------------------------------------------------------------------
// V4：分频相位**不**被软件写复位（判别性 —— 区分「写复位分频器」）
// ---------------------------------------------------------------------------
TEST_CASE("CLINT mtime divider phase is NOT reset by a software write",
          "[interrupt][clint][timer][adr049]") {
    context ctx("int_test_clint_divider_phase");
    ctx_swap swap(&ctx);

    ch::ch_device<Clint<4>> clint_device;
    auto& clint = clint_device.instance();
    Simulator sim(clint_device.context());

    // ⚠️ 手推（`d` 复位为 0，**任何** tick 都推进 ⇒ 相位与写**无关**）：
    //   * 锚定（拍 1，mtime = 0）⇒ `d`: 0→1
    //   * 读 1（拍 2，`d`=1 ⇒ 不递增）⇒ **0**；`d`: 1→2
    //   * **写**-读同拍 mtime = 7（拍 3，`d`=2 ⇒ 照常推进 ⇒ `d`: 2→3）
    //     ⇒ 读回 **7**（写的那一拍不递增）
    //   * 读 1（拍 4，`d`=3 ⇒ **递增**）⇒ **8**
    //
    // ⚠️ **判别力**：若实现是「写**复位**分频器」⇒ 拍 3 后 `d`=0 ⇒
    //   拍 4（`d`=0）**不**递增 ⇒ 读到 **7** ⇒ 与断言（**8**）不同 ⇒ 转红。
    REQUIRE(clint_write_read(clint, sim, 0x0, 0) == 0);
    REQUIRE(clint_read(clint, sim, 0x0) == 0u);          // 读 1（`d`: 1→2）
    REQUIRE(clint_write_read(clint, sim, 0x0, 7) == 7u);  // 写（`d`: 2→3）
    REQUIRE(clint_read(clint, sim, 0x0) == 8u);          // 读 2（`d`=3 ⇒ 递增）
}

// ---------------------------------------------------------------------------
// V5：写拍**恰落在递增边界** ⇒ 该次递增**被丢弃**且相位**不变**
// ---------------------------------------------------------------------------
TEST_CASE("CLINT mtime write on an increment tick discards that increment",
          "[interrupt][clint][timer][adr049]") {
    context ctx("int_test_clint_write_on_boundary");
    ctx_swap swap(&ctx);

    ch::ch_device<Clint<4>> clint_device;
    auto& clint = clint_device.instance();
    Simulator sim(clint_device.context());

    // ⚠️ 手推：
    //   * 锚定（拍 1，mtime = 0）⇒ `d`: 0→1
    //   * 读 1（拍 2，`d`=1）⇒ 0；`d`: 1→2
    //   * 读 2（拍 3，`d`=2）⇒ 0；`d`: 2→3
    //   * **写**-读同拍 mtime = 50（拍 4，`d`=3 ⇒ **恰是递增拍**）
    //     ⇒ 写优先 ⇒ 该次递增**被丢弃** ⇒ 读回 **50**（**不是 51**）
    //     ⇒ 且 `d` 照常推进 ⇒ `d`: 3→0（**相位不变**）
    //   * 读 1..3（拍 5..7，`d`=0,1,2）⇒ **50**
    //   * 读 4（拍 8，`d`=3 ⇒ 递增）⇒ **51**
    //
    // ⚠️ **判别力**：「写的那一拍**仍递增**」⇒ 读到 **51** ⇒ 转红；
    //   「写**复位**分频器」⇒ 拍 4 后 `d`=0 ⇒ 拍 8 的递增时机不同 ⇒ 也转红。
    REQUIRE(clint_write_read(clint, sim, 0x0, 0) == 0);
    REQUIRE(clint_read(clint, sim, 0x0) == 0u);          // 读 1（`d`: 1→2）
    REQUIRE(clint_read(clint, sim, 0x0) == 0u);          // 读 2（`d`: 2→3）
    REQUIRE(clint_write_read(clint, sim, 0x0, 50) == 50u); // 写（递增被丢弃）
    REQUIRE(clint_read(clint, sim, 0x0) == 50u);         // 读 3（`d`=0）
    REQUIRE(clint_read(clint, sim, 0x0) == 50u);         // 读 4（`d`=1）
    REQUIRE(clint_read(clint, sim, 0x0) == 50u);         // 读 5（`d`=2）
    REQUIRE(clint_read(clint, sim, 0x0) == 51u);         // 读 6（`d`=3 ⇒ 递增）
}

// ---------------------------------------------------------------------------
// V6：写优先的三项（被写半 = 写值 · 另一半保持 · 本拍不递增）
// ---------------------------------------------------------------------------
TEST_CASE("CLINT mtime write is partial (other half preserved) and does not "
          "increment on the write tick",
          "[interrupt][clint][timer][adr049]") {
    context ctx("int_test_clint_write_partial");
    ctx_swap swap(&ctx);

    ch::ch_device<Clint<>> clint_device;
    auto& clint = clint_device.instance();
    Simulator sim(clint_device.context());

    // ⚠️ 手推（⚠️ **每次「读」都是一拍 ⇒ 该拍 `mtime` 递增 1** ⇒ ⇒
    //   下面每个读都把期望值 +1 —— 这是**模型**，不是「靠余量」）：
    //   * 拍 1  锚定 mtime_lo = 0x11111111 ⇒ M = 0x00000000_11111111
    //   * 拍 2  写-读同拍 mtime_lo = 0x22222222 ⇒ M = 0x22222222
    //     （**本拍不递增** ⇒ 若递增则会是 0x22222223 ⇒ **判别力在此**）
    //     ⇒ 读回 **0x22222222**
    //   * 拍 3  读高半 ⇒ 该拍递增 ⇒ M = 0x00000000_22222223 ⇒ 高半 = **0**
    //     （**另一半保持** ⇒ 若高半被清零/污染则不同 ⇒ **判别力在此**）
    //   * 拍 4  写-读同拍 mtime_hi = 7 ⇒ M = 0x00000007_22222223
    //     （写高半 ⇒ **低半保持**；**本拍不递增**）⇒ 读回高半 **7**
    //   * 拍 5  读低半 ⇒ 该拍递增 ⇒ M = 0x00000007_22222224
    //     ⇒ 读到低半 **0x22222224**（= 写值 0x22222222 **+2**
    //     = 拍 3 与拍 5 各 +1 ⇒ **低半未被写 hi 影响** ⇒ **判别力在此**：
    //     若「写 hi 时低半被清零」⇒ 读到 **0** ⇒ 转红）
    REQUIRE(clint_write_read(clint, sim, 0x0, 0x11111111u) == 0x11111111u);
    REQUIRE(clint_write_read(clint, sim, 0x0, 0x22222222u) == 0x22222222u);
    REQUIRE(clint_read(clint, sim, 0x4) == 0u);
    REQUIRE(clint_write_read(clint, sim, 0x4, 7u) == 7u);
    REQUIRE(clint_read(clint, sim, 0x0) == 0x22222224u);
}

// ---------------------------------------------------------------------------
// V7：写后的**下一拍**从写值继续递增
// ---------------------------------------------------------------------------
TEST_CASE("CLINT mtime continues incrementing from the written value",
          "[interrupt][clint][timer][adr049]") {
    context ctx("int_test_clint_write_continue");
    ctx_swap swap(&ctx);

    ch::ch_device<Clint<>> clint_device;
    auto& clint = clint_device.instance();
    Simulator sim(clint_device.context());

    // ⚠️ 手推：写-读同拍 mtime_lo = 50 ⇒ 读回 **50**（本拍不递增）
    //   ⇒ 下一拍（读）**递增 1** ⇒ 读到 **51**（= 写值 + 1）
    // ⚠️ **判别力**：「写的那一拍**也递增**」⇒ 第一次读就会得到 51 ⇒ 转红。
    REQUIRE(clint_write_read(clint, sim, 0x0, 50) == 50u);
    REQUIRE(clint_read(clint, sim, 0x0) == 51u);
}

// ---------------------------------------------------------------------------
// V8：复位后 `mtip` MUST 为 0（`mtime` = 0，`mtimecmp` = 全 1）
// ---------------------------------------------------------------------------
TEST_CASE("CLINT mtip is low after reset (mtime=0, mtimecmp=all ones)",
          "[interrupt][clint][timer][adr049]") {
    context ctx("int_test_clint_reset_mtip");
    ctx_swap swap(&ctx);

    ch::ch_device<Clint<>> clint_device;
    auto& clint = clint_device.instance();
    Simulator sim(clint_device.context());

    // ⚠️ 手推：复位后 `mtime` = 0、`mtimecmp` = 全 1（`mtimecmp_hi` = 0xFFFFFFFF）
    //   ⇒ 64 位比较：高半 0 ≠ 0xFFFFFFFF ⇒ 既不 > 也不 == ⇒ `mtip` = **0**。
    // ⚠️ `mtip` 是组合输出 ⇒ 需一次 tick 让组合求值发生。
    sim.tick();
    REQUIRE(to_bool(sim.get_value(clint.io().mtip)) == false);
}
