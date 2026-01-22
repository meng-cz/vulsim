# 前后端通讯 API

## TCP Socket

VulSim 前后端通过两个 TCP Socket 进行通讯。后端监听两个指定端口，默认17995和17996，前端连接到该端口以发送和接收数据.

1. 主控 Socket （17995）: 用于普通操作请求和响应。端口号可配置。
2. 日志 Socket （17996）: 用于发送日志。端口号固定为主控 Socket 端口号加一。

主控 TCP Socket 执行半双工通讯。每一次通讯都包含一个请求和一个响应。前端发送请求后，必须等待后端的响应才能发送下一个请求。

日志 TCP Socket 执行异步单向通讯。后端会在有日志消息时发送数据，前端仅需连接 Socket 并读取，无需发送任何请求。

## 数据格式

所有 请求和响应 结构相同， 包括日志消息：

```
| Magic Number | Payload Length |      Payload      |
|:------------:|:--------------:|:-----------------:|
|   4 bytes    |    4 bytes     |  Variable Length  |
```

1. **Magic Number**: 固定为 `0x37549260U`，用于标识数据包的开始。
2. **Payload Length**: 一个 4 字节的无符号整数，表示后续 Payload 的字节长度。
3. **Payload**: 变长字节数组，包含实际的 JSON 数据。无压缩。

## 请求数据格式

Payload 使用 JSON 格式编码。请求的 JSON 结构如下：

```json
{
  "name": "operation.name",
  "args": [
    /* operation specific arguments */
    { "index": 0, "name": "modulename", "value": "test" },
    { "index": 1, "value": "42" },
    { "name": "flag", "value": true }
  ]
}
```

- **name**: 字符串，指定要执行的操作名称。
- **args**: 数组，包含操作的参数。每个参数是一个对象，包含以下字段：
  - **index** (可选): 参数的索引位置，从 0 开始。
  - **name** (可选): 参数的名称。
  - **value**: 参数的值，可以是字符串或布尔值，或是以字符串保存的另一个 JSON

参数有 index 或 name 两种索引方式。API 中每个操作同时定义了每一个参数的索引名和索引编号，请求JSON中必须至少使用其中一种方式来标识参数：
1. 仅使用 index。name 字段省略或设置为空字符串。
2. 仅使用 name。index 字段省略或设置为 -1。
3. 同时使用 index 和 name，但必须保证 index 和 name 对应同一个参数。

参数的值可以为另一个 JSON 子对象。此时内部JSON对象需要先序列化为字符串，然后作为外层 JSON 对象的字符串值传递，其中的双引号需要进行转义。

## 响应数据格式

Payload 使用 JSON 格式编码。响应的 JSON 结构如下：

```json
{
  "code": 0,
  "msg": "",
  "results" : {
    "result1" : "value1",
    "result2" : "42"
  },
  "list_results" : {
    "list1" : [ "item1", "item2", "item3" ],
    "list2" : [ "10", "20", "30" ]
  }
}
```

- **code**: 整数，表示操作的返回码。0 表示成功，非零表示失败。
- **msg**: 字符串，包含操作的返回消息，通常用于错误描述。
- **results**: 对象，包含操作的单值返回结果。每个字段表示一个返回值，字段名为结果名称，字段值为结果值（字符串形式）。
- **list_results**: 对象，包含操作的多值返回结果。每个字段表示一个返回值列表，字段名为结果名称，字段值为字符串数组形式的结果列表。

同样，结果字符串的值可以为另一个 JSON 子对象。此时内部JSON对象需要先序列化为字符串，然后作为外层 JSON 对象的字符串值传递，其中的双引号需要进行转义。

所有的操作都是事务化的，即要么完全成功，要么完全失败，不会出现部分成功的情况。当操作的返回码非零时，对项目数据不应产生任何影响。

## 日志数据格式

Payload 使用 JSON 格式编码。日志消息的 JSON 结构如下：

```json
{
  "level": "Info", // 日志级别， Debug Info Warning Error Critical
  "category": "Simulation", // 日志类别， General Generation Compilation Simulation 等
  "message": "This is a log message.",
  "timestamp": 1234567890 // Unix 时间戳（微秒）
}
```

## 操作子 JSON

部分操作使用嵌套序列化的 JSON 字符串作为参数或结果的一部分。这些子 JSON 的结构在[json目录](./json/)中有详细描述。

| Data Structure Name | Description                     | Comment                      |
|:-------------------:|:-------------------------------|:----------------------------|
| VulBundleItem      | Bundle 定义                     | 用于 bundlelib.* 和 module.bundle.* 操作 |
| VulInstance       | 模块实例定义                    | 用于 module.instance.* 操作        |
| VulStorage        | 存储实例定义                    | 用于 module.storage.* 操作        |
| VulPipe           | 管道实例定义                    | 用于 module.pipe.* 操作        |
| VulReqServ       | 请求/服务端口定义                | 用于 module.req.* 和 module.serv.* 操作 |
| VulModuleOverview | 模块概览信息                    | 用于 module.info 操作，**待补充可视化相关数据**  |

## 操作的可撤销属性

每一个操作有两个固有属性用于描述其撤销/重做行为：

1. **Modified** - 该操作是否会修改项目数据
2. **Undoable** - 该操作对项目数据的修改是否可撤销

两个属性的组合对撤销/重做行为的定义如下：

| Modified | Undoable | 行为描述                         |
|:--------:|:--------:|:-------------------------------|
|   N   |   any | 不修改项目数据，无需撤销/重做，不修改撤销/重做栈       |
|   Y   |   N   | 修改项目数据，但不可撤销/重做，操作成功时清空撤销/重做栈 |
|   Y   |   Y   | 修改项目数据且可撤销/重做，操作成功时将该操作推入撤销栈 |

