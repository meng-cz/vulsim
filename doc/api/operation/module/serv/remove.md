# module.serv.remove - 从模块中移除服务端口

## 概述

`module.serv.remove` 用于从模块中移除服务端口。可选择使用 `force` 强制移除已连接的端口。
## 参数

- name — 包含该端口的模块名称，字符串类型。
- port — 要移除的端口名称，字符串类型。
- force — （可选）是否强制移除已存在连接的端口，布尔/字符串类型。

## 结果

无（成功返回空结果）。

## 错误码

可能的错误包括：EOPModCommonMissArg、EOPModCommonNotFound、EOPModCommonImport、EOPModReqRemoveNotFound、EOPModReqRemoveConnected。