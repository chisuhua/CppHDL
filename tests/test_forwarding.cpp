/**
 * @file test_forwarding.cpp
 * @brief RV32I 数据前推单元编译测试
 * 
 * 验证前推单元定义可以正确编译，无错误无警告
 * 
 * 测试内容:
 * 1. ForwardingUnit 组件编译和实例化
 * 2. ForwardingMux 组件编译和实例化
 * 3. 与流水线寄存器的集成验证
 */

#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "../examples/riscv-mini/src/rv32i_forwarding.h"
#include "../examples/riscv-mini/src/rv32i_pipeline_regs.h"

using namespace riscv;

// ============================================================================
// 测试用例 1: 前推检测单元 (ForwardingUnit)
// ============================================================================
TEST_CASE("ForwardingUnit Component", "[forwarding][unit]") {
    SECTION("ForwardingUnit compiles and instantiates") {
        ch::Component* parent = nullptr;
        ForwardingUnit fwd(parent, "test_fwd_unit");
        (void)fwd;
    }
    
    SECTION("ForwardingUnit IO ports are accessible") {
        ch::Component* parent = nullptr;
        ForwardingUnit fwd(parent, "test_fwd_ports");
        
        // 验证输入端口可以连接
        fwd.io().id_ex_rs1_addr = ch_uint<5>(0_d);
        fwd.io().id_ex_rs2_addr = ch_uint<5>(0_d);
        fwd.io().ex_mem_rd = ch_uint<5>(0_d);
        fwd.io().ex_mem_reg_write = ch_bool(false);
        fwd.io().mem_wb_rd = ch_uint<5>(0_d);
        fwd.io().mem_wb_reg_write = ch_bool(false);
        
        // 验证输出端口可以访问
        (void)fwd.io().forward_a;
        (void)fwd.io().forward_b;
    }
}

// ============================================================================
// 测试用例 2: 前推选择单元 (ForwardingMux)
// ============================================================================
TEST_CASE("ForwardingMux Component", "[forwarding][mux]") {
    SECTION("ForwardingMux compiles and instantiates") {
        ch::Component* parent = nullptr;
        ForwardingMux mux(parent, "test_fwd_mux");
        (void)mux;
    }
    
    SECTION("ForwardingMux IO ports are accessible") {
        ch::Component* parent = nullptr;
        ForwardingMux mux(parent, "test_fwd_mux_ports");
        
        // 验证输入端口可以连接
        mux.io().forward_a = ch_uint<2>(0_d);
        mux.io().forward_b = ch_uint<2>(0_d);
        mux.io().rs1_data_reg = ch_uint<32>(0_d);
        mux.io().rs2_data_reg = ch_uint<32>(0_d);
        mux.io().ex_mem_alu_result = ch_uint<32>(0_d);
        mux.io().mem_wb_alu_result = ch_uint<32>(0_d);
        mux.io().mem_wb_mem_read_data = ch_uint<32>(0_d);
        mux.io().mem_wb_is_load = ch_bool(false);
        
        // 验证输出端口可以访问
        (void)mux.io().alu_input_a;
        (void)mux.io().alu_input_b;
    }
}