撤销/重做行为仅在操作被成功执行时有效（即响应 code 为 0 时）。

## 全部操作列表

| Operation Name            | Modified | Undoable | Information                   |
|:-------------------------:|:--------:|:--------:|:-----------------------------:|
| list | N | N | 列出项目库中的所有可用项目 |
| create | Y | N | 创建一个新的空白项目并打开 |
| info | N | N | 获取当前打开的项目的信息 |
| load | Y | N | 加载一个已存在的项目并打开 |
| save | Y | N | 保存当前打开的项目 |
| cancel | Y | N | 取消当前打开的项目的修改并强制关闭 |
| history | N | N | 获取当前项目的修改历史信息 |
| undo | N | N | 撤销上一个可撤销的操作 |
| redo | N | N | 重做上一个被撤销的操作 |
| bundlelib.list | N | N | 列出项目的全局 bundle 库列表 |
| bundlelib.listref | N | N | 递归列出 bundle 的引用关系 |
| bundlelib.add | Y | Y | 向项目的 bundle 库添加新的 bundle |
| bundlelib.update | Y | Y | 更新项目中的 bundle 定义 |
| bundlelib.remove | Y | Y | 从项目的 bundle 库中移除一个 bundle |
| bundlelib.rename | Y | Y | 重命名项目中的一个 bundle |
| bundlelib.comment | Y | Y | 更新项目中 bundle 的注释 |
| configlib.list | N | N | 列出项目的全局配置库列表 |
| configlib.listref | N | N | 递归列出配置的引用关系 |
| configlib.add | Y | Y | 向项目的配置库添加新的配置 |
| configlib.update | Y | Y | 更新项目中的配置定义 |
| configlib.remove | Y | Y | 从项目的配置库中移除一个配置 |
| configlib.rename | Y | Y | 重命名项目中的一个配置 |
| configlib.comment | Y | Y | 更新项目中配置的注释 |
| module.list | N | N | 列出项目的模块库列表 |
| module.info | N | N | 获取模块的详细信息 |
| module.add | Y | Y | 向项目的模块库添加新的模块 |
| module.remove | Y | Y | 从项目的模块库中移除一个模块 | |
| module.rename | Y | Y | 重命名项目中的一个模块 |
| module.connect | Y | Y | 连接两个模块内部的请求-服务端口 |
| module.disconn | Y | Y | 断开两个模块内部的请求-服务端口连接 |
| module.pconn | Y | Y | 连接两个模块内部的管道输入输出端口 |
| module.pdisconn | Y | Y | 断开两个模块内部的管道输入输出端口连接 |
| module.sconn | Y | Y | 建立模块内部的阻塞传递连接 |
| module.sdisconn | Y | Y | 断开模块内部的阻塞传递连接 |
| module.uconn | Y | Y | 建立模块内部的更新次序连接 |
| module.udisconn | Y | Y | 断开模块内部的更新次序连接 |
| module.bundle.get | N | N | 获取模块内部的本地 bundle 定义 |
| module.bundle.set | Y | Y | 设置模块内部的本地 bundle 定义 |
| module.config.set | Y | Y | 设置模块内部的本地配置定义 |
| module.req.get | N | N | 获取模块内部的请求端口定义 |
| module.req.add | Y | Y | 添加模块内部的请求端口定义 |
| module.req.remove | Y | Y | 移除模块内部的请求端口定义 |
| module.req.rename | Y | Y | 重命名模块内部的请求端口定义 |
| module.req.update | Y | Y | 更新模块内部的请求端口定义 |
| module.serv.get | N | N | 获取模块内部的服务端口定义 |
| module.serv.add | Y | Y | 添加模块内部的服务端口定义 |
| module.serv.remove | Y | Y | 移除模块内部的服务端口定义 |
| module.serv.rename | Y | Y | 重命名模块内部的服务端口定义 |
| module.serv.update | Y | Y | 更新模块内部的服务端口定义 |
| module.pipein.add | Y | Y | 添加模块内部的管道输入端口定义 |
| module.pipein.remove | Y | Y | 移除模块内部的管道输入端口定义 |
| module.pipein.rename | Y | Y | 重命名模块内部的管道输入端口定义 |
| module.pipein.update | Y | Y | 更新模块内部的管道输入端口定义 |
| module.pipeout.add | Y | Y | 添加模块内部的管道输出端口定义 |
| module.pipeout.remove | Y | Y | | 移除模块内部的管道输出端口定义 |
| module.pipeout.rename | Y | Y | 重命名模块内部的管道输出端口定义 |
| module.pipeout.update | Y | Y | 更新模块内部的管道输出端口定义 |
| module.instance.get | N | N | 获取模块内部的子实例的信息 |
| module.instance.set | Y | Y | 设置模块内部的子实例的信息 |
| module.pipe.get | N | N | 获取模块内部的管道子实例定义 |
| module.pipe.set | Y | Y | 设置模块内部的管道子实例定义 |
| module.storage.get | N | N | 获取模块内部存储实例 |
| module.storage.set | Y | Y | 设置模块内部存储实例 |
| simulation.start | Y | N | 启动当前项目的仿真任务 |
| simulation.list | N | N | 列出当前项目的所有仿真任务历史 |
| simulation.cancel | Y | N | 取消当前项目的正在运行的仿真任务 |
| simulation.state | N | N | 获取当前项目的正在运行的仿真任务状态 |
| code.get | N | N | 获取模块中指定代码块或接口的代码行及其符号表信息 |
| code.update | Y | Y | 更新或删除模块中指定代码块或接口的代码行 |
