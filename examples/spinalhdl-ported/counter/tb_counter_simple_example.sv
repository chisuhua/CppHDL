// examples/spinalhdl-ported/counter/tb_counter_simple_example.sv
//
// **独立 Verilog TB**（`examples-rtl-sim-coverage` E3）—— `counter_simple_example`
//
// EXPECT-SRC: .rddf/checks/examples-rtl-expectations.md#counter-simple-example
//
// ⚠️ **期望值出处** = 上面那份登记（**spec-derived**）：语义取自 **SpinalHDL 上游
//   Scala 原文**（`RegInit(0)` + `counterReg := counterReg + 1` + `io.value := counterReg`）
//   ⇒ ⇒ **不是**从被测 `.cpp` 的实现表达式反推（那是转录，不是推导）。
//   ⇒ ⇒ **手推表**（复位释放后的第 N 个 posedge 之后）：
//        N = 1..255 ⇒ N · N = 256 ⇒ **0**（8 位模 256 回绕）
//
// **metamorphic（相对判据，不依赖绝对值）**：
//   · **周期 256**：`v[n] == v[n+256]` ⇒ ⇒ 本 TB 跑 512 拍，后 256 拍只比相对值。
//
// ⚠️ **X 检查**：每个输出都查 `$isunknown`
//   ⇒ ⇒ ⚠️ **但它在本 runner 下恒 false**（Verilator 把 X 优化成已知值）
//   ⇒ ⇒ 保留是为对齐 E3 的「TB MUST 断言 `!==x`」约定，**不是**有效判据。

`timescale 1ns/1ps

module tb_counter_simple_example;

    logic clk = 0;
    logic rst = 1;
    logic [7:0] top_value;          // Top 自己的 value
    logic [7:0] top_counter_value;   // inline 后代 Counter 的 value

    integer checks = 0;
    integer fails  = 0;
    // ⚠️ 记录第一轮的**实测值** ⇒ ⇒ 第二轮与它比较（**真** metamorphic）
    logic [7:0] recorded [0:255];

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

        // ── ① spec-derived 绝对判据：第 N 拍 ⇒ N mod 256 ────────────
        for (int n = 1; n <= 256; n = n + 1) begin
            check_beat(n);
        end

        // ── ② metamorphic 相对判据：**周期 256** ────────────────────
        //   ⚠️ 旧写法（**已修**）第二轮又断言 `top_value == n[7:0]`（绝对值）
        //     ⇒ ⇒ 那**依赖第一轮的绝对期望**，第一轮期望若写错它也跟着错
        //     ⇒ ⇒ 不是「相对判据」。
        //   ⇒ ⇒ 现在：**第一轮把实测值记进数组**，第二轮与**记录的实测值**比
        //     ⇒ ⇒ 完全独立于绝对期望 ⇒ ⇒ 抓「周期不是 256」这类漂移。
        for (int n = 1; n <= 256; n = n + 1) begin
            @(posedge clk);
            #1;
            recorded[n - 1] = top_value;   // 记录第一轮实测
            checks = checks + 1;
            if ($isunknown(top_value)) fails = fails + 1;
            else if (top_value !== n[7:0]) fails = fails + 1;
        end
        for (int n = 1; n <= 256; n = n + 1) begin
            @(posedge clk);
            #1;
            checks = checks + 1;
            if ($isunknown(top_value))                    fails = fails + 1;
            else if (top_value !== recorded[n - 1])        fails = fails + 1;  // **相对**
        end

        $display("RESULT: checks=%0d fails=%0d", checks, fails);
        if (fails != 0) $fatal(1, "tb_counter_simple_example failed");
        $finish;
    end

endmodule
