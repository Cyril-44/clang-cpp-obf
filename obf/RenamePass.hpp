// obf/RenamePass.hpp — C++20 用户定义名字全量重命名 pass
//
// 核心设计：**按“原始名 → 新名”的翻译单元级映射**（而非按 Decl 粒度）。
//   * 同名声明共享同一个新名：重载集、模板特化/实例化、依赖名引用
//     （T::member / obj.member 于模板中）天然保持一致；
//   * 同名遮蔽关系（内层变量遮蔽外层/成员/全局）在重命名后保持原有拓扑；
//   * 引用点仅在“目标声明写在主文件”时才替换，绝不触碰 std / 头文件名字；
//   * 依赖上下文中无法解析的名字（CXXDependentScopeMemberExpr 等）按名字映射
//     替换，且带“限定符/基类型属于本文件”的守卫，避免误伤 std::enable_if<>::type；
//   * 出现在宏展开中的名字自动“冻结”（保持原名），因为宏体文本无法重写；
//   * 新名字避开源文件中所有标识符、全部关键字与保留标识符，杜绝捕获。
//
// 覆盖的声明（NamedDecl 全类）：变量/函数/成员/枚举与枚举值/类型与别名/
// 命名空间与别名/模板（函数/类/变量/别名/模板模板参数）/概念/结构化绑定/
// 标签/using 声明等；引用点覆盖 DeclRefExpr、MemberExpr、各类 TypeLoc、
// 依赖表达式、构造/析构与初始化列表、指定初始化器、lambda 捕获、
// sizeof...(pack)、命名空间限定符等。
#pragma once

#include <clang/AST/AST.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Basic/LangOptions.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Rewrite/Core/Rewriter.h>

#include <llvm/Support/raw_ostream.h>

#include "Passes.hpp"
#include "Support.hpp"

#include <map>
#include <set>
#include <string>

using namespace clang;

namespace obf {

class RenamePass;

using clang::RecursiveASTVisitor;

class RenamerVisitor : public RecursiveASTVisitor<RenamerVisitor> {
public:
    RenamePass &P;
    bool Collect; // true: 第一遍收集名字; false: 第二遍改写
    RenamerVisitor(RenamePass &p, bool collect) : P(p), Collect(collect) {}

    // ---------------- 声明 ----------------
    bool VisitNamedDecl(NamedDecl *D);
    bool VisitCXXConstructorDecl(CXXConstructorDecl *C);

    // ---------------- 已解析引用 ----------------
    bool VisitDeclRefExpr(DeclRefExpr *E);
    bool VisitMemberExpr(MemberExpr *E);
    bool VisitLabelStmt(LabelStmt *S);
    bool VisitGotoStmt(GotoStmt *S);
    bool VisitSizeOfPackExpr(SizeOfPackExpr *E);
    bool VisitDesignatedInitExpr(DesignatedInitExpr *E);
    bool VisitConceptSpecializationExpr(ConceptSpecializationExpr *E);

    // ---------------- 依赖/未解析引用（按名字映射 + 本地性守卫） ----------------
    bool VisitCXXDependentScopeMemberExpr(CXXDependentScopeMemberExpr *E);
    bool VisitDependentScopeDeclRefExpr(DependentScopeDeclRefExpr *E);
    bool VisitUnresolvedLookupExpr(UnresolvedLookupExpr *E);
    bool VisitUnresolvedMemberExpr(UnresolvedMemberExpr *E);

    // ---------------- 类型位置 ----------------
    bool VisitTagTypeLoc(TagTypeLoc L);
    bool VisitTemplateSpecializationTypeLoc(TemplateSpecializationTypeLoc L);
    bool VisitTemplateTypeParmTypeLoc(TemplateTypeParmTypeLoc L);
    bool VisitTypedefTypeLoc(TypedefTypeLoc L);
    bool VisitUsingTypeLoc(UsingTypeLoc L);
    bool VisitDependentNameTypeLoc(DependentNameTypeLoc L);
    bool VisitDependentTemplateSpecializationTypeLoc(
        DependentTemplateSpecializationTypeLoc L);
    bool VisitInjectedClassNameTypeLoc(InjectedClassNameTypeLoc L);

