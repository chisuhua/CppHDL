// examples/spinalhdl-ported/pwm/tb_pwm_example.sv
//
// **独立 Verilog TB**（`examples-rtl-sim-coverage` E3）—— `pwm_example`
//
// ⚠️ **期望值出处（手推，禁抄仿真输出）**：
//   被测实现 `examples/spinalhdl-ported/pwm/pwm.cpp`：
//   · `PWM<8>` 内部 `ch_reg<ch_uint<8>> counter(0_d)`（复位值 **0**）
//   · 每拍 `counter->next = counter + 1_d` ⇒ ⇒ **自由运行 8 位计数器**
//   · `less_than = (counter < duty_cycle)`
//   · `pwm_out = select(enable, less_than, false)`
//        ⇒ ⇒ **enable 为 0 ⇒ 输出恒 0**；**enable 为 1 ⇒ 输出 = (counter < duty)**
//   ⇒ ⇒ **手推表**（复位释放后的第 N 个 posedge 之后 ⇒ counter = N mod 256）：
//        duty_cycle = 4, enable = 1
//          N = 1, 2, 3   ⇒ counter < 4 ⇒ pwm = **1**
//          N = 4, 5, 6   ⇒ counter >= 4 ⇒ pwm = **0**
//        enable = 0 ⇒ pwm = **0**（无论 counter）
//        duty_cycle = 0 ⇒ pwm = **0**（counter < 0 恒假）
//
// ⚠️ **两个顶层输出**：`top_unnamed_output`（`PwmTop` 的 `pwm_out`）与
//   `top_pwm_inst_unnamed_output`（inline 后代 `PWM` 的 `pwm_out`）
//   ⇒ ⇒ 同一根线 ⇒ ⇒ 必须相等（也是一条判据）。
//
// ⚠️ **X 检查**：每个输出都查 `$isunknown`。

`timescale 1ns/1ps

module tb_pwm_example;

    logic clk = 0;
    logic rst = 1;
    logic       enable = 0;        // top_unnamed_input
    logic [7:0] duty_cycle = 0;   // top_unnamed_input_1
    logic       pwm_out;           // top_unnamed_output
    logic       pwm_inst_out;      // top_pwm_inst_unnamed_output

    integer checks = 0;
    integer fails  = 0;

    top dut (
        .default_clock(clk),
        .default_reset(rst),
        .top_unnamed_input(enable),
        .top_unnamed_input_1(duty_cycle),
        .top_unnamed_output(pwm_out),
        .top_pwm_inst_unnamed_output(pwm_inst_out)
    );

    always #5 clk = ~clk;

    // 一拍检查（沿后采样 ⇒ always_ff 已在沿上更新）
    task automatic check_beat(input bit want);
        begin
            @(posedge clk);
            #1;
            checks = checks + 1;
            if ($isunknown(pwm_out))      fails = fails + 1;
            else if (pwm_out !== want)    fails = fails + 1;
            if ($isunknown(pwm_inst_out)) fails = fails + 1;
            else if (pwm_inst_out !== want) fails = fails + 1;
        end
    endtask

    initial begin
        // 复位脉冲（4 拍）
        // ⚠️ MUST 在**下降沿**释放（避免与 always_ff 的 posedge 采样同刻竞争
        //   ⇒ 复位拍被吃掉 ⇒ 差整整一拍 —— 已在 counter 的 TB 里实测踩过）
        rst = 1;
        repeat (4) @(posedge clk);
        @(negedge clk);
        rst = 0;

        // ── 场景 1：enable=1 · duty=4 ⇒ 前 3 拍高、之后低 ──────────
        enable = 1;
        duty_cycle = 8'd4;
        check_beat(1'b1);   // counter = 1 < 4
        check_beat(1'b1);   // counter = 2 < 4
        check_beat(1'b1);   // counter = 3 < 4
        check_beat(1'b0);   // counter = 4 >= 4
        check_beat(1'b0);   // counter = 5 >= 4
        check_beat(1'b0);   // counter = 6 >= 4

        // ── 场景 2：enable=0 ⇒ 输出恒 0 ─────────────────────────────
        enable = 0;
        check_beat(1'b0);
        check_beat(1'b0);

        // ── 场景 3：enable=1 · duty=0 ⇒ counter < 0 恒假 ⇒ 输出 0 ───
        enable = 1;
        duty_cycle = 8'd0;
        check_beat(1'b0);
        check_beat(1'b0);

        // ── 场景 4：enable=1 · duty=255 ⇒ 只有 counter=255 时为 0 ───
        //   （counter 从场景 3 继续递增 ⇒ 这里只验证「高电平」这一侧）
        duty_cycle = 8'd255;
        check_beat(1'b1);
        check_beat(1'b1);

        $display("RESULT: checks=%0d fails=%0d", checks, fails);
        if (fails != 0) $fatal(1, "tb_pwm_example failed");
        $finish;
    end

endmodule
