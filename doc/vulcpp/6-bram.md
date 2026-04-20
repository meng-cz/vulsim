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

## 6.2 读端口

VulBRAM 读端口的地址和数据是分离的。在一个端口给出读地址后，下一个周期才能在相同端口获取到读数据。端口函数定义：

```cpp
// 读端口函数，port_id 从 0 开始编号
// addr 是地址输入，宽度为 addrwidth 位
template <uint32_t PortIdx>
void VulBRAM::readreq(UInt<addrwidth> addr);

// 下一周期的读数据获取函数，port_id 从 0 开始编号
// 返回值是读数据输出，宽度为 datawidth 位
template <uint32_t PortIdx>
void VulBRAM::readdata(UInt<datawidth> &data_out);
```

## 6.3 写端口

VulBRAM 的写请求会在下一个周期生效。默认为写先，即如果同周期对同一个地址既有读请求又有写请求，则下周期读出的数据为写入的数据而非写入前的数据。端口函数定义：

```cpp
// 写端口函数，port_id 从 0 开始编号
// addr 是地址输入，宽度为 addrwidth 位
// data 是数据输入，宽度为 datawidth 位
template <uint32_t PortIdx>
void VulBRAM::write(UInt<addrwidth> addr, UInt<datawidth> data);
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


