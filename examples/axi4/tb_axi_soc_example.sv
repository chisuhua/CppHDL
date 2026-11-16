// examples/axi4/tb_axi_soc_example.sv
//
// EXPECT-SRC: .rddf/checks/soc-rtl-expectations.md#soc-example
//
// **独立 Verilog TB**（`soc-rtl-product-and-tb-2026-11` · **T5**）—— SoC 层产物
// （`emit_rtl_axi_soc --out <dir>` ⇒ 5 个 module：`AxiLiteTestMaster` /
// `AxiLiteMatrix2x2_32_32_` / `AxiLiteGpio_8_` / `AxiLiteUart_16_` / `top`）
//
// ⚠️ **期望值全部手推**（期望源 = `.rddf/checks/soc-rtl-expectations.md#soc-example`）
//   ⇒ ⇒ **禁**把被测 `axi_soc_example.cpp` 或产物 `top.v` 写成出处（那是转录不是推导）。
//
// ⚠️ **顶层端口名是 `top_unnamed_input_N` / `top_unnamed_output_N`**（根 `__io` 用 legacy
//   未具名声明 ⇒ manifest 的 `cpp_member` 是占位符）⇒ ⇒ 映射表见期望源 §1
//   （**位宽序列唯一确定** ⇒ 不是猜编号）。
//
// ⚠️ **完成判定 MUST 用「按已知延迟采样」** —— `done` 是**粘滞**的
//   （`axi_soc_example.cpp:149-151` 的 `done_reg` 无清零分支 ⇒ 首笔后恒 1）
//   ⇒ ⇒ **禁**等 `done` 上升沿。
//
// ⚠️ **14 个端口 MUST 全部连接**（否则严格 flags 下 `%Warning-PINMISSING` 致命 ——
//   仓内实证 `tb_axi4_lite_example.sv:33-34`）：
//   `timer_irq` 是**零消费死端口**（ADR-073 事实⑭）⇒ 接 0；
//   `uart_rx` MUST 接**空闲高**（低电平会被当成起始位）。

