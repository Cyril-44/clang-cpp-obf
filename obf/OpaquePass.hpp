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
#include <clang/AST/ParentMapContext.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Rewrite/Core/Rewriter.h>

#include <llvm/Support/raw_ostream.h>

#include "Passes.hpp"
#include "Support.hpp"

#include <cstdio>
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

    // -rt：常量与字符串运行时混淆——密文存入静态存储，解密发生在运行期，
    // 阻断编译器常量折叠，二进制中不再出现明文常量/字符串
    bool RuntimeMode = false;

    unsigned LiteralsObfuscated = 0, StringsObfuscated = 0, JunkInserted = 0;

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
        bool VisitStringLiteral(StringLiteral *E) {
            P.obfuscateString(E);
            return true;
        }
        bool VisitFunctionDecl(FunctionDecl *FD) {
            P.insertJunk(FD);
            return true;
        }
    };

    void obfuscateLiteral(IntegerLiteral *E);
    void obfuscateString(StringLiteral *E);
    void insertJunk(FunctionDecl *FD);

    // 祖先链判定：字面量是否处于“必须保持常量/字面量”的上下文。
    // 运行时混淆（非常量表达式）只能用于普通表达式位置。
    bool badContext(const Expr *E);
};

void OpaquePass::obfuscateLiteral(IntegerLiteral *E) {
    SourceLocation b = E->getBeginLoc();
    if (!b.isValid() || b.isMacroID()) return;
    if (!SM_->isWrittenInMainFile(b)) return;
    // 模板实例化会让同一源码位置的字面量以多个 Expr 节点出现
    //（模板模式 + 各实例化），同一位置只允许替换一次，
    // 否则 Rewriter 的重叠替换会把文本拼接成乱码
    if (!DoneLiterals_.insert(b.getRawEncoding()).second) return;
    // 用户定义字面量的后缀不属于字面量节点范围，任何替换都会拼出非法 token
    for (const auto &N : Ctx_->getParents(*E))
        if (N.get<UserDefinedLiteral>()) return;

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
    std::string ty = E->getType().getAsString();

    // 运行时模式：密文存静态存储、运行期异或还原（阻断常量折叠），
    // 仅限普通表达式位置；其余位置回退到静态 XOR 分解。
    // IIFE 外层再包一对括号：紧跟下标/其他 '[' 时 "[[" 会被解析为属性
    if (RuntimeMode && !badContext(E)) {
        uint64_t K = (Rng_() >> 1) | 0x8000000000000000ULL;
        uint64_t M = bits ^ K;
        std::string nm = Gen_->next();
        std::string repl = "([]{ static " + ty + " " + nm + " = " +
                           std::to_string(M) + "; return (" + ty + ")(" + nm +
                           " ^ " + std::to_string(K) + "ULL); }())";
        if (!R_->ReplaceText(E->getSourceRange(), repl))
            ++LiteralsObfuscated;
        return;
    }

    // 静态 XOR 分解（常量表达式，任何位置合法）
    uint64_t A = (Rng_() >> 1) | 0x8000000000000000ULL;
    uint64_t B = A ^ bits;
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

void OpaquePass::obfuscateString(StringLiteral *E) {
    if (!RuntimeMode) return;
    SourceLocation b = E->getBeginLoc();
    if (!b.isValid() || b.isMacroID()) return;
    if (!SM_->isWrittenInMainFile(b)) return;
    if (!DoneLiterals_.insert(b.getRawEncoding()).second) return;
    // 只处理普通窄字符串：宽/UTF-8/16/32 及 UDL 后缀不做（后缀不属于节点范围）
    if (!E->isOrdinary() || E->getCharByteWidth() != 1) return;
    for (const auto &N : Ctx_->getParents(*E))
        if (N.get<UserDefinedLiteral>()) return;
    if (badContext(E)) return;
    if (atInsertOffset(b)) return;

    std::string bytes = E->getString().str();
    bytes.push_back('\0');

    // 字节级位置相关密钥：enc[i] = src[i] ^ (uint8_t)(K1 + i*K2)
    unsigned K1 = 1u + (unsigned)(Rng_() % 255u);
    unsigned K2 = 1u + (unsigned)(Rng_() % 15u);
    std::string arr;
    char cell[16];
    for (size_t i = 0; i < bytes.size(); ++i) {
        unsigned v = (unsigned char)bytes[i] ^
                     (unsigned char)((K1 + i * K2) & 0xFFu);
        std::snprintf(cell, sizeof cell, "(char)0x%02X%s", v,
                      i + 1 < bytes.size() ? "," : "");
        arr += cell;
    }

    std::string nm = Gen_->next(), on = Gen_->next(), ix = Gen_->next();
    std::string repl =
        "([]{ static char " + nm + "[] = {" + arr + "};"
        " static bool " + on + " = []{ for (unsigned " + ix + " = 0; " + ix +
        " < sizeof(" + nm + "); ++" + ix + ") " + nm + "[" + ix +
        "] ^= (char)(" + std::to_string(K1) + " + " + ix + " * " +
        std::to_string(K2) + "); return true; }();"
        " (void)" + on + "; return " + nm + "; }())";
    if (R_->ReplaceText(E->getSourceRange(), repl)) return;
    ++StringsObfuscated;
}

// 祖先链判定：字面量是否处于必须保持常量表达式/字面量的上下文。
// 命中则返回 true（该位置禁止运行时混淆）。
//  - case 标签 / 模板实参 / 枚举初值 / static_assert / sizeof / asm / 位域
//  - 声明的类型部分（数组界、模板参数默认值）——用“是否位于初始化器内”区分
//  - constexpr / const 声明（其初始化必须是常量）
//  - char a[] = "x" 一类数组推导（替换后推导失败）
//  - constexpr 函数体（lambda IIFE 破坏常量求值）
bool OpaquePass::badContext(const Expr *E) {
    auto Cur = Ctx_->getParents(*E);
    for (int depth = 0; depth < 24 && !Cur.empty(); ++depth) {
        const DynTypedNode &N = *Cur.begin();
        if (N.get<SwitchCase>() || N.get<GCCAsmStmt>() || N.get<MSAsmStmt>() ||
            N.get<StaticAssertDecl>() || N.get<EnumConstantDecl>() ||
            N.get<UnaryExprOrTypeTraitExpr>() || N.get<TemplateArgument>() ||
            N.get<UserDefinedLiteral>() || N.get<CoroutineBodyStmt>())
            return true;
        if (const auto *VD = N.get<VarDecl>()) {
            if (VD->isConstexpr() || VD->getType().isConstQualified())
                return true;
            // 数组类型：界表达式与 char a[]="x" 的推导都不能动
            if (VD->getType()->isArrayType()) return true;
            const Expr *init = VD->getAnyInitializer();
            if (!init) return true; // 没有初始化器 → 字面量位于类型部分
            if (SM_->getFileOffset(E->getBeginLoc()) <
                SM_->getFileOffset(init->getBeginLoc()))
                return true; // 位于初始化器之前（数组界）
        }
        if (N.get<FieldDecl>() || N.get<ParmVarDecl>() ||
            N.get<TypedefNameDecl>() || N.get<BindingDecl>() ||
            N.get<EnumDecl>())
            return true;
        if (const auto *FD = N.get<FunctionDecl>())
            return FD->isConstexpr(); // 到达函数边界：普通函数体内允许
        Cur = Ctx_->getParents(N);
    }
    return true; // 链走尽仍未到函数体（命名空间级初始化等）：保守禁止
}

} // namespace obf
