# VUL / VulCPP 硬件项目生成规则列表

## A. 总体定位

1. VUL / VulCPP 是用 C++ 宏语法描述周期同步硬件的建模框架，不是普通 C++ 软件框架。
2. 普通硬件模块必须采用可综合风格：无 I/O、无标准库（除了array）、无系统调用、无动态分配、无运行期全局状态。
3. Main 仿真模块是测试入口，可以使用 `printf`、文件 I/O、全局变量、仿真循环和外部激励。
4. 状态必须由 `REGISTER`、`REGISTER_MUL`、`REGISTER_ARRAY1`、`WIRE` 或内置组件表示。
5. 状态更新是 next-cycle 语义；`xxxnext()` 不立即改变本周期状态值。
6. 默认不要依赖同周期执行顺序；优先通过事务拆分、多写端口寄存器、模块划分消除顺序依赖。
7. 生成项目时优先生成能运行的最小完整项目，再扩展复杂结构，同步生成关键模块单元测试。

---

## B. 文件与模块规则

8. 一个项目至少包含：

   * `header.hpp`：全局配置、结构体、类型别名。
   * 若干模块 `.hpp`：如 `Top.hpp`、`ALU.hpp`、`FIFO.hpp`。
   * 至少一个 `Main.cpp`：仿真入口。
9. 项目可以有多个 Main 文件，分别用于不同模块单元测试。
10. 文件名即模块名；模块名在项目全局唯一。
11. 推荐目录：

```text
project/
  header.hpp
  Top.hpp
  ModuleA.hpp
  ModuleB.hpp
  part1/
    ModuleC.hpp
  Main.cpp
```

12. 普通模块文件头：

```cpp
#pragma once
#include <cstdint>
#include "header.hpp"
```

13. Main 文件头：

```cpp
#include <cstdio>
#include <cstdint>
#include "header.hpp"

TOP("./Top.hpp");
PROJECT(".");
```

14. `TOP(path)` 指定顶层模块文件。
15. `PROJECT(path)` 指定项目根目录。

---

## C. header.hpp 规则

16. `header.hpp` 只放共享定义。默认引用cstdint以使用uint32_t系列类型。
17. 可用宏：

```cpp
CONFIG(NAME, value_expr);
STRUCT(Name) { ... };
ALIAS(Name, Type);
ALIAS_ARRAY1(Name, Type, N);
ALIAS_ARRAY2(Name, Type, N1, N2);
```

18. 示例：

```cpp
#pragma once
#include <cstdint>

CONFIG(DATA_WIDTH, 32);
CONFIG(QUEUE_DEPTH, 8);

STRUCT(Packet) {
    uint32_t data;
    bool last;
};

ALIAS(Word, uint32_t);
ALIAS_ARRAY1(Vec4, uint32_t, 4);
```

19. `CONFIG` 是int64_t表达式，可使用整数、其他 `CONFIG`、括号、算术、位运算、比较、逻辑、三元运算符。
20. `@X` 表示 `log2ceil(X)`。

```cpp
CONFIG(SIZE, 16);
CONFIG(ADDR_WIDTH, @SIZE);
```

21. 禁止循环依赖：

```cpp
CONFIG(A, B + 1); // illegal
CONFIG(B, A + 1); // illegal
```

---

## D. 普通硬件模块可用宏

22. 模块局部定义：

```cpp
PARAMETER(name, default_value);
CONFIG(name, value);
STRUCT(name) { ... };
ALIAS(name, type);
ALIAS_ARRAY1(name, type, N);
ALIAS_ARRAY2(name, type, N1, N2);
HELPER() { ... }
```

23. 状态：

```cpp
REGISTER(name, type) { reset_code }
REGISTER_MUL(name, type, portnum) { reset_code }
REGISTER_ARRAY1(name, type, size, portnum) { reset_code }
WIRE(name, type) { init_expr }
```

24. 事务：

