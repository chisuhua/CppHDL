// examples/riscv-soc/tb_riscv_soc.sv
//
// EXPECT-SRC: .rddf/checks/soc-rtl-expectations.md#riscv-soc
//
// **SoC 级独立 Verilog TB**（ADR-073 D3 · `generated=no` 终态 —— ADR-074 D7 逃生口）。
//
// ⚠️ **只读根端口**（ADR-072 D1：核的 `dbg_*` / `obs_*` **不是**顶层端口）
//   根自有 **8** = `top_ext_rst` / `top_prog_we` / `top_prog_addr` / `top_prog_data`
//             / `top_store_count` / `top_writeback_count` / `top_gpio_out` / `top_gpio_in`
//   ⇒ ⇒ 产品端口 = 框架 2 + 根自有 8 = **10**（ADR-077 V1 · S4a 的 dated addendum）
//
// ⚠️ **判据 MUST 与 C++ 侧（`tb_riscv_soc.cpp` 默认模式）逐条同值**（AC7 的
//   「`interp_checks == min_checks == L`」）⇒ 本文件 **10** 条 · C++ 侧 **10** 条。
//   ⚠️ A9（读值回路）是 2026-11-24 critic BLOCKER 的靶子：原 9 条判据对「读数据 mux
//      与 M2 错拍」**结构性失明**（读值在 SoC 根端口不可观测 · gpio_in 又被 tie 0）。
//
// ⚠️ **程序与 C++ 侧同源**（S4a 起与核级产品**分叉** —— 核级没有 MMIO 从设备）：
//   主程序 = **12 字**（`.rddf/checks/soc-rtl-expectations.md#riscv-soc` §2），
//   A8 用**独立的 4 字程序**（未映射地址 ⇒ DECERR）。
//   ⇒ ⇒ **手推**：`store_count == 2`（D-L1 一条 + MMIO 一条）· `writeback_count == 6`
//     · `gpio_out == 0xA5` · DECERR 场景 `writeback_count == 3`
//   ⇒ ⇒ 前两值**不同** ⇒ 事件源接反 / 同源时必红。

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
    logic [7:0]  gpio_out;

    // ── 根自有输入（S4a：GPIO 物理面 · 未驱动 ⇒ 读到未定义值）──
    logic [7:0]  gpio_in = 8'h00;

    integer checks = 0;
    integer fails  = 0;

    // 手推期望值（ISA 语义 · 与 C++ 侧同一组字面量）
    localparam integer EXP_STORE = 2;   // D-L1 一条 + MMIO 一条
    localparam integer EXP_WB    = 6;   // x1/x2/x3/x4/x6/x5
    localparam integer EXP_GPIO  = 32'h000000A5;  // word 10 写的值（32 位零扩展避免 WIDTHEXPAND）
    localparam integer EXP_ERR_WB = 3;  // A8：x1(lui)/x2(lw)/x3(addi)
    localparam integer EXP_RDBACK = 32'h0000005A;  // A9：gpio_in 原样写回 DATA_OUT
    localparam integer TAIL_IDX  = 84;  // 第 85 拍记尾段基准（与 C++ 的 kTailStart 等价）

    function automatic logic [31:0] prog_word(input integer idx);
        case (idx)
            0:  prog_word = 32'h02A00093;  // addi x1, x0, 42
            1:  prog_word = 32'h00000013;  // nop
            2:  prog_word = 32'h00B00113;  // addi x2, x0, 11
            3:  prog_word = 32'h00000013;  // nop
            4:  prog_word = 32'h002081B3;  // add x3, x1, x2
            5:  prog_word = 32'h00302023;  // sw x3, 0(x0)      ← D-L1 路径
            6:  prog_word = 32'h00000013;  // nop
            7:  prog_word = 32'h80102237;  // lui x4, 0x80102   ⇒ x4 = 0x8010_2000
            8:  prog_word = 32'h00422303;  // lw x6, 4(x4)      ← MMIO 读（DATA_IN）
            9:  prog_word = 32'h0A500293;  // addi x5, x0, 0xA5
            10: prog_word = 32'h00522023;  // sw x5, 0(x4)      ← MMIO 写（DATA_OUT）
            default: prog_word = 32'h0000006F;  // jal x0, 0
        endcase
    endfunction

    // A9 的**独立 5 字程序**（MMIO 读值回路 ⇒ gpio_in 0x5A 必须原样写回 DATA_OUT）
    function automatic logic [31:0] rdback_word(input integer idx);
        case (idx)
            0:  rdback_word = 32'h80102237;  // lui  x4, 0x80102   ⇒ 0x8010_2000
            1:  rdback_word = 32'h00422303;  // lw   x6, 4(x4)     ← MMIO 读 DATA_IN（0x5A）
            2:  rdback_word = 32'h00100393;  // addi x7, x0, 1     ← **非 MMIO 后继**（逼出错拍）
            3:  rdback_word = 32'h00622023;  // sw   x6, 0(x4)     ⇒ DATA_OUT = x6
            default: rdback_word = 32'h0000006F;  // jal x0, 0
        endcase
    endfunction

    // A8 的**独立 4 字程序**（未映射地址 ⇒ DECERR）
    function automatic logic [31:0] err_word(input integer idx);
        case (idx)
            0:  err_word = 32'h801000B7;  // lui  x1, 0x80100  ⇒ 0x8010_0000
            1:  err_word = 32'h0000A103;  // lw   x2, 0(x1)     ← 未映射 ⇒ DECERR
            2:  err_word = 32'h00700193;  // addi x3, x0, 7     （挂死 ⇒ 永不写回）
            default: err_word = 32'h0000006F;  // jal x0, 0
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
        .top_writeback_count(writeback_count),
        // S4a：GPIO 物理面（根自有 2 根 ⇒ 产品端口 8 → 10）
        .top_gpio_out(gpio_out),
        .top_gpio_in(gpio_in)
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
        for (i = 0; i < 12; i = i + 1) begin
            load_word(i, prog_word(i), clean);
            if (!clean) load_clean = 1'b0;
        end
        // 尾部填 NOP（`ch_mem` 初值 0 = 未知 opcode ⇒ 流水线会冻结）
        for (i = 12; i < 64; i = i + 1) begin
            load_word(i, 32'h00000013, clean);
            if (!clean) load_clean = 1'b0;
        end

        // ══ A2：装载期两计数保持 0（1 条 · ext_rst 接反 ⇒ 装载被当指令执行 ⇒ 红）══
        expect_bit(load_clean, 1'b1);

        // ── 释放核复位 ──
        @(negedge clk);
        ext_rst = 1'b0;

        // ── 逐拍 **100** 拍（采样 A3–A6 · 12 字程序含 2 次 MMIO 事务）──
        store_prev   = 0;
        wb_prev      = 0;
        monotonic_ok = 1'b1;
        tail_store   = 0;
        tail_wb      = 0;
        for (k = 0; k < 100; k = k + 1) begin
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

        // ══ A7：MMIO **写通路的值** = GPIO 的 DATA_OUT（1 条）══
        expect_eq({24'h0, gpio_out}, EXP_GPIO);

        // ══ A8：未映射地址 ⇒ DECERR（**不挂死**）· 独立 4 字程序（1 条）══
        // ⚠️ `SocTraceCounter` **没有** `ext_rst` 复位路径（只有框架
        //    `default_reset` 的 `ch_reg` INIT）⇒ ⇒ 第二场景 MUST 用 `rst` 清计数。
        rst     = 1'b1;
        ext_rst = 1'b1;
        repeat (4) @(posedge clk);
        @(negedge clk);
        rst = 1'b0;
        for (i = 0; i < 4; i = i + 1) begin
            load_word(i, err_word(i), clean);
        end
        for (i = 4; i < 64; i = i + 1) begin
            load_word(i, 32'h00000013, clean);
        end
        @(negedge clk);
        ext_rst = 1'b0;
        for (k = 0; k < 100; k = k + 1) begin
            @(posedge clk);
            #1;
        end
        expect_eq(wb_count_w, EXP_ERR_WB);

        // ══ A9：MMIO **读值**回路（独立 5 字程序 · gpio_in = 0x5A）（1 条）══
        // ⚠️ 靶子 = 读数据 mux 的**选择线错拍**（用后继指令的地址选路径 ⇒ 读到另一条
        //    路径的保持寄存器）。⇒ ⇒ 若 mux 错拍 ⇒ x6 = dcache.rdata（本场景无 D-L1 读
        //    ⇒ 恒 0）⇒ ⇒ gpio_out = 0 ≠ 0x5A ⇒ **必红**。
        rst     = 1'b1;
        ext_rst = 1'b1;
        repeat (4) @(posedge clk);
        @(negedge clk);
        rst = 1'b0;
        gpio_in = 8'h5A;
        for (i = 0; i < 5; i = i + 1) begin
            load_word(i, rdback_word(i), clean);
        end
        for (i = 5; i < 64; i = i + 1) begin
            load_word(i, 32'h00000013, clean);
        end
        @(negedge clk);
        ext_rst = 1'b0;
        for (k = 0; k < 100; k = k + 1) begin
            @(posedge clk);
            #1;
        end
        expect_eq({24'h0, gpio_out}, EXP_RDBACK);

        $display("RESULT: checks=%0d fails=%0d", checks, fails);
        $finish;
    end

endmodule
