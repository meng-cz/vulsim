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

## REGISTER(name, type) { ... }

定义一个模块寄存器：
- `name`：寄存器的名称
- `type`：寄存器的数据类型，可以是标准整数类型、已定义的结构体类型、bool 类型，或者其他已定义的别名类型
- `{ ... }`：寄存器的复位赋值代码块

使用示例：
```cpp
REGISTER(counter, uint8_t) {
    counter = 0; // 寄存器的复位赋值逻辑，在模块复位时执行
}
```

对于Struct类型的寄存器，复位赋值代码块必须包含对寄存器所有字段的赋值逻辑，以保证寄存器在复位时被完全初始化：
```cpp
STRUCT(MyStruct) {
    uint8_t a;
    bool b;
}
REGISTER(myreg, MyStruct) {
    myreg.a = 0;
    myreg.b = false;
}
```

展开后提供以下等价声明供行为代码调用：
```cpp
const type name; // 当前周期寄存器的值，只读，仅对非结构体类型有效
const type& name_get(); // 获取当前周期寄存器的值的引用，适用于结构体类型
void name_setnext(type value); // 延迟赋值寄存器，在下个周期生效
```

## REGISTER_MUL(name, type, portnum) { ... }

带有多个写端口和优先级仲裁器的寄存器定义:
- `name`：寄存器的名称
- `type`：寄存器的数据类型，可以是标准整数类型、已定义的结构体类型、bool 类型，或者其他已定义的别名类型
- `portnum`：寄存器的写端口数量，整数类型，必须大于0
- `{ ... }`：寄存器的复位赋值代码块

展开后提供以下等价声明供行为代码调用：
```cpp
const type name; // 当前周期寄存器的值，只读
void name_setnext<Priority>(type value); // 延迟赋值寄存器，在下个周期生效，priority 参数用于仲裁多个写端口，值越小表示优先级越高
```

当多个写端口在同一周期写入时，具有更低 priority 值的写入会覆盖具有更高 priority 值的写入。priority 的默认值为0，表示最高优先级。例如：

```cpp
REGISTER_MUL(myreg, uint8_t, 2) {
    myreg = 0;
}

SERVICE_LOGIC_IMPL(serv1, ARG(uint8_t) a) {
    myreg_setnext<0>(a); // serv1 的写端口优先级为 0，优先级高于 tick
}

TICK_IMPL() {
    myreg_setnext<1>(myreg + 1); // tick 的写端口优先级为 1
}
```

这个硬件模块中，如果一个周期内，serv1 和 tick 都对 myreg 进行写入，那么 serv1 中对 myreg 的写入会覆盖 tick 中对 myreg 的写入。

## REGISTER_ARRAY1(name, type, size, portnum) { ... }

定义一个一维寄存器数组：
- `name`：寄存器数组的名称
- `type`：寄存器的数据类型
- `size`：数组的大小
- `portnum`：每个寄存器单元的写端口数量
- `{ ... }`：寄存器数组的复位赋值代码块

初始化代码中需要包含对数组中每个寄存器单元的赋值逻辑，可以使用循环等结构，以保证寄存器数组在复位时被完全初始化：
```cpp
REGISTER_ARRAY1(myarray, uint8_t, 10, 1) {
    for (int i = 0; i < 10; i++) {
        myarray[i] = 0;
    }
}
```

展开后提供以下等价声明供行为代码调用：
```cpp
const std::array<type, size> name; // 当前周期寄存器数组的值，只读
void name_setnext(const uint64_t idx, type value); // 延迟赋值寄存器数组，在下个周期生效，idx 参数指定要写入的寄存器单元索引
void name_setnext<Priority>(const uint64_t idx, type value); // 延迟赋值寄存器数组，在下个周期生效，idx 参数指定要写入的寄存器单元索引，priority 整数值用于仲裁多个写端口

```
## WIRE(name, type) { ... }

临时变量定义，类似于 verilog 中的 wire，仅在当前周期内生效：
- `name`：变量的名称
- `type`：变量的数据类型，可以是标准整数类型、bool 类型
- `{ ... }`：变量的赋值表达式，在每个周期开始时被重置回这个值

展开后提供以下等价声明供行为代码调用：
```cpp
type name; // 临时变量的值，可以读写
```

## REQUEST(name, ARG(type1) arg, ..., RESP(type2) resp, ...)

定义一个请求事务接口：
- `name`：事务接口的名称
- `ARG(type) arg`：事务接口的参数列表，每个参数由 `ARG(type) argname` 定义，**参数只读**，type 可以是标准整数类型、已定义的结构体类型、bool 类型，或者其他已定义的别名类型
- `RESP(type) resp`：事务接口的响应参数列表，每个响应参数由 `RESP(type) respname` 定义，**参数只写**，type 可以是标准整数类型、已定义的结构体类型、bool 类型，或者其他已定义的别名类型

