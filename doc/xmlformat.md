# VulSim XML 文件格式文档

## Vul ConfigLib XML 文件格式

ConfigLibVersion: 1.0

```
configlib
|-- version : 版本号
|-- [configitem]
    |-- name : 配置项名称
    |-- value : 配置项值（或表达式）
    |-- (comment) : 配置项注释（可选）
    |-- (group) : 配置项分组（可选）
```

## Vul BundleLib XML 文件格式

BundleLibVersion: 1.0

```
bundlelib
|-- version : 版本号
|-- [bundle]
    |-- name : Bundle 名称
    |-- (comment) : Bundle 描述
    |-- (isenum) : 是否为枚举类型（可选，默认为 false）
    |-- (isalias) : 是否为别名类型（可选，默认为 false）
    |-- [member]
        |-- name : 成员名称
        |-- (type) : 成员类型（可选，非枚举类型时为必须）
        |-- (uintlen) : 成员类型位宽（可选，针对整数类型）
        |-- (value) : 成员值（可选，枚举类型时为必须）
        |-- (comment) : 成员描述（可选）
        |-- [dims] : 成员数组维度（可选，重复时代表多维）
```


