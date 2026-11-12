// examples/spinalhdl-ported/uart/tb_uart_tx_example.sv
//
// EXPECT-SRC: .rddf/checks/examples-rtl-expectations.md#uart-tx-example
//
// **独立 Verilog TB**（`examples-rtl-sim-coverage` E3）—— `uart_tx_example`
//
// ⚠️ **期望源分级**：
//   · **spec-derived** = UART **8N1 帧格式**（外部协议）：空闲高电平 · 起始位 1 位低
//     · 8 个数据位 **LSB first** · 停止位 1 位高 · 共 10 位
//   · **impl-contract** = **位宽 434 拍**（`baud_counter == 433` ⇒ 0..433 共 434 拍）
//     · `busy` 的覆盖范围（START+DATA 为 1 · IDLE/STOP 为 0 · 见源码）
//     · `frame_start` 需**恰好 1 拍脉冲**（内部锁存器 `start_latch` ⇒ 1 拍延迟）
//
// ⚠️ **本 TB 的测量方法（关键）**：位宽是**测出来的**，不是假设的 ——
//   ⇒ 用**边沿时间戳**测量 ⇒ ⇒ 若实现的位宽变了，判据自动跟随（不会误红/漏红）。
//   ⇒ ⇒ 为此第一帧选 **0x55（0101_0101 ⇒ 交替位）** ⇒ **每个位边界都有边沿**
//     ⇒ 9 个可测段（起始位 + 8 个数据位）的宽度全部可直接测得。
//   ⇒ ⇒ 第二帧选 **0x5A（0101_1010）** ⇒ 存在**相邻同值位** ⇒ 边界无边沿
//     ⇒ ⇒ 改用「按**测得**的位宽在每个位格**中点采样**」来解码 ⇒ 抓位序错。
//
// ⚠️ **停止位的宽度**在本接口上**不可观测**（STOP 与 IDLE 的 `uart_tx` 都是 1 ⇒
//   停止位结束处**没有边沿**）⇒ ⇒ 本 TB **不判**停止位宽度，
//   只判「停止位存在且为高」（在停止位中点采样）。
//
// ⚠️ **inline 后代的输出也是顶层端口** ⇒ MUST 连接（否则严格 flags 下
//   `%Warning-PINMISSING` **致命** —— 实测踩过）；本 TB 顺带断言它们与顶层端口同源。

