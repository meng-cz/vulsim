# 1. VulCPP 简介

VulCPP 是一个基于 C++ 的硬件描述框架，旨在提供一种高效、灵活的方式来设计和模拟 RTL 电路。它利用 C++ 的强大功能，使得开发者能够以更自然的方式描述硬件行为，同时享受 C++ 的性能优势。

与传统硬件描述语言最大的不同是：**在这里，你不是在描述硬件，而是在描述行为**

你不需要考虑一个模块：

- 组合逻辑如何搭建
- 信号如何传递
- 如何与其他模块交互或握手

而是专注于**行为**，让 Vul 帮你处理底层细节。我们定义**行为**就是：**模块应当如何更新自身的寄存器**。**行为**使用 C++ 代码描述。

因此，在设计一个 VulCPP 模块时，你需要关注：

- 模块有哪些时序状态（寄存器）
- 每个时钟周期模块应该执行什么行为
- 这个模块会触发其他模块的什么**事务**，其他模块触发的**事务**会导致这个模块的什么行为

## 1.1. 基础概念：模块、事务、和连接

我们使用一个简单的 Producer-Consumer 模块来说明这些概念：

```cpp
// <Pruducer.hpp>

// include 帮助头文件以使能文件内语法检查和代码提示
#include <defhelper.hpp>

// 定义 Producer 模块中的两个寄存器
REGISTER(cycle, uint8_t) { // cycle 寄存器，数据类型为 uint8_t，初始值为 0
    cycle = 0; // 复位时的赋值代码
}
REGISTER(count, uint8_t) { // count 寄存器，数据类型为 uint8_t，初始值为 0
    count = 0;
}

// 定义 Producer 模块对外的事务接口
REQUEST_READY(send, ARG(uint8_t) d); // 这个模块可能会触发一个 send 事务，参数是一个 uint8_t 类型的数据 d，事务包含一个 bool 返回值作为 valid-ready 握手行为

// 定义 Producer 模块的行为
// TICK_IMPL 定义了模块在每个时钟周期应该执行的行为
TICK_IMPL() {
    cycle_setnext(cycle + 1);   // 每个时钟周期，cycle 寄存器的值通过延迟赋值增加 1，在下个周期生效
    if (send(cycle)) {          // 尝试触发一个 send 事务
        count_setnext(count + 1); // 如果 send 事务成功触发（即 valid-ready 握手成功），则执行 count 寄存器的延迟赋值，增加 1，在下个周期生效
    }
}
```

```cpp
// <Consumer.hpp>

#include <defhelper.hpp>

// 定义 Consumer 模块中的寄存器
REGISTER(cycle, uint8_t) {
    cycle = 0;
}
REGISTER(sum, uint8_t) {
    sum = 0;
}

// 定义 Consumer 模块对外的事务接口
REQUEST(output, void, ARG(uint8_t) s); // 这个模块可能会触发一个 output 事务，参数是一个 uint8_t 类型的数据 s，事务不包含 valid-ready 握手行为

SERVICE_READY(recv, (cycle & 1) == 0, ARG(uint8_t) d) {
    // 这个模块可以响应一个 recv 事务，参数是一个 uint8_t 类型的数据 d，_READY 表示事务包含一个 valid-ready 握手行为， 只有当握手条件 "(cycle & 1) == 0" 成立时才会成功响应事务
    // 事务被成功触发时进行的行为
    sum_setnext(sum + d);
    output(sum); // 触发一个 output 事务，参数是 sum 寄存器的当前值
}

// 定义 Consumer 模块的行为
// TICK_IMPL 定义了模块在每个时钟周期应该执行的行为
TICK_IMPL() {
    cycle_setnext(cycle + 1);
}
```

```cpp
// <TopModule.hpp>

#include <defhelper.hpp>

// 定义 Top 模块对外的事务接口
REQUEST_PORT(output, void, ARG(uint8_t) s);

// 定义 Top 模块的子模块实例
CHILD_INSTANCE(Producer, prod); // 创建一个 Producer 模块的实例，命名为 prod
CHILD_INSTANCE(Consumer, cons); // 创建一个 Consumer 模块的实例，命名为 cons

// 连接子模块的事务接口
CONNECT_CR_CS(prod, send, cons, recv); // 连接 prod 模块的 send 事务接口和 cons 模块的 recv 事务接口，建立 valid-ready 握手的模块交互关系
CONNECT_CR_R(cons, output, output); // 连接 cons 模块的 output 事务接口和 Top 模块的 output 事务接口，将 cons 模块的 output 事务暴露为 Top 模块的 output 事务
```

在这个例子中，我们定义了一个 Producer 模块和一个 Consumer 模块。Producer 模块每个时钟周期都会尝试触发一个 send 事务，Consumer 模块则根据特定条件响应 recv 事务，并在成功响应后触发 output 事务。最后，我们在 Top 模块中实例化了这两个模块，并连接了它们的事务接口，实现了模块之间的交互。

**事务**端口是 VulCPP 中用于模块间通信的接口。它们可以分为两个方向：
- **REQUEST_PORT**：表示模块可能会触发的事务接口
- **SERVICE_PORT**：表示模块可以响应的事务接口

