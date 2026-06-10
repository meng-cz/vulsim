# 6. BRAM 与 ROM 内置存储组件

## 6.1 组件定义

当前实现中，`BRAM` 不再支持“声明时文件初始化”。如果需要从文件加载内容，应使用 `ROM` 组件。

可用声明宏如下：

```cpp
// 多端口 BRAM：datatype 是数据类型（例如 UInt<32>）
BRAM(name, datatype, size, readports, writeports);

// 单端口 1RW BRAM：datatype 是数据类型（例如 UInt<32>）
BRAM_1RW(name, datatype, size);

// ROM：datawidth/size/readports 为编译期常量
// init_path 会被宏自动字符串化，写法示例见下文（不加双引号）
ROM(name, datawidth, size, readports, init_path);
```

这里的 `size` 表示存储体元素个数，不是地址位宽。地址类型位宽由库内部按 `log2ceil(size)` 自动推导。
当前实现要求 `size > 1`。另外，当 `size` 不是 2 的幂时，地址类型仍会扩展到 `log2ceil(size)` 位；若上层给出超出 `[0, size)` 的地址，模拟行为未定义。

## 6.2 多端口 BRAM 读写接口

`VulBRAM` 读写时序如下：
- 读请求当周期给地址，下一周期通过同端口 `readdata` 取数。
- 写请求在 `apply_next_tick()` 时生效；同一 tick 内可对多个写端口发请求。
- `readdata<PortIndex>()` 同一周期可多次调用，返回相同结果。
- 仅当上一周期该端口调用过 `readreq<PortIndex>()` 时，本周期 `readdata<PortIndex>()` 才有效。
- `readreq<PortIndex>()` / `write<PortIndex>()` 对每个端口同一周期只允许调用一次。
  非 `release` 编译下多次调用会触发 `assert`；`release` 编译下属于未定义行为。
- 若上一周期该端口没有 `readreq<PortIndex>()`，则本周期调用 `readdata<PortIndex>()`：
  非 `release` 编译下会触发 `assert`，`release` 编译下属于未定义行为；当前实现通常保留旧值。

接口定义：

```cpp
template <uint32_t PortIndex>
void readreq(const UInt<addrwidth> &addr);

template <uint32_t PortIndex>
const datatype& readdata() const;

template <uint32_t PortIndex>
void write(const UInt<addrwidth> &addr, const datatype &data);
```

冲突语义：
1. 同周期同地址读写冲突时，先读后写，读到的是本周期写入前的值。
2. 多个写端口同周期写同一地址时，端口号大的写端口最终生效。

示例：

```cpp
BRAM(myram, UInt<32>, 1024, 2, 1);

TICK_IMPL() {
    UInt<32> data0 = myram.readdata<0>(); // 上周期在端口0发的读请求在本周期返回数据
    UInt<32> data1 = myram.readdata<1>();

    myram.write<0>(0x3FF, value);

    myram.readreq<0>(0x3FF); // 读到的是本周期写入发生前的旧值
    myram.readreq<1>(0x000);
}
```

## 6.3 单端口 BRAM（1RW）接口

`VulBRAM1RW` 使用共享读写端口：
- `req(addr, data, true)` 表示写请求。
- `req(addr, data, false)` 表示读请求（`data` 参数被忽略）。
- `readdata()` 同一周期可多次调用，返回相同结果。
- 仅当上一周期调用的是读请求 `req(..., false)` 时，本周期 `readdata()` 才有效。
- `req()` 同一周期只允许调用一次。非 `release` 编译下多次调用会触发 `assert`；`release` 编译下属于未定义行为。
- 同周期对同一地址同时发起读写语义时，返回的是写入前旧值。
- 若上一周期没有 `req()`，或上一周期调用的是写请求 `req(..., true)`，则本周期调用 `readdata()`：
  非 `release` 编译下会触发 `assert`，`release` 编译下属于未定义行为；当前实现通常保留旧值。

接口定义：

```cpp
void req(const UInt<addrwidth> &addr, const datatype &write_data, bool write_en);
const datatype& readdata() const;
```

示例：

```cpp
BRAM_1RW(myram, UInt<32>, 1024);

TICK_IMPL() {
    myram.req(0x3FF, 0xDEADBEEF, true);
    myram.req(0x000, 0, false); // 覆盖本周期前一次 req

    UInt<32> data = myram.readdata();
}
```

## 6.4 ROM 组件与初始化文件

`ROM` 组件是只读存储器，数据在构造时由文件加载。接口如下：

```cpp
template <uint32_t PortIndex>
void readreq(const UInt<addrwidth> &addr);

template <uint32_t PortIndex>
const UInt<datawidth> readdata() const;
```

`readdata<PortIndex>()` 同一周期可多次调用，返回相同结果。仅当上一周期该端口调用过 `readreq<PortIndex>()` 时，本周期 `readdata<PortIndex>()` 才有效；否则非 `release` 编译下会触发 `assert`，`release` 编译下属于未定义行为，当前实现通常保留旧值。`readreq<PortIndex>()` 对每个端口同一周期只允许调用一次；非 `release` 编译下多次调用会触发 `assert`，`release` 编译下属于未定义行为。

声明示例：

```cpp
ROM(inst_rom, 32, 1024, 1, data/program.hex);
```

说明：
- `ROM(...)` 宏内部会把 `init_path` 字符串化，因此参数写路径标记即可，不要再额外加双引号。
- 默认按十六进制文本格式加载（等价于 `$readmemh` 风格）。

初始化文件规则（ROM）：
- 支持空白分隔的 token，支持空行。
- 支持注释：`//` 与 `#`。
- 支持地址跳转：`@<hex_addr>`。
- 十六进制 token 支持可选 `0x/0X` 前缀，支持 `_` 分隔符。
- 二进制 token 支持 `0/1` 与 `_` 分隔符（对应 `VulROM(path, false)` 构造方式）。
