# module.uconn - 添加/更新实例的 Update 连接

## 概述

`module.uconn` 命令用于在模块内添加或更新实例间的 update 顺序连接，保证实例更新序列不会引入循环依赖。

## 参数

- module — 要操作的模块名称，字符串类型。
- src_instance — 前置（former）实例名称（或用户 tick 块），字符串类型。
- dst_instance — 后置（latter）实例名称（或用户 tick 块），字符串类型。

## 结果

无（成功返回空结果）。

## 错误码

可能的错误包括：EOPModUConnMissArg、EOPModCommonNotFound、EOPModCommonImport、EOPModUConnSrcInstNotFound、EOPModUConnDstInstNotFound、EOPModUConnSelfLoop、EOPModUConnLoop。