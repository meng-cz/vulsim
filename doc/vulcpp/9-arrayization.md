# 8. VUL 阵列化子实例

本章介绍 VulCPP 中的阵列化相关能力，重点包括：
- 阵列化子实例的定义
- 阵列化子实例之间、子实例与父模块之间的事务连接
- `USE_CHILD_SERVICE_PORT` 在阵列化场景下的用法
- `simgen` 与 `rtlgen` 对阵列化子实例的处理方式

需要注意的是，本章讨论的是**子模块实例的阵列化**。请求端口、服务端口、寄存器数组、类型别名数组等能力仍按前文章节中的规则使用。


## CHILD_INSTANCE_ARRAY1(module, name, N0, ...)

定义一个一维阵列化子实例：
- `module`：子模块类型名
- `name`：子实例名
- `N0`：第 0 维大小，必须是可静态求值的常量表达式
- `...`：可选的参数覆盖列表，与 `CHILD_INSTANCE` 相同

示例：

```cpp
CHILD_INSTANCE_ARRAY1(LineNode, lane, LEN);
```

上例定义了一个名为 `lane` 的一维子实例阵列，包含 `LEN` 个 `LineNode` 节点。


## CHILD_INSTANCE_ARRAY2(module, name, N0, N1, ...)

定义一个二维阵列化子实例：
- `module`：子模块类型名
- `name`：子实例名
- `N0`：第 0 维大小，必须是可静态求值的常量表达式
- `N1`：第 1 维大小，必须是可静态求值的常量表达式
- `...`：可选的参数覆盖列表，与 `CHILD_INSTANCE` 相同

示例：

```cpp
CHILD_INSTANCE_ARRAY2(DiagNode, mesh, HEI, WID);
```

上例定义了一个 `HEI x WID` 的二维子实例阵列 `mesh`。


## 子实例索引表达式

在阵列化子实例相关语法中，实例名后可以带有索引表达式：

```cpp
lane[0]
lane[LEN - 1]
mesh[1][0]
mesh[HEI - 1][WID - 1]
```

索引表达式必须能够在静态解析阶段被检查，并且维度数必须与实例声明一致。

在连接规则中，还支持三类特殊索引符号：
- `$`：第一维循环变量
- `?`：第二维循环变量
- `*`：模块边界连接中的单维通配符

其中：
- `$` 和 `?` 仅用于**规则化阵列连接**
- `*` 仅用于**子实例与父模块边界之间的连接**
- 当前最多支持一个 `*`

对于内部规则连接，`$` 和 `?` 的约束分为源侧与目的侧两部分：
- 在**源实例侧**，`$` 和 `?` 不能出现在表达式中，只能单独作为某一维本身出现
- 因此 `[$][?]` 与 `[?][$]` 都合法，但 `[$+1][?]`、`[$+?][0]`、`[2*$][?]` 在源侧都不合法
- 在**目的实例侧**，`$` 和 `?` 可以出现在任意合法常量表达式中，例如 `$+1`、`$+?`、`WID-1-?`
- 目的侧表达式在实际连接时还会叠加索引范围判断，只有范围内的节点才会真正生成连接


## USE_CHILD_SERVICE_PORT(instance, serv, alias, ...)

声明把子实例中的服务事务端口引出为父模块内部可直接调用的隐式 request：
- `instance`：子实例名，标量实例可直接写名字，阵列化子实例可写带索引表达式的实例名
- `serv`：子实例中的服务事务端口名
- `alias`：在父模块逻辑代码中使用的名字
- `...`：与该服务端口一致的 `ARG(...)` / `RESP(...)` 参数列表

与旧语法相比，这里新增了显式的 `alias` 参数。

### 标量或单点引出

```cpp
USE_CHILD_SERVICE_PORT(lane[0], left_in, lane0_in, ARG(uint32_t) data);
```

这会在父模块行为代码中引出一个可直接调用的名字：

```cpp
lane0_in(5);
```

### 通配一维引出

如果在阵列化子实例上使用 `*`，则引出的隐式 request 本身也是阵列化的：

```cpp
USE_CHILD_SERVICE_PORT(mesh[*][0], left_in, mesh_in, ARG(uint32_t) data);
```

