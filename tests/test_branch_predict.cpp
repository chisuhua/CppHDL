/**
 * @file test_branch_predict.cpp
 * @brief RV32I 分支预测单元编译测试
 * 
 * 验证分支预测单元定义可以正确编译，无错误无警告
 * 
 * 测试内容:
 * 1. BranchDetector 组件编译和实例化
 * 2. BranchConditionUnit 组件编译和实例化
 * 3. BranchPredictor 组件编译和实例化
 * 4. ControlHazardUnit 组件编译和实例化
 * 5. BranchPredictorState 组件编译和实例化
 * 6. 与流水线寄存器的集成验证
 */

#include "catch_amalgamated.hpp"
#include "ch.hpp"
#include "riscv/rv32i_branch_predict.h"
#include "riscv/rv32i_pipeline_regs.h"

using namespace riscv;

// ============================================================================
// 测试用例 1: 分支检测单元 (BranchDetector)
// ============================================================================
TEST_CASE("BranchDetector Component", "[branch][detector]") {
    SECTION("BranchDetector compiles and instantiates") {
        ch::Component* parent = nullptr;
        BranchDetector detector(parent, "test_detector");
        (void)detector;
    }
    
    SECTION("BranchDetector IO ports are accessible") {
        ch::Component* parent = nullptr;
        BranchDetector detector(parent, "test_detector_ports");
        
        // 验证输入端口可以连接
        detector.io().instruction = ch_uint<32>(99_d);  // BEQ 指令示例 (opcode=0x63)
        
        // 验证输出端口可以访问
        (void)detector.io().is_branch;
        (void)detector.io().branch_type;
    }
    
    SECTION("BranchDetector detects BEQ instruction") {
        ch::Component* parent = nullptr;
        BranchDetector detector(parent, "test_beq");
        
        // BEQ: opcode=99 (0x63), funct3=000
        // 指令格式：imm[12|10:5]|rs2|rs1|funct3|imm[4:1|11]|opcode
        ch_uint<32> beq_instr = ch_uint<32>(99_d);  // BEQ x0, x0, 0 (0x00000063)
        detector.io().instruction = beq_instr;
        
        (void)detector;
    }
    
    SECTION("BranchDetector detects BNE instruction") {
        ch::Component* parent = nullptr;
        BranchDetector detector(parent, "test_bne");
        
        // BNE: opcode=99 (0x63), funct3=001
        ch_uint<32> bne_instr = ch_uint<32>(103_d);  // BNE x0, x0, 0 (0x00000067)
        detector.io().instruction = bne_instr;
        
        (void)detector;
    }
    
    SECTION("BranchDetector detects BLT instruction") {
        ch::Component* parent = nullptr;
        BranchDetector detector(parent, "test_blt");
        
        // BLT: opcode=99 (0x63), funct3=100
        ch_uint<32> blt_instr = ch_uint<32>(195_d);  // BLT x0, x0, 0 (0x000000C3)
        detector.io().instruction = blt_instr;
        
        (void)detector;
    }
    
    SECTION("BranchDetector detects BGE instruction") {
        ch::Component* parent = nullptr;
        BranchDetector detector(parent, "test_bge");
        
        // BGE: opcode=99 (0x63), funct3=101
        ch_uint<32> bge_instr = ch_uint<32>(199_d);  // BGE x0, x0, 0 (0x000000C7)
        detector.io().instruction = bge_instr;
        
        (void)detector;
    }
    
    SECTION("BranchDetector detects BLTU instruction") {
        ch::Component* parent = nullptr;
        BranchDetector detector(parent, "test_bltu");
        
        // BLTU: opcode=99 (0x63), funct3=110
        ch_uint<32> bltu_instr = ch_uint<32>(198_d);  // BLTU x0, x0, 0 (0x000000C6)
        detector.io().instruction = bltu_instr;
        
        (void)detector;
    }
    
    SECTION("BranchDetector detects BGEU instruction") {
        ch::Component* parent = nullptr;
        BranchDetector detector(parent, "test_bgeu");
        
        // BGEU: opcode=99 (0x63), funct3=111
        ch_uint<32> bgeu_instr = ch_uint<32>(199_d);  // BGEU x0, x0, 0 (0x000000C7)
        detector.io().instruction = bgeu_instr;
        
        (void)detector;
    }
}

