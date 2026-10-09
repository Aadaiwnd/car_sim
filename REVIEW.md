# REVIEW.md —— 四轮小车控制模拟

> 提交前请通读全文，把内容改成自己的语气重新组织一遍再录入提交。

## 问题 1：程序由哪些主要部分组成，各部分负责什么？main 中主要完成了什么工作？

我的程序分为两个层次、三个文件：

| 文件 | 层次 | 负责的内容 |
|------|------|-----------|
| `car.h` | 接口声明 | 定义 `Car`、`MotionRequest` 等数据结构，以及小车模型对外的全部函数接口 |
| `car.c` | 核心逻辑层 | 维护小车的状态机（IDLE / RUNNING / EMERGENCY）、自动与手动两份控制要求、四轮实际速度；实现 `start`、`step`、`emergency`、`reset` 等行为 |
| `main.c` | 解析交互层 | 从标准输入逐行读命令、分词、校验参数合法性，然后调用 `car.h` 的接口完成实际动作，并打印提示信息 |

之所以这样拆，是想让"车怎么动"和"命令怎么读"彻底分开：`car.c` 完全不做输入输出（除了被动的数据查询），`main.c` 完全不懂速度变化规则。这样以后如果想把命令来源从标准输入换成串口或网络，只需要改 `main.c`；反过来，改运动规则只动 `car.c`。

`main.c` 的 `main()` 中主要做了四件事：

1. 调用 `car_init()` 把小车初始化为待机状态；
2. 进入 `while (fgets(...))` 主循环，把每一行按空白切成单词数组（`split_line()`）；
3. 根据第一个单词分发到对应的处理分支：`start` / `auto` / `manual` / `step` / `emergency` / `reset` / `status` / `quit` 等，其中 `auto` 和 `manual` 的参数格式相同，共用 `handle_motion_command()` 一个解析函数；
4. 每个分支先做参数校验（缺参数、速度不是 0~100 的整数、多余参数、未知运动方式），校验通过再检查系统状态，最后才调用 `car.c` 的接口。`status` 分支调用 `print_status()` 按题目规定的 6 行格式输出。

## 问题 2：手动控制优先是如何保证的？`manual release` 后又是如何恢复自动要求的？

**优先级的保证集中在 `car.c` 的 `car_effective()` 一个函数里**，这是整个程序最关键的设计：

```c
const MotionRequest *car_effective(const Car *car)
{
    if (car->manual_req.valid)
        return &car->manual_req;         /* 手动优先 */
    if (car->auto_req.valid)
        return &car->auto_req;
    return &g_stop_request;              /* 无任何要求时为 STOP */
}
```

自动和手动的要求分别保存在 `Car` 结构体的 `auto_req` 和 `manual_req` 两个 `MotionRequest` 成员里，互不覆盖：

- 执行 `manual left 30` 时，只写 `manual_req`，`auto_req` 里原来保存的（比如 `FORWARD 60`）原封不动；
- 所有需要知道"现在真正该做什么"的地方——`car_step()` 计算目标速度、`car_target_wheels()`、以及 `status` 输出里的 `CONTROL` / `TARGET` 行——全部通过 `car_effective()` 取结果。

这样优先级规则只存在于这一处，不存在第二份判断逻辑，也就不会出现两处判断不一致的 bug。比如 `manual stop` 也只是把 `manual_req` 设为 `{valid=1, mode=STOP}`，因为 `valid` 仍然是 1，所以手动仍然"生效"，`CONTROL` 输出 `MANUAL`、`TARGET` 输出 `STOP`，与题目要求一致。

**`manual release` 的恢复是"自动发生"的**：`car_release_manual()` 只做一件事——把 `manual_req.valid` 清零：

```c
int car_release_manual(Car *car)
{
    if (!car->manual_req.valid)
        return -1;
    car->manual_req.valid = 0;    /* 只清手动，自动要求从未被破坏 */
    return 0;
}
```

由于 `auto_req` 在手动生效期间一直被完整保存（期间执行 `auto backward 80` 也只是更新 `auto_req`），`valid` 清零后，下一次任何地方调用 `car_effective()`，`manual_req.valid` 已经是 0，条件自然落到 `auto_req` 分支，返回最近保存的自动要求——不需要"恢复"这个动作，因为自动要求从未丢失。

