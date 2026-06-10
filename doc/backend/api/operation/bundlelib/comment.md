# bundlelib.comment - 为 bundle 添加或更新注释

## 概述

`bundlelib.comment` 命令用于为项目 bundle 库中的某个 bundle 添加或更新注释。若省略注释参数，则会清除该 bundle 的注释。

## 参数

- name — 要注释的 bundle 名称，字符串类型。
- comment — （可选）要设置的注释文本，字符串类型；若省略则清除注释。

## 结果

无（成功返回空结果）。

## 错误码

可能的错误包括：EOPBundCommentMissArg、EOPBundCommentNameNotFound。
