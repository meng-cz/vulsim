# list - 列举项目库中所有的项目及其状态

## 概述

list命令用于列举项目库中所有的项目及其状态信息，包括项目名称、路径、顶层模块名称以及修改状态等。

## 参数

无

## 结果

- rootpath — 当前项目库的根路径，字符串类型。

## 列表结果

- project_names — 项目名称列表，字符串数组类型。
- project_paths — 项目路径列表，字符串数组类型，与project_names一一对应。
- project_modtimes — 项目修改时间列表，字符串数组类型，与project_names一一对应。

## 错误码

无
