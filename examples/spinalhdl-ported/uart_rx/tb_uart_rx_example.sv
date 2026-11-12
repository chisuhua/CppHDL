// examples/spinalhdl-ported/uart_rx/tb_uart_rx_example.sv
//
// EXPECT-SRC: .rddf/checks/examples-rtl-expectations.md#uart-rx-example
//
// **独立 Verilog TB**（`examples-rtl-sim-coverage` E3）—— `uart_rx_example`
//
// ⚠️ **期望源分级**：
//   · **spec-derived** = UART **8N1 帧格式** + **中点采样**（外部协议要求）：
//     接收器 MUST 在**位的中段**采样（否则真实串口的时序抖动会直接采错）
//     ⇒ ⇒ 收到的字节 MUST 等于发出的字节（LSB first）· `valid` MUST 是**单拍脉冲**
//   · **impl-contract** = 位周期 = `prescale + 1` 拍 · 采样点 = `div == prescale/2`
//     · `data` 的复位值 = 0 · `valid` 的复位值 = 0
//
// ⚠️ **采样相位的判别方法（关键）**：用**长位周期**（`prescale = 99` ⇒ 100 拍/位）
//   + **两次探针**，把采样点**夹在中间三分之一**：
//   · 探针 A：数据位 0 在**前 1/3** 为 1、其余为 0 ⇒ 采样点在中段 ⇒ MUST 读到 **0**
//   · 探针 B：数据位 0 在**前 2/3** 为 0、其余为 1 ⇒ 采样点在中段 ⇒ MUST 读到 **0**
//   ⇒ ⇒ 两次都读到 0 ⇒ 采样点 ∈ (1/3, 2/3) ⇒ ⇒ **中点采样**得证。
//   ⇒ ⇒ 旧实现（采样点**硬编码** `div == 4`）⇒ 采样点落在**位的前沿**（≈ 1/20）
//     ⇒ ⇒ 探针 A 会读到 **1** ⇒ ⇒ **红** ⇒ ⇒ 该判据对「采样点缩放」有判别力。
//
// ⚠️ **不判**「停止位错误 ⇒ 不产生 valid」⇒ ⇒ 本实现**不校验停止位**（framing check）
//   ⇒ ⇒ 那是**协议健壮性**要求，本 example 未实现 ⇒ ⇒ 判它 = 要求一个没有的能力
//   ⇒ ⇒ 已登记为已知偏离（见期望登记文档）。
//
// ⚠️ **inline 后代的输出也是顶层端口** ⇒ MUST 连接（否则严格 flags 下
//   `%Warning-PINMISSING` **致命** —— 实测踩过）；本 TB 顺带断言它们与顶层端口同源。

