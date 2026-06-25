# VUL / VulCPP LLM 极简规则表

优先遵守本表；需要细节再查其他 md 文件或示例项目。

## 1. 项目与文件

- VulCPP 用 C++ 宏描述周期同步硬件行为；普通模块必须近似可综合：无 I/O、无动态分配、无运行期全局状态、无指针算术。Main 是仿真入口，可用标准库、I/O、全局变量和测试逻辑。
- 常见文件：项目根 `header/` 目录放全局类型/常量；兼容旧式根目录 `header.hpp`/`header.h` 单文件；每个硬件模块一个同名 `.hpp`；一个或多个 `Main.cpp` 做仿真/单测。
- 普通模块通常写：

```cpp
#pragma once
#include <defhelper.hpp>
#include "header.hpp"
```

- Main 通常写：

```cpp
#include <defhelper.hpp>
#include <run.hpp>
#include "header.hpp"
TOP("./Top.hpp");
PROJECT(".");
```

- `TOP(path)` 指顶层硬件模块；`PROJECT(path)` 指项目根。若 Main 已声明，`vulsimgen -m Main.cpp` 即可；可用 `-t/-p/-l/-o` 覆盖。

## 2. header 与局部定义

- 项目根必须存在 `header/` 或 `header.hpp`/`header.h` 之一，不能同时存在目录和单文件。`header/` 中所有 `.h/.hpp` 按 `#include` 拓扑排序解析，被 include 者先解析；循环报错。
- 项目级 header 只放全局共享定义：`CONFIG`、`STRUCT`、`ENUM`、`ALIAS`、`ALIAS_ARRAY1/2`、`HELPER`。这些定义对所有模块可见，即使模块没有 include 它。
- 全局 `CONFIG` 和 bundle（`STRUCT/ENUM/ALIAS*`）共用同一命名域，不允许重名或覆盖。
- 硬件模块/Main 内也可写这些宏，但只作为模块局部定义。
- `CONFIG(name, expr)` 是 `int64_t` 编译期表达式；支持整数、已定义配置、括号、算术/位运算/比较/逻辑/三元。`@X` 与 `clog2(X)` 表示 `log2ceil(X)`。禁止循环依赖。
- `STRUCT(Name) { fields... };` 字段可用标准整数、`bool`、`Int<W>`、其他 struct/enum/alias。
- `ENUM(Name) { A = 1, B, C = 7 };` 类似 `enum class`，可显式赋值。
- `ALIAS(Name, Type);`、`ALIAS_ARRAY1(Name, Type, N);`、`ALIAS_ARRAY2(Name, Type, N0, N1);`。
- `HELPER { ... }` 可写自封闭的inline辅助函数，禁止声明运行期变量/常量。

## 3. 硬件模块宏

- 参数/辅助定义：

```cpp
PARAMETER(name, default_expr);
HELPER() { /* helper functions/types only; no runtime state */ }
```

- 状态/组件：

```cpp
REGISTER(name, type) { reset_assignments }
REGISTER_MUL(name, type, portnum) { reset_assignments }
REGISTER_ARRAY1(name, type, size, portnum) { reset_loop_or_assignments }
WIRE(name, type) { init_each_cycle }
QUEUE(name, type, depth);
QUEUE_MP(name, type, depth, enqwidth, deqwidth);
BRAM(name, datatype, size, readports, writeports);
BRAM_1RW(name, datatype, size);
ROM(name, datawidth, size, readports, path/without/quotes);
```

- 事务/查询/时钟：

