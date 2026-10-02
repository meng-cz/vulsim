# 泺源微架构建模与模拟平台

泺源（LuoYuan）微架构建模与模拟平台，是一套用于处理器微架构建模、探索与调试的软件平台。基于新型高层芯片描述语言：可视化微架构语言（Visualized μarch Language，VUL）构建。

基于VUL，泺源平台为新型微架构设计者提供了：

1. 统一化的、结构化的通用微架构行为级设计描述
2. 周期精确的并行模拟，周期行为对齐到最终的RTL实现
3. 内置调试工具和性能统计工具
4. 自动化生成 RTL 代码

本项目为 Vul 的核心命令行工具链。相关项目目录如下：

- [前端GUI（Demo）](https://github.com/meng-cz/VulSimGUI)
- [案例项目：RISC-V TPU](https://github.com/meng-cz/oootpu-sim)：基于 VUL 原生设计实现的使用 RISC-V 指令扩展的可编程 TPU 计算核心
- [案例项目：TCA-ADK](https://gitee.com/wei-yanxuan/TCA-ADK)：张量计算适配评价框架，为后续张量加速器微架构探索与性能模拟提供算子分析和架构需求输入
- [案例项目：3DCacheSim](https://gitee.com/x1jia/large-cache-sched-sim)：面向 Chiplet 多核处理器与 3D 堆叠缓存架构的系统级周期精确模拟框架

## User Guide

- [1. VulCPP 简介（Get Started）](./doc/userguide/1-intro.md)
- [2. VulCPP header 常量与数据结构定义](./doc/userguide/2-header.md)
- [3. VulCPP 硬件模块定义](./doc/userguide/3-module.md)
- [4. VulCPP Main 模块（Test Harness）定义](./doc/userguide/4-Main.md)
- [5. VulCPP 无符号任意宽度整数类型 Int](./doc/userguide/5-uint.md)
- [6. VulCPP 通用块内存组件 BRAM/ROM](./doc/userguide/6-bram.md)
- [7. VulCPP 通用队列组件 QUEUE](./doc/userguide/7-queue.md)
- [8. VulCPP 阵列化子实例](./doc/userguide/8-arrayization.md)
- [9. VulCPP 波形追踪与调试](./doc/userguide/9-debugging.md)

介绍PPT：[Vul（泺源）项目介绍与工作整理](./doc/LuoYuan20260708-FULL.pdf)

## 依赖

基于 CMake 构建系统，支持 Linux 平台，Windows 下仍需测试。依赖 CMake 3.20 及以上版本，以及 LLVM/Clang 18 的开发包。

安装环境（以 Ubuntu / apt 工具为例，如果发行版仓库已经提供 LLVM 18）：

```bash
sudo apt update
sudo apt install git gcc g++ make cmake \
    llvm-18-dev clang-18 libclang-18-dev
```

如果当前 Ubuntu 版本默认仓库不提供 LLVM 18，可以参考 LLVM 官方 apt 源安装流程：

```bash
sudo apt update
sudo apt install wget gnupg software-properties-common
wget https://apt.llvm.org/llvm.sh
chmod +x llvm.sh
sudo ./llvm.sh 18
sudo apt install llvm-18-dev clang-18 libclang-18-dev
```

安装后可检查版本：

```bash
llvm-config-18 --version
clang-18 --version
```

## 构建

初始化 git 目录与 submodule：

```bash
git clone https://github.com/meng-cz/vulsim.git
cd vulsim
git submodule update --init --recursive
```

构建命令如下：

```bash
mkdir build
cd build
cmake ..
make -j8
```

构建完成后会在 `build` 目录下生成如下文件：
- `vulsimgen`：VulCPP 仿真代码生成工具，用于将 VulCPP 项目转换为可执行的仿真代码
- `vulrtlgen`：VulCPP RTL 代码生成工具，用于将 VulCPP 项目转换为可综合的 RTL 代码
- `vullib`：VulCPP 运行库目录，供生成的仿真/RTL代码使用；默认从当前目录下的 `./vullib` 读取，也可通过 `-l/--lib` 指定
- `userguide`：用户手册文档目录
- `example`：示例代码目录

运行示例：

```bash
# 生成仿真代码
./vulsimgen -m example/prodcon/Main.cpp
# -m/--main: 必需，指定 Main 测试文件
# -t/--top: 可选，覆盖 Main 中 TOP(...) 声明的顶层模块路径
# -p/--project: 可选，覆盖 Main 中 PROJECT(...) 声明的项目目录
# -l/--lib: 运行库目录（默认 ./vullib）
# -o/--out: 仿真代码输出目录（默认 ./simout）
# -f/--force: 输出目录非空时跳过确认并清空；未指定时会询问
# 可用 -T/--tracefile 或 --trace 配置追踪，用 -B/--breakfile 或 --break 配置断点

# 编译生成的仿真代码 (build.sh 默认使用 g++)
cd simout
source build.sh
# 运行仿真（可执行文件名取 Main 文件的基本名）
./Main

# 一键按默认 build.sh 参数编译并运行，也可直接传递程序参数
./run.sh
```

```bash
# 生成 RTL 代码
./vulrtlgen -t example/prodcon/TopModule.hpp
# -t/--top: 指定顶层模块文件；也可用 -m/--main 指定带 TOP(...) 的 TestMain 文件
# -p/--project: 项目目录（默认顶层模块文件所在目录）
# -l/--lib: 运行库目录（默认 ./vullib）
# -o/--out: RTL 输出目录（默认 ./rtlout）
# -j: 并行模块生成数（默认 1）
# -r/--release: 不保留中间 C++、调试文件和 TestMain C++ 文件
# --circt: 通过 CIRCT HW/Comb 后端而不是原生 RTL Emitter 生成质量更好的 RTL
# -f/--force: 输出目录非空时跳过确认并清空；未指定时会询问
ls -lah rtlout

# -m/--main 非 release 模式会生成 Verilator 驱动和一键构建/运行脚本
./vulrtlgen -m example/prodcon/Main.cpp
cd rtlout
./run.sh
# 应当输出与运行 vulsimgen 生成的仿真代码相同的结果
```

`vulrtlgen` 至少需要 `-t/--top` 或 `-m/--main` 之一；使用 `-m` 时，TestMain 中的 `TOP(...)` 可提供顶层模块路径。`-r/--release` 会省略中间文件，`--circt` 选择 CIRCT HW/Comb 后端。
使用 `-m` 且未指定 `-r/--release` 时，输出目录会包含 Verilator C++ 驱动和 `run.sh`。运行脚本需要已安装 Verilator 与 C++ 编译器，并会构建后立即启动仿真。可从任意当前目录调用该脚本，额外参数会传递给仿真程序。Verilator 构建日志和仿真日志分别保存在 `verilator_build.log` 与 `simulation.log`；失败时脚本会显示对应日志并返回失败状态。


## 开发文档：

[vullib模块文档](./doc/vullib) 

前期工作链接：
1. [nullrvsim](https://github.com/meng-cz/nullrvsim): 验证VUL模型生成的模拟器框架的正确性和并行效率
2. [FASE](https://github.com/meng-cz/fase-rv64)：对RTL实现的处理器实现系统调用模拟（SE模式），为VUL模型上的SE模式做准备

## “泺源”简介

“泺源”之名，取自济南趵突泉——古泺水的发源地，它是泉城的文化根脉，承载着“源头活水”的含义。

“泺”作为自然源头，代表着创新的起点，就像趵突泉为古泺水注入不竭动能，平台也为处理器研发提供源头性的性能模拟支撑；“源”也代表着处理器设计的源头，强调技术的源头开放、共享协作。“泺源”二字彰显了平台以源头创新驱动芯片技术突破的愿景，成为处理器研发领域的核心引擎。

## 致谢

### 引用开源项目：

- pugixml: [https://github.com/zeux/pugixml](https://github.com/zeux/pugixml)
- json : [https://github.com/nlohmann/json](https://github.com/nlohmann/json)
- argparse : [https://github.com/p-ranav/argparse](https://github.com/p-ranav/argparse)

生成 RTL 时可使用 `vulrtlgen -r -t example/aes1/AES1.hpp`（或 `--release`），不保留中间 `.logic.cpp`、`.dbg`、`.dbgmap` 和 TestMain C++ 文件。默认模式保持原有调试输出；综合所需的 RTL 库及显式资源仍会复制。
