# Vul 前后端交互操作文档

## 操作包格式

请求JSON格式：

```json
{
    "name": "operation_name",
    "args": [
            { "index": 0, "name": "width", "value": 64 },
            { "index": 1, "name": "height", "value": 32 },
            { "index": 2, "name": "pipeline_depth", "value": 5 }
        ]
}
```

返回JSON格式：

```json
{
    "code": 0,
    "msg": "Success",
    "results": {
        "key1": "value1"
        // 操作返回数据
    },
    "list_results": {
        "key1": [
            "item1",
            "item2"
            // 操作返回的列表数据
        ]
    }
}
```

# 项目管理操作

## load 操作

load 操作用于从后端的本地项目库中加载 Vul 项目文件及其相关模块、配置库和 Bundle 库。

### 请求参数

| 参数名         | 必填 | 描述                         |
|---------------|------|------------------------------|
| name          | 是   | Vul 项目名                    |

### 返回结果

| 是否为列表 | 参数名         | 描述                         |
|----------|---------------|------------------------------|
| 是       | logs          | 加载过程中的日志信息列表            |

### 错误码

```cpp
constexpr uint32_t EOPLoad = 30000;
constexpr uint32_t EOPLoadMissArg = 30001;
constexpr uint32_t EOPLoadNotClosed = 30002;
constexpr uint32_t EOPLoadInvalidPath = 30003;
constexpr uint32_t EOPLoadSerializeFailed = 30004;
constexpr uint32_t EOPLoadImportNotFound = 30005;
constexpr uint32_t EOPLoadImportInvalidPath = 30006;
constexpr uint32_t EOPLoadBundleConflict = 30007;
constexpr uint32_t EOPLoadModuleConflict = 30008;
constexpr uint32_t EOPLoadNoTopModule = 30009;
constexpr uint32_t EOPLoadConfigInvalidValue = 30010;
constexpr uint32_t EOPLoadConfigLoopRef = 30011;
constexpr uint32_t EOPLoadBundleInvalidValue = 30012;
constexpr uint32_t EOPLoadBundleMissingConf = 30013;
constexpr uint32_t EOPLoadBundleLoopRef = 30014;
constexpr uint32_t EOPLoadModuleMissingInst = 30015;
constexpr uint32_t EOPLoadModuleLoopRef = 30016;
```

## save 操作

save 操作用于保存当前 Vul 项目文。

**无参数**

**无返回**

### 错误码

```cpp
constexpr uint32_t EOPCancel = 30050;
constexpr uint32_t EOPCancelNotOpened = 30051;
```

## cancel 操作

cancel 操作用于关闭当前打开的 Vul 项目并放弃未保存的更改。

**无参数**

**无返回**

### 错误码

```cpp
constexpr uint32_t EOPSave = 30060;
constexpr uint32_t EOPSaveNotOpened = 30061;
constexpr uint32_t EOPSaveFileFailed = 30062;
```

## create 操作

create 操作用于创建一个空白的 Vul 项目。

### 请求参数

| 参数名         | 必填 | 描述                         |
|---------------|------|------------------------------|
| name          | 是   | Vul 项目名                    |

**无返回**

### 错误码

```cpp
constexpr uint32_t EOPCreate = 30070;
constexpr uint32_t EOPCreateAlreadyOpened = 30071;
constexpr uint32_t EOPCreateMissArg = 30072;
constexpr uint32_t EOPCreateNameExists = 30073;
constexpr uint32_t EOPCreateFileFailed = 30074;
```

## list 操作

list 操作用于列出后端本地项目库中的所有 Vul 项目。

**无参数**

### 返回结果

| 是否为列表 | 参数名         | 描述                         |
|----------|---------------|------------------------------|
| 否       | rootpath      | 后端本地项目库根路径               |
| 是       | names          | 后端本地项目库中的所有 Vul 项目名称列表 |
| 是       | paths          | 后端本地项目库中的所有 Vul 项目路径列表（相对根路径） |
| 是       | modtimes       | 后端本地项目库中的所有 Vul 项目最后修改时间列表 |

**无错误码**