// ============================================================================
// 测试用例 3: 前推单元与流水线寄存器集成
// ============================================================================
TEST_CASE("Forwarding Integration with Pipeline Registers", "[forwarding][integration]") {
    SECTION("ForwardingUnit connects to pipeline register fields") {
        ch::Component* parent = nullptr;
        
        // 实例化流水线寄存器
        IdExPipelineReg id_ex_reg(parent, "id_ex");
        ExMemPipelineReg ex_mem_reg(parent, "ex_mem");
        MemWbPipelineReg mem_wb_reg(parent, "mem_wb");
        
        // 实例化前推检测单元
        ForwardingUnit fwd(parent, "fwd");
        
        // 验证前推单元可以连接到流水线寄存器的字段
        // RS1/RS2 地址来自 ID/EX
        fwd.io().id_ex_rs1_addr = id_ex_reg.io().ex_rd;
        fwd.io().id_ex_rs2_addr = id_ex_reg.io().ex_rd;
        
        // 前推源来自 EX/MEM 和 MEM/WB
        fwd.io().ex_mem_rd = ex_mem_reg.io().mem_rd;
        fwd.io().ex_mem_reg_write = ex_mem_reg.io().mem_reg_write;
        fwd.io().mem_wb_rd = mem_wb_reg.io().wb_rd;
        fwd.io().mem_wb_reg_write = mem_wb_reg.io().wb_reg_write;
        
        (void)fwd;
    }
    
    SECTION("ForwardingMux connects to pipeline register fields") {
        ch::Component* parent = nullptr;
        
        // 实例化流水线寄存器
        IdExPipelineReg id_ex_reg(parent, "id_ex");
        ExMemPipelineReg ex_mem_reg(parent, "ex_mem");
        MemWbPipelineReg mem_wb_reg(parent, "mem_wb");
        
        // 实例化前推检测单元
        ForwardingUnit fwd(parent, "fwd");
        
        // 连接前推检测单元
        fwd.io().id_ex_rs1_addr = id_ex_reg.io().ex_rd;
        fwd.io().id_ex_rs2_addr = id_ex_reg.io().ex_rd;
        fwd.io().ex_mem_rd = ex_mem_reg.io().mem_rd;
        fwd.io().ex_mem_reg_write = ex_mem_reg.io().mem_reg_write;
        fwd.io().mem_wb_rd = mem_wb_reg.io().wb_rd;
        fwd.io().mem_wb_reg_write = mem_wb_reg.io().wb_reg_write;
        
        // 实例化前推选择单元
        ForwardingMux mux(parent, "mux");
        
        // 连接前推控制信号
        mux.io().forward_a = fwd.io().forward_a;
        mux.io().forward_b = fwd.io().forward_b;
        
        // 连接数据路径
        mux.io().rs1_data_reg = id_ex_reg.io().ex_rs1_data;
        mux.io().rs2_data_reg = id_ex_reg.io().ex_rs2_data;
        mux.io().ex_mem_alu_result = ex_mem_reg.io().mem_alu_result;
        mux.io().mem_wb_alu_result = mem_wb_reg.io().wb_alu_result;
        mux.io().mem_wb_mem_read_data = mem_wb_reg.io().wb_read_data;
        mux.io().mem_wb_is_load = mem_wb_reg.io().wb_is_load;
        
        // 验证输出可以访问
        (void)mux.io().alu_input_a;
        (void)mux.io().alu_input_b;
    }
    
    SECTION("Complete forwarding path compiles") {
        ch::Component* parent = nullptr;
        
        // 实例化所有流水线寄存器
        IfIdPipelineReg if_id(parent, "if_id");
        IdExPipelineReg id_ex(parent, "id_ex");
        ExMemPipelineReg ex_mem(parent, "ex_mem");
        MemWbPipelineReg mem_wb(parent, "mem_wb");
        
        // 实例化前推单元
        ForwardingUnit fwd(parent, "fwd");
        ForwardingMux mux(parent, "mux");
        
        // 连接前推检测
        fwd.io().id_ex_rs1_addr = id_ex.io().ex_rd;
        fwd.io().id_ex_rs2_addr = id_ex.io().ex_rd;
        fwd.io().ex_mem_rd = ex_mem.io().mem_rd;
        fwd.io().ex_mem_reg_write = ex_mem.io().mem_reg_write;
        fwd.io().mem_wb_rd = mem_wb.io().wb_rd;
        fwd.io().mem_wb_reg_write = mem_wb.io().wb_reg_write;
        
        // 连接前推选择
        mux.io().forward_a = fwd.io().forward_a;
        mux.io().forward_b = fwd.io().forward_b;
        mux.io().rs1_data_reg = id_ex.io().ex_rs1_data;
        mux.io().rs2_data_reg = id_ex.io().ex_rs2_data;
        mux.io().ex_mem_alu_result = ex_mem.io().mem_alu_result;
        mux.io().mem_wb_alu_result = mem_wb.io().wb_alu_result;
        mux.io().mem_wb_mem_read_data = mem_wb.io().wb_read_data;
        mux.io().mem_wb_is_load = mem_wb.io().wb_is_load;
        
        (void)if_id;
        (void)mux;
    }
}