```cpp
REQUEST(name, ARG(type) arg, ..., RESP(type) resp, ...);
REQUEST_READY(name, ARG(type) arg, ..., RESP(type) resp, ...);

SERVICE(name, ARG(type) arg, ..., RESP(type) resp, ...) { ... }
SERVICE_READY(name, condition, ARG(type) arg, ..., RESP(type) resp, ...) { ... }
SERVICE_PRIO(name, priority, ARG(type) arg, ..., RESP(type) resp, ...) { ... }
SERVICE_PRIO_READY(name, priority, condition, ARG(type) arg, ..., RESP(type) resp, ...) { ... }

QUERY(name, rettype) { ... }
TICK_IMPL() { ... }
```

25. 子模块：

```cpp
CHILD_INSTANCE(ModuleType, inst, optional_param_override...);
CHILD_INSTANCE_ARRAY1(ModuleType, inst, N0, optional_param_override...);
CHILD_INSTANCE_ARRAY2(ModuleType, inst, N0, N1, optional_param_override...);

USE_CHILD_SERVICE_PORT(instance, service, alias, ARG(...), RESP(...));
USE_CHILD_QUERY(instance, query, alias, rettype);
```

26. 连接：

```cpp
CONNECT_CR_CS(src_child, src_request, dst_child, dst_service);
CONNECT_CR_S(src_child, src_request, parent_service);
CONNECT_CR_R(src_child, src_request, parent_request);
CONNECT_S_CS(parent_service, dst_child, dst_service);
```

---

## E. REGISTER 规则

27. `REGISTER(name, type)` 定义周期状态。
28. reset 代码必须完整初始化所有字段。
29. 普通寄存器示例：

```cpp
REGISTER(counter, uint32_t) {
    counter = 0;
}
```

30. 读取当前值：

```cpp
uint32_t x = counter;
uint32_t y = counter.get();
```

31. 结构体寄存器读取字段时必须使用 `.get()`：

```cpp
uint32_t v = status.get().value;
```

32. 更新寄存器必须使用：

```cpp
counter.setnext(counter + 1);
```

33. `setnext()` 在下一次 `sim_commit()` 后生效。
34. 本周期读到的仍是旧值。
35. 同一寄存器同一写端口每周期最多一次 `setnext()`。
36. 多分支写同一寄存器时，推荐先计算 next 值，最后统一写：

```cpp
TICK_IMPL() {
    uint32_t next = count;
    if (inc) next = next + 1;
    if (clr) next = 0;
    count.setnext(next);
}
```

37. 若多个来源必须同周期写同一状态，优先使用 `REGISTER_MUL`。

---

## F. REGISTER_MUL 规则

38. 多写端口寄存器：

```cpp
REGISTER_MUL(state, uint32_t, 2) {
    state = 0;
}
```

39. 写端口：

```cpp
state.setnext<0>(v0);
state.setnext<1>(v1);
```

40. 端口编号越小，优先级越高。
41. 不同端口可同周期写同一寄存器。
42. 同一端口仍然每周期最多写一次。
43. 推荐用多写端口表达覆盖关系，而不是滥用 `SERVICE_PRIO`。

---

## G. REGISTER_ARRAY1 规则

44. 寄存器数组：

```cpp
REGISTER_ARRAY1(regfile, uint32_t, 32, 1) {
    for (int i = 0; i < 32; ++i) regfile[i] = 0;
}
```

45. 读取：

```cpp
uint32_t x = regfile[3];
```

46. 写入：

```cpp
regfile.setnext<0>(idx, value);
```

47. 对同一数组元素，同一写端口每周期最多写一次。
48. 不同数组元素可以同周期写。
49. 同一元素多来源写入时，使用多个写端口并明确优先级。

---

## H. WIRE 规则

50. `WIRE` 是本周期临时变量：

```cpp
WIRE(temp, uint32_t) {
    temp = 0;
}
```

