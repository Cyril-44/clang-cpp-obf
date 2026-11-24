// c_macros.c — C 语言下的重命名：宏密集代码
//
// checker.c（单文件 QuickJS-NG）暴露出的三类 C 语言问题，这里各留一个最小形态：
//   1. 宏实参里被宏体当作“裸 token”使用的名字——offsetof(T, member) /
//      container_of(p, T, member) 的成员名、#x 字符串化的名字——在 AST 中没有
//      对应节点。成员声明被改名而宏体里的引用无法改写，产物编译失败；
//   2. C 模式下 isExternC() 对所有外部链接的函数/变量恒为真：声明处被豁免、
//      引用处照改，结果是“调用了未声明的函数”；
//   3. 函数式宏（含变参 ...）的调用与定义：版式压缩把 ... 拆开、宏混淆把
//      实参的括号藏进别的宏，都会让调用不成立。
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct Link {
    struct Link *next;
    struct Link *prev;
} Link;

typedef struct Block {
    size_t size;
    Link link;
    unsigned char user_data[8];
} Block;

#define container_of(ptr, type, member) \
    ((type *)((uint8_t *)(ptr) - offsetof(type, member)))
#define NAME_OF(x) #x
#define CONCAT(a, b) a ## b
#define FIELD_OFFSET(type, member) offsetof(type, member)
#define LOG(fmt, ...) log_line(__FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define ENTRY(name, fn) { name, fn }

typedef struct {
    const char *name;
    int (*fn)(int, int);
} Entry;

/* 非 static 的函数与全局变量：声明、定义、引用必须一致改名 */
int global_counter;
int compute(int a, int b);
static int multiply(int a, int b);

static int add_impl(int a, int b) { return a + b; }
int compute(int a, int b) { return add_impl(a, b); }
static int multiply(int a, int b) { return a * b; }

static int log_line(const char *file, int line, const char *fmt, ...)
{
    va_list ap;
    int n;
    (void)file;
    (void)line;
    va_start(ap, fmt);
    n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    return n;
}

static Block *block_of_link(Link *lnk)
{
    /* 成员名 link 只作为裸 token 出现在宏实参里 */
    return container_of(lnk, Block, link);
}

static Block *block_of_data(void *p)
{
    return container_of(p, Block, user_data);
}

static const Entry table[] = {
    ENTRY("add", compute),
    ENTRY("mul", multiply),
};

int main(void)
{
    Block blk;
    Link *lnk = &blk.link;
    int pasted, total = 0;
    size_t i;

    memset(&blk, 0, sizeof(blk));
    blk.user_data[0] = 7;

    /* 粘接出来的名字是真实声明：引用必须跟着声明一起改名或一起保持原名 */
    pasted = CONCAT(glo, bal_counter);
    global_counter = 5;

    if (block_of_link(lnk) != &blk || block_of_data(blk.user_data) != &blk)
        return 1;
    if (FIELD_OFFSET(Block, user_data) != offsetof(Block, user_data))
        return 2;
    if (strcmp(NAME_OF(global_counter), "global_counter") != 0)
        return 3;
    if (strcmp(table[0].name, "add") != 0 || table[1].fn(3, 4) != 12)
        return 4;

    for (i = 0; i < sizeof(table) / sizeof(table[0]); i++)
        total += table[i].fn(2, 3);
    total += blk.user_data[0] + pasted + global_counter;
    total += LOG("%d %s", 1, "x") == 3 ? 1 : 0;

    printf("%d\n", total);
    return 0;
}
