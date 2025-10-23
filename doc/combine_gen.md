# Combine 组合逻辑块生成

一个组合逻辑块对应一个C++类，类名为$name$标签的内容

$name$.h:
```cpp
#pragma once

#include "common.h"
#include "global.h"
#include "bundle.h"
#include "vulsimlib.h"

/* $comment$ */
class $name$ {
public:
    class ConstructorArguments {
    public:
        // Constructor Arguments Field
        int32 __dummy = 0;
    };

    $name$(ConstructorArguments & arg);
    ~$name$();

    void init();
    void all_current_tick();
    void all_current_applytick();
    void user_current_tick();
    void user_current_applytick();
    
    // Member Field
};
```
$name$.cpp:
```cpp
#include "$$name$$.h"

$name$::$name$(ConstructorArguments & arg) {
    // Constructor Field
    init();
};
$name$::~$name$() {
    // De-Constructor Field
};

void $name$::init() {
    // Init Field
}

// Function Field

void $name$::all_current_tick() {
    // Tick Field
    user_current_tick();
}
void $name$::all_current_applytick() {
    // Applytick Field
    user_current_applytick();
}
void $name$::user_current_tick() {
    // User Tick Field
}
void $name$::user_current_applytick() {
    // User Applytick Field
}
```

xxx.cpp
```cpp
// Helper Field
```

## pipein

包含$name$, $type$, $comment$标签

Member Field +：
```cpp
PipePopPort<$type$> * _pipein_$name$;
/* $comment$ */
bool $name$_can_pop() { return _pipein_$name$->can_pop(); };
/* $comment$ */
$type$ (&) $name$_top() { return _pipein_$name$->top(); };
/* $comment$ */
void $name$_pop() { _pipein_$name$->pop(); };
```

Constructor Arguments Field +:
```cpp
PipePopPort<$type$> * _pipein_$name$;
```

Constructor Field +:
```cpp
this->_pipein_$name$ = arg._pipein_$name$;
```

Helper Field +:
```cpp
/* $comment$ */
bool $name$_can_pop();
/* $comment$ */
$type$ (&) $name$_top();
/* $comment$ */
void $name$_pop();
```

## pipeout

包含$name$, $type$, $comment$标签

Member Field +：
```cpp
PipePushPort<$type$> * _pipeout_$name$;
/* $comment$ */
bool $name$_can_push() { return _pipeout_$name$->can_push(); } ;
/* $comment$ */
void $name$_push($type$ (&) value) { _pipeout_$name$->push(value) } ;
```

Constructor Arguments Field +:
```cpp
PipePushPort<$type$> * _pipeout_$name$;
```

Constructor Field +:
```cpp
this->_pipeout_$name$ = arg._pipeout_$name$;
```

Helper Field +:
```cpp
/* $comment$ */
bool $name$_can_push();
/* $comment$ */
void $name$_push($type$ (&) value);
```


## request

包含$name$, $arg$, $return$, $comment$标签

Member Field +：
```cpp
void (*_request_$name$)($argtype$ (&) $argname$, $rettype$ * $retname$, ...);
/*
 * $comment$
 * @arg $argname$: $argcomment$
 * @ret $retname$: $retcomment$
*/
void $name$($argtype$ (&) $argname$, $rettype$ * $retname$, ...) { _request_$name($argname$, $retname$, ...); };
```

Constructor Arguments Field +:
```cpp
void (*_request_$name$)($argtype$ (&) $argname$, $rettype$ * $retname$, ...);
```

Constructor Field +:
```cpp
this->_request_$name$ = arg._request_$name$;
```

Helper Field +:
```cpp
/*
 * $comment$
 * @arg $argname$: $argcomment$
 * @ret $retname$: $retcomment$
*/
void $name$($argtype$ (&) $argname$, $rettype$ * $retname$, ...);
```

## service

包含$name$, $arg$, $return$, $cppfunc$, $comment$标签，引用组合逻辑类名$cname$

