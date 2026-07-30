# 3. VulCPP 模块定义

在 VulCPP 中，模块是构建硬件设计的基本单元。每个模块通常由一个同名 C++ 头文件定义，文件中包含模块参数、接口、状态组件、子实例、连接关系和行为代码。

本章优先介绍新的统一 API 写法。旧版本中通过宏名区分的写法仍然兼容，例如 `REQUEST_READY`、`SERVICE_PRIO_READY`、`REGISTER_ARRAY1` 等；这些兼容语法糖集中列在本文末尾。

## 3.1 通用参数约定

统一定义风格：

```cpp
MACRO(required_arg0, required_arg1, key=value, key2=value2, ...)
```

必要位置参数写在最前面，可选属性使用 `key=value`。事务端口的业务参数使用 `ARG(type) name` 和 `RESP(type) name`。参数间允许换行：

```cpp
REQUEST(send, handshake=1, ARG(uint8_t) data);
SERVICE(recv, ready=q.deqvalid(), RESP(uint32_t) data) {
    ...
}
```

布尔属性支持 `0/1` 和 `true/false`。维度列表使用方括号 `[...]`：

```cpp
REGISTER(pipe, PipeStage, dims=[PIPE_DEPTH], ports=1) { ... }
CHILD_INSTANCE(DiagNode, mesh, dims=[HEI, WID]);
ALIAS(U32x4, uint32_t, dims=[4]);
```

没有逗号的单个维度也可以直接写：

```cpp
REGISTER(pipe, PipeStage, dims=PIPE_DEPTH, ports=1) { ... }
```

## 3.2 模块实现版本选择

模块可以使用 `INTERFACE` / `USE_VERSION` / `VERSION` 将对外接口和具体实现分开。模块文件中没有 `INTERFACE()` 时，解析器按旧流程处理整个模块文件；一旦定义了 `INTERFACE()`，模块接口必须全部放在 `INTERFACE()` 中。

版本化模块的顶层宏顺序必须是：

```cpp
INTERFACE() {
    PARAMETER(...);
    REQUEST(...);
    SERVICE(...);
}

USE_VERSION(version_name);

VERSION(version_name) {
    ...
}

VERSION(other_version) {
    ...
}
```

`INTERFACE()` 必须是模块中的第一个顶层宏，并且必须位于所有实现代码之前。`USE_VERSION(name);` 用于选择当前实际使用的实现版本；如果使用 `USE_VERSION`，它必须紧跟在 `INTERFACE()` 之后，后面必须定义至少一个 `VERSION(name) { ... }`，并且被 `USE_VERSION` 选中的版本必须存在。

`INTERFACE()` 中只允许出现 `PARAMETER`、`REQUEST` 和不带代码块的 `SERVICE` 声明：

```cpp
INTERFACE() {
    PARAMETER(WIDTH, 32);
    REQUEST(out, ARG(uint32_t) data);
    SERVICE(in, handshake=1, ARG(uint32_t) data);
}
```

`INTERFACE` 中的 `SERVICE` 仅为前置声明（参考后续 SERVICE 定义），不提供实现代码块。

如果定义了 `INTERFACE()` 但没有定义 `USE_VERSION(...)`，这是合法的；此时不能再定义任何 `VERSION(...)`，`INTERFACE()` 后面的顶层宏整体构成默认实现版本。

实现版本中可以包含具体实现，例如 `CONFIG`、`STRUCT`、`REGISTER`、`WIRE`、`BRAM`、`QUEUE`、`CHILD_INSTANCE`、`CONNECT_*`、`SERVICE(...) { ... }` 和 `TICK_IMPL() { ... }`。无论是默认实现还是 `VERSION(name)`，都不能重新声明 `PARAMETER` 或 `REQUEST`；`SERVICE` 必须提供代码块，并且必须对应 `INTERFACE` 中已经声明过的同名服务。

解析时，工具会把 `INTERFACE()` 的内容和所选实现内容按顺序拼接后送入普通模块解析流程。因此接口中的 `SERVICE` 声明和实现中的 `SERVICE` 实现仍遵循普通前置声明规则：`ARG/RESP` 的数量、类型和名字、`array`、`handshake` 必须一致；如果声明中显式写了 `priority=`，实现也必须写同样的 priority 值。嵌套域中的错误位置和 RTL debug 信息仍保留用户源文件中的原始行号。

