# 5. UInt 任意宽度无符号整数类型

## 5.1 定义

VulCPP 提供了一个名为 `UInt` 的任意常量宽度无符号整数类型（宽度不得超过uint32_t的最大值），可以通过以下方式定义：

```cpp
UInt<N> varname; // 定义一个宽度为 N 位的无符号整数变量
```

或是在结构体或别名中使用：

```cpp
STRUCT(MyStruct) {
    UInt<N> field; // 定义一个宽度为 N 位的无符号整数字段
};
ALIAS(MyAlias, UInt<N>); // 定义一个宽度为 N 位的无符号整数别名
```

或是在事务接口参数中使用：

```cpp
REQUEST_PORT(MyRequest, void, ARG(UInt<N>) arg, RESP(UInt<M>) resp);
```

## 5.2 构造与赋值

`UInt` 类型支持从整数常量和其他 `UInt` 类型进行构造和赋值。例如：

```cpp
UInt<16> a = 12345; // 从整数常量构造
UInt<16> b = a; // 从另一个 UInt 构造
UInt<32> c = a; // 从较小宽度的 UInt 构造，自动扩展
UInt<8> d = a; // 从较大宽度的 UInt 构造，自动截断
```

## 5.3 算术运算

`UInt` 类型支持多种运算操作，包括算术运算、位运算和比较运算。例如：

```cpp
UInt<16> a = 12345;
UInt<16> b = 67890;
UInt<16> c = a + b; // 加法
UInt<16> d = a - b; // 减法
UInt<16> e = a * b; // 乘法
UInt<16> g = a & b; // 按位与
UInt<16> h = a | b; // 按位或
UInt<16> i = a ^ b; // 按位异或
UInt<16> j = ~a; // 按位取反
UInt<16> k = a << 2; // 左移
UInt<16> l = a >> 2; // 右移
bool eq = (a == b); // 相等比较
bool neq = (a != b); // 不相等比较
bool lt = (a < b); // 小于比较
bool gt = (a > b); // 大于比较
bool le = (a <= b); // 小于等于比较
bool ge = (a >= b); // 大于等于比较
```

**不支持除法，不支持除法，不支持除法，除法不能被综合成可用的组合逻辑，因此不应当使用。请使用截取运算做除2幂或模2幂**

## 5.4 截取与范围赋值

`UInt` 类型提供了截取操作，可以通过以下方式使用：

```cpp
UInt<32> a = 0x12345678;
UInt<16> b = a(31, 16); // 截取高16位，结果为 0x1234
UInt<16> c = a(15, 0); // 截取低16位，结果为 0x5678
a(31, 16) = 0xABCD; // 将高16位设置为 0xABCD，a 变为 0xABCD5678
bool bit = a(3); // 获取第3位的值
a(3) = 1; // 将第3位设置为1
```
