#ifndef CAR_H
#define CAR_H

/*
 * car.h —— 小车核心模型对外接口
 *
 * 小车模型只负责"状态与速度"的维护，不负责命令解析与提示信息输出，
 * 这样解析层（main.c）和逻辑层（car.c）职责分离，便于扩展和测试。
 */

#define CAR_WHEEL_COUNT 4
#define CAR_STEP_MAX    20   /* 每次 step 每个车轮最多变化的速度 */

/* 车轮下标顺序：左前、右前、左后、右后 */
enum {
    WHEEL_LF = 0,   /* left  front */
    WHEEL_RF = 1,   /* right front */
    WHEEL_LR = 2,   /* left  rear  */
    WHEEL_RR = 3    /* right rear  */
};

/* 系统状态 */
typedef enum {
    CAR_IDLE = 0,
    CAR_RUNNING,
    CAR_EMERGENCY
} CarState;

/*
 * 运动方式。新增运动方式时：
 *   1. 在此枚举中加一项（放在 MOTION_COUNT 之前）；
 *   2. 在 car.c 的 g_motion_table 表里登记名称与四个车轮的方向系数。
 */
typedef enum {
    MOTION_STOP = 0,
    MOTION_FORWARD,
    MOTION_BACKWARD,
    MOTION_LEFT,
    MOTION_RIGHT,
    MOTION_COUNT   /* 哨兵，表示运动方式总数 */
} MotionMode;

/* 一条控制要求（自动或手动） */
typedef struct {
    int        valid;   /* 0 = 不存在该要求，1 = 存在 */
    MotionMode mode;    /* 运动方式 */
    int        speed;   /* 速度参数 0~100，STOP 时不使用 */
} MotionRequest;

/* 小车整体状态 */
typedef struct {
    CarState      state;                        /* 系统状态 */
    MotionRequest auto_req;                     /* 保存的自动控制要求 */
    MotionRequest manual_req;                   /* 保存的手动控制要求 */
    int           wheel[CAR_WHEEL_COUNT];       /* 四轮实际速度 */
} Car;

/* ---------- 生命周期 ---------- */
void car_init(Car *car);                 /* 上电：IDLE，全部清零 */

/* ---------- 状态切换 ---------- */
int  car_start(Car *car);                /* IDLE -> RUNNING，成功返回 0 */
int  car_emergency(Car *car);            /* 任意状态 -> EMERGENCY，四轮立即归零 */
void car_reset(Car *car);                /* 回到 IDLE，控制要求清空，四轮为 0 */

/* ---------- 控制要求 ---------- */
int  car_set_auto(Car *car, MotionMode mode, int speed);     /* 仅 RUNNING 有效 */
int  car_set_manual(Car *car, MotionMode mode, int speed);   /* 仅 RUNNING 有效 */
int  car_release_manual(Car *car);       /* 释放手动控制，成功返回 0 */

/* ---------- 速度推进 ---------- */
int  car_step(Car *car);                 /* 仅 RUNNING 有效，四轮向目标最多变化 STEP_MAX */

/* ---------- 查询（不改变状态） ---------- */
const MotionRequest *car_effective(const Car *car);   /* 当前真正生效的要求（手动优先） */
void car_target_wheels(const Car *car, int out[CAR_WHEEL_COUNT]); /* 生效要求对应的四轮目标速度 */
const char *car_state_name(CarState s);               /* "IDLE"/"RUNNING"/"EMERGENCY" */
const char *car_control_name(const Car *car);         /* "NONE"/"AUTO"/"MANUAL" */

/* ---------- 运动方式表操作 ---------- */
const char *motion_name(MotionMode m);  /* 大写英文名 */
int  motion_find(const char *name);     /* 按名字查找，返回 MotionMode，找不到返回 -1 */

#endif /* CAR_H */