`timescale 1ns/1ps

module tb_uart_tx_example;

    logic clk = 0;
    logic rst = 1;

    logic       frame_start = 1'b0;   // top_unnamed_input
    logic [7:0] data = 8'h00;         // top_unnamed_input_1
    logic       uart_tx;              // top_unnamed_output
    logic       busy;                 // top_unnamed_output_1

    // ⚠️ inline 后代（`uart_tx_inst`）的输出**也是顶层端口** ⇒ MUST 连接
    logic       inst_uart_tx;         // top_uart_tx_inst_unnamed_output
    logic       inst_busy;            // top_uart_tx_inst_unnamed_output_1

    integer checks = 0;
    integer fails  = 0;

    // ── 采样与测量设施（module 级 ⇒ 避免 `%Warning-IMPLICITSTATIC`）────────
    integer cyc = 0;                  // 拍计数（复位释放后从 0 起）
    logic [32767:0] trace;            // 按拍记录 uart_tx（供位格中点采样）
    logic [32767:0] btrace;           // 按拍记录 busy
    integer edge_time [0:255];        // 边沿发生的拍号
    integer edge_val  [0:255];        // 边沿后的新值（0/1）
    integer edge_count = 0;
    logic   mon_prev = 1'b1;

    integer trig_cyc = 0;             // 本帧的触发拍号
    integer t0 = -1;                  // 起始位起点（-1 = 未找到）
    integer bitw = 0;                 // 测得的位宽（拍）
    integer base_cyc = 0;             // 测量基准（起始位起点）
    integer w = 0;                    // 临时：某个位段的宽度
    integer w_prev = -1;              // 临时：上一个边沿的拍号
    logic [7:0] decoded = 8'h00;      // 临时：解码出的字节
    logic       bitv = 1'b0;          // 临时：位格中点采样到的值

    // ⚠️ **时钟生成器**（重写时曾漏掉 ⇒ 仿真无事件即结束 ⇒ 表现为「end at 0s」）
    // ── DUT 实例（⚠️ 重写时曾整段丢失 ⇒ 输出悬空 = 0 ⇒ 判据全红而 DUT 毫无察觉）──
    top dut (
        .default_clock(clk),
        .default_reset(rst),
        .top_unnamed_input(frame_start),
        .top_unnamed_input_1(data),
        .top_unnamed_output(uart_tx),
        .top_unnamed_output_1(busy),
        .top_uart_tx_inst_unnamed_output(inst_uart_tx),
        .top_uart_tx_inst_unnamed_output_1(inst_busy)
    );

    always #5 clk = ~clk;

    always @(posedge clk) begin
        if (rst) begin
            cyc = 0;
            edge_count = 0;
            mon_prev = 1'b1;
        end else begin
            trace[cyc]  = uart_tx;
            btrace[cyc] = busy;
            // 只在 uart_tx 为**已知值**时记边沿（X 会被 Verilator 优化成 0 ⇒ 不可判）
            if ((uart_tx === 1'b0 || uart_tx === 1'b1) && uart_tx !== mon_prev) begin
                edge_time[edge_count] = cyc;
                edge_val[edge_count]  = (uart_tx === 1'b1) ? 1 : 0;
                edge_count = edge_count + 1;
                mon_prev = uart_tx;
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

    task automatic expect_eq(input integer got, input integer want);
        begin
            checks = checks + 1;
            if (got !== want) fails = fails + 1;
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

    // 触发一帧（`frame_start` **恰好 1 拍** —— 内部锁存器是电平敏感的 ⇒ 不可拉长）
    task automatic send_frame(input logic [7:0] payload);
        begin
            @(negedge clk);
            data = payload;
            frame_start = 1'b1;
            @(posedge clk);
            #1;
            frame_start = 1'b0;
            trig_cyc = cyc;
        end
    endtask

    // 找出「触发之后第一个 0 边沿」= 起始位起点
    task automatic find_start_edge(input integer from_edge);
        begin
            t0 = -1;
            for (int i = from_edge; i < edge_count; i = i + 1) begin
                if (t0 < 0 && edge_time[i] > trig_cyc && edge_val[i] == 0) begin
                    t0 = edge_time[i];
                end
            end
        end
    endtask

    // 在「位格中点」采样 uart_tx（位格 i = [base + i*bitw, base + (i+1)*bitw)）
    task automatic sample_cell(input integer base, input integer idx, output logic got);
        begin
            got = trace[base + idx * bitw + bitw / 2];
        end
    endtask

    initial begin
        // ── 复位 ─────────────────────────────────────────────────────
        rst = 1'b1;
        repeat (4) @(posedge clk);
        @(negedge clk);
        rst = 1'b0;
        repeat (2) @(posedge clk);

        // ── S1：空闲态（**spec-derived**：UART 空闲 = 高电平）────────
        for (int i = 0; i < 8; i = i + 1) begin
            @(posedge clk);
            #1;
            expect_bit(uart_tx, 1'b1);
        end
        // impl-contract：IDLE 时 `busy` = 0
        expect_bit(busy, 1'b0);
        // inline 后代端口与顶层端口同源
        expect_bit(inst_uart_tx, uart_tx);
        expect_bit(inst_busy, busy);

        // ══════════════════════════════════════════════════════════════
        // 帧 A：**0x55（交替位）** ⇒ 用边沿**测得**位宽与各位取值
        // ══════════════════════════════════════════════════════════════
        send_frame(8'h55);
        repeat (12 * 434) @(posedge clk);        // 等整帧结束（10 位 × 434 + 余量）

        find_start_edge(0);
        // spec-derived：帧必须**有**起始位（触发后出现 0 边沿）
        expect_eq(t0 >= 0 ? 1 : 0, 1);

        if (t0 >= 0) begin
            base_cyc = t0;
            // ── 测得位宽：起始位与第 1 个数据位之间的边沿间隔 ────────
            //   ⚠️ 0x55 ⇒ 起始位(0) 后必接 1 ⇒ **边界必有边沿** ⇒ 间隔 = 位宽
            bitw = 0;
            for (int i = 0; i < edge_count; i = i + 1) begin
                if (bitw == 0 && edge_time[i] > t0 && edge_val[i] == 1) begin
                    bitw = edge_time[i] - t0;
                end
            end
            expect_eq(bitw > 0 ? 1 : 0, 1);

            // ── **metamorphic**：9 个可测段（起始位 + 8 数据位）宽度**全等** ──
            //   ⇒ 相对判据（不依赖 434 这个常量）⇒ ⇒ 抓「第 8 个数据位只发 1 拍」
            //     这类 off-by-one（源码注释里点名过的那类缺陷）
            //   ⇒ 期望值 = **测得的**第一个宽度（`bitw`）⇒ 不写死 434
            w_prev = -1;
            for (int i = 0; i < edge_count; i = i + 1) begin
                if (edge_time[i] >= t0 && edge_time[i] <= t0 + 9 * bitw) begin
                    if (w_prev >= 0) begin
                        checks = checks + 1;
                        if (edge_time[i] - w_prev != bitw) fails = fails + 1;
                    end
                    w_prev = edge_time[i];
                end
            end

            // ── impl-contract：位宽 = 434 拍（`baud_counter` 0..433）──
            expect_eq(bitw, 434);

            // ── spec-derived：起始位为**低**电平（8N1）─────────────
            expect_bit(trace[t0 + bitw / 2], 1'b0);

            // ── spec-derived：8 个数据位 **LSB first** ⇒ 解码回驱动字节 ──
            decoded = 8'h00;
            for (int i = 0; i < 8; i = i + 1) begin
                sample_cell(base_cyc, i + 1, bitv);     // i+1 ⇒ 跳过起始位
                if (bitv === 1'b1) decoded = decoded | (8'h01 << i[7:0]);
            end
            expect_eq8(decoded, 8'h55);

            // ── spec-derived：停止位为**高**电平 ─────────────────────
            sample_cell(base_cyc, 9, bitv);             // 第 9 格 = 停止位
            expect_bit(bitv, 1'b1);

            // ── impl-contract：`busy` 在起始位 + 数据位期间为 1 ─────
            expect_bit(btrace[t0 + bitw / 2], 1'b1);
            expect_bit(btrace[t0 + 8 * bitw + bitw / 2], 1'b1);
            // ── impl-contract：停止位期间 `busy` = 0（源码：STOP 贡献 false）──
            expect_bit(btrace[t0 + 9 * bitw + bitw / 2], 1'b0);
        end

        // ══════════════════════════════════════════════════════════════
        // 帧 B：**0x5A**（含相邻同值位 ⇒ 边界无边沿）⇒ 用**测得**的位宽
        //       在位格中点采样来解码 ⇒ ⇒ 抓「位序 / LSB-first 错」
        // ══════════════════════════════════════════════════════════════
        if (bitw > 0) begin
            integer edge_mark;
            edge_mark = edge_count;
            send_frame(8'h5A);
            repeat (12 * 434) @(posedge clk);

            find_start_edge(edge_mark);
            expect_eq(t0 >= 0 ? 1 : 0, 1);

            if (t0 >= 0) begin
                base_cyc = t0;
                decoded = 8'h00;
                for (int i = 0; i < 8; i = i + 1) begin
                    sample_cell(base_cyc, i + 1, bitv);
                    if (bitv === 1'b1) decoded = decoded | (8'h01 << i[7:0]);
                end
                // spec-derived：LSB first ⇒ 解码回 **0x5A**（若按 MSB first 解 ⇒ 0xA5 ⇒ 红）
                expect_eq8(decoded, 8'h5A);
                // spec-derived：起始位低 / 停止位高
                expect_bit(trace[t0 + bitw / 2], 1'b0);
                sample_cell(base_cyc, 9, bitv);
                expect_bit(bitv, 1'b1);
                // impl-contract：帧 B 的位宽与帧 A **相同**（同一实现 ⇒ 同一常量）
                //   ⇒ 相对判据（不写死 434）⇒ 抓「位宽随负载变化」
                //   ⚠️ **不能用「首个 1 边沿」法**（0x5A 的首数据位与起始位**同为 0**
                //     ⇒ 该处无边沿 ⇒ 会量到 2W）⇒ ⇒ 改用**帧内最小边沿间隔**
                //     （0x5A 的边沿间隔集合 = {W, W, 2W, W, W, W} ⇒ 最小 = W ✓）
                w = 0;
                w_prev = -1;
                for (int i = edge_mark; i < edge_count; i = i + 1) begin
                    if (edge_time[i] >= t0 && edge_time[i] <= t0 + 9 * bitw) begin
                        if (w_prev >= 0) begin
                            if (w == 0 || (edge_time[i] - w_prev) < w) begin
                                w = edge_time[i] - w_prev;
                            end
                        end
                        w_prev = edge_time[i];
                    end
                end
                expect_eq(w, bitw);
            end
        end

        $display("RESULT: checks=%0d fails=%0d", checks, fails);
        if (fails != 0) $fatal(1, "tb_uart_tx_example failed");
        $finish;
    end

endmodule