51. `WIRE` 每周期开始重置为初值。
52. `WIRE` 不跨周期保存值。
53. 不推荐在 `QUERY` 中依赖 `WIRE`。
54. `QUERY` 应优先读取寄存器、子模块 `QUERY` 和内置组件只读接口。

---

## I. REQUEST / SERVICE 事务规则

55. `REQUEST` 表示本模块主动发起事务。
56. `SERVICE` 表示本模块响应事务。
57. 普通请求：

```cpp
REQUEST(output, ARG(uint32_t) data);
```

58. 普通服务：

```cpp
SERVICE(input, ARG(uint32_t) data) {
    reg.setnext(data);
}
```

59. ready-valid 请求：

```cpp
REQUEST_READY(push, ARG(uint32_t) data);
```

60. 调用 ready-valid 请求必须检查返回值：

```cpp
if (push(value)) {
    accepted.setnext(accepted + 1);
}
```

61. ready-valid 服务：

```cpp
SERVICE_READY(recv, fifo_ready, ARG(uint32_t) data) {
    ...
}
```

62. `SERVICE_READY` 的 ready 条件必须无副作用。
63. ready 条件中禁止写寄存器、调用 I/O、调用有副作用事务或改变状态。

---

## J. SERVICE_PRIO 顺序约束规则

64. 默认假设模块内所有 `SERVICE` 与 `TICK_IMPL` 不依赖执行顺序。
65. 只有确实需要表达“同周期覆盖关系”时才使用 `SERVICE_PRIO` / `SERVICE_PRIO_READY`。
66. `TICK_IMPL` 的基准优先级为 `0`。
67. 正 priority 表示服务早于 Tick 生效。
68. 负 priority 表示服务晚于 Tick 生效。
69. 数值越小，执行越靠后，其寄存器写入越容易覆盖前面的写入。
70. 顺序约束会沿事务调用链传播。
71. 深层调用链中应避免依赖 `SERVICE_PRIO`。
72. 同一实例的 `TICK_IMPL` 不能调用自身带 `SERVICE_PRIO` 的服务。
73. 不要让同一带优先级 `SERVICE` 被多个不同实例的 Tick 路径调用。
74. 多来源写同一寄存器时，优先使用多写端口寄存器表达覆盖关系。

---

## K. ARG / RESP 规则

75. 事务参数类型：

```cpp
ARG(type) name
RESP(type) name
```

76. `ARG` 是只读输入。
77. `RESP` 是只写输出。
78. 禁止读取 `RESP` 进入事务时的旧值。
79. 带响应服务：

```cpp
SERVICE(read, ARG(uint32_t) addr, RESP(uint32_t) data) {
    data = addr + 1;
}
```

80. 请求端调用：

```cpp
REQUEST(read, ARG(uint32_t) addr, RESP(uint32_t) data);

TICK_IMPL() {
    uint32_t r;
    read(5, r);
    out.setnext(r);
}
```

---

## L. 连接规则

81. 连接只能在父模块中声明。
82. 子请求到子服务：

```cpp
CONNECT_CR_CS(childA, req, childB, serv);
```

83. 子请求暴露为父请求：

```cpp
CONNECT_CR_R(child, req, parent_req);
```

84. 子请求连接到父服务：

```cpp
CONNECT_CR_S(child, req, parent_serv);
```

85. 父服务转发到子服务：

```cpp
CONNECT_S_CS(parent_serv, child, serv);
```

86. 请求和服务参数列表必须一致。
87. 请求和服务的 ready-valid 属性必须一致。
88. request 只能连接一个 service。
90. 子模块 request 不能悬空。
91. 父模块 service 必须实现，或连接到子模块 service。

---

## M. 子模块调用规则

92. 子实例：

```cpp
CHILD_INSTANCE(Producer, prod);
CHILD_INSTANCE(Consumer, cons);
CONNECT_CR_CS(prod, send, cons, recv);
```

93. 参数覆盖：

