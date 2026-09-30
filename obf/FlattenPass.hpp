// obf/FlattenPass.hpp — 控制流扁平化
//
// 形态：函数体顶层语句序列（声明序贯保留为前导，其余语句）全部改写为
//   <前导声明原样保留>
//   <垃圾块>
//   unsigned st = c0^K; const unsigned K = ...; bool done = false;
//   while (!done) switch (st ^= K) {
//     case c0: { <原语句0 原地保留> } st = c1^K; continue;
//     case c1: { <原语句1 原地保留> } st = c2^K; continue;
//     ...
//     default: done = true; break;
//   }
//
// 关键工程决策：**纯插入式改写**。不替换任何既有文本，只在语句边界
// （`;` / `}` / `{` 之后——语句不可能以标识符结尾）插入调度器文本：
//   1. 与 RenamePass 的按 token 替换编辑零重叠（Rewriter 不支持重叠编辑）；
//   2. 被包裹的语句仍在原位置、原 AST 节点上，重命名照常生效；
//   3. return 语义天然保持（case 内 return 直接退出函数）；
//   4. 嵌套控制流（if/for/嵌套 break/continue）作用于自身作用域，不受影响。
//
// 安全跳过：含 goto/label（含 GNU &&label）的函数、协程体、constexpr/consteval
// 函数、构造/析构函数、语句含宏展开位置的函数、可用语句少于 2 条的函数。
// 声明语句（含 VLA）全部留在前导序贯区——case 块间跳转会跨越初始化。
#pragma once

#include <clang/AST/AST.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Lex/Lexer.h>
#include <clang/Rewrite/Core/Rewriter.h>

#include <llvm/Support/raw_ostream.h>

#include "Passes.hpp"
#include "Support.hpp"

#include <random>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace obf {

using namespace clang;

class FlattenPass : public ASTObfPass {
public:
    bool Compress = false;
    size_t MinLen = 12, MaxLen = 20;
    uint64_t Seed = 0;

    unsigned FlattenedFuncs = 0;

    // 已扁平化函数的 body 区间与插入点（供后续 pass 避让）
    std::vector<std::pair<unsigned, unsigned>> ClaimedBodies;
    std::vector<unsigned> InsertOffsets;

    const char *name() const override { return "control-flow-flattening"; }

    void run(Rewriter &rew, ASTContext &ctx) override {
        R_ = &rew;
        Ctx_ = &ctx;
        SM_ = &ctx.getSourceManager();
        Rng_.seed(Seed ? Seed : 88172645463325252ULL);

        // 禁用集合取自“当前已改写缓冲区”（含 RenamePass 已生成的新名字），
        // 避免与前序 pass 生成的新名字撞车（如状态变量遮蔽同名函数）
        std::string cur;
        llvm::raw_string_ostream os(cur);
        R_->getEditBuffer(SM_->getMainFileID()).write(os);
        os.flush();
        Gen_ = std::make_unique<NameGenerator>(
            Compress, MinLen, MaxLen, Seed, collectIdentifiers(cur));

        Visitor V(*this);
        V.TraverseDecl(ctx.getTranslationUnitDecl());
    }

    // 恒真不透明谓词垃圾块：n*n+n 恒为偶数，读者难以一眼判定
    std::string junkBlock() {
        std::string v = Gen_->next();
        std::string r1 = std::to_string(Rng_() >> 1);
        std::string r2 = std::to_string(Rng_() >> 1);
        return "{ unsigned long long " + v + " = " + r1 + "ULL; "
               "if ((((" + v + " * " + v + ") + " + v + ") & 1ULL) == 0ULL) { " +
               v + " ^= " + r2 + "ULL; } "
               "else { " + v + " += " + r2 + "ULL; } ((void)(" + v + ")); }\n";
    }

private:
    friend struct FlattenVisitor;

    Rewriter *R_ = nullptr;
    ASTContext *Ctx_ = nullptr;
    SourceManager *SM_ = nullptr;
    std::mt19937_64 Rng_;
    std::unique_ptr<NameGenerator> Gen_;

    // 预扫描：出现这些构造的函数不能扁平化
    struct Checker : public RecursiveASTVisitor<Checker> {
        bool Bad = false;
        bool VisitLabelStmt(LabelStmt *) { Bad = true; return true; }
        bool VisitGotoStmt(GotoStmt *) { Bad = true; return true; }
        bool VisitIndirectGotoStmt(IndirectGotoStmt *) { Bad = true; return true; }
        bool VisitAddrLabelExpr(AddrLabelExpr *) { Bad = true; return true; }
        bool VisitCoroutineBodyStmt(CoroutineBodyStmt *) { Bad = true; return true; }
    };

    struct Visitor : public RecursiveASTVisitor<Visitor> {
        FlattenPass &P;
        explicit Visitor(FlattenPass &p) : P(p) {}
        bool VisitFunctionDecl(FunctionDecl *FD) {
            P.flattenOne(FD);
            return true;
        }
    };