    // ---------------- 结构化遍历钩子（CRTP 覆盖） ----------------
    bool TraverseNestedNameSpecifierLoc(NestedNameSpecifierLoc NNS);
    bool TraverseLambdaCapture(LambdaExpr *LE, const LambdaCapture *C,
                               Expr *Init);
};

class RenamePass : public ASTObfPass {
public:
    bool Compress = false;
    size_t MinLen = 12, MaxLen = 20;
    uint64_t Seed = 0;

    unsigned RenamedDecls = 0, FrozenNames = 0;

    const char *name() const override { return "rename"; }

    void run(clang::Rewriter &R, clang::ASTContext &Ctx) override {
        R_ = &R;
        Ctx_ = &Ctx;
        SM_ = &Ctx.getSourceManager();

        bool invalid = false;
        llvm::StringRef buf =
            SM_->getBufferData(SM_->getMainFileID(), &invalid);
        Forbidden_ = collectIdentifiers(buf.str());

        // 第一遍：收集可重命名名字与需要冻结的名字
        setCollecting(true);
        RenamerVisitor collector(*this, true);
        collector.TraverseDecl(Ctx.getTranslationUnitDecl());

        // 名字分配
        NameGenerator gen(Compress, MinLen, MaxLen, Seed, Forbidden_);
        for (const std::string &n : Names_) {
            if (Frozen_.count(n)) continue;
            Map_[n] = gen.next();
        }
        FrozenNames = (unsigned)Frozen_.size();
        RenamedDecls = (unsigned)Map_.size();

        // 第二遍：改写
        setCollecting(false);
        RenamerVisitor rewriter(*this, false);
        rewriter.TraverseDecl(Ctx.getTranslationUnitDecl());
    }

    // ============ 供 Visitor 调用的工具 ============

    clang::SourceManager &SM() { return *SM_; }

    bool inMain(clang::SourceLocation l) {
        return l.isValid() && !l.isMacroID() &&
               SM_->isWrittenInMainFile(l);
    }

    // 收集阶段：记录“出现在宏里”的名字，使其保持原名
    void noteMacroUse(llvm::StringRef name, clang::SourceLocation useLoc,
                      clang::SourceLocation declLoc = {}) {
        if (!CollectMode) return;
        if (name.empty()) return;
        if ((useLoc.isValid() && useLoc.isMacroID()) ||
            (declLoc.isValid() && declLoc.isMacroID()))
            Frozen_.insert(name.str());
    }

    void addName(llvm::StringRef name) { Names_.insert(name.str()); }

    bool collecting() const { return CollectMode; }
    void setCollecting(bool c) { CollectMode = c; }

    // 替换入口：校验 loc 处文本确为该名字且非标识符前缀后，执行替换
    bool renameAt(llvm::StringRef name, clang::SourceLocation loc) {
        if (CollectMode || name.empty() || !loc.isValid() || loc.isMacroID())
            return true;
        auto it = Map_.find(std::string(name));
        if (it == Map_.end()) return true;
        if (Done_.count(loc)) return true;
        if (!textMatchesAt(loc, name)) return true;
        Done_.insert(loc);
        R_->ReplaceText(loc, (unsigned)name.size(), it->second);
        return true;
    }

    // 智能替换：loc 处文本不精确匹配时，在 [loc, end) 窗口内扫描
    // （析构函数 ~Name、lambda 捕获 [&x]、基类初始化 : public Base 等场景）
    bool renameAtScan(llvm::StringRef name, clang::SourceLocation start,
                      clang::SourceLocation end) {
        if (CollectMode || name.empty() || !start.isValid()) return true;
        if (textMatchesAt(start, name)) return renameAt(name, start);
        if (!end.isValid() || !SM_->isWrittenInMainFile(start)) return true;
        const char *b = SM_->getCharacterData(start);
        const char *e = SM_->getCharacterData(end);
        if (!b || !e || e <= b) return true;
        std::string pat(name);
        for (const char *p = b; p + pat.size() <= e; ++p) {
            if (isIdentStart((unsigned char)*p) &&
                strncmp(p, pat.data(), pat.size()) == 0 &&
                !isIdentCont((unsigned char)p[pat.size()]) &&
                (p == b || !isIdentCont((unsigned char)p[-1]))) {
                clang::SourceLocation hit = start.getLocWithOffset(
                    (unsigned)(p - b));
                return renameAt(name, hit);
            }
        }
        return true;
    }

private:
    friend class RenamerVisitor;

