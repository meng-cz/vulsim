# configlib.comment - 为配置项添加或更新注释

## 概述

`configlib.comment` 命令用于为项目的配置项添加或更新注释。若省略注释参数则会清除该配置项的注释。仅能对非导入项进行注释修改。

## 参数

- name — 要注释的配置项名称，字符串类型。
- comment — （可选）注释文本，字符串类型；若省略则清除注释。

## 结果

无（成功返回空结果）。

## 错误码

可能的错误包括：EOPConfCommentMissArg、EOPConfCommentNameNotFound、EOPConfCommentImport。