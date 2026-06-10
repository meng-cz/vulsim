# `vullib/storage.hpp` 开发文档

本文档描述 `VulStorageNext` 与 `VulStorageNextArray` 的实现语义，重点关注：

- “当前值”和“下一拍值”的分离方式。
- 多写端口优先级。
- 同优先级重复写入的覆盖规则。

## 1. 组件概览

公开导出组件为：

```cpp
template<typename T, uint32_t WRPortNum = 1>
class VulStorageNext;

template<typename T, uint32_t Size, uint32_t WRPortNum = 1>
class VulStorageNextArray;
```

内部实现按写端口数和数组大小分化：

- `VulStorageNextImpl<T, WRPortNum>`
- `VulStorageNextImpl1<T>`
- `VulStorageNextArrayFullImpl<T, Size, WRPortNum>`
- `VulStorageNextArrayDirtyImpl<T, Size, WRPortNum>`

## 2. `VulStorageNext<T, WRPortNum>`

### 2.1 接口定义

```cpp
template<typename T, uint32_t WRPortNum = 1>
class VulStorageNext {
public:
    template <uint32_t P = 0>
    void setnext(const T &value);

    void apply_next_tick();
    const T& get() const;
    operator const T&() const;
    void reset(const T &value);
};
```

### 2.2 语义概述

- `get()` 返回当前已提交值。
- `setnext()` 只修改“下一拍候选值”。
- `apply_next_tick()` 把候选值提交为当前值。
- `reset()` 同时设置当前值和下一拍候选值。

## 3. `WRPortNum == 1` 的实现

### 3.1 内部行为

- 内部使用 `VulStorageNextImpl1<T>`。
- 只有两个状态：
  - `data_`：当前值
  - `next_buffer_`：下一拍值

### 3.2 规则

- `setnext()` 每次都会覆盖 `next_buffer_`。
- 同一 tick 内多次调用 `setnext()` 时，最后一次写入生效。
- `apply_next_tick()` 执行 `data_ = next_buffer_`。
- 如果某个 tick 没有调用 `setnext()`，则 `next_buffer_` 保持原值，因此下一次 `apply_next_tick()` 会再次把旧候选值提交到 `data_`。

### 3.3 由此得到的理解

- 这是“保持型 next 寄存器”。
- `setnext()` 不是边沿脉冲，而是修改一个一直存在的 next latch。

## 4. `WRPortNum > 1` 的实现

### 4.1 内部行为

- 内部使用 `VulStorageNextImpl<T, WRPortNum>`。
- 额外状态 `pending_write_ports_` 表示当前已接受的最高优先级写端口号。

### 4.2 优先级规则

- 端口号越小，优先级越高。
- `setnext<P>(value)` 仅当 `P < pending_write_ports_` 时才会更新候选值。

### 4.3 同 tick 多次写入行为

- 若先调用高优先级端口，再调用低优先级端口：
  - 低优先级会被忽略。
- 若先调用低优先级端口，再调用高优先级端口：
  - 高优先级会覆盖之前的候选值。
- 若同一端口号 `P` 在同一 tick 调用多次：
  - 只有第一次会生效。
  - 之后因为 `P < pending_write_ports_` 不再成立，后续同优先级写入会被忽略。

### 4.4 `apply_next_tick()`

- 执行 `data_ = next_`。
- 然后把 `pending_write_ports_` 重置为 `WRPortNum`，准备接受下一拍写入。

### 4.5 待确认项

- “同优先级重复写入 first-wins” 不是很多用户的直觉，很多系统更常见的是 last-wins，需要确认是否为预期。

## 5. `VulStorageNextArray<T, Size, WRPortNum>`

### 5.1 接口定义

```cpp
template<typename T, uint32_t Size, uint32_t WRPortNum = 1>
class VulStorageNextArray {
public:
    VulStorageNextArray();
    VulStorageNextArray(const T &initial_value);

    template <uint32_t P = 0>
    void setnext(uint32_t index, const T &value);

    void apply_next_tick();
    const T& operator[](uint32_t index) const;
    void reset(const T &value);
    void reset(const std::array<T, Size> &values);
};
```

### 5.2 实现选择

- `Size <= 16` 时使用 `VulStorageNextArrayFullImpl`
- `Size > 16` 时使用 `VulStorageNextArrayDirtyImpl`

两种实现目标是优化策略不同，对外语义应保持一致。

## 6. `VulStorageNextArrayFullImpl`

### 6.1 语义

- 内部是 `std::array<VulStorageNext<T, WRPortNum>, Size>`。
- 每个元素独立维护自己的当前值与下一拍值。
- `apply_next_tick()` 会遍历所有元素并提交。

### 6.2 多次写入规则

- 同一个数组元素的写入规则与标量 `VulStorageNext` 完全一致。
- 不同数组下标互不影响。

## 7. `VulStorageNextArrayDirtyImpl`

### 7.1 语义

- `curr_` 保存当前值。
- `pending_[i]` 保存每个下标的待提交写入信息：
  - `value`
  - `best_prio`
  - `has_write`
- `dirty_indices_` 记录本 tick 实际被写过的下标。

### 7.2 `setnext(index, value)`

- 仅当：
  - 该下标本 tick 还没有写入
  - 或者本次端口优先级更高
  时才更新 `pending_[index]`。
- 如果该下标第一次变脏，则被记录到 `dirty_indices_`。

### 7.3 `apply_next_tick()`

- 只遍历 `dirty_indices_` 中的下标。
- 对每个脏下标执行：
  - `curr_[index] = pending_[index].value`
  - 清除 `has_write`
  - 清除 `dirty_flags_`
- 最后把 `dirty_count_` 清零。

### 7.4 同优先级重复写入

- 与标量多端口版本一样，是 first-wins。
- 因为判断条件是 `P < slot.best_prio`，不是 `<=`。

### 7.5 `reset(...)`

- `reset(value)` 会把当前值和 pending 值都改成 `value`，并清空脏标记。
- `reset(array)` 对每个下标分别设置当前值和 pending 值。

## 8. 默认构造与初始化

- 当前实现中，标量与数组内部状态都做了值初始化。
- 默认构造后：
  - 标量当前值为 `T{}`
  - 数组各元素当前值为 `T{}`
- 若立刻执行 `apply_next_tick()`，不会读到未初始化垃圾值。

## 9. 待确认项

- 多端口同优先级重复写入采用 first-wins，而不是 last-wins。
- `setnext()` 的候选值会跨 tick 保持，直到被新的 `setnext()` 覆盖；这不是“必须每拍都重发 next”的一次性协议，需要确认这是否符合团队对 next-storage 的理解。
- `VulStorageNextArray` 在 `Size <= 16` 与 `Size > 16` 时内部实现不同，但外部没有显式标识，若将来扩展行为必须保证两套实现继续等价。
