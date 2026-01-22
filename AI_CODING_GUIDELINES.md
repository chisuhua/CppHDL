AI_CODING_GUIDELINES.md
# CppHDL AI Coding Guidelines

## 1. Bundle系统设计与硬件连接规范

### 1.1 核心设计原则
- **类型统一性**：Bundle内部字段应使用普通`ch_uint<N>`和`ch_bool`类型（非IO类型），不需实现`as_input()`/`as_output()`方法
- **嵌套支持**：支持嵌套Bundle结构（如`Stream<Fragment<T>>`）
- **语义一致性**：保持"内部可写、端口只读"的基本语义
- **角色管理**：
  - 保留`bundle_role { internal, master, slave }`枚举
  - 默认角色为`internal`
  - 外层Bundle（如Stream）控制整体角色，内层Bundle（如Fragment）作为结构视图不独立拥有角色
- **分层抽象**：必须支持Fragment/Stream分层抽象，允许任意嵌套Bundle结构
- **初始化支持**：嵌套Bundle的初始化必须支持带偏移构造，确保位段正确切分

### 1.2 方向控制机制
- **批量设置**：通过`as_master()`和`as_slave()`方法调用`make_output()`/`make_input()`实现批量方向设置
- **模板重载**：支持1-4个字段的模板重载，仅提供语义指示用于连接时的方向检查，实际不修改字段状态
- **错误做法**：禁止在Bundle中直接使用`ch_in<T>`或`ch_out<T>`类型作为字段，这会限制灵活性
- **类型安全**：使用`set_field_direction`配合`if constexpr (requires { field.as_input(); })`检查方法存在性，确保对普通类型静默忽略

### 1.3 字段类型扩展
- `ch_uint` 和 `ch_bool` 类型应提供 `as_input()` 和 `as_output()` 方法用于方向控制
- 方向状态通过 `direction_type` 成员管理，类型为 `std::variant<std::monostate, input_direction, output_direction>`
- `as_input()` 方法调用 `set_direction(input_direction{})` 设置输入方向
- `as_output()` 方法调用 `set_direction(output_direction{})` 设置输出方向
- 这些方法的实现需保持const正确性，允许在const对象上调用
- 移除独立的 `set_direction()` 函数，直接在 `as_input()` 和 `as_output()` 方法中设置 `dir_` 成员变量
- 该设计使普通数据类型也能参与Bundle的方向控制体系

### 1.4 连接机制
- **核心连接操作**: 使用 `operator<<=` 作为硬件连接的标准赋值操作符，表示数据流方向（右侧信号驱动左侧端口/寄存器）
- **适用类型**:
  - `ch_reg<T>`：连接寄存器下一值
  - `ch_out<T>`：连接输出端口值
  - `ch_uint<N>`：连接信号值
- **连接语义**: 实现硬件连接而非传统赋值，右侧值在下一个时钟周期影响左侧对象
- **设计目的**: 统一语法，提高可读性，明确信号驱动关系
- **Bundle连接方式**:
  - 必须使用 `operator<<=` 或 `ch::core::connect(src, dst)` 进行连接，禁止直接赋值
  - `connect`用于相同类型Bundle整体连接；`<<=`用于字段级选择性连接
  - 方向语义需匹配master/slave属性
  - 自定义Bundle必须继承`bundle_base`，使用CH_BUNDLE_FIELDS宏定义字段，并实现`as_master/as_slave`
  - 禁止同一字段被重复连接
  - 在硬件描述语言上下文中，bundle之间的连接必须使用`<<=`操作符表示硬件连接语义，禁止使用`=`赋值操作符
- **递归连接**：实现基于字段布局的递归连接`do_recursive_connect`
- **重载支持**：重载`operator<<=`支持任意Bundle连接
- **自定义行为**：允许特定Bundle（如Stream）通过`do_connect_from`覆盖默认连接行为处理反向信号
- **连接语义要求**:
  - 实现`operator<<=`时必须同时处理：
    1. bundle整体的主节点连接
    2. 内部字段逐个连接，遵循源（master）→目标（slave）方向
  - master和slave方向的assign操作必须使用不同的节点类型
  - 根据字段方向创建相应assign节点，确保连接语义正确
  - 验证方向兼容性，防止非法连接

