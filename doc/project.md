# 项目文件结构

```
--- Top dir
  |-- bundle // 每个xml文件一个bundle
    |-- bundles.xml
    |-- xxx.xml
  |-- combine // 每个xml文件一个combine
    |-- combines.xml
    |-- xxx.xml
  |-- cpp // 每个xml文件一个需要编写的代码块，命名为${combine name}_${member name}.cpp
    |-- global.h
    |-- cppcodes.cpp
    |-- xxx.cpp
  |-- config.xml
  |-- design.xml
  |-- ${PROJ_NAME}.vul
```


