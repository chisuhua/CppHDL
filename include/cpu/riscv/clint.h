/**
 * CLINT (Core Local Interrupt Controller) Module
 *
 * RISC-V 处理器核本地中断控制器
 * 实现 mtime 计数器、mtimecmp 比较器和 MTIP 中断信号
 *
 * 寄存器映射 (基地址偏移):
 * - 0x0: mtime[31:0]     - 当前时间低32位 (R/W, reset=0)
 * - 0x4: mtime[63:32]    - 当前时间高32位 (R/W, reset=0)
 * - 0x8: mtimecmp[31:0]  - 时间比较值低32位 (R/W, reset=0xFFFFFFFF)
 * - 0xC: mtimecmp[63:32] - 时间比较值高32位 (R/W, reset=0xFFFFFFFF)
 *
 * Memory map: CLINT at 0x40000000 (address decoder handles base in Phase D)
 */

#pragma once

#include "ch.hpp"
#include "component.h"

namespace ch::chlib {

using namespace ch::core;

/**
 * CLINT 模块 - 64位定时器比较器
 *
 * 提供机器定时器中断 (MTIP) 用于 FreeRTOS 等操作系统
 * 当 mtime >= mtimecmp 时，mtip 中断信号置高
 *
 * ⚠️ **`mtime` 的语义（ADR-049，`docs/adr/ADR-049-clint-mtime-semantics.md`）**：
 *    `mtime` 是 **64 位自由运行计数器** —— 每个递增拍自增 1（无符号回绕），
 *    **不再**是「只能由软件写」的静态值 ⇒ ⇒ 软件可以靠读它做时基。
 *
 *    - **递增频率**：`TIMER_DIVIDER`（**编译期**常量，默认 1 = 每拍递增 1）。
 *      ⚠️ 分频相位是「复位后拍数的纯函数」：内部计数器 `d` 复位为 0，
 *      递增发生在 `d == TIMER_DIVIDER - 1` 的那一拍 ⇒ ⇒ **复位后第
 *      `TIMER_DIVIDER` 拍**发生第一次递增。
 *    - **写优先**（RISC-V 允许软件写 `mtime` 校时）：软件写的那一拍取**写值**、
 *      **另一半保持**、**本拍不递增** ⇒ ⇒ 写后的值成为**下一次**递增的起点。
 *      ⚠️ 写拍的分频计数器**照常推进**（相位与写无关）⇒ ⇒ 写拍若恰是递增拍，
 *      该次递增**被丢弃**但相位**不变**。
 *    - **复位值**：`mtime` = 0（`mtimecmp` 保持全 1）⇒ ⇒ 复位后**不**产生
 *      定时器中断。
 *    - **64 位读允许撕裂**（已知边界）：软件经 `read_data` + 地址偏移分两次读
 *      ⇒ 两次读之间计数器可能已进位 ⇒ ⇒ 软件应读两次高半检测翻转。
 *
 * ⚠️ **三条已实测的实现陷阱**（ADR-049 §1.4 + 实施期探针）—— MUST 规避：
 *    ① **进位 / 分频边界 MUST 用硬件原语**（`select`）：C++ 三元
 *       （`d == N-1 ? 1_d : 0_d`）在 `ch_bool` 上是**建图期**求值 ⇒ **恒取 0**
 *       ⇒ 计数器**永远跨不过边界**；
 *    ② **「保高半换低半」MUST 用 `bits_update<32>`**：`<<` 在 64 位上**编译不过**
 *       （框架 shift 用 C++ `1 << N`）；手写 `hi | lo`（**漏移位**）⇒ **静默清零高半**；
 *    ③ ⚠️ **`bits_update<32>` 在 64 位 target 上返回 64 位值** ⇒ ⇒ MUST 用
 *       **64 位**变量接收（MUST NOT 用 32 位接收 ⇒ **静默截断** ⇒ 「写高半」
 *       完全失效 —— 实施期**实测踩中**过）。
 */
template <unsigned TIMER_DIVIDER = 1> class Clint : public ch::Component {
public:
    __io(
        // MMIO 接口 (地址解码器路由到这些端口)
        ch_in<ch_uint<32>> addr;          // MMIO 地址
        ch_in<ch_uint<32>> write_data;    // 写数据
        ch_in<ch_bool>     write_en;      // 写使能
        ch_out<ch_uint<32>> read_data;    // 读数据
        ch_in<ch_bool>     read_en;       // 读使能

        // 中断输出
        ch_out<ch_bool>    mtip;          // 机器定时器中断 pending
    )

    Clint(ch::Component* parent = nullptr, const std::string& name = "clint")
        : ch::Component(parent, name) {}

    void create_ports() override {
        new (io_storage_) io_type;
    }

    void describe() override {
        // ── mtime：64 位自由运行计数器（形态 B：单一 64 位寄存器）──
        //
        // ⚠️ **形态选择**（ADR-049 D6）：选「单一 64 位 `ch_reg` + `bits<>` 切片」
        //    ⇒ 进位**自动正确**（64 位加法由框架承担 ⇒ 回绕天然正确）⇒ 少一类 bug。
        ch_reg<ch_uint<64>> mtime(0_d);   // mtime[63:0]，复位值 0（ADR-049 D4）

        // ── 分频计数器 d（ADR-049 D2）──
        //
        // ⚠️ 宽度取 32 位：`TIMER_DIVIDER` 是编译期常量，实际远小于 2^32 ⇒
        //    `d + 1` 永不溢出 ⇒ 收窄回 32 位**不丢信息**（显式收窄以表明意图）。
        ch_reg<ch_uint<32>> d(0_d);       // 分频计数器，复位为 0

        // mtimecmp 寄存器 - 分为高低32位
        // 复位值为 0xFFFFFFFF 以便系统启动时不会立即触发中断
        ch_reg<ch_uint<32>> mtimecmp_lo(0xFFFFFFFF);  // mtimecmp[31:0]
        ch_reg<ch_uint<32>> mtimecmp_hi(0xFFFFFFFF); // mtimecmp[63:32]

        // 计算地址偏移 (bits[3:2])
        // ⚠️ 必须是 bits[3:2] 而非 bits[1:0]：寄存器映射是 0x0/0x4/0x8/0xC
        // （见文件头注释），即偏移由地址的第 2-3 位给出。用 bits[1:0] 会让
        // 0x0/0x4/0x8/0xC 全部解码为偏移 0 ⇒ mtime_hi / mtimecmp_lo /
        // mtimecmp_hi 三个寄存器**完全不可达**（整个 CLINT 寄存器组只有
        // mtime_lo 可写）。该缺陷此前无任何覆盖 —— CLINT 从未被任何测试执行过。
        ch_uint<2> addr_offset = bits<3, 2>(io().addr);

        // 地址解码 - 使用 select 实现多路选择
        ch_bool is_mtime_lo  = (addr_offset == ch_uint<2>(0_d));
        ch_bool is_mtime_hi  = (addr_offset == ch_uint<2>(1_d));
        ch_bool is_mtimecmp_lo = (addr_offset == ch_uint<2>(2_d));
        ch_bool is_mtimecmp_hi = (addr_offset == ch_uint<2>(3_d));

        // 写使能信号
        ch_bool write_mtime_lo    = io().write_en & is_mtime_lo;
        ch_bool write_mtime_hi    = io().write_en & is_mtime_hi;
        ch_bool write_mtimecmp_lo = io().write_en & is_mtimecmp_lo;
        ch_bool write_mtimecmp_hi = io().write_en & is_mtimecmp_hi;

        // ── 递增条件（分频边界）──
        //
        // ⚠️ MUST 用**硬件原语**：本处用 `==` 产生 `ch_bool`（比较**不是** C++ 三元）
        //    ⇒ 参与 `select` 时是**运行期**求值 ⇒ 正确。
        //    ⚠️ 反例：`d == N-1 ? 1_d : 0_d` 是 **C++ 三元** ⇒ 在 `ch_bool` 上
        //    **建图期**求值 ⇒ **恒取 0** ⇒ 计数器永远跨不过边界。
        //
        // ⚠️ `TIMER_DIVIDER - 1` 是 `unsigned` 常量 ⇒ `TIMER_DIVIDER == 1` 时
        //    为 0 ⇒ `d == 0` **恒真** ⇒ 每拍递增 ✔（默认分频 = 1）。
        ch_bool inc_en = (d == ch_uint<32>(TIMER_DIVIDER - 1));

        // ── 写路径：保高半换低半 / 保低半换高半 ──
        //
        // ⚠️ `bits_update<32>` 返回的是 **64 位**（`make_uint_result<
        //    ch_width_v<Target>>`）⇒ MUST 用 **64 位**变量接收 ⇒ 用 32 位接收
        //    会**静默截断** ⇒ 「写高半」完全失效（实施期实测踩中）。
        ch_uint<64> mtime_after_write_lo = bits_update<32>(mtime, io().write_data, 0);
        ch_uint<64> mtime_after_write_hi = bits_update<32>(mtime, io().write_data, 32);

        // ── 递增值 ──
        //
        // ⚠️ `ch_uint<64> + ch_uint<64>` 的结果宽度是 **65 位** ⇒ MUST **显式**
        //    收窄回 64 位（无符号回绕，正是 RISC-V 期望的语义）。
        //    ⚠️ 若不收窄 ⇒ `mtime->next` 的类型与寄存器宽度不符 ⇒ 静默截断
        //    （⇒ 显式收窄以表明意图）。
        ch_uint<64> mtime_incremented = ch_uint<64>(mtime + ch_uint<64>(1_d));

        // ── mtime 的 next：写优先（ADR-049 D3）──
        //
        // ⇒ 写的那一拍：被写半 = 写值 · 另一半保持 · **本拍不递增**。
        // ⇒ 未写且是递增拍：+1。
        // ⇒ 其余：保持。
        ch_uint<64> mtime_next =
            select(write_mtime_lo, mtime_after_write_lo,
            select(write_mtime_hi, mtime_after_write_hi,
            select(inc_en, mtime_incremented,
                           mtime)));

        mtime->next = mtime_next;

        // ── 分频计数器 ──
        //
        // ⚠️ 写拍**照常推进**（相位 = 复位后拍数的纯函数）⇒ ⇒ `d_next` 与写
        //    **无关** ⇒ 「写复位分频器」与「写暂停分频器」两种实现都被排除。
        //
        // ⚠️ `d + 1` 的结果宽度是 33 位 ⇒ 显式收窄回 32 位（`d ≤ TIMER_DIVIDER-2`
        //    时 +1 ≤ TIMER_DIVIDER-1 ⇒ 不丢信息）。
        ch_uint<32> d_next =
            select(inc_en, ch_uint<32>(0_d),
                   ch_uint<32>(d + ch_uint<32>(1_d)));
        d->next = d_next;

        // 更新 mtimecmp_lo 寄存器
        mtimecmp_lo->next = select(write_mtimecmp_lo, io().write_data, mtimecmp_lo);

        // 更新 mtimecmp_hi 寄存器
        mtimecmp_hi->next = select(write_mtimecmp_hi, io().write_data, mtimecmp_hi);

        // ── 读路径：从 64 位 mtime 切出两半 ──
        ch_uint<32> mtime_lo_view = bits<31, 0>(mtime);
        ch_uint<32> mtime_hi_view = bits<63, 32>(mtime);

        // 读取数据多路选择
        ch_uint<32> read_val(0_d);
        read_val = select(is_mtime_lo, mtime_lo_view, read_val);
        read_val = select(is_mtime_hi, mtime_hi_view, read_val);
        read_val = select(is_mtimecmp_lo, mtimecmp_lo, read_val);
        read_val = select(is_mtimecmp_hi, mtimecmp_hi, read_val);

        // 读响应
        io().read_data = select(io().read_en, read_val, ch_uint<32>(0_d));

        // 64位比较: mtime >= mtimecmp
        // 分两种情况:
        // 1. hi 部分: mtime_hi > mtimecmp_hi -> mtime >= mtimecmp
        // 2. hi 部分相等: mtime_hi == mtimecmp_hi && mtime_lo >= mtimecmp_lo
        ch_bool hi_gt  = mtime_hi_view > mtimecmp_hi;
        ch_bool hi_eq  = mtime_hi_view == mtimecmp_hi;
        ch_bool lo_ge  = mtime_lo_view >= mtimecmp_lo;
        ch_bool mtip_cond = hi_gt | (hi_eq & lo_ge);

        // MTIP 中断输出
        io().mtip = mtip_cond;
    }

    // ⚠️ 2026-10-23（change `riscv-mini-test-triage-2026-10`）删除了三处死代码：
    //    `void tick()`（空实现，注释还声称「Simulator 在每个 tick 调用」——
    //    但没有任何调用方）、`get_mtime_lo()` / `get_mtimecmp_lo()`
    //    （恒返 0 的占位 getter，全仓零调用方，且会让读者以为能读到真值）。
    //    观察 mtime/mtimecmp 请走读端口 (`read_en` + `read_data`)。
    //
    // ⚠️ **2026-10-30**（ADR-049）：`mtime` 改为 **64 位自由运行计数器** ⇒ ⇒
    //    递增 MUST 由 RTL 实现 ⇒ ⇒ **不恢复**那个 C++ 侧的 `tick()`
    //    （C++ 侧方法**不在**仿真时序里 ⇒ 上一版它就是空实现）。
};

} // namespace ch::chlib
