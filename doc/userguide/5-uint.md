# 5. Int 任意宽度整数类型

`vullib/fixint.hpp` 提供核心类型 `Int<BitWidth>`，用于描述任意正位宽的硬件整数/位向量。`Int` 默认按无符号位向量解释；需要按有符号补码解释时，使用 `.sint()` 创建有符号视图。

## 5.1 类型定义与基本常量

```cpp
Int<32> a;
Int<1> flag;
```

约束：
- `BitWidth > 0`。

类内常量：
- `Int<BitWidth>::WORD_BITS`：固定为 `64`。
- `Int<BitWidth>::NUM_WORDS`：底层 `uint64_t` word 数。
- `Int<BitWidth>::IDX_WIDTH`：`log2ceil(BitWidth)`，用于动态索引。

```cpp
static_assert(Int<130>::WORD_BITS == 64);
static_assert(Int<130>::NUM_WORDS == 3);
```

## 5.2 构造、赋值与导出

支持的常用构造：
- `Int()`：清零。
- `Int(bool)`：`false -> 0`，`true -> 1`。
- `Int(T)`：`T` 为标准整数时，按目标宽度截断；若 `T` 是有符号整数且目标更宽，会按符号位扩展。
- `Int(const Int<OtherWidth>&)`：无符号视图转换，窄到宽补零，宽到窄截断。
- `Int(ref_or_sint)`：可从 Slice、Bit、const Ref 和 `.sint()` 视图构造；`.sint()` 会按源符号位扩展或截断。

```cpp
Int<8> a;                       // 0
Int<8> b = true;                // 1
Int<8> c = 0x1234;              // 0x34
Int<16> d = Int<8>(0xAB);       // 0x00AB
Int<16> e = Int<8>(0x80).sint();// 0xFF80
```

导出为标准整数：

```cpp
Int<8> x = 0xFF;
uint32_t ux = x.to<uint32_t>(); // 255
int32_t sx = x.to<int32_t>();   // -1
```

## 5.3 位选与位段访问

静态访问：
- `a.at<Hi, Lo>()`：访问 `[Hi:Lo]`，返回 Slice Ref，可作为赋值左值。
- `a.at<Idx>()`：访问第 `Idx` 位，返回 Bit Ref，可作为赋值左值。

动态访问：
- `a.pick<DstWidth>(i)`：从第 `i` 位开始访问 `DstWidth` 位。
- `a.pick(i)`：访问第 `i` 位。

Slice/Bit Ref 支持读写：
- 读：可显式构造为同宽或其他宽度 `Int`。
- 写 Slice：接受同宽 `Int`、同宽 Slice、标准整数；不同宽 `Int/Ref` 需要显式构造目标宽度。
- 写 Bit：接受 `Int<1>`、其他 Bit、标准整数、`bool`。

```cpp
Int<32> a = 0x12345678;

Int<16> hi = a.at<31, 16>(); // 0x1234
uint8_t low8 = Int<8>(a.at<7, 0>()).to<uint8_t>();
bool b3 = static_cast<bool>(a.at<3>());

a.at<31, 16>() = 0xABCD;     // a = 0xABCD5678
a.at<7, 4>() = a.at<3, 0>(); // 同宽 Slice 赋值
a.at<0>() = true;

Int<64> wide = 0;
wide.pick<8>(3) = 0xA5;
Int<Int<64>::IDX_WIDTH> idx = 3; // 等价于 Int<6>，因为 clog2(64) == 6
Int<8> mid = wide.pick<8>(idx);
bool dyn_bit = static_cast<bool>(wide.pick(idx));
```

## 5.4 有符号视图

`.sint()` 不改变底层 bit，只改变后续构造、赋值、比较和乘法中的解释方式。

```cpp
Int<8> n = 0xFE;
Int<16> u = n;        // 0x00FE
Int<16> s = n.sint(); // 0xFFFE
```

当前已支持 `.sint()` 的场景包括：
- 作为构造/赋值来源时做符号扩展或截断。
- 参与乘法时对应操作数按有符号补码解释。
- 参与 `< <= > >=` 时按有符号数值比较。
- 作为右移左操作数时执行算术右移。

## 5.5 归约、拼接与重复

归约是自由函数：
- `ReduceOr(x)`：任意 bit 为 1。
- `ReduceAnd(x)`：全部有效 bit 为 1。
- `ReduceXor(x)`：有效 bit 中 1 的数量为奇数。

