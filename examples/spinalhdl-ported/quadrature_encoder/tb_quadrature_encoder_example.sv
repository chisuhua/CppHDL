// examples/spinalhdl-ported/quadrature_encoder/tb_quadrature_encoder_example.sv
//
// EXPECT-SRC: .rddf/checks/examples-rtl-expectations.md#quadrature-encoder-example
//
// **独立 Verilog TB**（`examples-rtl-sim-coverage` E3）—— `quadrature_encoder_example`
//
// ⚠️ **期望值出处（spec-derived）**：正交编码器的**外部语义**（不是实现表达式）：
//   · **4 倍频计数**：A/B 相的**每个边沿**都计数 ⇒ ⇒ **一个完整电周期 = 4 个计数**
//   · **方向**约定（该 example 头部原文）：`A 领先 B = CW` / `B 领先 A = CCW`
//   · **Z 相（索引）上升沿 ⇒ 位置清零**
//   ⇒ ⇒ 因此本 TB 用**语义级**判据（**不逐拍抄中间值**）：
//        正转 N 个完整周期 ⇒ `position` 变化 **+4N**
//        正转 N 后反转 N ⇒ `position` **回到原点**（metamorphic）
//        Z 上升 ⇒ `position == 0`
//
// ⚠️ **已知陷阱（impl-contract）**：四个同步寄存器的**初值是 `true`**
//   （`a_sync1(true)` / `a_sync2(true)` / `b_sync1(true)` / `b_sync2(true)`）
//   ⇒ ⇒ 静止初态是 **A=1 · B=1** ⇒ ⇒ TB 的输入初值 MUST 也是 **1**，
//   否则第一个拍就会造出一个**启动伪边沿**（实测最易错点）。
//
// ⚠️ **同步延迟（impl-contract）**：边沿判定用 `sync1/sync2`（2 拍延迟），
//   且 `count_up/down` 混用 `sync2` ⇒ ⇒ 每个相位 MUST 维持 **≥4 拍**
//   才能让边沿被检测**且只检测一次**（本 TB 用 4 拍）。

