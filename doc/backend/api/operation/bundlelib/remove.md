# bundlelib.remove - 从项目的 bundle 库中移除 bundle

## 概述

`bundlelib.remove` 命令用于从当前项目的 bundle 库中移除一个已存在的 bundle。若该 bundle 仍被其他 bundle 引用或为导入的非默认 tag，移除会失败。

## 参数

- name — 要移除的 bundle 名称，字符串类型。

## 结果

无（成功时可返回消息码，具体以调用返回为准）。

## 错误码

可能的错误包括：EOPBundRemoveMissArg、EOPBundRemoveNameNotFound、EOPBundRemoveReferenced。
