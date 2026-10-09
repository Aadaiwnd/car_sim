/*
 * main.c —— 命令解析与人机交互层
 *
 * 职责：
 *   1. 从标准输入逐行读取命令；
 *   2. 校验参数（缺参数、参数非法、速度越界、多余参数、未知命令）；
 *   3. 调用 car.h 提供的接口改变小车状态；
 *   4. 按题目规定的格式打印 status，其余提示文字格式自行设计。
 *
 * 说明：提示信息使用 ASCII 英文，避免不同终端（GBK/UTF-8）下中文乱码；
 *       status 输出全部为 ASCII，保证自动测试不受编码影响。
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "car.h"

#define LINE_BUF_SIZE   1024
#define MAX_TOKENS      8      /* 一行命令最多识别的单词数 */

static Car g_car;   /* 全局唯一的小车实例 */

/* ------------------------------------------------------------------ */
/* 行读取与分词                                                        */
/* ------------------------------------------------------------------ */

/* 行太长时丢弃一行中剩余的字符，避免残余内容被当作下一行命令 */
static void discard_rest_of_line(int last_char)
{
    if (last_char != '\n' && last_char != EOF) {
        int c;
        while ((c = getchar()) != '\n' && c != EOF)
            ;
    }
}

/* 按空白（空格/Tab/\r/\n）把一行切分成单词，返回单词个数 */
static int split_line(char *line, char *tokens[], int max_tokens)
{
    int   count = 0;
    char *p = line;

    while (count < max_tokens) {
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
            p++;
        if (*p == '\0')
            break;
        tokens[count++] = p;
        while (*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n')
            p++;
        if (*p != '\0') {
            *p = '\0';
            p++;
        }
    }
    return count;
}

/* ------------------------------------------------------------------ */
/* 辅助函数                                                            */
/* ------------------------------------------------------------------ */

/* 把一条要求格式化为 "FORWARD 60" / "STOP" 的形式（STOP 不带速度） */
static void format_request(const MotionRequest *req, char *buf, size_t size)
{
    if (!req->valid) {
        snprintf(buf, size, "NONE");
    } else if (req->mode == MOTION_STOP) {
        snprintf(buf, size, "STOP");
    } else {
        snprintf(buf, size, "%s %d", motion_name(req->mode), req->speed);
    }
}

/* 解析速度参数：必须是 0~100 的整数，成功返回 0 */
static int parse_speed(const char *tok, int *out)
{
    char *end = NULL;
    long  v;

    if (tok == NULL)
        return -1;                       /* 缺少速度参数 */
    v = strtol(tok, &end, 10);
    if (end == tok || *end != '\0')
        return -1;                       /* 不是合法整数（如 12abc、3.5） */
    if (v < 0 || v > 100)
        return -1;                       /* 超出 0~100 */
    *out = (int)v;
    return 0;
}

/* 打印题目规定的 6 行状态（中间不得夹杂其他文字） */
static void print_status(void)
{
    char auto_text[32], manual_text[32], target_text[32];
    const MotionRequest *eff = car_effective(&g_car);

    format_request(&g_car.auto_req,   auto_text,   sizeof(auto_text));
    format_request(&g_car.manual_req, manual_text, sizeof(manual_text));
    format_request(eff,               target_text, sizeof(target_text));

    printf("STATE %s\n",   car_state_name(g_car.state));
    printf("CONTROL %s\n", car_control_name(&g_car));
    printf("AUTO %s\n",    auto_text);
    printf("MANUAL %s\n",  manual_text);
    printf("TARGET %s\n",  target_text);
    printf("WHEELS %d %d %d %d\n",
           g_car.wheel[WHEEL_LF], g_car.wheel[WHEEL_RF],
           g_car.wheel[WHEEL_LR], g_car.wheel[WHEEL_RR]);
}

/* ------------------------------------------------------------------ */
/* auto / manual 命令的公共解析（两者参数格式完全一致）                  */
/* tokens[0] 是命令名，n 是单词总数；返回前只负责校验与保存，不打印状态  */
/* ------------------------------------------------------------------ */
static void handle_motion_command(const char *cmd_name, int is_manual,
                                  char *tokens[], int n)
{
    int mode;
    int speed = 0;

    if (n < 2) {
        printf("[error] %s: missing motion mode\n", cmd_name);
        return;
    }

    /* 查运动方式表：forward/backward/left/right/stop */
    mode = motion_find(tokens[1]);
    if (mode < 0) {
        printf("[error] %s: unknown motion mode '%s'\n", cmd_name, tokens[1]);
        return;
    }

    if (mode != MOTION_STOP) {
        if (parse_speed(n >= 3 ? tokens[2] : NULL, &speed) != 0) {
            if (n < 3)
                printf("[error] %s: missing speed parameter\n", cmd_name);
            else
                printf("[error] %s: invalid speed '%s' (must be 0~100)\n",
                       cmd_name, tokens[2]);
            return;
        }
    }

    if (n > (mode == MOTION_STOP ? 2 : 3)) {
        printf("[error] %s: too many parameters\n", cmd_name);
        return;
    }

    /* 参数合法后，再检查系统状态：只有 RUNNING 才能执行普通运动命令 */
    if (g_car.state != CAR_RUNNING) {
        printf("[error] %s: motion commands are only allowed in RUNNING state\n",
               cmd_name);
        return;
    }

    if (is_manual)
        car_set_manual(&g_car, (MotionMode)mode, speed);
    else
        car_set_auto(&g_car, (MotionMode)mode, speed);

    printf("[ok] %s requirement saved: %s\n", cmd_name,
           mode == MOTION_STOP ? "STOP" : motion_name((MotionMode)mode));
}

/* ------------------------------------------------------------------ */
/* 主循环                                                              */
/* ------------------------------------------------------------------ */
int main(void)
{
    char line[LINE_BUF_SIZE];

    car_init(&g_car);
    printf("car simulator ready. type 'help' for commands.\n");

    while (fgets(line, sizeof(line), stdin) != NULL) {
        char *tokens[MAX_TOKENS];
        int   n;

        {
            size_t len = strlen(line);
            discard_rest_of_line(len > 0 ? line[len - 1] : EOF);
        }

        n = split_line(line, tokens, MAX_TOKENS);
        if (n == 0)
            continue;                    /* 空行直接忽略 */

        if (strcmp(tokens[0], "quit") == 0 || strcmp(tokens[0], "exit") == 0) {
            printf("[ok] bye\n");
            break;
        } else if (strcmp(tokens[0], "start") == 0) {
            if (n > 1) {
                printf("[error] start: too many parameters\n");
            } else if (car_start(&g_car) == 0) {
                printf("[ok] car is now RUNNING\n");
            } else {
                printf("[error] start is only allowed in IDLE state\n");
            }
        } else if (strcmp(tokens[0], "auto") == 0) {
            handle_motion_command("auto", 0, tokens, n);
        } else if (strcmp(tokens[0], "manual") == 0) {
            if (n >= 2 && strcmp(tokens[1], "release") == 0) {
                if (n > 2) {
                    printf("[error] manual release: too many parameters\n");
                } else if (car_release_manual(&g_car) == 0) {
                    printf("[ok] manual control released\n");
                } else {
                    printf("[error] manual release: no manual control in effect\n");
                }
            } else {
                handle_motion_command("manual", 1, tokens, n);
            }
        } else if (strcmp(tokens[0], "step") == 0) {
            if (n > 1) {
                printf("[error] step: too many parameters\n");
            } else if (car_step(&g_car) == 0) {
                printf("[ok] step done: %d %d %d %d\n",
                       g_car.wheel[WHEEL_LF], g_car.wheel[WHEEL_RF],
                       g_car.wheel[WHEEL_LR], g_car.wheel[WHEEL_RR]);
            } else {
                printf("[error] step is only allowed in RUNNING state\n");
            }
        } else if (strcmp(tokens[0], "emergency") == 0) {
            car_emergency(&g_car);
            printf("[ok] EMERGENCY! wheels forced to 0\n");
        } else if (strcmp(tokens[0], "reset") == 0) {
            car_reset(&g_car);
            printf("[ok] system reset to IDLE\n");
        } else if (strcmp(tokens[0], "status") == 0) {
            if (n > 1)
                printf("[error] status: too many parameters\n");
            else
                print_status();
        } else if (strcmp(tokens[0], "help") == 0) {
            printf("commands: start | auto <mode> <speed> | manual <mode> <speed> | "
                   "manual release | auto stop | manual stop | step | emergency | "
                   "reset | status | quit\n"
                   "motion modes: forward | backward | left | right | stop, speed range 0~100\n");
        } else {
            printf("[error] unknown command '%s' (type 'help' for usage)\n", tokens[0]);
        }
    }

    return 0;   /* 正常退出 */
}