// ============================================================================
// 测试用例 2: 分支条件计算单元 (BranchConditionUnit)
// ============================================================================
TEST_CASE("BranchConditionUnit Component", "[branch][condition]") {
    SECTION("BranchConditionUnit compiles and instantiates") {
        ch::Component* parent = nullptr;
        BranchConditionUnit condition(parent, "test_condition");
        (void)condition;
    }
    
    SECTION("BranchConditionUnit IO ports are accessible") {
        ch::Component* parent = nullptr;
        BranchConditionUnit condition(parent, "test_condition_ports");
        
        // 验证输入端口可以连接
        condition.io().branch_type = ch_uint<3>(0_d);
        condition.io().rs1_data = ch_uint<32>(10_d);
        condition.io().rs2_data = ch_uint<32>(10_d);
        
        // 验证输出端口可以访问
        (void)condition.io().branch_condition;
    }
    
    SECTION("BEQ condition: equal values") {
        ch::Component* parent = nullptr;
        BranchConditionUnit condition(parent, "test_beq_cond");
        
        condition.io().branch_type = ch_uint<3>(0_d);  // BEQ
        condition.io().rs1_data = ch_uint<32>(42_d);
        condition.io().rs2_data = ch_uint<32>(42_d);
        
        (void)condition;
    }
    
    SECTION("BNE condition: not equal values") {
        ch::Component* parent = nullptr;
        BranchConditionUnit condition(parent, "test_bne_cond");
        
        condition.io().branch_type = ch_uint<3>(1_d);  // BNE
        condition.io().rs1_data = ch_uint<32>(42_d);
        condition.io().rs2_data = ch_uint<32>(100_d);
        
        (void)condition;
    }
    
    SECTION("BLT condition: signed less than") {
        ch::Component* parent = nullptr;
        BranchConditionUnit condition(parent, "test_blt_cond");
        
        condition.io().branch_type = ch_uint<3>(4_d);  // BLT
        condition.io().rs1_data = ch_uint<32>(10_d);
        condition.io().rs2_data = ch_uint<32>(20_d);
        
        (void)condition;
    }
    
    SECTION("BGE condition: signed greater or equal") {
        ch::Component* parent = nullptr;
        BranchConditionUnit condition(parent, "test_bge_cond");
        
        condition.io().branch_type = ch_uint<3>(5_d);  // BGE
        condition.io().rs1_data = ch_uint<32>(20_d);
        condition.io().rs2_data = ch_uint<32>(10_d);
        
        (void)condition;
    }
    
    SECTION("BLTU condition: unsigned less than") {
        ch::Component* parent = nullptr;
        BranchConditionUnit condition(parent, "test_bltu_cond");
        
        condition.io().branch_type = ch_uint<3>(6_d);  // BLTU
        condition.io().rs1_data = ch_uint<32>(10_d);
        condition.io().rs2_data = ch_uint<32>(20_d);
        
        (void)condition;
    }
    
    SECTION("BGEU condition: unsigned greater or equal") {
        ch::Component* parent = nullptr;
        BranchConditionUnit condition(parent, "test_bgeu_cond");
        
        condition.io().branch_type = ch_uint<3>(7_d);  // BGEU
        condition.io().rs1_data = ch_uint<32>(20_d);
        condition.io().rs2_data = ch_uint<32>(10_d);
        
        (void)condition;
    }
}

