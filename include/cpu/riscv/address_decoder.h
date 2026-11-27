/**
 * @file address_decoder.h
 * @brief RISC-V MMIO 地址解码器 - 将 CPU 内存访问路由到正确的外设
 *
 * 功能:
 * - 解析 CPU 32 位地址
 * - 根据地址范围生成片选信号 (one-hot 编码)
 * - 计算本地地址偏移 (移除基地址)
 *
 * 内存映射:
 * | 区域   | 基地址          | 大小   | 片选信号   |
 * |--------|-----------------|--------|------------|
 * | NULL   | 0x00000000      | 512MB  | error_cs   |
 * | I-TCM  | 0x80000000      | 16KB   | i_tcm_cs   |
 * | D-TCM  | 0x80010000      | 16KB   | d_tcm_cs   |
 * | CLINT  | 0x80100000      | 4KB    | clint_cs   |
 * | UART   | 0x80101000      | 4KB    | uart_cs    |
 * | GPIO   | 0x80102000      | 4KB    | gpio_cs    |
 * | SPI    | 0x80103000      | 4KB    | spi_cs     |
 * | I2C    | 0x80104000      | 4KB    | i2c_cs     |
 * | PWM    | 0x80105000      | 4KB    | pwm_cs     |
 * | DMA    | 0x80106000      | 4KB    | dma_cs     |
 * | TIMER  | 0x80107000      | 4KB    | timer_cs   |
 *
 * ⚠️ **2026-11-24 · change `rv32i-soc-mmio-channel-2026-11`（G-6）**：
 *   ① 本表**全部基址 / 容量改为派生**自 `rv32i_mem_map.h`（**单一真相**）
 *      ⇒ ⇒ 禁手写字面量（原先 `ADDR_CLINT_BASE` 等是**手写**的 ⇒ ⇒ 与库层表**两份真相**）
 *      ⇒ ⇒ 顺带订正「I-TCM / D-TCM **64 KB**」的**既有漂移**（真实容量 = **16 KB**）
 *   ② **片选由 5 个扩到 10 个**（2 TCM + **8 MMIO**）+ `error_cs` ⇒ ⇒ **11 个片选类输出**
 *      （另有 `local_addr` ⇒ ⇒ 共 **12** 个输出端口）
 *   ③ 新增 4 个输出端口 = **公开面变更** ⇒ ⇒ 已登记 `public-api-surface.md`
 *
 * ⚠️ **2026-11-27 · change `rv32i-soc-peripherals-interconnect-2026-11`（D-1）**：
 *   追加 **TIMER 段**（`0x8010_7000` ⇒ ⇒ **既有 7 段零移动**）⇒ ⇒ 新增片选
 *   `timer_cs`（**具名** ⇒ ⇒ 端口命名棘轮**零新增债**）⇒ ⇒ **公开面变更** ⇒ ⇒ 已登记。
 */

#pragma once
#include <cstdint>
#include "ch.hpp"
#include "component.h"

// ⚠️ **2026-11-14 · L1**：TCM 的 **base / size** 改为**派生**自
//    `rv32i_mem_map.h`（**单一真相**）⇒ ⇒ 修掉了「**声明 64 KB · 实现 16 KB**」
//    ⇒ ⇒ 那段不一致会让 `[0x8000_4000, 0x8001_0000)` 的访问**静默截断**
//    （PC 32 位 ⇒ TCM 索引 12 位 ⇒ 高位被丢）⇒ ⇒ 现在它落在**区域外** ⇒ ⇒ 可诊断。
//    ⇒ ⇒ ⚠️ **TCM 容量 MUST 与 `kTcmAddrWidth` 同步**（改一处 ⇒ 改两处）。
//
// ⚠️⚠️ **2026-11-24 · G-6：include MUST 在 namespace 之**外****
//    （**既有隐患 · 本次订正**）：此前本 include 位于 `namespace riscv { … }` **内部**
//    ⇒ ⇒ 该头的 `namespace riscv { … }` 变成**嵌套** ⇒ ⇒ 其符号落在
//    **`riscv::riscv::kItcm`** ⇒ ⇒ 而**另一个 TU**（直接 include 它）里的同名符号落在
//    **`riscv::kItcm`** ⇒ ⇒ ⇒ **两个 TU 里是「两个不同的实体」** ⇒ ⇒ **ODR 违规**
//    （且 `#pragma once` 让「谁先 include」决定符号落在哪 ⇒ ⇒ **顺序依赖**）。
//    ⇒ ⇒ 现象（2026-11-24 实测）：测试文件直接用 `in_mmio` ⇒ 编译报
//    「did you mean `riscv::riscv::in_mmio`」⇒ ⇒ **include 移到 namespace 之外**后
//    ⇒ 全仓**单一** `riscv::kItcm` / `riscv::in_mmio` ⇒ ⇒ ODR 隐患消除 ✓
#include "cpu/riscv/rv32i_mem_map.h"