Member Field +：
```cpp
/*
 * $comment$
 * @arg $argname$: $argcomment$
 * @ret $retname$: $retcomment$
*/
void $name$($argtype$ (&) $argname$, $rettype$ * $retname$, ...);
```

Function Field +:
```cpp
void $cname$::$name$($argtype$ (&) $argname$, $rettype$ * $retname$, ...) {
    $cppfunc$
};
```

Helper Field +:
```cpp
/*
 * $comment$
 * @arg $argname$: $argcomment$
 * @ret $retname$: $retcomment$
*/
void $name$($argtype$ (&) $argname$, $rettype$ * $retname$, ...);
```

## function

同service

## storage

包含$name$, $type$, $value$, $comment$标签

Member Field +：
```cpp
/* $comment$ */
$type$ $name$( = $value$);
```

Helper Field +:
```cpp
/* $comment$ */
$type$ $name$( = $value$);
```


## storagenext

包含$name$, $type$, $value$, $comment$标签

Member Field +：
```cpp
StorageNext<$type$> _storagenext_$name$( = StorageNext<$type$>($value$));
/* $comment$ */
$type$ (&) $name$_get() { return _storagenext_$name$.get(); };
/* $comment$ */
void $name$_setnext($type$ (&) value, uint8 priority) { _storagenext_$name$.setnext(value, priority); } ;
```

Helper Field +:
```cpp
/* $comment$ */
$type$ (&) $name$_get();
/* $comment$ */
void $name$_setnext($type$ (&) value, uint8 priority);
```

Applytick Field +:
```cpp
_storagenext_$name$.apply_tick();
```

## storagenextarray

包含$name$, $type$, $size$, $value$, $comment$标签

Member Field +：
```cpp
StorageNextArray<$type$> _storagenextarray_$name$;
/* $comment$ */
$type$ (&) $name$_get(int64 index) { return _storagenextarray_$name$.get(index); };
/* $comment$ */
void $name$_setnext(int64 index, $type$ (&) value, uint8 priority) { _storagenextarray_$name$.setnext(index, value, priority); } ;
```

Constructor Field +:
```
_storagenextarray_$name$ = StorageNextArray<$type$>(size (, value));
```


Helper Field +:
```cpp
/* $comment$ */
$type$ (&) $name$_get(int64 index);
/* $comment$ */
void $name$_setnext(int64 index, $type$ (&) value, uint8 priority);
```

Applytick Field +:
```cpp
_storagenextarray_$name$.apply_tick();
```


## storagetick

包含$name$, $type$, $value$, $comment$标签，必须为基本类型

Member Field +：
```cpp
/* $comment$ */
$type$ $name$ = $value$;
```

Helper Field +:
```cpp
/* $comment$ */
$type$ $name$ = $value$;
```

Applytick Field +:
```cpp
$name$ = $value$;
```

## tick

包含$cppfunc$, $comment$标签

User Tick Field +:
```cpp
    $cppfunc$
```

## applytick

包含$cppfunc$, $comment$标签

User Applitick Field +:
```cpp
    $cppfunc$
```

## config

包含$name$, $value$, $comment$标签

Constructor Arguments Field +:
```cpp
int64 $name$;
```

Constructor Field +:
```cpp
this->$name$ = arg.$name$;
```

Member Field +：
```cpp
/* $comment$ */
int64 $name$;
```

Helper Field +:
```cpp
/* $comment$ */
const int64 $name$ = $value$;
```

## init

包含$cppfunc$标签

Constructor Field +:
```cpp
    $cppfunc$
```



## stallable

空

Member Field +：
```cpp
bool _stalled = false;
void (*_external_stall)();
void stall() { _stalled = true; if(_external_stall) _external_stall(); }
bool is_stalled() { return _stalled; }
```

Constructor Arguments Field +:
```cpp
void (*_external_stall)();
```

Constructor Field +:
```cpp
this->_external_stall = arg._external_stall;
```


Helper Field +:
```cpp
bool is_stalled();
void stall();
```

Applytick Field +:
```cpp
stalled = false;
```




