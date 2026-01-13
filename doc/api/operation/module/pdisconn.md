# module.pdisconn - 断开实例与 pipe 的连接

## 概述

`module.pdisconn` 命令用于断开模块中实例与 pipe 实例（或顶层）之间的 pipe 连接，可按实例、端口或 pipe 名称筛选。

## 参数

- module — 要移除 pipe 连接的模块名称，字符串类型。
- instance — （可选）源实例名称，字符串类型。
- port — （可选）源实例的 pipe 端口名称，字符串类型。
- pipe — （可选）目标 pipe 实例名称（或 `__top__`），字符串类型。
- pipe_port — （可选）当 `pipe` 为 `__top__` 时，指定要断开的顶层 pipe 端口名称，字符串类型。

## 结果

无（成功返回空结果）。

## 错误码

可能的错误包括：EOPModCommonMissArg、EOPModCommonNotFound、EOPModCommonImport。