```cpp
CHILD_INSTANCE(Cache, icache, SIZE=64, WAY=4);
```

94. 父模块直接调用子模块 service 时，必须引出端口：

```cpp
USE_CHILD_SERVICE_PORT(fifo, deq, fifo_deq, RESP(uint32_t) data);
```

95. 调用示例：

```cpp
TICK_IMPL() {
    uint32_t x;
    if (fifo_deq(x)) {
        ...
    }
}
```

96. 父模块读取子模块 query 时：

```cpp
USE_CHILD_QUERY(node, snapshot, node_snapshot, ChildSnapshot);
```

97. query 调用示例：

```cpp
QUERY(snapshot, TopSnapshot) {
    TopSnapshot s;
    s.child = node_snapshot();
    return s;
}
```

---

## N. QUERY 规则

98. `QUERY(name, rettype)` 是无副作用只读查询。
99. `QUERY` 不参与 `CONNECT_*`。
100. `QUERY` 没有 `ARG` / `RESP`。
101. `QUERY` 直接返回一个值。
102. `QUERY` 允许一周期多次调用。
103. 同一周期多次调用应返回一致结果。
104. `QUERY` 禁止写寄存器。
105. `QUERY` 禁止调用 `setnext()`。
106. `QUERY` 禁止发起事务。
107. `QUERY` 禁止调用 `enqnext()`、`deqnext()`、RAM 读写请求等有副作用接口。
108. `QUERY` 只读取：
     - 本模块寄存器当前值；
     - 子模块 `QUERY`；
     - 其他只读内置组件接口如队列 `enqready()` / `deqvalid()`。
109. 多字段返回值应定义结构体：

```cpp
STRUCT(NodeStatus) {
    uint32_t sum;
    bool ready;
};

QUERY(status, NodeStatus) {
    NodeStatus s;
    s.sum = sum;
    s.ready = q.enqready();
    return s;
}
```

---

## O. TICK_IMPL 规则

110. 每个模块可定义多个 `TICK_IMPL()`，跨代码块的执行顺序未定义。
111. `TICK_IMPL()` 定义模块每周期行为。
112. `TICK_IMPL()` 中禁止 I/O。
113. 状态更新必须使用 `setnext()` 或内置组件 next 接口。
114. 本周期读取寄存器得到旧值。
115. 禁止同周期重复调用同一非QUERY事务接口。
117. 顶层模块的 `TICK_IMPL()` 应尽量简单；复杂行为下沉到子模块。

---

## P. Main 仿真模块规则

118. Main 模块允许 I/O、全局变量和仿真控制。
119. Main 常用宏：

```cpp
TOP("./Top.hpp");
PROJECT(".");

GLOBAL() { ... }
VAR(type, name);
VAR_INIT(type, name, init);

REQUEST(name, ARG(...), RESP(...));
REQUEST_READY(name, ARG(...), RESP(...));

SERVICE(name, ARG(...), RESP(...)) { ... }
SERVICE_READY(name, condition, ARG(...), RESP(...)) { ... }

SIMULATION() { ... }
```

120. Main 中定义与顶层模块方向相反的事务。
121. 顶层有 `REQUEST(output, ...)`，Main 写 `SERVICE(output, ...)`。
122. 顶层有 `SERVICE(input, ...)`，Main 写 `REQUEST(input, ...)`。
123. 仿真推进函数：

```cpp
sim_reset();
sim_execute();
sim_commit();
sim_exit();
```

124. 典型仿真循环：

```cpp
GLOBAL() {
    uint64_t cycle = 0;
}

SIMULATION() {
    sim_reset();

    for (cycle = 0; cycle < 100; ++cycle) {
        sim_execute();
        sim_commit();
    }
}
```

---

## Q. Queue 规则

125. 单宽队列：

```cpp
QUEUE(q, uint32_t, 8);
```

126. 单宽接口：

