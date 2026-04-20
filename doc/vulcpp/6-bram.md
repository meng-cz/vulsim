# 6. BRAM 内置内存块组件

## 6.1 组件定义

VulCPP 提供了一个名为 `VulBRAM` 的内置内存块组件，可以通过以下宏在模块hpp文件中定义：

```cpp
// 定义一个名为 name 的 BRAM 组件，数据宽度为 datawidth 位，地址宽度为 addrwidth 位，具有 readports 个读端口和 writeports 个写端口
// datawidth, addrwidth, readports, writeports 都必须是编译时常量表达式
BRAM(name, datawidth, addrwidth, readports, writeports);

// 从十六进制文本文件 path 初始化 BRAM
// 文件格式同 verilog 的 $readmemh，path 必须是字符串常量
BRAM_INIT_H(name, datawidth, addrwidth, readports, writeports, "init_file.hex");

// 从二进制文本文件 path 初始化 BRAM
// 文件格式同 verilog 的 $readmemb，path 必须是字符串常量
BRAM_INIT_B(name, datawidth, addrwidth, readports, writeports, "init_file.mem");
```

特殊的，对于仅有一个共同读写端口的 BRAM，可以使用以下宏定义：

```cpp
BRAM_1RW(name, datawidth, addrwidth);
BRAM_1RW_INIT_H(name, datawidth, addrwidth, "init_file.hex");
BRAM_1RW_INIT_B(name, datawidth, addrwidth, "init_file.mem");
```

## 6.2 多端口定义下的读写端口定义

VulBRAM 读端口的地址和数据是分离的。在一个端口给出读地址后，下一个周期才能在相同端口获取到读数据。读端口函数定义：

```cpp
// 读端口函数，port_id 从 0 开始编号
// addr 是地址输入，宽度为 addrwidth 位
template <uint32_t PortIdx>
void VulBRAM::readreq(UInt<addrwidth> addr);

// 下一周期的读数据获取函数，port_id 从 0 开始编号
// 返回值是读数据输出，宽度为 datawidth 位
template <uint32_t PortIdx>
const UInt<datawidth> VulBRAM::readdata() const;
```

VulBRAM 的写请求会在下一个周期生效。写端口函数定义：

```cpp
// 写端口函数，port_id 从 0 开始编号
// addr 是地址输入，宽度为 addrwidth 位
// data 是数据输入，宽度为 datawidth 位
template <uint32_t PortIdx>
void VulBRAM::write(UInt<addrwidth> addr, UInt<datawidth> data);
```

端口冲突：
1. 写端口优先。如果一个周期内读写端口访问同一地址，写操作会覆盖读操作，读端口会得到写入的数据。
2. 多个写端口访问同一地址时，优先级由端口编号决定，编号较大的端口优先。

使用示例：

```cpp
BRAM(myram, 32, 10, 2, 1); // 定义一个数据宽度为32位，地址宽度为10位，具有2个读端口和1个写端口的BRAM

TICK_IMPL() {
    // 读端口0的读请求
    myram.readreq<0>(0x3FF); // 读地址为0x3FF
    // 读端口1的读请求
    myram.readreq<1>(0x000); // 读地址为0x000

    // 获取上一周期读端口0的数据
    UInt<32> data0 = myram.readdata<0>();
    // 获取上一周期读端口1的数据
    UInt<32> data1 = myram.readdata<1>();

    // 写端口的写请求
    myram.write<0>(0x3FF, 0xDEADBEEF); // 写地址为0x3FF，写数据为0xDEADBEEF
}
```


## 6.3 单端口 BRAM 定义下的读写端口定义

对于单端口 BRAM，读写端口是共享的。在一个周期内，如果给出地址和数据，则进行写操作；如果只给出地址，则进行读操作。函数定义如下：

```cpp
// 读写端口函数
void VulBRAM1RW::req(UInt<addrwidth> addr, const UInt<datawidth> &write_data, bool write_en);

// 下一周期的读数据获取函数
const UInt<datawidth> VulBRAM1RW::readdata() const;
```

使用示例：

```cpp
BRAM_1RW(myram, 32, 10); // 定义一个数据宽度为32位，地址宽度为10位的单端口BRAM

TICK_IMPL() {
    // 读写端口的请求
    myram.req(0x3FF, 0xDEADBEEF, true); // 写地址为0x3FF，写数据为0xDEADBEEF，进行写操作
    myram.req(0x000, 0, false); // 读地址为0x000，不提供写数据，进行读操作，会覆盖之前的同周期的写请求

    // 获取上一周期的读数据
    UInt<32> data = myram.readdata();
}
```

## 6.4 初始化文件格式

BRAM 的初始化文件可以是十六进制文本文件或二进制文本文件。两者的格式如下：

### 十六进制文件格式
- 每行表示一个内存位置的值
- 地址以 `@` 开头，后跟十六进制地址
- 数据以 `0x` 或 `0X` 开头，后跟十六进制数据

### 二进制文件格式
- 每行表示一个内存位置的值
- 地址以 `@` 开头，后跟十六进制地址
- 数据以二进制形式表示