默认实现示例：

```cpp
INTERFACE() {
    PARAMETER(WIDTH, 32);
    REQUEST(done);
    SERVICE(start, handshake=1);
}

REGISTER(count, uint32_t) {
    count = 0;
}

SERVICE(start, handshake=1, ready=true) {
    count.setnext(count + 1);
    done();
}
```

显式版本选择示例：

```cpp
INTERFACE() {
    PARAMETER(WIDTH, 32);
    REQUEST(done);
    SERVICE(start, handshake=1);
}

USE_VERSION(selected);

VERSION(simple) {
    SERVICE(start, handshake=1, ready=true) {
        done();
    }
}

VERSION(selected) {
    REGISTER(count, uint32_t) {
        count = 0;
    }

    SERVICE(start, handshake=1, ready=((count & 1) == 0)) {
        done();
    }

    TICK_IMPL() {
        count.setnext(count + 1);
    }
}
```

## 3.3 Header / 类型定义

以下宏既可以出现在项目 `header.hpp` / `header.h` / `header/` 中，也可以出现在模块头文件中。不同之处是：项目 header 中定义的是全局可见名字，模块头文件中定义的是模块局部名字。

### CONFIG(name, value)

定义一个常量：

```cpp
CONFIG(WIDTH, 32);
CONFIG(LEN, WIDTH / 8);
```

`value` 是常量表达式，类型按 `int64_t` 处理。全局 header 中的 `CONFIG` 会对所有模块可见；模块内 `CONFIG` 只在当前模块内可见。

### PARAMETER(name, value)

定义一个可由子实例化处覆盖的模块参数：

```cpp
PARAMETER(WIDTH, 32);
```

实例化模块时可以通过 `CHILD_INSTANCE(..., PARAM(WIDTH)=64)` 覆盖默认值。`PARAMETER` 的值同样按常量表达式处理。

### ALIAS(name, type, dims=[...])

定义类型别名。无 `dims` 时是普通别名，有 `dims` 时是数组别名：

```cpp
ALIAS(Word, uint32_t);
ALIAS(U32x4, uint32_t, dims=[4]);
ALIAS(Matrix, uint8_t, dims=[2, 3]);
```

语法意义上等价于：

```cpp
using Word = uint32_t;
using U32x4 = std::array<uint32_t, 4>;
using Matrix = std::array<std::array<uint8_t, 3>, 2>;
```

### ENUM(name) { ... }

定义枚举类型：

```cpp
ENUM(Op) {
    Add = 0,
    Sub = 1,
};
```
### STRUCT(name) { ... }

定义结构体类型：

```cpp
STRUCT(Packet) {
    uint32_t data;
    bool valid;
};
```

结构体字段可以使用标准整数类型、`bool`、`Int<N>`、`ALIAS`、`ENUM`、或其他 `STRUCT` 类型。


### HELPER() { ... }

定义模块内部使用的辅助函数或纯编译期代码：

```cpp
HELPER() {
inline constexpr uint32_t inc(uint32_t x) {
    return x + 1;
}
}
```

在可综合语法子集下，`HELPER` 中禁止定义运行期变量。需要跨周期保存的状态必须使用 `REGISTER`，需要周期内临时状态必须使用 `WIRE`。

## 3.4 状态组件

### REGISTER(name, type, ports=1, dims=[...]) { ... }

定义寄存器或寄存器数组：

```cpp
REGISTER(counter, uint32_t) {
    counter = 0;
}

REGISTER(scoreboard, bool, dims=[32], ports=2) {
    for (uint32_t i = 0; i < 32; ++i) {
        scoreboard[i] = false;
    }
}
```

参数说明：

- `name`：寄存器名称。
- `type`：寄存器元素类型。
- `ports`：写端口数量，默认 `1`。多端口寄存器通过 `setnext<P>(...)` 指定写端口优先级。
- `dims`：数组维度。省略时是标量寄存器；`dims=[N]` 是一维寄存器数组。
- `{ ... }`：复位赋值代码块。

