# module.pconn - 将实例的 pipe 端口连接到 pipe 实例或顶层

## 概述

`module.pconn` 命令用于将模块中某个实例的 pipe 端口连接到 pipe 实例或顶层的 pipe 端口。

## 参数

- module — 要添加 pipe 连接的模块名称，字符串类型。
- instance — 源实例名称，字符串类型。
- port — 源实例的 pipe 端口名称，字符串类型。
- pipe — 目标 pipe 实例名称（或 `__top__`），字符串类型。
- pipe_port — （可选）当 `pipe` 为 `__top__` 时，指定顶层 pipe 端口名称，字符串类型。

## 结果

无（成功返回空结果）。

## 错误码

可能的错误包括：EOPModPConnMissArg、EOPModCommonNotFound、EOPModCommonImport、EOPModPConnSrcInstNotFound、EOPModPConnSrcPortNotFound、EOPModPConnDstPipeNotFound、EOPModPConnDstPortNotFound、EOPModPConnMismatch、EOPModPConnMultiConn、EOPModPConnAlreadyExists。