### 1.5 反射系统
- 支持模板Bundle的字段反射`CH_BUNDLE_FIELDS_T(TYPE, ...)`
- `get_bundle_layout`需能处理模板类型
- 宽度计算、命名设置等需递归处理嵌套结构

### 1.6 标准组件
- 提供开箱即用的`Fragment<T>`和`Stream<T>`模板
- `Fragment<T>`：纯数据结构，包含payload和last信号
- `Stream<T>`：传输协议载体，包含valid/ready握手机制

### 1.7 硬件抽象层规范
- `sread`/`aread`返回的`read_port`是`ch_uint`代理类型，支持隐式转换为`lnode<T>()()`
- 禁止显式类型转换或中间变量赋值，应直接作为`ch_uint`使用
- 所有输出端口遵循此代理模式

### 1.8 类型一致性要求
- 所有硬件信号类型统一持有`lnodeimpl*`作为底层实现
- 统一连接语义（`operator<<=`）、序列化接口（`to_bits/from_bits`）
- 继承公共基类（如`logic_buffer`）共享节点管理功能
- 保持方向控制接口一致性（`as_master/as_slave`）

### 1.9 Bundle类继承规范
- `bundle_base<Derived>`继承`logic_buffer<Derived>`，使其成为硬件信号并持有`lnodeimpl*`
- 子类自动获得：
  - 节点管理（`impl()`, `node_impl_`）
  - 连接语义（`operator<<=`）
  - 序列化/反序列化（`to_bits()` / `from_bits()`）
  - 字段同步
  - 方向控制（`as_master()` / `as_slave()`）

### 1.10 架构原则（统一主节点模式）
- Bundle作为一个整体使用单一主节点（`node_impl_`）表示，与其他硬件类型保持一致抽象模型
- 通过`sync_members_from_node()`方法将主节点状态同步到各字段成员，确保字段可独立访问且值一致
- 支持完整的位打包/解包机制（`to_bits`/`from_bits`），实现字段与整体间的序列化转换

### 1.11 构造与生命周期管理
- **默认构造函数**: 不创建节点（`node_impl_ = nullptr`），用于声明未初始化对象
- **带参数构造函数**: 仅当提供字面量值时才创建对应字面量节点
- **赋值操作符**: 当目标`node_impl_`为`nullptr`时复制源`impl()`指针，建立共享关系
- 禁止对已绑定节点的对象重新绑定，确保连接稳定性

### 1.12 最佳实践
- 使用C++17折叠表达式配合`std::index_sequence`安全展开元组字段
- 在连接前确保源和目标对象有效性及方向匹配
- 对复杂嵌套结构，优先分解为独立连接函数
- 保持与`ch_uint`等基础类型`operator<<=`语义的一致性

## 2. 核心设计约束

### 2.1 编译期与运行时分离
- **禁止将运行时值用于需要编译期常量的上下文**（如`bit_select`索引）
- 动态位移使用`shl<N>/shr<N>`函数而非`<<`/`>>`
- 使用`if constexpr`结合模板递归或`std::index_sequence`实现编译期循环展开

### 2.2 条件选择
- **禁止使用命令式if-else/switch-case**，必须使用`select(condition, true_value, false_value)`
- 复杂控制流通过`switch_case`等函数式接口实现

### 2.3 对象不可变性
- `ch_uint`等基本类型为不可变对象，状态保持使用显式`ch_reg`

## 3. 参数传递与生命周期

### 3.1 参数传递规则
- 禁止对按值传递的bundle执行`operator<<=`
- 物理连接必须通过引用或指针传递对象
- 函数应接受非const引用（`T&`）或指针以修改连接关系
- 推荐使用`std::array<T, N>&`传递bundle数组

