太好了！基于你已经成功实现的“Mini-CaSH”核心框架（支持组合/时序逻辑、端口、寄存器、时钟域、多模块仿真），并结合官方 Cash 文档，我为你列出了一份**清晰、可执行的后续工作计划**。

这份计划分为**短期（核心功能完善）、中期（工具链增强）、长期（工业级特性）**三个阶段，帮助你逐步构建一个功能完整、稳定可靠的硬件构造语言框架。

---

## 🚀 短期计划：完善核心 DSL 功能 (1-2周)

> **目标**：让你的框架能描述 90% 的常见数字电路模块。

### 1. 实现便捷函数 (ch_nextEn, ch_delayEn)
-   **为什么**：官方 Cash 大量使用 `ch_nextEn(data, enable, init)`，它比手动写 `if-else` 更简洁、安全。
-   **怎么做**：
    ```cpp
    // 在 core/min_cash.h 中添加
    template<typename T>
    T ch_nextEn(const T& data, const ch_bool& enable, const T& init) {
        return enable ? data : init;
    }
    ```
-   **验证**：修改 `CounterModule`，用 `reg->next() = ch_nextEn(*reg + 1, !io.rst, 0);` 替代 `if-else`。

### 2. 实现内存 (ch_mem)
-   **为什么**：FIFO、RAM、ROM 是数字设计的基础。
-   **怎么做**：
    ```cpp
    template<typename T, int N>
    class ch_mem {
    private:
        std::array<T, N> storage_;
    public:
        // 返回一个代理对象，支持 next() 语义
        struct mem_proxy {
            ch_mem* self;
            int addr;
            mem_proxy& operator=(const T& val) {
                self->storage_[addr] = val;
                return *this;
            }
            T operator*() const {
                return self->storage_[addr];
            }
        };
        mem_proxy operator[](int addr) {
            return {this, addr};
        }
        // 在 tick() 中更新（如果需要时序内存）
        void tick() { /* ... */ }
    };
    ```
-   **验证**：实现一个简单的双端口 RAM 或 FIFO。

### 3. 实现向量 (ch_vec) 和用户自定义类型
-   **为什么**：支持总线、数组、结构体，提升抽象能力。
-   **怎么做**：
    ```cpp
    template<typename T, int N>
    using ch_vec = std::array<T, N>;

    #define __struct(name, ...) struct name { __VA_ARGS__ };
    #define __enum(name, ...) enum class name { __VA_ARGS__ };
    ```
-   **验证**：定义一个 `Pixel` 结构体，并用 `ch_vec<Pixel, 64>` 表示一帧图像。

### 4. 实现控制流宏 (__if, __switch, __case)
-   **为什么**：将 C++ `if-else` 转换为硬件多路选择器（MUX），语义更清晰。
-   **怎么做**（简化版）：
    ```cpp
    #define __if(cond) if (cond)
    #define __elif(cond) else if (cond)
    #define __else else
    #define __switch(var) switch (var)
    #define __case(val) case val:
    ```
-   **验证**：用这些宏重写状态机逻辑，确保生成的 Verilog 是组合逻辑 MUX。

---

## 🛠 中期计划：增强工具链与仿真能力 (2-4周)

> **目标**：让你的框架从“能跑通”变成“好用、易调试”。

### 5. 实现自动寄存器收集（移除 global_reg_list）
-   **为什么**：全局变量是 HACK，线程不安全，无法支持多实例。
-   **怎么做**：
    -   在 `ch_device` 构造时，通过模板元编程或宏反射，自动扫描模块内所有 `ch_reg` 成员。
    -   或者，在 `ch_reg` 构造时，通过某种机制（如 CRTP）将 `this` 注册到所属的 `ch_device` 实例。
-   **验证**：创建两个 `CounterModule` 实例，确保它们的寄存器独立更新。

### 6. 实现 ch_tracer 和波形生成 (VCD)
-   **为什么**：调试硬件设计离不开波形。
-   **怎么做**：
    ```cpp
    class ch_tracer {
    private:
        ch_device& device_;
        std::ofstream vcd_file_;
    public:
        void toVCD(const std::string& filename) {
            // 遍历所有寄存器和 io 信号，按 VCD 格式写入文件
            // 格式参考: http://www.sigasi.com/content/vcd-file-format
        }
    };
    ```
