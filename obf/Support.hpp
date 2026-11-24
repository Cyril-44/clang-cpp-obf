// obf/Support.hpp — 通用基础设施：C++ 源码分词器、标识符收集、名字生成器
//
// 分词器同时服务两个用途：
//   1. RenamePass：收集源文件中出现的所有标识符（含指令行内的），作为新名字的
//      禁用集合，保证重命名绝不会与任何残留标识符冲突；
//   2. MacroObfuscator：把源码拆成 token 流以生成 #define 树。
#pragma once

#include <algorithm>
#include <cctype>
#include <cstring>
#include <map>
#include <random>
#include <set>
#include <string>
#include <vector>

namespace obf {

inline bool isIdentStart(unsigned char c) {
    return std::isalpha(c) || c == '_' || c == '$';
}
inline bool isIdentCont(unsigned char c) {
    return std::isalnum(c) || c == '_' || c == '$';
}

// C++20 全部关键字 + 严格上下文关键字（作为新名字一律禁用）
inline const std::set<std::string> &cppKeywords() {
    static const std::set<std::string> K = {
        "alignas", "alignof", "and", "and_eq", "asm", "auto", "bitand",
        "bitor", "bool", "break", "case", "catch", "char", "char8_t",
        "char16_t", "char32_t", "class", "compl", "concept", "const",
        "consteval", "constexpr", "constinit", "const_cast", "continue",
        "co_await", "co_return", "co_yield", "decltype", "default", "delete",
        "do", "double", "dynamic_cast", "else", "enum", "explicit", "export",
        "extern", "false", "float", "for", "friend", "goto", "if", "inline",
        "int", "long", "mutable", "namespace", "new", "noexcept", "not",
        "not_eq", "nullptr", "operator", "or", "or_eq", "private",
        "protected", "public",
        "register", "reinterpret_cast", "requires", "return", "short",
        "signed", "sizeof", "static", "static_assert", "static_cast",
        "struct", "switch", "template", "this", "thread_local", "throw",
        "true", "try", "typedef", "typeid", "typename", "union", "unsigned",
        "using", "virtual", "void", "volatile", "wchar_t", "while", "xor",
        "xor_eq",
        // 上下文关键字，同样禁用以免歧义
        "final", "override",
    };
    return K;
}

// 是否为保留标识符（含双下划线、或下划线后跟大写/再跟下划线开头）
inline bool isReservedName(const std::string &s) {
    if (s.find("__") != std::string::npos) return true;
    if (s.size() >= 2 && s[0] == '_' && std::isupper((unsigned char)s[1]))
        return true;
    if (s == "_") return false; // 单个下划线允许
    return false;
}

// ============================ 源码分词器 ============================
// 比旧版 def_obf.py 修正了以下致命缺陷：
//   * 数字：pp-number 规则（0x1FULL、1e-9、1'000'000 等），不再把 "1+2" 吞成一个 token
//   * 运算符：补上 :: ... ->* .* <=> 及系字符组，保证多字符运算符不被空格拆散
//   * <:: 特判为 "<" "::" 而非系字符 "<:"
//   * 字符串：raw string（R"delim(...)delim"）、编码前缀（u8/u/U/L 及其 R 组合）、
//     用户定义字面量后缀（"s"_x、'c'_y、1_z 全部并入同一 token）
//   * 反斜杠续行在指令/字符串内各归其位，普通代码中视作空白

enum class TokKind {
    Ident,     // 标识符 / 关键字
    Number,    // pp-number（含字面量后缀）
    Str,       // 字符串字面量（含前缀、raw、UDL 后缀）
    Chr,       // 字符字面量（含前缀、UDL 后缀）
    Op,        // 运算符 / 标点
    Comment,   // 注释
    Directive, // 预处理指令（整条逻辑行，含续行，含结尾换行）
    Space,     // 行内空白（不含换行）
    Newline,   // 换行
};

struct Token {
    TokKind kind;
    std::string text;
    // 在源文本中的起始偏移（lexSource 产生的 token 有效；由别的 token 拼成的
    // 合并 token 不对应源文本，保持 0）
    size_t begin = 0;
};

// 三字符优先的运算符表
static const char *const kOps3[] = {"<<=", ">>=", "<=>", "->*", "...", "%:%:", nullptr};
static const char *const kOps2[] = {"::",
                                    ".*",
                                    "==", "!=", "<=", ">=", "&&", "||", "++",
                                    "--", "->", "<<", ">>", "+=", "-=", "*=",
                                    "/=", "%=", "&=", "|=", "^=",
                                    "<:", ":>", "<%", "%>", "%:", nullptr};

inline bool lexMatchAt(const std::string &s, size_t i, const char *const *tbl) {
    for (int t = 0; tbl[t]; ++t) {
        size_t n = std::strlen(tbl[t]);
        if (s.compare(i, n, tbl[t]) == 0) return true;
    }
    return false;
}

// 判断位置 i 是否是字符串/字符字面量开头（含前缀），返回结束位置（不含），0 表示不是
inline size_t lexStringEnd(const std::string &s, size_t i) {
    size_t n = s.size();
    // 前缀：u8 / u / U / L / R 及组合（u8R、uR、UR、LR）
    size_t p = i;
    while (p < n && (s[p] == 'u' || s[p] == 'U' || s[p] == 'L' || s[p] == 'R' ||
                     (p == i && s[p] == 'u' && p + 1 < n && s[p + 1] == '8')))
        ++p;
    // u8 前缀：确保 "u8" 两个字符一起处理
    if (i < n && s[i] == 'u' && i + 1 < n && s[i + 1] == '8') {
        p = i + 2;
        if (p < n && s[p] == 'R') ++p;
    } else {
        p = i;
        if (p < n && (s[p] == 'u' || s[p] == 'U' || s[p] == 'L')) {
            ++p;
            if (p < n && s[p] == 'R') ++p;
        } else if (p < n && s[p] == 'R') {
            ++p;
        }
    }
    if (p >= n || (s[p] != '"' && s[p] != '\'')) return 0;
    char q = s[p];
    bool raw = (p > i) && s[p - 1] == 'R' && q == '"';
    if (raw) {
        // R"delim( ... )delim"
        size_t d0 = p + 1;
        size_t op = s.find('(', d0);
        if (op == std::string::npos) return 0;
        std::string delim = s.substr(d0, op - d0);
        std::string close = ")" + delim + "\"";
        size_t cl = s.find(close, op + 1);
        if (cl == std::string::npos) return n; // 未闭合：吞到结尾
        size_t end = cl + close.size();
        // UDL 后缀
        while (end < n && isIdentCont((unsigned char)s[end])) ++end;
        return end;
    }
    size_t j = p + 1;
    while (j < n) {
        if (s[j] == '\\') {
            j += (s[j + 1] == '\r' && s[j + 2] == '\n') ? 3 : 2; // 续行（含 CRLF）或转义
            continue;
        }
        if (s[j] == q) {
            ++j;
            break;
        }
        ++j;
    }
    if (j > n) j = n;
    while (j < n && isIdentCont((unsigned char)s[j])) ++j; // UDL 后缀
    return j;
}

// 位置 k 的换行是否被 `\` 续行。`\` 与换行之间夹空白时编译器同样按续行处理
// （并告警），这里保持一致；CRLF 的 `\r` 属于被跳过的空白。
inline bool isSplicedNewline(const std::string &s, size_t k) {
    while (k > 0 && (s[k - 1] == ' ' || s[k - 1] == '\t' || s[k - 1] == '\r'))
        --k;
    return k > 0 && s[k - 1] == '\\';
}

// 从 from 起找逻辑行的行尾换行（被续行跨过的换行不算）；找不到返回 npos
inline size_t lexLogicalLineEnd(const std::string &s, size_t from) {
    for (size_t k = from; k < s.size(); ++k)
        if (s[k] == '\n' && !isSplicedNewline(s, k)) return k;
    return std::string::npos;
}

// 同 lexStringEnd，但字面量必须在一行（逻辑行）内闭合：中途出现非续行的换行
// 即认为这里不是字面量，返回 0。逐行处理的阶段与指令行内的扫描都依赖这一点——
// `#error don't` 里的撇号、已失效的注释里的引号不能让后面的文本被当成字面量。
inline size_t lexLiteralEndInLine(const std::string &s, size_t i) {
    if (i >= s.size()) return 0;
    char c = s[i];
    if (c != '"' && c != '\'' && !isIdentStart((unsigned char)c)) return 0;
    size_t e = lexStringEnd(s, i);
    if (!e || e <= i) return 0;
    for (size_t j = i; j + 1 < e; ++j)
        if (s[j] == '\n' && !isSplicedNewline(s, j)) return 0;
    return e;
}

// 去除单条预处理指令内的注释：块注释 → 一个空格；行注释连同其续行一并删除，
// 行尾换行保留。字面量整体跳过（`#define X "a/*b"` 里的 `/*` 不是注释）。
inline std::string stripDirectiveComments(const std::string &dir) {
    if (dir.find('/') == std::string::npos) return dir;
    std::string out;
    size_t n = dir.size();
    for (size_t i = 0; i < n;) {
        char c = dir[i];
        if (c == '/' && i + 1 < n && dir[i + 1] == '*') {
            size_t e = dir.find("*/", i + 2);
            out += ' ';
            i = (e == std::string::npos) ? n : e + 2;
        } else if (c == '/' && i + 1 < n && dir[i + 1] == '/') {
            size_t e = lexLogicalLineEnd(dir, i + 2);
            out += dir.substr(e == std::string::npos ? n : e);
            break;
        } else if (size_t e = lexLiteralEndInLine(dir, i)) {
            out += dir.substr(i, e - i);
            i = e;
        } else {
            out += c;
            ++i;
        }
    }
    return out;
}

inline std::vector<Token> lexSource(const std::string &src) {
    std::vector<Token> out;
    size_t i = 0, n = src.size();
    bool lineStart = true; // 当前行到目前为止只有空白（允许 # 出现在缩进后）
    auto push = [&](TokKind k, size_t b, size_t e) {
        out.push_back({k, src.substr(b, e - b), b});
    };

    while (i < n) {
        char c = src[i];

        // 行首预处理指令（允许前面有空白）：整条逻辑行（含续行）作为一个 token。
        // 扫描时跳过注释与字面量——块注释里的换行不结束指令，注释和字面量里的
        // 引号、反斜杠也不算数
        if (lineStart && c == '#') {
            size_t j = i;
            while (j < n) {
                char d = src[j];
                if (d == '\n') {
                    ++j;
                    if (!isSplicedNewline(src, j - 1)) break; // 逻辑行到此结束
                } else if (d == '/' && j + 1 < n && src[j + 1] == '*') {
                    size_t e = src.find("*/", j + 2);
                    j = (e == std::string::npos) ? n : e + 2;
                } else if (d == '/' && j + 1 < n && src[j + 1] == '/') {
                    // 行注释到逻辑行尾，被续行带上的后续各行也属于注释
                    size_t e = lexLogicalLineEnd(src, j + 2);
                    j = (e == std::string::npos) ? n : e + 1;
                    break;
                } else if (size_t e = lexLiteralEndInLine(src, j)) {
                    j = e;
                } else {
                    ++j;
                }
            }
            push(TokKind::Directive, i, j);
            i = j;
            lineStart = true;
            continue;
        }

        if (c == '\n') {
            push(TokKind::Newline, i, i + 1);
            ++i;
            lineStart = true;
            continue;
        }
        // 普通代码中的反斜杠续行（LF 或 CRLF）：视作空白
        if (c == '\\' && i + 1 < n &&
            (src[i + 1] == '\n' ||
             (src[i + 1] == '\r' && i + 2 < n && src[i + 2] == '\n'))) {
            push(TokKind::Space, i, i + 1);
            i += (src[i + 1] == '\r') ? 3 : 2;
            continue;
        }
        if (c == ' ' || c == '\t' || c == '\r' || c == '\v' || c == '\f') {
            size_t j = i;
            while (j < n && (src[j] == ' ' || src[j] == '\t' || src[j] == '\r' ||
                             src[j] == '\v' || src[j] == '\f'))
                ++j;
            push(TokKind::Space, i, j);
            i = j;
            continue;
        }
        lineStart = false;

        // 行注释：到逻辑行尾为止（续行把注释延长到下一行）；行尾换行另行成 token
        if (c == '/' && i + 1 < n && src[i + 1] == '/') {
            size_t e = lexLogicalLineEnd(src, i + 2);
            size_t j = (e == std::string::npos) ? n : e;
            push(TokKind::Comment, i, j);
            i = j;
            continue;
        }
        if (c == '/' && i + 1 < n && src[i + 1] == '*') {
            size_t j = i + 2;
            while (j + 1 < n && !(src[j] == '*' && src[j + 1] == '/')) ++j;
            j = std::min(j + 2, n);
            push(TokKind::Comment, i, j);
            i = j;
            continue;
        }

        // 字符串 / 字符（含前缀与 UDL）——尝试从前缀起点开始
        {
            size_t probe = i;
            if (c == '"' || c == '\'') {
                probe = i;
            } else if (isIdentStart((unsigned char)c)) {
                // 检查 ident 的一部分是否为字面量前缀（如 u8R"..."）
                size_t e = lexStringEnd(src, i);
                if (e && e > i) {
                    // 仅当整个前缀紧贴引号（前缀与引号之间无空白）时成立
                    size_t q = e; // 回找引号位置
                    for (size_t t = i; t < e && t < n; ++t)
                        if (src[t] == '"' || src[t] == '\'') {
                            q = t;
                            break;
                        }
                    if (q > i) {
                        // i..q-1 是前缀，必须全是合法前缀字符
                        bool ok = true;
                        for (size_t t = i; t < q; ++t) {
                            char pc = src[t];
                            if (!(pc == 'u' || pc == 'U' || pc == 'L' ||
                                  pc == 'R' || pc == '8'))
                                ok = false;
                        }
                        if (ok) {
                            push(TokKind::Str, i, e); // 统一按 Str 记录
                            i = e;
                            continue;
                        }
                    }
                }
            } else {
                probe = i;
            }
            (void)probe;
            if (c == '"' || c == '\'') {
                size_t e = lexStringEnd(src, i);
                if (e && e > i) {
                    push(c == '"' ? TokKind::Str : TokKind::Chr, i, e);
                    i = e;
                    continue;
                }
            }
        }

        if (isIdentStart((unsigned char)c)) {
            size_t j = i + 1;
            while (j < n && isIdentCont((unsigned char)src[j])) ++j;
            push(TokKind::Ident, i, j);
            i = j;
            continue;
        }

        if (std::isdigit((unsigned char)c) ||
            (c == '.' && i + 1 < n && std::isdigit((unsigned char)src[i + 1]))) {
            // pp-number
            size_t j = i + 1;
            while (j < n) {
                char d = src[j];
                if (isIdentCont((unsigned char)d) || d == '.') {
                    ++j;
                } else if ((d == '+' || d == '-') && j > i) {
                    char prev = src[j - 1];
                    if (prev == 'e' || prev == 'E' || prev == 'p' || prev == 'P')
                        ++j;
                    else
                        break;
                } else {
                    break;
                }
            }
            push(TokKind::Number, i, j);
            i = j;
            continue;
        }

        // <:: 特判：按 "<" "::" 处理
        if (c == '<' && i + 2 < n && src[i + 1] == ':' && src[i + 2] == ':') {
            push(TokKind::Op, i, i + 1);
            ++i;
            continue;
        }

        if (lexMatchAt(src, i, kOps3)) {
            push(TokKind::Op, i, i + 3);
            i += 3;
            continue;
        }
        if (lexMatchAt(src, i, kOps2)) {
            push(TokKind::Op, i, i + 2);
            i += 2;
            continue;
        }
        push(TokKind::Op, i, i + 1);
        ++i;
    }
    return out;
}

// 收集源文件中所有标识符（含指令行内部的，如宏名、#if 表达式中的名字）
inline std::set<std::string> collectIdentifiers(const std::string &src) {
    std::set<std::string> ids;
    for (const Token &t : lexSource(src)) {
        if (t.kind == TokKind::Ident) {
            ids.insert(t.text);
        } else if (t.kind == TokKind::Directive) {
            // 指令行内再扫一遍标识符
            const std::string &d = t.text;
            for (size_t i = 0; i < d.size();) {
                if (isIdentStart((unsigned char)d[i])) {
                    size_t j = i + 1;
                    while (j < d.size() && isIdentCont((unsigned char)d[j])) ++j;
                    ids.insert(d.substr(i, j - i));
                    i = j;
                } else {
                    ++i;
                }
            }
        }
    }
    return ids;
}

// 去除注释：所有 token 原样拼接、注释 token 替换为一个空格。
// 空格必不可少——直接删除会让 a/**/b 粘连成 ab；行注释后紧跟的换行
// 属独立 token，行结构得以保留。指令行内的注释同样清除（见
// stripDirectiveComments），指令行外的换行不受影响。
inline std::string stripComments(const std::string &src) {
    std::string out;
    out.reserve(src.size());
    for (const Token &t : lexSource(src)) {
        if (t.kind == TokKind::Comment)
            out += ' ';
        else if (t.kind == TokKind::Directive)
            out += stripDirectiveComments(t.text);
        else
            out += t.text;
    }
    return out;
}

// ============================ 名字生成器 ============================
// compress 模式：53/63 混合进制最短编码（a, b, ..., _, aa, ab, ...），天然
//               "尽量复用"——不同声明若原始名相同则共享同一个短名；
// random  模式：minLen..maxLen 随机字母数字。
// 两种模式都跳过：禁用集合（源文件残留标识符）、关键字、保留标识符。
class NameGenerator {
public:
    NameGenerator(bool compress, size_t minLen, size_t maxLen, uint64_t seed,
                  std::set<std::string> forbidden)
        : Compress_(compress), MinLen_(minLen), MaxLen_(maxLen),
          Rng_(seed ? seed : std::random_device{}()),
          Forbidden_(std::move(forbidden)) {
        Forbidden_.insert(cppKeywords().begin(), cppKeywords().end());
    }