// ============================================================================
// 测试用例 4: 前推路径验证
// ============================================================================
TEST_CASE("Forwarding Path Scenarios", "[forwarding][paths]") {
    SECTION("EX→EX forwarding path compiles") {
        // EX→EX 前推：EX/MEM 的 ALU 结果前推到 ALU 输入
        ch::Component* parent = nullptr;
        ForwardingUnit fwd(parent, "ex_ex_fwd");
        ForwardingMux mux(parent, "ex_ex_mux");
        
        // 设置 EX/MEM 前推条件
        fwd.io().id_ex_rs1_addr = ch_uint<5>(5_d);
        fwd.io().ex_mem_rd = ch_uint<5>(5_d);
        fwd.io().ex_mem_reg_write = ch_bool(true);
        
        // 连接前推控制到 MUX
        mux.io().forward_a = fwd.io().forward_a;
        mux.io().forward_b = fwd.io().forward_b;
        mux.io().rs1_data_reg = ch_uint<32>(0_d);
        mux.io().rs2_data_reg = ch_uint<32>(0_d);
        mux.io().ex_mem_alu_result = ch_uint<32>(42_d);
        mux.io().mem_wb_alu_result = ch_uint<32>(0_d);
        mux.io().mem_wb_mem_read_data = ch_uint<32>(0_d);
        mux.io().mem_wb_is_load = ch_bool(false);
        
        (void)mux;
    }
    
    SECTION("MEM→EX forwarding path compiles") {
        // MEM→EX 前推：MEM/WB 的结果前推到 ALU 输入
        ch::Component* parent = nullptr;
        ForwardingUnit fwd(parent, "mem_ex_fwd");
        ForwardingMux mux(parent, "mem_ex_mux");
        
        // EX/MEM 不冲突
        fwd.io().id_ex_rs1_addr = ch_uint<5>(5_d);
        fwd.io().ex_mem_rd = ch_uint<5>(10_d);
        fwd.io().ex_mem_reg_write = ch_bool(true);
        
        // MEM/WB 前推条件
        fwd.io().mem_wb_rd = ch_uint<5>(5_d);
        fwd.io().mem_wb_reg_write = ch_bool(true);
        
        // 连接 MUX
        mux.io().forward_a = fwd.io().forward_a;
        mux.io().forward_b = fwd.io().forward_b;
        mux.io().rs1_data_reg = ch_uint<32>(0_d);
        mux.io().rs2_data_reg = ch_uint<32>(0_d);
        mux.io().ex_mem_alu_result = ch_uint<32>(0_d);
        mux.io().mem_wb_alu_result = ch_uint<32>(123_d);
        mux.io().mem_wb_mem_read_data = ch_uint<32>(0_d);
        mux.io().mem_wb_is_load = ch_bool(false);
        
        (void)mux;
    }
    
    SECTION("LOAD forwarding path compiles") {
        // LOAD 指令前推：MEM/WB 的内存读数据前推
        ch::Component* parent = nullptr;
        ForwardingUnit fwd(parent, "load_fwd");
        ForwardingMux mux(parent, "load_mux");
        
        // 设置 LOAD 指令前推
        fwd.io().id_ex_rs1_addr = ch_uint<5>(5_d);
        fwd.io().ex_mem_rd = ch_uint<5>(10_d);
        fwd.io().ex_mem_reg_write = ch_bool(true);
        
        fwd.io().mem_wb_rd = ch_uint<5>(5_d);
        fwd.io().mem_wb_reg_write = ch_bool(true);
        
        // 连接 MUX (LOAD 指令)
        mux.io().forward_a = fwd.io().forward_a;
        mux.io().forward_b = fwd.io().forward_b;
        mux.io().rs1_data_reg = ch_uint<32>(0_d);
        mux.io().rs2_data_reg = ch_uint<32>(0_d);
        mux.io().ex_mem_alu_result = ch_uint<32>(0_d);
        mux.io().mem_wb_alu_result = ch_uint<32>(0_d);
        mux.io().mem_wb_mem_read_data = ch_uint<32>(789_d);
        mux.io().mem_wb_is_load = ch_bool(true);  // LOAD 指令
        
        (void)mux;
    }
}

