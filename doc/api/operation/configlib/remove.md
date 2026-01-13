# configlib.remove - 从项目的配置库中移除配置项

## 概述

`configlib.remove` 命令用于从当前项目的配置库中移除一个已存在的配置项。若该配置项被引用或来自导入库则移除会失败。

## 参数

- name — 要移除的配置项名称，字符串类型。

## 结果

无（成功返回空结果）。

## 错误码

可能的错误包括：EOPConfRemoveMissArg、EOPConfRemoveNameNotFound、EOPConfRemoveImport、EOPConfRemoveReferenced。