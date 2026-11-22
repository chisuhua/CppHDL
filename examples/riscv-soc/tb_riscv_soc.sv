// examples/riscv-soc/tb_riscv_soc.sv
//
// EXPECT-SRC: .rddf/checks/soc-rtl-expectations.md#riscv-soc
//
// **SoC 级独立 Verilog TB**（ADR-073 D3 · `generated=no` 终态 —— ADR-074 D7 逃生口）。
//
// ⚠️ **只读 8 根根端口**（ADR-072 D1：核的 `dbg_*` / `obs_*` **不是**顶层端口）
//   根自有 6 = `top_ext_rst` / `top_prog_we` / `top_prog_addr` / `top_prog_data`
//             / `top_store_count` / `top_writeback_count`
//   ⇒ ⇒ 产品端口 = 框架 2（`default_clock` / `default_reset`）+ 6 = **8**（ADR-077 V1）
//
// ⚠️ **判据 MUST 与 C++ 侧（`tb_riscv_soc.cpp` 默认模式）逐条同值**（AC7 的
//   「`interp_checks == min_checks == L`」）⇒ 本文件 **7** 条 · C++ 侧 **7** 条。
//
// ⚠️ **程序与手推台账同源于核级产品**
//   （`.rddf/checks/examples-rtl-expectations.md#riscv-mini-rv32i-soc`）：
//   word 0: addi x1, x0, 42  = 0x02A00093
//   word 1: nop              = 0x00000013
//   word 2: addi x2, x0, 11  = 0x00B00113
//   word 3: nop              = 0x00000013
//   word 4: add  x3, x1, x2  = 0x002081B3   ⇒ x3 = 53
//   word 5: sw   x3, 0(x0)   = 0x00302023   ⇒ [字节地址 0] = 53
//   word 6: jal  x0, 0       = 0x0000006F   ⇒ 自环（rd = x0 ⇒ 不写回）
//   ⇒ ⇒ **手推**：`store_count == 1`（1 条 sw）· `writeback_count == 3`（x1/x2/x3）
//   ⇒ ⇒ 两值**不同** ⇒ 事件源接反 / 同源时必红。

