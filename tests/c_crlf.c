// c_crlf.c — CRLF 行尾下的词法边界
//
// !! 本文件使用 CRLF 行尾，编辑时请勿转换（checker.c 也是 CRLF）。
//
// 行尾是 \r\n 时，反斜杠续行是 "\" + CR + LF。把续行只认成 "\" + LF 的实现会：
//   * 把以 \ 结尾的 // 注释在 CR 处截断——编译器认为下一行仍是注释，产物却把它
//     当成了代码：下面的 hidden_return 一旦“复活”，返回值就从 10 变成 99；
//   * 拼错跨行的 #define 与字符串字面量。
#include <stdio.h>
#define LONG_MACRO(a, b) \
    ((a) * 10 + (b))
#define SPANNING_COMMENT /* 块注释跨行：指令要一直延伸到注释结束 \
*/ 42
static const char *spliced = "ab\
cd";
static int hidden_return(int x)
{
    int y = x + 7;
    // 注释以反斜杠结尾：下一行仍是注释 \
    return 99;
    return y;
}
int main(void)
{
    /* 块注释 */ int v = /* 行内 */ 3; // 行注释
    int w = LONG_MACRO(v,
                       2);
    printf("%d %d %s %d %d\n", w, SPANNING_COMMENT, spliced, hidden_return(v),
           v == 3 ? 1 : 0);
    return 0;
}
