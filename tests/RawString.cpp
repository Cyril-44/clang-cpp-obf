// RawString.cpp — 原始字符串在文本级 pass 中的边界
//
// 两处曾把字符串内容静默改写（产物照样能编译，只是结果不对）：
//   * 版式压缩对原始字符串原样旁路，但定位靠 "R\"(" … ")\"" 的字面模式：带
//     自定义分隔符的 R"x(...)x" 认不出来，内容被当成代码压缩（空白、换行被删）；
//     普通字符串里的 R"( 和 )" 也会让旁路区间错位；
//   * 宏混淆把含换行的 token 原样透传，其后不补换行，紧跟的 #define 被粘在
//     同一行上，产物无法编译。
// 各模式的产物必须编译通过、运行结果与原程序一致、预处理结果与原文件一致。
#include <iostream>
#include <string>

const char *js = R"(line one
line two /* not a comment */
line three)";

const char *py = R"x(
    print("hi")
)x";

/* 自定义分隔符里嵌着默认形式的 R"( … )"：字面模式会在内层的 )" 处提前结束 */
const char *tag = R"tag(  R"( nested )"(  )tag";

const char *quoted = "then )\" and R\"( are just text";

const wchar_t *wide = LR"(wide /* raw */)";
const char8_t *utf8 = u8R"u8(  // not a comment
)u8";

int main()
{
    using namespace std::string_literals;
    auto suffixed = R"(a  b)"s;
    std::string a(js), b(py), c(tag), d(quoted);
    std::u8string e(utf8);
    std::wstring w(wide);

    std::cout << a.size() << ' ' << a.find("/*") << ' ' << a[8] << ' ' << a[9] << '\n'
              << b.size() << ' ' << b[0] << ' ' << b[1] << ' ' << b[2] << ' ' << b[3] << ' ' << b[4] << '\n'
              << c.size() << ' ' << c.find("R\"(") << ' ' << c.find(")\"(") << ' ' << c.find(")(  ") << '\n'
              << d.size() << ' ' << d.find("R\"(") << ' ' << d.find(")\"") << '\n'
              << w.size() << ' ' << (w[6] == L'/') << ' ' << (w[15] == L'*') << '\n'
              << e.size() << ' ' << e.find(u8"//") << ' ' << e.find(u8'\n') << '\n'
              << suffixed.size() << ' ' << suffixed.find("  ") << ' ' << suffixed[4] << '\n'
              << (a + b + c + d).size() + e.size() << '\n';
    return 0;
}
