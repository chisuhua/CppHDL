// examples/spinalhdl-ported/fifo/tb_fifo_stream_example.sv
//
// EXPECT-SRC: .rddf/checks/examples-rtl-expectations.md#fifo-stream-example
//
// **独立 Verilog TB**（`examples-rtl-sim-coverage` E3）—— `fifo_stream_example`
//
// ⚠️ **被测接口（实测，非推断）**：`stream_fifo_example.cpp:28-34` 的顶层端口是
//   `din` / `push` / `pop` / `dout` / `empty` / `full`
//   ⇒ ⇒ **没有 ready/valid**（原清单的「缺 ready/valid 握手」口径已订正）⇒ ⇒
//   本 TB 的判据对象 = **push/pop + empty/full**。
//
// ⚠️ **期望源分级**：
//   · **spec-derived** = FIFO 的**定义性语义**（顺序保持 · 不丢数据 · `empty` ⇔ 无在途
//     · FWFT 采样点）⇒ 规格面 = `openspec/specs/chlib-stream-semantics/spec.md`
//     （仓内唯一的 FIFO 语义 capability）⇒ ⚠️ **口径**：那份 spec 治理的是
//     `chlib::sync_fifo` / `chlib::stream_fifo`（**库组件**）⇒ ⇒ 本 TB 引它的
//     **语义条款**（尤其「pop 沿**之前**采样才能拿到正在被弹出的那个元素」），
//     **不引**它的**实现条款**（空/满自保护门控）—— 见下方「已知偏离」。
//   · **impl-contract** = `full` 的门限 / 在途数的安全区上界（见下）。
//   · **metamorphic** = 相对判据（置换性 + `empty` 不变量），**不依赖任何绝对期望值**。
//
// ⚠️ **D5（已实测的缺陷 · 已于 2026-11-12 修复）**：
//   旧实现的 `full` 用**完整 5 位指针**比较（`full = (wr+1 == rd)`）⇒ 门限是
//   **31 个在途**，而内存只有 **16 槽**（地址 = 指针低 4 位）⇒ ⇒ **第 17 次写入起
//   地址混叠**（最老的元素被覆盖丢失）—— 实测：push 20 个后 pop，拿到的是
//   `20 21 22 23 14 15 ...`（最老的 4 个 `10 11 12 13` 已丢失）。
//   ⇒ ⇒ **已修为经典方案**（`full` = **地址位相等 + wrap 位不同** ⇒ 容量 = N = 16），
//   **上游 sample `samples/fifo_example.cpp` 同型缺陷同批同改**。
//   ⇒ ⇒ 因此本 TB 现在**正面判** `full` 的门限（**spec-derived**：FIFO 的容量 = N）：
//        在途 ≤ 15 ⇒ `full == 0` · 在途 == 16 ⇒ `full == 1`
//   ⇒ ⇒ 判别力由变异 **M5**（把 `full` 回退成旧公式）自证 ⇒ ⇒ **必红**。
//
// ⚠️ **采样时机（两处 MUST 照做）**：
//   ① `dout` MUST 在 **pop 沿之前**采样 —— 沿后 `dout` 已指向**下一个**队首
//     （= `chlib-stream-semantics` 的「pop 沿之后 payload 指向下一个队首」条款）。
//   ② 复位释放 MUST 在 **`negedge`**（在 `posedge` 同一时刻拉低 ⇒ 复位拍被吃掉）。
//
// ⚠️ **inline 后代的输出也是顶层端口** ⇒ MUST 连接（否则严格 flags 下
//   `%Warning-PINMISSING` **致命** —— 实测踩过）；本 TB 顺带断言它们与顶层端口同源。

