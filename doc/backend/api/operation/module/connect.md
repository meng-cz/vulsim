# module.connect - 在模块实例间建立请求-服务连接

## 概述

`module.connect` 命令用于在同一模块的两个实例（或顶层接口 `__top__`）之间建立请求-服务连接。

## 参数

- module — 要添加连接的模块名称，字符串类型。
- src_instance — 请求方实例名称（或 `__top__`），字符串类型。
- src_port — 请求端口名称，字符串类型。
- dst_instance — 服务方实例名称（或 `__top__`），字符串类型。
- dst_port — 服务端口名称，字符串类型。

## 结果

无（成功返回空结果）。

## 错误码

可能的错误包括：EOPModConnMissArg、EOPModCommonNotFound、EOPModCommonImport、EOPModConnSrcInstNotFound、EOPModConnSrcPortNotFound、EOPModConnDstInstNotFound、EOPModConnDstPortNotFound、EOPModConnMismatch、EOPModConnAlreadyExists、EOPModConnMultiConn。