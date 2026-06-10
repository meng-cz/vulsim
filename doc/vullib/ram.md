# `vullib/ram.hpp` 开发文档

本文档覆盖 `VulBRAM1RW`、`VulBRAM`、`VulROM` 三类组件，并重点说明：

- 读请求/写请求在哪一拍被消费。
- 同拍读写同地址时返回旧值还是新值。
- 构造函数中与文件相关参数是否真的生效。

## 1. 组件概览

```cpp
template <typename DataT, uint32_t Size>
class VulBRAM1RW;

template <typename DataT, uint32_t Size, uint32_t ReadPorts, uint32_t WritePorts>
class VulBRAM;

template <uint32_t DataWidth, uint32_t Size, uint32_t ReadPorts>
class VulROM;
```

所有地址类型均使用：

```cpp
static constexpr uint32_t AddrWidth = log2ceil(Size);
using AddrType = Int<AddrWidth>;
```

## 2. `VulBRAM1RW<DataT, Size>`

### 2.1 接口定义

```cpp
template <typename DataT, uint32_t Size>
class VulBRAM1RW {
public:
    VulBRAM1RW();
    VulBRAM1RW(const std::string &path, bool hex);

    void req(const AddrType &addr, const DataT &write_data, bool write_en);
    const DataT& readdata() const;
    void apply_next_tick();
};
```

- `Size` 表示地址空间大小，也就是存储体一共有多少个元素槽位。
- 地址位宽不再由模板参数直接给出，而是由 `log2ceil(Size)` 自动计算。
- 但由于当前 `Int` 类型要求位宽大于 0，现实现只接受 `Size > 1`。
- `Size` 不是 2 的幂时，地址类型仍可表示更大的编码空间，但超出 `[0, Size)` 的地址在当前实现下不会做边界检查，行为未定义。

### 2.2 状态模型

- `memory_`：实际存储阵列，默认值初始化。
- `addr_ / write_data_ / write_en_`：当前挂起的一组请求。
- `read_data_`：当前对外暴露的读数据寄存器。

### 2.3 `req(...)`

- 记录下一次 `apply_next_tick()` 要使用的地址、写数据、写使能。
- 若同一个 tick 内多次调用，后一次会覆盖前一次。

### 2.4 `apply_next_tick()` 的提交顺序

顺序固定如下：

1. 先执行 `read_data_ = memory_[addr_]`。
2. 若 `write_en_ == true`，再执行 `memory_[addr_] = write_data_`。

### 2.5 由此得到的语义

- 这是一个单端口、单请求接口。
- 读地址与写地址永远是同一个 `addr_`，因为接口只有一组地址。
- 对同一个地址在同一次 `apply_next_tick()` 内同时读写时，`readdata()` 看到的是写前旧值。
- `readdata()` 返回的是最近一次 `apply_next_tick()` 更新后的寄存器值，不是组合读。

### 2.6 构造函数说明

- `VulBRAM1RW()` 不从文件加载。
- `VulBRAM1RW(const std::string&, bool)` 当前实现也不使用 `path` 和 `hex` 参数。
- 即，带文件参数的构造函数目前与默认构造没有行为差别。


## 3. `VulBRAM<DataT, Size, ReadPorts, WritePorts>`

### 3.1 接口定义

```cpp
template <typename DataT, uint32_t Size, uint32_t ReadPorts, uint32_t WritePorts>
class VulBRAM {
public:
    VulBRAM();
    VulBRAM(const std::string &path, bool hex);

    template <uint32_t PortIndex>
    void readreq(const AddrType &addr);

    template <uint32_t PortIndex>
    const DataT& readdata() const;

    template <uint32_t PortIndex>
    void write(const AddrType &addr, const DataT &data);

    void apply_next_tick();
};
```

### 3.2 状态模型

- `memory_`：实际 RAM。
- `read_addresses_[i]`：每个读端口当前挂起的读地址。
- `read_data_[i]`：每个读端口当前可见的读寄存器值。
- `write_addresses_[i] / write_data_[i]`：每个写端口当前挂起的写请求。
- `write_enables_`：按位表示本 tick 哪些写端口有效。

