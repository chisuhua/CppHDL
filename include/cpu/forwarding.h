#ifndef CHLIB_FORWARDING_H
#define CHLIB_FORWARDING_H

#include "ch.hpp"
#include "chlib/switch.h"
#include "core/bool.h"
#include "core/literal.h"
#include "core/operators.h"
#include "core/uint.h"

using namespace ch::core;

namespace chlib {

// ============================================================================
// 前推源选择枚举
// ============================================================================
/**
 * @brief 前推源选择编码
 * 
 * 用于 ForwardingUnit 输出和 ForwardingMux 输入的控制信号
 * 
 * 编码规则:
 * - 0: 从寄存器文件读取 (REG)
 * - 1: 从前一级 ALU 结果前推 (EX)
 * - 2: 从后一级结果前推 (MEM/WB)
 * - 3: 保留
 */
enum class ForwardSource : uint64_t {
    REG = 0,  ///< 寄存器文件
    EX = 1,   ///< EX 阶段结果 (最近)
    MEM = 2,  ///< MEM/WB 阶段结果 (较旧)
    RESERVED = 3
};

// ============================================================================
// 通用前推检测单元 (组合逻辑)
// ============================================================================
/**
 * @brief 通用前推检测单元
 * 
 * 检测数据冒险 (RAW - Read After Write) 并生成前推控制信号
 * 
 * 工作原理:
 * 1. 比较当前阶段需要的寄存器地址与后续阶段的目的寄存器地址
 * 2. 如果地址匹配且后续阶段会写回该寄存器，则产生前推信号
 * 3. 前推优先级：EX 阶段 > MEM/WB 阶段 > 寄存器文件
 * 
 * 冒险类型支持:
 * - RAW (Read After Write): 需要前推 ✅
 * - WAW (Write After Write): 顺序执行不会发生
 * - WAR (Write After Read): 顺序执行不会发生
 * 
 * @tparam REG_ADDR_WIDTH 寄存器地址位宽 (例如：5 位支持 32 个寄存器)
 * @tparam NUM_STAGES 前推级数 (2=EX+MEM, 3=EX+MEM+WB)
 * 
 * 使用示例:
 * @code
 * // 创建 5 位地址、2 级前推的 ForwardingUnit
 * ForwardingUnit<5, 2> fwd_unit;
 * 
 * // 设置输入
 * sim.set_value(fwd_unit.io().rs1_addr, 5);
 * sim.set_value(fwd_unit.io().rs2_addr, 6);
 * sim.set_value(fwd_unit.io().ex_rd, 5);
 * sim.set_value(fwd_unit.io().ex_reg_write, 1);
 * sim.set_value(fwd_unit.io().mem_rd, 7);
 * sim.set_value(fwd_unit.io().mem_reg_write, 1);
 * 
 * // 获取前推控制信号
 * auto forward_a = sim.get_value(fwd_unit.io().forward_a);
 * auto forward_b = sim.get_value(fwd_unit.io().forward_b);
 * @endcode
 */
template <unsigned REG_ADDR_WIDTH, unsigned NUM_STAGES = 2>
class ForwardingUnit : public ch::Component {
    static_assert(REG_ADDR_WIDTH > 0, "寄存器地址位宽必须大于 0");
    static_assert(NUM_STAGES >= 2 && NUM_STAGES <= 3, "前推级数必须为 2 或 3");

public:
    __io(
        // 当前阶段需要读取的寄存器地址
        ch_in<ch_uint<REG_ADDR_WIDTH>> rs1_addr;   ///< RS1 寄存器地址
        ch_in<ch_uint<REG_ADDR_WIDTH>> rs2_addr;   ///< RS2 寄存器地址
        
        // EX 阶段信息 (最近的前推源)
        ch_in<ch_uint<REG_ADDR_WIDTH>> ex_rd;          ///< EX 阶段目的寄存器地址
        ch_in<ch_bool>                 ex_reg_write;   ///< EX 阶段寄存器写使能
        
        // MEM/WB 阶段信息 (较旧的前推源)
        ch_in<ch_uint<REG_ADDR_WIDTH>> mem_rd;         ///< MEM/WB 阶段目的寄存器地址
        ch_in<ch_bool>                 mem_reg_write;  ///< MEM/WB 阶段寄存器写使能
        
        // 前推控制输出
        ch_out<ch_uint<2>> forward_a;  ///< RS1 的前推源选择 (0=REG, 1=EX, 2=MEM)
        ch_out<ch_uint<2>> forward_b   ///< RS2 的前推源选择 (0=REG, 1=EX, 2=MEM)
    )