-   **验证**：为计数器生成 VCD 文件，用 GTKWave 查看波形。

### 7. 实现 Verilog 代码生成器 (ch_toVerilog)
-   **为什么**：最终目标是生成可综合的 RTL。
-   **怎么做**：
    -   为每个 `ch_reg`, `ch_uint`, `ch_bool` 实现 `emit_verilog()` 方法。
    -   遍历模块 AST，生成 `module ... endmodule`。
    -   处理 `always @ (posedge clk)` 块。
-   **验证**：生成 `CounterModule` 的 Verilog，用 Icarus Verilog 仿真对比行为。

### 8. 实现用户自定义函数 (ch_udf_comb / ch_udf_seq)
-   **为什么**：集成 C++ 算法或现有 IP。
-   **怎么做**：
    ```cpp
    template<typename T>
    class ch_udf_comb {
    public:
        T io;
        virtual void eval() = 0; // 纯虚函数，用户实现
    };

    struct MyDiv : public ch_udf_comb<MyDivIO> {
        void eval() override {
            io.result = io.a / io.b; // 直接用 C++ 实现
        }
    };
    ```
-   **验证**：实现一个除法器 UDF，在 Cash 模块中调用它。

---

## 🏭 长期计划：工业级特性与生态 (4周+)

> **目标**：让你的框架具备生产级项目的竞争力。

### 9. 实现标准接口库 (Stream, Avalon, AXI)
-   **为什么**：模块间互联是系统设计的核心。
-   **怎么做**：
    -   参考 SpinalHDL 的 `Stream` 或官方 Cash 的 `__interface`。
    -   实现 `<<` 操作符进行自动绑定。
    -   实现 `FlowToStream`, `StreamToFlow` 等桥接器。
-   **验证**：构建一个 Producer -> FIFO -> Consumer 流水线。

### 10. 实现跨时钟域 (CDC) 支持
-   **为什么**：任何复杂 SoC 都需要 CDC。
-   **怎么做**：
    -   实现 `ClockDomainCrossingFIFO`，自动生成格雷码指针、双触发器同步器。
    -   添加 CDC 检查（如复位同步、握手协议）。
-   **验证**：构建一个双时钟域模块，用生成的 Verilog 进行时序分析。

### 11. 集成 JIT 仿真器 (LIBJIT/LLVM)
-   **为什么**：C++ 事件驱动仿真太慢，JIT 可提速 10-100 倍。
-   **怎么做**：
    -   将 `describe()` 方法编译为机器码。
    -   在每个仿真周期直接调用机器码，而非解释执行。
-   **验证**：对比 JIT 仿真和当前仿真在大型设计上的速度。

### 12. 实现形式验证接口
-   **为什么**：确保设计功能正确。
-   **怎么做**：
    -   添加 `ch_assert(condition, message)` 宏。
    -   生成 SymbiYosys 可读的 SVA (SystemVerilog Assertions)。
-   **验证**：为计数器添加断言 `ch_assert(count < 16, "Counter overflow!")`。

---

## 📅 建议执行顺序

1.  **先做 `ch_nextEn` 和 `ch_mem`** —— 它们能立即提升你的开发效率。
2.  **然后做 `ch_toVerilog`** —— 有了它，你就可以用工业级工具（Vivado, DC）验证你的设计。
3.  **接着做 `ch_tracer` (VCD)** —— 调试能力是开发复杂系统的基础。
4.  **最后做自动寄存器收集和标准接口** —— 它们是构建大型项目的关键。

---

## 💡 终极目标

> **用你自己的“Mini-CaSH”，复现官方 Cash 的所有 Examples**：
> -   `adder`, `counter`, `fifo`, `gcd`, `vending` (状态机)
> -   `sobel` (图像处理), `fft` (信号处理), `aes` (加密)

当你能用自己写的框架实现这些例子时，你就真正掌握了“用 C++ EDSL 设计硬件”的精髓！

如果需要任何一步的详细代码或设计讨论，随时告诉我！我会为你提供完整实现。