标量寄存器使用示例：

```cpp
REGISTER(counter, uint32_t, ports=2) {
    counter = 0;
}

TICK_IMPL() {
    uint32_t now = counter;
    counter.setnext<0>(now + 1);
    counter.setnext<1>(counter.get() + 2); // 等价显式读取
}
```

结构体寄存器建议显式使用 `.get()` 读取字段：

```cpp
STRUCT(Status) {
    uint8_t code;
    bool valid;
};

REGISTER(status, Status) {
    status.code = 0;
    status.valid = false;
}

TICK_IMPL() {
    Status s = status;
    uint8_t code = status.get().code; // status.code 在 C++ 中不能合法重载
}
```

多端口写入示例：

```cpp
REGISTER(value, uint8_t, ports=2) {
    value = 0;
}

SERVICE(write, ARG(uint8_t) data) {
    value.setnext<0>(data);
}

TICK_IMPL() {
    value.setnext<1>(value + 1);
}
```

同一周期中，较小端口编号具有更高优先级。上例中端口 `0` 的写入会覆盖端口 `1`。同一个写端口在同一周期内重复 `setnext` 属于非法用法；非 release 编译通常会触发断言。

数组寄存器示例：

```cpp
REGISTER(pipe, uint32_t, dims=[4], ports=1) {
    for (uint32_t i = 0; i < 4; ++i) {
        pipe[i] = 0;
    }
}

TICK_IMPL() {
    uint32_t v = pipe[0];
    pipe.setnext<0>(1, v + 1);
}
```

### WIRE(name, type) { ... }

定义周期内临时变量：

```cpp
WIRE(hit, bool) {
    hit = false;
}

TICK_IMPL() {
    hit = true;
}
```

`WIRE` 在每个周期开始时按代码块恢复默认值，之后可在当前周期内读写。

## 3.5 事务端口

事务端口分为请求端口 `REQUEST` 和服务端口 `SERVICE`。二者通过 `CONNECT` 建立连接，或者在模块行为代码中直接调用已引入的端口函数。

端口业务参数使用：

- `ARG(type) name`：只读输入参数，调用方驱动被调用方。
- `RESP(type) name`：只写响应参数，被调用方驱动调用方。

### REQUEST(name, handshake=0, array=N, ARG(...), RESP(...))

定义请求事务端口：

```cpp
REQUEST(output, ARG(Int<25>) data, ARG(Int<7>) tag);
REQUEST(fetch, handshake=1, RESP(uint32_t) data);
REQUEST(lane_out, array=LANES, ARG(uint32_t) data);
```

参数说明：

- `handshake`：是否包含 valid-ready 握手返回值，默认 `0`。`handshake=1` 时事务函数调用返回 `bool`。
- `array`：阵列化事务端口数量。省略时为普通端口。
- `ARG/RESP`：端口参数列表。

等价行为：

```cpp
REQUEST(send, ARG(uint8_t) data);              // void send(const uint8_t &data);
REQUEST(send, handshake=1, ARG(uint8_t) data); // bool send(const uint8_t &data);
```

阵列化请求端口调用时使用模板参数：

```cpp
REQUEST(out, array=2, ARG(uint32_t) data);

TICK_IMPL() {
    out<0>(1);
    out<1>(2);
}
```

### SERVICE(name, handshake=0, ready=cond, priority=prio, array=N, ARG(...), RESP(...)) { ... }

定义服务事务端口：

```cpp
SERVICE(recv, ARG(uint8_t) data) {
    ...
}

SERVICE(deq, ready=q.deqvalid(), RESP(uint32_t) data) {
    data = q.front();
    q.deqnext();
}

SERVICE(write, priority=1, ARG(uint8_t) data) {
    ...
}
```

参数说明：

- `handshake`：是否包含 valid-ready 握手，默认 `0`。
- `ready`：握手 ready 条件。提供 `ready` 时等价于 `handshake=1`；如果显式写 `handshake=1`，必须提供 `ready`。
- `priority`：服务逻辑块优先级。较小值优先级更高。
- `array`：阵列化服务端口数量。
- `ARG/RESP`：端口参数列表。
- `{ ... }`：事务成功触发时执行的逻辑。

