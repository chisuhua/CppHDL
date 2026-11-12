// examples/axi4/tb_axi4_lite_example.sv
//
// EXPECT-SRC: .rddf/checks/examples-rtl-expectations.md#axi4-lite-example
//
// **独立 Verilog TB**（`examples-rtl-sim-coverage` E3）—— `axi4_lite_example`
//
// ⚠️ **期望源 = capability `axi-slave-conformance` + `dual-backend-test-coverage`**
//   （仓内的 AXI4-Lite 规范面，逐条 Requirement 可引）：
//   · 「MUST 提供**低有效复位**」⇒ 复位后 `bvalid`/`rvalid` MUST 为 0、寄存器 MUST 清零
//   · 「写响应 MUST **保持到 BREADY**」⇒ `bvalid` MUST 跨周期保持；
//     且「slave MUST NOT 接受下一笔写事务（`awready` 保持 0）」
//   · 「读响应 MUST **保持到 RREADY**」⇒ `rvalid` MUST 跨周期保持
//   · 「BREADY/RREADY 握手后 MUST 清除」且「MUST 能接受下一笔事务」
//   · 「未映射地址 MUST 返回 **DECERR**」⇒ `bresp`/`rresp` = `2'b11`
//   · 「MUST 支持**字节写**」⇒ `wstrb` bit i ⇒ 第 i 个字节道（从 LSB 起）
//   · 「MUST 支持 **AW 与 W 相位分离**」+「MUST **锁存地址**」
//
// ⚠️ **端口映射**（由**产物连线**逐条反推，并经 `awvalid`/`wready` 两条链**印证**）：
//   ⚠️ 顶层输入编号 = 组件 `describe()` 里**首次引用**的顺序（此 example 与 `__io`
//   声明顺序一致 ⇒ 但**不能靠猜** ⇒ 本映射以产物为准）：
//   awaddr=input · awprot=input_1 · awvalid=input_2 · wdata=input_3 · wstrb=input_4
//   · wvalid=input_5 · bready=input_6 · araddr=input_7 · arprot=input_8
//   · arvalid=input_9 · rready=input_10 · rst_n=input_11
//   awready=output · wready=output_1 · bresp=output_2 · bvalid=output_3
//   · arready=output_4 · rdata=output_5 · rresp=output_6 · rvalid=output_7
//
// ⚠️ **TB 作为 master MUST 遵守 AXI 规则**（这些是 **master 侧**义务，不是从端判据）：
//   · **VALID 不得等 READY** ⇒ 本 TB 总是**先**拉 VALID 再等 READY
//   · `valid && !ready` 期间 payload MUST 保持稳定
//   ⚠️ 从端的 `awready`/`arready`/`wready` 是**组合**的（= valid & !busy）
//     ⇒ ⇒ 握手在**同拍**完成 ⇒ ⇒ TB 的「等 READY」循环第一次就成立。
//
// ⚠️ **inline 后代的输出也是顶层端口** ⇒ MUST 连接（否则严格 flags 下
//   `%Warning-PINMISSING` **致命** —— 实测踩过）；本 TB 顺带断言它们与顶层端口同源。

