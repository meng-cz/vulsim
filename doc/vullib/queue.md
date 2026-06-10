# `vullib/queue.hpp` 开发文档

本文档描述 `VulQueue` 与 `VulQueueMP` 的当前实现语义，重点是：

- 请求在哪个阶段生效。
- 读写与清空在同一个 tick 内如何交互。
- 多次调用同一接口时是否会覆盖之前的请求。

## 1. 组件概览

`queue.hpp` 当前包含两个组件：

```cpp
template<typename T, uint32_t Depth>
class VulQueue;

template<typename T, uint32_t Depth, uint32_t EnqWidth, uint32_t DeqWidth>
class VulQueueMP;
```

这两个组件都采用“请求在本 tick 记录，`apply_next_tick()` 时统一提交”的风格。

## 2. `VulQueue<T, Depth>`

### 2.1 接口定义

```cpp
template<typename T, uint32_t Depth>
class VulQueue {
public:
    VulQueue() = default;

    bool enqready() const;
    bool deqvalid() const;
    bool enqnext(const T &value);
    const T& front() const;
    void deqnext();
    void clrnext();
    void apply_next_tick();
};
```

### 2.2 状态模型

- `data_`：实际存储。
- `head_ / tail_ / size_`：当前已提交队列状态。
- `enq_buf_ / enq_pending_`：本 tick 的待入队请求。
- `deq_buf_ / deq_buf_valid_`：当前对外暴露的“已准备好被读取的头元素缓存”。
- `deq_pending_`：本 tick 的待出队请求。
- `clr_pending_`：本 tick 的待清空请求。

### 2.3 `enqready()`

- 返回值仅依据当前已提交状态 `size_ < Depth`。
- 不考虑本 tick 已经发出的 `deqnext()` 是否将在下一次 `apply_next_tick()` 释放空间。
- 不考虑本 tick 已经发出的 `enqnext()`。

### 2.4 `deqvalid()`

- 返回 `deq_buf_valid_`。
- 它表示当前 `deq_buf_` 是否有效。
- 该值只会在 `apply_next_tick()` 末尾更新。

### 2.5 `enqnext(const T&)`

- 单个 tick 内只允许调用一次。
- 若同一个 tick 内再次调用，debug 构建下会触发 `assert`。
- release 构建下重复调用的行为未定义。
- 如果当前 `enqready()` 为假，则返回 `false`。
- 否则把参数写入 `enq_buf_`，并把 `enq_pending_` 设为 `true`，返回 `true`。

### 2.6 `front()`

- 该接口返回当前 `deq_buf_` 的引用。
- 它不修改任何状态，也不会隐式设置 dequeue 请求。
- 调用方必须自行先检查 `deqvalid()`，否则可能读取到上一个周期残留的 `deq_buf_`。

### 2.7 `deqnext()`

- 单个 tick 内只允许调用一次。
- 若同一个 tick 内再次调用，debug 构建下会触发 `assert`。
- release 构建下重复调用的行为未定义。
- 该接口仅把 `deq_pending_` 设为 `true`。
- 它不返回数据。

### 2.8 `clrnext()`

- 单个 tick 内只允许调用一次。
- 若同一个 tick 内再次调用，debug 构建下会触发 `assert`。
- release 构建下重复调用的行为未定义。
- 仅记录“下一次 `apply_next_tick()` 需要清空”的请求。

### 2.9 `apply_next_tick()` 提交顺序

提交顺序固定如下：

1. 若 `clr_pending_ == true`，先把 `head_ / tail_ / size_` 清零。
2. 若 `deq_pending_ == true && size_ > 0`，执行一次出队。
3. 若 `enq_pending_ == true && size_ < Depth`，执行一次入队。
4. 清掉 `deq_pending_` 与 `enq_pending_`。
5. 根据新的 `size_` 重新生成 `deq_buf_` 与 `deq_buf_valid_`。

### 2.10 由实现直接得到的行为

- `clrnext()` 与 `enqnext()` 同 tick 并存时：
  - `apply_next_tick()` 会先清空。
  - 同 tick 的 enqueue 请求会被丢弃，不会在下一拍进入队列。
  - 结果是“下一拍队列全空”。
- `clrnext()` 与 `deqnext()` 同 tick 并存时：
  - 清空先发生，因此本次出队不会再消耗原有元素。
- `deqnext()` 与 `enqnext()` 同 tick 并存时：
  - 先出队，再入队。
  - 因此如果队列原本满，`enqready()` 仍然会在该 tick 返回 `false`，即使本 tick 已经调用了 `deqnext()`。

### 2.11 多次调用行为

- `front()`：
  - 多次调用没有副作用。
- `enqready()` / `deqvalid()`：
  - 多次调用没有副作用。
- `deqnext()`：
  - 重复调用在 debug 下触发 `assert`，release 下未定义。
- `enqnext()`：
  - 重复调用在 debug 下触发 `assert`，release 下未定义。
