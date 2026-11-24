// c_literals.c — 文本级 pass（注释清除 / 版式压缩）的字面量边界
//
// 版式压缩器自带的引号状态跟踪对 '\\'、'\'' 这类转义的收尾判断有误：读到
// 'e\\' 之类含反斜杠的字面量之后，会把后面的文本误判成“仍在字符串内”，再把
// 字符串里的 "/*" 当成注释开头，一直删到下一个 "*/"——中间的代码被静默删除，
// 产物照样能编译，只是行为变了。
//
// 下面的 live_* 函数夹在这类字面量之间。产物必须编译通过、运行结果正确，
// 且预处理结果（去空白）与原文件一致。
#include <stdio.h>
#include <string.h>

static int live_a(void) { return 1; }

/* 反斜杠、单双引号的字符字面量：转义的收尾必须判对 */
static int backslash(void)
{
    return ('\\' == 92 && '\'' == 39 && '\"' == 34 && '\0' == 0 &&
            '\n' == 10 && '\x41' == 'A' && '\101' == 'A' && '?' == '?')
               ? 2
               : 0;
}

static int live_b(void) { return 3; }

/* 字符串里的注释起始符与二合/三字符组序列：都不是注释，也不能被“归一” */
static const char *open_comment_in_string = "a /* not a comment";
static int live_c(void) { return 4; }
static const char *line_comment_in_string = "x // y";
static const char *digraph_like = "%:";
static const char *trigraph_like = "what??! ??= ??/ ok";
static int live_d(void) { return 5; }
static const char *close_comment_in_string = "*/";
static int live_e(void) { return 6; }

/* shell 引号转义：字符串里同时有单引号、双反斜杠和连续引号 */
static const char *shell_quote = "'\\''";
static int live_f(void) { return 7; }

static int live_g(void) { return 8; }

int main(void)
{
    /* 块注释里有撇号和引号；行注释里有未配对的撇号 */
    /* don't "quote" me on this: /* 嵌套的注释起始符也在注释里 */
    int sum = live_a() + backslash() + live_b() + live_c() + live_d() +
              live_e() + live_f() + live_g();
    size_t len = strlen(open_comment_in_string) + strlen(line_comment_in_string) +
                 strlen(digraph_like) + strlen(trigraph_like) +
                 strlen(close_comment_in_string) + strlen(shell_quote);
    // it's a line comment with an apostrophe and "quotes"
    printf("%d %d %d\n", sum, (int)len, (int)strlen("ab\ncd\t\"e\""));
    return 0;
}
