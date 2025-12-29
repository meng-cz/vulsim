# VulSim XML 文件格式文档

## Vul 项目目录结构

```
project_root/
|-- project_name.xml            # Vul 项目文件
|-- configlib.xml               # Vul 配置库文件
|-- bundlelib.xml               # Vul Bundle 库文件
|-- modules/                    # Vul 模块文件目录
    |-- module1.xml             # Vul 模块文件
    |-- module2.xml             # Vul 模块文件
    |-- ...
```

```
import_root/
|-- modulename.xml            # 导入模块文件
|-- configlib.xml             # 导入配置库文件
|-- bundlelib.xml             # 导入 Bundle 库文件
```

## Vul Project XML 文件格式

ProjectVersion: 1.0

```
project
|-- version : 版本号
|-- [import]
    |-- abspath : 导入项目绝对路径，为空则使用VULLIB环境变量
    |-- name : 导入模块名称
    |-- [configoverride]
        |-- name : 配置项名称
        |-- value : 配置项值（或表达式）
|-- topmodule : 顶层模块名称
```

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

## Vul Module XML 文件格式

ModuleVersion: 1.0

```
modulebase
|-- version : 版本号
|-- name : 模块名称
|-- (comment) : 模块描述（可选）
|-- [localconf]
    |-- name : 本地配置项名称
    |-- value : 本地配置项值（或表达式）
    |-- (comment) : 本地配置项注释（可选）
|-- [localbundle]
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
|-- [request/service]
    |-- name : 请求名称
    |-- (comment) : 请求描述（可选）
    |-- (array_size) : 请求数组大小（可选，非数组请求时为空）
    |-- (handshake) : 是否包含握手（可选，默认为 false）
    |-- [arg]
        |-- name : 参数名称
        |-- type : 参数类型
        |-- (comment) : 参数描述（可选）
    |-- [ret]
        |-- name : 返回值名称
        |-- type : 返回值类型
        |-- (comment) : 返回值描述（可选）
|-- [pipein/pipeout]
    |-- name : 端口名称
    |-- (comment) : 端口描述（可选）
    |-- type : 管道端口数据类型
```
```
module
|-- version : 版本号
|-- name : 模块名称
|-- (comment) : 模块描述（可选）
|-- [localconf]
    |-- name : 本地配置项名称
    |-- value : 本地配置项值（或表达式）
    |-- (comment) : 本地配置项注释（可选）
|-- [request/service]
    |-- name : 请求名称
    |-- (comment) : 请求描述（可选）
    |-- (array_size) : 请求数组大小（可选，非数组请求时为空）
    |-- (handshake) : 是否包含握手（可选，默认为 false）
    |-- [arg]
        |-- name : 参数名称
        |-- type : 参数类型
        |-- (comment) : 参数描述（可选）
    |-- [ret]
        |-- name : 返回值名称
        |-- type : 返回值类型
        |-- (comment) : 返回值描述（可选）
|-- [pipein/pipeout]
    |-- name : 端口名称
    |-- (comment) : 端口描述（可选）
    |-- type : 管道端口数据类型
|-- [instance]
    |-- name : 模块实例名称
    |-- (comment) : 模块实例描述（可选）
    |-- type : 模块实例类型
    |-- [config]
        |-- name : 配置项名称
        |-- value : 配置项值（或表达式）
|-- [pipe]
    |-- name : 管道名称
    |-- (comment) : 管道描述（可选）
    |-- type : 管道数据类型
    |-- (inputsize) : 管道输入缓冲区大小（可选，默认1）
    |-- (outputsize) : 管道输出缓冲区大小（可选，默认1）
    |-- (buffersize) : 管道总缓冲区大小（可选，默认0）
    |-- (latency) : 管道延迟（可选，默认1）
    |-- (handshake) : 是否启用握手（可选，默认false）
    |-- (valid) : 是否启用数据有效标志（可选，默认false）
|-- [reqconn/pipeconn]
    |-- frominstance : 源实例名称
    |-- fromport : 源端口名称
    |-- toinstance : 目标实例名称
    |-- toport : 目标端口名称
|-- [stallconn/seqconn]
    |-- frominstance : 源实例名称
    |-- toinstance : 目标实例名称
|-- (userheadercode)： 用户自定义头文件代码行Base64（可选）
|-- [codeblock]
    |-- instance : 模块实例名称
    |-- blockname : 代码块名称（Req，Serv端口名称）
    |-- code : 模块实例时钟周期处理代码行Base64（可选）
```