`timescale 1ns/1ps

module tb_riscv_soc;

    // ── 时钟 / 复位 ──
    logic clk = 1'b0;
    logic rst = 1'b1;          // 框架复位（default_reset · 高有效电平）

    // ── 根自有输入 ──
    logic        ext_rst = 1'b1;
    logic        prog_we = 1'b0;
    logic [31:0] prog_addr = 32'h0;
    logic [31:0] prog_data = 32'h0;

    // ── 根自有输出 ──
    logic [15:0] store_count;
    logic [15:0] writeback_count;

    integer checks = 0;
    integer fails  = 0;

    // 手推期望值（ISA 语义 · 与 C++ 侧同一组字面量）
    localparam integer EXP_STORE = 1;
    localparam integer EXP_WB    = 3;
    localparam integer TAIL_IDX  = 39;  // 第 40 拍记尾段基准（与 C++ 的 kTailStart 等价）

    function automatic logic [31:0] prog_word(input integer idx);
        case (idx)
            0: prog_word = 32'h02A00093;   // addi x1, x0, 42
            1: prog_word = 32'h00000013;   // nop
            2: prog_word = 32'h00B00113;   // addi x2, x0, 11
            3: prog_word = 32'h00000013;   // nop
            4: prog_word = 32'h002081B3;   // add x3, x1, x2
            5: prog_word = 32'h00302023;   // sw x3, 0(x0)
            default: prog_word = 32'h0000006F;  // jal x0, 0
        endcase
    endfunction

    task automatic expect_bit(input logic got, input logic want);
        begin
            checks = checks + 1;
            if ($isunknown(got))   fails = fails + 1;
            else if (got !== want) fails = fails + 1;
        end
    endtask

    task automatic expect_eq(input integer got, input integer want);
        begin
            checks = checks + 1;
            if (got !== want) fails = fails + 1;
        end
    endtask

    // ── 装载一个字到 I-TCM（装载期 MUST 保持 `ext_rst = 1`）──
    //    返回值：该拍两计数是否仍为 0（聚合给 A2 用）
    task automatic load_word(input integer idx, input logic [31:0] w, output logic clean);
        begin
            @(negedge clk);
            prog_we   = 1'b1;
            prog_addr = idx[31:0];
            prog_data = w;
            @(posedge clk);
            #1;
            prog_we = 1'b0;
            clean = (store_count == 16'h0) && (writeback_count == 16'h0);
        end
    endtask

    // ── 16 位计数的 32 位零扩展别名（显式拼接 ⇒ 无 WIDTHEXPAND 警告）──
    wire [31:0] store_count_w = {16'h0, store_count};
    wire [31:0] wb_count_w    = {16'h0, writeback_count};

    // ── DUT 实例 ──
    top dut (
        .default_clock(clk),
        .default_reset(rst),
        .top_ext_rst(ext_rst),
        .top_prog_we(prog_we),
        .top_prog_addr(prog_addr),
        .top_prog_data(prog_data),
        .top_store_count(store_count),
        .top_writeback_count(writeback_count)
    );

    always #5 clk = ~clk;

    integer i;
    integer k;
    logic   clean;
    logic   load_clean;
    logic   monotonic_ok;
    integer store_now;
    integer wb_now;
    integer store_prev;
    integer wb_prev;
    integer tail_store;
    integer tail_wb;
    integer final_store;
    integer final_wb;

    initial begin
        // ── 框架复位脉冲（`default_reset` 高有效电平 ⇒ 清所有计数）──
        rst     = 1'b1;
        ext_rst = 1'b1;
        repeat (4) @(posedge clk);
        @(negedge clk);
        rst = 1'b0;

        // ══ A1：复位后两计数为 0（2 条）══
        expect_eq(store_count_w, 0);
        expect_eq(wb_count_w, 0);

        // ── 装载：`ext_rst` 保持 1（核被复位 ⇒ 装载动作不产生写回/store）──
        load_clean = 1'b1;
        for (i = 0; i < 7; i = i + 1) begin
            load_word(i, prog_word(i), clean);
            if (!clean) load_clean = 1'b0;
        end
        // 尾部填 NOP（`ch_mem` 初值 0 = 未知 opcode ⇒ 流水线会冻结）
        for (i = 7; i < 64; i = i + 1) begin
            load_word(i, 32'h00000013, clean);
            if (!clean) load_clean = 1'b0;
        end

        // ══ A2：装载期两计数保持 0（1 条 · ext_rst 接反 ⇒ 装载被当指令执行 ⇒ 红）══
        expect_bit(load_clean, 1'b1);

        // ── 释放核复位 ──
        @(negedge clk);
        ext_rst = 1'b0;

        // ── 逐拍 60 拍（采样 A3–A6）──
        store_prev   = 0;
        wb_prev      = 0;
        monotonic_ok = 1'b1;
        tail_store   = 0;
        tail_wb      = 0;
        for (k = 0; k < 60; k = k + 1) begin
            @(posedge clk);
            #1;
            store_now = store_count_w;
            wb_now    = wb_count_w;
            if (k == TAIL_IDX) begin
                tail_store = store_now;
                tail_wb    = wb_now;
            end
            if (store_now < store_prev || wb_now < wb_prev) monotonic_ok = 1'b0;
            store_prev = store_now;
            wb_prev    = wb_now;
        end
        final_store = store_now;
        final_wb    = wb_now;

        // ══ A3 / A4：手推终值（2 条）══
        expect_eq(final_store, EXP_STORE);
        expect_eq(final_wb, EXP_WB);

        // ══ A5：单调不减（1 条）══
        expect_bit(monotonic_ok, 1'b1);

        // ══ A6：尾段不再增长（1 条）══
        expect_bit((final_store == tail_store) && (final_wb == tail_wb), 1'b1);

        $display("RESULT: checks=%0d fails=%0d", checks, fails);
        $finish;
    end

endmodule