namespace riscv {

using namespace ch::core;

// 地址空间基地址常量 (使用原生 uint32_t 避免 ch_uint constexpr 问题)
// riscv-tests ELF 期望: ITCM=0x80000000, DTCM=0x80010000, peripherals above

constexpr uint32_t ADDR_NULL_BASE  = 0x00000000;
constexpr uint32_t ADDR_ITCM_BASE  = static_cast<uint32_t>(riscv::kItcm.base);
constexpr uint32_t ADDR_DTCM_BASE  = static_cast<uint32_t>(riscv::kDtcm.base);
// ⚠️ **2026-11-24 · G-6**：7 个 MMIO 基址**全部派生**自 `rv32i_mem_map.h`
//    （**单一真相**）⇒ ⇒ 禁手写字面量 ⇒ ⇒ 与库层表**不可能**不一致。
constexpr uint32_t ADDR_CLINT_BASE = static_cast<uint32_t>(riscv::kClint.base);
constexpr uint32_t ADDR_UART_BASE  = static_cast<uint32_t>(riscv::kUart.base);
constexpr uint32_t ADDR_GPIO_BASE  = static_cast<uint32_t>(riscv::kGpio.base);
constexpr uint32_t ADDR_SPI_BASE   = static_cast<uint32_t>(riscv::kSpi.base);
constexpr uint32_t ADDR_I2C_BASE   = static_cast<uint32_t>(riscv::kI2c.base);
constexpr uint32_t ADDR_PWM_BASE   = static_cast<uint32_t>(riscv::kPwm.base);
constexpr uint32_t ADDR_DMA_BASE   = static_cast<uint32_t>(riscv::kDma.base);
// ⚠️ **2026-11-27 · D-1**：TIMER 段**追加在末尾**（`0x8010_7000`）⇒ ⇒ **既有 7 段零移动**
constexpr uint32_t ADDR_TIMER_BASE = static_cast<uint32_t>(riscv::kTimer.base);
// ⚠️ **size 由容量派生**（**禁**手写 0x10000 —— 那与硬件的 16 KB 不一致；
//    MMIO 段的 4 KB 同样**派生**自库层表 ⇒ ⇒ 禁在此处再写一遍 0x1000）
constexpr uint32_t ADDR_ITCM_SIZE  = static_cast<uint32_t>(riscv::kItcm.size);
constexpr uint32_t ADDR_DTCM_SIZE  = static_cast<uint32_t>(riscv::kDtcm.size);
constexpr uint32_t ADDR_CLINT_SIZE = static_cast<uint32_t>(riscv::kClint.size);
constexpr uint32_t ADDR_UART_SIZE  = static_cast<uint32_t>(riscv::kUart.size);
constexpr uint32_t ADDR_GPIO_SIZE  = static_cast<uint32_t>(riscv::kGpio.size);
constexpr uint32_t ADDR_SPI_SIZE   = static_cast<uint32_t>(riscv::kSpi.size);
constexpr uint32_t ADDR_I2C_SIZE   = static_cast<uint32_t>(riscv::kI2c.size);
constexpr uint32_t ADDR_PWM_SIZE   = static_cast<uint32_t>(riscv::kPwm.size);
constexpr uint32_t ADDR_DMA_SIZE   = static_cast<uint32_t>(riscv::kDma.size);
constexpr uint32_t ADDR_TIMER_SIZE = static_cast<uint32_t>(riscv::kTimer.size);
constexpr uint32_t ADDR_ITCM_END   = ADDR_ITCM_BASE + ADDR_ITCM_SIZE - 1;
constexpr uint32_t ADDR_DTCM_END   = ADDR_DTCM_BASE + ADDR_DTCM_SIZE - 1;
constexpr uint32_t ADDR_CLINT_END  = ADDR_CLINT_BASE + ADDR_CLINT_SIZE - 1;
constexpr uint32_t ADDR_UART_END   = ADDR_UART_BASE + ADDR_UART_SIZE - 1;
constexpr uint32_t ADDR_GPIO_END   = ADDR_GPIO_BASE + ADDR_GPIO_SIZE - 1;
constexpr uint32_t ADDR_SPI_END    = ADDR_SPI_BASE + ADDR_SPI_SIZE - 1;
constexpr uint32_t ADDR_I2C_END    = ADDR_I2C_BASE + ADDR_I2C_SIZE - 1;
constexpr uint32_t ADDR_PWM_END    = ADDR_PWM_BASE + ADDR_PWM_SIZE - 1;
constexpr uint32_t ADDR_DMA_END    = ADDR_DMA_BASE + ADDR_DMA_SIZE - 1;
constexpr uint32_t ADDR_TIMER_END  = ADDR_TIMER_BASE + ADDR_TIMER_SIZE - 1;

class AddressDecoder : public ch::Component {
public:
    __io(
        ch_in<ch_uint<32>> addr;
        ch_out<ch_bool>    i_tcm_cs;
        ch_out<ch_bool>    d_tcm_cs;
        ch_out<ch_bool>    clint_cs;
        ch_out<ch_bool>    uart_cs;
        ch_out<ch_bool>    gpio_cs;
        // ⚠️ **2026-11-24 · G-6**：新增 4 个 MMIO 片选（**总片选 9** = 2 TCM + 7 MMIO）
        // ⚠️ **端口命名棘轮**（`port-naming-ratchet-2026-11` N2）：**新增端口 MUST 具名**
        //    （`标识符 == 显式名字串` ⇒ ⇒ 不进豁免表 ⇒ ⇒ **债零增长**）⇒ ⇒ 故这 4 个
        //    **带**名字串 ⇒ ⇒ 而上方 5 个**存量**端口**保持原样**（它们在
        //    `.rddf/checks/port-naming-exempt.tsv` 里是**基线豁免** ⇒ ⇒ 改它们会改
        //    **产物端口名** ⇒ ⇒ C6 快照破 ⇒ ⇒ **本次不动**）。
        ch_out<ch_bool>    spi_cs{"spi_cs"};
        ch_out<ch_bool>    i2c_cs{"i2c_cs"};
        ch_out<ch_bool>    pwm_cs{"pwm_cs"};
        ch_out<ch_bool>    dma_cs{"dma_cs"};
        // ⚠️ **2026-11-27 · D-1**：TIMER 段（`0x8010_7000`）的片选 —— **具名**
        //    （端口命名棘轮：新增端口 MUST 具名 ⇒ ⇒ 零新增债）。
        ch_out<ch_bool>    timer_cs{"timer_cs"};
        ch_out<ch_bool>    error_cs;
        ch_out<ch_uint<32>> local_addr;
    )

