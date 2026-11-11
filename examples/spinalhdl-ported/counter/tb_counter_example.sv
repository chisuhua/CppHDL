// examples/spinalhdl-ported/counter/tb_counter_example.sv
//
// **独立 Verilog TB**（`examples-rtl-sim-coverage` E3）—— `counter_example`
//
// ⚠️ **期望值出处（手推，禁抄仿真输出）**：
//   被测实现 `examples/spinalhdl-ported/counter/counter.cpp` 的 `Counter<8>`：
//   · `counter_reg` 复位值 **0**
//   · `next = select(clear, 0, select(enable, counter_reg + 1, counter_reg))`
//        ⇒ ⇒ **clear 优先**；否则 **enable ⇒ +1**；否则 **保持**
//   · `counter_reg + 1` 是 **9 位**（8+1）⇒ ⇒ 写回 8 位寄存器时**截断**
//        ⇒ ⇒ **255 + 1 ⇒ 0**（模 256 回绕）
//   ⇒ ⇒ **手推表**（沿后读）：
//        enable=0 clear=0 ⇒ **保持**
//        enable=1 clear=0 ⇒ **+1**（模 256）
//        enable=1 clear=1 ⇒ **0**（clear 优先于 enable）
//        enable=0 clear=1 ⇒ **0**
//
// ⚠️ **关于 `counter.cpp:211-214` 的自认注释**：
//   那条注释（「enable/clear may not fully work yet」）出现在 example 的 **C++
//   interpreter** 仿真路径里。**RTL 路径实测全部正常**（enable/clear/保持/回绕
//   四项都对）⇒ ⇒ 那条注释对 **RTL** 是**过时**的。
//   ⇒ ⇒ ⚠️ **这意味着同一个 example 的 interpreter 路径与 RTL 路径可能分叉**
//   ⇒ ⇒ 且 examples 的 C++ 侧**无任何断言** ⇒ ⇒ 分叉不会被任何测试发现
//   ⇒ ⇒ 已登记为独立待立项项（**不在** B2 范围）。
//
// ⚠️ **两个顶层输出**：`top_unnamed_output`（`Top` 的 value）与
//   `top_counter1_unnamed_output`（inline 后代）⇒ 同一根线 ⇒ 必须相等。

`timescale 1ns/1ps

module tb_counter_example;

    logic clk = 0;
    logic rst = 1;
    logic       enable = 1'b0;   // top_unnamed_input
    logic       clear  = 1'b0;   // top_unnamed_input_1
    logic [7:0] value;           // top_unnamed_output
    logic [7:0] counter_value;    // top_counter1_unnamed_output

    integer checks = 0;
    integer fails  = 0;

    top dut (
        .default_clock(clk),
        .default_reset(rst),
        .top_unnamed_input(enable),
        .top_unnamed_input_1(clear),
        .top_unnamed_output(value),
        .top_counter1_unnamed_output(counter_value)
    );

    always #5 clk = ~clk;

    task automatic check_beat(input [7:0] want);
        begin
            @(posedge clk);
            #1;
            checks = checks + 1;
            if ($isunknown(value))        fails = fails + 1;
            else if (value !== want)      fails = fails + 1;
            // ⚠️ **不再断言** `counter_value == value`：两者在产物里是
            //   `assign top_unnamed_output = top_counter1_unnamed_output;` 的
            //   同一条线 ⇒ ⇒ 断言**恒真** ⇒ 零信息量，且把「inline 后代复制出
            //   同名端口」这个**实现产物**固化成契约（metis 指出的 slop 形态）。
            //   ⇒ ⇒ 端口存在性由「编译通过」保证（E1 已有同类判据）。
        end
    endtask

    initial begin
        // 复位脉冲（4 拍）—— ⚠️ MUST 在**下降沿**释放（避 posedge 采样竞态）
        rst = 1;
        repeat (4) @(posedge clk);
        @(negedge clk);
        rst = 0;

        // ── 场景 1：enable=0 · clear=0 ⇒ **保持** 0 ─────────────────
        enable = 1'b0;
        clear  = 1'b0;
        check_beat(8'd0);
        check_beat(8'd0);
        check_beat(8'd0);

        // ── 场景 2：enable=1 ⇒ **+1** ───────────────────────────────
        enable = 1'b1;
        check_beat(8'd1);
        check_beat(8'd2);
        check_beat(8'd3);

        // ── 场景 3：enable=0 ⇒ **保持** 3 ───────────────────────────
        enable = 1'b0;
        check_beat(8'd3);
        check_beat(8'd3);

        // ── 场景 4：clear=1 ⇒ **0**（且 **clear 优先于 enable**）────
        clear  = 1'b1;
        enable = 1'b1;          // ← clear 与 enable 同时为 1 ⇒ clear 优先
        check_beat(8'd0);
        check_beat(8'd0);

        // ── 场景 5：clear=0 · enable=1 ⇒ 从 0 继续 +1 ─────────────
        clear  = 1'b0;
        check_beat(8'd1);
        check_beat(8'd2);

        // ── 场景 6：**溢出回绕**（255 → 0）────────────────────────
        //   直接把计数推到 255：连续 253 拍 +1（当前 2 ⇒ 2+253 = 255）
        for (int i = 0; i < 253; i = i + 1) begin
            @(posedge clk);
        end
        #1;
        checks = checks + 1;
        if ($isunknown(value))   fails = fails + 1;
        else if (value !== 8'd255) fails = fails + 1;
        // 再一拍 ⇒ 255 + 1 = 256 ⇒ 截断到 8 位 ⇒ **0**
        check_beat(8'd0);

        $display("RESULT: checks=%0d fails=%0d", checks, fails);
        if (fails != 0) $fatal(1, "tb_counter_example failed");
        $finish;
    end

endmodule
