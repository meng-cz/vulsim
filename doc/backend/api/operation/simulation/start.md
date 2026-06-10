# simulation.start - 启动仿真任务

## 概述

启动当前打开项目的仿真任务。可选择执行生成、编译和仿真三个阶段中的一或多个。

## 参数

- `runid` — 仿真任务的 ID（字符串），必需。
- `do_generation` — 是否执行生成步骤，可选，True/False。
- `do_compilation` — 是否执行编译步骤，可选，True/False。
- `do_simulation` — 是否执行仿真步骤，可选，True/False。
- `comp_release` — 编译后是否释放编译资源，可选，True/False。

## 结果

- 成功/失败的响应（错误码和错误信息通过统一的错误返回结构传递）。

## 错误码

- 视 SimulationManager 返回的错误码而定（例如步骤非法、库路径无效等）。
