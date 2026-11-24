// obf/Passes.hpp — 混淆 pass 流水线骨架
//
// 两类 pass：
//   ASTObfPass：在 Clang AST + Rewriter 上工作（重命名、控制流扁平化、
//               插入混淆逻辑），按注册顺序执行；
//   TextObfPass：对 AST 阶段产出的最终文本做变换（宏定义混淆）。
//
// 多个 AST pass 的编辑互不重叠由各 pass 自行保证：
//   RenamePass  按 token 范围替换；FlattenPass 纯插入（只插在 ';'、'}'
//   之后）；OpaquePass 常量替换避开 Flatten 的插入点、垃圾块只插在
//   未扁平化函数的语句边界之后。
#pragma once

#include <clang/AST/ASTContext.h>
#include <clang/Rewrite/Core/Rewriter.h>

#include <llvm/Support/raw_ostream.h>

#include "Support.hpp"

#include <memory>
#include <set>
#include <string>
#include <vector>

namespace obf {

// 各 pass 共用的禁用名集合：当前文本里的全部标识符，并入预处理阶段见过的
// 全部标识符（PredefIdentifiers，由驱动注入）。
// 后者覆盖系统头文件里的宏名与声明——压缩短名撞上它们时，主文件的新声明
// 会与头文件声明冲突（如 glibc 的 j0/j1/jn/y0/y1/yn），或被同名宏替换掉。
inline std::set<std::string> forbiddenNames(
    const std::string &text,
    const std::set<std::string> *predef) {
    std::set<std::string> f = collectIdentifiers(text);
    if (predef) f.insert(predef->begin(), predef->end());
    return f;
}

class ASTObfPass {
public:
    virtual ~ASTObfPass() = default;
    virtual const char *name() const = 0;
    virtual void run(clang::Rewriter &R, clang::ASTContext &Ctx) = 0;

    // 预处理阶段见过的全部标识符（见 forbiddenNames）；由驱动在 run 之前设置
    const std::set<std::string> *PredefIdentifiers = nullptr;
};

class TextObfPass {
public:
    virtual ~TextObfPass() = default;
    virtual const char *name() const = 0;
    virtual std::string run(std::string src) = 0;
};

} // namespace obf
