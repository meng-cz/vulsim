# 3. VulCPP 模块定义

在 VulCPP 中，模块是构建硬件设计的基本单元。每个模块都由一个同名 C++ 头文件定义，包含了模块的接口、行为和内部实现细节。

下面列举了所有能用在模块头文件中的宏定义：

## 所有在 header.hpp 中可以使用的宏

包括：
- `CONST(name, value)`
- `STRUCT(name) { ... }`
- `ALIAS(name, type)`
- `ALIAS_ARRAY1(name, type, N)`
- `ALIAS_ARRAY2(name, type, N1, N2)`

但不同与 `header.hpp` 中，这些宏定义在模块头文件中只能用来定义模块内部使用的常量、结构体和类型别名，不能定义全局可见的常量和类型。

## PARAMETER(name, value)

定义一个模块实例化参数：
- `name`：参数的名称
- `value`：参数的默认值表达式

类型固定为 `int64_t`。类似于 systemverilog 中的 parameter 或 C++ 中的模板参数，模块实例化时可以覆盖这个默认值。

## REGISTER(name, type)

定义一个模块寄存器：
- `name`：寄存器的名称
- `type`：寄存器的数据类型，可以是标准整数类型、已定义的结构体类型、bool 类型，或者其他已定义的别名类型

展开后提供以下等价声明供行为代码调用：
```cpp
const type name; // 当前周期寄存器的值，只读
void name_setnext(type value); // 延迟赋值寄存器，在下个周期生效
```

## REGISTER_INIT(name, type, init)

定义一个带有初始值的模块寄存器：
- `name`：寄存器的名称
- `type`：寄存器的数据类型，可以是标准整数类型、bool 类型
- `init`：寄存器的初始值表达式

展开后提供以下等价声明供行为代码调用：
```cpp
const type name; // 当前周期寄存器的值，只读
void name_setnext(type value); // 延迟赋值寄存器，在下个周期生效
```
并且在模块复位时，寄存器会被设置为 `init` 的值。

## REGISTER_ARRAY1(name, type, N)

**未来实现。** 特殊性能优化的一维寄存器数组定义

## REGISTER_MUL(name, type, portnum)

**未来实现。** 带有多个写端口和优先级仲裁器的寄存器定义

## WIRE(name, type, init)

临时变量定义，类似于 verilog 中的 wire，仅在当前周期内生效：
- `name`：变量的名称
- `type`：变量的数据类型，可以是标准整数类型、bool 类型
- `init`：变量的初始值表达式，在每个周期开始时被重置回这个值

展开后提供以下等价声明供行为代码调用：
```cpp
type name; // 临时变量的值，可以读写
```

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

**对 RESP 定义的参数的读取是非法的，RESP 参数在进入逻辑函数时其值是未定义的**

**由 VulCPP 事务连接器生成调用，无法在行为代码中直接调用这个函数**


## TICK_IMPL()

定义模块的时钟行为实现：
- 无参数

函数体中的逻辑代码可以包含任意合法的**无I/O的** C++ 代码，表示模块在每个时钟周期应该执行的行为逻辑。

等价于：
```cpp
void tick_impl()
```

**TICK_IMPL() 在模块里是唯一的，多个定义中仅第一个有效**


## CHILD_INSTANCE(module, name, param1=value1, param2=value2, ...)

定义一个子模块实例：
- `module`：子模块的名称
- `name`：实例的名称
- `param1=value1, param2=value2, ...`：实例化参数列表，每个参数由 `paramname=value` 定义

会以 `name_xxx` 的形式暴露出子实例所属模块中定义的 CONST、STRUCT、ALIAS、ALIAS_ARRAY1、ALIAS_ARRAY2，以及 REQUEST_PORT 和 SERVICE_PORT 接口供父模块的行为代码调用。

参数列表是可选的，如果子模块没有实例化参数或者使用默认值构造，可以省略参数列表。

## CHILD_INSTANCE_PRIO(module, name, order, param1=value1, param2=value2, ...)

定义一个带有优先级的子模块实例：
- `module`：子模块的名称
- `name`：实例的名称
- `order`：更新顺序，整数类型，顺序越大越靠前更新，负顺序表示后于 tick 更新，正顺序表示先于 tick 更新
- `param1=value1, param2=value2, ...`：实例化参数列表，每个参数由 `paramname=value` 定义

