# Prefab 文件结构

```
|-- PrefabName
  |-- PrefabName.vulp
  |-- PrefabName.h
  |-- bundle
    |-- bundles.xml
    |-- ...
  |-- source
    |-- src.cpp
    |-- ...
  |-- slib
    |-- win
      |-- xxx.lib
      |-- ...
    |-- linux
      |-- xxx.a
      |-- ...
```

# Prefab.vulp XML文件结构

```
|-- prefab
  |-- comment
  |-- [config]
    |-- name
    |-- value
    |-- comment
  |-- [pipein]
    |-- name
    |-- type
    |-- comment
  |-- [pipeout]
    |-- name
    |-- type
    |-- comment
  |-- [request]
    |-- name
    |-- comment
    |-- [arg]
      |-- name
      |-- type
      |-- comment
    |-- [return]
      |-- name
      |-- type
      |-- comment
  |-- [service]
    |-- name
    |-- comment
    |-- [arg]
      |-- name
      |-- type
      |-- comment
    |-- [return]
      |-- name
      |-- type
      |-- comment
```

