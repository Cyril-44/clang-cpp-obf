// obf/Compress.hpp — 源码版式压缩器（随 -compress 自动启用）
//
// 移植自用户提供的 JavaScript 参考实现，算法保持一致：
//   compressSource:
//     processTrigraph   三字符组归一（??= -> # 等）——仅当编译器启用三字符组
//   compressSingle:
//     processLineBreak  反斜杠续行拼接（去掉 '\' 与换行）
//     processMultilineCommentAndReplaceMark
//                       块注释 -> 单个空格；行注释删除；%: -> #
//     逐行词法划分 token（空白分隔，字符串/字符字面量含转义不切分）；
//     行间/行内按 needSpace 最小间隔拼接，注释丢弃，
//     '#' 指令行独立成行并保持 #define 名称分隔
//   compressPass: 原始字符串（含自定义分隔符与前缀）原样旁路，其余逐段压缩
//   compressSource: compressPass 执行两遍（第一遍拼接后重排）
//
// 与参考实现的差异：
//   * check 中 ' + '（含空格的三字符串，恒 false）按语义还原为 '+'
//   * 未闭合的 /* 吞到文件尾（参考实现的 indexOf+2==-1 分支不可达）
//   * 字符串/字符字面量统一经 lexLiteralEndInLine 整体跳过（参考实现自带的
//     引号状态跟踪对 '\\'、'\'' 这类转义的收尾判断有误，会把后面的代码
//     误判成字符串内/外，进而把字符串里的 "/*" 当成注释开头整段删除）；
//     行注释改在全局阶段按逻辑行删除，注释里的撇号不再干扰引号状态
//   * 三字符组归一以编译选项为准（见 compressSource）
#pragma once

#include <map>
#include <string>
#include <vector>

#include "Support.hpp"

