# module.udisconn - 断开实例间的 Update 连接

## 概述

`module.udisconn` 命令用于断开模块中匹配条件的 update 顺序连接。

## 参数

- module — 要移除连接的模块名称，字符串类型。
- src_instance — （可选）前置实例名称（或用户 tick 块），字符串类型。
- dst_instance — （可选）后置实例名称（或用户 tick 块），字符串类型。

## 结果

无（成功返回空结果）。

## 错误码

可能的错误包括：EOPModCommonMissArg、EOPModCommonNotFound、EOPModCommonImport。