### 3.2 寄存器状态访问规范
1. 当前值必须通过模拟器内部状态获取，不能通过`.value()`或`.out`访问
2. 设置下一周期值使用`reg->next = value`
3. 值更新发生在模拟器tick过程中
4. 禁止使用不存在成员或直接类型转换访问当前值

## 4. 类型规范

### 4.1 ch_bool 类型规范
- 声明布尔类型必须使用`ch_bool`而非`bool`
- 创建布尔寄存器使用`ch_reg<ch_bool>`
- 使用`1_b`表示真，`0_b`表示假
- 禁止混用`bool`与`ch_bool`，防止重载歧义

### 4.2 三元运算符类型统一
- 禁止分支返回不同位宽的`ch_uint<N>`类型
- 使用`std::common_type_t<T1, T2>`确定统一返回类型并显式转换
- 封装类型统一逻辑于模板内部，对用户透明

## 5. 单元测试与验证要求

### 5.1 测试覆盖
- 所有公共函数必须有对应单元测试，模板函数需完整覆盖
- `operator<<=` 测试场景包括：
  1. 基础连接：验证`ch_uint`、`ch_reg`、IO类型间直接连接
  2. 复杂结构：嵌套、链式连接
  3. 操作符协同：与位选择、范围选择兼容性
  4. 仿真验证：通过Simulator确认信号传播行为
- 测试约束：
  - 禁止引用未定义模块
  - 在独立组件上下文中执行
  - 不破坏原有驱动关系
  - 保持与现有构建语义一致
  - 注意bundle生命周期，避免临时对象导致连接失效

### 5.2 测试环境初始化
1. **Context管理**：
   - 每个测试用例必须创建独立的`context`实例
   - 使用`ctx_swap`工具类进行上下文切换和管理
   - `Simulator`构造函数必须传入有效的`context`指针
2. **头文件包含**：
   - 必须包含`core/context.h`以获取上下文管理功能
   - 必须包含`simulator.h`以使用仿真器功能
3. **仿真执行API**
   - 驱动仿真：使用`sim.tick()`方法推进一个时钟周期
   - 值获取：使用`sim.get_value(signal)`方法获取信号当前值
   - 禁止使用已废弃或不存在的`watch()`和`run()`方法
4. **测试设计最佳实践**
   - 参照`test_selector_arbiter.cpp`等成熟测试文件的设计模式
   - 保持测试结构一致性，便于维护和理解
   - 在断言前输出调试信息，使用二进制格式显示硬件信号值
5. **测试断言前输入输出打印规范**
   - 在单元测试中执行 `REQUIRE` 或类似断言之前，应先通过 `sim.get_value()` 等方法将被验证函数/模块的所有输入和输出值读取到本地变量中
   - 随后统一使用 `std::cout` 等输出机制打印这些本地变量的值
   - 打印时应使用二进制格式（带`0b`前缀）显示硬件信号，使用辅助函数（如`to_binary_string`）转换为指定宽度的二进制字符串
   - 打印内容应包含：request、last_grant、grant等关键信号及其当前周期标识
   - 目的：提高调试效率，确保断言与打印值一致，增强位级信号可观测性
   - 适用场景：所有涉及硬件行为验证的单元测试，特别是状态机、选择器、仲裁器等复杂逻辑以及时序逻辑和内部寄存器的验证

## 6. 代码风格与规范

### 6.1 整型类型安全规范
- API要求`uint32_t`时禁止传递`int`，索引必须使用无符号类型
- 显式转换使用`static_cast<uint32_t>`，常量使用`0u`
- 循环变量声明为`uint32_t`，模板参数约束使用`std::unsigned_integral`

### 6.2 模块连接与测试覆盖
- 禁止循环连接，必须形成有向无环图(DAG)
- 必须覆盖顶层-子模块、子模块间、多级级联、混合IO方向连接的测试
- 反馈逻辑必须通过`ch_reg`显式插入打破组合环路