`timescale 1ns/1ps

module tb_quadrature_encoder_example;

    logic clk = 0;
    logic rst = 1;
    logic        enc_a = 1'b1;   // ⚠️ 与同步寄存器初值一致（见文件头「已知陷阱」）
    logic        enc_b = 1'b1;
    logic        enc_z = 1'b0;
    logic [15:0] position;       // top_unnamed_output
    logic        direction;      // top_unnamed_output_1
    logic        a_sync;         // top_unnamed_output_2
    logic        b_sync;         // top_unnamed_output_3
    // ⚠️ **inline 后代的 4 个输出也是顶层端口** ⇒ MUST 连接
    //   （否则严格 flags 下 `%Warning-PINMISSING` **致命** —— 实测踩过）
    logic [15:0] enc_position;   // top_enc_unnamed_output
    logic        enc_direction;  // top_enc_unnamed_output_1
    logic        enc_a_sync;     // top_enc_unnamed_output_2
    logic        enc_b_sync;     // top_enc_unnamed_output_3

    integer checks = 0;
    integer fails  = 0;
    logic [15:0] pos0 = 16'd0;

    top dut (
        .default_clock(clk),
        .default_reset(rst),
        .top_unnamed_input(enc_a),
        .top_unnamed_input_1(enc_b),
        .top_unnamed_input_2(enc_z),
        .top_unnamed_output(position),
        .top_unnamed_output_1(direction),
        .top_unnamed_output_2(a_sync),
        .top_unnamed_output_3(b_sync),
        .top_enc_unnamed_output(enc_position),
        .top_enc_unnamed_output_1(enc_direction),
        .top_enc_unnamed_output_2(enc_a_sync),
        .top_enc_unnamed_output_3(enc_b_sync)
    );

    always #5 clk = ~clk;

    // 维持一个相位 n 拍（n ≥ 4 ⇒ 覆盖 2 拍同步延迟 + 边沿只触发一次）
    task automatic phase(input logic a, input logic b, input integer n);
        begin
            enc_a = a;
            enc_b = b;
            repeat (n) @(posedge clk);
        end
    endtask

    // ⚠️ 参数用 **`[15:0]`**（不是 `integer`）：`integer` 是 32 位 ⇒
    //   与 16 位 `position` 比较会触发 `%Warning-WIDTHEXPAND` ⇒ 严格 flags 下**致命**
    task automatic expect_position(input [15:0] want);
        begin
            checks = checks + 1;
            if ($isunknown(position))          fails = fails + 1;
            else if (position !== want)        fails = fails + 1;
        end
    endtask

    // ⚠️ **符号无关的增量判据**（`spec-derived` 只规定**幅值**）：
    //   正交编码器的规范只保证「**一个完整电周期 = 4 个计数**」
    //   （A 领先 B 的**方向名**是 CW，但「CW 对应 +1 还是 -1」取决于计数器的
    //    符号约定 ⇒ ⇒ 属 **impl-contract**，本 TB **不硬判符号**）。
    //   ⇒ ⇒ 判据 = `(position - base) mod 65536` ∈ {+magnitude, -magnitude}
    task automatic expect_delta_magnitude(input [15:0] base, input [15:0] magnitude);
        logic [15:0] d;
        begin
            d = position - base;
            checks = checks + 1;
            if ($isunknown(position))                         fails = fails + 1;
            else if (d !== magnitude && d !== (16'd0 - magnitude)) fails = fails + 1;
        end
    endtask

    initial begin
        // 复位脉冲（4 拍）—— ⚠️ MUST 在**下降沿**释放（避 posedge 采样竞态）
        rst = 1;
        repeat (4) @(posedge clk);
        @(negedge clk);
        rst = 0;

        // 静止 4 拍（让同步链稳定在 A=1 / B=1）
        phase(1'b1, 1'b1, 4);
        #1;
        pos0 = position;

        // ── ① spec-derived 绝对判据：复位后位置 = 0 ─────────────────
        expect_position(16'd0);

        // ── ② 正转 2 个完整周期 ⇒ **+8** ────────────────────────────
        //   序列 11 → 10 → 00 → 01 → 11（每转移 1 个边沿 ⇒ 每周期 4 个）
        repeat (2) begin
            phase(1'b1, 1'b0, 4);
            phase(1'b0, 1'b0, 4);
            phase(1'b0, 1'b1, 4);
            phase(1'b1, 1'b1, 4);
        end
        phase(1'b1, 1'b1, 4);   // 静置让同步链清空
        #1;
        expect_delta_magnitude(pos0, 16'd8);   // **2 周期 ⇒ 8 个计数**（符号无关）

        // ── ③ metamorphic：反转 2 个完整周期 ⇒ **回到原点** ─────────
        //   序列 11 → 01 → 00 → 10 → 11（反向 ⇒ 每转移 -1）
        repeat (2) begin
            phase(1'b0, 1'b1, 4);
            phase(1'b0, 1'b0, 4);
            phase(1'b1, 1'b0, 4);
            phase(1'b1, 1'b1, 4);
        end
        phase(1'b1, 1'b1, 4);
        #1;
        expect_position(pos0);

        // ── ④ spec-derived：**Z 相上升沿 ⇒ 位置清零** ───────────────
        //   先把位置推走（正转 1 个周期 ⇒ +4）⇒ 再打 Z
        phase(1'b1, 1'b0, 4);
        phase(1'b0, 1'b0, 4);
        phase(1'b0, 1'b1, 4);
        phase(1'b1, 1'b1, 4);
        phase(1'b1, 1'b1, 4);
        #1;
        expect_delta_magnitude(pos0, 16'd4);   // 1 周期 ⇒ 4 个计数，确认此刻确实非 0

        enc_z = 1'b1;
        repeat (4) @(posedge clk);
        enc_z = 1'b0;
        repeat (4) @(posedge clk);
        #1;
        expect_position(16'd0);          // Z 上升沿已清零

        // ── ⑤ impl-contract：`a_sync` / `b_sync` 是 A/B 的**2 拍延迟** ──
        //   （静止在 A=1 / B=1 ⇒ 两者都应为 1）
        checks = checks + 1;
        if ($isunknown(a_sync))    fails = fails + 1;
        else if (a_sync !== 1'b1)  fails = fails + 1;
        checks = checks + 1;
        if ($isunknown(b_sync))    fails = fails + 1;
        else if (b_sync !== 1'b1)  fails = fails + 1;

        $display("RESULT: checks=%0d fails=%0d", checks, fails);
        if (fails != 0) $fatal(1, "tb_quadrature_encoder_example failed");
        $finish;
    end

endmodule
