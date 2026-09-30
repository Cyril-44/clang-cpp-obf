// obf/OpaquePass.hpp — 插入混淆逻辑 / 不透明谓词 / 常数混淆
//
// 两个功能：
//   1. 常数 XOR 分解（沿用旧项目 OBF_raw.cpp 的
//      ((int)(((unsigned long long)A) ^ B)) 风格）：
//      AST 级把值 >= 10 的整数字面量替换为同值的异或分解表达式。
//      分解结果是常量表达式，可用于 case 标签、数组界、模板实参、
//      枚举初值等一切要求常量的位置。跳过浮点/字符/布尔、宏展开处、
//      与 Flatten 插入点重合处。
//   2. 垃圾逻辑块：对未被 FlattenPass 处理的函数，在顶层语句前插入
//      恒真不透明谓词驱动的自包含垃圾计算（无副作用，仅混淆阅读）。
//      已扁平化函数的垃圾块由 FlattenPass 自己在每个 case 中插入。
//
// 插入点同样只取 ';'、'}'、'{' 之后的位置，与 RenamePass 的替换零冲突。
#pragma once

#include <clang/AST/AST.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Rewrite/Core/Rewriter.h>

#include <llvm/Support/raw_ostream.h>

#include "Passes.hpp"
#include "Support.hpp"

#include <random>
#include <string>
#include <utility>
#include <vector>

namespace obf {

using namespace clang;

class OpaquePass : public ASTObfPass {
public:
    bool Compress = false;
    size_t MinLen = 12, MaxLen = 20;
    uint64_t Seed = 0;

    unsigned LiteralsObfuscated = 0, JunkInserted = 0;

    // 由 FlattenPass 提供（可为空：未启用扁平化时）
    const std::vector<std::pair<unsigned, unsigned>> *ClaimedBodies = nullptr;
    const std::vector<unsigned> *InsertOffsets = nullptr;

    const char *name() const override { return "opaque-flow"; }

    void run(Rewriter &rew, ASTContext &ctx) override {
        R_ = &rew;
        Ctx_ = &ctx;
        SM_ = &ctx.getSourceManager();
        Rng_.seed(Seed ? Seed ^ 0x2545F4914F6CDD1DULL : 11400714819323198485ULL);

        // 禁用集合取自“当前已改写缓冲区”（含 Rename/Flatten 生成的新名字）
        std::string cur;
        llvm::raw_string_ostream os(cur);
        R_->getEditBuffer(SM_->getMainFileID()).write(os);
        os.flush();
        Gen_ = std::make_unique<NameGenerator>(
            Compress, MinLen, MaxLen, Seed, collectIdentifiers(cur));

        Visitor V(*this);
        V.TraverseDecl(ctx.getTranslationUnitDecl());
    }

    std::string junkBlock() {
        std::string v = Gen_->next();
        std::string r1 = std::to_string(Rng_() >> 1);
        std::string r2 = std::to_string(Rng_() >> 1);
        return "{ unsigned long long " + v + " = " + r1 + "ULL; "
               "if ((((" + v + " * " + v + ") + " + v + ") & 1ULL) == 1ULL) { " +
               v + " ^= " + r2 + "ULL; } "
               "else { " + v + " -= " + r2 + "ULL; } ((void)(" + v + ")); }\n";
    }

private:
    friend struct OpaqueVisitor;

    Rewriter *R_ = nullptr;
    ASTContext *Ctx_ = nullptr;
    SourceManager *SM_ = nullptr;
    std::mt19937_64 Rng_;
    std::unique_ptr<NameGenerator> Gen_;
    std::set<unsigned> DoneLiterals_;

    bool bodyClaimed(SourceLocation l) {
        if (!ClaimedBodies) return false;
        unsigned o = SM_->getFileOffset(l);
        for (auto &pr : *ClaimedBodies)
            if (o >= pr.first && o <= pr.second) return true;
        return false;
    }

    bool atInsertOffset(SourceLocation l) {
        if (!InsertOffsets) return false;
        unsigned o = SM_->getFileOffset(l);
        for (unsigned x : *InsertOffsets)
            if (x == o) return true;
        return false;
    }