    bool insideClaim(SourceLocation l) {
        unsigned o = SM_->getFileOffset(l);
        for (auto &pr : ClaimedBodies)
            if (o >= pr.first && o <= pr.second) return true;
        return false;
    }

    void flattenOne(FunctionDecl *FD);
};

void FlattenPass::flattenOne(FunctionDecl *FD) {
    if (!FD || !FD->isThisDeclarationADefinition()) return;
    if (isa<CXXConstructorDecl>(FD) || isa<CXXDestructorDecl>(FD)) return;
    if (FD->isConstexpr()) return;
    CompoundStmt *Body = dyn_cast_or_null<CompoundStmt>(FD->getBody());
    if (!Body) return;
    SourceLocation lb = Body->getLBracLoc(), rb = Body->getRBracLoc();
    if (!lb.isValid() || !rb.isValid()) return;
    if (lb.isMacroID() || !SM_->isWrittenInMainFile(lb)) return;
    if (insideClaim(lb)) return; // 嵌套在外层已扁平化体内（如 lambda）

    Checker chk;
    chk.TraverseStmt(Body);
    if (chk.Bad) return;

    // 顶层语句切分：最后一个 DeclStmt 之后的部分才参与扁平化
    std::vector<Stmt *> stmts(Body->body_begin(), Body->body_end());
    size_t lastDecl = 0;
    bool anyDecl = false;
    for (size_t i = 0; i < stmts.size(); ++i)
        if (isa<DeclStmt>(stmts[i])) {
            lastDecl = i;
            anyDecl = true;
        }
    size_t first = anyDecl ? lastDecl + 1 : 0;

    std::vector<Stmt *> rem;
    for (size_t i = first; i < stmts.size(); ++i) {
        Stmt *S = stmts[i];
        if (isa<NullStmt>(S)) continue;
        SourceLocation b = S->getBeginLoc(), e = S->getEndLoc();
        if (!b.isValid() || !e.isValid() || b.isMacroID() || e.isMacroID())
            return; // 宏语句：整函数放弃
        rem.push_back(S);
    }
    if (rem.size() < 2) return;

    // 状态机常量
    std::string st = Gen_->next(), kk = Gen_->next(), dn = Gen_->next();
    auto randU = [this]() { return (unsigned)(Rng_() >> 32); };
    unsigned K = randU() | 1u;
    std::set<unsigned> used;
    std::vector<unsigned> cs(rem.size());
    for (unsigned &c : cs)
        do {
            c = randU();
        } while (!used.insert(c).second);
    unsigned exitC;
    do {
        exitC = randU();
    } while (!used.insert(exitC).second);

    // 插入点：优先取前导最后一个非宏声明之后；否则函数体 '{' 之后。
    // （永远插在 ';'、'}'、'{' 类 token 之后，与重命名的 token 替换零冲突）
    SourceLocation anchor = lb;
    for (size_t i = first; i-- > 0;) {
        SourceLocation e = stmts[i]->getEndLoc();
        if (e.isValid() && !e.isMacroID()) {
            anchor = e;
            break;
        }
    }

    std::string head = "\n" + junkBlock() + "unsigned " + st + " = " +
                       std::to_string(cs[0] ^ K) + "u; const unsigned " + kk +
                       " = " + std::to_string(K) +
                       "u; bool " + dn + " = false;\n"
                       "while (!" + dn + ") switch (" + st + " ^= " + kk +
                       ") {\n"
                       "case " + std::to_string(cs[0]) + "u: { ";
    R_->InsertTextAfterToken(anchor, head);
    InsertOffsets.push_back(SM_->getFileOffset(anchor));

    for (size_t i = 0; i + 1 < rem.size(); ++i) {
        // 前导的 ';'：表达式语句的结尾分号不在 getEndLoc() 内，补一个
        // 空语句永无副作用；对本身以 ';' / '}' 结尾的语句也合法
        std::string mid = "; } " + st + " = " + std::to_string(cs[i + 1] ^ K) +
                          "u; continue; case " + std::to_string(cs[i + 1]) +
                          "u: { ";
        R_->InsertTextAfterToken(rem[i]->getEndLoc(), mid);
        InsertOffsets.push_back(SM_->getFileOffset(rem[i]->getEndLoc()));
    }
    std::string tail = "; } " + st + " = " + std::to_string(exitC ^ K) +
                       "u; continue; default: " + dn + " = true; break; } ";
    R_->InsertTextAfterToken(rem.back()->getEndLoc(), tail);
    InsertOffsets.push_back(SM_->getFileOffset(rem.back()->getEndLoc()));

    ClaimedBodies.push_back({SM_->getFileOffset(lb), SM_->getFileOffset(rb)});
    ++FlattenedFuncs;
}

} // namespace obf