会以 `name_xxx` 的形式暴露出子实例所属模块中定义的 CONST、STRUCT、ALIAS、ALIAS_ARRAY1、ALIAS_ARRAY2，以及 REQUEST_PORT 和 SERVICE_PORT 接口供父模块的行为代码调用。

参数列表是可选的，如果子模块没有实例化参数或者使用默认值构造，可以省略参数列表。

order 参数是一个带符号整数。用于强制定义更新顺序，约束子模块内部 tick 和 SERVICE 行为的更新顺序与数据覆盖逻辑。

例如，考虑这样的场景：
- 子模块 A 定义了一个寄存器 reg 和一个 SERVICE 接口 foo，在 A 的 TICK_IMPL() 中 reg 被更新，在 foo 的 SERVICE_LOGIC_IMPL() 中 reg 同样被更新。
- 在设计中，A 中的 tick 对 reg 有更高的优先级，即 tick 对 reg 的更新应当后于 foo 中对 reg 的更新，以保证 foo 中对 reg 的更新被 tick 覆盖掉。
- 父模块 B 实例化了 A，并且通过暴露的 REQUEST 端口在自己的 tick 中调用了 A 的 foo 事务接口。

那么，A.tick < A.foo 的顺序等价于 A.tick < B.tick。即 B 在实例化 A 时需要给出一个小于0的优先级。

## USE_CHILD_PORT(instance, serv, ret, ARG(type1) arg, ..., RESP(type2) resp, ...)

声明使用了子实例中的事务端口定义，仅用于生成函数声明和代码提示，无实际意义

## CONNECT_CR_CS(srcmod, srcreq, dstmod, dstserv)

连接一个子实例请求事务端口到一个子实例服务事务端口：
- `srcmod`：请求事务端口所属的子实例名称
- `srcreq`：请求事务端口的名称，必须与子实例模块中定义的 REQUEST_PORT 的 name 相同
- `dstmod`：服务事务端口所属的子实例名称
- `dstserv`：服务事务端口的名称，必须与子实例模块中定义的 SERVICE_PORT 的 name 相同

## CONNECT_CR_S(srcmod, srcreq, dstserv)

连接一个子实例请求事务端口到父模块的一个服务事务端口：
- `srcmod`：请求事务端口所属的子实例名称
- `srcreq`：请求事务端口的名称，必须与子实例模块中定义的 REQUEST_PORT 的 name 相同
- `dstserv`：服务事务端口的名称，必须与父模块中定义的 SERVICE_PORT 的 name 相同

## CONNECT_CR_R(srcmod, srcreq, dstreq)

连接一个子实例请求事务端口到父模块的一个请求事务端口：
- `srcmod`：请求事务端口所属的子实例名称
- `srcreq`：请求事务端口的名称，必须与子实例模块中定义的 REQUEST_PORT 的 name 相同
- `dstreq`：请求事务端口的名称，必须与父模块中定义的 REQUEST_PORT 的 name 相同

## CONNECT_S_CS(srcserv, dstmod, dstserv)

连接一个父模块服务事务端口到一个子实例服务事务端口：
- `srcserv`：服务事务端口的名称，必须与父模块中定义的 SERVICE_PORT 的 name 相同
- `dstmod`：服务事务端口所属的子实例名称
- `dstserv`：服务事务端口的名称，必须与子实例模块中定义的 SERVICE_PORT 的 name 相同

## 连接约束

对于父模块中定义的服务，则必须进行以下处理之一：
1. 被连接到某个子模块的某个服务，且必须具有相同的参数与返回值定义。
2. 被用户代码实现（在 `SERVICE_LOGIC_IMPL` 中提供实现代码）。

对于子模块的外部端口中定义的请求，则必须进行以下处理之一：
1. 被连接到某个子模块的某个服务，且必须具有相同的参数与返回值定义。
2. 被连接到本模块的外部端口中的某个请求，且必须具有相同的参数与返回值定义。
3. 被连接到本模块的外部端口中的某个服务，且必须具有相同的参数与返回值定义。

如果一个请求包含返回值或包含握手信号（`has_handshake` 为 `true`），则其仅能被连接到一个服务或代码实现，不允许一对多连接。
反之，如果一个请求不包含返回值且不包含握手信号，则允许一对多连接（广播连接）。




