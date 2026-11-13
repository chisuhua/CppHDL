// examples/riscv-mini/tb_rv32i_soc.sv
//
// EXPECT-SRC: .rddf/checks/examples-rtl-expectations.md#riscv-mini-rv32i-soc
//
// **独立 Verilog TB**（`examples-rtl-sim-coverage` E3）—— `riscv-mini` 的 RV32I SoC
// （`kind=dut` —— 它**不是** `CH_TRACKED_EXAMPLE_TARGETS` 的条目 ⇒ 单列）
//
// ⚠️ **本 TB 的做法与原清单的差异（重要）**：清单原写「minitest retire 数 / `sw` 落到 tohost」
//   ⇒ ⇒ 那要求**外部 TB 装载 minitest 固件** ⇒ ⇒ 期望值将依赖**仓内那个金标**
//     （178 条退休）⇒ ⇒ **B3 的 T5 会重算该金标** ⇒ ⇒ 硬判它必然在 B3 落地时转红。
//   ⇒ ⇒ 本 TB 改为：**TB 装载自己的小程序**（经顶层的 `prog_we`/`prog_addr`/`prog_data` 装载口）
//     ⇒ ⇒ 期望值**完全由 ISA 语义手推** ⇒ ⇒ **不依赖任何仓内金标** ⇒ ⇒ B3 改金标时不会红。
//   ⇒ ⇒ 「retire 数」因此只判「单调递增且 > 0」（impl-contract），**不锁死具体值**。
//
// ⚠️ **小程序（7 个字 · 编码**手推**，非抄仿真输出）**：
//   word 0: `addi x1, x0, 42`   = 0x02A00093
//           imm[11:0]=0x02A · rs1=0 · funct3=000 · rd=1 · opcode=0010011
//   word 1: `nop`               = 0x00000013   ← 等 x1 写回（本流水线**无数据前推**）
//   word 2: `addi x2, x0, 11`   = 0x00B00113
//   word 3: `nop`               = 0x00000013   ← 等 x2 写回
//   word 4: `add  x3, x1, x2`   = 0x002081B3   ⇒ x3 = 42 + 11 = **53**
//           funct7=0000000 · rs2=2 · rs1=1 · funct3=000 · rd=3 · opcode=0110011
//   word 5: `sw   x3, 0(x0)`    = 0x00302023   ⇒ 把 53 写到**字节地址 0**
//           imm[11:5]=0000000 · rs2=3 · rs1=0 · funct3=010 · imm[4:0]=00000 · opcode=0100011
//   word 6: `jal  x0, 0`        = 0x0000006F   ⇒ 自环（停机）
//   ⇒ 编码口径已与仓内 `rv32i-isa-semantics` capability 的样例交叉核对
//     （该 capability 给出 `addi x21, x0, -8` = `0xFF800A93` ⇒ 与本表同一套编码规则）。
//
// ✅ **端口名已具名（2026-11-13 · R17.2 · 用户 ⑤ 的原始诉求）**：本顶层 **32** 个端口
//   全部带**显式名** ⇒ ⇒ 产物端口名 = `top_<C++ 成员名>` ⇒ ⇒ **本 TB 不再需要「查 port map」**。
//   ⇒ ⇒ 根组件自有的 32 个端口：`top_ext_clk` / `top_ext_rst` / `top_prog_we` / `top_prog_addr` /
//   `top_prog_data` / `top_obs_*`（11）/ `top_dbg_*`（16）。
//   ⇒ ⇒ ⚠️ **仍保留未具名的是后代冒泡端口**（`top_pipeline_*` / `top_itcm_*` / `top_dtcm_*`）
//   ⇒ ⇒ 它们**不在**本批范围（不是根组件自有 IO）⇒ ⇒ 归属**后续批次**。
//
// ⚠️ **端口映射（实测确认，非猜）**：本顶层产物端口表 = **114** 个
//   （**7** 输入 + **107** 输出 · 2026-11-13 实测）⇒ ⇒ **7** 输入 = 框架的
//   `default_clock` / `default_reset` + 本组件自有的 **5** 个
//   ⇒ ⇒ **5** 个自有输入里 **1 位输入有 3 个** ⇒ 必须实证：
//   · `top_ext_rst` = `ext_rst`（实测：只有它拉低后流水线才取指）
//   · `top_prog_we` = `prog_we`（实测：装载口生效）
//   · `top_ext_clk` = `ext_clk` ⇒ ⚠️ **该端口是「死端口」** —— 产物里
//     **所有**流水线寄存器都锁在框架 `default_clock`（`always_ff @(posedge default_clock …)`）
//     ⇒ ⇒ `ext_clk` 对行为**无影响** ⇒ ⇒ 已登记为发现（见期望登记文档）。
//   ⇒ ⇒ **107** 个输出 = 本组件自有的 **27** 个 + **后代冒泡**的 **80** 个
//   （`top_pipeline_*` / `top_itcm_*` / `top_dtcm_*` ⇒ ⇒ **未具名** ⇒ ⇒ 仍在 `sink*` 里）。
//   ⚠️ **历史订正**：原文写「113 个端口（5 输入 + 108 输出）」⇒ ⇒ 该数字是
//   **C8 tie-off 之前**的（B0 批给 `mem_is_load` 补 driver ⇒ ⇒ 顶层 input 8 → 7）
//   ⇒ ⇒ 2026-11-13 按产物实测订正为 **114（7 + 107）**。
//
// ⚠️ **未用输出 MUST 被读**：本 TB 把 96 个不参与判据的输出接进 `sink*` 数组
//   ⇒ ⇒ 严格 flags 下不读会触发 `%Warning-UNUSEDSIGNAL`（**致命** —— 实测踩过同类坑）。