    ForwardingUnit(ch::Component* parent = nullptr, const std::string& name = "forwarding_unit")
        : ch::Component(parent, name) {}

    void create_ports() override {
        new (io_storage_) io_type;
    }

    void describe() override {
        // 检查零寄存器 (RISC-V x0 恒为 0，不需要前推)
        auto ex_rd_nonzero = io().ex_rd != ch_uint<REG_ADDR_WIDTH>(0_d);
        auto mem_rd_nonzero = io().mem_rd != ch_uint<REG_ADDR_WIDTH>(0_d);

        // EX 阶段前推条件：reg_write & (rd != 0)
        auto ex_forward_enable = io().ex_reg_write & ex_rd_nonzero;

        // MEM/WB 阶段前推条件：reg_write & (rd != 0)
        auto mem_forward_enable = io().mem_reg_write & mem_rd_nonzero;

        // =========================================================================
        // RS1 前推检测逻辑
        // =========================================================================
        auto ex_addr_match_rs1 = io().ex_rd == io().rs1_addr;
        auto ex_forward_a = ex_forward_enable & ex_addr_match_rs1;

        auto mem_addr_match_rs1 = io().mem_rd == io().rs1_addr;
        auto mem_forward_a = mem_forward_enable & mem_addr_match_rs1;

        // RS1 前推源选择 (优先级：EX > MEM > REG)
        io().forward_a = select(ex_forward_a,
                                ch_uint<2>(static_cast<uint64_t>(ForwardSource::EX)),
                                select(mem_forward_a,
                                       ch_uint<2>(static_cast<uint64_t>(ForwardSource::MEM)),
                                       ch_uint<2>(static_cast<uint64_t>(ForwardSource::REG))));

        // =========================================================================
        // RS2 前推检测逻辑
        // =========================================================================
        auto ex_addr_match_rs2 = io().ex_rd == io().rs2_addr;
        auto ex_forward_b = ex_forward_enable & ex_addr_match_rs2;

        auto mem_addr_match_rs2 = io().mem_rd == io().rs2_addr;
        auto mem_forward_b = mem_forward_enable & mem_addr_match_rs2;

        // RS2 前推源选择 (优先级：EX > MEM > REG)
        io().forward_b = select(ex_forward_b,
                                ch_uint<2>(static_cast<uint64_t>(ForwardSource::EX)),
                                select(mem_forward_b,
                                       ch_uint<2>(static_cast<uint64_t>(ForwardSource::MEM)),
                                       ch_uint<2>(static_cast<uint64_t>(ForwardSource::REG))));
    }
};

// ============================================================================
// 通用前推数据选择器 (多路复用器)
// ============================================================================
/**
 * @brief 通用前推数据选择单元
 * 
 * 根据 ForwardingUnit 生成的控制信号，选择正确的数据源
 * 
 * 数据源:
 * - REG: 从寄存器文件读取的原始数据
 * - EX: EX 阶段的 ALU 结果 (最近的前推源)
 * - MEM: MEM/WB 阶段的结果 (较旧的前推源)
 * 
 * @tparam DATA_WIDTH 数据位宽 (例如：32 位用于 RV32I)
 * 
 * 使用示例:
 * @code
 * // 创建 32 位数据的 ForwardingMux
 * ForwardingMux<32> fwd_mux;
 * 
 * // 设置输入
 * sim.set_value(fwd_mux.io().rs1_data_reg, 0x12345678);
 * sim.set_value(fwd_mux.io().rs2_data_reg, 0x87654321);
 * sim.set_value(fwd_mux.io().ex_mem_alu_result, 0xDEADBEEF);
 * sim.set_value(fwd_mux.io().mem_wb_result, 0xCAFEBABE);
 * sim.set_value(fwd_mux.io().forward_a, 1);  // RS1 选择 EX 前推
 * sim.set_value(fwd_mux.io().forward_b, 0);  // RS2 选择寄存器
 * 
 * // 获取前推后的 ALU 输入
 * auto alu_a = sim.get_value(fwd_mux.io().alu_input_a);
 * auto alu_b = sim.get_value(fwd_mux.io().alu_input_b);
 * @endcode
 */
template <unsigned DATA_WIDTH>
class ForwardingMux : public ch::Component {
    static_assert(DATA_WIDTH > 0, "数据位宽必须大于 0");

public:
    __io(
        // 前推控制信号 (来自 ForwardingUnit)
        ch_in<ch_uint<2>> forward_a;
        ch_in<ch_uint<2>> forward_b;

        // 寄存器文件输出 (原始数据)
        ch_in<ch_uint<DATA_WIDTH>> rs1_data_reg;
        ch_in<ch_uint<DATA_WIDTH>> rs2_data_reg;

        // EX 阶段数据 (前推源 1 - 最近)
        ch_in<ch_uint<DATA_WIDTH>> ex_mem_alu_result;

        // MEM/WB 阶段数据 (前推源 2 - 较旧)
        ch_in<ch_uint<DATA_WIDTH>> mem_wb_result;

        // ALU 输入输出 (前推后的数据)
        ch_out<ch_uint<DATA_WIDTH>> alu_input_a;
        ch_out<ch_uint<DATA_WIDTH>> alu_input_b
    )