```cpp
REQUEST(name, ARG(T) a, RESP(U) r);
REQUEST_READY(name, ARG(T) a, RESP(U) r);        // 返回 bool
REQUEST(name, ARRAY(N), ARG(T) a);               // 阵列化 request，用 name<i>(...)
REQUEST_READY(name, ARRAY(N), ARG(T) a);
SERVICE(name, ARG(T) a, RESP(U) r) { ... }
SERVICE_READY(name, cond, ARG(T) a, RESP(U) r) { ... }
SERVICE_PRIO(name, priority, ARG(T) a) { ... }
SERVICE_PRIO_READY(name, priority, cond, ARG(T) a) { ... }
SERVICE(name, ARRAY(N), ARG(T) a) { /* IDX 为当前阵列下标 */ }
SERVICE_READY(name, cond, ARRAY(N), ARG(T) a) { ... }
QUERY(name, RetType) { return value; }
TICK_IMPL() { ... }
```

- `ARG` 只读；`RESP` 只写，进入服务逻辑时值未定义，不要读取。
- `_READY` 事务有 valid-ready 握手：request 调用返回 `bool`，只有 ready 为真时 service 逻辑执行。
- `SERVICE_READY` 的 `cond` 必须无副作用，不写寄存器/组件，不做 I/O。
- `SERVICE_PRIO` 仅在必须约束服务与 tick 执行顺序时用；优先用多写端口寄存器或模块拆分避免顺序依赖。priority 值越小越靠后执行，负值低于 Tick，正值高于 Tick。
- `QUERY` 只读、无副作用、不参与 `CONNECT_*`；只能观测当前稳定状态、子 `QUERY`、组件 query。

## 4. 状态语义

- 所有寄存器、队列、BRAM 写入都是 next-cycle 语义：本周期调用 `setnext/enqnext/deqnext/readreq/write/req`，`sim_commit()` 后才提交。不要依赖同周期立即生效。
- `REGISTER` 复位块必须完整初始化标量/结构体所有字段。
- 读寄存器当前值：基础类型可隐式读或 `.get()`；结构体/数组字段必须先 `.get()`：

```cpp
uint32_t x = counter;
uint32_t y = counter.get();
uint32_t a = status.get().field;   // 不写 status.field
```

- 写寄存器：`reg.setnext(v);`。同一寄存器同一写端口每周期最多一次；重复写 debug 下 assert，release 下未定义。
- 单端口寄存器的 next 值是保持型：某周期不写时，下次提交仍提交旧候选值。推荐每周期显式计算 next 并统一写：

```cpp
TICK_IMPL() {
    auto next = count.get();
    if (inc) next = next + 1;
    if (clr) next = 0;
    count.setnext(next);
}
```

- 多来源同周期写同一状态，用 `REGISTER_MUL(reg, T, P)` 或 `REGISTER_ARRAY1(..., P)`：`reg.setnext<0>(v)` 优先级高于 `setnext<1>`；同一端口仍最多一次。
- `REGISTER_ARRAY1(arr,T,N,P)`：读 `arr[i]`，写 `arr.setnext<p>(i,v)`；同一下标同一端口每周期最多一次。
- `WIRE` 是当前周期临时变量，每周期按块重置，可在 service/tick 中读写；不跨周期保存。

## 5. 子模块与连接

- 实例：

```cpp
CHILD_INSTANCE(Module, inst, optional_param_overrides...);
CHILD_INSTANCE_ARRAY1(Module, arr, N0, optional_param_overrides...);
CHILD_INSTANCE_ARRAY2(Module, mesh, N0, N1, optional_param_overrides...);
```

- 连接：

```cpp
CONNECT_CR_CS(child, child_req, child2, child2_serv);
CONNECT_CR_S(child, child_req, parent_serv);
CONNECT_CR_R(child, child_req, parent_req);
CONNECT_S_CS(parent_serv, child, child_serv);
```

- 连接双方事务名称可不同，但 `ARG/RESP` 类型、数量和 ready/非 ready 语义必须匹配；子模块 `REQUEST` 不应悬空。
- 父模块直接调用子 service/query：

```cpp
USE_CHILD_SERVICE_PORT(inst, serv, alias, ARG(T) a, RESP(U) r);
USE_CHILD_QUERY(inst, query, alias, RetType);
alias(...);
RetType v = alias();
```