namespace obf {

inline bool isSpaceCh(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' ||
           c == '\f';
}
inline bool isIdentPartCh(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '$' || c == '_';
}

// 相邻 token 是否必须保留一个空格：
// 标识符字符相邻、同号 +/- 相邻（防 a+ +b 粘成 a++b）、/ 与 * 相邻（防成注释）
inline bool needSpace(char a, char b) {
    return (isIdentPartCh(a) && isIdentPartCh(b)) ||
           ((a == '+' || a == '-') && a == b) || (a == '/' && b == '*');
}

// 在 token 内（空白之前）查找 ch；找不到返回 -1
inline long safeIndexOf(const std::string &s, char ch, size_t ind) {
    while (ind != s.size() && !isSpaceCh(s[ind])) {
        if (s[ind] == ch) return (long)ind;
        ++ind;
    }
    return -1;
}

inline std::string processTrigraph(const std::string &str) {
    static const std::map<char, char> trigraphs = {
        {'=', '#'}, {'/', '\\'}, {'\'', '^'}, {'(', '['}, {')', ']'},
        {'<', '{'}, {'>', '}'},  {'!', '|'},  {'-', '~'},
    };
    std::string s = str;
    for (const auto &kv : trigraphs) {
        const std::string pattern = std::string("??") + kv.first;
        std::string ret;
        size_t lst = 0;
        while (lst != s.size()) {
            size_t ind = s.find(pattern, lst);
            if (ind == std::string::npos) break;
            ret += s.substr(lst, ind - lst) + kv.second;
            lst = ind + pattern.size();
        }
        ret += s.substr(lst);
        s = ret;
    }
    return s;
}

inline std::string processLineBreak(const std::string &str) {
    // 行尾（忽略尾随空白）为 '\' 时去掉 '\' 并与下一行拼接
    std::string ret;
    size_t lst = 0;
    while (lst <= str.size()) {
        size_t nl = str.find('\n', lst);
        std::string line = str.substr(lst, nl == std::string::npos
                                               ? std::string::npos
                                               : nl - lst);
        long j = (long)line.size();
        while ((--j) >= 0 && isSpaceCh(line[j])) {}
        if (j >= 0 && line[j] == '\\')
            ret += line.substr(0, (size_t)j);
        else
            ret += line + '\n';
        if (nl == std::string::npos) break;
        lst = nl + 1;
    }
    if (!ret.empty() && ret.back() == '\n') ret.pop_back();
    return ret;
}

inline std::string processMultilineCommentAndReplaceMark(
    const std::string &str) {
    std::string ret;
    size_t lst = 0;
    for (size_t i = 0; i < str.size(); ++i) {
        char c = str[i];
        if (c == '/' && i + 1 < str.size() && str[i + 1] == '*') {
            ret += str.substr(lst, i - lst) + " ";
            size_t e = str.find("*/", i + 2);
            if (e == std::string::npos) { // 未闭合：吞到文件尾
                lst = str.size();
                break;
            }
            i = e + 1;
            lst = e + 2;
        } else if (c == '/' && i + 1 < str.size() && str[i + 1] == '/') {
            // 行注释连同其续行一并删除；行尾换行保留，行结构不变
            size_t e = lexLogicalLineEnd(str, i + 2);
            ret += str.substr(lst, i - lst);
            if (e == std::string::npos) {
                lst = str.size();
                break;
            }
            i = e;
            lst = e;
        } else if (c == '%' && i + 1 < str.size() && str[i + 1] == ':') {
            ret += str.substr(lst, i - lst) + "#";
            ++i;
            lst = i + 1;
        } else if (size_t e = lexLiteralEndInLine(str, i)) {
            i = e - 1; // 字面量（含前缀）整体跳过：其内的 /* // %: 不参与判定
        }
    }
    ret += str.substr(lst);
    return ret;
}

inline std::string compressSingle(const std::string &input) {
    std::string str = processLineBreak(input);
    str = processMultilineCommentAndReplaceMark(str);

    // 按 '\n' 拆行
    std::vector<std::string> arr;
    for (size_t p = 0;;) {
        size_t nl = str.find('\n', p);
        arr.push_back(str.substr(p, nl == std::string::npos
                                        ? std::string::npos
                                        : nl - p));
        if (nl == std::string::npos) break;
        p = nl + 1;
    }

    std::string ret;
    bool forceNewline = true;
    char last = '\0';
    for (size_t w = 0; w < arr.size(); ++w) {
        std::string &line = arr[w];
        if (line.empty()) continue;
        size_t i = 0;
        std::vector<size_t> lef, rig;
        bool lexDone = false;
        while (!lexDone) {
            while (i != line.size() && isSpaceCh(line[i])) ++i;
            if (i == line.size()) break;
            lef.push_back(i);
            while (i != line.size() && !isSpaceCh(line[i])) {
                if (line[i] == '/' && i != line.size() - 1 &&
                    line[i + 1] == '/') { // 行注释：注释本身记为一个 token
                    rig.push_back(i - 1);
                    lef.push_back(i);
                    rig.push_back(line.size() - 1);
                    lexDone = true;
                    break;
                }
                // 字面量（含 u8/R 等前缀；'\''、'\\' 等；1'000'000 的数字
                // 分隔符）整体并入当前 token：其内空白不切分、`//` 不算注释
                size_t e = lexLiteralEndInLine(line, i);
                if (e) {
                    i = e;
                    continue;
                }
                ++i;
            }
            if (!lexDone) rig.push_back(i - 1);
        }

        size_t n = lef.size();
        if (!n) continue;
        bool originForceNewline = forceNewline;
        char originLast = last;
        if (last != '\0') {
            if (forceNewline) ret += '\n';
            else if (needSpace(last, line[lef[0]])) ret += ' ';
        }
        last = line[rig[n - 1]];
        size_t j = 0;
        if (line[lef[0]] == '#') {
            if (!forceNewline) ret += '\n';
            forceNewline = true;
            // '#' 独占行尾的罕见形态：与下一 token 粘连为 "#xxx"
            if (lef[0] + 1 >= line.size() && n > 1) {
                j = 1;
                line[--lef[j]] = '#';
            }
            ret += line.substr(lef[j], rig[j] - lef[j] + 1);
            // "#define NAME（非函数式）"：名字与替换体之间必须分隔。
            // 注意：++j 必须独立成句——与 rig[j]/lef[j] 同处一个实参表
            // 时求值顺序未指定，会截错长度
            if (j + 1 < n && lef[j] + 1 < line.size() &&
                line[lef[j] + 1] == 'd' &&
                j + 1 < n && safeIndexOf(line, '(', lef[j + 1]) == -1) {
                ++j;
                ret += ' ';
                ret += line.substr(lef[j], rig[j] - lef[j] + 1);
                if (j + 1 < n) {
                    ++j;
                    ret += ' ';
                    ret += line.substr(lef[j], rig[j] - lef[j] + 1);
                }
            }
            ++j;
        } else {
            forceNewline = false;
        }
        for (; j < n;) {
            if (lef[j] + 2 <= line.size() &&
                line.compare(lef[j], 2, "//") == 0) {
                if (j == 0) forceNewline = originForceNewline;
                break; // 注释 token：丢弃
            }
            if (j && needSpace(line[rig[j - 1]], line[lef[j]])) ret += ' ';
            // rig >= lef 为正常 token（长度至少 1）；rig == lef-1 是
            // "注释前空 token"的退化形态，长度按 0 处理
            size_t len = rig[j] >= lef[j] ? rig[j] - lef[j] + 1 : 0;
            ret += line.substr(lef[j], len);
            ++j;
        }
        last = j ? line[rig[j - 1]] : originLast;
    }
    return ret;
}

// 原始字符串（R"(...)"、R"delim(...)delim"，含 u8/u/U/L 前缀）原样旁路，其余逐段
// 压缩。原始字符串的定位交给分词器——按 "R\"(" … ")\"" 的字面模式查找时，带
// 自定义分隔符的 R"x(...)x" 认不出来，其内容会被当成代码压缩（空白、换行被删，
// 字符串被静默改写，而产物照样能编译）。
inline std::string compressPass(const std::string &str) {
    std::string ret;
    size_t lst = 0;
    for (const Token &t : lexSource(str)) {
        if (t.kind != TokKind::Str) continue;
        size_t q = t.text.find('"'); // 引号前紧挨着前缀 R 的才是原始字符串
        if (q == std::string::npos || q == 0 || t.text[q - 1] != 'R') continue;
        ret += compressSingle(str.substr(lst, t.begin - lst));
        ret += t.text;
        lst = t.begin + t.text.size();
    }
    ret += compressSingle(str.substr(lst));
    return ret;
}

// 参考实现执行两遍：第一遍拼接续行后整体重排。
// trigraphs：仅当编译器启用三字符组时才做归一——严格 ISO 模式下 ??= 在翻译
// 阶段 1 就被换成 #，归一与编译器行为一致；gnu 模式与 C++17 起默认关闭，
// 此时 "??!" 就是三个普通字符，改写会改变程序。
inline std::string compressSource(const std::string &src, bool trigraphs) {
    return compressPass(compressPass(trigraphs ? processTrigraph(src) : src));
}

} // namespace obf
