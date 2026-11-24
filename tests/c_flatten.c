// c_flatten.c — C 语言下的控制流扁平化（不包含 <stdbool.h>）
//
// 两件事：
//   1. bool / false / true 在 C23 之前只是 <stdbool.h> 里的宏。扁平化生成的
//      `bool done = false;` 在没有包含它的 C 文件里直接编译失败；
//   2. 返回类型非 void、且所有路径都以 return 收尾的函数，扁平化后分派循环的
//      default 出口实际不可达，但编译器会报 -Wreturn-type。各控制流形态
//      （无限循环、循环体内 break、内层循环的 break、switch 全覆盖、if/else
//      双分支、noreturn 调用）下，出口保护必须加得准：
//      该加的不加会告警，不该加的加了会把落空路径变成死循环。
#include <stdio.h>
#include <stdlib.h>

/* 每个函数都带两条以上非声明语句，才会被扁平化 */

static int loop_return(int n)
{
    int r = 7;
    n++;
    for (;;) {
        if (n-- <= 0)
            return n + r;
    }
}

static int loop_break(int n)      /* 循环正常退出后 return：出口保护必须加 */
{
    int r = 2;
    n++;
    for (;;) {
        if (n-- <= 0)
            break;
    }
    return 5 + r;
}

static int while_break(int n)     /* 循环正常退出后 return：出口保护必须加 */
{
    int r = 1;
    while (1) {
        if (n-- <= 0)
            break;
    }
    return r;
}

static int inner_break(int n)     /* 内层循环的 break 不属于外层循环 */
{
    int r = 1;
    n++;
    for (;;) {
        for (;;) {
            if (n-- > 0)
                break;
            else
                return 9 + r;
        }
    }
}

static int while_true(int n)
{
    int r = 3;
    n++;
    while (1) {
        if (n < 0)
            return -1 + r;
        n -= 3;
    }
}

static int do_while(int n)
{
    int r = 4;
    n++;
    do {
        if (n > 100)
            return n + r;
        n *= 2;
    } while (1);
}

static int switch_all(int n)      /* 有 default 且各分支都 return */
{
    int r = 5;
    n++;
    switch (n) {
    case 1:
    case 2:
        return 10 + r;
    default:
        return 20 + r;
    }
}

static int switch_fall(int n)     /* 有分支会落空：不得加出口保护 */
{
    int r = 6;
    switch (n) {
    case 1:
        n += 100;
    default:
        break;
    }
    return n + r;
}

static int switch_no_default(int n) /* 没有 default：语法上可能落空（运行时不会），
                                        不得加出口保护 */
{
    int r = 7;
    n &= 1;
    switch (n) {
    case 0:
        return r;
    case 1:
        return -r;
    }
}

static int if_else(int n)
{
    int r = 8;
    n++;
    if (n) {
        return 1 + r;
    } else {
        return 2 + r;
    }
}

static int if_no_else(int n)      /* 没有 else：可能落空 */
{
    int r = 9;
    if (n)
        return r;
    return n + r;
}

static int call_exit(int n)
{
    n++;
    if (n)
        return 1;
    exit(3);
}

int main(void)
{
    int v[12], i, s = 0;

    v[0] = loop_return(4);
    v[1] = loop_break(7);
    v[2] = while_break(5);
    v[3] = inner_break(3);
    v[4] = while_true(10);
    v[5] = do_while(60);
    v[6] = switch_all(1);
    v[7] = switch_fall(1);
    v[8] = if_else(0);
    v[9] = if_no_else(1);
    v[10] = call_exit(2);
    v[11] = switch_no_default(6);
    for (i = 0; i < 12; i++)
        s += v[i] * (i + 1);
    printf("%d\n", s);
    return 0;
}
