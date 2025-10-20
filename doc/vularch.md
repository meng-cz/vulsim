# Vul模型简介

VUL（Visualized Unified Layer for microarchitecture design）是一个描述复杂集成电路的结构化模型。其借鉴并融合了TLV（Transaction Layer Model）和周期行为模型，为集成电路设计和微架构研究探索提供了可视化的设计-仿真-RTL生成方案。

RTL（Register Transfer Level）是目前应用最广泛的集成电路底层建模方案，类似软件界的汇编语言。上游的设计工具链对下生成RTL代码；硬件工具链通过RTL代码构建物理芯片。

RTL分为两部分：寄存器和组合逻辑。
- 寄存器：一个存储单元，在每个周期结束时，其输入值会被存储；每次读取时返回当前存储的值；
- 组合逻辑：一组输入值到一组输出值的映射关系，即通过当前若干寄存器的值计算这些寄存器下周期该被更新的新值。

VUL是对RTL的一种高层次抽象。

项目地址：https://github.com/meng-cz/vulsim 

# Vul模型架构设计

```
Design
├── ConfigLib
│   └── Items
├── [Bundle]
│   └── Members
├── [Combine]
│   ├── Configs
│   ├── Storages
│   ├── Ports
│   ├── Requests
│   ├── Services
│   └── Behaviors
├── [Instance]
│   ├── Configs
│   └── RefCombine
├── [Pipe]
└── [Connection]
    ├── Req-Serv
    ├── Mod-Pipe
    ├── Stall
    └── Sequence
```

## 1 全局内容

一个VUL设计中包含若干用于定义的全局配置参数（ConfigLib）和线束（Bundle）：
- ConfigLib：一组name-value对，保存整数格式的全局配置。具体每个模块的配置可以被链接到全局配置以实现快速更改。
- Bundle：类似结构体struct，将若干基础类型单元合成一个结构体类型，用于跨模块交互中的数据类型定义。

## 2 组件

一个VUL设计由一组实例（Instance），一组管道（Pipe）和之间的连接组成。
- Instance：一个用户定义的硬件模块，包含一组组合逻辑。
- Pipe：内置的延时组件。所有关于“延迟多少周期到达的数据”均通过Pipe构建。

每一个Pipe有如下属性：
- InputSize：输入端口的数量。即一个周期内能同时push多少个元素。
- OutputSize：输出端口的数量。即一个周期内能同时pop多少个元素。
- BufferSize：缓存队列的最大长度。即有多少个额外的元素可以被FIFO队列缓存。
- Latency：输入输出之间额外引入的延迟。即输入的元素最早能在1+多少周期后被输出。
- Handshake：输入输出是否有握手。有握手：can_push，can_pop正常工作，仅在可以push / pop时允许。无握手：can_push，can_pop始终为true，未被及时pop的数据会被覆盖。

每一个Instance都对应一个Combine，类似于面向对象中的Object对应一个Class。Instance展现出的对外输入输出行为均由对应的Combine定义。Instance仅允许覆盖Combine中定义的Config常量值。

Combine描述了一组组合逻辑的对外行为，包括：
- Port：用于对外连接Pipe的输入输出端口。
- Request：调用其他模块进行控制或数据交换。
- Service：提供给其他模块调用的接口。

对内，每个Combine提供了若干内部变量：
- Config：内部常量，可以被链接到全局配置库ConfigLib中的配置项。
- Storage：内部变量，可使用各种内置类型构建，用于描述局部的寄存器行为。

Combine使用C/C++语法来描述周期内该模块的硬件行为：
- Init：初始化时需要进行的行为。
- Tick：上半周期（计算组合逻辑时）该模块的行为。
- Applytick：下半周期（应用寄存器更改时）该模块的行为。
- Service：对于每个Service，当其被调用时该模块的行为。

## 3 连接