    std::string next() {
        if (Compress_) {
            for (;;) {
                std::string s = encodeCounter(Counter_++);
                if (usable(s)) return s;
            }
        }
        std::uniform_int_distribution<size_t> lenDist(MinLen_, MaxLen_);
        for (;;) {
            size_t len = lenDist(Rng_);
            std::string s;
            s.reserve(len);
            s.push_back(kFirst[Rng_() % kFirstCount]);
            for (size_t i = 1; i < len; ++i)
                s.push_back(kOther[Rng_() % kOtherCount]);
            if (usable(s)) return s;
        }
    }

private:
    static constexpr const char *kFirst =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_";
    static constexpr const char *kOther =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_0123456789";
    static constexpr size_t kFirstCount = 53;
    static constexpr size_t kOtherCount = 63;

    bool usable(const std::string &s) const {
        if (Forbidden_.count(s)) return false;
        if (isReservedName(s)) return false;
        return true;
    }

    static std::string encodeCounter(size_t x) {
        std::string s(1, kFirst[x % kFirstCount]);
        x /= kFirstCount;
        while (x) {
            s.push_back(kOther[x % kOtherCount]);
            x /= kOtherCount;
        }
        return s;
    }

    bool Compress_;
    size_t MinLen_, MaxLen_;
    std::mt19937_64 Rng_;
    size_t Counter_ = 0;
    std::set<std::string> Forbidden_;
};

} // namespace obf
