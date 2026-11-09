/**
 * @file d_cache_write_buffer.h
 * @brief D-Cache Write Buffer wrapper —— ⚠️ **直通式**（**尚未实现真缓冲**）
 *
 * ⚠️ **2026-10-28 行为订正**（change `riscv-mini-cache-wrapper-live-logic-2026-10`）：
 *   本文件此前用**普通 C++ `if`** 包裹 `ch_bool` 条件发射 RTL（**3 处**）
 *   ⇒ 建图期条件恒为假 ⇒ **分支体从不发射**
 *   ⇒ `ready` / `axi_valid` **恒为未驱动**；
 *     `axi_addr` / `axi_wdata` 的驱动**被注释掉**；
 *     `axi_we` **从未被驱动过**。
 *   ⇒ 现在全部改为**无条件**的组合赋值（每个 `ch_out` 都被驱动）。
 *
 * **当前语义（直通式）**：
 * - `ready` = 「**本次写已被 AXI 接受**」= `req & we & axi_ready`
 *   ⇒ 消费方 MUST 保持 `req` / `we` / `addr` / `wdata` 直到 `ready` 拉高
 *     （AXI 主端的既有义务）；
 * - `axi_valid` = `req & we` ⚠️ **不含 `axi_ready`**
 *   ⇒ AXI 要求「VALID **不得**等 READY 才拉高」⇒ READY 为低时该写**仍**
 *     出现在总线上等待握手 ⇒ 本 wrapper **无状态/队列**可保留它
 *   ⇒ 若把 READY 引进 VALID ⇒ **写被静默丢弃**；
 * - `axi_addr` / `axi_wdata` / `axi_we` = CPU 请求的**直通**
 *   （**不**做块对齐、**不**缓冲）。
 *
 * ⚠️ **名字 over-promise（如实登记；backlog 见该 change 的 design.md §7）**：
 *   本组件**不缓冲**、**不合并**、**不仲裁** ⇒ ⚠️ **2026-11-09（phase-6 **P9** ·
 *   Zero-Debt）**：原 `WriteBufferEntry buffer[4]` 死数组（**零消费者**）**已删除**
 *   （删除登记见 `.rddf/checks/public-api-surface.md` 的「已移除 / 已改名的公开符号」章）
 *   ⇒ ⇒ **本组件现在不含任何缓冲状态** ⇒ 真 FIFO 写缓冲 = **独立立项**；
 *   届时 `ready` 才有「buffer 有空位」的**背压**语义。
 * ⚠️ **本组件没有内层 cache、也没有读通路** —— 它是一个**纯 AXI 写直通 shim**
 *   （`__io` 里**没有** `rdata` / `axi_rdata`）⇒ 读请求**不经**它。
 * ⚠️ **`axi_resp_valid`（AXI B 响应）当前不被观察** ⇒ `ready` 只表示
 *   「**W 握手成立**」，**不是**「写已提交」⇒ 消费方不得据此认为写已落盘。
 *
 * 作者：DevMate
 * 日期：2026-04-10（2026-10-28 行为订正）
 */
#pragma once

#include "ch.hpp"
#include "component.h"

namespace chlib {

using namespace ch::core;

/**
 * @brief D-Cache Write Buffer 控制器
 */
class DCacheWriteBuffer : public ch::Component {
public:
    __io(
        // CPU 写请求接口
        ch_in<ch_uint<32>> addr;          // 写地址
        ch_in<ch_uint<32>> wdata;         // 写数据
        ch_in<ch_bool> req;               // 写请求
        ch_in<ch_bool> we;                // 写使能

        // CPU 响应接口
        ch_out<ch_bool> ready;            // 本次写已被 AXI 接受（req & we & axi_ready）

        // AXI 主设备接口
        ch_out<ch_uint<32>> axi_addr;     // AXI 地址（直通，不做块对齐）
        ch_out<ch_uint<32>> axi_wdata;    // AXI 写数据
        ch_out<ch_bool> axi_we;          // AXI 写使能
        ch_out<ch_bool> axi_valid;        // AXI 请求有效（req & we，**不含** axi_ready）
        ch_in<ch_bool> axi_ready;         // AXI 就绪
        ch_in<ch_bool> axi_resp_valid;    // AXI 响应有效
    )

    DCacheWriteBuffer(ch::Component* parent = nullptr,
                      const std::string& name = "dcache_wb")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        // =====================================================================
        // Write Buffer 队列：**已删除**（2026-11-09 · phase-6 P9 · Zero-Debt）
        // =====================================================================
        // 原先的 `WriteBufferEntry buffer[4]` 是**零消费者**的死数据 ⇒ 已删
        // （删除登记见 `.rddf/checks/public-api-surface.md` 的「已移除 / 已改名的
        // 公开符号」章）⇒ ⇒ 真 FIFO 写缓冲 = **独立立项**。

        // =====================================================================
        // 写请求 → AXI 总线（**直通**）
        // =====================================================================
        // ⚠️ 每个 `ch_out` MUST 被**无条件**驱动 —— 条件 MUST 体现在表达式的
        //    **取值**里 ⇒ 条件为假时也有确定值 ⇒ **不是**「条件为真才 `<<=`」
        //    那种「条件为假又变回未驱动」的形态。
        //
        // ⚠️ `axi_valid` **不得**含 `io().axi_ready`（AXI「VALID 不得等 READY」）
        //    —— 本 wrapper **无状态/队列**可保留一次写 ⇒ 含了 READY 就等于
        //    「READY 为低时写根本不出现在总线上」⇒ **写被静默丢弃**。
        io().ready     <<= io().req & io().we & io().axi_ready;
        io().axi_valid <<= io().req & io().we;
        io().axi_addr  <<= io().addr;
        io().axi_wdata <<= io().wdata;
        io().axi_we    <<= io().we;
    }
};

} // namespace chlib