```cpp
q.enqready(); (const bool)
q.enqnext(value);
q.deqvalid(); (const bool)
q.deqnext();
q.clrnext();
```

127. 推荐写法：

```cpp
if (q.enqready()) {
    q.enqnext(value);
}

if (q.deqvalid()) {
    uint32_t x = q.front();
    q.deqnext();
}
```

128. `enqnext()` / `deqnext()` 是 next 语义。
129. 每周期最多一次 `enqnext()`。
130. 每周期最多一次 `deqnext()`。
131. `front()` 返回当前队头，`deqnext()` 只登记出队请求。
132. `clrnext()` 提交时优先于出队和入队。
133. 提交顺序：清空、出队、入队。
134. 调用 `enqnext()` 后不要继续依赖本周期 `enqready()`。
135. 调用 `deqnext()` 后不要继续依赖本周期 `deqvalid()`。
136. 多宽队列：

```cpp
QUEUE_MP(q, uint32_t, 16, 2, 2);
```

137. 多宽接口：

```cpp
q.enqreqdy(); (const uint32_t)
q.enqnext(values, num);
q.deqvalid(); (const uint32_t)
q.front(num);
q.deqnext(num);
q.clrnext();
```

138. `enqreqdy()` 返回当前最多可接受数量。
139. `enqnext(values, num)` 返回 `void`，调用前必须保证 `num <= enqreqdy()`。
140. `deqvalid()` 返回当前最多可出队数量。
141. `front(num)` 返回队头窗口，`deqnext(num)` 只登记出队请求，调用前必须保证 `num <= deqvalid()`。
142. 多宽队列必须以 valid/ready 数量为准，不能假设全部成功。

---

## R. BRAM / ROM 规则

143. BRAM / ROM 是同步读，不是组合读。
144. 单端口 1RW RAM：
     - `req(addr, write_data, write_en)` 登记请求。
     - `apply_next_tick()` 时先读旧值，再执行写。
     - `readdata()` 返回上一次提交后的读数据寄存器 (const)。
     - `req()` 每周期最多一次。
     - 只有上一周期发出读请求后，本周期 `readdata()` 才有效。
145. 多端口 BRAM：
     - `readreq<port>(addr)` 登记读请求。
     - `readdata<port>()` 返回上一周期读数据 (const)。
     - `write<port>(addr, data)` 登记写请求。
     - 提交时先读后写。
     - 同地址读写读到旧值。
     - 多写端口写同地址时，端口索引更大的写最后生效。
     - 每个读端口每周期最多一次 `readreq()`。
     - 每个写端口每周期最多一次 `write()`。
146. ROM：
     - 构造时从 `readmemh` 文件初始化。
     - 同步读：`readreq()` 登记地址，下一周期 `readdata()` (const) 可见。
     - `readmemh` 支持空白分隔 token、`@` 地址跳转、`0x` 前缀、下划线、`//` 注释、`#` 注释。
     - 超出数据位宽的 token 高位截断。
     - 地址超过深度视为错误。

---

## S. Int / fixint 规则

147. 定宽整数类型：

```cpp
Int<BitWidth>
```

148. `BitWidth > 0`。
149. 构造：

```cpp
Int<8> a = 3;
Int<16> b = a;
```

150. 导出：

```cpp
uint32_t x = a.to<uint32_t>();
```

151. 静态切片：

```cpp
auto low4 = x.at<3, 0>();
x.at<7, 4>() = Int<4>(0xF);
```

152. 动态 bit / slice：

```cpp
auto bit = x.pick(idx);
```

153. 有符号视图：

```cpp
auto sx = x.sint();
```

154. 拼接与重复：

```cpp
auto y = Cat(a, b);      // a 高位, b 低位
auto z = Repeat<4>(a);
```

155. 归约：

```cpp
bool all1 = ReduceAnd(x);
bool any1 = ReduceOr(x);
bool parity = ReduceXor(x);
```