    bool textMatchesAt(clang::SourceLocation loc, llvm::StringRef name) {
        if (!loc.isValid() || loc.isMacroID()) return false;
        if (!SM_->isWrittenInMainFile(loc)) return false;
        const char *p = SM_->getCharacterData(loc);
        if (!p) return false;
        if (strncmp(p, name.data(), name.size()) != 0) return false;
        return !isIdentCont((unsigned char)p[name.size()]);
    }

    // ---- 本地性判定：限定符/类型是否完全由本文件声明构成 ----
    bool nnsLocal(const clang::NestedNameSpecifier *N);
    bool typeLocal(const clang::Type *T);

public:
    clang::Rewriter *R_ = nullptr;
    clang::ASTContext *Ctx_ = nullptr;
    clang::SourceManager *SM_ = nullptr;

    std::set<std::string> Names_;   // 主文件中声明的名字
    std::set<std::string> Frozen_;  // 宏污染等需保持原名的名字
    std::set<std::string> Forbidden_;
    std::map<std::string, std::string> Map_;
    std::set<clang::SourceLocation> Done_;
    bool CollectMode = false;
};

// ============================ Visitor 实现 ============================

using namespace clang;

class RenamePass; // （已前置）

static inline bool renameDestructor(RenamePass &P, CXXDestructorDecl *D);

// 解析 UsingShadowDecl 得到真实目标
static inline const NamedDecl *throughShadows(const NamedDecl *D) {
    while (D && isa<UsingShadowDecl>(D))
        D = cast<UsingShadowDecl>(D)->getTargetDecl();
    return D;
}

bool RenamerVisitor::VisitNamedDecl(NamedDecl *D) {
    // 先做文件检查再取名字：头文件（尤其经由其内部宏）声明的 Decl
    // 不属于重命名范围，且其 IdentifierInfo 可能来自特殊构造，读取名字
    // 有崩溃风险；同时用 getFileLoc 把宏内定位解析回真实文件位置。
    SourceLocation loc = D->getLocation();
    if (!loc.isValid()) return true;
    if (!P.SM_->isWrittenInMainFile(P.SM_->getFileLoc(loc))) return true;
    bool inMacro = loc.isMacroID();

    // 取名字：无名声明（DeclarationName() 默认构造）的存储指针为 null，
    // 其 kind 位恰好读作 Identifier 且 getAsIdentifierInfo() 返回 null，
    // 因此必须以 II 指针判空，不能用 isIdentifier()。
    // 构造/析构函数的 DeclarationName 是特殊的 CXXConstructorName/
    // CXXDestructorName（II 也为 null），其文本即父类名。
    std::string nameStr;
    DeclarationName dn = D->getDeclName();
    if (const IdentifierInfo *II = dn.getAsIdentifierInfo()) {
        nameStr = II->getName().str();
    } else if (const auto *CD = dyn_cast<CXXConstructorDecl>(D)) {
        if (CD->getParent()) {
            if (const IdentifierInfo *PII =
                    CD->getParent()->getDeclName().getAsIdentifierInfo())
                nameStr = PII->getName().str();
        }
    } else if (const auto *DD = dyn_cast<CXXDestructorDecl>(D)) {
        if (DD->getParent()) {
            if (const IdentifierInfo *PII =
                    DD->getParent()->getDeclName().getAsIdentifierInfo())
                nameStr = PII->getName().str();
        }
    }
    llvm::StringRef name = nameStr;
    if (name.empty() || name.starts_with("operator")) return true;
    if (D->isImplicit()) return true;
    if (isa<UsingShadowDecl>(D) || isa<UsingDirectiveDecl>(D)) return true;

    if (inMacro) {
        P.Frozen_.insert(name.str()); // 宏中声明的名字：保持原名
        return true;
    }

    // C++20 type-constraint：template <IntegerWithI128 T> 中的概念名
    if (const auto *TP = dyn_cast<TemplateTypeParmDecl>(D)) {
        if (const auto *TC = TP->getTypeConstraint()) {
            const ConceptReference *CR = TC->getConceptReference();
            const NamedDecl *C = CR ? CR->getNamedConcept() : nullptr;
            if (C && C->getIdentifier()) {
                SourceLocation cloc = TC->getConceptNameLoc();
                if (P.collecting())
                    P.noteMacroUse(C->getName(), cloc, C->getLocation());
                else if (P.inMain(C->getLocation()))
                    P.renameAt(C->getName(), cloc);
            }
        }
    }

    if (P.collecting()) {
        if (const auto *FD = dyn_cast<FunctionDecl>(D))
            if (FD->isMain() || FD->isExternC()) return true;
        if (const auto *VD = dyn_cast<VarDecl>(D))
            if (VD->isExternC()) return true;
        P.addName(name);
        return true;
    }

    // 改写阶段
    if (const auto *DD = dyn_cast<CXXDestructorDecl>(D))
        return renameDestructor(P, const_cast<CXXDestructorDecl *>(DD));
    if (const auto *FD = dyn_cast<FunctionDecl>(D))
        if (FD->isMain() || FD->isExternC()) return true;
    if (const auto *VD = dyn_cast<VarDecl>(D))
        if (VD->isExternC()) return true;
    P.renameAt(name, loc);
    return true;
}

bool RenamerVisitor::VisitCXXConstructorDecl(CXXConstructorDecl *C) {
    // 构造函数声明处的名字即类名（由 VisitNamedDecl 处理），这里处理初始化列表：
    //   成员初始化 Foo() : member(expr)
    //   基类/委托初始化 Foo() : public Base(expr) / Foo() : Foo(expr)
    for (CXXCtorInitializer *init : C->inits()) {
        if (init->isBaseInitializer()) {
            const Type *T = init->getBaseClass();
            const NamedDecl *base = T ? T->getAsTagDecl() : nullptr;
            if (!base || !base->getIdentifier()) continue;
            if (!P.inMain(base->getLocation())) continue;
            SourceRange sr(C->getLocation(), init->getInit()
                                                  ? init->getInit()->getBeginLoc()
                                                  : SourceLocation());
            P.renameAtScan(base->getName(), sr.getBegin(), sr.getEnd());
        } else if (init->isDelegatingInitializer()) {
            CXXRecordDecl *RD = C->getParent();
            if (!RD || !RD->getIdentifier() || !P.inMain(RD->getLocation()))
                continue;
            SourceRange sr(C->getLocation(), init->getInit()
                                                  ? init->getInit()->getBeginLoc()
                                                  : SourceLocation());
            P.renameAtScan(RD->getName(), sr.getBegin(), sr.getEnd());
        } else if (init->isMemberInitializer() && init->getMember() &&
                   init->getMember()->getIdentifier()) {
            const NamedDecl *M = init->getMember();
            if (P.collecting()) {
                P.noteMacroUse(M->getName(), init->getMemberLocation(),
                               M->getLocation());
                continue;
            }
            if (!P.inMain(M->getLocation())) continue;
            P.renameAt(M->getName(), init->getMemberLocation());
        }
    }
    return true;
}

bool RenamerVisitor::VisitDeclRefExpr(DeclRefExpr *E) {
    const NamedDecl *D = throughShadows(E->getFoundDecl());
    if (!D || !D->getIdentifier()) return true;
    if (!P.SM_->isWrittenInMainFile(D->getLocation())) return true;
    if (P.collecting()) {
        P.noteMacroUse(D->getName(), E->getLocation(), D->getLocation());
        return true;
    }
    P.renameAt(D->getName(), E->getLocation());
    return true;
}

bool RenamerVisitor::VisitMemberExpr(MemberExpr *E) {
    const NamedDecl *D = throughShadows(E->getFoundDecl().getDecl());
    if (!D || !D->getIdentifier()) return true;
    if (!P.SM_->isWrittenInMainFile(D->getLocation())) return true;
    if (P.collecting()) {
        P.noteMacroUse(D->getName(), E->getMemberLoc(), D->getLocation());
        return true;
    }
    P.renameAt(D->getName(), E->getMemberLoc());
    return true;
}

bool RenamerVisitor::VisitLabelStmt(LabelStmt *S) {
    if (!S->getDecl() || !S->getDecl()->getIdentifier()) return true;
    if (P.collecting())
        P.noteMacroUse(S->getDecl()->getName(), S->getIdentLoc(),
                       S->getDecl()->getLocation());
    else
        P.renameAt(S->getDecl()->getName(), S->getIdentLoc());
    return true;
}

bool RenamerVisitor::VisitGotoStmt(GotoStmt *S) {
    if (!S->getLabel() || !S->getLabel()->getIdentifier()) return true;
    if (P.collecting())
        P.noteMacroUse(S->getLabel()->getName(), S->getLabelLoc(),
                       S->getLabel()->getLocation());
    else
        P.renameAt(S->getLabel()->getName(), S->getLabelLoc());
    return true;
}

bool RenamerVisitor::VisitSizeOfPackExpr(SizeOfPackExpr *E) {
    const NamedDecl *pack = E->getPack();
    if (!pack || !pack->getIdentifier()) return true;
    if (!P.SM_->isWrittenInMainFile(pack->getLocation())) return true;
    if (P.collecting())
        P.noteMacroUse(pack->getName(), E->getPackLoc(), pack->getLocation());
    else
        P.renameAt(pack->getName(), E->getPackLoc());
    return true;
}

bool RenamerVisitor::VisitDesignatedInitExpr(DesignatedInitExpr *E) {
    for (const DesignatedInitExpr::Designator &d : E->designators()) {
        if (!d.isFieldDesignator() || !d.getFieldName()) continue;
        SourceLocation loc = d.getFieldLoc();
        llvm::StringRef name = d.getFieldName()->getName();
        if (P.collecting()) {
            P.noteMacroUse(name, loc);
            continue;
        }
        if (!loc.isValid())
            P.renameAtScan(name, d.getDotLoc(), E->getEndLoc());
        else
            P.renameAt(name, loc);
    }
    return true;
}

bool RenamerVisitor::VisitConceptSpecializationExpr(
    ConceptSpecializationExpr *E) {
    const NamedDecl *C = E->getNamedConcept();
    if (!C || !C->getIdentifier()) return true;
    if (!P.SM_->isWrittenInMainFile(C->getLocation())) return true;
    if (P.collecting()) {
        P.noteMacroUse(C->getName(), E->getExprLoc(), C->getLocation());
        return true;
    }
    P.renameAtScan(C->getName(), E->getBeginLoc(), E->getEndLoc());
    return true;
}

// ---------- 依赖/未解析引用 ----------

bool RenamerVisitor::VisitCXXDependentScopeMemberExpr(
    CXXDependentScopeMemberExpr *E) {
    const IdentifierInfo *ii = E->getMember().getAsIdentifierInfo();
    if (!ii) return true;
    llvm::StringRef name = ii->getName();
    if (P.collecting()) {
        P.noteMacroUse(name, E->getMemberLoc());
        return true;
    }
    // 基类型或限定符涉及非本文件类型（如 std 容器）时不改；
    // 隐式 this 访问（isImplicitAccess）没有显式基表达式，只查限定符
    if (!E->isImplicitAccess() && E->getBase() &&
        !P.typeLocal(E->getBase()->getType().getTypePtr()))
        return true;
    if (E->getQualifier() && !P.nnsLocal(E->getQualifier())) return true;
    P.renameAt(name, E->getMemberLoc());
    return true;
}

bool RenamerVisitor::VisitDependentScopeDeclRefExpr(
    DependentScopeDeclRefExpr *E) {
    const IdentifierInfo *ii = E->getDeclName().getAsIdentifierInfo();
    if (!ii) return true;
    llvm::StringRef name = ii->getName();
    if (P.collecting()) {
        P.noteMacroUse(name, E->getLocation());
        return true;
    }
    if (E->getQualifier() && !P.nnsLocal(E->getQualifier())) return true;
    P.renameAtScan(name, E->getExprLoc(), E->getEndLoc());
    return true;
}

bool RenamerVisitor::VisitUnresolvedLookupExpr(UnresolvedLookupExpr *E) {
    const IdentifierInfo *ii = E->getName().getAsIdentifierInfo();
    if (!ii) return true;
    llvm::StringRef name = ii->getName();
    if (P.collecting()) {
        P.noteMacroUse(name, E->getNameLoc());
        return true;
    }
    if (E->getQualifier() && !P.nnsLocal(E->getQualifier())) return true;
    P.renameAt(name, E->getNameLoc());
    return true;
}

bool RenamerVisitor::VisitUnresolvedMemberExpr(UnresolvedMemberExpr *E) {
    const IdentifierInfo *ii = E->getName().getAsIdentifierInfo();
    if (!ii) return true;
    llvm::StringRef name = ii->getName();
    if (P.collecting()) {
        P.noteMacroUse(name, E->getNameLoc());
        return true;
    }
    QualType bt = E->getBaseType();
    if (!bt.isNull() && !P.typeLocal(bt.getTypePtr())) return true;
    if (E->getQualifier() && !P.nnsLocal(E->getQualifier())) return true;
    P.renameAt(name, E->getNameLoc());
    return true;
}

// ---------- 类型位置 ----------

bool RenamerVisitor::VisitTagTypeLoc(TagTypeLoc L) {
    if (L.isDefinition()) return true;
    NamedDecl *D = L.getDecl();
    if (!D || !P.inMain(D->getLocation())) return true;
    if (!D->getIdentifier()) return true;
    if (P.collecting())
        P.noteMacroUse(D->getName(), L.getNameLoc(), D->getLocation());
    else
        P.renameAt(D->getName(), L.getNameLoc());
    return true;
}

bool RenamerVisitor::VisitTemplateSpecializationTypeLoc(
    TemplateSpecializationTypeLoc L) {
    TemplateDecl *TD =
        L.getTypePtr()->getTemplateName().getAsTemplateDecl();
    if (!TD || !P.inMain(TD->getLocation())) return true;
    if (!TD->getIdentifier()) return true;
    if (P.collecting())
        P.noteMacroUse(TD->getName(), L.getTemplateNameLoc(),
                       TD->getLocation());
    else
        P.renameAt(TD->getName(), L.getTemplateNameLoc());
    return true;
}

bool RenamerVisitor::VisitTemplateTypeParmTypeLoc(TemplateTypeParmTypeLoc L) {
    TemplateTypeParmDecl *D = L.getDecl();
    if (!D || !P.inMain(D->getLocation())) return true;
    if (!D->getIdentifier()) return true;
    if (P.collecting())
        P.noteMacroUse(D->getName(), L.getNameLoc(), D->getLocation());
    else
        P.renameAt(D->getName(), L.getNameLoc());
    return true;
}

bool RenamerVisitor::VisitTypedefTypeLoc(TypedefTypeLoc L) {
    TypedefNameDecl *D = L.getTypedefNameDecl();
    if (!D || !P.inMain(D->getLocation())) return true;
    if (!D->getIdentifier()) return true;
    if (P.collecting())
        P.noteMacroUse(D->getName(), L.getNameLoc(), D->getLocation());
    else
        P.renameAt(D->getName(), L.getNameLoc());
    return true;
}

bool RenamerVisitor::VisitUsingTypeLoc(UsingTypeLoc L) {
    const NamedDecl *D =
        throughShadows(L.getTypePtr()->getFoundDecl());
    if (!D || !P.inMain(D->getLocation())) return true;
    if (!D->getIdentifier()) return true;
    if (P.collecting())
        P.noteMacroUse(D->getName(), L.getNameLoc(), D->getLocation());
    else
        P.renameAt(D->getName(), L.getNameLoc());
    return true;
}

bool RenamerVisitor::VisitDependentNameTypeLoc(DependentNameTypeLoc L) {
    const DependentNameType *T = L.getTypePtr();
    const IdentifierInfo *ii = T->getIdentifier();
    if (!ii) return true;
    llvm::StringRef name = ii->getName();
    if (P.collecting()) {
        P.noteMacroUse(name, L.getNameLoc());
        return true;
    }
    if (!P.nnsLocal(T->getQualifier())) return true;
    P.renameAt(name, L.getNameLoc());
    return true;
}

bool RenamerVisitor::VisitDependentTemplateSpecializationTypeLoc(
    DependentTemplateSpecializationTypeLoc L) {
    const DependentTemplateSpecializationType *T = L.getTypePtr();
    const IdentifierInfo *ii = T->getIdentifier();
    if (!ii) return true;
    llvm::StringRef name = ii->getName();
    if (P.collecting()) {
        P.noteMacroUse(name, L.getTemplateNameLoc());
        return true;
    }
    if (!P.nnsLocal(T->getQualifier())) return true;
    P.renameAt(name, L.getTemplateNameLoc());
    return true;
}

// 类模板定义内的自引用类型（如 PolyBase& 出现在 PolyBase 自己的定义里）
bool RenamerVisitor::VisitInjectedClassNameTypeLoc(InjectedClassNameTypeLoc L) {
    CXXRecordDecl *D = L.getDecl();
    if (!D || !P.inMain(D->getLocation())) return true;
    if (!D->getIdentifier()) return true;
    if (P.collecting())
        P.noteMacroUse(D->getName(), L.getNameLoc(), D->getLocation());
    else
        P.renameAt(D->getName(), L.getNameLoc());
    return true;
}

// ---------- 结构化遍历钩子 ----------

bool RenamerVisitor::TraverseNestedNameSpecifierLoc(NestedNameSpecifierLoc NNS) {
    if (NNS) {
        const NestedNameSpecifier *S = NNS.getNestedNameSpecifier();
        if (S->getKind() == NestedNameSpecifier::Namespace) {
            NamespaceDecl *ND = S->getAsNamespace();
            if (ND && ND->getIdentifier()) {
                if (P.collecting())
                    P.noteMacroUse(ND->getName(), NNS.getLocalBeginLoc(),
                                   ND->getLocation());
                else if (P.inMain(ND->getLocation()))
                    P.renameAt(ND->getName(), NNS.getLocalBeginLoc());
            }
        } else if (S->getKind() == NestedNameSpecifier::NamespaceAlias) {
            if (NamespaceAliasDecl *NA = S->getAsNamespaceAlias()) {
                if (P.collecting())
                    P.noteMacroUse(NA->getName(), NNS.getLocalBeginLoc(),
                                   NA->getLocation());
                else if (P.inMain(NA->getLocation()))
                    P.renameAt(NA->getName(), NNS.getLocalBeginLoc());
            }
        }
    }
    return RecursiveASTVisitor<RenamerVisitor>::TraverseNestedNameSpecifierLoc(
        NNS);
}

bool RenamerVisitor::TraverseLambdaCapture(LambdaExpr *LE,
                                           const LambdaCapture *C, Expr *Init) {
    if (C->capturesVariable()) {
        ValueDecl *V = C->getCapturedVar();
        if (V && V->getIdentifier()) {
            if (P.collecting()) {
                P.noteMacroUse(V->getName(), C->getLocation(),
                               V->getLocation());
            } else if (P.inMain(V->getLocation())) {
                // [&x] 时 getLocation 在 '&' 上，向后扫描
                P.renameAtScan(V->getName(), C->getLocation(),
                               LE->getIntroducerRange().getEnd());
            }
        }
    }
    return RecursiveASTVisitor<RenamerVisitor>::TraverseLambdaCapture(LE, C,
                                                                      Init);
}

// ---------- 析构函数：~Name ----------
// 放在文件末尾以 specialize：VisitNamedDecl 中已拦截，这里补充真正的改名逻辑。
// 做法：CXXDestructorDecl::getLocation() 指向 '~'（或 compl），从那里向后扫描类名。
static inline bool renameDestructor(RenamePass &P, CXXDestructorDecl *D) {
    CXXRecordDecl *RD = D->getParent();
    if (!RD || !RD->getIdentifier()) return true;
    llvm::StringRef name = RD->getName();
    if (P.collecting()) {
        P.noteMacroUse(name, D->getLocation(), D->getLocation());
        return true;
    }
    if (!P.inMain(D->getLocation()) || !P.inMain(RD->getLocation()))
        return true;
    SourceLocation loc = D->getLocation();
    if (loc.isValid() && !loc.isMacroID() && P.SM_->isWrittenInMainFile(loc)) {
        const char *p = P.SM_->getCharacterData(loc);
        // 跳过 '~' / '!' / "compl" / 空白
        size_t off = 0;
        while (p[off] == '~' || p[off] == '!' || p[off] == ' ' ||
               p[off] == '\t')
            ++off;
        P.renameAt(name, loc.getLocWithOffset((unsigned)off));
    }
    return true;
}

// ============================ 本地性判定 ============================

bool RenamePass::nnsLocal(const NestedNameSpecifier *N) {
    if (!N) return true;
    for (const NestedNameSpecifier *s = N; s; s = s->getPrefix()) {
        switch (s->getKind()) {
        case NestedNameSpecifier::Global:
        case NestedNameSpecifier::Identifier: // 依赖名（模板参数）：视作本地
            return true;
        case NestedNameSpecifier::Namespace:
            return s->getAsNamespace() &&
                   inMain(s->getAsNamespace()->getLocation());
        case NestedNameSpecifier::NamespaceAlias: {
            const auto *na = s->getAsNamespaceAlias();
            return na && inMain(na->getLocation());
        }
        case NestedNameSpecifier::TypeSpec:
            return typeLocal(s->getAsType());
        case NestedNameSpecifier::Super:
            return false;
        }
    }
    return true;
}

bool RenamePass::typeLocal(const Type *T) {
    if (!T) return true;
    if (isa<TemplateTypeParmType>(T)) return true;
    if (const auto *ET = dyn_cast<ElaboratedType>(T))
        return typeLocal(ET->getNamedType().getTypePtr());
    if (const auto *AT = dyn_cast<AutoType>(T)) {
        QualType d = AT->getDeducedType();
        return d.isNull() ? false : typeLocal(d.getTypePtr());
    }
    if (const auto *DT = dyn_cast<DecltypeType>(T))
        return typeLocal(DT->getUnderlyingType().getTypePtr());
    if (const auto *PT = dyn_cast<PointerType>(T))
        return typeLocal(PT->getPointeeType().getTypePtr());
    if (const auto *RT = dyn_cast<ReferenceType>(T))
        return typeLocal(RT->getPointeeType().getTypePtr());
    if (const auto *MPT = dyn_cast<MemberPointerType>(T))
        return typeLocal(MPT->getPointeeType().getTypePtr());
    if (const auto *AT = dyn_cast<ArrayType>(T))
        return typeLocal(AT->getElementType().getTypePtr());
    if (const auto *TT = dyn_cast<TagType>(T)) {
        const TagDecl *D = TT->getDecl();
        return D && inMain(D->getLocation());
    }
    if (const auto *IC = dyn_cast<InjectedClassNameType>(T)) {
        const TagDecl *D = IC->getDecl();
        return D && inMain(D->getLocation());
    }
    if (const auto *TD = dyn_cast<TypedefType>(T)) {
        const TypedefNameDecl *D = TD->getDecl();
        return D && inMain(D->getLocation());
    }
    if (const auto *UT = dyn_cast<UsingType>(T)) {
        const NamedDecl *D = throughShadows(UT->getFoundDecl());
        return D && inMain(D->getLocation());
    }
    if (const auto *TS = dyn_cast<TemplateSpecializationType>(T)) {
        const TemplateDecl *TD = TS->getTemplateName().getAsTemplateDecl();
        if (TD && !inMain(TD->getLocation())) return false;
        for (const TemplateArgument &arg : TS->template_arguments()) {
            if (arg.getKind() == TemplateArgument::Type &&
                !typeLocal(arg.getAsType().getTypePtr()))
                return false;
        }
        return true;
    }
    if (const auto *DN = dyn_cast<DependentNameType>(T))
        return nnsLocal(DN->getQualifier());
    if (const auto *DTS = dyn_cast<DependentTemplateSpecializationType>(T))
        return nnsLocal(DTS->getQualifier());
    if (const auto *PT = dyn_cast<ParenType>(T))
        return typeLocal(PT->getInnerType().getTypePtr());
    // 未知种类：保守视作非本地（不改名）
    return false;
}

} // namespace obf