### 6.3 双端口SRAM时序
- 写操作在时钟上升沿生效
- 读操作获取写入前的数据值
- 同周期写入数据不可见，需经一个完整周期后才可读取

## 7. 节点管理要求

### 7.1 节点初始化
- 所有参与连接的信号对象必须具有有效节点(`node_impl_ != nullptr`)
- 禁止使用默认构造函数创建用于连接的对象
- 应通过带值构造函数初始化，确保节点正确建立

### 7.2 API命名一致性
- 重置检查方法命名为`has_default_reset()`和`get_default_reset()`
- 节点使用者查询使用`get_users()`返回容器
- 位宽查询使用`bitwidth()`，禁用`size()`
- 零值重置使用`reset()`，禁用`fill_with(value)`

## 8. 位操作规范

### 8.1 bit_select使用规范
- 统一使用函数参数形式`bit_select(value, index)`，禁止模板语法`bit_select<Index>(value)`
- 索引应为编译期常量，范围0 ≤ index < N

## 9. 字面量与类型转换

### 9.1 字面量创建
- 使用`make_literal(value, width)`创建运行时字面量
- 进制后缀统一为`_b`,`_d`,`_h`,`_o`小写形式
- `ch_bool`转`ch_uint<N>`使用`ch_bool ? 1_d : 0_d`

## 10. 框架缺陷环境下的Bundle实现替代方案

当CppHDL框架存在`bundle_base`初始化缺陷导致段错误时，可采用以下替代实现方案：
1. **避免直接继承**：暂时移除`bundle_base<Derived>`继承以规避初始化问题
2. **保留元数据**：继续使用`CH_BUNDLE_FIELDS_T`宏定义字段元数据
3. **手动补全接口**：显式实现必要的成员方法：
   - `as_master()` / `as_slave()` 方向控制
   - `width()` 位宽查询
   - 字段同步相关接口
4. **连接语义保持**：确保仍能使用`operator<<=`进行硬件连接
5. **测试验证**：通过基础连接和序列化测试验证功能完整性
此方案可在框架修复前临时使用，既能避开已知缺陷，又能保持Bundle核心功能的可用性。

## 11. 模板Bundle的特殊处理

### 11.1 模板Bundle定义
- 对于模板Bundle，必须使用`CH_BUNDLE_FIELDS_T(...)`宏而不是`CH_BUNDLE_FIELDS(Self, ...)`宏
- 模板Bundle示例：
  ```cpp
  template<typename T>
  struct MyBundle : public bundle_base<MyBundle<T>> {
      using Self = MyBundle<T>;
      
      T data;
      ch_bool valid;
      ch_bool ready;
  
      MyBundle() = default;
      explicit MyBundle(const std::string& prefix) {
          this->set_name_prefix(prefix);
      }
  
      CH_BUNDLE_FIELDS_T(data, valid, ready)
  
      void as_master() override {
          this->make_output(data, valid);
          this->make_input(ready);
      }
  
      void as_slave() override {
          this->make_input(data, valid);
          this->make_output(ready);
      }
  };
  ```

注意：只有模板类才使用`CH_BUNDLE_FIELDS_T`，非模板类仍应使用`CH_BUNDLE_FIELDS(Self, ...)`。

## 通用编码规范

### 1. C++ 语言标准
- 使用 C++20 标准
- 利用现代 C++ 特性，如概念(Concepts)、范围(Ranges)、模块化等
- 遵循 RAII 原则进行资源管理

### 2. 命名约定
- 使用驼峰命名法：变量和函数使用小驼峰 `camelCase`
- 类名使用大驼峰 `PascalCase`
- 常量使用全大写加下划线 `CONSTANT_CASE`

### 3. 代码格式
- 使用 4 个空格缩进，禁用制表符
- 遵循 Google C++ Style Guide
- 行长度不超过 100 个字符
- 使用 clang-format 统一代码风格

## CppHDL 特定规范