`timescale 1ns/1ps

module tb_uart_rx_example;

    logic clk = 0;
    logic rst = 1;

    logic        rx = 1'b1;          // top_unnamed_input
    logic [15:0] prescale = 16'd9;   // top_unnamed_input_1
    logic [7:0]  data;               // top_unnamed_output
    logic        valid;              // top_unnamed_output_1

    // ⚠️ inline 后代（`u`）的输出**也是顶层端口** ⇒ MUST 连接
    logic [7:0]  u_data;             // top_u_unnamed_output
    logic        u_valid;            // top_u_unnamed_output_1

    integer checks = 0;
    integer fails  = 0;

    // ── `valid` 上升沿监视器（module 级 ⇒ 避免 `%Warning-IMPLICITSTATIC`）────
    integer cyc = 0;
    integer vrise_cyc  [0:63];
    logic [7:0] vrise_data [0:63];
    integer vcount = 0;              // valid 上升沿次数
    integer vhigh_cycles = 0;        // valid 为高的**总拍数**（⇒ 单拍脉冲的判据）
    logic   vhigh = 1'b0;

    // ── DUT 实例（⚠️ 写 TB 时曾两次漏掉 ⇒ 一次「无时钟」、一次「悬空输出」⇒ ⇒
    //   ⇒ 现象分别是「end at 0s」与「判据全红」⇒ ⇒ **两者 MUST 成对出现**
    //   ⇒ ⇒ 下面这段 + 时钟生成器构成 TB 的**最小骨架**）
    top dut (
        .default_clock(clk),
        .default_reset(rst),
        .top_unnamed_input(rx),
        .top_unnamed_input_1(prescale),
        .top_unnamed_output(data),
        .top_unnamed_output_1(valid),
        .top_u_unnamed_output(u_data),
        .top_u_unnamed_output_1(u_valid)
    );

    // ⚠️ **时钟生成器**（漏掉 ⇒ 仿真无事件即结束 ⇒ 表现为「end at 0s」）
    always #5 clk = ~clk;

    always @(posedge clk) begin
        if (rst) begin
            cyc = 0;
            vcount = 0;
            vhigh_cycles = 0;
            vhigh = 1'b0;
        end else begin
            if (valid === 1'b1) begin
                vhigh_cycles = vhigh_cycles + 1;
                if (!vhigh) begin
                    vrise_cyc[vcount] = cyc;
                    vrise_data[vcount] = data;
                    vcount = vcount + 1;
                end
                vhigh = 1'b1;
            end else begin
                vhigh = 1'b0;
            end
            cyc = cyc + 1;
        end
    end

    task automatic expect_bit(input logic got, input logic want);
        begin
            checks = checks + 1;
            if ($isunknown(got))   fails = fails + 1;
            else if (got !== want) fails = fails + 1;
        end
    endtask

    // ⚠️ 字节比较 MUST 用 **8 位**形参（`integer` 是 32 位 ⇒ 与 8 位实参比较会触发
    //   `%Warning-WIDTHEXPAND` ⇒ 严格 flags 下**致命** —— 实测踩过）
    task automatic expect_eq8(input logic [7:0] got, input logic [7:0] want);
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

    // ── 发送一帧 8N1 ───────────────────────────────────────────────────
    //   `period` = 位周期（拍）= prescale + 1
    //   `flip_at` ≥ 0 ⇒ 数据位 0 在前 `flip_at` 拍为 `flip_val`、其余为其反值
    //     （用于把采样点**夹**在某个区间 —— 探针用）
    //   `gap` = 帧后额外空闲拍数
    task automatic send_frame(input logic [7:0] payload, input integer period,
                              input integer flip_at, input logic flip_val,
                              input integer gap);
        begin
            rx = 1'b0;                                  // 起始位
            repeat (period) @(posedge clk);
            for (int i = 0; i < 8; i = i + 1) begin
                if ((i == 0) && (flip_at >= 0)) begin
                    rx = flip_val;
                    repeat (flip_at) @(posedge clk);
                    rx = ~flip_val;
                    repeat (period - flip_at) @(posedge clk);
                end else begin
                    rx = payload[i];
                    repeat (period) @(posedge clk);
                end
            end
            rx = 1'b1;                                  // 停止位
            repeat (period) @(posedge clk);
            repeat (gap) @(posedge clk);
        end
    endtask

    integer mark = 0;      // 帧数标记
    integer i = 0;         // 临时循环变量
    integer idx = 0;       // 临时索引

    initial begin
        // ── 复位 ─────────────────────────────────────────────────────
        rst = 1'b1;
        repeat (4) @(posedge clk);
        @(negedge clk);
        rst = 1'b0;
        repeat (2) @(posedge clk);

        // ── S1：复位态（**impl-contract**：`valid`/`data` 的复位值）──
        expect_bit(valid, 1'b0);
        expect_eq8(data, 8'h00);
        // inline 后代端口与顶层端口同源
        expect_eq8(u_data, data);
        expect_bit(u_valid, valid);

        // ══════════════════════════════════════════════════════════════
        // 阶段 1：`prescale = 9`（位周期 10 拍）⇒ 收字节
        // ══════════════════════════════════════════════════════════════
        prescale = 16'd9;

        // ── S2：**spec-derived**：收到的字节 MUST 等于发出的字节 ──────
        //   ⚠️ **测试值 MUST 避开「位回文」**（如 0x5A / 0xC3 反转后与自身相同）
        //   ⇒ ⇒ 旧实现（解出的字节**位反转**）在回文值上**恰好通过** ⇒ ⇒ 掩盖缺陷
        //   ⇒ ⇒ 这里用 **0x96 / 0x69**（互为反转 ⇒ 判别力最强）
        send_frame(8'h96, 10, -1, 1'b0, 20);
        expect_eq(vcount, 1);
        if (vcount >= 1) expect_eq8(vrise_data[0], 8'h96);

        send_frame(8'h69, 10, -1, 1'b0, 20);
        expect_eq(vcount, 2);
        if (vcount >= 2) expect_eq8(vrise_data[1], 8'h69);

        // ── S3：**metamorphic**：连发 4 字节 ⇒ 收到的序列 MUST **逐项恒等** ──
        //   ⇒ 比较对象 = TB 侧记录的**发送值**（不是写死的期望常量）⇒ ⇒ 相对判据
        begin
            logic [7:0] sent [0:3];
            integer base_cnt;
            base_cnt = vcount;
            for (i = 0; i < 4; i = i + 1) begin
                sent[i] = 8'h10 + i[7:0];
                send_frame(sent[i], 10, -1, 1'b0, 20);
            end
            expect_eq(vcount - base_cnt, 4);
            for (idx = 0; idx < 4; idx = idx + 1) begin
                if (vcount - base_cnt > idx) begin
                    expect_eq8(vrise_data[base_cnt + idx], sent[idx]);   // **相对**比较
                end
            end
        end

        // ── S4：**metamorphic**：`valid` MUST 是**单拍脉冲** ──────────
        //   ⇒ valid 为高的**总拍数** MUST == 收到的字节数（每个脉冲恰好 1 拍）
        expect_eq(vhigh_cycles, vcount);

        // ── S5：**impl-contract**：帧间隔 = 10 × 位周期 ──────────────
        //   ⇒ 背靠背发两帧 ⇒ 两个 valid 上升沿的间隔 MUST == 10 × 10 = 100 拍
        //   ⇒ ⇒ **相对**判据（随 prescale 缩放，见 S6）
        begin
            integer base_cnt;
            integer gap_cyc;
            base_cnt = vcount;
            send_frame(8'h11, 10, -1, 1'b0, 0);   // 帧 N
            send_frame(8'h22, 10, -1, 1'b0, 0);   // 帧 N+1（背靠背）
            expect_eq(vcount - base_cnt, 2);
            if (vcount - base_cnt >= 2) begin
                gap_cyc = vrise_cyc[base_cnt + 1] - vrise_cyc[base_cnt];
                expect_eq(gap_cyc, 100);
            end
        end

        // ══════════════════════════════════════════════════════════════
        // 阶段 2：`prescale = 99`（位周期 100 拍）⇒ **采样相位**探针
        // ══════════════════════════════════════════════════════════════
        prescale = 16'd99;

        // ── S6（**spec-derived**：中点采样）：探针 A ────────────────
        //   数据位 0 在**前 1/3**（33 拍）为 1、其余为 0
        //   ⇒ 采样点在中段 ⇒ MUST 读到 **0**
        mark = vcount;
        send_frame(8'h00, 100, 33, 1'b1, 100);
        expect_eq(vcount - mark, 1);
        if (vcount - mark >= 1) begin
            // 收到的字节的 bit0 MUST 是 0（采样点不在前 1/3）
            expect_bit(vrise_data[mark][0], 1'b0);
        end

        // ── S7（**spec-derived**：中点采样）：探针 B ────────────────
        //   数据位 0 在**前 2/3**（66 拍）为 0、其余为 1
        //   ⇒ 采样点在中段 ⇒ MUST 读到 **0**（还没翻转）
        mark = vcount;
        send_frame(8'h00, 100, 66, 1'b0, 100);
        expect_eq(vcount - mark, 1);
        if (vcount - mark >= 1) begin
            expect_bit(vrise_data[mark][0], 1'b0);
        end

        // ── S8（**spec-derived**：中点采样）：探针 A′（**反向**对照）──
        //   数据位 0 在**前 1/3** 为 0、其余为 1 ⇒ 采样点在中段（**晚于**翻转点）
        //   ⇒ MUST 读到 **1**（读到翻转后的值 ⇒ ⇒ 证明「翻转确实生效」）
        //   ⇒ 三个探针合起来的夹逼：
        //      A  （前 1/3 为 1）⇒ 读到 0 ⇒ 采样点**不在**前 1/3
        //      B  （前 2/3 为 0）⇒ 读到 0 ⇒ 采样点**不在**后 1/3
        //      A′ （前 1/3 为 0）⇒ 读到 1 ⇒ 采样点**晚于**前 1/3（与 A 互为印证）
        //      ⇒ ⇒ 采样点 ∈ [1/3, 2/3) ⇒ ⇒ **中点采样**得证
        mark = vcount;
        send_frame(8'h00, 100, 33, 1'b0, 100);
        expect_eq(vcount - mark, 1);
        if (vcount - mark >= 1) begin
            expect_bit(vrise_data[mark][0], 1'b1);
        end

        $display("RESULT: checks=%0d fails=%0d", checks, fails);
        if (fails != 0) $fatal(1, "tb_uart_rx_example failed");
        $finish;
    end

endmodule
