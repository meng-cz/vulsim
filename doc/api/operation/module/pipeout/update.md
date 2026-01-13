# module.pipeout.update - 更新模块的管道输出端口

## 概述

`module.pipeout.update` 命令用于更新指定模块的管道输出端口。

## 参数

- name — 模块名称，字符串类型。
- port — 要更新的管道输入端口的名称，字符串类型。
- type — （可选）管道输出端口的数据类型，字符串类型。
- comment — （可选）管道输出端口的注释，字符串类型。
- force — （可选）是否强制更新已连接的管道输出端口（true/false），默认 false。

## 结果

无。

## 错误码

可能的错误包括：EOPModCommonNotFound、EOPModCommonImport、EOPModCommonMissArg
