# VulTestHarnessModule and test harness generation

# VulTestHarnessModule 代码生成细节

## 模块类外形定义

```cpp

class /*ModuleName*/ {
public:

/*ModuleName*/() {
    // <Init Field>
}

FORCE_INLINE void simulation() {
    // <Simulation Field>
}

protected:

static void __childstall_wrapper(void *ctx) {}
void sim_execute() {
    /*TopModuleName*/->on_current_tick();
}
void sim_commit() {
    /*TopModuleName*/->apply_next_tick();
}
// <Member Field>

};

```

## 成员函数生成细节