// ============================================================================
// 测试用例 5: 边界条件
// ============================================================================
TEST_CASE("Forwarding Edge Cases", "[forwarding][edge]") {
    SECTION("x0 (zero register) handling compiles") {
        // x0 寄存器不应该触发前推
        ch::Component* parent = nullptr;
        ForwardingUnit fwd(parent, "x0_fwd");
        
        fwd.io().id_ex_rs1_addr = ch_uint<5>(0_d);  // x0
        fwd.io().ex_mem_rd = ch_uint<5>(0_d);       // x0
        fwd.io().ex_mem_reg_write = ch_bool(true);
        fwd.io().mem_wb_rd = ch_uint<5>(0_d);
        fwd.io().mem_wb_reg_write = ch_bool(true);
        
        (void)fwd;
    }
    
    SECTION("No reg_write handling compiles") {
        // 没有写使能时不应该前推
        ch::Component* parent = nullptr;
        ForwardingUnit fwd(parent, "no_write_fwd");
        
        fwd.io().id_ex_rs1_addr = ch_uint<5>(5_d);
        fwd.io().ex_mem_rd = ch_uint<5>(5_d);
        fwd.io().ex_mem_reg_write = ch_bool(false);  // 无写使能
        fwd.io().mem_wb_rd = ch_uint<5>(5_d);
        fwd.io().mem_wb_reg_write = ch_bool(false);  // 无写使能
        
        (void)fwd;
    }
    
    SECTION("Priority handling (EX/MEM > MEM/WB) compiles") {
        // EX/MEM 优先级高于 MEM/WB
        ch::Component* parent = nullptr;
        ForwardingUnit fwd(parent, "priority_fwd");
        
        fwd.io().id_ex_rs1_addr = ch_uint<5>(5_d);
        fwd.io().ex_mem_rd = ch_uint<5>(5_d);       // EX/MEM 匹配
        fwd.io().ex_mem_reg_write = ch_bool(true);
        fwd.io().mem_wb_rd = ch_uint<5>(5_d);       // MEM/WB 也匹配
        fwd.io().mem_wb_reg_write = ch_bool(true);
        
        (void)fwd;
    }
    
    SECTION("Dual operand forwarding compiles") {
        // 双操作数前推：RS1 和 RS2 都需要前推
        ch::Component* parent = nullptr;
        ForwardingUnit fwd(parent, "dual_fwd");
        ForwardingMux mux(parent, "dual_mux");
        
        // RS1 从 EX/MEM 前推，RS2 从 MEM/WB 前推
        fwd.io().id_ex_rs1_addr = ch_uint<5>(5_d);
        fwd.io().id_ex_rs2_addr = ch_uint<5>(6_d);
        fwd.io().ex_mem_rd = ch_uint<5>(5_d);
        fwd.io().ex_mem_reg_write = ch_bool(true);
        fwd.io().mem_wb_rd = ch_uint<5>(6_d);
        fwd.io().mem_wb_reg_write = ch_bool(true);
        
        // 连接 MUX
        mux.io().forward_a = fwd.io().forward_a;
        mux.io().forward_b = fwd.io().forward_b;
        mux.io().rs1_data_reg = ch_uint<32>(0_d);
        mux.io().rs2_data_reg = ch_uint<32>(0_d);
        mux.io().ex_mem_alu_result = ch_uint<32>(100_d);
        mux.io().mem_wb_alu_result = ch_uint<32>(50_d);
        mux.io().mem_wb_mem_read_data = ch_uint<32>(0_d);
        mux.io().mem_wb_is_load = ch_bool(false);
        
        (void)mux;
    }
}

// ============================================================================
// 测试用例 6: 编译验证 (确保所有结构体和组件可以实例化)
// ============================================================================
TEST_CASE("Forwarding Structures Compilation", "[forwarding][compile]") {
    SECTION("ForwardingUnit compiles and instantiates") {
        ch::Component* parent = nullptr;
        ForwardingUnit fwd(parent, "test_fwd_unit");
        (void)fwd;
    }
    
    SECTION("ForwardingMux compiles and instantiates") {
        ch::Component* parent = nullptr;
        ForwardingMux mux(parent, "test_fwd_mux");
        (void)mux;
    }
}