### 3.3 `readreq<PortIndex>(addr)`

- 记录该读端口下一次 `apply_next_tick()` 要读取的地址。
- 同一 tick 内对同一端口多次调用时，后一次覆盖前一次。

### 3.4 `write<PortIndex>(addr, data)`

- 记录该写端口的挂起写请求，并把对应使能位置 1。
- 同一 tick 内对同一写端口多次调用时：
  - 地址和数据会被后一次覆盖。
  - 使能位保持为 1。

### 3.5 `apply_next_tick()` 的提交顺序

顺序固定如下：

1. 先对所有读端口执行：
   - `read_data_[i] = memory_[read_addresses_[i]]`
2. 再对所有有效写端口执行写入：
   - `memory_[write_addresses_[i]] = write_data_[i]`
3. 最后将 `write_enables_` 清零。

### 3.6 由此得到的语义

- 这是“先读后写”的同步 RAM。
- 同一次 `apply_next_tick()` 中，若某读端口读取的地址恰好被某写端口写入，则该读端口读到的是写前旧值。
- 多写端口写同一地址时，端口索引越大的写端口最后执行，因此最终内存保留最大端口号的写入值。
- `readdata<PortIndex>()` 返回的是上一次 `apply_next_tick()` 更新后的寄存器值。

### 3.7 构造函数说明

- `VulBRAM()` 默认零初始化。
- `VulBRAM(const std::string&, bool)` 当前也不使用 `path` 与 `hex` 参数。

## 4. `VulROM<DataWidth, Size, ReadPorts>`

### 4.1 接口定义

```cpp
template <uint32_t DataWidth, uint32_t Size, uint32_t ReadPorts>
class VulROM {
public:
    using DataType = Int<DataWidth>;
    using AddrType = Int<AddrWidth>;

    VulROM(const std::string &readmemh_path);

    template <uint32_t PortIndex>
    void readreq(const AddrType &addr);

    template <uint32_t PortIndex>
    const DataType readdata() const;

    void apply_next_tick();
};
```

### 4.2 基本语义

- 构造时会调用 `init_from_readmemh(readmemh_path)`。
- ROM 存储默认零初始化，然后按文件内容覆盖。
- 读行为是同步读：
  - `readreq()` 记录地址
  - `apply_next_tick()` 才把 `memory_[addr]` 取到 `read_data_[i]`
- `readdata()` 按值返回 `DataType`，不是引用返回。

### 4.3 `readmemh` 装载规则

- 支持 token 之间用空白分隔。
- 支持 `@<hex_addr>` 跳转地址。
- 支持 `0x` / `0X` 前缀。
- 支持下划线 `_` 分隔。
- 支持行注释：
  - `//`
  - `#`
- 默认 `strict_width = false`：
  - token 宽度超过 `DataWidth` 时不会报错。
  - 多出的高位会被截断。
- 若地址超过深度，则抛异常。

### 4.4 私有 `readmemb` 装载规则

- 头文件中存在 `init_from_readmemb()`，但当前没有公开构造函数或公开 API 调用它。
- 其规则与 `readmemh` 类似，但 token 只能由 `0/1/_` 组成。

### 4.5 待确认项

- `VulROM` 对外只暴露 `readmemh` 构造路径，`readmemb` 实现目前属于内部未导出功能。
- `strict_width` 没有对外公开入口，因此当前使用者无法切换到严格宽度检查模式。

## 5. 需求评审建议重点

- BRAM 带文件参数构造函数当前未实际装载文件，是否需要补齐。
- ROM 是否需要正式支持 `readmemb` 构造入口。
- 对于 `Size` 不是 2 的幂的情况，当前接口会生成 `log2ceil(Size)` 位地址，但实现没有越界保护；若上层给出超出 `[0, Size)` 的地址，会直接访问越界，建议与需求团队明确是否要求：
  - 强制 `Size` 为 2 的幂；
  - 或在模拟库中加入断言/检查。
