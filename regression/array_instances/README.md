# 阵列实例回归

Top 持有 `PE arr1[8]` 和 `PE arr2[4]`，每个 PE 持有一个标量 FPU。
FPU 是用于验证参数化数据通路的位向量测试模块，并非浮点算术实现。

- `arr1` 使用 `COORD(0)=ROW`，`arr2` 保留 `ROW=3` 默认值。
- PE 显式传递 `WIDTH=ROW+9` 和 `DEPTH=ROW+1`；FPU 的 `TAG=23` 保持默认值，`DERIVED=DEPTH+3` 验证依赖默认参数。
- 命令沿 `CONNECT_S_CS` 和子服务到达 FPU；QUERY 汇总回 Top；PE 的流事务通过 `CONNECT_CR_S` 写入 Top 快照。
- 对每个周期检查数据截断、实例独立状态、事务索引、队列满时序和前周期数据；重复复位后重新运行。
- 生成器专项脚本另检查五组声明路径、25 个具体实例、非法绑定、非方形完整绑定、嵌套结构变化、调用诊断和 priority；同时严格比较原始 `systolic2d` 的双路径输出，检查 RTL 实现去重及具体 Trace 路径。

当前 C++ 生成采用同名类模板的具体坐标特化，特化集中在声明路径对应的一组文件中。RTL 使用公开参数化模块选择已求值实现，支持本工程展开得到的有限合法坐标集合。

运行全部回归：`./regression.sh`。

单独运行：

```sh
build/vulsimgen -m regression/array_instances/Main.cpp -f -o build/array_instances-sim
build/array_instances-sim/run.sh
build/vulrtlgen -m regression/array_instances/Main.cpp -f -o build/array_instances-rtl
build/array_instances-rtl/run.sh
python3 regression/generator_array_instances.py
```