### 1. 硬件类型定义
- 使用 `ch_uint<N>` 表示 N 位无符号整数
- 使用 `ch_bool` 表示布尔类型
- 使用 `ch_reg<T>` 表示寄存器类型
- 使用 `ch_in<T>`, `ch_out<T>`, `ch_inout<T>` 表示端口类型

### 2. 字面量规范
- 使用 `_d` 后缀表示十进制字面量，如 `12_d`
- 使用 `_b` 后缀表示二进制字面量，如 `1100_b`（**注意：不要在二进制字面量前添加 `0b` 前缀**）
- 使用 `_h` 后缀表示十六进制字面量，如 `0xC_h`
- 使用 `_o` 后缀表示八进制字面量，如 `14_o`
- 确保字面量的位宽不超过目标 `ch_uint<N>` 的宽度
- 优先使用`make_literal(value, width)`工厂函数创建指定宽度的运行时字面量信号
- 禁止直接使用`ch_literal_runtime`构造函数

### 3. 布尔值使用规范
- **禁止在select语句或其他硬件操作中使用C++内置的`true`/`false`**
- **必须使用CppHDL类型表示法：`1_b`表示真值，`0_b`表示假值**
- **对于ch_bool类型的变量，应使用`ch_bool(1_b)`或`ch_bool(0_b)`创建**

**正确示例**：
```
// 在select语句中使用CppHDL字面量
ch_bool result = select(condition, 1_b, 0_b);

// 初始化ch_bool寄存器
ch_reg<ch_bool> reg(0_b, "name");

// 在条件赋值中使用
reg->next = select(rst, 0_b, signal);
```

**错误示例**：
```
// 不要使用C++内置的true/false
ch_bool result = select(condition, true, false); // 错误：应使用1_b和0_b

// 不要直接使用true/false初始化
ch_reg<ch_bool> reg(false, "name"); // 错误：应使用0_b

// 避免混用类型
reg->next = select(rst, false, signal); // 错误：应使用0_b
```

