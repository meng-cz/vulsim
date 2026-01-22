# code.get - 生成模块辅助 C++ 代码或符号信息

## 概述

为项目中指定的模块生成辅助的 C++ 代码和函数签名信息。

## 参数

- `[0] module_name` — 要生成代码的模块名（字符串），必需。
- `[1] codeblock` — （可选）用户 tick 代码块名，若提供则为该代码块生成代码。
- `[2] service` — （可选）服务端口名，若提供则为该服务生成代码。
- `[3] instance` — （可选）子实例名称，若提供则用于定位子实例请求。
- `[4] request` — （可选）子实例请求名称，若提供则用于定位请求。

## 结果

- `signature` — 为生成代码提供的函数签名字符串。

## 列表结果

- `code_lines` — 代码行列表。

## 错误码

- `EOPCodeGetInvalidArg` — 参数无效或缺失。
- `EOPModCommonNotFound` — 指定模块未找到。
- `EOPModCommonImport` — 指定模块为导入模块，无法修改或生成。
- `EOPCodeGetNotFound` — 指定的代码块/服务/请求未找到（按上下文返回）。
