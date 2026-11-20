// examples/axi4/tb_phase3a_example.sv
//
// **Phase 3A 产品的独立 Verilog TB**（`phase3a-rtl-product-2026-11` B3 · **手写**）
//
// EXPECT-SRC: .rddf/checks/examples-rtl-expectations.md#phase3a-example
//
// ⚠️ **本 TB 是「手写」而非生成**（`generated=no`）：phase3a 的激励**不是** `.verif` 驱动
//   ⇒ ⇒ 迁移属 ADR-074 §7（D2′/D3′）的**实施侧**范围 ⇒ ⇒ 本单元拿不到 ⑥'/⑥''/⑥‴/⑧''
//   五条「生成物」判据（它们只对 `generated=yes` 的行有可判对象）⇒ ⇒ 这是 ADR-074 D7 的
//   **手写 TB 逃生口**（合法）。
//
// ⚠️ **期望值全部来自 EXPECT-SRC（手推）** —— 禁从仿真输出 / 禁从被测实现转录
//   （根 AGENTS.md：「两后端一致」不能当作正确性证据 ⇒ 同构缺陷会两后端一致地错）。
//
// ⚠️ **两种复位语义不同（必须分别驱动）**：
//   · `rst`（= `default_reset`，**高有效 · level**）⇒ 拉住期间**所有 ch_reg** 复位
//     （含内部 FSM）⇒ ⇒ **必须先拉低释放**，否则事务永不完成。
//   · `rst_n`（根自有，**低有效**）⇒ **只**透传 AXI 子系统（converter → slave）
//     ⇒ ⇒ 清 **slave 的寄存器** ⇒ ⇒ **不影响**内部 FSM ⇒ ⇒ 两者**不是**同一个复位。
//
// ⚠️ **端口数 = 9**（框架 2 + 根自有 7）—— 漏连框架 2 ⇒ 严格 flags 下
//   `%Warning-PINMISSING` **致命**。