带 ready 条件的服务：

```cpp
SERVICE(recv, handshake=1, ready=((cycle & 1) == 0), ARG(uint8_t) data) {
    sum.setnext(sum + data);
}
```

实现代码中读取 `RESP` 参数是非法的，`RESP` 参数进入逻辑函数时值未定义，只能由服务逻辑写出。

SERVICE 也支持前置声明。前置声明使用不带 `{ ... }` 代码块的 `SERVICE(...);`，用于先声明端口签名，之后必须由同名 `SERVICE(...) { ... }` 实现：

```cpp
SERVICE(recv, handshake=1, ARG(uint8_t) data);

SERVICE(recv, handshake=1, ready=q.deqvalid(), ARG(uint8_t) data) {
    ...
}
```

如果写了前置声明，则之后的同名实现必须与声明保持一致：`ARG/RESP` 的数量、类型和名字、`array`、`handshake` 都必须匹配。

前置声明中不写 `ready=`，因为 ready 条件属于实现逻辑；`handshake=1` 的声明是合法的，对应实现仍必须提供 `ready=`。`priority` 是服务实现语义，声明中可以省略；如果声明中显式写了 `priority=`，则实现必须写出同样的 priority 值。

### 带有优先级的服务事务接口

- `priority`：相对于Tick和其他服务的优先级整数值，值越小表示越靠后被执行（即后执行的会覆盖先执行的），负值表示执行顺序后于 Tick，正值表示执行顺序先于 Tick

服务优先级指定了一个周期内服务被触发时的顺序约束。但尽量通过寄存器优先级赋值和合理的模块划分来避免依赖优先级保证行为正确性，因为顺序约束会随着潜在的事务调用被传递，很容易导致循环依赖。

## 3.6 QUERY 与周期行为

### QUERY(name, rettype) { ... }

定义只读查询接口：

```cpp
STRUCT(Status) {
    uint32_t sum;
    bool valid;
};

QUERY(status, Status) {
    Status s;
    s.sum = sum;
    s.valid = true;
    return s;
}
```

`QUERY` 不参与 `CONNECT` 连接，没有 `ARG/RESP` 参数，代表当前周期稳定状态下的一次无副作用观测。`QUERY` 中只应读取本模块状态、组合线网、内置组件 query 或已引入的子实例 query。

### TICK_IMPL() { ... }

定义模块每周期执行的行为：

```cpp
TICK_IMPL() {
    counter.setnext(counter + 1);
}
```

一个模块可以有多个 `TICK_IMPL` 代码块，生成器会把它们都纳入周期行为。

## 3.7 子实例与子接口引入

### CHILD_INSTANCE(module, name, dims=[...], PARAM(param)=value, ...)

定义子模块实例：

```cpp
CHILD_INSTANCE(Producer, prod);
CHILD_INSTANCE(LineNode, lane, dims=[LEN]);
CHILD_INSTANCE(DiagNode, mesh, dims=[HEI, WID], PARAM(WIDTH)=32);
```

参数说明：

- `module`：子模块类型名称，对应项目中的 `module.hpp` / `module.h` 等文件。
- `name`：子实例名称。
- `dims`：子实例数组维度。省略时为标量子实例。
- `PARAM(param)=value`：覆盖子模块中的 `PARAMETER`。

### USE_CHILD_SERVICE(instance, service, alias, array=N, ARG(...), RESP(...))

把子实例服务端口引入当前模块行为代码：

```cpp
CHILD_INSTANCE(Consumer, cons);
USE_CHILD_SERVICE(cons, recv, cons_recv, ARG(uint8_t) data);

TICK_IMPL() {
    cons_recv(1);
}
```

阵列化子实例可以使用具体索引或一个 `*` 通配维度：

```cpp
CHILD_INSTANCE(DiagNode, mesh, dims=[HEI, WID]);
USE_CHILD_SERVICE(mesh[*][0], left_in, mesh_in, array=HEI, ARG(uint32_t) data);

TICK_IMPL() {
    mesh_in<0>(1); // call mesh[0][0].left_in(1)
    mesh_in<1>(10); // call mesh[1][0].left_in(10)
}
```