156. `Int` 默认按无符号位向量解释。
157. `.sint()` 只改变解释方式，不复制数据。
158. `+`、`-` 只接受无符号 `Int/Ref`。
159. `*` 可接受无符号或 signed view。
160. unsigned `>>` 是逻辑右移。
161. signed view `>>` 是算术右移。
162. `& | ^ ~ <<` 不接受 signed view。
163. `< <= > >=` 可使用 signed view。
164. `== !=` 只接受等宽无符号 `Int/Ref`。
165. 不同宽度赋值应显式构造目标宽度，避免隐式截断。

---

## T. 阵列化子实例规则

166. 一维子实例阵列：

```cpp
CHILD_INSTANCE_ARRAY1(LineNode, lane, LEN);
```

167. 二维子实例阵列：

```cpp
CHILD_INSTANCE_ARRAY2(DiagNode, mesh, HEI, WID);
```

168. 规则连接使用 `$` 和 `?`：

```cpp
CONNECT_CR_CS(lane[$], right_out, lane[$+1], left_in);
CONNECT_CR_CS(mesh[$][?], right_out, mesh[$+1][?+1], left_in);
```

169. `$` 表示第一维循环变量。
170. `?` 表示第二维循环变量。
171. 源侧 `$` / `?` 必须单独占据一个维度。
172. 源侧禁止写 `lane[$+1]`。
173. 目的侧可以写 `$+1`、`?+1`、`WID-1-?` 等表达式。
174. USE_CHILD_SERVICE_PORT 边界通配使用 `*`：

```cpp
CONNECT_CR_S(mesh[*][WID-1], right_out, output);
USE_CHILD_SERVICE_PORT(mesh[*][0], left_in, mesh_in, ARG(uint32_t) data);
```

175. 通配引出的服务调用使用模板风格：

```cpp
mesh_in<0>(1);
mesh_in<1>(10);
```

176. 当前只直接支持一维和二维子实例阵列。
177. `*` 只用于子实例与父模块边界连接或引出。
178. 当前最多支持一个 `*`。
179. 阵列子实例参与连接时必须显式写索引表达式。
180. 父模块端口若接收 `mesh[*]` 聚合连接，本身也必须是维度匹配的阵列化事务端口。
181. 阵列化服务端口可用：

```cpp
SERVICE(output, ARRAY(HEI), ARG(uint32_t) data) {
    if constexpr (IDX == 0) {
        captured0.setnext(data);
    } else if constexpr (IDX == 1) {
        captured1.setnext(data);
    }
}
```

---

## U. 推荐模块生成顺序

182. 普通模块推荐顺序：

```cpp
#pragma once
#include <cstdint>
#include "header.hpp"

// local config / alias / struct
CONFIG(...);

// helper
HELPER() { ... }

// state
REGISTER(...);
REGISTER_ARRAY1(...);
QUEUE(...);

// ports
REQUEST(...);
SERVICE(...) { ... }
QUERY(...) { ... }

// child instances
CHILD_INSTANCE(...);
USE_CHILD_SERVICE_PORT(...);
USE_CHILD_QUERY(...);

// connections
CONNECT_CR_CS(...);

// tick
TICK_IMPL() { ... }
```

183. 顶层模块通常只负责：
     - 声明对外事务端口；
     - 实例化子模块；
     - 连接子模块事务；
     - 必要时引出子模块接口；
     - 少量顶层调度逻辑。
184. 复杂行为应放入子模块，不要堆在顶层。

---

## V. 常用模式

### V1. Producer-Consumer

```cpp
// Producer
REGISTER(cycle, uint8_t) { cycle = 0; }
REGISTER(count, uint8_t) { count = 0; }

REQUEST_READY(send, ARG(uint8_t) d);

TICK_IMPL() {
    cycle.setnext(cycle + 1);
    if (send(cycle)) {
        count.setnext(count + 1);
    }
}
```

