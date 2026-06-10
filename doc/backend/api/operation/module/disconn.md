# module.disconn - 断开模块实例间的连接

## 概述

`module.disconn` 命令用于断开匹配指定条件的请求-服务连接（可根据实例或端口选择性断开）。

## 参数

- module — 要移除连接的模块名称，字符串类型。
- src_instance — （可选）请求方实例名称（或 `__top__`），字符串类型。
- src_port — （可选）请求端口名称，字符串类型。
- dst_instance — （可选）服务方实例名称（或 `__top__`），字符串类型。
- dst_port — （可选）服务端口名称，字符串类型。

## 结果

无（成功返回空结果）。

## 错误码

可能的错误包括：EOPModCommonMissArg、EOPModCommonNotFound、EOPModCommonImport。