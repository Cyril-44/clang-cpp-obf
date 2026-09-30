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

#include <memory>
#include <string>
#include <vector>

namespace obf {

class ASTObfPass {
public:
    virtual ~ASTObfPass() = default;
    virtual const char *name() const = 0;
    virtual void run(clang::Rewriter &R, clang::ASTContext &Ctx) = 0;
};

class TextObfPass {
public:
    virtual ~TextObfPass() = default;
    virtual const char *name() const = 0;
    virtual std::string run(std::string src) = 0;
};

} // namespace obf