`timescale 1ns/1ps

module tb_phase3a_example;

    logic clk = 0;
    logic rst = 1'b1;          // default_reset（高有效 · level）
    logic rst_n = 1'b0;        // AXI 级低有效（与框架复位**不是**同一个）

    // 顶层输入（映射见期望源 §1）
    logic        start    = 1'b0;
    logic        is_write = 1'b0;
    logic [31:0] addr     = 32'h0;
    logic [31:0] wdata    = 32'h0;

    // 顶层输出
    logic [31:0] rdata;
    logic        done;

    integer checks = 0;
    integer fails  = 0;

    // ── DUT 实例（实例化 + 时钟生成器 = TB 的最小骨架 ⇒ MUST 成对出现）────────
    top dut (
        .default_clock(clk),
        .default_reset(rst),
        .top_start(start),
        .top_is_write(is_write),
        .top_addr(addr),
        .top_wdata(wdata),
        .top_rst_n(rst_n),
        .top_rdata(rdata),
        .top_done(done)
    );

    always #5 clk = ~clk;

    // ── 断言任务（⚠️ 每次调用 `checks` +1 ⇒ ⇒ 条数 == 清单登记的 `min_checks`）─────
    // ⚠️ **失败项 MUST 逐条点名**（根 AGENTS.md：「判别性探针 MUST 输出显式 PASS/FAIL
    //   判定」—— 只报失败总数 ⇒ 读者无法定位是哪一条 ⇒ 判别力自证失效）。
    task automatic expect_bit(input logic got, input logic want, input string what);
        begin
            checks = checks + 1;
            if ($isunknown(got)) begin
                fails = fails + 1;
                $display("CHECK FAIL: %s (got=X want=%b)", what, want);
            end else if (got !== want) begin
                fails = fails + 1;
                $display("CHECK FAIL: %s (got=%b want=%b)", what, got, want);
            end
        end
    endtask

    task automatic expect_eq32(input logic [31:0] got, input logic [31:0] want, input string what);
        begin
            checks = checks + 1;
            if (got !== want) begin
                fails = fails + 1;
                $display("CHECK FAIL: %s (got=0x%08h want=0x%08h)", what, got, want);
            end
        end
    endtask

    // ── 事务任务（**按已知延迟采样** ⇒ 64 拍余量 · impl-contract · **不计入** checks）──
    //   ⚠️ 任务内**不**做断言（否则条数随实际拍数漂移 ⇒ 破坏护栏 ⑥ 的等式）。
    task automatic do_write(input logic [31:0] a, input logic [31:0] d);
        begin
            addr     = a;
            wdata    = d;
            is_write = 1'b1;
            start    = 1'b1;
            @(posedge clk); #1;
            start    = 1'b0;
            is_write = 1'b0;
            repeat (64) @(posedge clk);
            #1;
        end
    endtask

    task automatic do_read(input logic [31:0] a);
        begin
            addr     = a;
            is_write = 1'b0;
            start    = 1'b1;
            @(posedge clk); #1;
            start    = 1'b0;
            repeat (64) @(posedge clk);
            #1;
        end
    endtask

    initial begin
        // ── 初始复位态（`default_reset` 拉住 ⇒ 所有 ch_reg 归 INIT）────────────
        rst   = 1'b1;
        rst_n = 1'b0;
        @(posedge clk); #1;
        expect_bit(done, 1'b0, "1-init-done-zero");            // ① 复位态 done == 0

        // ── 释放复位（**level 有效 ⇒ 必须先拉低**，否则事务永不完成）─────────────
        rst   = 1'b0;
        rst_n = 1'b1;
        @(posedge clk); #1;

        // ── ② 写事务（AXI4-Lite 写 = AW+W+B 三段握手 ⇒ 完成信号必须拉高）──────
        do_write(32'h0000_0000, 32'h0000_00AB);
        expect_bit(done, 1'b1, "2-write-done");                // ② 写完成

        // ── ③ 读事务（AXI4-Lite 读 = AR+R 两段握手）────────────────────────────
        do_read(32'h0000_0000);
        expect_bit(done, 1'b1, "3-read-done");                 // ③ 读完成

        // ── ④ 读回值（AXI4-Lite 寄存器语义 = 同址写后读回同值 ⇒ **spec-derived**）──
        expect_eq32(rdata, 32'h0000_00AB, "4-readback-0xAB");   // ④ 读回 0xAB

        // ── ⑤ 框架复位（高有效 ⇒ 清**所有** ch_reg ⇒ 内部 FSM 归 INIT）────────
        //   ⚠️ 此刻 `done` **曾为 1**（断言 ③ 之后）⇒ ⇒ 本条**有判别力**。
        rst = 1'b1;
        @(posedge clk); #1;
        expect_bit(done, 1'b0, "5-default-reset-done-zero");   // ⑤ 框架复位后 done == 0
        rst = 1'b0;                                            // 释放框架复位（level 有效）
        @(posedge clk); #1;

        // ── ⑥ AXI 级复位（低有效 ⇒ 只清 AXI 子系统）⇒ 读事务**不完成**─────────
        //   ⚠️ 手推：`rst_n` 拉低 ⇒ slave 的 `ch_reg` 被 `select(!rst_n, <init>, …)`
        //   清零 ⇒ ⇒ 但 **master 的 FSM（`mstate` / `done_reg`）不受 `rst_n` 影响**
        //   （它们的复位源是**框架** `default_reset` ⇒ `ch_reg` 共享 default_reset）
        //   ⇒ ⇒ master 仍在等 R 响应 ⇒ ⇒ 而 slave 已被复位 ⇒ ⇒ 响应永不到来
        //   ⇒ ⇒ **在途/后续事务被丢弃 ⇒ `done` 永不拉高** ⇒ ⇒ 期望 `done == 0`。
        //   ⚠️ **判别力**：若 `rst_n` 链路失效 ⇒ 读事务正常完成 ⇒ `done == 1` ⇒ 转红。
        //   ⚠️ **本条测的是 phase3a 的设计边界**（master 不响应 AXI 级复位）⇒ ⇒ 不是缺陷。
        rst_n = 1'b0;
        @(posedge clk); #1;
        do_read(32'h0000_0000);
        expect_bit(done, 1'b0, "6-rst-n-no-completion");       // ⑥ 读事务不完成

        $display("RESULT: checks=%0d fails=%0d", checks, fails);
        if (fails != 0) begin
            $display("FAIL: %0d 项断言失败", fails);
            $fatal(1, "tb_phase3a_example: 断言失败");
        end else begin
            // ⚠️ **必须显式 `$finish`** —— 时钟生成器 `always #5 clk = ~clk` **永不停止**
            //   ⇒ ⇒ 无 `$finish` / `$fatal` 时仿真**不会结束**（实测 2026-11-20：全部
            //   断言通过时 TB 挂住 ⇒ CI 超时）⇒ ⇒ 正常路径 MUST 收尾。
            $finish;
        end
    end

endmodule
