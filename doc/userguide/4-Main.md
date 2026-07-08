# 4. VulCPP Main 模块定义

在 VulCPP 中，Main 模块是仿真的入口模块，负责定义仿真环境、输入输出接口以及仿真流程。Main 模块的代码可以包含不可综合的输入输出代码，这些代码仅用于仿真时与外部环境进行交互。

Main 模块需要定义与顶层硬件设计模块方向相反、参数相同的事务接口，这些接口会被隐式连接到顶层模块的事务接口上。例如，如果顶层模块有一个 output Request 事务接口，那么 Main 模块就需要定义一个对应的 output Service 事务接口来响应这个请求。

Main 模块中还需要定义一个 SIMULATION() 函数，作为仿真的入口点。在 SIMULATION() 函数中，我们可以编写仿真流程的代码，例如执行一定数量的时钟周期、触发事务接口、监控仿真状态等。

下面列举了所有能用在 Main 模块头文件中的宏定义：

## 所有在 header.hpp 中可以使用的宏

包括：
- `CONFIG(name, value)`
- `STRUCT(name) { ... }`
- `ENUM(name) { ... }`
- `ALIAS(name, type)`
- `ALIAS_ARRAY1(name, type, N)`
- `ALIAS_ARRAY2(name, type, N1, N2)`

但不同与 `header.hpp` 中，这些宏定义在模块头文件中只能用来定义模块内部使用的常量、结构体和类型别名，不能定义全局可见的常量和类型。

## include

在 Main 模块中可以使用 `#include <xxx>` 来包含一个标准库头文件。由于 Main 模块不参与综合，所以可以包含任何合法的 C++ 头文件来使用其中定义的类型和函数。

`#include "xxx"` 形式的项目内头文件主要用于编辑期代码检查与补全，不会被直接保留到生成的仿真代码中。

下面几个特殊头文件名字不会被解析并保留到生成的仿真代码中：
```cpp
inline constexpr std::array<std::string_view, 7> VulLibFiles = {
    "vullib.h",
    "common.h",
    "queue.hpp",
    "ram.hpp",
    "storage.hpp",
    "fixint.hpp",
    "vcdrecord.hpp",
};
inline constexpr std::array<std::string_view, 4> VulEscapedHeaders = {
    "defhelper.hpp",
    "run.hpp",
    "header.hpp",
    "header.h",
};
```

## PARAMETER(name, value)

定义了在本次仿真中 Top 模块实例化参数的覆盖值：
- `name`：参数的名称，必须与 Top 模块中定义的 PARAMETER 的 name 相同
- `value`：参数的覆盖值表达式

如果 TopModule 中有的参数没有被指定覆盖值，那么它们将使用在 TopModule 中定义的默认值进行实例化。

## GLOBAL() { ... }

定义一组全局变量，这些变量在整个仿真过程中都保持可见和可访问。

示例：
```cpp
GLOBAL() {
    uint64_t cycle = 0; // 定义一个全局变量 cycle 来跟踪仿真时钟周期数
}
```

## VAR(type, name)

定义一个全局变量：
- `type`：变量的类型，可以是标准整数类型、已定义的结构体类型、bool 类型，或者其他已定义的别名类型
- `name`：变量的名称

Main 模块中无法定义 REGISTER 或 WIRE，取而代之的是与标准 C++ 成员变量行为完全一致的 VAR 定义的全局变量。这些变量在仿真流程中可以被任意读取和写入，并且其值在每个时钟周期之间保持不变。

## VAR_INIT(type, name, init)

定义一个带初始值的全局变量：
- `type`：变量的类型，可以是标准整数类型、已定义的结构体类型、bool 类型，或者其他已定义的别名类型
- `name`：变量的名称
- `init`：变量的初始值

## SIMULATION() { Logic Code Body }

定义仿真入口函数。这个代码仅会在仿真流程中被执行一次。该函数返回意为仿真结束。

其中可以调用自身定义的 REQUEST 事务接口来触发顶层模块的行为。
如果 Main 中声明了 `QUERY(name, type);`，也可以在这里直接调用对应函数来读取顶层模块当前周期稳定状态。

三个特殊的函数可以被调用，用于推进仿真时钟、复位和退出：
- `void sim_nextcycle()`：推进一个完整时钟周期，依次处理事务交互并提交寄存器状态更新
- `void sim_reset()`：执行一次仿真复位，所有寄存器将被赋值为它们的复位值
- `void sim_exit()`：退出仿真流程

## REQUEST_PORT(name, ret, ARG(type1) arg, ..., RESP(type2) resp, ...)