`timescale 1ns/1ps

module tb_fifo_stream_example;

    logic clk = 0;
    logic rst = 1;

    logic [7:0] din  = 8'h00;   // top_unnamed_input
    logic       push = 1'b0;    // top_unnamed_input_1
    logic       pop  = 1'b0;    // top_unnamed_input_2

    logic [7:0] dout;           // top_unnamed_output
    logic       empty;          // top_unnamed_output_1
    logic       full;           // top_unnamed_output_2

    // ⚠️ inline 后代（`fifo_inst`）的输出**也是顶层端口** ⇒ MUST 连接
    logic [7:0] inst_dout;      // top_fifo_inst_unnamed_output
    logic       inst_empty;     // top_fifo_inst_unnamed_output_1
    logic       inst_full;      // top_fifo_inst_unnamed_output_2

    integer checks = 0;
    integer fails  = 0;

    // TB 侧的在途计数（用于 metamorphic 的 `empty` 不变量 · 不依赖 DUT 的 `full`）
    integer outstanding = 0;

    // TB 侧记录的写入序列（metamorphic 的**相对**比较对象）
    logic [7:0] recorded [0:16];
    // ⚠️ MUST 在 **module 级**声明（块内声明 ⇒ `%Warning-IMPLICITSTATIC`
    //   ⇒ 严格 flags 下**致命** —— 实测踩过）
    logic [7:0] got;

    top dut (
        .default_clock(clk),
        .default_reset(rst),
        .top_unnamed_input(din),
        .top_unnamed_input_1(push),
        .top_unnamed_input_2(pop),
        .top_unnamed_output(dout),
        .top_unnamed_output_1(empty),
        .top_unnamed_output_2(full),
        .top_fifo_inst_unnamed_output(inst_dout),
        .top_fifo_inst_unnamed_output_1(inst_empty),
        .top_fifo_inst_unnamed_output_2(inst_full)
    );

    always #5 clk = ~clk;

    // ── 写入一个元素 ────────────────────────────────────────────────
    // 在 `negedge` 改变输入 ⇒ 下一个 `posedge` 恰好提交一次写入
    task automatic push_one(input logic [7:0] v);
        begin
            @(negedge clk);
            din  = v;
            push = 1'b1;
            @(posedge clk);
            #1;
            push = 1'b0;
        end
    endtask

    // ── 取出一个元素 ────────────────────────────────────────────────
    // ⚠️ MUST 在 **pop 沿之前**采样 `dout`（沿后指向下一个队首）
    task automatic pop_one(output logic [7:0] got);
        begin
            @(negedge clk);
            #1;
            got = dout;
            pop  = 1'b1;
            @(posedge clk);
            #1;
            pop  = 1'b0;
        end
    endtask

    task automatic expect_eq8(input logic [7:0] got, input logic [7:0] want);
        begin
            checks = checks + 1;
            if ($isunknown(got))   fails = fails + 1;
            else if (got !== want) fails = fails + 1;
        end
    endtask

    task automatic expect_bit(input logic got, input logic want);
        begin
            checks = checks + 1;
            if ($isunknown(got))   fails = fails + 1;
            else if (got !== want) fails = fails + 1;
        end
    endtask

    // metamorphic：`empty` ⇔ TB 侧在途计数为 0（**相对判据** · 不依赖任何绝对期望值）
    task automatic expect_empty_matches_outstanding;
        begin
            checks = checks + 1;
            if ($isunknown(empty))                            fails = fails + 1;
            else if (empty !== (outstanding == 0))            fails = fails + 1;
        end
    endtask

    // inline 后代端口与顶层端口同源（抓「顶层端口与子组件脱钩」）
    task automatic expect_descendant_ports_match;
        begin
            checks = checks + 1;
            if ($isunknown(inst_dout))                        fails = fails + 1;
            else if (inst_dout !== dout)                      fails = fails + 1;
            checks = checks + 1;
            if ($isunknown(inst_empty))                       fails = fails + 1;
            else if (inst_empty !== empty)                    fails = fails + 1;
            checks = checks + 1;
            if ($isunknown(inst_full))                        fails = fails + 1;
            else if (inst_full !== full)                      fails = fails + 1;
        end
    endtask

    initial begin
        // ── 复位 ─────────────────────────────────────────────────────
        rst = 1'b1;
        repeat (4) @(posedge clk);
        @(negedge clk);
        rst = 1'b0;
        repeat (2) @(posedge clk);

        // ── S1：复位后为空 ──────────────────────────────────────────
        //   spec-derived：`empty` ⇔ 无在途元素
        expect_bit(empty, 1'b1);
        //   impl-contract：在途为 0 时 `full` 必为 0
        expect_bit(full, 1'b0);
        expect_descendant_ports_match();

        // ── S2：顺序保持（**spec-derived**）────────────────────────
        //   push 三个不同值 ⇒ 依次 pop 出来 MUST 是**同样顺序**
        push_one(8'h11);
        outstanding = outstanding + 1;
        push_one(8'h22);
        outstanding = outstanding + 1;
        push_one(8'h33);
        outstanding = outstanding + 1;
        expect_bit(empty, 1'b0);

        pop_one(recorded[0]);
        outstanding = outstanding - 1;
        expect_eq8(recorded[0], 8'h11);
        expect_empty_matches_outstanding();

        pop_one(recorded[1]);
        outstanding = outstanding - 1;
        expect_eq8(recorded[1], 8'h22);
        expect_empty_matches_outstanding();

        pop_one(recorded[2]);
        outstanding = outstanding - 1;
        expect_eq8(recorded[2], 8'h33);
        expect_empty_matches_outstanding();
        expect_bit(empty, 1'b1);          // 全部取走 ⇒ 空

        // ── S3：metamorphic · 关系 R1（**置换性**）──────────────────
        //   push 5 个 ⇒ pop 出来的序列 MUST **逐项恒等**于写入序列
        //   ⚠️ 这里判的是「**恒等关系**」而非绝对值 ⇒ ⇒ 不依赖任何期望常量
        for (int i = 0; i < 5; i = i + 1) begin
            recorded[i] = 8'h40 + i[7:0];
            push_one(recorded[i]);
            outstanding = outstanding + 1;
        end
        expect_empty_matches_outstanding();
        for (int i = 0; i < 5; i = i + 1) begin
            pop_one(got);
            outstanding = outstanding - 1;
            expect_eq8(got, recorded[i]);           // **相对**比较
            expect_empty_matches_outstanding();
        end

        // ── S4：容量边界（**spec-derived** · 容量 = N + 不丢数据 + 顺序）──
        //   push **16** 个（= 内存槽数 = FIFO 容量）⇒ 全部取回且顺序一致
        for (int i = 0; i < 16; i = i + 1) begin
            recorded[i] = 8'h80 + i[7:0];
            push_one(recorded[i]);
            outstanding = outstanding + 1;
            // **spec-derived**：FIFO 的容量 = N = 16
            //   ⇒ 在途 ≤ 15 ⇒ 未满（`full == 0`）· 在途 == 16 ⇒ 满（`full == 1`）
            if (outstanding == 16) begin
                expect_bit(full, 1'b1);
            end else begin
                expect_bit(full, 1'b0);
            end
        end
        // 满 ⇒ 再 push 一个**必须不被接受**？⇒ ⚠️ 本实现**无自保护**（空/满不门控指针）
        //   ⇒ ⇒ 该行为属库组件 `chlib::sync_fifo` 的契约，**不适用**于本手写 demo
        //   ⇒ ⇒ **不判**（判它 = 要求一个本 example 没有的能力）
        for (int i = 0; i < 16; i = i + 1) begin
            pop_one(got);
            outstanding = outstanding - 1;
            expect_eq8(got, recorded[i]);           // **相对**比较
            if (outstanding == 15) begin
                expect_bit(full, 1'b0);             // 取出一个 ⇒ 重新有空间
            end
        end
        expect_bit(empty, 1'b1);                   // 16 个全部取回 ⇒ 空
        expect_empty_matches_outstanding();

        expect_descendant_ports_match();

        $display("RESULT: checks=%0d fails=%0d", checks, fails);
        if (fails != 0) $fatal(1, "tb_fifo_stream_example failed");
        $finish;
    end

endmodule