```cpp
Int<4> a = 0b1010;
bool any1 = ReduceOr(a);   // true
bool all1 = ReduceAnd(a);  // false
bool odd1 = ReduceXor(a);  // false
```

拼接与重复也是自由函数：
- `Repeat<N>(x)`：把 `x` 重复 `N` 次，返回 `Int<width(x) * N>`。
- `Cat(a, b, c, ...)`：从左到右按高位到低位拼接，返回宽度为所有参数宽度之和的 `Int`。

```cpp
Int<4> n = 0b1100;
Int<12> r = Repeat<3>(n); // 1100_1100_1100

Int<8> h = 0xAB;
Int<8> l = 0xCD;
Int<16> x = Cat(h, l);    // 0xABCD
Int<24> y = Cat(Int<8>(0x12), Int<8>(0x34), Int<8>(0x56));
```

`Reduce*`、`Repeat`、`Cat` 均接受 `Int`、Slice Ref、Bit Ref 和 const Ref，不接受 `.sint()` 视图。

## 5.6 算术运算

加法：
- `Int<A> + Int<B> -> Int<max(A, B) + 1>`。
- `Int<W> + integral` 会先把整数构造成 `Int<W>`，再复用加法。
- 加法只接受无符号 `Int/Ref`，不接受 `.sint()`。

减法：
- `Int<A> - Int<B> -> Int<max(A, B)>`。
- 不额外保留借位；溢出/下溢按结果位宽截断。
- 减法只接受无符号 `Int/Ref`，不接受 `.sint()`。

取负：
- `-x -> Int<width(x)>`。
- 接受无符号视图和 `.sint()` 视图，按补码规则取负。

乘法：
- `a * b -> Int<width(a) + width(b)>`。
- 普通 `Int/Ref` 按无符号解释。
- 任一操作数使用 `.sint()` 时，该操作数按有符号补码解释。

```cpp
Int<8> a = 200, b = 100;
auto s = a + b;                         // Int<9>
auto d = a - b;                         // Int<8>
auto n = -a;                            // Int<8>
auto p0 = Int<8>(0xF0) * Int<8>(2);     // Int<16> 无符号乘法
auto p1 = Int<8>(0xF0).sint() * Int<8>(2); // Int<16> 有符号乘法
```

## 5.7 位运算

支持：
- `& | ^`：两侧必须是等宽无符号 `Int/Ref`，结果同宽。
- `~`：接受无符号 `Int/Ref`，结果同宽。
- `& | ^` 可与标准整数混合；整数会先构造成另一侧同宽 `Int`。
- `<<`：左侧接受无符号 `Int/Ref`，右侧接受无符号 `Int/Ref` 或标准整数，结果保持左侧宽度。
- `>>`：左侧接受 `Int/Ref` 或 `.sint()`；无符号视图为逻辑右移，`.sint()` 为算术右移。右侧接受无符号 `Int/Ref` 或标准整数。

```cpp
Int<8> a = 0xA5, b = 0x3C;
Int<8> x = a & b;
Int<8> y = a | b;
Int<8> z = a ^ b;
Int<8> w = ~a;
Int<8> l = a << 2;
Int<8> r0 = a >> 2;        // logical right shift
Int<8> r1 = a.sint() >> 2; // arithmetic right shift
```

移位量为 signed 标准整数且小于 0 时会触发 `assert`。移位量大于或等于左侧宽度时返回全 0；右侧 `.sint()` 视图不被接受。

## 5.8 比较运算

等值比较：
- `== !=` 只接受等宽无符号 `Int/Ref`。
- 可与标准整数比较。
- 不接受 `.sint()`。

数值比较：
- `< <= > >=` 接受等宽 `Int/Ref/.sint()` 混合。
- 与标准整数比较时，signed 整数会构造成同宽 `Int` 后以 `.sint()` 视图参与比较；unsigned 整数按无符号视图参与比较。

```cpp
Int<8> u = 0xFF;
bool c0 = (u == uint8_t(0xFF)); // true
bool c1 = (u.sint() < Int<8>(1)); // true, -1 < 1
bool c2 = (u > Int<8>(1)); // true, 255 > 1
```

## 5.9 当前未提供的运算

当前 `fixint.hpp` 未提供 `/`、`%`。硬件描述中如需拼接或结构性重排，优先使用 `Cat`、`Repeat`、`at`/`pick` 明确表达位级结构。