## 问题 3：普通运动命令、step 和 emergency 对四轮实际速度的影响有什么区别？分别在哪里处理？

三者的本质区别是：**运动命令只改"目标"，step 让"实际"追"目标"，emergency 直接把"实际"清零**。

| 操作 | 对目标速度 | 对实际速度 | 处理位置 |
|------|-----------|-----------|---------|
| `auto/manual ...` | 更新生效要求对应的四轮目标速度 | **完全不碰**实际速度 | `main.c` 的 `handle_motion_command()` 校验参数后调用 `car_set_auto()` / `car_set_manual()`，只写 `req` 成员 |
| `step` | 不改目标 | 每轮实际速度向各自目标最多变化 20，正确经过 0 | `car.c` 的 `car_step()`：先 `car_target_wheels()` 算出目标，再逐轮做 ±20 的逼近夹取 |
| `emergency` | 不改已保存的控制要求 | 四轮实际速度**立即**变 0，不经过 step | `car.c` 的 `car_emergency()`：直接把 `wheel[4]` 全部置 0 |

`car_step()` 里的逼近逻辑：

```c
if (car->wheel[i] < target[i]) {
    car->wheel[i] += CAR_STEP_MAX;
    if (car->wheel[i] > target[i])
        car->wheel[i] = target[i];
} else if (car->wheel[i] > target[i]) {
    car->wheel[i] -= CAR_STEP_MAX;
    if (car->wheel[i] < target[i])
        car->wheel[i] = target[i];
}
```

因为是加/减 20 后再与目标夹取，从 40 变到 -40 时会依次得到 20、0、-20、-40，自然经过 0，不会跳变。`CAR_STEP_MAX` 定义为宏，步长调整只改一处。

另外我做了两个自己的设计决定（题目没有明确规定）：`step` 在待机和急停状态下会拒绝执行（防止急停后 step 又把速度拉起来）；`emergency` 只清实际速度、不清控制要求，所以急停后 `status` 仍能看到保存的 `AUTO`/`MANUAL` 要求，`WHEELS` 为 0。只有 `reset` 才会清空全部控制要求。

## 问题 4：如果增加一种新的运动方式，需要修改哪些位置？

假设新增"原地顺时针旋转 `spin`"（左轮反转、右轮正转）：

**必须修改的只有两处：**

1. `car.h` 的 `MotionMode` 枚举加一项 `MOTION_SPIN`（放在 `MOTION_COUNT` 之前）；
2. `car.c` 的运动方式表 `g_motion_table` 加一行 `{ "SPIN", { -1, +1, -1, +1 } }`。

之后 `motion_find()` 能自动识别 `spin` 输入，`status` 能自动输出 `SPIN`，四轮目标速度按表中的系数乘以速度自动算出——因为命令解析（`main.c`）、目标速度计算（`car_target_wheels()`）、状态输出（`print_status()`）全都是**查表驱动**的，没有任何一个 `if` 写死了"forward 怎么办、left 怎么办"。

**比较方便扩展的地方**：凡是"每个运动方式各自一行数据"的结构——运动方式表、枚举、`format_request()` 这类遍历代码，新增方式都是加数据不改逻辑。

**比较麻烦的地方**：目前的模型假设"四轮目标速度 = 方向系数 ±1 × 统一速度参数"。如果新运动方式不符合这个模型——比如四轮速度大小各不相同（"斜向漂移"让某个轮子只转 70%），或者需要额外参数（"spin 45 度"）——光加表项就不够了，得把 `MotionDef` 从系数数组升级成"每轮独立的比例数组"甚至函数指针，`format_request()` 的输出格式也要跟着变。这是这个设计最大的局限。

**如果继续完善，我会优先改进**：

1. **自动化回归测试**——把 `tests/` 下的几个测试脚本整理成"输入文件 + 期望输出文件"的对比测试，每次改动后一键跑完，防止改扩展时破坏原有行为；
2. 把 `main.c` 里的分发逻辑从一长串 `else if` 改成"命令表 + 处理函数指针"的结构，使命令本身也可以像运动方式一样查表扩展；
3. `emergency` 状态下 `CONTROL`/`TARGET` 的语义（保留还是显示 STOP）是我自己的解释，题目没有明确规定，我会先和出题方确认后再固化。
