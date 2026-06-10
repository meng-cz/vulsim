# VulStorage 序列化 JSON 格式

用于 `module.storage.*` 操作的 VulStorage 对象的 JSON 序列化格式说明。

```json
{
  "name": "...",
  "comment": "...",
  "type": "...",
  "value": "...",
  "uint_length": "...",
  "dims": ["...", "...", ...]
}
```

一个合法的 VulStorage 定义必须符合以下条件：
1. 类型定义为以下之一：
    - 命名类型： type 为基础类型或其他已定义的 Bundle 名称， uint_length 为空字符串， dims 可以为空或非空。
    - 无符号整数类型： type 为空， uint_length 为表达式字符串， dims 可以为空或非空。
2. 如果 value 字段非空，则类型不为其他自定义的 Bundle 名称。