定义一个请求事务接口：
- `name`：事务接口的名称
- `ret`：事务接口的返回值类型，可以是 void 或 bool，bool 表示事务包含一个 valid-ready 握手行为
- `ARG(type) arg`：事务接口的参数列表，每个参数由 `ARG(type) argname` 定义，**参数只读**，type 可以是标准整数类型、已定义的结构体类型、bool 类型，或者其他已定义的别名类型
- `RESP(type) resp`：事务接口的响应参数列表，每个响应参数由 `RESP(type) respname` 定义，**参数只写**，type 可以是标准整数类型、已定义的结构体类型、bool 类型，或者其他已定义的别名类型

展开后提供以下等价声明供行为代码调用：
```cpp
ret name(const type1& arg1, ..., type2& resp1, ...); // 事务接口的函数声明
```

## QUERY(name, type);

声明需要访问的顶层模块 `QUERY` 接口：
- `name`：查询接口名称，必须与顶层模块中的 `QUERY` 名称一致
- `type`：返回值类型，必须与顶层模块中的 `QUERY` 返回类型一致

与普通硬件模块不同，Main 中的 `QUERY` 只能写成无代码块的声明形式：

```cpp
QUERY(status, Status);
```

在生成的仿真代码中，这会被构造成一个转发函数，等价于调用顶层实例上的同名 `QUERY`：

```cpp
Status s = status();
```

这通常用于单元测试、调试断言或读取顶层模块的内部稳定状态。

## SERVICE_PORT(name, ret, ARG(type1) arg, ..., RESP(type2) resp, ...)

定义一个服务事务接口：
- `name`：事务接口的名称
- `ret`：事务接口的返回值类型，可以是 void 或 bool，bool 表示事务包含一个 valid-ready 握手行为
- `ARG(type) arg`：事务接口的参数列表，每个参数由 `ARG(type) argname` 定义，**参数只读**，type 可以是标准整数类型、已定义的结构体类型、bool 类型，或者其他已定义的别名类型
- `RESP(type) resp`：事务接口的响应参数列表，每个响应参数由 `RESP(type) respname` 定义，**参数只写**，type 可以是标准整数类型、已定义的结构体类型、bool 类型，或者其他已定义的别名类型

展开后提供以下等价声明供行为代码调用：
```cpp
ret name(const type1& arg1, ..., type2& resp1, ...); // 事务接口的函数声明
```


## SERVICE_COND_IMPL(name, ARG(type1) arg, ..., RESP(type2) resp, ...) { Logic Code Body }

定义一个服务事务接口的握手条件实现：
- `name`：事务接口的名称，必须与之前定义的 SERVICE_PORT 的 name 相同
- `ARG(type) arg`：事务接口的参数列表，每个参数由 `ARG(type) argname` 定义，**参数只读**，type 可以是标准整数类型、已定义的结构体类型、bool 类型，或者其他已定义的别名类型
- `RESP(type) resp`：可以省略，因为这里面不能写任何响应参数的赋值逻辑

函数体中的逻辑代码应该只包含一个 return 语句，返回一个 bool 类型的表达式，表示 ready 信号的触发条件。

等价于：
```cpp
bool name(const type1& arg1, ..., type2& resp1, ...)
```

示例：
```cpp
SERVICE_COND_IMPL(recv, ARG(uint8_t) d) {
    return (cycle & 1) == 0;
}
```

**由 VulCPP 事务连接器生成调用，无法在行为代码中直接调用这个函数**

**在这个函数中任何对参数或模块状态的写入都是非法的**

**该定义仅对返回值为 bool 的 SERVICE_PORT 有效**


## SERVICE_LOGIC_IMPL(name, ARG(type1) arg, ..., RESP(type2) resp, ...) { Logic Code Body }

定义一个服务事务接口的响应逻辑实现：
- `name`：事务接口的名称，必须与之前定义的 SERVICE_PORT 的 name 相同
- `ARG(type) arg`：事务接口的参数列表，每个参数由 `ARG(type) argname` 定义，**参数只读**，type 可以是标准整数类型、已定义的结构体类型、bool 类型，或者其他已定义的别名类型
- `RESP(type) resp`：事务接口的响应参数列表，每个响应参数由 `RESP(type) respname` 定义，**参数只写**，type 可以是标准整数类型、已定义的结构体类型、bool 类型，或者其他已定义的别名类型

函数体中的逻辑代码可以包含任意合法的**无I/O的** C++ 代码，表示当握手条件满足时应该执行的行为逻辑。

等价于：
```cpp
void name(const type1& arg1, ..., type2& resp1, ...)
```
示例：
```cpp
SERVICE_LOGIC_IMPL(recv, ARG(uint8_t) a, RESP(uint8_t) b) {
    b = a + 1;
}
```

在服务响应代码中也可以调用 `sim_exit()` 来结束仿真流程。

**对 RESP 定义的参数的读取是非法的，RESP 参数在进入逻辑函数时其值是未定义的**

**由 VulCPP 事务连接器生成调用，无法在行为代码中直接调用这个函数**