```cpp
// Consumer
REGISTER(cycle, uint8_t) { cycle = 0; }
REGISTER(sum, uint8_t) { sum = 0; }

REQUEST(output, ARG(uint8_t) s);

SERVICE_READY(recv, (cycle & 1) == 0, ARG(uint8_t) d) {
    sum.setnext(sum + d);
    output(sum);
}

TICK_IMPL() {
    cycle.setnext(cycle + 1);
}
```

```cpp
// Top
REQUEST(output, ARG(uint8_t) s);

CHILD_INSTANCE(Producer, prod);
CHILD_INSTANCE(Consumer, cons);

CONNECT_CR_CS(prod, send, cons, recv);
CONNECT_CR_R(cons, output, output);
```

### V2. FIFO wrapper

```cpp
CONFIG(QUEUE_SIZE, 8);

QUEUE(q, uint32_t, QUEUE_SIZE);

SERVICE_READY(deq, q.deqvalid(), RESP(uint32_t) data) {
    data = q.front();
    q.deqnext();
}

REGISTER(cycle, uint32_t) {
    cycle = 0;
}

TICK_IMPL() {
    cycle.setnext(cycle + 1);
    if (cycle % 2 == 0 && q.enqready()) {
        q.enqnext(cycle);
    }
}
```

### V3. QUERY snapshot

```cpp
STRUCT(NodeStatus) {
    uint32_t sum;
    bool ready;
};

REGISTER(sum, uint32_t) {
    sum = 0;
}

QUEUE(q, uint32_t, 4);

QUERY(status, NodeStatus) {
    NodeStatus s;
    s.sum = sum;
    s.ready = q.enqready();
    return s;
}
```

### V4. 1D chain

```cpp
CHILD_INSTANCE_ARRAY1(LineNode, lane, LEN);

USE_CHILD_SERVICE_PORT(lane[0], left_in, lane0_in);

CONNECT_CR_CS(lane[$], right_out, lane[$+1], left_in);
CONNECT_CR_R(lane[LEN-1], right_out, print);

TICK_IMPL() {
    if (cycle == 0) {
        lane0_in(5);
    }
    cycle.setnext(cycle + 1);
}
```

### V5. 2D systolic-like mesh

```cpp
CHILD_INSTANCE_ARRAY2(DiagNode, mesh, HEI, WID);

USE_CHILD_SERVICE_PORT(mesh[*][0], left_in, mesh_in, ARG(uint32_t) data);

CONNECT_CR_CS(mesh[$][?], right_out, mesh[$+1][?+1], left_in);
CONNECT_CR_S(mesh[*][WID-1], right_out, output);
CONNECT_CR_R(mesh[HEI-1][0], right_out, spill);

TICK_IMPL() {
    if (cycle == 0) {
        mesh_in<0>(1);
        mesh_in<1>(10);
    }
    cycle.setnext(cycle + 1);
}
```

---

## W. Trace / Debug 规则

185. trace 命令示例：

```bash
./vulsimgen -m Main.cpp --trace "top::*.*"
```

186. trace 匹配格式：

```text
instance_path.signal_path
```

187. 实例路径用 `::` 分层。
188. 信号路径用 `.` 分层。
189. 示例：

```text
top::conn.*
top::*.*
top::cpu.regfile.*
*.valid
top::mesh[0][0].datareg
top::mesh[*][0].datareg
top::mesh.datareg
```

190. 阵列实例匹配：
     - `top::mesh[0][0].datareg`：精确单元。
     - `top::mesh[*][0].datareg`：某一列或某一维通配。
     - `top::mesh.datareg`：整个阵列所有单元。
191. 实例路径必须用 `::`，禁止写成 `top.mesh[0][0].datareg`。
192. 断点必须配合 trace 使用：

```bash
./vulsimgen -m Main.cpp \
  --trace "top::mesh.datareg" \
  --break "(top::mesh[0][0].datareg == 1) && (top::mesh[1][0].datareg == 10)"
```