### 4. 循环变量规范
- 在循环中创建表示索引或其他运行期值的硬件节点时，应优先使用`make_uint<WIDTH>(i)`而非`make_literal(i)`
- `make_uint`用于创建指定位宽的无符号整数信号，更适合循环变量场景
- `make_literal`主要用于常量值的字面量构造，一般不适用于运行期变化的循环变量
- **例外情况**：当[make_literal(i)](file:///mnt/ubuntu/chisuhua/github/CppHDL/include/core/literal.h#L163-L165)仅在赋值语句中作为右操作数使用时，是允许的例外，如 `ch_uint<N> value = make_literal(i);`
- **禁止用法**：将[make_literal(i)](file:///mnt/ubuntu/chisuhua/github/CppHDL/include/core/literal.h#L163-L165)用于比较操作或作为函数参数传递，如 `if (some_signal == make_literal(i))` 或 `auto value = make_uint<WIDTH>(make_literal(i));`
- 正确用法示例：
  ```cpp
  for (unsigned i = 0; i < N; ++i) {
      ch_uint<WIDTH> current_value = make_uint<WIDTH>(i);
      // ... 其他操作
  }
  ```
- 错误用法示例：
  ```cpp
  for (unsigned i = 0; i < N; ++i) {
      ch_uint<WIDTH> current_value = make_literal(i);  // 不推荐
      // ... 其他操作
  }
  ```

### 5. 组件定义
- 使用 `__io` 宏定义组件接口
- 继承自 `ch::Component` 创建新组件
- 实现 `create_ports()` 和 `describe()` 方法
- 在构造函数中初始化组件名称和父组件

### 6. 操作符使用
- 优先使用框架提供的自由函数而非自定义函数
- 使用 `zext` 进行零扩展，而非 `zero_extend`
- 使用 `bits<MSB, LSB>(value)` 提取位段，而非成员函数调用
- 位宽计算规则：提取WIDTH位从LSB开始，则MSB = LSB + WIDTH - 1

### 7. 仿真与测试
- 使用 `ch::Simulator` 进行仿真
- 使用 `sim.tick()` 驱动时钟，而非手动翻转时钟信号
- 使用 Catch2 框架编写测试用例
- 为 `ch_reg` 提供初始值

### 8. 模板参数使用规范
- **核心原则**：模板参数必须是编译期可确定的常量表达式
- **禁止将运行时变量（如循环变量、函数参数等）作为模板参数使用**
- **所有模板实参必须在编译时具有确定值**

**常见错误示例**：
- 在`make_uint<compute_bit_width(width)>(width)`中，如果[width](file:///mnt/ubuntu/chisuhua/github/CppHDL/include/core/literal.h#L75-L75)是函数参数（运行时值），则`compute_bit_width(width)`无法作为模板参数，因为[width](file:///mnt/ubuntu/chisuhua/github/CppHDL/include/core/literal.h#L75-L75)不是编译期常量
- 在`bits<MSB, LSB>()`、`make_literal<Value, Width>()`等模板中使用运行时变量

**正确实践**：
1. **编译期计算**：对于依赖于变量的位宽或其他类型参数，应先通过constexpr函数或模板在编译期计算出结果
2. **运行时场景**：当参数是运行时值时，应使用运行时函数而非模板函数，如使用循环和字面量操作来处理运行时值
3. **常量提取**：将复杂的表达式简化为编译期常量，避免在模板参数中直接使用算术运算
4. **替代方案**：当需要运行时行为时，考虑使用函数参数而非模板参数，或采用标签分发（tag dispatching）等技术

**设计优势**：
- 确保代码可通过编译，避免"non-type template parameter is not a constant expression"等错误
- 提高类型安全性，保证生成的硬件结构在编译期就已确定
- 符合CppHDL框架对静态类型和编译期优化的要求

### 9. 运行时值处理规范
- **运行时值处理**：当需要处理运行时值（如函数参数）时，不能直接将其用作模板参数
- **替代方法**：使用循环和字面量操作来处理运行时值，而不是依赖模板计算
- **字面量操作**：使用如`1_d`等字面量操作来构建运行时计算逻辑

**示例**：
```
// 错误：试图使用运行时值作为模板参数
ch_uint<N> mask = (ch_uint<N>(1_d) << make_uint<compute_bit_width(width)>(width)) - 1_d;

// 正确：使用循环和字面量操作处理运行时值
ch_uint<N> mask_val = 1_d;
for(unsigned i = 1; i < width; ++i) {
    mask_val = (mask_val << 1_d) + 1_d;
}
```

### 10. 硬件连接赋值操作规范
- **核心操作**：使用 `operator<<=` 作为硬件连接的标准赋值操作符
- **适用类型**：
  - `ch_reg<T>` 类型：用于连接寄存器的下一个值
  - `ch_out<T>` 类型：用于连接输出端口的值
  - `ch_uint<N>` 类型：用于连接信号的值
- **语义表达**：`<<=` 操作符表示将右侧信号连接到左侧端口或寄存器
- **连接语义**：该操作符实现硬件连接，而非传统赋值，右侧值将在下一个时钟周期影响左侧对象

**正确示例**：
```
// 连接寄存器的下一个值
ch_reg<ch_uint<8>> reg("counter");
reg <<= select(rst, 0_d, reg + 1_d);

// 连接输出端口
ch_out<ch_uint<8>> output_port;
output_port <<= some_signal;

// 连接信号
ch_uint<8> signal1, signal2;
signal1 <<= signal2;  // 将signal2连接到signal1
```

**设计优势**：
- 明确区分硬件连接与传统赋值操作
- 提供一致的硬件连接语法
- 避免对硬件信号的错误理解

## 代码结构规范

### 1. 头文件包含顺序
```
1. 对应的头文件（如果适用）
2. C 系统头文件
3. C++ 系统头文件
4. 其他库头文件
5. 项目头文件
```

### 2. 类和函数组织
- 在头文件中声明，在源文件中定义实现
- 将公共接口放在类的开头
- 使用 RAII 模式管理资源
- 避免在头文件中包含不必要的实现细节

## 错误处理和调试

### 1. 断言和检查
- 使用 `CHREQUIRE` 进行前置条件检查
- 使用 `CHCHECK` 进行条件检查
- 提供有意义的错误消息

### 2. 调试输出
- 使用 `CHDBG` 进行调试输出
- 包含有意义的上下文信息
- 包含类型信息或状态信息以提升诊断效率

## 测试规范

### 1. 测试用例编写
- 使用 Catch2 的 SECTION 机制组织测试
- 测试应覆盖边界情况
- 使用适当的测试名称和标签
- 优先使用公共 API 进行测试，而非内部实现

### 2. 验证方法
- 使用 `REQUIRE` 和 `CHECK` 进行断言
- 使用 `sim.get_value()` 获取仿真值
- 验证硬件行为符合预期

## 性能优化

### 1. 编译时优化
- 利用模板和 constexpr 进行编译时计算
- 避免不必要的运行时开销
- 使用适当的算法和数据结构

### 2. 硬件优化
- 避免创建不必要的中间节点
- 重用已创建的节点以减少重复
- 遵循硬件语义，一个物理连接点对应一个唯一的节点表示

## 文档规范

### 1. 注释风格
- 使用 Doxygen 风格的注释
- 为公共接口提供详细文档
- 解释复杂算法和设计决策

### 2. 代码文档
- 在函数和类定义前添加文档注释
- 说明参数、返回值和异常情况
- 提供使用示例

## AI 特定指导

### 1. 模板使用
- 当不确定时，参考现有实现而非自行发明
- 优先使用框架提供的公共 API
- 避免使用未在项目中验证的 C++ 特性

### 2. 代码生成
- 确保生成的代码符合上述所有规范
- 生成的代码应易于理解和维护
- 提供适当的错误处理和边界检查

### 3. 问题修复
- 修复问题时保持与现有架构一致
- 优先使用框架提供的标准操作符进行测试
- 验证修改后的代码符合预期语义

## 提示词编写模板

### 1. 功能实现提示词模板
```
请根据CppHDL框架规范实现[功能描述]，遵循以下要求：
- 使用正确的CppHDL API和语法
- 遵循自由函数优先原则
- 使用正确的模板参数语法
- 包含必要的头文件
- 遵循CppHDL_UsageGuide.md中的最佳实践
```

### 2. 问题修复提示词模板
```
修复以下CppHDL代码中的错误：
[错误代码或错误信息]

要求：
- 识别错误原因，特别是CppHDL特有的问题
- 提供符合CppHDL规范的修复方案
- 确保修复后的代码遵循文档规范
- 解释修复的原因和影响
```

### 3. 代码优化提示词模板
```
优化以下CppHDL代码以提高性能和可读性：
[待优化代码]

要求：
- 遵循CppHDL_UsageGuide.md中的最佳实践
- 使用更高效的CppHDL API
- 保持代码的可维护性和类型安全性
- 符合函数式编程风格推荐
```

## 常见错误和避免方式

### 1. 模板参数错误
- 错误：`zext<N>(other)`导致的模板参数推导失败
- 正确：确认模板参数顺序和类型，如`zext<ch_uint<M>, N>(other)`

### 2. 函数调用错误
- 错误：使用不存在的成员方法如`ch_uint.slice()`
- 正确：使用自由函数如`bits<MSB, LSB>()`

### 3. 字面量构造错误
- 错误：直接使用构造函数创建字面量
- 正确：使用`make_literal(value, width)`工厂函数

### 4. 布尔值使用错误
- 错误：在select语句中使用C++内置的true/false
- 正确：使用CppHDL字面量如1_b/0_b或ch_bool类型

### 5. 条件选择操作符使用规范
- **核心实现**：使用`select`函数作为CppHDL项目中条件选择操作的标准方式，而非`mux`
- **语义表达**：`select(condition, true_val, false_val)`具有明确的三元选择语义
- **推荐使用**：对于基础条件选择功能，应优先使用`select`函数，因其为直接、清晰的基础构建块
- **避免使用**：避免使用`ch::mux`函数，保持代码简洁性和一致性

**正确示例**：
```
// 使用select作为条件选择的标准方式
auto result = select(condition, value_a, value_b);
```

**错误示例**：
```
// 不要使用ch::mux
auto result = ch::mux(condition, value_a, value_b); // 错误：应使用select函数
```

### 6. 寄存器状态访问规范
- **当前值获取**：访问寄存器当前周期的输出值时，应直接使用寄存器对象本身，因其继承自对应类型并可隐式转换为数据值
- **下一值设置**：设置寄存器下一周期的值时，必须通过`->next`语法进行赋值
- **禁止访问**：禁止使用不存在的成员（如`.out`）来访问寄存器当前值

**正确示例**：
```
// 直接使用寄存器对象获取当前值
ch_uint<4> current_value = counter;

// 使用->next语法设置下一周期的值
counter->next = new_value;
```

**错误示例**：
```
// 不要使用不存在的.out成员
ch_uint<4> current_value = counter.out; // 错误：应直接使用counter

// 不要直接赋值给寄存器对象
counter = new_value;  // 错误：应使用counter->next
```

### 7. ch_bool类型使用规范
- **类型声明**：声明布尔类型时使用`ch_bool`而非C++内置的`bool`类型
- **寄存器初始化**：创建布尔类型寄存器时使用`ch_reg<ch_bool>`类型
- **字面量使用**：在布尔上下文中使用`1_b`表示真值，`0_b`表示假值，而非C++内置的true/false

**正确示例**：
```
// 声明ch_bool类型的寄存器
ch_reg<ch_bool> flag(0_b, "flag_reg");

// 在tick中设置下一个值
flag->next = select(condition, 1_b, 0_b);
```

**错误示例**：
```
// 不要使用C++内置bool类型
ch_reg<bool> flag(false, "flag_reg");  // 错误：应使用ch_bool和0_b

// 不要使用C++内置true/false
flag->next = select(condition, true, false);  // 错误：应使用1_b和0_b
```

## Bundle 字段方向控制规范

- **字段类型选择**：在Bundle定义中，使用普通 `ch_uint` 和 `ch_bool` 类型作为字段，而非IO类型（如 `ch_in<T>`、`ch_out<T>`）
- **方向控制方法**：通过在Bundle类中实现 `as_master()` 和 `as_slave()` 方法来控制Bundle内字段的方向
  - `as_master()` 方法中调用 `make_output(field1, field2, ...)` 将字段设置为输出方向
  - `as_slave()` 方法中调用 `make_input(field1, field2, ...)` 将字段设置为输入方向
- **普通字段类型特点**：普通 `ch_uint` 和 `ch_bool` 类型同样具有 `set_direction()` 方法，但Bundle方向设置主要用于连接验证
- **语义说明**：Bundle的方向设置更多是提供语义上的主从关系指示，而非直接修改字段状态
- **正确示例**：
  ```cpp
  struct TestBundle : public bundle_base<TestBundle> {
      using Self = TestBundle;
      using bundle_base<TestBundle>::bundle_base;
      ch_uint<8> data;
      ch_bool valid;

      TestBundle() = default;

      CH_BUNDLE_FIELDS(Self, data, valid)

      void as_master() override { this->make_output(data, valid); }
      void as_slave() override { this->make_input(data, valid); }
  };
  ```

## 检查清单

在AI辅助编码时，请确保：
- [ ] 所有代码遵循CppHDL_UsageGuide.md中的规范
- [ ] 模板函数使用正确的语法
- [ ] 位操作使用标准自由函数
- [ ] 组件生命周期方法使用正确
- [ ] 字面量构造使用工厂函数
- [ ] 使用CppHDL类型（1_b/0_b）替代C++内置true/false
- [ ] 代码风格与项目保持一致
- [ ] 包含了适当的错误处理和验证