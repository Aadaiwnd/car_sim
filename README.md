# 四轮小车控制模拟 —— C语言开放工程题（25分）

## 文件结构

```
car_sim/
├── main.c      命令解析与人机交互层（读命令、校验参数、分发、打印 status）
├── car.h       小车核心模型接口（数据结构 + 函数声明）
├── car.c       小车核心模型实现（状态机、控制优先级、速度推进、运动方式表）
├── REVIEW.md   Review 文档（回答题目要求的 4 个问题）
├── build.bat   Windows + MSVC 一键编译脚本
├── Makefile    gcc 编译脚本（Linux / MinGW）
└── tests/      测试用例
    ├── test1_example.txt           题目示例（含期望输出注释）
    ├── test2_switch.txt            自动/手动切换、stop、release 组合
    ├── test3_emergency_reset.txt   急停与复位流程
    └── test4_errors.txt            全部错误输入场景 + 速度过零验证
```

## 编译

方式一（Windows + Visual Studio，双击或命令行执行）：

```
build.bat
```

方式二（gcc / MinGW / Linux）：

```
make
```

## 运行

```
./car < test.txt        （Windows 下为 car.exe < test.txt）
./car                   交互式输入，输入 quit 退出
```

## 命令一览

| 命令 | 说明 |
|------|------|
| `start` | 待机 → 运行（仅待机状态可用） |
| `auto forward\|backward\|left\|right <0~100>` | 设置自动控制要求（仅运行状态） |
| `auto stop` | 自动要求置为停止 |
| `manual forward\|backward\|left\|right <0~100>` | 设置手动控制要求（优先于自动） |
| `manual stop` | 手动要求置为停止（手动仍生效） |
| `manual release` | 释放手动，恢复自动要求 |
| `step` | 四轮实际速度向目标各最多变化 20（仅运行状态） |
| `emergency` | 立即急停，四轮实际速度瞬间归零 |
| `reset` | 回到待机，清空全部控制要求 |
| `status` | 严格按题目格式输出 6 行状态 |
| `quit` / `exit` | 正常退出（EOF 同样正常退出） |

## 设计说明

- 解析层（main.c）与逻辑层（car.c）分离，逻辑层不做任何输入输出；
- 运动方式采用**查表驱动**（`g_motion_table`），新增运动方式只需加一行表项；
- 手动/自动优先级集中在 `car_effective()` 一处判断；
- 提示信息使用 ASCII 英文，避免 Windows GBK 终端乱码；status 输出为纯 ASCII，不受编码影响。
