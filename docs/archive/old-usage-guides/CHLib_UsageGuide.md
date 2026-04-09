## CHLib 模块指南

CppHDL 的 CHLib 是一个硬件构建库，提供了一系列模块化组件，用于快速构建复杂的数字电路。以下是按文件组织的模块说明：

### arithmetic.h - 算术运算模块
提供基本的算术运算功能：
- `add(a, b)` - 加法运算，返回两数之和
- `subtract(a, b)` - 减法运算，返回两数之差
- `multiply(a, b)` - 乘法运算，返回两数之积（2*N位宽）
- `add_with_carry(a, b, carry_in)` - 带进位加法器，返回和与进位输出
- `sub_with_borrow(a, b, borrow_in)` - 带借位减法器，返回差与借位输出
- `divide(a, b)` - 除法运算（整数除法）
- `modulo(a, b)` - 取模运算
- `negate(a)` - 取负运算
- `abs(a)` - 绝对值运算

### arithmetic_advance.h - 高级算术运算模块
提供高级算术运算功能：
- `square_root<N>(a)` - 计算N位数值的平方根
- `power<N>(base, exp)` - 计算幂运算
- `logarithm<N>(a)` - 计算对数值
- `fixed_point_multiply<N, F>(a, b)` - 定点数乘法

### bitwise.h - 位运算模块
提供基本的位运算功能：
- `bitwise_and<N>(a, b)` - 按位与运算
- `bitwise_or<N>(a, b)` - 按位或运算
- `bitwise_xor<N>(a, b)` - 按位异或运算
- `bitwise_not<N>(a)` - 按位取反运算
- `bitwise_nand<N>(a, b)` - 按位与非运算
- `bitwise_nor<N>(a, b)` - 按位或非运算
- `bitwise_xnor<N>(a, b)` - 按位同或运算
- `population_count<N>(a)` - 计算比特中1的个数
- `leading_zeros<N>(a)` - 计算前导零的个数
- `trailing_zeros<N>(a)` - 计算尾随零的个数

### combinational.h - 组合逻辑模块
提供常用的组合逻辑功能：
- `priority_encoder<N>(input)` - 优先级编码器，将one-hot编码转换为二进制索引
- `binary_encoder<N>(input)` - 二进制编码器
- `binary_decoder<N>(input)` - 二进制解码器，将二进制索引转换为one-hot编码
- `odd_parity_gen<N>(input)` - 奇校验生成器
- `even_parity_gen<N>(input)` - 偶校验生成器

### converter.h - 数据转换模块
提供数据格式转换功能：
- `binary_to_gray<N>(input)` - 二进制转格雷码
- `gray_to_binary<N>(input)` - 格雷码转二进制
- `binary_to_bcd<N>(input)` - 二进制转BCD码
- `bcd_to_binary<N>(input)` - BCD码转二进制
- `unsigned_to_signed<N>(input)` - 无符号转有符号
- `signed_to_unsigned<N>(input)` - 有符号转无符号

### fifo.h - FIFO存储器模块
提供FIFO队列功能：
- `sync_fifo<DATA_WIDTH, ADDR_WIDTH>()` - 同步FIFO，支持同步读写操作
- `async_fifo<DATA_WIDTH, ADDR_WIDTH>()` - 异步FIFO，支持跨时钟域操作
- `fwft_fifo<DATA_WIDTH, ADDR_WIDTH>()` - First-Word Fall-Through FIFO，读取延迟为0
- `is_empty(count)` - 检查FIFO是否为空
- `is_full(count)` - 检查FIFO是否为满

### fragment.h - 数据分片模块
提供数据分片和重组功能：
- `fragment(data, size)` - 将大数据分片
- `defragment(fragments)` - 将分片数据重组

### if.h - 条件选择模块
提供条件选择功能：
- `if_(condition).then(value1).else_(value2)` - 条件选择函数式接口
- `select(condition, true_value, false_value)` - 三元选择操作（基于mux）

### if_stmt.h - 条件语句模块
提供高级条件语句功能：
- `if_then_else(condition, then_func, else_func)` - 函数式条件语句
- `switch_case(value, cases...)` - 多路分支语句
- `switch_pairs(cases...)` - 基于键值对的分支语句

