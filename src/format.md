# config XML文件结构

```
|-- configlib
  |-- [configitem]
    |-- name
    |-- value
    |-- (comment)
```

# bundle XML文件结构

```
|-- bundle
  |-- name
  |-- (comment)
  |-- [member]
    |-- name
    |-- type
    |-- (comment)
```

# combine XML文件结构

```
|-- combine
  |-- name
  |-- (comment)
  |-- [config]
    |-- name
    |-- ref
    |-- (comment)
  |-- [pipein]
    |-- name
    |-- type
    |-- (comment)
  |-- [pipeout]
    |-- name
    |-- type
    |-- (comment)
  |-- [request]
    |-- name
    |-- (comment)
    |-- [arg]
      |-- name
      |-- type
      |-- (comment)
    |-- [return]
      |-- name
      |-- type
      |-- (comment)
  |-- [service]
    |-- name
    |-- (comment)
    |-- [arg]
      |-- name
      |-- type
      |-- (comment)
    |-- [return]
      |-- name
      |-- type
      |-- (comment)
    |-- cppfunc
      |-- (code)
      |-- (file)
      |-- (name)
  |-- [storage/storagenext/storagetick]
    |-- name
    |-- type
    |-- (value)
    |-- (comment)
  |-- (stallable)
  |-- (tick/applytick/init)
    |-- cppfunc
      |-- (code)
      |-- (file)
      |-- (name)
```

# design XML文件结构

```
|-- design
  |-- [instance]
    |-- name
    |-- combine
    |-- (comment)
    |-- [config]
      |-- name
      |-- value
    |-- visual
  |-- [pipe]
    |-- name
    |-- type
    |-- (comment)
    |-- inputsize
    |-- outputsize
    |-- buffersize
    |-- visual
  |-- [reqconn]
    |-- reqport
      |-- instname
      |-- portname
    |-- [servport]
      |-- instname
      |-- portname
    |-- visual
  |-- [modpipe, pipemod]
    |-- instname
    |-- portname
    |-- pipename
    |-- portindex
    |-- visual
  |-- [stalledconn]
    |-- src
      |-- instname
    |-- dest
      |-- instname
    |-- visual
  |-- [updateseq]
    |-- former
      |-- instname
    |-- later
      |-- instname
    |-- visual
  |-- [block, text]
    |-- name
    |-- visual
```

```
  |-- visual
    |-- x,y,w,h
    |-- visibal, enabled, selected
    |-- color
    |-- bordercolor
    |-- borderwidth
```

