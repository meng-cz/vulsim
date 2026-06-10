# module.pipeout.add - 向模块添加管道输出端口

## 概述

`module.pipeout.add` 命令用于向指定模块添加一个管道输出端口。

## 参数

- name — 模块名称，字符串类型。
- port — 要添加的管道输出端口名称，字符串类型。
- type — 管道输出端口的数据类型，字符串类型。
- comment — （可选）管道输出端口的注释，字符串类型。

## 结果

无。

## 错误码

可能的错误包括：EOPModCommonNotFound、EOPModCommonImport、EOPModCommonMissArg、EOPModPipePortAddNameInvalid、EOPModPipePortAddNameDup

