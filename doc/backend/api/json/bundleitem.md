# VulBundleItem 序列化 JSON 格式

用于 `bundlelib.*` 和 `module.bundle.*` 操作的 VulBundleItem 对象的 JSON 序列化格式说明。

```json
{
  "members": [
   {
   "name": "...",
   "comment": "...",
   "type": "...",
   "value": "...",
   "uint_length": "...",
   "dims": ["...", "...", ...]
  }, ...
  ]
  "enum_members": [
  {
  "name": "...",
  "comment": "...",
  "value": "..."
  }, ...
  ],
  "is_alias": true/false
}
```

- members: 包含 bundle 成员的数组，每个成员包含以下字段：
  - name: 成员名称，字符串类型。
  - comment: 成员注释，字符串类型。
  - type: 成员数据类型，字符串类型（可以是基础类型或其他自定义的 Bundle 名）。
  - value: 成员的默认值，字符串类型。
  - uint_length: 对于无符号整数类型，指定其位长度，整数类型。
  - dims: 成员的维度数组，表示多维数组的各维大小，整数数组。
- enum_members: （可选）如果该 bundle 是枚举类型，则包含枚举成员的数组，每个枚举成员包含以下字段：
  - name: 枚举成员名称，字符串类型。
  - comment: 枚举成员注释，字符串类型。
  - value: 枚举成员的值，字符串类型。
- is_alias: 布尔值，指示该 bundle 是否为别名类型。

一个合法的 VulBundleItem JSON 定义必须符合以下条件之一：
1. 别名： is_alias 为 true， members 仅包含一项， enum_members 为空数组。
2. 枚举： is_alias 为 false， members 为空数组， enum_members 非空。
3. 结构体： is_alias 为 false， members 非空， enum_members 为空数组。

一个合法的 members 字段定义必须符合以下条件：
1. 类型定义为以下之一：
    - 命名类型： type 为基础类型或其他已定义的 Bundle 名称， uint_length 为空字符串， dims 可以为空或非空。
    - 无符号整数类型： type 为空， uint_length 为表达式字符串， dims 可以为空或非空。
2. 如果 value 字段非空，则类型不为其他自定义的 Bundle 名称。


