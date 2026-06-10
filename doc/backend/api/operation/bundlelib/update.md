# bundlelib.update - 更新项目中的 bundle 定义

## 概述

`bundlelib.update` 命令用于更新项目 bundle 库中已存在 bundle 的定义，需要提供新的 JSON 定义。函数会验证引用、检测循环引用等。

## 参数

- name — 要更新的 bundle 名称，字符串类型。
- definition — 新的 bundle JSON 定义，字符串类型。

## 结果

无（成功返回空结果）。

## 错误码

可能的错误包括：EOPBundUpdateMissArg、EOPBundUpdateNameNotFound、EOPBundUpdateImport、EOPBundUpdateDefinitionInvalid、EOPBundUpdateRefLoop。