### logic.h - 基本逻辑门模块
提供基本逻辑门功能：
- `and_gate<N>(a, b)` - N位与门
- `or_gate<N>(a, b)` - N位或门
- `not_gate<N>(a)` - N位非门
- `xor_gate<N>(a, b)` - N位异或门
- `nand_gate<N>(a, b)` - N位与非门
- `nor_gate<N>(a, b)` - N位或非门
- `xnor_gate<N>(a, b)` - N位同或门
- `multi_and_gate(inputs)` - 多输入与门
- `mux<N, M>(inputs, sel)` - M选1多路选择器
- `mux2<N>(in0, in1, sel)` - 2选1多路选择器
- `mux4<N>(in0, in1, in2, in3, sel)` - 4选1多路选择器
- `demux<N, M>(input, sel)` - 分配器，将输入分配到M个输出之一

### memory.h - 存储器模块
提供存储器功能：
- `ch_mem<T, SIZE>` - 通用存储器类型
- `sread(addr, enable)` - 同步读取操作
- `aread(addr)` - 异步读取操作
- `write(addr, data, enable)` - 写入操作

### onehot.h - One-Hot编码模块
提供One-Hot编码相关功能：
- `onehot_to_binary<N>(onehot)` - 将N位One-Hot编码转换为二进制
- `binary_to_onehot<N>(binary)` - 将二进制转换为N位One-Hot编码
- `is_onehot<N>(value)` - 检查值是否为One-Hot编码
- `priority_onehot<N>(input)` - 优先级One-Hot编码

### selector_arbiter.h - 选择器与仲裁器模块
提供选择和仲裁功能：
- `priority_selector<N>(request)` - 优先级选择器，低位优先级更高
- `round_robin_arbiter<N>(request)` - 循环仲裁器，确保公平访问
- `round_robin_selector<N>(request, last_grant)` - 简化版循环仲裁器

### sequential.h - 时序逻辑模块
提供时序逻辑功能：
- `d_flip_flop(data, clk, rst)` - D触发器
- `jk_flip_flop(j, k, clk, rst)` - JK触发器
- `t_flip_flop(t, clk, rst)` - T触发器
- `counter<WIDTH>(clk, rst, enable, load, data, up_down)` - 可控计数器
- `up_counter<WIDTH>(clk, rst, enable)` - 向上计数器
- `down_counter<WIDTH>(clk, rst, enable)` - 向下计数器
- `up_down_counter<WIDTH>(clk, rst, up, down)` - 可逆计数器
- `shift_register<WIDTH>(clk, rst, enable, serial_in, parallel_in, shift_right)` - 移位寄存器

### stream.h - 流处理模块
提供流式数据处理功能：
- `Stream<T>` - 带反压的数据流接口（包含payload, valid, ready信号）
- `Flow<T>` - 无反压的数据流接口（包含payload, valid信号）
- `stream_fifo<T, DEPTH>()` - 带反压的FIFO
- `stream_fork<T, N_OUTPUTS>()` - 将输入流复制到多个输出流
- `stream_join<T, N_INPUTS>()` - 等待所有输入流都有效后传输
- `stream_arbiter_round_robin<T, N_INPUTS>()` - 流仲裁器（轮询方式）
- `stream_mux<T, N_INPUTS>()` - 流多路选择器
- `stream_demux<T, N_OUTPUTS>()` - 流解复用器

### switch.h - Switch语句模块
提供高级Switch语句功能：
- `switch_<T>(value)` - 基础Switch语句
- `switch_case<T>(value, cases...)` - 带默认值的Switch语句
- `switch_pairs<T>(pairs...)` - 基于std::pair的Switch语句
- `switch_parallel<T>(value, cases...)` - 并行比较Switch语句
- `switch_sequential<T>(value, cases...)` - 串行比较Switch语句

### axi_lite.h - AXI4-Lite总线接口模块
提供AXI4-Lite总线接口功能：
- `axi_lite_bundle` - AXI4-Lite总线bundle定义
- `axi_lite_read_master` - AXI4-Lite读主设备
- `axi_lite_write_master` - AXI4-Lite写主设备
- `axi_lite_read_slave` - AXI4-Lite读从设备
- `axi_lite_write_slave` - AXI4-Lite写从设备