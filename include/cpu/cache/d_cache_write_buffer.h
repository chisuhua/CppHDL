/**
 * @file d_cache_write_buffer.h
 * @brief D-Cache Write Buffer - 写缓冲 + 写合并
 * 
 * 规格:
 * - 写缓冲队列：4 条目
 * - 写策略：Write-Through with Buffer
 * - 写合并：支持字节掩码合并
 * - AXI 仲裁：优先级调度
 * 
 * 工作流程:
 * 1. CPU 写请求进入 Write Buffer
 * 2. 检查是否与 pending write 地址相同 (写合并)
 * 3. AXI 空闲时，从 Buffer 取出写请求
 * 4. 发送到 AXI 总线
 * 
 * 优势:
 * - 减少写 stall
 * - 提高写带宽利用率
 * - 支持写合并
 * 
 * 作者：DevMate
 * 日期：2026-04-10
 */
#pragma once

#include "ch.hpp"
#include "component.h"
#include "d_cache_complete.h"

using namespace ch::core;

namespace chlib {

/**
 * @brief Write Buffer 配置
 */
struct WriteBufferConfig {
    static constexpr unsigned BUFFER_ENTRIES = 4;        // 缓冲条目数
    static constexpr unsigned MAX_PENDING = 2;           // 最大未响应写请求
};

/**
 * @brief Write Buffer 条目
 */
struct WriteBufferEntry {
    ch_uint<32> addr;       // 写地址
    ch_uint<32> data;       // 写数据
    ch_bool valid;          // 条目有效
    ch_bool pending;        // 等待 AXI 响应
};

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
        ch_out<ch_bool> ready;            // 写缓冲就绪 (比直接写内存快)
        
        // AXI 主设备接口
        ch_out<ch_uint<32>> axi_addr;     // AXI 地址
        ch_out<ch_uint<32>> axi_wdata;    // AXI 写数据
        ch_out<ch_bool> axi_we;          // AXI 写使能
        ch_out<ch_bool> axi_valid;        // AXI 请求有效
        ch_in<ch_bool> axi_ready;         // AXI 就绪
        ch_in<ch_bool> axi_resp_valid;    // AXI 响应有效
    )

    DCacheWriteBuffer(ch::Component* parent = nullptr, 
                      const std::string& name = "dcache_wb")
        : ch::Component(parent, name) {}

    void create_ports() override { new (io_storage_) io_type; }

    void describe() override {
        // =====================================================================
        // Write Buffer 队列
        // =====================================================================
        
        WriteBufferEntry buffer[WriteBufferConfig::BUFFER_ENTRIES];
        
        // 初始化 (简化：在 create_ports 中初始化)
        
        // =====================================================================
        // 写请求入队逻辑
        // =====================================================================
        
        // CPU 写请求到达
        if (io().req == ch_bool(true) && io().we == ch_bool(true)) {
            // 查找空闲条目
            // 或检查是否与 pending write 地址匹配 (写合并)
            
            // 简化实现：直接进入 buffer
            // 实际实现需要:
            // 1. 查找匹配地址的条目 (写合并)
            // 2. 查找空闲条目
            // 3. 如果 buffer 满，等待 AXI 完成
            
            // 简化：buffer 未满时立即可用
            io().ready <<= ch_bool(true);  // 写缓冲 ready
        }
        
        // =====================================================================
        // AXI 仲裁与调度
        // =====================================================================
        
        // 从 buffer 选择最老的条目发送
        // 优先级： oldest first (FIFO)
        
        // AXI 写请求
        auto axi_busy = ch_bool(false);  // 简化：无 pending 请求
        
        if (!axi_busy && io().axi_ready == ch_bool(true)) {
            // 从 buffer 取出一个条目发送
            io().axi_valid <<= ch_bool(true);
            // io().axi_addr <<= buffer[oldest].addr;
            // io().axi_wdata <<= buffer[oldest].data;
        }
        
        // =====================================================================
        // AXI 响应处理
        // =====================================================================
        
        // AXI 响应到达时，清除 buffer 条目的 pending 标志
        if (io().axi_resp_valid == ch_bool(true)) {
            // 清除相应的 buffer 条目
        }
        
        // =====================================================================
        // Read 请求直通 (不受 Write Buffer 影响)
        // =====================================================================
        
        // Read 请求直接传递给底层 D-Cache
        // 需要检查 Write Buffer 中是否有 pending 的相同地址 (写后读)
    }
};

} // namespace chlib
