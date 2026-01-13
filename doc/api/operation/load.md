# load - 从磁盘加载项目到当前会话

## 概述

load 命令用于从磁盘加载一个项目到当前会话，包含模块、配置和 bundle 库。若当前已有项目打开则返回错误。该操作会在执行结果中返回加载过程的日志信息。

## 参数

- name — 要加载的项目名称，字符串类型。
- import_paths — （可选）冒号分隔的导入模块搜索路径字符串。

## 结果

- logs — 加载过程产生的日志列表，字符串数组类型。

## 错误码

可能的错误码包括但不限于：EOPLoadNotClosed、EOPLoadMissArg、EOPLoadInvalidPath、EOPLoadImportNotFound、EOPLoadImportInvalidPath、EOPLoadBundleConflict 等。