    ForwardingMux(ch::Component* parent = nullptr, const std::string& name = "forwarding_mux")
        : ch::Component(parent, name) {}

    void create_ports() override {
        new (io_storage_) io_type;
    }

    void describe() override {
        // =========================================================================
        // ALU 输入 A 选择 (RS1 路径)
        // forward_a = 0: 寄存器文件
        // forward_a = 1: EX/MEM ALU 结果
        // forward_a = 2: MEM/WB 结果
        // =========================================================================
        auto sel_a_is_reg = io().forward_a == ch_uint<2>(static_cast<uint64_t>(ForwardSource::REG));
        auto sel_a_is_ex = io().forward_a == ch_uint<2>(static_cast<uint64_t>(ForwardSource::EX));

        io().alu_input_a = select(sel_a_is_reg,
                                  io().rs1_data_reg,
                                  select(sel_a_is_ex,
                                         io().ex_mem_alu_result,
                                         io().mem_wb_result));

        // =========================================================================
        // ALU 输入 B 选择 (RS2 路径)
        // forward_b = 0: 寄存器文件
        // forward_b = 1: EX/MEM ALU 结果
        // forward_b = 2: MEM/WB 结果
        // =========================================================================
        auto sel_b_is_reg = io().forward_b == ch_uint<2>(static_cast<uint64_t>(ForwardSource::REG));
        auto sel_b_is_ex = io().forward_b == ch_uint<2>(static_cast<uint64_t>(ForwardSource::EX));

        io().alu_input_b = select(sel_b_is_reg,
                                  io().rs2_data_reg,
                                  select(sel_b_is_ex,
                                         io().ex_mem_alu_result,
                                         io().mem_wb_result));
    }
};

// ============================================================================
// 简化的函数式接口 (用于组合逻辑仿真)
// ============================================================================
/**
 * @brief 前推检测结果结构
 */
struct ForwardResult {
    ch_uint<2> forward_a;  ///< RS1 的前推源选择
    ch_uint<2> forward_b;  ///< RS2 的前推源选择
};

/**
 * @brief 前推检测函数式接口
 * 
 * 用于仿真和测试的组合逻辑函数
 * 
 * @tparam REG_ADDR_WIDTH 寄存器地址位宽
 * 
 * @param rs1_addr RS1 寄存器地址
 * @param rs2_addr RS2 寄存器地址
 * @param ex_rd EX 阶段目的寄存器地址
 * @param ex_reg_write EX 阶段写使能
 * @param mem_rd MEM/WB 阶段目的寄存器地址
 * @param mem_reg_write MEM/WB 阶段写使能
 * @return ForwardResult 前推选择结果
 */
template <unsigned REG_ADDR_WIDTH>
ForwardResult
forward_detect(ch_uint<REG_ADDR_WIDTH> rs1_addr, ch_uint<REG_ADDR_WIDTH> rs2_addr,
               ch_uint<REG_ADDR_WIDTH> ex_rd, ch_bool ex_reg_write,
               ch_uint<REG_ADDR_WIDTH> mem_rd, ch_bool mem_reg_write) {
    ForwardResult result;

    auto ex_rd_nonzero = ex_rd != ch_uint<REG_ADDR_WIDTH>(0_d);
    auto mem_rd_nonzero = mem_rd != ch_uint<REG_ADDR_WIDTH>(0_d);
    auto ex_enable = ex_reg_write & ex_rd_nonzero;
    auto mem_enable = mem_reg_write & mem_rd_nonzero;

    auto ex_match_rs1 = ex_rd == rs1_addr;
    auto mem_match_rs1 = mem_rd == rs1_addr;
    result.forward_a = select(ex_enable & ex_match_rs1,
                              ch_uint<2>(1_d),
                              select(mem_enable & mem_match_rs1,
                                     ch_uint<2>(2_d),
                                     ch_uint<2>(0_d)));

    auto ex_match_rs2 = ex_rd == rs2_addr;
    auto mem_match_rs2 = mem_rd == rs2_addr;
    result.forward_b = select(ex_enable & ex_match_rs2,
                              ch_uint<2>(1_d),
                              select(mem_enable & mem_match_rs2,
                                     ch_uint<2>(2_d),
                                     ch_uint<2>(0_d)));

    return result;
}

/**
 * @brief 前推数据选择函数式接口
 * 
 * 用于仿真和测试的组合逻辑函数
 * 
 * @tparam DATA_WIDTH 数据位宽
 * 
 * @param forward_a RS1 前推选择信号
 * @param forward_b RS2 前推选择信号
 * @param rs1_data_reg RS1 寄存器数据
 * @param rs2_data_reg RS2 寄存器数据
 * @param ex_result EX 阶段结果
 * @param mem_result MEM/WB 阶段结果
 * @return 包含前推后数据的结构
 */
template <unsigned DATA_WIDTH>
struct ForwardMuxResult {
    ch_uint<DATA_WIDTH> alu_input_a;
    ch_uint<DATA_WIDTH> alu_input_b;
};

template <unsigned DATA_WIDTH>
ForwardMuxResult<DATA_WIDTH>
forward_select(ch_uint<2> forward_a, ch_uint<2> forward_b,
               ch_uint<DATA_WIDTH> rs1_data_reg, ch_uint<DATA_WIDTH> rs2_data_reg,
               ch_uint<DATA_WIDTH> ex_result, ch_uint<DATA_WIDTH> mem_result) {
    ForwardMuxResult<DATA_WIDTH> result;

    auto sel_a_is_reg = forward_a == ch_uint<2>(0_d);
    auto sel_a_is_ex = forward_a == ch_uint<2>(1_d);
    result.alu_input_a = select(sel_a_is_reg, rs1_data_reg,
                                select(sel_a_is_ex, ex_result, mem_result));

    auto sel_b_is_reg = forward_b == ch_uint<2>(0_d);
    auto sel_b_is_ex = forward_b == ch_uint<2>(1_d);
    result.alu_input_b = select(sel_b_is_reg, rs2_data_reg,
                                select(sel_b_is_ex, ex_result, mem_result));

    return result;
}

} // namespace chlib

#endif // CHLIB_FORWARDING_H