`timescale 1ns/1ps

module tb_axi_soc_example;

    logic clk = 0;
    logic rst = 1'b1;          // default_reset（高有效）
    logic rst_n = 1'b0;        // AXI 级低有效（与框架复位**不是**同一个）

    // 顶层输入（映射见期望源 §1）
    logic        start = 1'b0;
    logic        write = 1'b0;
    logic [31:0] addr  = 32'h0;
    logic [31:0] wdata = 32'h0;
    logic [7:0]  gpio_in = 8'h5A;
    logic        uart_rx = 1'b1;
    logic        timer_irq = 1'b0;

    // 顶层输出
    logic [31:0] rdata;
    logic        done;
    logic [7:0]  gpio_out;
    logic        uart_tx;

    integer checks = 0;
    integer fails  = 0;
    integer si      = 0;
    integer first_low = -1;

    // ── DUT 实例（⚠️ 实例化 + 时钟生成器构成 TB 的**最小骨架** ⇒ MUST 成对出现
    //   —— 写 TB 时曾两次分别漏掉 ⇒ 现象是「end at 0s」与「输出悬空 = 0」）
    top dut (
        .default_clock(clk),
        .default_reset(rst),
        .top_unnamed_input(start),
        .top_unnamed_input_1(write),
        .top_unnamed_input_2(addr),
        .top_unnamed_input_3(wdata),
        .top_unnamed_input_4(rst_n),
        .top_unnamed_input_5(gpio_in),
        .top_unnamed_input_6(uart_rx),
        .top_unnamed_input_7(timer_irq),
        .top_unnamed_output(rdata),
        .top_unnamed_output_1(done),
        .top_unnamed_output_2(gpio_out),
        .top_unnamed_output_3(uart_tx)
    );

    // ⚠️ **时钟生成器**（漏掉 ⇒ 仿真无事件即结束 ⇒ 表现为「end at 0s」）
    always #5 clk = ~clk;

    // ── 断言任务 ────────────────────────────────────────────────────
    task automatic expect_bit(input logic got, input logic want);
        begin
            checks = checks + 1;
            if ($isunknown(got))   fails = fails + 1;
            else if (got !== want) fails = fails + 1;
        end
    endtask

    task automatic expect_eq8(input logic [7:0] got, input logic [7:0] want);
        begin
            checks = checks + 1;
            if (got !== want) fails = fails + 1;
        end
    endtask

    task automatic expect_eq32(input logic [31:0] got, input logic [31:0] want);
        begin
            checks = checks + 1;
            if (got !== want) fails = fails + 1;
        end
    endtask

    // ── 事务任务（**按已知延迟采样**：手推事务 ≤ 4 拍 ⇒ 取 16 拍余量）────────
    task automatic do_write(input logic [31:0] a, input logic [31:0] d);
        begin
            addr  = a;
            wdata = d;
            write = 1'b1;
            start = 1'b1;
            @(posedge clk); #1;      // ← AW/W 握手在这一拍完成
            start = 1'b0;
            write = 1'b0;
            repeat (16) @(posedge clk);
            #1;
        end
    endtask

    task automatic do_read(input logic [31:0] a);
        begin
            addr  = a;
            write = 1'b0;
            start = 1'b1;
            @(posedge clk); #1;      // ← AR 握手在这一拍完成
            start = 1'b0;
            repeat (16) @(posedge clk);
            #1;
        end
    endtask

    // ── UART 帧采样（8N1 · **LSB 先出** · bit 周期 16 拍）──────────────────
    // ⚠️ **做法 = 先逐拍采样成数组、再定位帧**（**不用**下降沿检测 —— 实测那有竞态：
    //   `uart_tx_d` 的采样延迟会让定位漂 1 拍，且不同编译之间结果不稳定）。
    // ⚠️ 定位：帧起点 = 数组里**第一个低电平**的样本（起始位 `0`），
    //   中点 = 起点 + 8；数据位 k 中点 = 起点 + 8 + 16×(k+1)；停止位 = 起点 + 8 + 16×9。
    //   ⇒ ⇒ **不需要知道事务的精确完成拍**（窗口从写脉冲之后开始，覆盖整个帧）。
    localparam int BIT_PERIOD = 16;
    localparam int WIN        = 240;   // 采样窗（写事务 + 160 拍帧 + 余量）

    logic       tx_samples [0:WIN-1];
    logic [7:0] exp_bits = 8'h5A;      // 手推：写 TX_DATA = 0x5A ⇒ 数据位 LSB 先出

    initial begin
        // ── 复位 ──────────────────────────────────────────────────────
        // ⚠️ **复位释放 MUST 在 negedge**（在 posedge 同刻拉低 ⇒ 该沿会采到新值
        //   ⇒ 复位拍被吃掉）—— 通用前提（期望源「通用推导前提」节）
        rst    = 1'b1;
        rst_n  = 1'b0;
        start  = 1'b0;
        write  = 1'b0;
        repeat (4) @(posedge clk);
        #1;
        @(negedge clk);
        rst   = 1'b0;
        rst_n = 1'b1;

        // ── 场景 ①：写 GPIO DATA_OUT（addr 0x00）⇒ gpio_out == 0xA5 ─────
        do_write(32'h00000000, 32'h000000A5);
        expect_eq8(gpio_out, 8'hA5);

        // ── 场景 ②：读 GPIO DATA_IN（addr 0x04）⇒ rdata == 0x0000005A ───
        do_read(32'h00000004);
        expect_eq32(rdata, 32'h0000005A);

        // ── 场景 ③：写 UART TX_DATA（addr 0x8000_0000）⇒ 8N1 帧 ─────────
        // ⚠️ 这个场景同时证明 **matrix.s1_* ↔ uart 的 19 根接线**是通的
        //   （写 TX_DATA 必须经 matrix 的 m0 → s1 路由）。
        // ⚠️ **采样窗 MUST 从「写脉冲」起算**（**实测**：帧紧贴写脉冲启动 ——
        //   matrix 的 m0→s1 路由是**组合**的 ⇒ ⇒ 写握手与帧起点相隔不到 1 拍；
        //   若像①②那样先等 16 拍再采 ⇒ **起始位已经过去** ⇒ 整帧错位）。
        addr  = 32'h80000000;
        wdata = 32'h0000005A;
        write = 1'b1;
        start = 1'b1;
        @(posedge clk); #1;
        start = 1'b0;
        write = 1'b0;
        // 逐拍采样整个窗口（**先采后判** ⇒ 无定位竞态）
        for (si = 0; si < WIN; si = si + 1) begin
            @(posedge clk); #1;
            tx_samples[si] = uart_tx;
        end
        // 定帧：第一个低电平样本 = 起始位起点
        first_low = -1;
        for (si = 0; si < WIN; si = si + 1) begin
            if (first_low < 0 && tx_samples[si] === 1'b0) first_low = si;
        end
        if (first_low < 0) begin
            expect_bit(1'b1, 1'b0);        // 窗口里没有起始位 ⇒ 无帧 ⇒ 必红
        end else begin
            expect_bit(tx_samples[first_low + 8], 1'b0);   // 起始位
            for (si = 0; si < 8; si = si + 1) begin
                expect_bit(tx_samples[first_low + 8 + BIT_PERIOD * (si + 1)],
                           exp_bits[si]);      // 数据位 k（LSB 先出）
            end
            expect_bit(tx_samples[first_low + 8 + BIT_PERIOD * 9], 1'b1);  // 停止位
        end

        // ── 场景 ④：rst_n 拉低 ⇒ GPIO 输出寄存器清零 ──────────────────
        // ⚠️ MUST 放最后（rst_n 拉低后整个 SoC 复位 ⇒ 后续事务不可用）
        @(negedge clk);
        rst_n = 1'b0;
        repeat (3) @(posedge clk);
        #1;
        expect_eq8(gpio_out, 8'h00);
        expect_bit(uart_tx, 1'b1);         // 回到空闲高

        $display("RESULT: checks=%0d fails=%0d", checks, fails);
        if (fails != 0) $fatal(1, "tb_axi_soc_example failed");
        $finish;
    end

endmodule