其中 `array=N` 用于将阵列化子实例的同名端口作为阵列化事务端口引出；如果能直接从 `instance` 表达式的 `*` 维度推断，也可以省略 `array`。无 `*` 通配时不应写 `array=`。

### USE_CHILD_QUERY(instance, query, alias, rettype, array=N)

把子实例 query 引入当前模块：

```cpp
CHILD_INSTANCE(Node, node);
USE_CHILD_QUERY(node, status, node_status, Status);

QUERY(snapshot, Status) {
    return node_status();
}
```

阵列化子实例同样可以使用具体索引或一个 `*` 通配维度。通配维度引出的 alias 使用模板参数调用；`array=N` 的含义和 `USE_CHILD_SERVICE` 相同。

QUERY 不像 SERVICE 一样有阵列化选项。如需批量透传阵列化子实例的 QUERY，推荐实现为让 QUERY 返回一个数组或结构体类型，在 QUERY 内部循环调用子实例 query。

## 3.8 连接声明

连接 API 有四个宏名，差异来自连接两端是否属于子实例或当前模块边界。命名与参数顺序一一对应：

- **C**: Child，子实例名
- **R**: Request，请求端口名
- **S**: Service，服务端口名

### CONNECT_CR_CS(srcchild, srcreq, dstchild, dstserv)

连接子实例 request 到另一个子实例 service：

```cpp
CONNECT_CR_CS(prod, send, cons, recv);
```

### CONNECT_CR_S(srcchild, srcreq, dstserv)

连接子实例 request 到当前模块 service：

```cpp
CONNECT_CR_S(prod, send, recv);
```

### CONNECT_CR_R(srcchild, srcreq, dstreq)

连接子实例 request 到当前模块 request，用于把子实例请求暴露到模块边界：

```cpp
CONNECT_CR_R(cons, output, output);
```

### CONNECT_S_CS(srcserv, dstchild, dstserv)

连接当前模块 service 到子实例 service：

```cpp
CONNECT_S_CS(input, cons, recv);
```

连接两端的事务端口必须存在，`ARG/RESP` 类型和数量必须匹配，握手属性也必须一致。

阵列化内部连接示例：

```cpp
CHILD_INSTANCE(DiagNode, mesh, dims=[HEI, WID]);
CONNECT_CR_CS(mesh[$][?], right_out, mesh[$+1][?+1], left_in);
```

### 连接约束

对于父模块中定义的服务，则必须进行以下处理之一：
1. 被连接到某个子模块的某个服务，且必须具有相同的参数与返回值定义。
2. 被用户代码实现（在 `SERVICE_LOGIC_IMPL` 中提供实现代码）。

对于子模块的外部端口中定义的请求，则必须进行以下处理之一：
1. 被连接到某个子模块的某个服务，且必须具有相同的参数与返回值定义。
2. 被连接到本模块的外部端口中的某个请求，且必须具有相同的参数与返回值定义。
3. 被连接到本模块的外部端口中的某个服务，且必须具有相同的参数与返回值定义。

如果一个请求包含返回值或包含握手信号（`has_handshake` 为 `true`），则其仅能被连接到一个服务或代码实现，不允许一对多连接。
反之，如果一个请求不包含返回值且不包含握手信号，则允许一对多连接（广播连接）。

## 3.9 存储组件

### BRAM(name, type, size, read_ports=1, write_ports=1, mode=generic)

定义块 RAM：

```cpp
BRAM(data_array, uint32_t, 1024);
BRAM(data_array, uint32_t, 1024, read_ports=2, write_ports=1);
BRAM(data_array, uint32_t, 1024, mode=1rw);
```

`mode=generic` 使用显式读写端口数；`mode=1rw` 使用单读写口 BRAM 语义。

### ROM(name, data_width, size, read_ports=1, init=path)

定义只读存储器：

```cpp
ROM(inst_rom, 32, 1024, init="program.hex");
ROM(inst_rom, 32, 1024, read_ports=2, init="program.hex");
```

### QUEUE(name, type, depth, enq_width=1, deq_width=1)

定义队列：

```cpp
QUEUE(q, uint32_t, 8);
QUEUE(q, uint32_t, 8, enq_width=3, deq_width=2);
```

