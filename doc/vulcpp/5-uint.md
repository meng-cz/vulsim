# 5. Int 任意宽度整数类型

`vullib/uint.hpp` 当前实现的核心类型是 `Int<BitWidth>`。

## 5.1 类型定义与基本常量

定义方式：

```cpp
Int<32> a;
Int<1> flag;
```

约束：
- `BitWidth > 0`（编译期检查）。

类内常量：
- `Int<BitWidth>::WORD_BITS`：固定为 `64`。
- `Int<BitWidth>::NUM_WORDS`：底层 `uint64_t` 分块数量。
- `Int<BitWidth>::IDX_WIDTH`：`log2ceil(BitWidth)`，用于 `range_at/bit_at` 的索引位宽。

典型示例：

```cpp
static_assert(Int<130>::WORD_BITS == 64);
static_assert(Int<130>::NUM_WORDS == 3);
```

## 5.2 构造、宽度转换、类型转换

支持的构造 API：
- `Int()`：默认清零。
- `Int(bool)`：`false -> 0`，`true -> 1`。
- `Int(T)`：`T` 为内建整数类型，按目标宽度截断；若 `T` 为有符号且目标更宽，会做符号扩展。
- `Int(const Int<OtherBitWidth>&)`：按位复制，窄到宽零扩展，宽到窄截断。
- `Int(const IntSignedView<OtherBitWidth>&)`：按有符号语义进行宽度变换。

典型示例：

```cpp
Int<8> a;                // 0
Int<8> b = true;         // 1
Int<8> c = 0x1234;       // 0x34（截断）
Int<16> d = Int<8>(0xAB);// 0x00AB（零扩展）
Int<16> e = Int<8>(-1).sint(); // 0xFFFF（符号扩展）
```

类型导出 API：
- `template <typename T> T to() const`：导出为内建整数类型（`T` 必须是整数）。
- `sint()`：得到 `IntSignedView<BitWidth>`，用于有符号运算/比较/右移。

典型示例：

```cpp
Int<8> x = 0xFF;
uint32_t ux = x.to<uint32_t>(); // 255
int32_t sx = x.to<int32_t>();   // -1（按 BitWidth=8 的符号位做扩展）
```

## 5.3 位选与位段访问 API

范围/位访问：
- `operator()(hi, lo)`：返回可读写范围代理。
- `operator()(bit)`：返回可读写单比特代理。
- `const` 对象返回只读代理。

动态索引访问：
- `range_at<DstBitWidth>(Int<IDX_WIDTH> idx)`：从 `lo=idx` 开始取 `DstBitWidth` 位。
- `bit_at(Int<IDX_WIDTH> idx)`：动态取单 bit。

代理可做的事情：
- 读：可转成 `Int<N>` 或内建整数类型。
- 写：可赋值 `Int<M>`、内建整数、另一个范围代理。

典型示例：

```cpp
Int<32> a = 0x12345678;

Int<16> hi = a(31, 16);   // 0x1234
uint8_t low8 = a(7, 0);   // 0x78
bool b3 = a(3);           // 第 3 位

a(31, 16) = 0xABCD;       // a = 0xABCD5678
a(7, 4) = a(3, 0);        // 范围到范围赋值
a(0) = true;              // 单 bit 赋值

Int<64> wide = 0;
wide(10, 3) = 0xA5;
Int<Int<64>::IDX_WIDTH> idx = 3; // 同 Int<6> idx，因为 log2ceil(64)=6
Int<8> mid = wide.range_at<8>(idx); // 取 wide(10,3)
bool dyn_bit = wide.bit_at(idx);    // 取 wide(3)
```

## 5.4 归约、拼接、重复

归约 API：
- `reduce_or()`：任一 bit 为 1 则为真。
- `reduce_and()`：全部 bit 为 1 才为真。
- `reduce_xor()`：bit 总奇偶（奇数个 1 为真）。

典型示例：

```cpp
Int<4> a = 0b1010;
bool any1 = a.reduce_or();   // true
bool all1 = a.reduce_and();  // false
bool odd1 = a.reduce_xor();  // false（2 个 1）
```

拼接与重复 API：
- `repeat<K>()`：把当前值重复 `K` 次，结果位宽 `BitWidth*K`。
- `cat(other)`：`this` 放高位，`other` 放低位。
- `Cat(a, b, c, ...)`：多参数拼接，第一个参数在最高位。

典型示例：

```cpp
Int<4> n = 0b1100;
Int<12> r = n.repeat<3>(); // 1100_1100_1100

Int<8> h = 0xAB;
Int<8> l = 0xCD;
Int<16> x = h.cat(l);      // 0xABCD
Int<24> y = Cat(Int<8>(0x12), Int<8>(0x34), Int<8>(0x56)); // 0x123456
```

## 5.5 运算符 API（与位宽规则）

算术运算：
- `a + b`：结果位宽 `max(Wa, Wb) + 1`。
- `a - b`：结果位宽 `max(Wa, Wb)`。
- `-a`：二补数取负，位宽不变。
- `a * b`：结果位宽 `Wa + Wb`；支持无符号与 `sint()` 有符号混合乘法。

典型示例：

```cpp
Int<8> a = 200, b = 100;
auto s = a + b;            // Int<9>
auto d = a - b;            // Int<8>
auto n = -a;               // Int<8>
auto p0 = Int<8>(0xF0) * Int<8>(2);          // 无符号乘法
auto p1 = Int<8>(0xF0).sint() * Int<8>(2);   // 有符号乘法（-16 * 2）
```

按位运算：
- `& | ^`：两侧位宽必须相同，结果位宽不变。
- `~`：结果位宽不变。

典型示例：

```cpp
Int<8> a = 0xA5, b = 0x3C;
Int<8> x = a & b;
Int<8> y = a | b;
Int<8> z = a ^ b;
Int<8> w = ~a;
```

移位运算：
- `a << sh`、`a >> sh`：`sh` 可是 `Int<M>` 或内建整数，结果位宽不变。
- `a.sint() >> sh`：算术右移（符号扩展）。
- 当 `sh >= BitWidth` 时：
- 逻辑左/右移结果为全 0。
- 算术右移：若原值为负，结果为全 1；否则全 0。

典型示例：

```cpp
Int<8> a = 0b11110000;
Int<8> l = a << 2;             // 11000000
Int<8> r0 = a >> 2;            // 00111100（逻辑右移）
Int<8> r1 = a.sint() >> 2;     // 11111100（算术右移）
Int<8> z = a << 99;            // 0
```

比较运算：
- `== != < <= > >=` 支持：
- `Int` 与 `Int`（可不同位宽）。
- `IntSignedView` 与 `Int/IntSignedView` 混合比较。
- `Int/IntSignedView` 与内建整数混合比较（有符号常量按有符号语义处理）。

典型示例：

```cpp
Int<8> u = 0xFF;               // 255
bool c0 = (u == 255);          // true
bool c1 = (u < 0);             // false（0 为有符号常量，但 u 默认无符号语义）
bool c2 = (u.sint() < 0);      // true（按 -1 比较）
bool c3 = (Int<4>(0xF).sint() == Int<8>(-1).sint()); // true
```

## 5.6 不支持的运算

当前 `Int` API 未提供 `/`、`%`。  
硬件描述中通常不建议直接使用除法组合逻辑；2 的幂除法/取模可用位截取或移位实现。