- 阵列化实例索引：`lane[0]`、`mesh[i][j]`。规则连接中 `$`/`?` 是源侧循环变量；源侧只能单独作维度，如 `mesh[$][?]`，目的侧可写表达式，如 `mesh[$+1][?-1]`，越界连接自动忽略。
- `*` 只用于模块边界或 `USE_CHILD_SERVICE_PORT` 通配一维，最多一个：

```cpp
USE_CHILD_SERVICE_PORT(mesh[*][0], left_in, mesh_in, ARG(uint32_t) data);
mesh_in<0>(1);
CONNECT_CR_S(mesh[*][WID-1], right_out, output);
```

## 6. Main 仿真模块

- Main 可用 `GLOBAL(){...}`、`VAR(T,name)`、`VAR_INIT(T,name,init)` 保存仿真状态。
- Main 的事务方向与 Top 相反，且名称/参数匹配：Top 的 `REQUEST` 对应 Main 的 `SERVICE`；Top 的 `SERVICE` 对应 Main 的 `REQUEST`。
- Main 可声明顶层 query：`QUERY(status, Status);`，无代码块，仿真中直接 `status()`。
- Main 可用标准库、`printf`、`std::exit`、文件 I/O 等。
- 仿真函数：

```cpp
SIMULATION() {
    sim_reset();       // 可选
    request_to_top(...);
    sim_execute();     // 执行一拍行为/事务
    sim_commit();      // 提交 next 状态
    sim_exit();        // 可选退出
}
```

- Main 也可用底层端口宏：`REQUEST_PORT/SERVICE_PORT`、`SERVICE_COND_IMPL`、`SERVICE_LOGIC_IMPL`；普通写法优先用 `REQUEST/SERVICE/SERVICE_READY`。

## 7. Int<W> 任意宽度整数

- 用 `Int<BitWidth>` 描述硬件位向量，`BitWidth > 0`。默认无符号解释；`.sint()` 只改变后续转换/比较/乘法/右移解释，不改变 bit。
- 构造/赋值会按目标宽度截断或扩展；`to<T>()` 导出标准整数。
- 静态位选：`x.at<Hi,Lo>()`、`x.at<Idx>()`；动态位选：`x.pick<W>(idx)`、`x.pick(idx)`。Slice/bit ref 可作左值；不同宽赋值请显式构造目标宽度。
- 拼接/重复/归约：`Cat(a,b,...)` 高位到低位拼接；`Repeat<N>(x)`；`ReduceOr/And/Xor(x)`。
- 运算位宽：`+ -> Int<max(A,B)+1>`，`- -> Int<max(A,B)>`，`* -> Int<A+B>`，移位结果保持左侧宽度。
- `& | ^ == !=` 通常要求等宽无符号 `Int/Ref`；`< <= > >=` 可混合 `.sint()`。当前无 `/`、`%`。

## 8. Queue

- `QUEUE(q,T,D)`：`enqready()`、`enqnext(v)`、`deqvalid()`、`front()`、`deqnext()`、`clrnext()`。
- 调用前先检查：`enqready()` 为真才能 `enqnext`；`deqvalid()` 为真才能 `front/deqnext`。
- 每周期最多一次 `enqnext`、一次 `deqnext`、一次 `clrnext`；重复 debug assert，release 未定义。
- `front()` 不出队；`deqnext()` 不返回数据且下周期才生效。
- 提交顺序：clear 最先；然后 dequeue；然后 enqueue。`enqready()` 不考虑本周期尚未提交的 dequeue。
- `QUEUE_MP(q,T,D,EW,DW)`：`enqreqdy()` 返回本周期可入队数；`deqvalid()` 返回可出队数；`front(num)` 返回长度 `DW` 的数组且只有前 `deqvalid()` 项有效；`enqnext(values,num)`、`deqnext(num)` 前保证 num 合法。