这会在父模块逻辑中引出一个按维度编号访问的名字：

```cpp
mesh_in<0>(1);
mesh_in<1>(10);
```

上例中，`mesh[*][0]` 表示把第一列所有节点的 `left_in` 服务端口引出为一个长度为 `HEI` 的阵列化隐式 request。


## 阵列化连接规则

阵列化子实例的事务连接仍然使用以下宏：
- `CONNECT_CR_CS(srcmod, srcreq, dstmod, dstserv)`
- `CONNECT_CR_S(srcmod, srcreq, dstserv)`
- `CONNECT_CR_R(srcmod, srcreq, dstreq)`
- `CONNECT_S_CS(srcserv, dstmod, dstserv)`

其中：
- `CR` 表示 child request
- `CS` 表示 child service
- `S` 表示 parent service
- `R` 表示 parent request

### 1. 阵列内部规则连接

阵列内部规则连接通常使用 `$`、`?` 来描述一组有规律的边：

```cpp
CONNECT_CR_CS(lane[$], right_out, lane[$+1], left_in);
```

表示把所有 `lane[i]` 的 `right_out` 连接到 `lane[i+1]` 的 `left_in`。

二维情况示例：

```cpp
CONNECT_CR_CS(mesh[$][?], right_out, mesh[$+1][?+1], left_in);
```

表示把所有 `mesh[i][j]` 的 `right_out` 连接到 `mesh[i+1][j+1]` 的 `left_in`。

还可以在部分维度上写常量：

```cpp
CONNECT_CR_CS(mesh[0][?], right_out, mesh[0][?+1], left_in);
```

表示仅连接第 0 行上的相邻节点。

目的侧也可以使用更一般的表达式：

```cpp
CONNECT_CR_CS(mesh[$][?], out, mesh[$+?][0], in);
```

这条规则是合法的。其含义是：
- 源侧使用 `[$][?]` 定义两个循环变量
- 目的侧第 0 维索引为 `$+?`
- 目的侧第 1 维索引恒为 `0`

实际生成连接时，还会检查 `$+?` 是否落在目标阵列的合法范围内，因此通常只有部分源节点会真正形成连接。

### 2. 阵列到父模块 service

当子实例 request 连接到父模块 service 时，可以在子实例一侧使用 `*`：

```cpp
CONNECT_CR_S(mesh[*][WID-1], right_out, output);
```

含义是：
- 选择 `mesh` 的最后一列节点
- 把每个节点的 `right_out` 聚合为一个阵列化 request
- 再连接到父模块中同维度的阵列化 service `output`

如果 `mesh` 的维度为 `[HEI][WID]`，则这里得到的是一个长度为 `HEI` 的事务阵列。

### 3. 阵列到父模块 request

```cpp
CONNECT_CR_R(mesh[HEI-1][0], right_out, spill);
```

表示把左下角节点的 `right_out` 暴露为父模块的 `spill` request。

### 4. 父模块 service 到阵列子实例

```cpp
CONNECT_S_CS(input, mesh[0][0], left_in);
```

表示把父模块中的 service `input` 连接到 `mesh[0][0]` 的 `left_in`。


## 索引变量的含义

在规则化连接中：
- `$` 和 `?` 表示的是两类循环变量本身，而**不强制绑定到源实例中的固定维度位置**
- 因此源侧写成 `mesh[$][?]` 或 `mesh[?][$]` 都是合法的
- `$` 始终引用源侧由 `$` 所在维度提供的索引值
- `?` 始终引用源侧由 `?` 所在维度提供的索引值

例如：

```cpp
CONNECT_CR_CS(mesh[$][?], right_out, mesh[$+1][?+1], left_in);
```

可理解为：

```cpp
for (uint32_t i = 0; i < HEI; ++i) {
    for (uint32_t j = 0; j < WID; ++j) {
        if (i + 1 < HEI && j + 1 < WID) {
            mesh[i][j].right_out -> mesh[i+1][j+1].left_in;
        }
    }
}
```

这里的“规则”在静态解析阶段会被检查其维度合法性，而在代码生成阶段保留为规则化描述进行处理。


## 生成行为

### simgen

