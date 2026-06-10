# module.req.add - 在模块中添加请求端口

## 概述

`module.req.add` 用于向模块添加新的请求端口。

## 参数

- name — 要操作的模块名称，字符串类型。
- port — 要添加的端口名称，字符串类型。
- definition — 端口的 JSON 定义，字符串类型。

## 结果

无（成功返回空结果）。

## 错误码

可能的错误包括：EOPModCommonMissArg、EOPModCommonNotFound、EOPModCommonImport、EOPModReqAddReqNameDup。