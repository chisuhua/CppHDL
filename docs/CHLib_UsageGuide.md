## 基础模块
1. **算术运算模块**
   - 加法器 (Adder) - 基础加法运算
   - 减法器 (Subtractor) - 基础减法运算
   - 乘法器 (Multiplier) - 基础乘法运算
   - 比较器 (Comparator) - 大小比较
   - 移位器 (Shifter) - 左移、右移、算术移位、逻辑移位

2. **基本逻辑模块**
   - 与门 (AND)、或门 (OR)、非门 (NOT)、异或门 (XOR)
   - 多输入与门/或门 (NAND/NOR)
   - 解码器 (Decoder) - 二进制到one-hot转换（除了onehot外的其他解码）

## 中级模块
3. **组合逻辑模块**
   - 多路选择器 (Multiplexer) - 2选1, 4选1, 8选1等
   - 分配器 (Demultiplexer) - 信号分配
   - 编码器 (Encoder) - 优先级编码器
   - 奇偶校验生成器/校验器 (Parity Generator/Checker)
   - 海明码生成器/校验器 (Hamming Code Generator/Checker)

4. **时序逻辑模块**
   - D触发器 (D Flip-Flop) - 带使能和复位
   - JK触发器 (JK Flip-Flop)
   - T触发器 (T Flip-Flop)
   - 寄存器 (Register) - 带并行加载功能
   - 移位寄存器 (Shift Register) - 串行/并行输入输出

5. **计数器模块**
   - 二进制计数器 (Binary Counter)
   - 约翰逊计数器 (Johnson Counter)
   - 环形计数器 (Ring Counter)
   - BCD计数器 (BCD Counter)
   - 可逆计数器 (Up/Down Counter)

## 高级模块
6. **存储器模块**
   - 单端口RAM (Single Port RAM)
   - 双端口RAM (Dual Port RAM)
   - FIFO (First In First Out) - 同步/异步FIFO
   - LIFO (Last In First Out) - 栈存储器
   - CAM (Content Addressable Memory) - 相联存储器

7. **接口协议模块**
   - AXI总线接口 (AXI Bus Interface)
   - AXI-Lite接口 (AXI-Lite Interface)
   - UART控制器 (UART Controller)
   - SPI控制器 (SPI Controller)
   - I2C控制器 (I2C Controller)
   - PCIe接口 (PCIe Interface)

8. **算术和逻辑单元 (ALU)**
   - 基础ALU - 加减与或运算
   - 高级ALU - 包含乘除、移位、比较等
   - 浮点运算单元 (Floating Point Unit)

9. **处理器模块**
   - 简单CPU核心 (Simple CPU Core)
   - RISC-V兼容处理器 (RISC-V Compatible Processor)
   - 流水线处理器 (Pipelined Processor)
   - 多核处理器 (Multi-core Processor)

10. **数字信号处理模块**
    - 数字滤波器 (Digital Filter) - FIR/IIR
    - FFT处理器 (FFT Processor)
    - DDS信号发生器 (DDS Signal Generator)
    - 数模/模数转换接口 (ADC/DAC Interface)

11. **错误检测与纠正模块**
    - CRC生成器/校验器 (CRC Generator/Checker)
    - ECC编码器/解码器 (ECC Encoder/Decoder)
    - 检错重传控制器 (ARQ Controller)

12. **时钟和复位管理模块**
    - 时钟分频器 (Clock Divider)
    - 时钟倍频器 (Clock Multiplier/PLL)
    - 复位同步器 (Reset Synchronizer)
    - 时钟门控 (Clock Gating)

13. **调试和测试模块**
    - 内建自测试 (BIST - Built-In Self Test)
    - 边界扫描 (Boundary Scan - JTAG)
    - 调试接口 (Debug Interface)
    - 仿真模型 (Simulation Models)

14. **高级控制模块**
    - 状态机生成器 (FSM Generator)
    - 调度器 (Scheduler)
    - 仲裁器 (Arbiter) - 轮询、优先级、公平仲裁
    - 缓冲管理器 (Buffer Manager)
