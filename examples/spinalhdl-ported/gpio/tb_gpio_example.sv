// examples/spinalhdl-ported/gpio/tb_gpio_example.sv
//
// **独立 Verilog TB**（`examples-rtl-sim-coverage` E3）—— `gpio_example`
//
// ⚠️ **期望值出处（手推，禁抄仿真输出）**：
//   被测实现 `examples/spinalhdl-ported/gpio/gpio.cpp` 的 `GPIO`：
//   · 两级同步：`sync_stage1 ← gpio_in` · `sync_stage2 ← sync_stage1`
//   · 上升沿 `rise  = gpio_in & ~sync_stage2`（**逐位**与，不是「任一位上升」）
//   · 下降沿 `fall  = ~gpio_in & sync_stage2`
//   · `interrupt = interrupt_enable ? (rise|fall != 0) : 0`
//   · `output_reg ← gpio_in`（沿上采样 ⇒ 沿后 = 沿时的 gpio_in）
//   · `gpio_out = output_enable ? output_reg : 0`
//
//   ⇒ ⇒ **手推表**（沿后读；记 `s1` = sync_stage1、`s2` = sync_stage2）
//        场景 A：gpio_in=0x00 · 全程无变化
//          拍 1：s1=0x00 s2=0x00 ⇒ rise=0 fall=0 ⇒ interrupt=**0**
//        场景 B：gpio_in=0xFF（**上升**）
//          拍 1：s1=0xFF s2=0x00 ⇒ rise=0xFF ⇒ interrupt=**1**
//          拍 2：s2=0xFF ⇒ rise=0 fall=0 ⇒ interrupt=**0**
//        场景 C：gpio_in=0x00（**下降**）
//          拍 1：s1=0x00 s2=0xFF ⇒ fall=0xFF ⇒ interrupt=**1**
//          拍 2：s2=0x00 ⇒ fall=0 ⇒ interrupt=**0**
//        场景 D：output_enable=1 · gpio_in 0x00 → **0xA5**
//          拍 1：output_reg ← gpio_in(沿上采样) ⇒ gpio_out = **0xA5**
//                ⚠️ **同时这也是一次「上升沿」**（s2 仍是 0x00 ⇒
//                   rise = 0xA5 & ~0x00 = 0xA5 ≠ 0）⇒ interrupt = **1**
//                   ⇒ ⇒ **场景切换本身就是一次边沿** —— 手推时 MUST 计入！
//        场景 E：output_enable=0 · gpio_in 0xA5 → **0x5A**
//          拍 1：s2 = 0xA5 ⇒ ~s2 = 0x5A ⇒ rise = 0x5A & 0x5A = 0x5A ≠ 0
//                ⇒ interrupt = **1**；oe=0 ⇒ gpio_out = **0x00**（高阻用 0 模拟）
//        场景 F：interrupt_enable=0 ⇒ 中断恒 **0**（边沿仍发生，只是被屏蔽）
//
// ⚠️ **两个顶层输出组**：`top_unnamed_output`(gpio_out) 与
//   `top_gpio_inst_unnamed_output`（inline 后代）⇒ 同一根线 ⇒ 必须相等；
//   `top_unnamed_output_1`(interrupt) 与 `top_gpio_inst_unnamed_output_1` 同理。

`timescale 1ns/1ps

module tb_gpio_example;

    logic clk = 0;
    logic rst = 1;
    logic [7:0] gpio_in = 8'h00;       // top_unnamed_input
    logic       output_enable = 1'b0;  // top_unnamed_input_1
    logic       interrupt_enable = 1'b0;// top_unnamed_input_2
    logic [7:0] gpio_out;              // top_unnamed_output
    logic       interrupt;             // top_unnamed_output_1
    logic [7:0] gpio_inst_out;         // top_gpio_inst_unnamed_output
    logic       interrupt_inst;        // top_gpio_inst_unnamed_output_1

    integer checks = 0;
    integer fails  = 0;

    top dut (
        .default_clock(clk),
        .default_reset(rst),
        .top_unnamed_input(gpio_in),
        .top_unnamed_input_1(output_enable),
        .top_unnamed_input_2(interrupt_enable),
        .top_unnamed_output(gpio_out),
        .top_unnamed_output_1(interrupt),
        .top_gpio_inst_unnamed_output(gpio_inst_out),
        .top_gpio_inst_unnamed_output_1(interrupt_inst)
    );

    always #5 clk = ~clk;

    task automatic check_beat(input [7:0] want_out, input bit want_irq);
        begin
            @(posedge clk);
            #1;
            checks = checks + 1;
            if ($isunknown(gpio_out))       fails = fails + 1;
            else if (gpio_out !== want_out) fails = fails + 1;
            if ($isunknown(interrupt))      fails = fails + 1;
            else if (interrupt !== want_irq) fails = fails + 1;
            // inline 后代的输出必须与顶层一致（同一根线）
            if (gpio_inst_out !== gpio_out) fails = fails + 1;
            if (interrupt_inst !== interrupt) fails = fails + 1;
        end
    endtask

    initial begin
        // 复位脉冲（4 拍）—— ⚠️ MUST 在**下降沿**释放（避 posedge 采样竞态）
        rst = 1;
        repeat (4) @(posedge clk);
        @(negedge clk);
        rst = 0;

        // ── 场景 A：无变化 · 中断使能开 ────────────────────────────
        interrupt_enable = 1'b1;
        output_enable = 1'b0;
        gpio_in = 8'h00;
        check_beat(8'h00, 1'b0);

        // ── 场景 B：上升沿（0x00 → 0xFF）⇒ 一拍脉冲 ────────────────
        gpio_in = 8'hFF;
        check_beat(8'h00, 1'b1);   // s1=0xFF s2=0x00 ⇒ rise
        check_beat(8'h00, 1'b0);   // s2=0xFF ⇒ 无边沿

        // ── 场景 C：下降沿（0xFF → 0x00）⇒ 一拍脉冲 ────────────────
        gpio_in = 8'h00;
        check_beat(8'h00, 1'b1);   // s1=0x00 s2=0xFF ⇒ fall
        check_beat(8'h00, 1'b0);   // s2=0x00 ⇒ 无边沿

        // ── 场景 D：output_enable=1 ⇒ gpio_out = output_reg ───────
        //   ⚠️ gpio_in 0x00 → 0xA5 **本身就是一次上升沿** ⇒ irq = 1
        output_enable = 1'b1;
        gpio_in = 8'hA5;
        check_beat(8'hA5, 1'b1);   // output_reg ← gpio_in(沿上采样) · 且 rise ≠ 0

        // ── 场景 E：output_enable=0 ⇒ gpio_out = 0 ─────────────────
        //   ⚠️ 0xA5 → 0x5A 也是一次「上升」⇒ rise = 0x5A & ~0xA5 = 0x5A ≠ 0 ⇒ irq = 1
        output_enable = 1'b0;
        gpio_in = 8'h5A;
        check_beat(8'h00, 1'b1);

        // ── 场景 F：interrupt_enable=0 ⇒ 中断恒 0 ───────────────────
        interrupt_enable = 1'b0;
        gpio_in = 8'hFF;            // 制造一次上升
        check_beat(8'h00, 1'b0);

        $display("RESULT: checks=%0d fails=%0d", checks, fails);
        if (fails != 0) $fatal(1, "tb_gpio_example failed");
        $finish;
    end

endmodule
