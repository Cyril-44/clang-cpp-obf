// obf/MacroObfuscator.hpp — 宏定义混淆（文本级 pass）
//
// 移植自旧项目 def_obf.py，并做以下关键修正/增强：
//   * 预处理指令不再只保留“文件头部”，而是**在原位置**原样保留
//     （#include / #define / #if 等永远不能来自宏展开）；
//   * 分词器修正：:: ... ->* .* <=> 多字符运算符不拆散、pp-number 正确、
//     raw string / 编码前缀 / UDL 后缀成单一 token（见 Support.hpp）；
//   * 宏名用工具统一的名字生成器（随机长名或压缩短名），并禁用与
//     源文件任何标识符冲突的名字；
//   * 压缩模式按 token 文本去重复用：同一个 ")" 只定义一个宏，反复引用；
//     去重作用域随条件编译分层——定义写在首次出现的分支里，分支外的代码
//     不能引用（否则 C 模式下 #ifdef __cplusplus 分支里的定义全部失效）；
//   * 叶宏 → 8 叉树 → 段顶宏 的层级结构保留（每行宏定义有长度上限）。
//
// 输出结构：
//   [所有 #define 定义]  [原序事件流：指令行原样 / 段顶宏调用]
#pragma once

#include <llvm/Support/raw_ostream.h>

#include "Passes.hpp"
#include "Support.hpp"

#include <functional>
#include <map>
#include <string>
#include <vector>

namespace obf {

// 条件编译指令的种类（见 MacroObfuscator 中去重作用域的处理）
enum CondKind { CondNone, CondEnter, CondAlt, CondEnd };

// 判断整条指令行的文本是否为条件编译指令：#if/#ifdef/#ifndef（进入）、
// #elif/#else（切换分支）、#endif（退出）。允许 # 与指令名之间有空白。
inline CondKind condDirectiveKind(const std::string &d) {
    size_t i = 0;
    while (i < d.size() && (d[i] == ' ' || d[i] == '\t' || d[i] == '\r')) ++i;
    if (i == d.size() || d[i] != '#') return CondNone;
    ++i;
    while (i < d.size() && (d[i] == ' ' || d[i] == '\t')) ++i;
    static const struct { const char *kw; CondKind k; } Tab[] = {
        {"if", CondEnter}, {"ifdef", CondEnter}, {"ifndef", CondEnter},
        {"elif", CondAlt}, {"else", CondAlt}, {"endif", CondEnd},
    };
    for (const auto &e : Tab) {
        size_t n = std::strlen(e.kw);
        if (d.compare(i, n, e.kw) == 0 &&
            (i + n == d.size() || !isIdentCont((unsigned char)d[i + n])))
            return e.k;
    }
    return CondNone;
}

class MacroObfuscator {
public:
    bool Compress = false;
    size_t MinLen = 12, MaxLen = 20;
    uint64_t Seed = 0;
    // 预处理阶段见过的全部标识符（见 Passes.hpp 的 forbiddenNames）：宏名
    // 撞上头文件里的任何名字都会让该名字被宏替换掉
    const std::set<std::string> *PredefIdentifiers = nullptr;

    static constexpr size_t SegSize = 2000;    // 每段叶 token 数上限
    static constexpr size_t ChunkFactor = 8;   // 宏树分叉数
    static constexpr size_t MaxMacroLine = 1800; // 单行 #define 长度上限