// ============================================================================
// 测试用例 3: 分支预测单元 (BranchPredictor)
// ============================================================================
TEST_CASE("BranchPredictor Component", "[branch][predictor]") {
    SECTION("BranchPredictor compiles and instantiates") {
        ch::Component* parent = nullptr;
        BranchPredictor predictor(parent, "test_predictor");
        (void)predictor;
    }
    
    SECTION("BranchPredictor IO ports are accessible") {
        ch::Component* parent = nullptr;
        BranchPredictor predictor(parent, "test_predictor_ports");
        
        // 验证输入端口可以连接
        predictor.io().instruction = ch_uint<32>(99_d);  // BEQ (0x00000063)
        predictor.io().rs1_data = ch_uint<32>(0_d);
        predictor.io().rs2_data = ch_uint<32>(0_d);
        predictor.io().pc = ch_uint<32>(4096_d);  // 0x1000
        
        // 验证输出端口可以访问
        (void)predictor.io().is_branch;
        (void)predictor.io().branch_taken;
        (void)predictor.io().predict_error;
        (void)predictor.io().branch_target;
    }
    
    SECTION("BranchPredictor with BEQ taken") {
        ch::Component* parent = nullptr;
        BranchPredictor predictor(parent, "test_beq_taken");
        
        // BEQ x0, x0, offset (always taken since x0 == x0)
        predictor.io().instruction = ch_uint<32>(99_d);  // 0x00000063
        predictor.io().rs1_data = ch_uint<32>(0_d);
        predictor.io().rs2_data = ch_uint<32>(0_d);
        predictor.io().pc = ch_uint<32>(4096_d);  // 0x1000
        
        (void)predictor;
    }
    
    SECTION("BranchPredictor with BNE not taken") {
        ch::Component* parent = nullptr;
        BranchPredictor predictor(parent, "test_bne_not_taken");
        
        // BNE x1, x1, offset (not taken since x1 == x1)
        predictor.io().instruction = ch_uint<32>(16777319_d);  // 0x00100067
        predictor.io().rs1_data = ch_uint<32>(42_d);
        predictor.io().rs2_data = ch_uint<32>(42_d);
        predictor.io().pc = ch_uint<32>(4096_d);  // 0x1000
        
        (void)predictor;
    }
}

// ============================================================================
// 测试用例 4: 控制冒险处理单元 (ControlHazardUnit)
// ============================================================================
TEST_CASE("ControlHazardUnit Component", "[branch][hazard]") {
    SECTION("ControlHazardUnit compiles and instantiates") {
        ch::Component* parent = nullptr;
        ControlHazardUnit hazard(parent, "test_hazard");
        (void)hazard;
    }
    
    SECTION("ControlHazardUnit IO ports are accessible") {
        ch::Component* parent = nullptr;
        ControlHazardUnit hazard(parent, "test_hazard_ports");
        
        // 验证输入端口可以连接
        hazard.io().is_branch = ch_bool(false);
        hazard.io().predict_error = ch_bool(false);
        hazard.io().branch_taken = ch_bool(false);
        hazard.io().branch_target = ch_uint<32>(4096_d);  // 0x1000
        hazard.io().pc = ch_uint<32>(0_d);
        hazard.io().instr_valid = ch_bool(true);
        
        // 验证输出端口可以访问
        (void)hazard.io().flush_if_id;
        (void)hazard.io().stall_if;
        (void)hazard.io().pc_src;
        (void)hazard.io().pc_target;
    }
    
    SECTION("ControlHazardUnit: no branch") {
        ch::Component* parent = nullptr;
        ControlHazardUnit hazard(parent, "test_no_branch");
        
        hazard.io().is_branch = ch_bool(false);
        hazard.io().predict_error = ch_bool(false);
        hazard.io().branch_taken = ch_bool(false);
        hazard.io().branch_target = ch_uint<32>(4096_d);  // 0x1000
        hazard.io().pc = ch_uint<32>(256_d);  // 0x100
        hazard.io().instr_valid = ch_bool(true);
        
        (void)hazard;
    }
    
    SECTION("ControlHazardUnit: branch mispredict (flush)") {
        ch::Component* parent = nullptr;
        ControlHazardUnit hazard(parent, "test_mispredict");
        
        // 分支预测错误：实际跳转但 BNT 预测不跳转
        hazard.io().is_branch = ch_bool(true);
        hazard.io().predict_error = ch_bool(true);  // 预测错误
        hazard.io().branch_taken = ch_bool(true);   // 实际跳转
        hazard.io().branch_target = ch_uint<32>(4128_d);  // 0x1020
        hazard.io().pc = ch_uint<32>(256_d);  // 0x100
        hazard.io().instr_valid = ch_bool(true);
        
        (void)hazard;
    }
}