## 9. BRAM / ROM

- `BRAM(name,T,size,R,W)`：多读多写同步 RAM；`size` 是元素数，地址位宽自动 `log2ceil(size)`，要求 `size > 1`。
- 每读端口：`readreq<p>(addr)` 本周期发地址，下一周期 `readdata<p>()` 有效；每端口每周期最多一次 `readreq`。
- 每写端口：`write<p>(addr,data)` 下周期提交；每端口每周期最多一次。
- 同周期同地址读写读到写前旧值；多写端口同地址时端口号大的最终生效。
- `BRAM_1RW(name,T,size)`：共享端口 `req(addr, data, write_en)`；上一周期读请求 `write_en=false` 后，本周期 `readdata()` 才有效；每周期最多一次 `req`。
- `ROM(name,datawidth,size,R,path/without/quotes)`：同步读，只读文件初始化；路径参数不加双引号。支持 `$readmemh` 风格 hex token、`@addr`、`0x`、`_`、`//`/`#` 注释。

## 10. 生成代码禁忌与惯用法

- 普通硬件模块中不要写 `printf`、文件 I/O、随机数、线程、动态分配、STL 容器运行期状态。数组类型用 `ALIAS_ARRAY*` 或固定 `std::array` 类型。
- 不要在一个周期从多个分支重复写同一寄存器/队列/BRAM端口；先算临时 next，再调用一次。
- 不要读未被握手保护的 queue/BRAM 数据；不要读取 `RESP` 入参旧值。
- 不要依赖 service/tick 的隐式执行顺序；需要仲裁时显式用 `REGISTER_MUL`、service priority 或拆模块。
- 结构体寄存器更新常用模式：

```cpp
MyReg n = reg.get();
n.valid = true;
n.data = data;
reg.setnext(n);
```

- 输出事务常传当前周期旧状态；若要输出更新后的值，先计算临时变量并同时用于输出和 `setnext`。

## 11. Trace / Break 调试

- `vulsimgen --trace "top::inst.signal"` 或 `-T trace_config.txt` 生成 VCD。无 `.` 时等价追踪该实例所有信号；`*` 匹配一层或多层。
- 实例层级用 `::`，信号层级用 `.`：如 `top::cpu.regfile`、`top::*.*`。
- 阵列实例可写 `top::mesh[0][0].datareg`、`top::mesh[*][0].datareg`；省略索引表示所有单元。
- 断点必须启用 trace：`--break "(top::x.sig == 0x10) && (...)"` 或 `-B file`；断点左侧必须是已 trace 的精确信号路径，不允许 `*`。右侧支持十进制、`0b`、`0x`。

## 12. 最小模板

`header.hpp`

```cpp
#pragma once
#include <defhelper.hpp>

CONFIG(W, 32);
STRUCT(Packet) {
    Int<W> data;
    bool valid;
};
```

`Top.hpp`

```cpp
#pragma once
#include <defhelper.hpp>
#include "header.hpp"

REGISTER(cnt, Int<W>) { cnt = 0; }
REQUEST(out, ARG(Int<W>) value);

QUERY(value, Int<W>) { return cnt; }

TICK_IMPL() {
    Int<W> next = cnt + 1;
    out(cnt);
    cnt.setnext(next);
}
```

`Main.cpp`

```cpp
#include <cstdio>
#include <defhelper.hpp>
#include <run.hpp>
#include "header.hpp"

TOP("./Top.hpp");
PROJECT(".");

GLOBAL() { uint64_t cycle = 0; }
SERVICE(out, ARG(Int<W>) value) {
    std::printf("%lu: %u\n", cycle, value.to<uint32_t>());
}
QUERY(value, Int<W>);

SIMULATION() {
    for (cycle = 0; cycle < 10; ++cycle) {
        sim_execute();
        sim_commit();
    }
}
```