    std::string run(const std::string &src) const {
        std::set<std::string> forbidden =
            forbiddenNames(src, PredefIdentifiers);
        NameGenerator gen(Compress, MinLen, MaxLen, Seed,
                          std::move(forbidden));

        auto toks = lexSource(src);

        // 1. 构建事件流：Directive（原样）| LeafRun（待宏化 token 序列）
        struct Event {
            bool directive;
            CondKind cond = CondNone;  // directive 且为条件编译指令时的种类
            std::string raw;               // directive / passthrough 原文
            std::vector<Token> leaves;     // leaf run
        };
        std::vector<Event> events;
        std::vector<Token> cur;
        auto flush = [&] {
            if (!cur.empty()) {
                events.push_back({false, CondNone, "", std::move(cur)});
                cur.clear();
            }
        };
        for (const Token &t : toks) {
            switch (t.kind) {
            case TokKind::Directive:
                flush();
                events.push_back({true, condDirectiveKind(t.text), t.text, {}});
                break;
            case TokKind::Comment:
            case TokKind::Space:
            case TokKind::Newline:
                break;
            default:
                // 含换行的 token（跨行 raw string 等）无法进入 #define 体，
                // 原样透传
                if (t.text.find('\n') != std::string::npos) {
                    flush();
                    events.push_back({false, CondNone, t.text, {}});
                } else {
                    cur.push_back(t);
                }
                break;
            }
        }
        flush();

        // 合并“标识符 + 平衡括号实参列表”为单个叶 token（如 assert ( q )、
        // For ( i , max ( a , b ) )）：
        //   函数式宏调用的名字、'('、实参、')' 必须位于同一个替换列表中，
        //   否则参数收集时闭括号藏在别的宏里，调用无法成立。
        //   合并对真实函数调用同样安全（文本原样、仅空白规整）。
        // 括号不配对（跨指令/跨行字符串等罕见情形）则放弃合并。
        for (auto &ev : events) {
            if (ev.directive) continue;
            std::vector<Token> merged;
            std::vector<Token> &L = ev.leaves;
            for (size_t i = 0; i < L.size(); ++i) {
                if (L[i].kind == TokKind::Ident && i + 1 < L.size() &&
                    L[i + 1].kind == TokKind::Op && L[i + 1].text == "(") {
                    int depth = 0;
                    size_t j = i + 1;
                    bool ok = false;
                    for (; j < L.size(); ++j) {
                        if (L[j].kind == TokKind::Op && L[j].text == "(")
                            ++depth;
                        else if (L[j].kind == TokKind::Op &&
                                 L[j].text == ")") {
                            --depth;
                            if (depth == 0) {
                                ok = true;
                                break;
                            }
                        }
                    }
                    if (ok) {
                        std::string text = L[i].text;
                        for (size_t k = i + 1; k <= j; ++k) {
                            text += " ";
                            text += L[k].text;
                        }
                        merged.push_back(
                            {TokKind::Ident, std::move(text)});
                        i = j;
                        continue;
                    }
                }
                merged.push_back(L[i]);
            }
            ev.leaves = std::move(merged);
        }


        // 2. 生成所有宏定义 + 每个事件对应的顶层宏名
        std::string defines;
        std::vector<std::vector<std::string>> topNames(events.size());

        // 压缩模式：按 token 文本去重。去重作用域随条件编译分层——宏定义写在
        // 它首次出现的条件分支里，分支外的代码看不到：
        //   #ifdef __cplusplus
        //   extern "C" {      ← '{' 在这里首次出现，#define m { 落在该分支内
        //   #endif
        //   int f(void) {     ← C 模式下该分支不生效，m 未定义
        // 因此 #if 族压入新作用域，#else/#elif 清空当前层，#endif 弹出；
        // 外层作用域里的定义此刻仍然有效，可以直接复用。
        std::vector<std::map<std::string, std::string>> dedup(1);

        auto leafMacroName = [&](const Token &t) {
            if (Compress) {
                for (auto s = dedup.rbegin(); s != dedup.rend(); ++s) {
                    auto it = s->find(t.text);
                    if (it != s->end()) return it->second;
                }
                std::string nm = gen.next();
                dedup.back().emplace(t.text, nm);
                defines += "#define " + nm + " " + t.text + "\n";
                return nm;
            }
            std::string nm = gen.next();
            defines += "#define " + nm + " " + t.text + "\n";
            return nm;
        };

        // 递归受限的宏定义（超长按名字边界贪心拆分为中间宏）
        std::function<void(const std::string &, const std::string &)>
            emitDef = [&](const std::string &name,
                          const std::string &body) {
                if (body.size() <= MaxMacroLine) {
                    defines += "#define " + name + " " + body + "\n";
                    return;
                }
                // body 是以空格连接的宏名序列：贪心切块
                std::vector<std::string> chunks, tokens;
                for (size_t s = 0; s < body.size();) {
                    size_t e = body.find(' ', s);
                    if (e == std::string::npos) e = body.size();
                    tokens.push_back(body.substr(s, e - s));
                    s = e + 1;
                }
                std::string curChunk;
                for (const std::string &tok : tokens) {
                    if (!curChunk.empty() &&
                        curChunk.size() + 1 + tok.size() > MaxMacroLine) {
                        chunks.push_back(curChunk);
                        curChunk = tok;
                    } else {
                        if (!curChunk.empty()) curChunk += " ";
                        curChunk += tok;
                    }
                }
                if (!curChunk.empty()) chunks.push_back(curChunk);
                std::string joined;
                for (size_t i = 0; i < chunks.size(); ++i) {
                    std::string inter = gen.next();
                    defines += "#define " + inter + " " + chunks[i] + "\n";
                    if (!joined.empty()) joined += " ";
                    joined += inter;
                }
                defines += "#define " + name + " " + joined + "\n";
            };

        // 记录每个事件处理完后 defines 的长度，供输出时按位置切片
        std::vector<size_t> marks(events.size(), 0);

        for (size_t ei = 0; ei < events.size(); ++ei) {
            Event &ev = events[ei];
            if (ev.directive || ev.leaves.empty()) {
                // 去重作用域随条件编译分层（见 dedup 的说明）
                switch (ev.directive ? ev.cond : CondNone) {
                case CondEnter:
                    dedup.emplace_back();
                    break;
                case CondAlt:
                    dedup.back().clear();
                    break;
                case CondEnd:
                    if (dedup.size() > 1) dedup.pop_back();
                    break;
                default:
                    break;
                }
                marks[ei] = defines.size();
                continue;
            }
            // 分段（> SegSize）后逐段建树；字面括号直接作为树节点文本
            size_t total = ev.leaves.size();
            for (size_t off = 0; off < total; off += SegSize) {
                size_t cnt = std::min(SegSize, total - off);
                std::vector<std::string> level;
                level.reserve(cnt);
                for (size_t i = 0; i < cnt; ++i) {
                    const Token &t = ev.leaves[off + i];
                    if (t.kind == TokKind::Op &&
                        (t.text == "(" || t.text == ")"))
                        level.push_back(t.text); // 字面输出
                    else
                        level.push_back(leafMacroName(t));
                }
                while (level.size() > 1) {
                    std::vector<std::string> next;
                    for (size_t i = 0; i < level.size(); i += ChunkFactor) {
                        std::string body;
                        for (size_t j = i;
                             j < level.size() && j < i + ChunkFactor; ++j) {
                            if (!body.empty()) body += " ";
                            body += level[j];
                        }
                        std::string nm = gen.next();
                        emitDef(nm, body);
                        next.push_back(nm);
                    }
                    level = std::move(next);
                }
                topNames[ei].push_back(level.front());
            }
            marks[ei] = defines.size();
        }

        // 3. 组装输出：定义与事件流交错——每个代码段的宏定义紧随其前的
        //    指令（如 #include）之后输出，保证头文件在干净环境下解析；
        //    之后再输出该段的顶层宏调用
        std::string out;
        size_t emitted = 0; // defines 已输出到的位置
        for (size_t ei = 0; ei < events.size(); ++ei) {
            Event &ev = events[ei];
            if (ev.directive) {
                out += ev.raw;
            } else if (!ev.leaves.empty()) {
                out += defines.substr(emitted, marks[ei] - emitted);
                emitted = marks[ei];
                for (const std::string &top : topNames[ei]) out += top + "\n";
            } else {
                // 透传 token（续行拆开的字符串字面量、跨行 raw string 等）。其后
                // 紧跟的是下一段的 #define 或指令行，必须另起一行
                out += ev.raw;
                out += '\n';
            }
        }
        // 兜底：输出剩余定义（防御性，正常不会发生）
        if (emitted < defines.size()) out += defines.substr(emitted);
        if (!out.empty() && out.back() != '\n') out += "\n";
        return out;
    }
};

} // namespace obf