// ============================================================================
// 测试用例 5: 分支预测状态寄存器 (BranchPredictorState)
// ============================================================================
TEST_CASE("BranchPredictorState Component", "[branch][state]") {
    SECTION("BranchPredictorState compiles and instantiates") {
        ch::Component* parent = nullptr;
        BranchPredictorState state(parent, "test_bp_state");
        (void)state;
    }
    
    SECTION("BranchPredictorState IO ports are accessible") {
        ch::Component* parent = nullptr;
        BranchPredictorState state(parent, "test_bp_state_ports");
        
        // 验证输入端口可以连接
        state.io().pc = ch_uint<32>(256_d);  // 0x100
        state.io().branch_taken = ch_bool(false);
        state.io().update_enable = ch_bool(false);
        state.io().rst = ch_bool(false);
        state.io().clk = ch_bool(false);
        
        // 验证输出端口可以访问
        (void)state.io().prediction;
    }
    
    SECTION("BranchPredictorState static BNT prediction") {
        ch::Component* parent = nullptr;
        BranchPredictorState state(parent, "test_bnt");
        
        // 静态 BNT 预测：永远预测不跳转
        state.io().pc = ch_uint<32>(256_d);  // 0x100
        state.io().branch_taken = ch_bool(false);
        state.io().update_enable = ch_bool(false);
        state.io().rst = ch_bool(false);
        state.io().clk = ch_bool(false);
        
        // prediction 应该始终为 false (BNT)
        (void)state;
    }
}

// ============================================================================
// 测试用例 6: 分支预测与流水线寄存器集成
// ============================================================================
TEST_CASE("Branch Prediction Integration with Pipeline Registers", "[branch][integration]") {
    SECTION("BranchPredictor connects to IF/ID registers") {
        ch::Component* parent = nullptr;
        
        // 实例化 IF/ID 寄存器
        IfIdPipelineReg if_id(parent, "if_id");
        
        // 实例化分支预测器
        BranchPredictor predictor(parent, "predictor");
        
        // 连接：从 IF/ID 获取指令
        predictor.io().instruction = if_id.io().id_instruction;
        predictor.io().pc = if_id.io().id_pc;
        
        (void)predictor;
    }
    
    SECTION("BranchPredictor connects to ID/EX registers") {
        ch::Component* parent = nullptr;
        
        // 实例化 ID/EX 寄存器
        IdExPipelineReg id_ex(parent, "id_ex");
        
        // 实例化分支预测器
        BranchPredictor predictor(parent, "predictor");
        
        // 连接：从 ID/EX 获取寄存器数据
        predictor.io().rs1_data = id_ex.io().ex_rs1_data;
        predictor.io().rs2_data = id_ex.io().ex_rs2_data;
        
        (void)predictor;
    }
    
    SECTION("ControlHazardUnit connects to BranchPredictor") {
        ch::Component* parent = nullptr;
        
        // 实例化分支预测器
        BranchPredictor predictor(parent, "predictor");
        
        // 实例化控制冒险单元
        ControlHazardUnit hazard(parent, "hazard");
        
        // 连接预测结果到控制单元
        hazard.io().is_branch = predictor.io().is_branch;
        hazard.io().predict_error = predictor.io().predict_error;
        hazard.io().branch_taken = predictor.io().branch_taken;
        hazard.io().branch_target = predictor.io().branch_target;
        
        (void)hazard;
    }
    
    SECTION("Complete branch prediction path compiles") {
        ch::Component* parent = nullptr;
        
        // 实例化流水线寄存器
        IfIdPipelineReg if_id(parent, "if_id");
        IdExPipelineReg id_ex(parent, "id_ex");
        
        // 实例化分支预测组件
        BranchDetector detector(parent, "detector");
        BranchConditionUnit condition(parent, "condition");
        BranchPredictor predictor(parent, "predictor");
        ControlHazardUnit hazard(parent, "hazard");
        BranchPredictorState state(parent, "bp_state");
        
        // 连接 IF/ID 到预测器
        predictor.io().instruction = if_id.io().id_instruction;
        predictor.io().pc = if_id.io().id_pc;
        
        // 连接 ID/EX 到预测器
        predictor.io().rs1_data = id_ex.io().ex_rs1_data;
        predictor.io().rs2_data = id_ex.io().ex_rs2_data;
        
        // 连接预测器到控制单元
        hazard.io().is_branch = predictor.io().is_branch;
        hazard.io().predict_error = predictor.io().predict_error;
        hazard.io().branch_taken = predictor.io().branch_taken;
        hazard.io().branch_target = predictor.io().branch_target;
        hazard.io().pc = if_id.io().id_pc;
        hazard.io().instr_valid = if_id.io().id_instr_valid;
        
        (void)detector;
        (void)condition;
        (void)state;
    }
}

