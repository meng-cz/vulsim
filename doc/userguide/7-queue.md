# 7. Queue 内置队列组件

VulCPP 提供了内置队列组件，支持：

- 单元素入队/出队队列 `VulQueue`
- 多元素并行入队/出队队列 `VulQueueMP`

它们都采用 `next` 语义：本周期发起入队/出队，在下一个周期提交后生效。

## 7.1 组件定义

可以通过以下宏在模块 hpp 文件中定义队列：

```cpp
// 定义一个单宽队列：深度为 depth，元素类型为 type
// depth 必须是编译期常量，且 depth >= 1
QUEUE(name, type, depth);

// 定义一个多宽队列：深度 depth，每周期最多 enqwidth 个入队、deqwidth 个出队
// depth/enqwidth/deqwidth 都必须是编译期常量，且都 >= 1
QUEUE_MP(name, type, depth, enqwidth, deqwidth);
```

## 7.2 单宽队列 `VulQueue` 接口

接口原型如下：

```cpp
bool enqready() const;
bool deqvalid() const;
bool enqnext(const T &value);
const T& deqnext();
void clrnext();
```

语义说明：

1. `enqready()`：当前周期是否允许发起一次入队请求。
2. `enqnext(value)`：发起一次入队请求，成功返回 `true`，失败返回 `false`。
3. `deqvalid()`：当前周期是否有可出队元素。
4. `deqnext()`：读取当前队头数据，并登记“本周期出队一次”。
5. `clrnext()`：登记清空队列请求，在下个周期提交时生效。

同周期限制（非常重要）：

1. 单宽队列每周期最多调用一次 `enqnext()`，调用后该周期 `enqready()` 变为无效。
2. 单宽队列每周期最多调用一次 `deqnext()`，调用后该周期 `deqvalid()` 变为无效。
3. `deqnext()` 不会在调用当周期立刻移除元素，真正弹出发生在下周期提交时。

## 7.3 多宽队列 `VulQueueMP` 接口

接口原型如下：

```cpp
uint32_t enqreqdy() const;
uint32_t deqvalid() const;
uint32_t enqnext(const std::array<T, EnqWidth> &values, uint32_t num = EnqWidth);
const std::array<T, DeqWidth>& deqnext(uint32_t num = DeqWidth);
void clrnext();
```

语义说明：

1. `enqreqdy()`：当前周期最多还能接受多少个入队，范围 `[0, EnqWidth]`。
2. `enqnext(values, num)`：请求入队 `num` 个元素，返回实际接收数量。当请求数量大于 `enqreqdy()` 时，队列会接受前 `enqreqdy()` 个元素，剩余元素被拒绝。
3. `deqvalid()`：当前周期最多可出队的元素数量，范围 `[0, DeqWidth]`。
4. `deqnext(num)`：返回当前可见队头窗口，并登记最多 `num` 个出队请求。
5. `clrnext()`：登记清空请求，下周期提交时生效。

同周期限制：

1. 每周期最多调用一次 `enqnext()`；调用后同周期 `enqreqdy()` 视为 0。
2. 每周期最多调用一次 `deqnext()`；调用后同周期 `deqvalid()` 视为 0。
3. 返回的数组长度恒为 `DeqWidth`，但只有前 `deqvalid()` 个元素在当前周期有效。

## 7.4 时序与行为规则

两类队列都遵循“先登记请求，后统一提交”的时序。

1. 入队请求与出队请求都在“下一周期提交”时更新队列状态。
2. 同一提交点内，队列先处理出队，再处理入队。
3. 清空请求在提交阶段最先执行，然后再执行出队/入队逻辑。
4. 如果同周期既出队又入队，等价于先弹出再写入。

建议使用模式：

1. 先用 `deqvalid()`/`enqready()`（或 `enqreqdy()`/`deqvalid()`）判断可用性。
2. 本周期内仅调用一次对应的 `*_next()` 接口。
3. 不依赖同周期重复调用接口的副作用。

## 7.5 `example/fifotest` 使用示例

下面是仓库中单宽队列的典型用法。

### 7.5.1 队列定义与服务导出（`FIFO.hpp`）

```cpp
CONST(QUEUE_SIZE, 8);
QUEUE(q, uint32_t, QUEUE_SIZE);

SERVICE_READY(deq, q.deqvalid(), RESP(uint32_t) data) {
    data = q.deqnext();
}

REGISTER(cycle, uint32_t) {
    cycle = 0;
}

TICK_IMPL() {
    cycle_setnext(cycle_get() + 1);
    if (cycle_get() % 2 == 0) {
        q.enqnext(cycle_get());
    }
}
```

这个模块做了两件事：

1. 每隔一个周期向队列入队一个 `cycle` 值。
2. 通过 `SERVICE_READY(deq, q.deqvalid(), ...)` 把 `deqvalid()` 直接作为服务就绪条件，只有可出队时才允许外部调用 `deqnext()`。

### 7.5.2 消费端按节拍读取（`Reader.hpp`）

```cpp
REQUEST_READY(deq, RESP(uint32_t) data);
REQUEST(output, ARG(uint32_t) data);

TICK_IMPL() {
    cycle_setnext(cycle_get() + 1);
    if ((cycle_get() >> 1) % 2 == 0) {
        uint32_t enq_data = 0;
        if (deq(enq_data)) {
            output(enq_data);
        }
    }
}
```

消费者并不是每周期都读，而是按自己的节拍发起 `deq` 请求。请求是否成功由队列 `deqvalid()` 决定，空队列时不会读出无效数据。

### 7.5.3 顶层连接与仿真输出

`Top.hpp` 中把 `reader.deq` 连接到 `fifo.deq`，`TestMain.cpp` 中打印 `output` 数据，构成完整的“生产者-队列-消费者”示例。

## 7.6 常见注意事项

1. `deqnext()`/`enqnext()` 名字中的 `next` 表示“提交到下一周期”，不是组合逻辑即时生效。
2. 对单宽队列，建议始终使用 `if (q.deqvalid()) { ... q.deqnext(); }` 和 `if (q.enqready()) { ... q.enqnext(...); }` 的防护写法。
3. 对多宽队列，始终以返回值（实际接收数）/`enqreqdy()`（实际可入队数）/`deqvalid()`（实际可读数）为准，不要假设请求数一定全被接受。