    AddressDecoder(ch::Component* parent = nullptr, const std::string& name = "address_decoder")
        : ch::Component(parent, name) {}

    void create_ports() override {
        new (io_storage_) io_type;
    }

    void describe() override {
        ch_uint<32> addr_in = io().addr;

        auto itcm_base  = ch_uint<32>(ADDR_ITCM_BASE);
        auto dtcm_base  = ch_uint<32>(ADDR_DTCM_BASE);
        auto clint_base = ch_uint<32>(ADDR_CLINT_BASE);
        auto uart_base  = ch_uint<32>(ADDR_UART_BASE);
        auto gpio_base  = ch_uint<32>(ADDR_GPIO_BASE);
        auto spi_base   = ch_uint<32>(ADDR_SPI_BASE);
        auto i2c_base   = ch_uint<32>(ADDR_I2C_BASE);
        auto pwm_base   = ch_uint<32>(ADDR_PWM_BASE);
        auto dma_base   = ch_uint<32>(ADDR_DMA_BASE);
        auto timer_base = ch_uint<32>(ADDR_TIMER_BASE);

        auto itcm_end  = ch_uint<32>(ADDR_ITCM_END);
        auto dtcm_end  = ch_uint<32>(ADDR_DTCM_END);
        auto clint_end = ch_uint<32>(ADDR_CLINT_END);
        auto uart_end  = ch_uint<32>(ADDR_UART_END);
        auto gpio_end  = ch_uint<32>(ADDR_GPIO_END);
        auto spi_end   = ch_uint<32>(ADDR_SPI_END);
        auto i2c_end   = ch_uint<32>(ADDR_I2C_END);
        auto pwm_end   = ch_uint<32>(ADDR_PWM_END);
        auto dma_end   = ch_uint<32>(ADDR_DMA_END);
        auto timer_end = ch_uint<32>(ADDR_TIMER_END);

        ch_bool in_itcm = select(addr_in >= itcm_base, addr_in <= itcm_end, ch_bool(false));
        ch_bool in_dtcm = select(addr_in >= dtcm_base, addr_in <= dtcm_end, ch_bool(false));
        ch_bool in_clint = select(addr_in >= clint_base, addr_in <= clint_end, ch_bool(false));
        ch_bool in_uart = select(addr_in >= uart_base, addr_in <= uart_end, ch_bool(false));
        ch_bool in_gpio = select(addr_in >= gpio_base, addr_in <= gpio_end, ch_bool(false));
        ch_bool in_spi = select(addr_in >= spi_base, addr_in <= spi_end, ch_bool(false));
        ch_bool in_i2c = select(addr_in >= i2c_base, addr_in <= i2c_end, ch_bool(false));
        ch_bool in_pwm = select(addr_in >= pwm_base, addr_in <= pwm_end, ch_bool(false));
        ch_bool in_dma = select(addr_in >= dma_base, addr_in <= dma_end, ch_bool(false));
        ch_bool in_timer = select(addr_in >= timer_base, addr_in <= timer_end, ch_bool(false));

        ch_bool in_valid_region = select(in_itcm, ch_bool(true),
                              select(in_dtcm, ch_bool(true),
                              select(in_clint, ch_bool(true),
                              select(in_uart, ch_bool(true),
                              select(in_gpio, ch_bool(true),
                              select(in_spi, ch_bool(true),
                              select(in_i2c, ch_bool(true),
                              select(in_pwm, ch_bool(true),
                              select(in_dma, ch_bool(true),
                              select(in_timer, ch_bool(true),
                              ch_bool(false)))))))))));

        io().i_tcm_cs = in_itcm;
        io().d_tcm_cs = in_dtcm;
        io().clint_cs = in_clint;
        io().uart_cs = in_uart;
        io().gpio_cs = in_gpio;
        io().spi_cs = in_spi;
        io().i2c_cs = in_i2c;
        io().pwm_cs = in_pwm;
        io().dma_cs = in_dma;
        io().timer_cs = in_timer;
        io().error_cs = select(in_valid_region, ch_bool(false), ch_bool(true));

        ch_uint<32> itcm_local = addr_in - itcm_base;
        ch_uint<32> dtcm_local = addr_in - dtcm_base;
        ch_uint<32> clint_local = addr_in - clint_base;
        ch_uint<32> uart_local = addr_in - uart_base;
        ch_uint<32> gpio_local = addr_in - gpio_base;
        ch_uint<32> spi_local = addr_in - spi_base;
        ch_uint<32> i2c_local = addr_in - i2c_base;
        ch_uint<32> pwm_local = addr_in - pwm_base;
        ch_uint<32> dma_local = addr_in - dma_base;
        ch_uint<32> timer_local = addr_in - timer_base;

        io().local_addr = select(in_itcm, itcm_local,
                       select(in_dtcm, dtcm_local,
                       select(in_clint, clint_local,
                       select(in_uart, uart_local,
                       select(in_gpio, gpio_local,
                       select(in_spi, spi_local,
                       select(in_i2c, i2c_local,
                       select(in_pwm, pwm_local,
                       select(in_dma, dma_local,
                       select(in_timer, timer_local,
                       ch_uint<32>(0_d)))))))))));
    }
};

} // namespace riscv
