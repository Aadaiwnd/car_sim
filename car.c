/*
 * car.c —— 小车核心模型实现
 *
 * 设计要点：
 *  1. 运动方式全部登记在 g_motion_table 一张表里，四轮方向用 +1/-1/0 表示，
 *     新增运动方式原则上只需加一行表项（详见 REVIEW.md 问题 4）。
 *  2. 生效规则的唯一出口是 car_effective()：手动存在则手动优先，
 *     否则看自动，否则 STOP。所有需要"当前目标"的地方都调用它，
 *     保证自动/手动切换逻辑只有一份。
 */

#include <string.h>
#include <ctype.h>
#include "car.h"

/* ------------------------------------------------------------------ */
/* 运动方式表：新增运动方式时在此登记即可                                */
/* ------------------------------------------------------------------ */
typedef struct {
    const char *name;              /* 大写英文名，用于 status 输出与查找 */
    int         sign[CAR_WHEEL_COUNT]; /* 左前/右前/左后/右后 的方向系数 */
} MotionDef;

static const MotionDef g_motion_table[MOTION_COUNT] = {
    /* name         LF   RF   LR   RR */
    { "STOP",     {  0,   0,   0,   0 } },
    { "FORWARD",  { +1,  +1,  +1,  +1 } },
    { "BACKWARD", { -1,  -1,  -1,  -1 } },
    { "LEFT",     { -1,  +1,  +1,  -1 } },
    { "RIGHT",    { +1,  -1,  -1,  +1 } },
};

/* 不存在任何要求时使用的"停止"要求 */
static const MotionRequest g_stop_request = { 1, MOTION_STOP, 0 };

/* ------------------------------------------------------------------ */
/* 生命周期                                                            */
/* ------------------------------------------------------------------ */
void car_init(Car *car)
{
    memset(car, 0, sizeof(*car));
    car->state = CAR_IDLE;
}

/* ------------------------------------------------------------------ */
/* 状态切换                                                            */
/* ------------------------------------------------------------------ */
int car_start(Car *car)
{
    if (car->state != CAR_IDLE)
        return -1;                       /* 只有待机状态才能 start */
    car->state = CAR_RUNNING;
    return 0;
}

int car_emergency(Car *car)
{
    int i;
    car->state = CAR_EMERGENCY;
    for (i = 0; i < CAR_WHEEL_COUNT; i++)
        car->wheel[i] = 0;               /* 急停：实际速度立即归零，不经过 step */
    return 0;
}

void car_reset(Car *car)
{
    car->state       = CAR_IDLE;
    car->auto_req.valid   = 0;
    car->manual_req.valid = 0;           /* 自动、手动要求一并清空 */
    memset(car->wheel, 0, sizeof(car->wheel));
}

/* ------------------------------------------------------------------ */
/* 控制要求                                                            */
/* ------------------------------------------------------------------ */
/* 普通运动命令只有运行状态才能正常执行（待机/急停下由 main.c 拦截并提示） */
static int car_state_allows_motion(const Car *car)
{
    return car->state == CAR_RUNNING;
}

int car_set_auto(Car *car, MotionMode mode, int speed)
{
    if (!car_state_allows_motion(car))
        return -1;
    car->auto_req.valid = 1;
    car->auto_req.mode  = mode;
    car->auto_req.speed = speed;
    return 0;
}

int car_set_manual(Car *car, MotionMode mode, int speed)
{
    if (!car_state_allows_motion(car))
        return -1;
    car->manual_req.valid = 1;
    car->manual_req.mode  = mode;
    car->manual_req.speed = speed;
    return 0;
}

int car_release_manual(Car *car)
{
    if (!car->manual_req.valid)
        return -1;                       /* 没有手动控制时 release 报错 */
    car->manual_req.valid = 0;
    return 0;
}

/* ------------------------------------------------------------------ */
/* 速度推进                                                            */
/* ------------------------------------------------------------------ */
int car_step(Car *car)
{
    int target[CAR_WHEEL_COUNT];
    int i;

    if (!car_state_allows_motion(car))
        return -1;                       /* 待机/急停下不允许推进速度 */

    car_target_wheels(car, target);
    for (i = 0; i < CAR_WHEEL_COUNT; i++) {
        if (car->wheel[i] < target[i]) {
            car->wheel[i] += CAR_STEP_MAX;
            if (car->wheel[i] > target[i])
                car->wheel[i] = target[i];
        } else if (car->wheel[i] > target[i]) {
            car->wheel[i] -= CAR_STEP_MAX;
            if (car->wheel[i] < target[i])
                car->wheel[i] = target[i];
        }
        /* 相等则不变。正向跨越 0 时按 -20/+20 自然经过 0，不会跳变 */
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* 查询                                                                */
/* ------------------------------------------------------------------ */
const MotionRequest *car_effective(const Car *car)
{
    if (car->manual_req.valid)
        return &car->manual_req;         /* 手动优先 */
    if (car->auto_req.valid)
        return &car->auto_req;
    return &g_stop_request;              /* 无任何要求时为 STOP */
}

void car_target_wheels(const Car *car, int out[CAR_WHEEL_COUNT])
{
    const MotionRequest *req = car_effective(car);
    const MotionDef     *def = &g_motion_table[req->mode];
    int i;

    for (i = 0; i < CAR_WHEEL_COUNT; i++)
        out[i] = def->sign[i] * req->speed;
}

const char *car_state_name(CarState s)
{
    switch (s) {
    case CAR_IDLE:      return "IDLE";
    case CAR_RUNNING:   return "RUNNING";
    case CAR_EMERGENCY: return "EMERGENCY";
    default:            return "UNKNOWN";
    }
}

const char *car_control_name(const Car *car)
{
    if (car->manual_req.valid)
        return "MANUAL";
    if (car->auto_req.valid)
        return "AUTO";
    return "NONE";
}

/* ------------------------------------------------------------------ */
/* 运动方式表操作                                                      */
/* ------------------------------------------------------------------ */
const char *motion_name(MotionMode m)
{
    if (m < 0 || m >= MOTION_COUNT)
        return "UNKNOWN";
    return g_motion_table[m].name;
}

/* 大小写不敏感查找，便于兼容各种输入习惯 */
int motion_find(const char *name)
{
    int i, j;
    for (i = 0; i < MOTION_COUNT; i++) {
        const char *a = g_motion_table[i].name;
        for (j = 0; a[j] && name[j]; j++) {
            if (toupper((unsigned char)a[j]) != toupper((unsigned char)name[j]))
                break;
        }
        if (a[j] == '\0' && name[j] == '\0')
            return i;
    }
    return -1;
}