`timescale 1ns/1ps

module tb_rv32i_soc;

    logic clk = 0;
    logic rst = 1'b1;            // 框架复位（`default_reset`，高有效）

    logic        ext_clk = 1'b0;
    logic        ext_rst = 1'b0;
    logic        prog_we = 1'b0;
    logic [31:0] prog_addr = 32'h0;
    logic [31:0] prog_data = 32'h0;
    logic [31:0] obs_data_addr;
    logic [31:0] obs_data_write_data;
    logic        obs_data_write_en;
    logic [47:0] obs_perf_cycles;
    logic [47:0] obs_perf_instructions;
    logic [31:0] dbg_id_instr;
    logic [31:0] dbg_id_pc;
    logic        dbg_id_valid;
    logic        dbg_wb_write_en;
    logic [31:0] dbg_wb_write_data;
    logic [4:0]  dbg_wb_rd_addr;

    logic [37:0] sink1;   // 38 个未用输出（宽度 1）
    logic [3:0] sink2;   // 2 个未用输出（宽度 2）
    logic [2:0] sink3;   // 1 个未用输出（宽度 3）
    logic [7:0] sink4;   // 2 个未用输出（宽度 4）
    logic [39:0] sink5;   // 8 个未用输出（宽度 5）
    logic [13:0] sink7;   // 2 个未用输出（宽度 7）
    logic [1311:0] sink32;   // 41 个未用输出（宽度 32）
    logic [95:0] sink48;   // 2 个未用输出（宽度 48）

    integer checks = 0;
    integer fails  = 0;

    // ── 观测收集 ───────────────────────────────────────────────────
    integer fetch_cnt  [0:6];    // 每个程序字被取到的次数
    integer wb_cnt     = 0;      // 写回事件数
    integer wb_rd      [0:31];   // 写回的 rd
    logic [31:0] wb_data [0:31];
    integer store_cnt  = 0;      // 写存储事件数
    logic [31:0] store_data [0:15];
    logic [31:0] store_addr [0:15];
    integer fetch_pc_cnt = 0;
    logic [31:0] fetch_pc [0:15];
    logic [47:0] last_perf = 48'h0;
    logic        perf_decreased = 1'b0;

    always @(posedge clk) begin
        if (!rst) begin
            if (dbg_id_valid === 1'b1) begin
                if (fetch_pc_cnt < 16) begin
                    fetch_pc[fetch_pc_cnt] = dbg_id_pc;
                    fetch_pc_cnt = fetch_pc_cnt + 1;
                end
                for (integer k = 0; k < 7; k = k + 1) begin
                    if (dbg_id_instr == prog_word(k)) fetch_cnt[k] = fetch_cnt[k] + 1;
                end
            end
            if (dbg_wb_write_en === 1'b1) begin
                if (wb_cnt < 32) begin
                    wb_rd[wb_cnt] = {27'b0, dbg_wb_rd_addr};   // ⚠️ 显式零扩展（5 位 → 32 位）
                    wb_data[wb_cnt] = dbg_wb_write_data;
                    wb_cnt = wb_cnt + 1;
                end
            end
            if (obs_data_write_en === 1'b1) begin
                if (store_cnt < 16) begin
                    store_data[store_cnt] = obs_data_write_data;
                    store_addr[store_cnt] = obs_data_addr;
                    store_cnt = store_cnt + 1;
                end
            end
            if (obs_perf_instructions < last_perf) perf_decreased = 1'b1;
            last_perf = obs_perf_instructions;
        end
    end

    // ── 程序字（手推的编码）─────────────────────────────────────────
    function automatic logic [31:0] prog_word(input integer idx);
        begin
            case (idx)
                0: prog_word = 32'h02A00093;   // addi x1, x0, 42
                1: prog_word = 32'h00000013;   // nop
                2: prog_word = 32'h00B00113;   // addi x2, x0, 11
                3: prog_word = 32'h00000013;   // nop
                4: prog_word = 32'h002081B3;   // add x3, x1, x2
                5: prog_word = 32'h00302023;   // sw x3, 0(x0)
                default: prog_word = 32'h0000006F;  // jal x0, 0
            endcase
        end
    endfunction

    task automatic expect_bit(input logic got, input logic want);
        begin
            checks = checks + 1;
            if ($isunknown(got))   fails = fails + 1;
            else if (got !== want) fails = fails + 1;
        end
    endtask

    task automatic expect_eq32(input logic [31:0] got, input logic [31:0] want);
        begin
            checks = checks + 1;
            if (got !== want) fails = fails + 1;
        end
    endtask

    task automatic expect_eq(input integer got, input integer want);
        begin
            checks = checks + 1;
            if (got !== want) fails = fails + 1;
        end
    endtask

    // ── 装载一个字到 I-TCM（装载期 MUST 保持 `ext_rst = 1`，见顶层头注释）──
    task automatic load_word(input integer idx, input logic [31:0] w);
        begin
            @(negedge clk);
            prog_we   = 1'b1;
            prog_addr = idx[31:0];
            prog_data = w;
            @(posedge clk);
            #1;
            prog_we = 1'b0;
        end
    endtask

    // ── DUT 实例（⚠️ 实例化 + 时钟生成器构成 TB 的**最小骨架**）──
    top dut (
        .default_clock(clk),
        .default_reset(rst),
        .top_ext_clk(ext_clk),
        .top_ext_rst(ext_rst),
        .top_prog_we(prog_we),
        .top_prog_addr(prog_addr),
        .top_prog_data(prog_data),
        .top_obs_instr_addr(sink32[0*32 +: 32]),
        .top_obs_data_addr(obs_data_addr),
        .top_obs_data_write_data(obs_data_write_data),
        .top_obs_data_write_en(obs_data_write_en),
        .top_obs_perf_cycles(obs_perf_cycles),
        .top_obs_perf_instructions(obs_perf_instructions),
        .top_obs_perf_branch_count(sink32[1*32 +: 32]),
        .top_obs_perf_branch_mispredict(sink32[2*32 +: 32]),
        .top_obs_perf_load_use_stalls(sink32[3*32 +: 32]),
        .top_obs_perf_control_stalls(sink32[4*32 +: 32]),
        .top_dbg_id_instr(dbg_id_instr),
        .top_dbg_id_pc(dbg_id_pc),
        .top_dbg_id_valid(dbg_id_valid),
        .top_dbg_id_is_branch(sink1[0*1 +: 1]),
        .top_dbg_id_is_load(sink1[1*1 +: 1]),
        .top_dbg_id_is_store(sink1[2*1 +: 1]),
        .top_dbg_branch_taken(sink1[3*1 +: 1]),
        .top_dbg_wb_write_en(dbg_wb_write_en),
        .top_dbg_wb_write_data(dbg_wb_write_data),
        .top_dbg_wb_rd_addr(dbg_wb_rd_addr),
        .top_dbg_exmem_alu_result(sink32[5*32 +: 32]),
        .top_dbg_exmem_imm(sink32[6*32 +: 32]),
        .top_dbg_exmem_rs2_data(sink32[7*32 +: 32]),
        .top_obs_data_rdata(sink32[8*32 +: 32]),
        .top_dbg_exmem_valid(sink1[4*1 +: 1]),
        .top_dbg_mem_valid_out(sink1[5*1 +: 1]),
        .top_dbg_load_use_detected(sink1[6*1 +: 1]),
        .top_pipeline_if_stage_unnamed_output_2(sink32[9*32 +: 32]),
        .top_pipeline_if_stage_unnamed_output_1(sink32[10*32 +: 32]),
        .top_pipeline_if_stage_unnamed_output_3(sink1[7*1 +: 1]),
        .top_pipeline_if_stage_unnamed_output(sink32[11*32 +: 32]),
        .top_pipeline_id_stage_unnamed_output_13(sink5[0*5 +: 5]),
        .top_pipeline_id_stage_unnamed_output_14(sink5[1*5 +: 5]),
        .top_pipeline_id_stage_unnamed_output_5(sink32[12*32 +: 32]),
        .top_pipeline_id_stage_unnamed_output_8(sink32[13*32 +: 32]),
        .top_pipeline_id_stage_unnamed_output_7(sink32[14*32 +: 32]),
        .top_pipeline_id_stage_unnamed_output_6(sink7[0*7 +: 7]),
        .top_pipeline_id_stage_unnamed_output_4(sink3[0*3 +: 3]),
        .top_pipeline_id_stage_unnamed_output(sink1[8*1 +: 1]),
        .top_pipeline_id_stage_unnamed_output_1(sink1[9*1 +: 1]),
        .top_pipeline_id_stage_unnamed_output_2(sink1[10*1 +: 1]),
        .top_pipeline_id_stage_unnamed_output_3(sink1[11*1 +: 1]),
        .top_pipeline_id_stage_unnamed_output_15(sink1[12*1 +: 1]),
        .top_pipeline_id_stage_unnamed_output_9(sink5[2*5 +: 5]),
        .top_pipeline_id_stage_unnamed_output_16(sink1[13*1 +: 1]),
        .top_pipeline_id_stage_unnamed_output_12(sink32[15*32 +: 32]),
        .top_pipeline_id_stage_unnamed_output_10(sink5[3*5 +: 5]),
        .top_pipeline_id_stage_unnamed_output_11(sink5[4*5 +: 5]),
        .top_pipeline_id_stage_unnamed_output_17(sink7[1*7 +: 7]),
        .top_pipeline_ex_stage_unnamed_output_2(sink32[16*32 +: 32]),
        .top_pipeline_ex_stage_unnamed_output(sink1[14*1 +: 1]),
        .top_pipeline_ex_stage_unnamed_output_1(sink32[17*32 +: 32]),
        .top_pipeline_ex_stage_unnamed_output_3(sink32[18*32 +: 32]),
        .top_pipeline_ex_stage_unnamed_output_4(sink1[15*1 +: 1]),
        .top_pipeline_ex_stage_unnamed_output_5(sink1[16*1 +: 1]),
        .top_pipeline_ex_stage_unnamed_output_6(sink1[17*1 +: 1]),
        .top_pipeline_ex_stage_unnamed_output_7(sink1[18*1 +: 1]),
        .top_pipeline_mem_stage_unnamed_output_1(sink32[19*32 +: 32]),
        .top_pipeline_mem_stage_unnamed_output_2(sink5[5*5 +: 5]),
        .top_pipeline_mem_stage_unnamed_output_3(sink32[20*32 +: 32]),
        .top_pipeline_mem_stage_unnamed_output_4(sink4[0*4 +: 4]),
        .top_pipeline_mem_stage_unnamed_output(sink1[19*1 +: 1]),
        .top_pipeline_wb_stage_unnamed_output_2(sink5[6*5 +: 5]),
        .top_pipeline_wb_stage_unnamed_output(sink32[21*32 +: 32]),
        .top_pipeline_wb_stage_unnamed_output_1(sink1[20*1 +: 1]),
        .top_pipeline_hazard_unit_unnamed_output_3(sink2[0*2 +: 2]),
        .top_pipeline_hazard_unit_unnamed_output_4(sink2[1*2 +: 2]),
        .top_pipeline_hazard_unit_unnamed_output_5(sink32[22*32 +: 32]),
        .top_pipeline_hazard_unit_unnamed_output_6(sink32[23*32 +: 32]),
        .top_pipeline_hazard_unit_unnamed_output_1(sink1[21*1 +: 1]),
        .top_pipeline_hazard_unit_unnamed_output(sink1[22*1 +: 1]),
        .top_pipeline_hazard_unit_unnamed_output_2(sink1[23*1 +: 1]),
        .top_pipeline_hazard_unit_unnamed_output_7(sink1[24*1 +: 1]),
        .top_pipeline_forwarding_mux_unnamed_output(sink32[24*32 +: 32]),
        .top_pipeline_forwarding_mux_unnamed_output_1(sink32[25*32 +: 32]),
        .top_pipeline_unnamed_output(sink32[26*32 +: 32]),
        .top_pipeline_unnamed_output_1(sink32[27*32 +: 32]),
        .top_pipeline_unnamed_output_2(sink32[28*32 +: 32]),
        .top_pipeline_unnamed_output_26(sink4[1*4 +: 4]),
        .top_pipeline_unnamed_output_3(sink1[25*1 +: 1]),
        .top_pipeline_unnamed_output_4(sink48[0*48 +: 48]),
        .top_pipeline_unnamed_output_5(sink48[1*48 +: 48]),
        .top_pipeline_unnamed_output_6(sink32[29*32 +: 32]),
        .top_pipeline_unnamed_output_7(sink32[30*32 +: 32]),
        .top_pipeline_unnamed_output_8(sink32[31*32 +: 32]),
        .top_pipeline_unnamed_output_9(sink32[32*32 +: 32]),
        .top_pipeline_unnamed_output_10(sink32[33*32 +: 32]),
        .top_pipeline_unnamed_output_11(sink32[34*32 +: 32]),
        .top_pipeline_unnamed_output_12(sink1[26*1 +: 1]),
        .top_pipeline_unnamed_output_13(sink1[27*1 +: 1]),
        .top_pipeline_unnamed_output_14(sink1[28*1 +: 1]),
        .top_pipeline_unnamed_output_15(sink1[29*1 +: 1]),
        .top_pipeline_unnamed_output_16(sink1[30*1 +: 1]),
        .top_pipeline_unnamed_output_23(sink1[31*1 +: 1]),
        .top_pipeline_unnamed_output_27(sink1[32*1 +: 1]),
        .top_pipeline_unnamed_output_24(sink1[33*1 +: 1]),
        .top_pipeline_unnamed_output_25(sink1[34*1 +: 1]),
        .top_pipeline_unnamed_output_17(sink1[35*1 +: 1]),
        .top_pipeline_unnamed_output_18(sink32[35*32 +: 32]),
        .top_pipeline_unnamed_output_19(sink5[7*5 +: 5]),
        .top_pipeline_unnamed_output_20(sink32[36*32 +: 32]),
        .top_pipeline_unnamed_output_21(sink32[37*32 +: 32]),
        .top_pipeline_unnamed_output_22(sink32[38*32 +: 32]),
        .top_itcm_unnamed_output(sink32[39*32 +: 32]),
        .top_itcm_unnamed_output_1(sink1[36*1 +: 1]),
        .top_dtcm_unnamed_output_1(sink1[37*1 +: 1]),
        .top_dtcm_unnamed_output(sink32[40*32 +: 32])
    );

    always #5 clk = ~clk;

    integer i = 0;
    integer k = 0;
    integer found = 0;

    initial begin
        // ── 复位 + 装载（`ext_rst` 保持 1 ⇒ 计数器不被装载污染）──
        rst    = 1'b1;
        ext_rst = 1'b1;
        repeat (4) @(posedge clk);
        for (i = 0; i < 7; i = i + 1) begin
            load_word(i, prog_word(i));
        end
        // ── 释放复位 ────────────────────────────────────────────────
        @(negedge clk);
        rst = 1'b0;
        ext_rst = 1'b0;

        // ── 跑 60 拍（足够覆盖 7 个字 + 写回 + 访存）──────────────
        repeat (60) @(posedge clk);
        #1;

        // ══════════════════════════════════════════════════════════════
        // S1：**复位 pc + 首条指令**（spec-derived）
        //   · IF 阶段 `pc_reg` 复位为 **0x80000000**（`stages/if_stage.h:71`）
        //   · I-TCM 从**字 0** 取指（顶层头注释：地址高位被截断 ⇒ 低 12 位为 0）
        // ══════════════════════════════════════════════════════════════
        expect_eq(fetch_pc_cnt > 0 ? 1 : 0, 1);
        if (fetch_pc_cnt > 0) begin
            expect_eq32(fetch_pc[0], 32'h80000000);
        end

        // ══════════════════════════════════════════════════════════════
        // S2：**程序里的每个字都被取到**（spec-derived）
        // ══════════════════════════════════════════════════════════════
        for (i = 0; i < 7; i = i + 1) begin
            expect_eq(fetch_cnt[i] > 0 ? 1 : 0, 1);
        end

        // ══════════════════════════════════════════════════════════════
        // S3：**取指 pc 按 +4 递增**（spec-derived · 字地址 +4）
        //   ⚠️ 只判前 6 段（第 7 个字是 `jal` ⇒ 会跳转 ⇒ 不再 +4）
        // ══════════════════════════════════════════════════════════════
        for (i = 1; i < 7 && i < fetch_pc_cnt; i = i + 1) begin
            expect_eq32(fetch_pc[i] - fetch_pc[i - 1], 32'd4);
        end

        // ══════════════════════════════════════════════════════════════
        // S4：**写回值**（spec-derived · ISA 语义手推）
        //   · `addi x1, x0, 42` ⇒ 写回 x1 = **42**
        //   · `addi x2, x0, 11` ⇒ 写回 x2 = **11**
        //   · `add  x3, x1, x2` ⇒ 写回 x3 = 42 + 11 = **53**
        // ══════════════════════════════════════════════════════════════
        for (k = 0; k < 3; k = k + 1) begin
            found = 0;
            for (i = 0; i < wb_cnt; i = i + 1) begin
                if (wb_rd[i] == k + 1) found = found + 1;   // rd = x1/x2/x3
            end
            expect_eq(found > 0 ? 1 : 0, 1);
            found = 0;
            for (i = 0; i < wb_cnt; i = i + 1) begin
                if (wb_rd[i] == k + 1 && wb_data[i] == (k == 0 ? 32'd42 : (k == 1 ? 32'd11 : 32'd53))) begin
                    found = found + 1;
                end
            end
            expect_eq(found > 0 ? 1 : 0, 1);
        end

        // ══════════════════════════════════════════════════════════════
        // S5：**访存**（spec-derived · `sw x3, 0(x0)` ⇒ 地址 = x0+0 = 0 · 数据 = 53）
        // ══════════════════════════════════════════════════════════════
        found = 0;
        for (i = 0; i < store_cnt; i = i + 1) begin
            if (store_data[i] == 32'd53 && store_addr[i] == 32'd0) found = found + 1;
        end
        expect_eq(found > 0 ? 1 : 0, 1);

        // ══════════════════════════════════════════════════════════════
        // S6：**退休计数**（impl-contract · **不锁死具体值**）
        //   ⚠️ 原清单写「minitest retire 数」⇒ ⇒ 那依赖仓内金标（178）
        //     ⇒ ⇒ B3 的 T5 会重算它 ⇒ ⇒ 硬判必然在 B3 落地时转红
        //   ⇒ ⇒ 只判「单调递增」+「> 0」
        // ══════════════════════════════════════════════════════════════
        checks = checks + 1;
        if (perf_decreased === 1'b1) fails = fails + 1;   // ⚠️ 不用 1 位 → integer（位宽扩张）
        checks = checks + 1;
        if (!(last_perf > 48'd0)) fails = fails + 1;   // ⚠️ 不用三元（位宽歧义）

        $display("RESULT: checks=%0d fails=%0d", checks, fails);
        if (fails != 0) $fatal(1, "tb_rv32i_soc failed");
        $finish;
    end

endmodule
