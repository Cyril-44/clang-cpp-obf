// obf/Compress.hpp — 源码版式压缩器（随 -compress 自动启用）
//
// 移植自用户提供的 JavaScript 参考实现，算法保持一致：
//   compressSingle:
//     processTrigraph   三字符组归一（??= -> # 等）
//     processLineBreak  反斜杠续行拼接（去掉 '\' 与换行）
//     processMultilineCommentAndReplaceMark
//                       块注释 -> 单个空格；%: -> #
//     逐行词法（字符串感知，含转义）划分 token；
//     行间/行内按 needSpace 最小间隔拼接，注释丢弃，
//     '#' 指令行独立成行并保持 #define 名称分隔
//   compressPass: R"(...)" 原始字符串原样旁路，其余逐段压缩
//   compressSource: compressPass 执行两遍（第一遍拼接后重排）
//
// 与参考实现的差异（仅限明显笔误的修正）：
//   * check 中 ' + '（含空格的三字符串，恒 false）按语义还原为 '+'
//   * 未闭合的 /* 吞到文件尾（参考实现的 indexOf+2==-1 分支不可达）
#pragma once

#include <map>
#include <string>
#include <vector>

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
    char stringBegin = 0;
    bool esc = false, inString = false;
    size_t lst = 0;
    for (size_t i = 0; i < str.size(); ++i) {
        if (inString) {
            if (!esc && str[i] == stringBegin) inString = false;
        } else if (str[i] == '\'' || str[i] == '"') {
            inString = true;
            stringBegin = str[i];
        } else if (i + 1 < str.size()) {
            if (str[i] == '/' && str[i + 1] == '*') {
                ret += str.substr(lst, i - lst) + " ";
                size_t e = str.find("*/", i + 2);
                if (e == std::string::npos) { // 未闭合：吞到文件尾
                    lst = str.size();
                    i = str.size();
                    break;
                }
                i = e + 2;
                lst = i;
            } else if (str[i] == '%' && str[i + 1] == ':') {
                ret += str.substr(lst, i - lst) + "#";
                i += 2;
                lst = i;
            }
        }
        esc = i < str.size() && str[i] == '\\';
    }
    ret += str.substr(lst == std::string::npos ? 0 : lst);
    return ret;
}

inline std::string compressSingle(const std::string &input) {
    std::string str = processTrigraph(input);
    str = processLineBreak(str);
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
            char stringBegin = 0;
            bool esc = false, inString = false;
            while (i != line.size() && (inString || !isSpaceCh(line[i]))) {
                if (inString) {
                    if (!esc && line[i] == stringBegin) inString = false;
                } else if (line[i] == '"' || line[i] == '\'') {
                    inString = true;
                    stringBegin = line[i];
                } else if (line[i] == '/' && i != line.size() - 1) {
                    if (line[i + 1] == '/') { // 行注释：注释本身记为一个 token
                        rig.push_back(i - 1);
                        lef.push_back(i);
                        rig.push_back(line.size() - 1);
                        lexDone = true;
                        break;
                    }
                }
                ++i;
                if (i == line.size()) break;
                esc = line[i] == '\\';
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

// R"(...)" 原始字符串旁路，其余逐段压缩
inline std::string compressPass(const std::string &str) {
    const std::string patternS = "R\"(";
    const std::string patternT = ")\"";
    std::string ret;
    size_t lst = 0;
    while (lst != str.size()) {
        size_t ind = str.find(patternS, lst);
        if (ind == std::string::npos) break;
        ret += compressSingle(str.substr(lst, ind - lst));
        size_t rig = str.find(patternT, ind + patternS.size());
        if (rig == std::string::npos) {
            ret += str.substr(ind);
            lst = str.size();
            break;
        }
        lst = rig + patternT.size();
        ret += str.substr(ind, lst - ind);
    }
    ret += compressSingle(str.substr(lst));
    return ret;
}

// 参考实现执行两遍：第一遍拼接续行后整体重排
inline std::string compressSource(const std::string &src) {
    return compressPass(compressPass(src));
}

} // namespace obf