    struct Visitor : public RecursiveASTVisitor<Visitor> {
        OpaquePass &P;
        explicit Visitor(OpaquePass &p) : P(p) {}
        bool VisitIntegerLiteral(IntegerLiteral *E) {
            P.obfuscateLiteral(E);
            return true;
        }
        bool VisitFunctionDecl(FunctionDecl *FD) {
            P.insertJunk(FD);
            return true;
        }
    };

    void obfuscateLiteral(IntegerLiteral *E);
    void insertJunk(FunctionDecl *FD);
};

void OpaquePass::obfuscateLiteral(IntegerLiteral *E) {
    SourceLocation b = E->getBeginLoc();
    if (!b.isValid() || b.isMacroID()) return;
    if (!SM_->isWrittenInMainFile(b)) return;
    // 模板实例化会让同一源码位置的字面量以多个 Expr 节点出现
    //（模板模式 + 各实例化），同一位置只允许替换一次，
    // 否则 Rewriter 的重叠替换会把文本拼接成乱码
    if (!DoneLiterals_.insert(b.getRawEncoding()).second) return;

    const BuiltinType *BT = E->getType()->getAs<BuiltinType>();
    if (!BT) return;
    switch (BT->getKind()) {
    case BuiltinType::Int:
    case BuiltinType::UInt:
    case BuiltinType::Long:
    case BuiltinType::ULong:
    case BuiltinType::LongLong:
    case BuiltinType::ULongLong:
        break;
    default:
        return; // 浮点/字符/布尔等不做异或分解
    }
    if (E->getValue().ult(10)) return; // 小常数不处理，避免噪音
    if (atInsertOffset(b)) return;     // 与 Flatten 插入点重合处避让

    uint64_t bits = E->getValue().getZExtValue();
    uint64_t A = (Rng_() >> 1) | 0x8000000000000000ULL;
    uint64_t B = A ^ bits;
    std::string ty = E->getType().getAsString();
    std::string repl =
        "((" + ty + ")(" + std::to_string(A) + "ULL ^ " + std::to_string(B) +
        "ULL))";
    if (R_->ReplaceText(E->getSourceRange(), repl)) return; // 失败则放弃
    ++LiteralsObfuscated;
}

void OpaquePass::insertJunk(FunctionDecl *FD) {
    if (!FD || !FD->isThisDeclarationADefinition()) return;
    CompoundStmt *Body = dyn_cast_or_null<CompoundStmt>(FD->getBody());
    if (!Body) return;
    SourceLocation lb = Body->getLBracLoc();
    if (!lb.isValid() || lb.isMacroID()) return;
    if (!SM_->isWrittenInMainFile(lb)) return;
    if (bodyClaimed(lb)) return; // 已扁平化的函数：垃圾块由 Flatten 负责

    // 最多在 2 个顶层语句前插入垃圾块；插入点取前一语句结尾或 '{' 之后
    std::vector<Stmt *> cands;
    for (Stmt *S : Body->body()) {
        if (isa<NullStmt>(S)) continue;
        SourceLocation b = S->getBeginLoc(), e = S->getEndLoc();
        if (!b.isValid() || !e.isValid() || b.isMacroID() || e.isMacroID())
            continue;
        if (!SM_->isWrittenInMainFile(b)) continue;
        cands.push_back(S);
    }
    if (cands.empty()) return;

    std::shuffle(cands.begin(), cands.end(), Rng_);
    size_t count = std::min<size_t>(2, cands.size());
    for (size_t i = 0; i < count; ++i) {
        Stmt *target = cands[i];
        // 找 target 之前最近的可用锚点（前一条语句的结尾；首句用 '{'）
        SourceLocation anchor = lb;
        for (Stmt *S : Body->body()) {
            if (S == target) break;
            SourceLocation e = S->getEndLoc();
            if (e.isValid() && !e.isMacroID()) anchor = e;
        }
        // 前导 ';'：表达式语句的结尾分号不在 EndLoc 内——提前终止当前语句，
        // 对本就以 ';' / '}' / '{' 收尾的锚点则是无害的空语句
        R_->InsertTextAfterToken(anchor, ";\n" + junkBlock());
        ++JunkInserted;
    }
}

} // namespace obf