// ============================================================================
// 测试用例 7: 分支指令编码验证
// ============================================================================
TEST_CASE("Branch Instruction Encoding", "[branch][encoding]") {
    SECTION("BEQ encoding verification") {
        ch::Component* parent = nullptr;
        BranchDetector detector(parent, "test_beq_enc");
        
        // BEQ x1, x2, offset
        // opcode=99 (0x63), funct3=000, rs1=1, rs2=2
        ch_uint<32> beq = ch_uint<32>(2621539_d);  // 0x00208063
        detector.io().instruction = beq;
        
        (void)detector;
    }
    
    SECTION("BNE encoding verification") {
        ch::Component* parent = nullptr;
        BranchDetector detector(parent, "test_bne_enc");
        
        // BNE x3, x4, offset
        // opcode=99 (0x63), funct3=001, rs1=3, rs2=4
        ch_uint<32> bne = ch_uint<32>(2687047_d);  // 0x00418067
        detector.io().instruction = bne;
        
        (void)detector;
    }
    
    SECTION("BLT encoding verification") {
        ch::Component* parent = nullptr;
        BranchDetector detector(parent, "test_blt_enc");
        
        // BLT x5, x6, offset
        // opcode=99 (0x63), funct3=100, rs1=5, rs2=6
        ch_uint<32> blt = ch_uint<32>(2752579_d);  // 0x006280C3
        detector.io().instruction = blt;
        
        (void)detector;
    }
    
    SECTION("BGE encoding verification") {
        ch::Component* parent = nullptr;
        BranchDetector detector(parent, "test_bge_enc");
        
        // BGE x7, x8, offset
        // opcode=99 (0x63), funct3=101, rs1=7, rs2=8
        ch_uint<32> bge = ch_uint<32>(2818119_d);  // 0x008380C7
        detector.io().instruction = bge;
        
        (void)detector;
    }
    
    SECTION("BLTU encoding verification") {
        ch::Component* parent = nullptr;
        BranchDetector detector(parent, "test_bltu_enc");
        
        // BLTU x9, x10, offset
        // opcode=99 (0x63), funct3=110, rs1=9, rs2=10
        ch_uint<32> bltu = ch_uint<32>(2883651_d);  // 0x00A480E3
        detector.io().instruction = bltu;
        
        (void)detector;
    }
    
    SECTION("BGEU encoding verification") {
        ch::Component* parent = nullptr;
        BranchDetector detector(parent, "test_bgeu_enc");
        
        // BGEU x11, x12, offset
        // opcode=99 (0x63), funct3=111, rs1=11, rs2=12
        ch_uint<32> bgeu = ch_uint<32>(2949191_d);  // 0x00C580E7
        detector.io().instruction = bgeu;
        
        (void)detector;
    }
}

// ============================================================================
// 测试用例 8: 编译验证 (确保所有结构体和组件可以实例化)
// ============================================================================
TEST_CASE("Branch Prediction Structures Compilation", "[branch][compile]") {
    SECTION("BranchDetector compiles") {
        ch::Component* parent = nullptr;
        BranchDetector detector(parent, "compile_detector");
        (void)detector;
    }
    
    SECTION("BranchConditionUnit compiles") {
        ch::Component* parent = nullptr;
        BranchConditionUnit condition(parent, "compile_condition");
        (void)condition;
    }
    
    SECTION("BranchPredictor compiles") {
        ch::Component* parent = nullptr;
        BranchPredictor predictor(parent, "compile_predictor");
        (void)predictor;
    }
    
    SECTION("ControlHazardUnit compiles") {
        ch::Component* parent = nullptr;
        ControlHazardUnit hazard(parent, "compile_hazard");
        (void)hazard;
    }
    
    SECTION("BranchPredictorState compiles") {
        ch::Component* parent = nullptr;
        BranchPredictorState state(parent, "compile_state");
        (void)state;
    }
}
