# 双通道 DMA 回归

同一个 Main 在 C++ 和 Verilator 中运行独立逐周期模型；无需额外配置。
从仓库根目录执行 `./regression.sh` 可运行本项目及现有回归。
单独执行：

```sh
build/vulsimgen -m regression/dma2/Main.cpp -f -o build/dma2-sim
build/dma2-sim/run.sh
build/vulrtlgen -m regression/dma2/Main.cpp -f -o build/dma2-rtl
build/dma2-rtl/run.sh
```

## 结构与参数

Top 包含共享存储、观察模块和 DMA 子系统。两个 DmaChannel 各包含
CommandBuffer 和 TransferEngine；MemorySubsystem 包含 MemoryArbiter
和 Scratchpad。包装模块通过 USE_CHILD_SERVICE 调用缓冲和存储服务，
既保留独立子实例，又测试 CR-S 及隐式子服务请求。

Main 将 BASE_DEPTH 默认 2 覆盖为 3，将 RESET_SEED 覆盖为 0x4A31。
通道 0 使用默认命令深度 1、数据位宽 17、地址步进 1；通道 1 使用父参数
表达式得到深度 5、步进 2，并覆盖位宽为 29。局部 LocalData 类型和状态
必须保持实例隔离。Scratchpad 大小从默认 32 覆盖为 37，派生 ADDR_BITS
应为 6，复位 word[i] 为 uint32_t(0x4A31 XOR (i * 0x9E3779B9))。

搬运按元素逐一读取、截取通道位宽、零扩展写回。长度 0 直接完成。
所有命令地址合法，允许源目的重叠；重叠操作按实际周期读值建模，
不采用整块 memmove 的语义。

## 事务和顺序覆盖

- CONNECT_S_CS：提交和响应跨多级边界透传。
- CONNECT_CR_R：预约、响应、完成向上暴露。
- CONNECT_CR_CS：DMA／存储／观察模块之间的连接。
- CONNECT_CR_S：命令 pop、共享存储读写及完成统计。
- USE_CHILD_SERVICE：缓冲 pop、存储读写，包含 RESP 返回。
- USE_CHILD_QUERY：全部层次的状态汇总，包括完整存储快照。

通道 0 offer 的 priority=20，通道 1 为 10；仲裁器 Tick 为 0，最多执行
一次访问；引擎 response 为 -10，检查本周期 issued WIRE 并通过寄存器
端口 0 覆盖 Tick 端口 1 的等待状态；observer 的 audit 为 -20，读取
仲裁器已执行的周期内 WIRE。存储在 cycle % 5 == 2 时关闭。
同值 priority 不规定顺序。实例命名和源声明顺序与预期顺序相反，
不依赖默认实例排序。WIRE 的复位、寄存器 next 语义、Service 的
执行顺序和寄存器端口优先级分别检查。

## 验证

每周期检查所有状态、请求／响应内容、完成回调、仲裁器和 Observer
审计记录，以及全部 37 个存储元素。软件模型只使用自身状态和外部
激励预测结果，不从 DUT 的快照推导预期行为。

定向测试包含边界地址、零长度、两个通道的截断和步进、同周期竞争、
重叠地址、空／满队列时序、队列容量、完成背压、竞争和完成阻塞时
复位、空闲周期 WIRE 清除。随机阶段采用 xorshift32、种子 0xD2A05EED，
两个通道各提交 100 个任务，长度 0～4，随机合法地址、提交间隔和
完成背压。每阶段超时为 20,000 周期。

通过时必须命中两个通道的读写／完成、两个队列满、两个完成阻塞、
低优先级读写竞争失败、存储关闭拒绝、空队列提交和满队列同时 pop。
失败打印阶段、周期、通道、任务、元素、字段及期望／实际值并返回 1。

独立回归元素 `regression/generator_scheduling_connectivity.py` 使用临时工程
补充祖先 Tick／后代调用排序和数组服务
转发的正例，以及缺少实现、类型不匹配、双重实现、多调用源、调用环、
重复调用和更新顺序环的拒绝诊断。`regression.sh` 扫描 `regression/*.py`，
按文件名顺序逐一运行，并将每个脚本的退出码作为独立 PASS／FAIL 项目汇总；
专项检查结果与 DMA2 的 C++／RTL 回归结果分别统计。

单独运行专项检查：

```sh
python3 regression/generator_scheduling_connectivity.py
```