在VUL中，共有以下五种连接：
- Req-Serv：连接一个实例的Request到另一个实例的Service，表示调用请求-处理请求的关系。相连的Request和Service必须具有完全一致的参数类型。有返回值的Req必须连接至仅一个Serv。无返回值的Req可以连接任意个Serv或悬空。
- Mod-Pipe：连接一个实例的pipeout端口到一个Pipe的input端口，表示该实例会向管道中push元素。每一个实例的pipeout端口必须被连接到一个Pipe。每个Pipe的input端口数量可配置且必须被全部连接。
- Pipe-Mod：连接一个Pipe的output端口到一个实例的pipein端口，表示该实例会从管道中pop元素。约束同上。
- Stalled：连接两个被标记为可阻塞（Stallable）的实例。实例A连接到实例B意为，如果实例A在本周期内判断自身应当阻塞，则实例B也需要在本周期内阻塞。一般用于连接通过非握手Pipe相连的流水线阶段，以防止数据在流水线中因后面的模块阻塞而丢失。该连接约束了实例A必须先于实例B被更新。
- Sequence：连接实例A到实例B意为A必须先于B更新。用于处理两个通过req-serv连接的实例中serv与tick的优先级问题。Sequence+Stalled均约束了更新次序，因此这两个约束的并集内部不能成环。

## 4 预制件（目前版本的console中暂未实现）

VUL中定义的从外部引用的库模块。一个Prefab被import时，以下内容会被加入到VUL Design中：
- 一个与Prefab同名的只读Combine。用户可以在Design中实例化该Combine。
- 一组依赖的Bundle。如果同名Bundle已经在Design中被定义，则其成员类型和名字必须完全一致，否则不允许引入此Prefab。

## 5 VUL项目文件结构

```
project_dir/
├── bundle/
│   ├── bundles.xml
│   └── ...
├── combine/
│   ├── combines.xml
│   └── ...
├── cpp/
│   ├── global.h
│   ├── comb_tick.cpp
│   ├── comb_serv_serv1.cpp
│   └── ...
├── config.xml
├── design.xml
└── projname.vul
```

- bundle/：保存了Design中定义的Bundles。每一个Bundle对应一个同名.xml文件。
- combine/：保存了Design中定义的Combines。每一个Combine对应一个同名.xml文件。
- cpp/：保存了Combine中引用的C++行为描述代码。每一个代码段（tick/applytick/init/service）对应一个名为combname_type_servname的C++函数，保存在同名.cpp文件。
- config.xml：保存了Design中定义的ConfigLib。
- design.xml：保存了Design中定义的所有Instances、Pipes和Connections。
- projname.vul：以XML格式保存了项目版本信息，引入的预制件信息等。

## 6 VUL Console

VUL Console是一个字符终端，以字符串输入输出形式完成对VUL Design的基本编辑操作。
详细接口与命令列表在`src/console.h`文件中。


# GUI内容
GUI代码完全基于现有的VulConsole构建。GUI模块只读访问VulConsole中的VulDesign指针并显示。任何对VulDesign的编辑和修改必须通过构建VulConsole命令进行以保证一致性。

## v0.1：

- 实现对VulDesign中ConfigLib、Bundle、Combine、Instance、Pipe、Connection的可视化显示
- ConfigLib、Bundle、Combine分别作为侧边栏显示。Instance、Pipe、Connection作为主体显示
- 在GUI上以底边栏或子窗口的形式实现一个终端，接入VulConsole以进行基于TUI的编辑。

## v0.2：

- 实现通过GUI的项目新建、打开、关闭、保存、另存为、生成目标仿真代码
- 实现通过GUI的Instance、Pipe、Connection可视化元素移动、缩放、文本线条颜色样式修改

## v0.3：

- 实现通过GUI的VulDesign编辑，覆盖所有VulConsole编辑命令
- 预留编辑C++代码模型的按钮，暂时实现为调用vscode等编辑器打开对应的.cpp文件（需要先保存项目并调用combine updatecpps命令更新生成的代码模板。

## v1.0：

- 测试并发布初版

## v1.1：

- 实现基于事件记录的撤销-重做功能
- 实现全局符号搜索/替换功能