普通队列默认单 enq / 单 deq。显式设置 `enq_width` 或 `deq_width` 大于 1 时表示多端口队列。

## 3.10 旧版兼容语法糖

以下旧写法仍然兼容，推荐新代码优先使用前文的新 API。

### 类型别名数组

```cpp
ALIAS_ARRAY1(name, type, N);
ALIAS_ARRAY2(name, type, N1, N2);
```

等价于：

```cpp
ALIAS(name, type, dims=[N]);
ALIAS(name, type, dims=[N1, N2]);
```

当前 `ALIAS(name, type, N...)` 的位置参数维度写法也仍然兼容。

### 多端口和数组寄存器

```cpp
REGISTER_MUL(name, type, portnum) { ... }
REGISTER_ARRAY1(name, type, size, portnum) { ... }
```

等价于：

```cpp
REGISTER(name, type, ports=portnum) { ... }
REGISTER(name, type, dims=[size], ports=portnum) { ... }
```

旧的 `REGISTER(name, type, portnum, dim...)` 位置参数形式仍然兼容，但新代码建议显式写 `ports=` 和 `dims=`。

### 握手请求端口

```cpp
REQUEST_READY(name, ARG(type) arg, RESP(type) ret);
```

等价于：

```cpp
REQUEST(name, handshake=1, ARG(type) arg, RESP(type) ret);
```

旧的 `ARRAY(N)` 端口阵列写法仍然兼容：

```cpp
REQUEST(name, ARRAY(N), ARG(type) arg);
```

建议新代码写：

```cpp
REQUEST(name, array=N, ARG(type) arg);
```

### 握手和优先级服务端口

```cpp
SERVICE_READY(name, condition, ARG(type) arg) { ... }
SERVICE_PRIO(name, priority, ARG(type) arg) { ... }
SERVICE_PRIO_READY(name, priority, condition, ARG(type) arg) { ... }
```

等价于：

```cpp
SERVICE(name, handshake=1, ready=condition, ARG(type) arg) { ... }
SERVICE(name, priority=priority, ARG(type) arg) { ... }
SERVICE(name, handshake=1, priority=priority, ready=condition, ARG(type) arg) { ... }
```

`SERVICE(..., ARRAY(N), ...)` 的旧阵列写法也仍然兼容，推荐改为 `array=N`。

### 子实例数组

```cpp
CHILD_INSTANCE_ARRAY1(module, name, N0, PARAM(param)=value);
CHILD_INSTANCE_ARRAY2(module, name, N0, N1, PARAM(param)=value);
```

等价于：

```cpp
CHILD_INSTANCE(module, name, dims=[N0], PARAM(param)=value);
CHILD_INSTANCE(module, name, dims=[N0, N1], PARAM(param)=value);
```

### 子服务端口引入

```cpp
USE_CHILD_SERVICE_PORT(instance, serv, alias, ARG(type) arg, RESP(type) ret);
```

等价于：

```cpp
USE_CHILD_SERVICE(instance, serv, alias, ARG(type) arg, RESP(type) ret);
```

### 连接宏

```cpp
CONNECT_CR_CS(srcmod, srcreq, dstmod, dstserv);
CONNECT_CR_S(srcmod, srcreq, dstserv);
CONNECT_CR_R(srcmod, srcreq, dstreq);
CONNECT_S_CS(srcserv, dstmod, dstserv);
```

连接宏当前保持旧版四宏名形式，不提供合并后的 `CONNECT(...)` 写法。

### 旧存储组件变体

```cpp
BRAM_1RW(name, datatype, size);
QUEUE_MP(name, type, depth, enqwidth, deqwidth);
```

等价于：

```cpp
BRAM(name, datatype, size, mode=1rw);
QUEUE(name, type, depth, enq_width=enqwidth, deq_width=deqwidth);
```

旧的通用 BRAM 和 ROM 位置参数仍然兼容：

```cpp
BRAM(name, datatype, size, readports, writeports);
ROM(name, datawidth, size, readports, init_path);
```

推荐新代码写：

```cpp
BRAM(name, datatype, size, read_ports=readports, write_ports=writeports);
ROM(name, datawidth, size, read_ports=readports, init=init_path);
```