`timescale 1ns/1ps

module tb_axi4_lite_example;

    logic clk = 0;
    logic rst = 1'b1;           // 框架复位（default_reset）
    logic rst_n = 1'b0;          // ⚠️ **AXI 级低有效**复位（与框架复位**不是**同一个）

    logic [31:0] awaddr = 32'h0;
    logic [1:0]  awprot = 2'b00;
    logic        awvalid = 1'b0;
    logic        awready;

    logic [31:0] wdata = 32'h0;
    logic [3:0]  wstrb = 4'h0;
    logic        wvalid = 1'b0;
    logic        wready;

    logic [1:0]  bresp;
    logic        bvalid;
    logic        bready = 1'b0;

    logic [31:0] araddr = 32'h0;
    logic [1:0]  arprot = 2'b00;
    logic        arvalid = 1'b0;
    logic        arready;

    logic [31:0] rdata;
    logic [1:0]  rresp;
    logic        rvalid;
    logic        rready = 1'b0;

    // ⚠️ inline 后代（`slave`）的输出**也是顶层端口** ⇒ MUST 连接
    logic        s_awready;
    logic        s_wready;
    logic [1:0]  s_bresp;
    logic        s_bvalid;
    logic        s_arready;
    logic [31:0] s_rdata;
    logic [1:0]  s_rresp;
    logic        s_rvalid;

    integer checks = 0;
    integer fails  = 0;

    // 临时量（module 级 ⇒ 避免 `%Warning-IMPLICITSTATIC`）
    logic [31:0] got_data = 32'h0;
    logic [1:0]  got_resp = 2'b00;
    logic [31:0] wrote    = 32'h0;
    integer i = 0;

    // ── DUT 实例（⚠️ 实例化 + 时钟生成器构成 TB 的**最小骨架** ⇒ MUST 成对出现
    //   —— 写 TB 时曾两次分别漏掉 ⇒ 现象是「end at 0s」与「输出悬空 = 0」）
    top dut (
        .default_clock(clk),
        .default_reset(rst),
        .top_unnamed_input(awaddr),
        .top_unnamed_input_1(arprot),
        .top_unnamed_input_2(awvalid),
        .top_unnamed_input_3(wdata),
        .top_unnamed_input_4(wstrb),
        .top_unnamed_input_5(wvalid),
        .top_unnamed_input_6(bready),
        .top_unnamed_input_7(araddr),
        .top_unnamed_input_8(awprot),
        .top_unnamed_input_9(arvalid),
        .top_unnamed_input_10(rready),
        .top_unnamed_input_11(rst_n),
        .top_unnamed_output(awready),
        .top_unnamed_output_1(wready),
        .top_unnamed_output_2(bresp),
        .top_unnamed_output_3(bvalid),
        .top_unnamed_output_4(arready),
        .top_unnamed_output_5(rdata),
        .top_unnamed_output_6(rresp),
        .top_unnamed_output_7(rvalid),
        .top_slave_unnamed_output(s_awready),
        .top_slave_unnamed_output_1(s_wready),
        .top_slave_unnamed_output_2(s_bresp),
        .top_slave_unnamed_output_3(s_bvalid),
        .top_slave_unnamed_output_4(s_arready),
        .top_slave_unnamed_output_5(s_rdata),
        .top_slave_unnamed_output_6(s_rresp),
        .top_slave_unnamed_output_7(s_rvalid)
    );

    // ⚠️ **时钟生成器**（漏掉 ⇒ 仿真无事件即结束 ⇒ 表现为「end at 0s」）
    always #5 clk = ~clk;

    task automatic expect_bit(input logic got, input logic want);
        begin
            checks = checks + 1;
            if ($isunknown(got))   fails = fails + 1;
            else if (got !== want) fails = fails + 1;
        end
    endtask

    // ⚠️ 32 位比较 MUST 用 **32 位**形参（`integer` 是 32 位但**有符号** ⇒
    //   与 32 位实参比较会触发 `%Warning-WIDTHEXPAND` ⇒ 严格 flags 下**致命**）
    task automatic expect_eq32(input logic [31:0] got, input logic [31:0] want);
        begin
            checks = checks + 1;
            if (got !== want) fails = fails + 1;
        end
    endtask

    task automatic expect_eq2(input logic [1:0] got, input logic [1:0] want);
        begin
            checks = checks + 1;
            if (got !== want) fails = fails + 1;
        end
    endtask

    // ── AW 相位（地址握手）──────────────────────────────────────────
    task automatic send_aw(input logic [31:0] addr);
        begin
            integer g;
            @(negedge clk);
            awaddr  = addr;
            awprot  = 2'b00;
            awvalid = 1'b1;
            // ⚠️ 等待**有界**：坏 DUT 时 MUST 快速失败（而不是挂死等 TIMEOUT）
            //   ⚠️ MUST 用 **do-while**：`for` 的条件在**边沿之前**求值 ⇒ 组合 ready
            //     时**一次边沿都不走** ⇒ 握手不会发生 ⇒ ⇒ do-while 保证至少走一拍
            g = 0;
            do begin @(posedge clk); g = g + 1; end while (!awready && g < 64);
            #1;
            awvalid = 1'b0;
        end
    endtask

    // ── W 相位（数据握手）───────────────────────────────────────────
    task automatic send_w(input logic [31:0] data, input logic [3:0] strb);
        begin
            integer g;
            @(negedge clk);
            wdata  = data;
            wstrb  = strb;
            wvalid = 1'b1;
            g = 0;
            do begin @(posedge clk); g = g + 1; end while (!wready && g < 64);
            #1;
            wvalid = 1'b0;
        end
    endtask

    // ── B 相位（写响应握手）─────────────────────────────────────────
    task automatic take_b(output logic [1:0] got_resp);
        begin
            integer g;
            @(negedge clk);
            bready = 1'b1;
            // ⚠️ 有界等待 ⇒ 坏 DUT 快速失败（do-while：至少走一拍）
            g = 0;
            do begin @(posedge clk); g = g + 1; end while (!bvalid && g < 64);
            #1;
            got_resp = bresp;
            bready = 1'b0;
        end
    endtask

    // ── AR 相位（读地址握手）────────────────────────────────────────
    task automatic send_ar(input logic [31:0] addr);
        begin
            integer g;
            @(negedge clk);
            araddr  = addr;
            arprot  = 2'b00;
            arvalid = 1'b1;
            g = 0;
            do begin @(posedge clk); g = g + 1; end while (!arready && g < 64);
            #1;
            arvalid = 1'b0;
        end
    endtask

    // ── R 相位（读响应握手）─────────────────────────────────────────
    task automatic take_r(output logic [31:0] got_data, output logic [1:0] got_resp);
        begin
            integer g;
            @(negedge clk);
            rready = 1'b1;
            g = 0;
            do begin @(posedge clk); g = g + 1; end while (!rvalid && g < 64);
            #1;
            got_data = rdata;
            got_resp = rresp;
            rready = 1'b0;
        end
    endtask

    initial begin
        // ── S0：框架复位释放 ─────────────────────────────────────────
        rst = 1'b1;
        rst_n = 1'b0;
        repeat (3) @(posedge clk);
        rst = 1'b0;
        repeat (2) @(posedge clk);
        rst_n = 1'b1;
        repeat (2) @(posedge clk);

        // ══════════════════════════════════════════════════════════════
        // S1：**低有效复位**（spec-derived）
        //   ⚠️ MUST 在**设计已运行之后**重新拉低 `rst_n` ⇒ 否则判据恒真
        //   · `bvalid`/`rvalid` MUST 为 0
        //   · 状态寄存器 MUST 清零（⇒ 先写一个值再复位，读回 MUST 为 0）
        // ══════════════════════════════════════════════════════════════
        send_aw(32'h00000000);
        send_w(32'h11223344, 4'hF);
        take_b(got_resp);
        rst_n = 1'b0;
        repeat (2) @(posedge clk);
        #1;
        expect_bit(bvalid, 1'b0);
        expect_bit(rvalid, 1'b0);
        rst_n = 1'b1;
        repeat (2) @(posedge clk);
        send_ar(32'h00000000);
        take_r(got_data, got_resp);
        expect_eq32(got_data, 32'h00000000);   // 复位 MUST 清零寄存器

        // ══════════════════════════════════════════════════════════════
        // S2：**AW/W 相位分离 + 地址锁存**（spec-derived）
        //   · AW 先到、W 延后 ⇒ 中间 MUST NOT 产生 `bvalid`
        //   · AW 握手后 master 可改写 `awaddr` ⇒ 写入 MUST 落在**锁存**的地址
        // ══════════════════════════════════════════════════════════════
        send_aw(32'h00000000);
        // 握手后 master 改写 awaddr（AXI 允许）⇒ ⇒ 从端 MUST 用锁存地址
        awaddr = 32'h00000008;
        repeat (2) @(posedge clk);
        #1;
        expect_bit(bvalid, 1'b0);          // W 未到 ⇒ 写未完成 ⇒ 无响应
        send_w(32'hCAFEBABE, 4'hF);
        take_b(got_resp);
        expect_eq2(got_resp, 2'b00);      // 映射地址 ⇒ OKAY
        // 读回 0x00 ⇒ MUST 是刚写的值（**锁存**生效）
        send_ar(32'h00000000);
        take_r(got_data, got_resp);
        expect_eq32(got_data, 32'hCAFEBABE);
        // 读回 0x08 ⇒ MUST **未被**这次写影响（写落在 0x00）
        send_ar(32'h00000008);
        take_r(got_data, got_resp);
        expect_eq32(got_data, 32'h00000000);

        // ══════════════════════════════════════════════════════════════
        // S3：**写读回值一致**（**metamorphic**：写入什么就读回什么）
        //   · 4 个寄存器（0x00 / 0x04 / 0x08 / 0x0C）逐个写读
        // ══════════════════════════════════════════════════════════════
        for (i = 0; i < 4; i = i + 1) begin
            wrote = 32'hA5000000 + i[31:0];
            send_aw(i[31:0] * 32'd4);
            send_w(wrote, 4'hF);
            take_b(got_resp);
            expect_eq2(got_resp, 2'b00);
            send_ar(i[31:0] * 32'd4);
            take_r(got_data, got_resp);
            expect_eq32(got_data, wrote);          // **相对**比较
            expect_eq2(got_resp, 2'b00);
        end

        // ══════════════════════════════════════════════════════════════
        // S4：**未映射地址 ⇒ DECERR**（spec-derived）
        //   · 窗口 = 0x00..0x0C（4 个寄存器，步长 4 字节）⇒ 0x10 起窗口外
        // ══════════════════════════════════════════════════════════════
        send_aw(32'h00000010);
        send_w(32'hDEADBEEF, 4'hF);
        take_b(got_resp);
        expect_eq2(got_resp, 2'b11);      // DECERR
        send_ar(32'h00000010);
        take_r(got_data, got_resp);
        expect_eq2(got_resp, 2'b11);      // DECERR
        expect_eq32(got_data, 32'h00000000);   // MUST NOT 呈现其它寄存器的值

        // ══════════════════════════════════════════════════════════════
        // S5：**写响应保持到 BREADY**（spec-derived）
        //   · `bvalid` MUST 跨周期保持 · 且 MUST NOT 接受下一笔写事务
        // ══════════════════════════════════════════════════════════════
        send_aw(32'h00000004);
        send_w(32'h12345678, 4'hF);
        repeat (2) @(posedge clk);
        #1;
        expect_bit(bvalid, 1'b1);          // 响应已产生
        // 不拉 BREADY ⇒ `bvalid` MUST 保持
        repeat (3) @(posedge clk);
        #1;
        expect_bit(bvalid, 1'b1);
        // 且 MUST NOT 接受下一笔写事务（拉高 awvalid ⇒ awready MUST 仍为 0）
        awaddr  = 32'h0000000C;
        awvalid = 1'b1;
        repeat (2) @(posedge clk);
        #1;
        expect_bit(awready, 1'b0);
        awvalid = 1'b0;
        // BREADY 握手 ⇒ `bvalid` MUST 清除
        bready = 1'b1;
        @(posedge clk);
        #1;
        bready = 1'b0;
        expect_bit(bvalid, 1'b0);
        repeat (2) @(posedge clk);
        #1;
        expect_bit(bvalid, 1'b0);

        // ══════════════════════════════════════════════════════════════
        // S6：**读响应保持到 RREADY**（spec-derived）
        // ══════════════════════════════════════════════════════════════
        send_ar(32'h00000004);
        repeat (2) @(posedge clk);
        #1;
        expect_bit(rvalid, 1'b1);
        repeat (3) @(posedge clk);
        #1;
        expect_bit(rvalid, 1'b1);
        // 且 MUST NOT 接受下一笔读事务
        araddr  = 32'h00000008;
        arvalid = 1'b1;
        repeat (2) @(posedge clk);
        #1;
        expect_bit(arready, 1'b0);
        arvalid = 1'b0;
        // RREADY 握手 ⇒ `rvalid` MUST 清除
        rready = 1'b1;
        @(posedge clk);
        #1;
        rready = 1'b0;
        expect_bit(rvalid, 1'b0);
        repeat (2) @(posedge clk);
        #1;
        expect_bit(rvalid, 1'b0);

        // ══════════════════════════════════════════════════════════════
        // S7：**字节道（wstrb）**（spec-derived）
        //   · `wstrb` bit i ⇒ 第 i 个字节道（从 LSB 起）
        //   · 部分字节写 MUST 只改选中的字节
        //   · 空写（wstrb == 0）的响应 MUST 仍为 OKAY
        // ══════════════════════════════════════════════════════════════
        send_aw(32'h00000000);
        send_w(32'h11223344, 4'hF);
        take_b(got_resp);
        expect_eq2(got_resp, 2'b00);
        // 部分字节写（lane 0/1 = 低 2 字节）
        send_aw(32'h00000000);
        send_w(32'hAABBCCDD, 4'b0011);
        take_b(got_resp);
        expect_eq2(got_resp, 2'b00);
        send_ar(32'h00000000);
        take_r(got_data, got_resp);
        expect_eq32(got_data, 32'h1122CCDD);   // 高 2 字节保持、低 2 字节被改
        // 空写（wstrb == 0）⇒ 响应仍 OKAY 且寄存器不变
        send_aw(32'h00000000);
        send_w(32'hFFFFFFFF, 4'h0);
        take_b(got_resp);
        expect_eq2(got_resp, 2'b00);
        send_ar(32'h00000000);
        take_r(got_data, got_resp);
        expect_eq32(got_data, 32'h1122CCDD);   // MUST NOT 被改

        // ── S8：inline 后代端口与顶层端口同源 ───────────────────────
        expect_bit(s_awready, awready);
        expect_bit(s_wready, wready);
        expect_eq2(s_bresp, bresp);
        expect_bit(s_bvalid, bvalid);
        expect_bit(s_arready, arready);
        expect_eq32(s_rdata, rdata);
        expect_eq2(s_rresp, rresp);
        expect_bit(s_rvalid, rvalid);

        $display("RESULT: checks=%0d fails=%0d", checks, fails);
        if (fails != 0) $fatal(1, "tb_axi4_lite_example failed");
        $finish;
    end

endmodule
