# REVIEW.md —— 四轮小车控制模拟

## 问题 1：程序由哪些主要部分组成，各部分负责什么？main 中主要完成了什么工作？

我把程序拆成了三个文件，分成两层：

- `car.h` + `car.c` 是核心模型层。`car.h` 里定义了 `Car`、`MotionRequest` 这几个结构体和对外函数的声明；`car.c` 保存小车的状态机（待机 / 运行 / 急停）、自动和手动两份控制要求、四个轮子的实际速度，`start`、`step`、`emergency`、`reset` 这些行为都在这里实现。
- `main.c` 是命令交互层，只负责读输入、拆单词、检查参数合不合法，然后调用 `car.h` 声明的接口去做事，最后打印结果。

这样拆的原因是让"车怎么动"和"命令怎么读"互不干扰：`car.c` 里没有任何输入输出，`main.c` 里也不写速度变化规则。以后就算把输入源换成串口或者网络，只需要改 `main.c`。

`main()` 里做的事情按顺序是：先调 `car_init()` 把小车初始化成待机；然后进入 `while (fgets(...))` 主循环，每读一行就用 `split_line()` 按空白切成单词；接着看第一个单词是什么命令，分发给对应的处理分支，一共有 `start` / `auto` / `manual` / `step` / `emergency` / `reset` / `status` / `quit` 这几个，其中 `auto` 和 `manual` 参数格式一样，我让它们共用 `handle_motion_command()` 一个函数；每个分支都是先检查参数（缺不缺参数、速度是不是 0~100 的整数、有没有多余参数、运动方式认不认识），通过了再检查小车的状态，最后才真正调 `car.c` 的接口。`status` 分支调 `print_status()`，按题目要求的 6 行格式输出。

## 问题 2：手动控制优先是如何保证的？`manual release` 后又是如何恢复自动要求的？

优先级判断我只写了一个函数 `car_effective()`，所有需要知道"小车现在到底该执行哪个要求"的地方都调它：

```c
const MotionRequest *car_effective(const Car *car)
{
    if (car->manual_req.valid)
        return &car->manual_req;         /* 手动优先 */
    if (car->auto_req.valid)
        return &car->auto_req;
    return &g_stop_request;              /* 什么都没有就停车 */
}
```

`Car` 结构体里有两个 `MotionRequest` 成员，`auto_req` 和 `manual_req`，分别存自动和手动的控制要求，互相不覆盖。比如先 `auto forward 60`，再 `manual left 30`，后者只会写 `manual_req`，`auto_req` 里存的 `FORWARD 60` 一直原样保留着。

`car_step()` 算目标速度、`status` 输出里的 `CONTROL` 和 `TARGET` 行，全部都是从 `car_effective()` 拿的结果。这样做的好处是优先级规则只有这一份，不存在第二个地方再判断一遍，自然不会出现两边判断打架的情况。`manual stop` 的情况也一样：它只是把 `manual_req` 设成 valid 且模式为 STOP，所以 `CONTROL` 仍然显示 MANUAL、`TARGET` 显示 STOP，符合题目要求。

`manual release` 的恢复其实是"不用恢复"。`car_release_manual()` 只干一件事，把 `manual_req.valid` 清零：

```c
int car_release_manual(Car *car)
{
    if (!car->manual_req.valid)
        return -1;
    car->manual_req.valid = 0;
    return 0;
}
```

因为 `auto_req` 在整个手动期间从来没有被破坏过（手动期间执行 `auto backward 80` 也只是更新 `auto_req`），所以 valid 一清零，下一次 `car_effective()` 的第一个条件不成立，自然就落到 `auto_req` 那个分支，返回最近一次保存的自动要求。不是"恢复"出来的，是一直就在那里。

## 问题 3：普通运动命令、step 和 emergency 对四轮实际速度的影响有什么区别？分别在哪里处理？

我的理解是三者的区别一句话就能说清：**运动命令只改"目标"，step 让"实际"去追"目标"，emergency 直接把"实际"清零**。

- `auto/manual ...` 命令：在 `main.c` 的 `handle_motion_command()` 里校验完参数后调 `car_set_auto()` / `car_set_manual()`，只更新控制要求和对应的目标速度，完全不碰实际速度。
- `step`：在 `car.c` 的 `car_step()` 里处理。先用 `car_target_wheels()` 算出四轮目标，再让每个轮子的实际速度朝各自的目标最多变化 20：

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

先加减 20 再和目标夹一下，所以从 40 变到 -40 会依次经过 20、0、-20、-40，不会跳变。步长 `CAR_STEP_MAX` 是宏，以后想改只动一处。

- `emergency`：在 `car.c` 的 `car_emergency()` 里，直接把四个轮子的实际速度全部置 0，一步到位，不经过 step 的渐进过程，也不清除已保存的控制要求。

另外有两个题目没明确规定、我自己拿主意的地方：一是 `step` 在待机和急停状态下会拒绝执行，防止急停之后 step 又把速度拉起来；二是 `emergency` 之后 `status` 仍能看到保存的 AUTO/MANUAL 要求，`WHEELS` 是 0，只有 `reset` 才会清空全部控制要求。

## 问题 4：如果增加一种新的运动方式，需要修改哪些位置？

拿"原地顺时针旋转 spin"举例（左轮反转、右轮正转），必须改的只有两处：

1. `car.h` 的 `MotionMode` 枚举里加一项 `MOTION_SPIN`（放在 `MOTION_COUNT` 之前）；
2. `car.c` 的运动方式表 `g_motion_table` 加一行 `{ "SPIN", { -1, +1, -1, +1 } }`。

改完这两处，`motion_find()` 就能识别 `spin` 输入，`status` 能输出 `SPIN`，四轮目标速度按表里的系数乘速度自动算出来。因为命令解析（`main.c`）、目标速度计算（`car_target_wheels()`）、状态输出（`print_status()`）全是查表驱动的，没有哪处写死了"forward 怎么办、left 怎么办"，所以加数据就是加功能。

方便扩展的地方：凡是"每种运动方式占一行数据"的结构，包括运动方式表、枚举、`format_request()` 这类遍历代码，都是加数据不改逻辑。

麻烦的地方：现在的模型默认"四轮目标速度 = ±1 的方向系数 × 统一速度"。如果新方式不符合这个假设，比如某个轮子只想转到 70% 的速度，或者需要额外参数（像"spin 45 度"这种），光加表项就不够了，得把 `MotionDef` 从系数数组升级成每轮独立的比例数组甚至函数指针，输出格式也得跟着改。这是这个设计最大的局限。

如果继续完善，我最想先做的是把 `tests/` 下的测试脚本整理成"输入文件 + 期望输出文件"的对比测试，每次改动后一键跑完，防止加新功能时把原来的行为改坏了。
