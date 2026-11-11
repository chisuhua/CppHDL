// examples/spinalhdl-ported/counter/tb_counter_simple_example.sv
//
// **独立 Verilog TB**（`examples-rtl-sim-coverage` E3）—— `counter_simple_example`
//
// ⚠️ **期望值出处（手推，禁抄仿真输出）**：
//   被测实现 `examples/spinalhdl-ported/counter/counter_simple.cpp`：
//   · `Counter` 内部 `ch_reg<ch_uint<8>> counter_reg(0_d)`（复位值 **0**）
//   · 每拍 `counter_reg->next = counter_reg + 1_d` ⇒ ⇒ **自由运行 +1 计数器**
//   · `Top` 把 `Counter` 的 `value` 直接冒泡到顶层
//   ⇒ ⇒ **手推表**（复位释放后的第 N 个 posedge 之后）：
//        N = 1   ⇒ 1
//        N = 2   ⇒ 2
//        N = 3   ⇒ 3
//        N = 10  ⇒ 10
//        N = 255 ⇒ 255
//        N = 256 ⇒ 0        （8 位模 256 回绕）
//   ⇒ ⇒ 依据 = **实现的算术语义**（不是「跑一遍看输出」）
//
// ⚠️ **两个顶层输出**：`top_unnamed_output`（`Top` 自己的 `value`）与
//   `top_counter1_unnamed_output`（inline 后代 `Counter` 的 `value`）
//   ⇒ ⇒ 它们**必须相等**（同一根线）⇒ ⇒ 这也是一条判据。
//
// ⚠️ **X 检查**：每个输出都查 `$isunknown` ⇒ 未驱动时必红。

`timescale 1ns/1ps

module tb_counter_simple_example;

    logic clk = 0;
    logic rst = 1;
    logic [7:0] top_value;          // Top 自己的 value
    logic [7:0] top_counter_value;   // inline 后代 Counter 的 value

    integer checks = 0;
    integer fails  = 0;

    top dut (
        .default_clock(clk),
        .default_reset(rst),
        .top_unnamed_output(top_value),
        .top_counter1_unnamed_output(top_counter_value)
    );

    always #5 clk = ~clk;

    // ⚠️ **逐拍检查**（不是「跳到第 N 拍再查一次」）：
    //   ⚠️ 实测踩坑：先写成 `expect_value(n, want)`（task 内只等 **1** 拍，
    //     期望值却按「第 n 拍」写）⇒ ⇒ 期望值与实际拍数**错位**
    //     ⇒ ⇒ 实测 `fails=6`（后 3 拍全错）⇒ ⇒ 而这恰恰说明**期望值必须手推**：
    //     手推表写对、拍数对应不上时，红的是 TB 而不是实现。
    //   ⇒ ⇒ 现在**每一拍**都断言「第 N 拍 ⇒ 值 == N mod 256」。
    task automatic check_beat(input integer n);
        begin
            @(posedge clk);
            #1;   // 采样**沿后**的值（always_ff 在沿上更新 ⇒ 沿后读才是新值）
            checks = checks + 1;
            if ($isunknown(top_value))           fails = fails + 1;
            else if (top_value !== n[7:0])        fails = fails + 1;
            // ⚠️ **不断言** `top_counter_value == top_value`：两者是
            //   `assign top_unnamed_output = top_counter1_unnamed_output;` 的同一条线
            //   ⇒ ⇒ 恒真 ⇒ 零信息量（metis 指出的 slop 形态）
        end
    endtask

    initial begin
        // 复位脉冲（4 拍）
        // ⚠️ ⚠️ **必须在时钟的【下降沿】释放复位**（实测踩坑）：
        //   若在 `@(posedge clk)` 之后**同一时刻**（0 delta）把 rst 拉低，
        //   该沿的 `always_ff` 采样会看到**新的** rst 值 ⇒ ⇒ 复位拍被吃掉
        //   ⇒ ⇒ 计数从 2 开始（实测：手推 1、实测 2）⇒ ⇒ 差整整一拍。
        //   ⇒ ⇒ 下降沿释放 ⇒ 远离 posedge ⇒ ⇒ 时序唯一确定。
        rst = 1;
        repeat (4) @(posedge clk);
        @(negedge clk);
        rst = 0;

        // ── 手推表（逐拍）────────────────────────────────────────────
        //   第 N 拍（复位释放后）⇒ 值 == N mod 256 ⇒ ⇒ 跑满 256 拍覆盖回绕
        for (int n = 1; n <= 256; n = n + 1) begin
            check_beat(n);
        end

        $display("RESULT: checks=%0d fails=%0d", checks, fails);
        if (fails != 0) $fatal(1, "tb_counter_simple_example failed");
        $finish;
    end

endmodule