193. 断点左侧必须是被 trace 的精确信号路径。
194. 断点条件中禁止使用 `*`。
195. 多条件使用 `&&`。
196. 常量支持十进制、`0b...`、`0x...`。
197. 断点文件每行是一组断点，行间 OR。

---

## X. 禁止生成的代码

198. 普通硬件模块中禁止 `printf`、`std::cout`、文件读写、随机数、系统调用。
199. 普通硬件模块中禁止运行期全局变量和普通成员变量。
200. `HELPER()` 中禁止定义运行期状态变量。
201. 禁止同一周期同一寄存器写端口多次 `setnext()`。
202. 禁止同一周期同一队列多次 `enqnext()`。
203. 禁止同一周期同一队列多次 `deqnext()`。
204. 禁止同一周期同一 RAM 读端口多次 `readreq()`。
205. 禁止同一周期同一 RAM 写端口多次 `write()`。
206. 禁止在 `QUERY` 中调用有副作用接口。
207. 禁止在 `SERVICE_READY` 条件中写状态或调用 I/O。
208. 禁止读取 `RESP` 参数旧值。
209. 禁止子模块 request 悬空。
210. 禁止将带握手或带响应的 request 广播到多个 service。
211. 禁止在结构体寄存器上直接读取 `reg.field`；必须使用 `reg.get().field`。
212. 禁止把 `setnext()` 当成立即赋值。
213. 禁止把 BRAM / ROM 读当成组合读。
214. 禁止阵列连接源侧写 `lane[$+1]`。
215. 禁止断点条件中使用通配符 `*`。

---

## Y. 生成 VUL 项目的执行流程

216. 明确硬件模块划分。
217. 明确每个模块的状态寄存器。
218. 明确每个模块的 `REQUEST` / `SERVICE` / `QUERY` 接口。
219. 明确哪些事务需要 ready-valid 握手。
220. 明确子模块实例化关系。
221. 明确事务连接关系。
222. 判断是否需要 Queue、BRAM、ROM、寄存器数组、阵列化子实例。
223. 生成 `header.hpp`。
224. 生成普通模块 `.hpp`。
225. 生成顶层模块 `.hpp`。
226. 生成 `Main.cpp`。
227. 检查所有 `setnext()` 是否同端口同周期重复。
228. 检查所有 Queue/RAM 有副作用接口是否同周期重复。
229. 检查所有 request/service 参数是否一致。
230. 检查所有子模块 request 是否已连接。
231. 检查 Main 是否提供顶层外部端口的反向事务。
232. 给出运行命令。

---

## Z. 输出格式要求

233. 生成项目时按文件分块输出：

```text
// file: header.hpp
...

// file: ModuleA.hpp
...

// file: Top.hpp
...

// file: Main.cpp
...
```

234. 最后给出构建运行命令：

```bash
./vulsimgen -m Main.cpp -l /path/to/vullib -o sim_out
cd sim_out
source build.sh
./Main
```

235. 如果 `Main.cpp` 已写 `TOP(...)` 和 `PROJECT(...)`，通常只需传 `-m`。
236. 如需临时覆盖顶层或项目路径：

```bash
./vulsimgen -m Main.cpp -t Top.hpp -p .
```

---

## AA. 代码质量检查表

237. 模块边界清晰。
238. 每个寄存器完整复位。
239. 事务命名反映方向和语义，如 `push`、`pop`、`recv`、`send`、`output`。
240. ready-valid 请求调用必须检查返回值。
241. Queue 使用前检查 valid/ready。
242. RAM/ROM 访问遵循同步读延迟。
243. 不依赖 C++ 未定义行为。
244. 不依赖同周期多次调用副作用接口。
245. 不滥用 `SERVICE_PRIO`。
246. 顶层主要做结构连接。
247. Main 只负责仿真驱动和输出。
248. 优先生成短小、直接、可运行的最小完整项目。