**事务**之间的**连接**发生在定义了子模块实例的父模块中，**连接事务端口A到事务端口B**等价于**当事务A被触发时，实质上是在触发事务B**。一次**事务**的触发本身就隐含了一个 valid 信号，当事务函数的返回类型为 void 时，代表该事务不包含 ready 信号；当返回类型为 bool 时，代表该事务包含一个 valid-ready 握手，只有当握手成功时才会执行事务相关的行为。

除了 `REQUEST` / `SERVICE` 这两类会参与连接与握手的事务接口外，VulCPP 还支持一种特殊的只读查询接口 `QUERY`。`QUERY` 不参与连接，没有 `ARG` / `RESP` 参数，而是直接声明一个返回类型，用来描述“当前周期稳定状态下的观测值”。`QUERY` 可以读取本模块寄存器当前值，也可以调用子实例或内置组件提供的 query 接口，并把结果组合成普通整数、别名类型或 `STRUCT` 类型返回给父模块使用。

## 1.2. 基础概念：Main 模块与仿真

在 VulCPP 中，有两种模块（类似于 Vivado 中的 source 文件和 simulation 文件）：
- **普通硬件模块**：用于描述硬件行为的模块。其中不能包含任何输入输出代码或其他未定义的函数、变量访问
- **Main 模块**：作为仿真入口的模块。其中可以包含不可综合的输入输出代码

一个可运行的 VulCPP 项目必须包含一个 **顶层硬件设计模块** 和一个 **Main 模块**。在上述的例子中，`TopModule` 可以作为顶层硬件设计模块，而 `Main` 模块则是仿真入口模块：

```cpp
// <Main.cpp>

#include <defhelper.hpp>
#include <run.hpp> // 专用于 Main 模块的帮助头文件

TOP("TopModule.hpp") // 声明 Main 模块绑定的顶层模块文件是 ./TopModule.hpp
PROJECT(".") // 声明项目根目录

// 定义一个全局变量 tick_count 来跟踪仿真的时钟周期数
GLOBAL() {
    // 这里的内容会对所有Service和Simulation函数可见，可以用来存储全局状态
    uint64_t tick_count = 0; // tick_count 变量，初始值为 0
}

// Main 模块需要定义与顶层硬件设计模块方向相反、参数相同的事务接口，这些会被隐式连接到顶层模块的事务接口上
SERVICE(output, ARG(uint8_t) s) { // 对应到 Top 模块的 output Request 接口
    // 这里是仿真代码，不需要可综合，所以可以使用输入输出代码来观察仿真结果
    printf("%ld: Output: %u\n", tick_count, s);
}

// SIMULATION() 定义了仿真的入口，在这里我们执行 20 个时钟周期的仿真
SIMULATION() {
    for (tick_count = 0; tick_count < 20; ++tick_count) {
        sim_execute(); // 执行一个时钟周期的仿真，处理所有模块的行为和事务交互
        sim_commit(); // 提交当前时钟周期的状态更新，使得所有寄存器的延迟赋值生效
    }
}
```

在这个 `Main` 模块中，我们定义了一个 `output` 事务接口来响应来自 `Top` 模块的 `output` 事务。当 `output` 事务被触发时，我们在控制台打印出当前的时钟周期数和输出值。通过 `SIMULATION()` 函数，我们执行了一个简单的仿真循环，模拟了 20 个时钟周期的系统行为。

同样的，Main 模块中也能包含 Request 事务接口，这些接口会被隐式连接到顶层模块的 Service 事务接口上。在 SIMULATION() 函数中，我们也可以通过函数调用直接触发这些 Request 事务接口来模拟外部输入对系统的影响。

## 1.3. 构建与运行仿真

要构建和运行 VulCPP 仿真，我们需要：
- `vulsimgen`：VulCPP 仿真代码生成工具，用于将 VulCPP 项目转换为可执行的仿真代码
- 任意 C++ 编译器：用于编译生成的仿真代码

在代码仓库中提供了上述的示例代码，位于 `example/prodcon` 目录下。假定现在已经在 `build` 目录下完成了工具的编译，我们可以使用以下命令来构建和运行仿真：

如果 `Main.cpp` 中已经通过 `TOP(...)` / `PROJECT(...)` 声明了绑定的顶层模块和项目目录，那么只需要传入 `-m`；如果没有声明，或者想临时切换目标，也可以继续使用 `-t` / `-p` 进行覆盖。

```bash
# 生成仿真代码
./vulsimgen -m example/prodcon/Main.cpp
# -m: 指定 Main 模块的头文件路径
# -t: 可选，用于覆盖 Main 中通过 TOP(...) 声明的顶层模块路径
# -p: 可选，用于覆盖 Main 中通过 PROJECT(...) 声明的项目根目录路径
# -l: 指定 VulCPP 库文件的路径（默认./vullib/）
# -o: 指定生成的仿真代码输出目录（默认./sim_out/）

# 编译生成的仿真代码 (build.sh 默认使用 g++)
cd sim_out
source build.sh
# 运行仿真
./Main
```

## 1.4. 后续

在后续章节中，我们将详细介绍 VulCPP 中的各种定义和语法规则，帮助你更深入地理解如何使用 VulCPP 来设计和模拟复杂的硬件系统。
