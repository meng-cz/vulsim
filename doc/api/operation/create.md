# create ：创建新的空白项目

## 概述

create命令用于在项目库中创建一个新的空白项目。

## 参数

0. name：项目名称，字符串类型，必需参数。

## 结果

无

## 列表结果

无

## 错误码

- EOPCreateAlreadyOpened — 已经有一个项目被打开，无法创建新项目。
- EOPCreateMissArg — 缺少必要的参数name。
- EOPCreateNameExists — 指定的项目名称已经存在于项目库中。
- EOPCreateFileFailed — 创建项目文件失败，可能是由于文件系统权限或空间不足等原因。