- `clrnext()`：
  - 重复调用在 debug 下触发 `assert`，release 下未定义。

## 3. `VulQueueMP<T, Depth, EnqWidth, DeqWidth>`

### 3.1 接口定义

```cpp
template<typename T, uint32_t Depth, uint32_t EnqWidth, uint32_t DeqWidth>
class VulQueueMP {
public:
    static constexpr uint32_t EnqCntWidth = log2ceil(EnqWidth + 1);
    static constexpr uint32_t DeqCntWidth = log2ceil(DeqWidth + 1);
    static constexpr uint32_t DepthWidth  = log2ceil(Depth + 1);

    using EnqCntInt   = Int<EnqCntWidth>;
    using DeqCntInt   = Int<DeqCntWidth>;
    using DepthCntInt = Int<DepthWidth>;

    VulQueueMP() = default;

    EnqCntInt enqreqdy() const;
    DeqCntInt deqvalid() const;
    EnqCntInt enqnext(const std::array<T, EnqWidth> &values,
                      const EnqCntInt num = EnqWidth);
    const std::array<T, DeqWidth>& front(const DeqCntInt num = DeqWidth) const;
    void deqnext(const DeqCntInt num = DeqWidth);
    void clrnext();
    void apply_next_tick();
};
```

### 3.2 `enqreqdy()`

- 返回“当前已提交状态下最多还能接受多少个 enqueue 请求”。
- 值域是 `0..EnqWidth`。
- 与单端口版本一样，它不考虑本 tick 尚未提交的 `deqnext()`。

### 3.3 `deqvalid()`

- 返回当前 `deq_buf_` 中有效元素个数。
- 该值只在 `apply_next_tick()` 后更新。

### 3.4 `enqnext(...)`

- 单个 tick 内只允许调用一次。
- 若同一个 tick 内再次调用，debug 构建下会触发 `assert`。
- release 构建下重复调用的行为未定义。
- 参数 `num` 表示本次请求入队的元素个数。
- 实现会先把 `num` 截断到不超过 `EnqWidth`。
- 再与 `enqreqdy()` 取最小值，得到 `accepted`。
- 仅 `values[0 .. accepted-1]` 会被拷入 `enq_buf_`。
- 返回值就是 `accepted`。
- `enq_pending_num_` 会被整个覆盖，因此一个 tick 内多次调用时，后一次请求会覆盖前一次请求。

### 3.5 `front(...)`

- 返回当前 `deq_buf_` 的引用。
- 当前实现中 `num` 参数不参与裁剪返回值，仅用于与 `deqnext(num)` 保持接口对称。
- 调用方应先读取 `deqvalid()`，再决定如何解释返回数组中的有效前缀。

### 3.6 `deqnext(...)`

- 单个 tick 内只允许调用一次。
- 若同一个 tick 内再次调用，debug 构建下会触发 `assert`。
- release 构建下重复调用的行为未定义。
- 参数 `num` 表示本次想在下一次 `apply_next_tick()` 中弹出的元素数。
- 实现会把 `num` 截断到不超过 `DeqWidth`。
- 再与当前 `deqvalid()` 取最小值，写入 `deq_pending_num_`。

### 3.7 `clrnext()`

- 单个 tick 内只允许调用一次。
- 若同一个 tick 内再次调用，debug 构建下会触发 `assert`。
- release 构建下重复调用的行为未定义。

### 3.8 `apply_next_tick()` 提交顺序

顺序固定如下：

1. 若 `clr_pending_ == true`，先把 `head_ / tail_ / size_` 清零。
2. 根据 `deq_pending_num_` 出队若干元素。
3. 根据 `enq_pending_num_` 入队若干元素。
4. 清零 `enq_pending_num_` 与 `deq_pending_num_`。
5. 重新计算 `deq_valid_num_`。
6. 按新的 `head_` 填充 `deq_buf_` 的前 `deq_valid_num_` 项。

### 3.9 由实现直接得到的行为

- 同 tick 的 dequeue 不会立即影响 `enqreqdy()` 的返回值。
- 同 tick 的 clear 先于 dequeue/enqueue 生效。
- `clrnext()` 与 `enqnext()` 同 tick 并存时，同 tick 的 enqueue 请求会被丢弃，下一拍队列保持全空。
- `front()` 不会声明 dequeue 请求，也不会立即修改 `deqvalid()`。
- `deqnext()` 只声明“下一个 tick 要出队多少个”，不会立即修改 `deqvalid()`。

### 3.10 多次调用行为

- 多次 `enqreqdy()` / `deqvalid()`：没有副作用。
- 多次 `front()`：没有副作用。
- 多次 `enqnext()`：重复调用在 debug 下触发 `assert`，release 下未定义。
- 多次 `deqnext()`：重复调用在 debug 下触发 `assert`，release 下未定义。
- 多次 `clrnext()`：重复调用在 debug 下触发 `assert`，release 下未定义。