在仿真代码中：
- 阵列化子实例只生成**一份**子模块实例类
- 父模块持有该子模块的智能指针数组
- 子模块构造函数带有若干个 `uint32_t` 索引参数，用于标识当前节点在阵列中的坐标
- 父模块对阵列化子实例 request 的包装函数，其参数列表前部也会携带这些索引参数

对于规则化连接，`simgen` 不会简单把所有连接完全展开成大量分支，而是尽量保留偏移表达式，例如：

```cpp
if (__instidx0 + 1u < LEN) {
    ...
}
```

### rtlgen

在 RTL 生成中，阵列化连接必须最终**显式展开**到具体的信号和实例端口上。

需要区分两部分：

1. `top.logic.cpp`

这是提供给 HLS 的组合逻辑 C++ 函数，其参数表示逻辑模块端口：
- `Int<N>` 表示位宽为 `N` 的信号
- `std::array<T, N>` 表示一个长度为 `N` 的信号数组

2. `top.sv`

这是 RTL 框架模块：
- 会把阵列化事务端口映射成 SystemVerilog 信号数组
- 会把阵列化子实例的规则连接显式展开为具体线网和具体实例端口连接

因此，尽管 `simgen` 可以保留规则化表达式，`rtlgen` 的最终结果中，连接必须体现为：
- 明确的 `wire`
- 明确的 `child_instance.port(signal)` 端口绑定


## 使用示例

### 一维链式阵列

```cpp
REGISTER(cycle, uint32_t) {
    cycle = 0;
}

REQUEST(print, ARG(uint32_t) data);

CHILD_INSTANCE_ARRAY1(LineNode, lane, LEN);

USE_CHILD_SERVICE_PORT(lane[0], left_in, lane0_in, ARG(uint32_t) data);

CONNECT_CR_CS(lane[$], right_out, lane[$+1], left_in);
CONNECT_CR_R(lane[LEN-1], right_out, print);

TICK_IMPL() {
    if (cycle == 0) {
        lane0_in(5);
    }
    cycle.setnext(cycle + 1);
}
```

该例对应 `example/array1d`。

### 二维脉动阵列

```cpp
REQUEST(print, ARRAY(HEI), ARG(uint32_t) data);
REQUEST(spill, ARG(uint32_t) data);

SERVICE(output, ARRAY(HEI), ARG(uint32_t) data) {
    if constexpr (IDX == 0) {
        captured0.setnext(data);
    } else if constexpr (IDX == 1) {
        captured1.setnext(data);
    }
}

CHILD_INSTANCE_ARRAY2(DiagNode, mesh, HEI, WID);

USE_CHILD_SERVICE_PORT(mesh[*][0], left_in, mesh_in, ARG(uint32_t) data);

CONNECT_CR_CS(mesh[$][?], right_out, mesh[$+1][?+1], left_in);
CONNECT_CR_S(mesh[*][WID-1], right_out, output);
CONNECT_CR_R(mesh[HEI-1][0], right_out, spill);
```

该例对应 `example/systolic2d`。


## 约束与注意事项

1. 阵列维度必须是可静态求值的正整数常量表达式。
2. 当前子实例阵列宏只直接提供到二维，即 `CHILD_INSTANCE_ARRAY1` 和 `CHILD_INSTANCE_ARRAY2`。
3. `$` 只能表示第一维，`?` 只能表示第二维。
4. 在内部规则连接的源实例侧，`$` / `?` 若出现，则必须单独占据一个维度，不能写成带运算的表达式。
5. 在内部规则连接的目的实例侧，`$` / `?` 可以出现在任意合法索引表达式中。
6. `*` 仅允许出现在子实例与父模块边界连接中。
7. 当前最多支持一个 `*`。
8. 阵列化子实例若参与连接或 `USE_CHILD_SERVICE_PORT`，必须显式写出索引表达式，不能只写实例名。
9. `CONNECT_CR_S(mesh[*][WID-1], right_out, output)` 这种写法要求父模块端口 `output` 本身也是维度匹配的阵列化事务端口。
10. `USE_CHILD_SERVICE_PORT(mesh[*][0], left_in, mesh_in, ...)` 引出的 `mesh_in` 是模板风格调用接口，调用时应写作 `mesh_in<IDX>(...)`。
