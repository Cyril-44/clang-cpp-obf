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
//   * 叶宏 → 8 叉树 → 段顶宏 的层级结构保留（每行宏定义有长度上限）。
//
// 输出结构：
//   [所有 #define 定义]  [原序事件流：指令行原样 / 段顶宏调用]
#pragma once

#include <llvm/Support/raw_ostream.h>

#include "Support.hpp"

#include <functional>
#include <map>
#include <string>
#include <vector>

namespace obf {

class MacroObfuscator {
public:
    bool Compress = false;
    size_t MinLen = 12, MaxLen = 20;
    uint64_t Seed = 0;

    static constexpr size_t SegSize = 2000;    // 每段叶 token 数上限
    static constexpr size_t ChunkFactor = 8;   // 宏树分叉数
    static constexpr size_t MaxMacroLine = 1800; // 单行 #define 长度上限

    std::string run(const std::string &src) const {
        std::set<std::string> forbidden = collectIdentifiers(src);
        NameGenerator gen(Compress, MinLen, MaxLen, Seed,
                          std::move(forbidden));

        auto toks = lexSource(src);

        // 1. 构建事件流：Directive（原样）| LeafRun（待宏化 token 序列）
        struct Event {
            bool directive;
            std::string raw;               // directive / passthrough 原文
            std::vector<Token> leaves;     // leaf run
        };
        std::vector<Event> events;
        std::vector<Token> cur;
        auto flush = [&] {
            if (!cur.empty()) {
                events.push_back({false, "", std::move(cur)});
                cur.clear();
            }
        };
        for (const Token &t : toks) {
            switch (t.kind) {
            case TokKind::Directive:
                flush();
                events.push_back({true, t.text, {}});
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
                    events.push_back({false, t.text, {}});
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

        // 压缩模式：按 token 文本去重
        std::map<std::string, std::string> dedup;

        auto leafMacroName = [&](const Token &t) {
            if (Compress) {
                auto it = dedup.find(t.text);
                if (it != dedup.end()) return it->second;
                std::string nm = gen.next();
                dedup.emplace(t.text, nm);
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
                out += ev.raw; // 透传 token（跨行字符串等）
            }
        }
        // 兜底：输出剩余定义（防御性，正常不会发生）
        if (emitted < defines.size()) out += defines.substr(emitted);
        if (!out.empty() && out.back() != '\n') out += "\n";
        return out;
    }
};

} // namespace obf