展开后提供以下等价声明供行为代码调用：
```cpp
void name(const type1& arg1, ..., type2& resp1, ...); // 事务接口的函数声明
```

## REQUEST_READY(name, ARG(type1) arg, ..., RESP(type2) resp, ...)

同 REQUEST，但定义了一个包含 valid-ready 握手行为的请求事务接口。

展开后提供以下等价声明供行为代码调用：
```cpp
bool name(const type1& arg1, ..., type2& resp1, ...); // 事务接口的函数声明，返回值为 bool 表示事务包含 valid-ready 握手行为
```

## SERVICE(name, ARG(type1) arg, ..., RESP(type2) resp, ...) { ... }

定义一个服务事务接口：
- `name`：事务接口的名称
- `ARG(type) arg`：事务接口的参数列表，每个参数由 `ARG(type) argname` 定义，**参数只读**，type 可以是标准整数类型、已定义的结构体类型、bool 类型，或者其他已定义的别名类型
- `RESP(type) resp`：事务接口的响应参数列表，每个响应参数由 `RESP(type) respname` 定义，**参数只写**，type 可以是标准整数类型、已定义的结构体类型、bool 类型，或者其他已定义的别名类型
- `{ ... }`：事务接口的实现代码块，包含了当事务被成功触发时应该执行的行为逻辑

例如：
```cpp
SERVICE(recv, ARG(uint8_t) a, RESP(uint8_t) b) {
    b = a + 1;
}
```

**实现代码中对 RESP 定义的参数的读取是非法的，RESP 参数在进入逻辑函数时其值是未定义的**


## SERVICE_READY(name, condition, ARG(type1) arg, ..., RESP(type2) resp, ...) { ... }

同 SERVICE，但定义了一个包含 valid-ready 握手行为的服务事务接口:
- `condition`：握手条件表达式，只有当这个表达式的值为 true 时，事务才会被成功触发

condition 是一个标准的 C++ 表达式，结果为 bool 类型，可以包含对模块寄存器的读取以及对事务接口参数的读取，但不允许包含任何 I/O 操作（如打印语句）或对寄存器的写入。

等价于：
```cpp
bool _condition(const type1& arg1, ..., type2& resp1, ...) const {
    return (condition); // condition 表达式的计算结果
}
void _implementation(const type1& arg1, ..., type2& resp1, ...) {
    ... // 事务被成功触发时执行的行为逻辑
}
bool service_name(const type1& arg1, ..., type2& resp1, ...) {
    bool rdy = _condition(arg1, ..., resp1, ...); // 计算握手条件
    if (rdy) _implementation(arg1, ..., resp1, ...); // 如果握手条件满足，则执行事务逻辑
    return rdy; // 返回握手条件的计算结果，表示事务是否被成功触发
}
```


## SERVICE_PRIO(name, priority, ARG(type1) arg, ..., RESP(type2) resp, ...) { ... }

同 SERVICE，但定义了一个带有优先级的服务事务接口:
- `priority`：相对于Tick和其他服务的优先级整数值，值越小表示越靠后被执行（即后执行的会覆盖先执行的），负值表示优先级低于 Tick，正值表示优先级高于 Tick

服务优先级指定了一个周期内服务被触发时的顺序约束。但尽量通过寄存器优先级赋值和合理的模块划分来避免依赖优先级保证行为正确性，因为顺序约束会随着潜在的事务调用被传递，很容易导致循环依赖。

## SERVICE_PRIO_READY(name, priority, condition, ARG(type1) arg, ..., RESP(type2) resp, ...) { ... }

SERVICE_READY 和 SERVICE_PRIO 的组合，定义了一个带有优先级的服务事务接口，并且包含 valid-ready 握手行为。


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

## USE_CHILD_SERVICE_PORT(instance, serv, ret, ARG(type1) arg, ..., RESP(type2) resp, ...)

声明使用了子实例中的服务事务端口定义，以便在父模块的行为代码中直接调用这个服务事务接口：
- `instance`：子实例的名称
- `serv`：子实例中服务事务端口的名称，必须与子实例模块中定义的 SERVICE_PORT 的 name 相同
- `ret`：void 或 bool，表示这个服务事务接口是否包含 valid-ready 握手行为
- `ARG(type) arg`：同 SERVICE 定义中的 ARG 参数列表
- `RESP(type) resp`：同 SERVICE 定义中的 RESP 参数列表

展开后提供以下等价声明供行为代码调用：
```cpp
void instance_serv(const type1& arg1, ..., type2& resp1, ...);
```

示例：
```cpp
CHILD_INSTANCE(Consumer, cons);
USE_CHILD_SERVICE_PORT(cons, get, bool, ARG(uint8_t) d, RESP(uint8_t) s);

TICK_IMPL() {
    uint8_t data = 42;
    uint8_t resp;
    if (cons_get(data, resp)) { // 调用子实例 cons 的 get 服务事务接口，传入参数 data，并获取响应参数 resp
        count_setnext(count + resp);
    }
}
```

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




