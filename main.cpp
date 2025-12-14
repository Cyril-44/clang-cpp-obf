// main_refactored.cpp
// 重构自用户上传的 main.cpp，目标：清晰化函数边界、命名、并移除重复代码。
// 保持原有功能（基于 Clang 的重命名与字面量混淆）的大体流程。

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <random>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "clang/AST/AST.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/Expr.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/Stmt.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Index/USRGeneration.h"
#include "clang/Lex/PPCallbacks.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Error.h"
#include "clang/Basic/CharInfo.h"


using namespace clang;
using namespace clang::tooling;
using namespace llvm;
using std::string;

// -------------------- 小工具 / 类型别名 --------------------
using StringVec = std::vector<string>;

// -------------------- NameGenerator --------------------
class NameGenerator {
public:
  explicit NameGenerator(size_t len = 32, const std::string &pref = "")
      : length(len), prefix(pref) {
    seedRng();
    buildCharPools();
    distAll = std::uniform_int_distribution<size_t>(0, alphaNum.size() - 1);
    distFirst = std::uniform_int_distribution<size_t>(0, firstChar.size() - 1);
  }

  std::string generate() {
    std::string s;
    s.reserve(prefix.size() + length);
    s.push_back(firstChar[distFirst(rng)]);
    for (size_t i = 1; i < length; ++i)
      s.push_back(alphaNum[distAll(rng)]);
    if (!prefix.empty())
      s = prefix + s;

    // 确保唯一
    for (int tries = 0; !used.insert(s).second; ++tries) {
      if (tries > 16) {
        s += '_' + std::to_string(tries);
        if (used.insert(s).second)
          break;
      }
      s.clear();
      s.push_back(firstChar[distFirst(rng)]);
      for (size_t i = 1; i < length; ++i)
        s.push_back(alphaNum[distAll(rng)]);
      if (!prefix.empty())
        s = prefix + s;
    }
    return s;
  }

  void reset() { used.clear(); }

private:
  void seedRng() {
    auto now = std::chrono::high_resolution_clock::now().time_since_epoch();
    uint64_t seed = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
    rng.seed(seed);
  }
  void buildCharPools() {
    for (char c = 'A'; c <= 'Z'; ++c)
      alphaNum.push_back(c);
    for (char c = 'a'; c <= 'z'; ++c)
      alphaNum.push_back(c);
    for (char c = '0'; c <= '9'; ++c)
      alphaNum.push_back(c);
    for (char c = 'A'; c <= 'Z'; ++c)
      firstChar.push_back(c);
    for (char c = 'a'; c <= 'z'; ++c)
      firstChar.push_back(c);
    firstChar.push_back('_');
  }

  size_t length;
  std::string prefix;
  std::mt19937_64 rng;
  std::vector<char> alphaNum;
  std::vector<char> firstChar;
  std::unordered_set<std::string> used;
  std::uniform_int_distribution<size_t> distAll;
  std::uniform_int_distribution<size_t> distFirst;
};

// -------------------- ObfDataManager --------------------
// 负责生成并缓存用于混淆的静态定义（字符串、整数、字符），以及在需要时
// 将这些定义注入到翻译单元开头。
struct ObfDataManager {
  Rewriter &R;
  NameGenerator &gen;
  const SourceManager &SM;

  ObfDataManager(Rewriter &R, NameGenerator &g, const SourceManager &SM)
      : R(R), gen(g), SM(SM) {}

  // 生成单字节 key
  static uint8_t chooseKey() {
    static std::mt19937_64 rng(
        (uint64_t)std::chrono::high_resolution_clock::now()
            .time_since_epoch()
            .count());
    return static_cast<uint8_t>(rng() & 0xFF);
  }

  // 为字符串生成静态编码数组并返回存储标识 "name:hexKey"
  std::string makeStrVar(const std::string &s) {
    auto it = strMap.find(s);
    if (it != strMap.end())
      return it->second;

    std::string var = gen.generate();
    uint8_t key = chooseKey();

    // 用 key 对 bytes 做 XOR 编码并构建数组字面量
    std::string arr = "static constexpr unsigned char " + var + "[] = {";
    for (unsigned char c : s) {
      unsigned char b = static_cast<unsigned char>(c ^ key);
      char buf[8];
      snprintf(buf, sizeof(buf), "0x%02x", (unsigned)b);
      arr += buf;
      arr += ',';
    }
    // 终止符
    char buf0[8];
    snprintf(buf0, sizeof(buf0), "0x%02x", (unsigned)(0 ^ key));
    arr += buf0;
    arr += "};\n";

    // 延迟注入：把解码器和数组追加到 headerInsert，之后统一插入
    if (!injected) {
      decodeFuncName = gen.generate();
      headerInsert += "static inline const char* ";
      headerInsert +=
          decodeFuncName + "(const unsigned char* p, unsigned char b) {";
      headerInsert +=
          " static char a[1<<10]; for (unsigned long i=0;;++i){ unsigned char "
          "c=p[i]; unsigned char d = (unsigned char)(c^b); a[i]= (char)d; if "
          "(c==0x00){ a[i+1]='\\0'; break;} } return a; }\n";
      injected = true;
    }
    headerInsert += arr;

    char ks[8];
    snprintf(ks, sizeof(ks), "%02x", key);
    std::string stored = var + ":" + std::string(ks);
    strMap[s] = stored;
    return stored;
  }

  // 整数混淆存储：生成一个静态常量（XOR 掩码）
  std::string makeIntVar(long long v) {
    auto it = intMap.find(v);
    if (it != intMap.end())
      return it->second;
    std::string var = gen.generate();
    // 随机生成 64-bit key（低成本）
    uint64_t key = ((uint64_t)chooseKey() << 56) ^
                   ((uint64_t)chooseKey() << 48) ^
                   ((uint64_t)chooseKey() << 40);
    uint64_t encoded = (uint64_t)v ^ key;
    char buf[128];
    snprintf(buf, sizeof(buf),
             "static constexpr unsigned long long %s = 0x%llxULL;\n",
             var.c_str(), (unsigned long long)encoded);
    headerInsert += buf;
    char kbuf[64];
    snprintf(kbuf, sizeof(kbuf), "%llx", (unsigned long long)key);
    std::string stored = var + ":" + std::string(kbuf);
    intMap[v] = stored;
    return stored;
  }

  // 字符字面量混淆
  std::string makeCharVar(char c) {
    int ci = static_cast<unsigned char>(c);
    auto it = charMap.find(ci);
    if (it != charMap.end())
      return it->second;
    std::string var = gen.generate();
    uint8_t key = chooseKey();
    unsigned char enc = static_cast<unsigned char>(c ^ key);
    char buf[128];
    snprintf(buf, sizeof(buf), "static constexpr unsigned char %s = 0x%02x;\n",
             var.c_str(), (unsigned)enc);
    headerInsert += buf;
    char kbuf[8];
    snprintf(kbuf, sizeof(kbuf), "%02x", key);
    std::string stored = var + ":" + std::string(kbuf);
    charMap[ci] = stored;
    return stored;
  }

  // 若 headerInsert 非空，则把其注入到主文件文件头
  void injectHeaderIfNeeded(const FileID &FID) {
    if (headerInsert.empty())
      return;
    SourceLocation start = SM.getLocForStartOfFile(FID);
    R.InsertText(start, headerInsert, true, true);
    headerInsert.clear();
  }

  // 公有缓存（供其它模块读取）
  std::unordered_map<std::string, std::string> strMap;
  std::unordered_map<long long, std::string> intMap;
  std::unordered_map<int, std::string> charMap;

private:
  // Rewriter &R;
  // NameGenerator &gen;
  // const SourceManager &SM;
  bool injected = false;
  string decodeFuncName;
  string headerInsert;
};

// -------------------- 映射文件辅助 --------------------
static bool loadMapping(const std::string &path,
                        std::unordered_map<std::string, std::string> &mapOut) {
  if (path.empty())
    return true;
  std::ifstream in(path);
  if (!in.is_open())
    return false;
  std::string a, b;
  while (in >> a >> b)
    mapOut[a] = b;
  return true;
}
static bool
saveMapping(const std::string &path,
            const std::unordered_map<std::string, std::string> &mapIn) {
  if (path.empty())
    return true;
  std::ofstream out(path + ".tmp");
  if (!out.is_open())
    return false;
  for (auto &p : mapIn)
    out << p.first << ' ' << p.second << '\n';
  out.close();
  std::rename((path + ".tmp").c_str(), path.c_str());
  return true;
}

// -------------------- FullRenamer（AST 访问器） --------------------
class FullRenamer : public RecursiveASTVisitor<FullRenamer> {
public:
  FullRenamer(Rewriter &R, NameGenerator &G,
              std::unordered_map<std::string, std::string> &usrMap,
              const SourceManager &SM, const LangOptions &LO,
              bool allowExternal)
      : R(R), gen(G), usrToObf(usrMap), SM(SM), LO(LO), allowExternal(allowExternal) {}

  // 一些可复用的检查
  bool isInMainFile(SourceLocation L) const {
    if (!L.isValid())
      return false;
    return SM.isWrittenInMainFile(SM.getSpellingLoc(L));
  }

  std::string getDeclUSR(const Decl *D) const {
    SmallString<128> buf;
    if (index::generateUSRForDecl(D, buf))
      return {};
    return string(buf.begin(), buf.end());
  }

  // 统一改名入口：在合适的位置创建 mapping 并替换声明处文本
  void applyRenameIfNeeded(const Decl *D, SourceLocation declLoc) {
    if (!D || !declLoc.isValid())
      return;
    std::string usr = getDeclUSR(D);
    if (usr.empty())
      return;
    // 若已存在映射，不再重复生成
    if (usrToObf.count(usr)) {
      // 仍然确保源代码上的声明处被替换（防止某些场景只替换了引用）
      SourceLocation SL = SM.getSpellingLoc(declLoc);
      if (SL.isValid() && (isInMainFile(SL) || allowExternal))
        R.ReplaceText(CharSourceRange::getTokenRange(SL), usrToObf[usr]);
      return;
    }

    std::string obf = gen.generate();
    usrToObf[usr] = obf;

    SourceLocation SL = SM.getSpellingLoc(declLoc);
    if (!SL.isValid())
      return;
    if (!isInMainFile(SL) && !allowExternal)
      return;
    R.ReplaceText(CharSourceRange::getTokenRange(SL), obf);
  }

  // 访问函数/方法
  bool VisitFunctionDecl(FunctionDecl *FD) {
    if (!FD || FD->isImplicit() || FD->isInvalidDecl())
      return true;
    if (!FD->getIdentifier())
      return true;
    if (isa<CXXConstructorDecl>(FD) || isa<CXXDestructorDecl>(FD) ||
        isa<CXXConversionDecl>(FD))
      return true;

    FunctionDecl *Def =
        FD->isThisDeclarationADefinition() ? FD : FD->getDefinition();
    if (!Def)
      return true;

    SourceLocation spellLoc = SM.getSpellingLoc(Def->getLocation());
    if (!spellLoc.isValid())
      return true;
    if (!SM.isWrittenInMainFile(spellLoc) && !allowExternal)
      return true;
    if (!allowExternal && Def->hasExternalFormalLinkage())
      return true;

    if (Def->isTemplateInstantiation())
      return true;

    applyRenameIfNeeded(Def, spellLoc);
    return true;
  }

  bool VisitCXXConstructorDecl(CXXConstructorDecl *Ctor) {
    if (!Ctor || Ctor->isImplicit())
      return true;
    SourceLocation ctorSpell = SM.getSpellingLoc(Ctor->getLocation());
    if (!ctorSpell.isValid())
      return true;
    if (!SM.isWrittenInMainFile(ctorSpell) && !allowExternal)
      return true;

    // 用父类映射替换构造函数名（如果映射尚不存在则创建它）
    if (const CXXRecordDecl *Parent = Ctor->getParent()) {
      if (!Parent->isImplicit() && Parent->getIdentifier()) {
        std::string recUSR = getDeclUSR(Parent);
        if (!recUSR.empty()) {
          auto it = usrToObf.find(recUSR);
          if (it == usrToObf.end()) {
            // 生成并注入 Parent 的映射（如果适用）
            applyRenameIfNeeded(Parent, Parent->getLocation());
            it = usrToObf.find(recUSR);
          }
          if (it != usrToObf.end()) {
            R.ReplaceText(CharSourceRange::getTokenRange(ctorSpell),
                          it->second);
          }
        }
      }
    }

    // 替换成员初始化器名字（只使用已有映射）
    for (auto *Init : Ctor->inits()) {
      if (!Init || !Init->isMemberInitializer())
        continue;
      FieldDecl *FD = Init->getMember();
      if (!FD || FD->isImplicit() || !FD->getIdentifier())
        continue;
      SourceLocation nameLoc = Init->getMemberLocation();
      if (!nameLoc.isValid())
        nameLoc = Init->getSourceLocation();
      if (!nameLoc.isValid())
        continue;
      nameLoc = SM.getSpellingLoc(nameLoc);
      if (!nameLoc.isValid())
        continue;
      if (!SM.isWrittenInMainFile(nameLoc) && !allowExternal)
        continue;
      std::string fldUSR = getDeclUSR(FD);
      if (fldUSR.empty())
        continue;
      auto it = usrToObf.find(fldUSR);
      if (it == usrToObf.end())
        continue;
      R.ReplaceText(CharSourceRange::getTokenRange(nameLoc), it->second);
    }
    return true;
  }

  bool VisitCXXRecordDecl(CXXRecordDecl *RD) {
    if (!RD || RD->isImplicit() || RD->isAnonymousStructOrUnion() ||
        !RD->getIdentifier())
      return true;
    const TagDecl *Target = RD->getDefinition() ? RD->getDefinition() : RD;
    SourceLocation nameLoc = Target->getLocation();
    nameLoc = SM.getSpellingLoc(nameLoc);
    if (!nameLoc.isValid())
      return true;
    // 这里放宽判断：只要名字在主文件或者允许处理外部符号，就创建/替换映射。
    if (!isInMainFile(nameLoc) && !allowExternal)
      return true;
    applyRenameIfNeeded(Target, nameLoc);
    return true;
  }

  bool VisitVarDecl(VarDecl *VD) {
    if (!VD || !VD->getIdentifier())
      return true;
    if (!isInMainFile(VD->getLocation()))
      return true;
    if (!allowExternal && VD->hasExternalStorage())
      return true;
    applyRenameIfNeeded(VD, VD->getLocation());
    return true;
  }

  bool VisitFieldDecl(FieldDecl *FD) {
    if (!FD || FD->isImplicit() || !FD->getIdentifier())
      return true;
    SourceLocation loc = SM.getSpellingLoc(FD->getLocation());
    if (!loc.isValid() || !isInMainFile(loc))
      return true;
    applyRenameIfNeeded(FD, loc);
    return true;
  }

  bool VisitEnumDecl(EnumDecl *ED) {
    if (!ED || !ED->getIdentifier())
      return true;
    if (!isInMainFile(ED->getLocation()))
      return true;
    if (!allowExternal && ED->hasExternalFormalLinkage())
      return true;
    applyRenameIfNeeded(ED, ED->getLocation());
    return true;
  }

  bool VisitTypedefNameDecl(TypedefNameDecl *TD) {
    if (!TD || !TD->getIdentifier())
      return true;
    if (!isInMainFile(TD->getLocation()))
      return true;
    applyRenameIfNeeded(TD, TD->getLocation());
    return true;
  }

  bool VisitDeclRefExpr(DeclRefExpr *DRE) {
    ValueDecl *VD = DRE->getDecl();
    if (!VD || !VD->getIdentifier())
      return true;
    if (!isInMainFile(DRE->getLocation()))
      return true;
    std::string usr = getDeclUSR(VD);
    if (usr.empty())
      return true;
    auto it = usrToObf.find(usr);
    if (it == usrToObf.end())
      return true;
    SourceLocation SL = SM.getSpellingLoc(DRE->getLocation());
    if (!SL.isValid())
      return true;
    R.ReplaceText(CharSourceRange::getTokenRange(SL), it->second);
    return true;
  }

  bool VisitMemberExpr(MemberExpr *ME) {
    ValueDecl *VD = ME->getMemberDecl();
    if (!VD || !VD->getIdentifier())
      return true;
    if (!isInMainFile(ME->getMemberLoc()))
      return true;
    std::string usr = getDeclUSR(VD);
    if (usr.empty())
      return true;
    auto it = usrToObf.find(usr);
    if (it == usrToObf.end())
      return true;
    SourceLocation SL = SM.getSpellingLoc(ME->getMemberLoc());
    if (!SL.isValid())
      return true;
    R.ReplaceText(CharSourceRange::getTokenRange(SL), it->second);
    return true;
  }

  bool VisitTypeLoc(TypeLoc TL) {
    if (!TL.getBeginLoc().isValid())
      return true;

    SourceLocation beginSpell = SM.getSpellingLoc(TL.getBeginLoc());
    if (!beginSpell.isValid())
      return true;
    if (!isInMainFile(beginSpell) && !allowExternal)
      return true;

    auto isWritableLoc = [&](SourceLocation Loc) -> bool {
      if (!Loc.isValid())
        return false;
      Loc = SM.getFileLoc(Loc); // 归一为文件位置
      if (!Loc.isValid())
        return false;
      if (Loc.isMacroID())
        return false; // 跳过宏位置
      if (!isInMainFile(Loc) && !allowExternal)
        return false;
      return !SM.getFileID(Loc).isInvalid();
    };

    auto safeReplaceToken = [&](SourceLocation Loc, llvm::StringRef NewText) {
      if (!isWritableLoc(Loc))
        return;
      Loc = SM.getFileLoc(Loc);

      CharSourceRange TR = CharSourceRange::getTokenRange(Loc);
      llvm::StringRef Old =
          clang::Lexer::getSourceText(TR, SM, /*LangOpts*/ LO);
      if (Old.empty())
        return;
      auto isIdent = [](llvm::StringRef S) {
        if (S.empty()) return false;
        if (!clang::isIdentifierHead(S.front(), /*LangOpts*/ true)) return false;
        for (char c : S.drop_front())
          if (!clang::isIdentifierBody(c, /*LangOpts*/ true)) return false;
        return true;
      };

      if (!isIdent(Old))
        return; // 只改标识符，避免误伤符号

      R.ReplaceText(TR, NewText);
    };

    // 处理 tag 类型（struct/class/union/enum）
    if (TagTypeLoc tagTL = TL.getAs<TagTypeLoc>()) {
      if (const TagDecl *TD = tagTL.getDecl()) {
        if (!TD->isImplicit() && TD->getIdentifier()) {
          std::string usr = getDeclUSR(TD);
          if (!usr.empty()) {
            auto it = usrToObf.find(usr);
            if (it == usrToObf.end()) {
              // 若无映射则在声明处创建（受 allowExternal 控制）
              applyRenameIfNeeded(TD, TD->getLocation());
              it = usrToObf.find(usr);
            }
            if (it != usrToObf.end()) {
              // TagTypeLoc
              SourceLocation nameLoc = tagTL.getNameLoc();
              if (!nameLoc.isValid()) nameLoc = TL.getBeginLoc();
              nameLoc = SM.getSpellingLoc(nameLoc);
              safeReplaceToken(nameLoc, it->second);
            }
          }
        }
      }
    }

    // 处理 typedef / alias 引用（只改该引用处的名字）
    if (TypedefTypeLoc tdTL = TL.getAs<TypedefTypeLoc>()) {
      if (TypedefNameDecl *TND = tdTL.getTypedefNameDecl()) {
        if (!TND->isImplicit() && TND->getIdentifier()) {
          std::string usr = getDeclUSR(TND);
          if (!usr.empty()) {
            auto it = usrToObf.find(usr);
            if (it == usrToObf.end()) {
              applyRenameIfNeeded(TND, TND->getLocation());
              it = usrToObf.find(usr);
            }
            if (it != usrToObf.end()) {
              SourceLocation nameLoc = tdTL.getNameLoc();
              if (!nameLoc.isValid())
                nameLoc = TL.getBeginLoc();
              nameLoc = SM.getSpellingLoc(nameLoc);
              safeReplaceToken(nameLoc, it->second);
            }
          }
        }
      }
    }

    return true;
  }

  // 字符串 / 字符 / 整数 处理 -- 委托给 obfMgr
  bool VisitStringLiteral(clang::StringLiteral *SLit) {
    if (!SLit || !isInMainFile(SLit->getBeginLoc()) || !obfMgr)
      return true;
    if (SLit->isWide())
      return true; // skip wide
    string s = SLit->getString().str();
    string stored = obfMgr->makeStrVar(s);
    auto pos = stored.find(':');
    string var = stored.substr(0, pos);
    string keyhex = stored.substr(pos + 1);
    char repl[512];
    snprintf(repl, sizeof(repl),
             "(reinterpret_cast<const char*>(%s(%s, 0x%s)))",
             obfMgrDecodeName().c_str(), var.c_str(), keyhex.c_str());
    R.ReplaceText(CharSourceRange::getTokenRange(SLit->getBeginLoc()), repl);
    return true;
  }

  bool VisitCharacterLiteral(CharacterLiteral *CL) {
    if (!CL || !isInMainFile(CL->getLocation()) || !obfMgr)
      return true;
    unsigned val = CL->getValue();
    char ch = static_cast<char>(val & 0xFF);
    string stored = obfMgr->makeCharVar(ch);
    auto pos = stored.find(':');
    string var = stored.substr(0, pos);
    string keyhex = stored.substr(pos + 1);
    char repl[128];
    snprintf(repl, sizeof(repl), "((char)((unsigned char)(%s) ^ 0x%s))",
             var.c_str(), keyhex.c_str());
    R.ReplaceText(CharSourceRange::getTokenRange(CL->getLocation()), repl);
    return true;
  }

  bool VisitIntegerLiteral(IntegerLiteral *IL) {
    if (!IL || !isInMainFile(IL->getLocation()) || !obfMgr || !Ctx)
      return true;
    // 保守地跳过 enum / non-type template 参数 / case label
    auto parents = Ctx->getParents(ast_type_traits::DynTypedNode::create(*IL));
    for (auto &P : parents) {
      if (P.get<EnumConstantDecl>() || P.get<NonTypeTemplateParmDecl>() ||
          P.get<CaseStmt>())
        return true;
    }
    APInt api = IL->getValue();
    if (api.getBitWidth() > 63)
      return true;
    long long v = api.getSExtValue();

    string stored = obfMgr->makeIntVar(v);
    auto pos = stored.find(':');
    string var = stored.substr(0, pos);
    string keyhex = stored.substr(pos + 1);
    string typeStr = IL->getType().getAsString();
    char repl[256];
    snprintf(repl, sizeof(repl), "((%s)(((unsigned long long)%s) ^ 0x%sULL))",
             typeStr.c_str(), var.c_str(), keyhex.c_str());
    R.ReplaceText(CharSourceRange::getTokenRange(IL->getLocation()), repl);
    return true;
  }

  // 获取 decode 函数名字（从 obfMgr 的 headerInsert 中生成的名称）
  std::string obfMgrDecodeName() const { return "__cppobf_decode"; }

public:
  Rewriter &R;
  NameGenerator &gen;
  std::unordered_map<std::string, std::string> &usrToObf;
  const SourceManager &SM;
  bool allowExternal;
  const LangOptions &LO;

  // 外部注入点
  ObfDataManager *obfMgr = nullptr;
  ASTContext *Ctx = nullptr;
};

// -------------------- RenamerConsumer --------------------
class RenamerConsumer : public ASTConsumer {
public:
  RenamerConsumer(Rewriter &R, NameGenerator &G,
                  std::unordered_map<std::string, std::string> &usrMap,
                  const SourceManager &SM, const LangOptions &LO,
                  bool allowExternal)
      : obfManager(R, G, SM), renamer(R, G, usrMap, SM, LO, allowExternal) {
    renamer.obfMgr = &obfManager;
  }

  void HandleTranslationUnit(ASTContext &Context) override {
    renamer.Ctx = &Context;
    renamer.TraverseDecl(Context.getTranslationUnitDecl());
    FileID fid = Context.getSourceManager().getMainFileID();
    obfManager.injectHeaderIfNeeded(fid);
  }

private:
  ObfDataManager obfManager;
  FullRenamer renamer;
};

// -------------------- FrontendAction / Factory --------------------
class ObfuscateAction : public ASTFrontendAction {
public:
  ObfuscateAction(NameGenerator &G,
                  std::unordered_map<std::string, std::string> &usrMap,
                  std::unordered_map<std::string, std::string> &macroMap,
                  bool processMacros, bool allowExternal)
      : gen(G), usrMap(usrMap), macroMap(macroMap),
        processMacros(processMacros), allowExternal(allowExternal) {}

  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                                 StringRef file) override {
    rewriter_.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
    return std::make_unique<RenamerConsumer>(rewriter_, gen, usrMap,
                                             CI.getSourceManager(),
                                             CI.getLangOpts(), allowExternal);
  }

  void EndSourceFileAction() override {
    FileID ID = rewriter_.getSourceMgr().getMainFileID();
    std::string out;
    llvm::raw_string_ostream os(out);
    rewriter_.getEditBuffer(ID).write(os);
    finalResult = os.str();
    llvm::outs() << "Wrote obfuscated source to OBF_raw.cpp\n";
  }

private:
  Rewriter rewriter_;
  NameGenerator &gen;
  std::unordered_map<std::string, std::string> &usrMap;
  std::unordered_map<std::string, std::string> &macroMap;
  bool processMacros;
  bool allowExternal;

public:
  static std::string finalResult;
};
std::string ObfuscateAction::finalResult;

class ObfuscateActionFactory : public FrontendActionFactory {
public:
  ObfuscateActionFactory(NameGenerator &G,
                         std::unordered_map<std::string, std::string> &usrMap,
                         std::unordered_map<std::string, std::string> &macroMap,
                         bool processMacros, bool allowExternal)
      : gen(G), usrMap(usrMap), macroMap(macroMap),
        processMacros(processMacros), allowExternal(allowExternal) {}

  std::unique_ptr<FrontendAction> create() override {
    return std::make_unique<ObfuscateAction>(gen, usrMap, macroMap,
                                             processMacros, allowExternal);
  }

private:
  NameGenerator &gen;
  std::unordered_map<std::string, std::string> &usrMap;
  std::unordered_map<std::string, std::string> &macroMap;
  bool processMacros;
  bool allowExternal;
};

// -------------------- Define_Obfuscation (tokenization + tree builder)
// --------------------
namespace Define_Obfuscation {
using std::pair;
using std::vector;
using Edge = std::pair<string, std::vector<string>>;

// 把源码拆成 token 与预处理指令
pair<vector<string>, vector<string>>
splitCppTokensWithDirectives(const string &s) {
  static const std::unordered_set<string> two = {
      "==", "!=", "<=", "=>", "&&", "||", "++", "--", "+=", "-=", "*=",
      "/=", "%=", "&=", "|=", "^=", "<<", ">>", "->", "::", ".*", "->*"};
  static const std::unordered_set<char> one = {
      '+', '-', '*', '/', '%', '&', '|', '^', '~', '!', '=', '<',
      '>', '(', ')', '[', ']', '{', '}', ',', ';', '.', ':', '?'};

  vector<string> tokens;
  tokens.reserve(1024);
  vector<string> directives;
  string cur;
  enum State {
    Normal,
    InStr,
    InChar,
    LineCmt,
    BlockCmt,
    InDirective
  } st = Normal;
  for (size_t i = 0; i < s.size(); ++i) {
    char c = s[i];
    if (st == InStr) {
      cur.push_back(c);
      if (c == '"' && s[i - 1] != '\\') {
        tokens.push_back(cur);
        cur.clear();
        st = Normal;
      }
      continue;
    }
    if (st == InChar) {
      cur.push_back(c);
      if (c == '\'' && s[i - 1] != '\\') {
        tokens.push_back(cur);
        cur.clear();
        st = Normal;
      }
      continue;
    }
    if (st == LineCmt) {
      if (c == '\n')
        st = Normal;
      continue;
    }
    if (st == BlockCmt) {
      if (c == '*' && i + 1 < s.size() && s[i + 1] == '/') {
        ++i;
        st = Normal;
      }
      continue;
    }
    if (st == InDirective) { // 收集整行，允许续行
      size_t start = i;
      bool cont = false;
      for (; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size() && s[i + 1] == '\n') {
          ++i;
          cont = true;
          continue;
        }
        if (s[i] == '\n')
          break;
      }
      size_t end = (i < s.size() ? i : s.size() - 1);
      string dir = s.substr(start, end - start + 1);
      if (!dir.empty() && dir.back() == '\n')
        dir.pop_back();
      directives.push_back(dir);
      st = Normal;
      continue;
    }

    // Normal
    if (c == '"') {
      cur.push_back(c);
      st = InStr;
      continue;
    }
    if (c == '\'') {
      cur.push_back(c);
      st = InChar;
      continue;
    }
    if (c == '/' && i + 1 < s.size() && s[i + 1] == '/') {
      st = LineCmt;
      ++i;
      continue;
    }
    if (c == '/' && i + 1 < s.size() && s[i + 1] == '*') {
      st = BlockCmt;
      ++i;
      continue;
    }

    // 预处理指令判断：行首的 '#'
    if (c == '#') {
      bool onlySpaceBefore = true;
      if (i > 0) {
        size_t j = i;
        while (j > 0) {
          --j;
          char p = s[j];
          if (p == '\n')
            break;
          if (!std::isspace((unsigned char)p)) {
            onlySpaceBefore = false;
            break;
          }
        }
      }
      if (onlySpaceBefore) {
        if (!cur.empty()) {
          tokens.push_back(cur);
          cur.clear();
        }
        st = InDirective;
        --i;
        continue;
      }
    }
    if (std::isspace((unsigned char)c)) {
      if (!cur.empty()) {
        tokens.push_back(cur);
        cur.clear();
      }
      continue;
    }
    if (i + 1 < s.size()) {
      string t;
      t.push_back(c);
      t.push_back(s[i + 1]);
      if (two.find(t) != two.end()) {
        if (!cur.empty()) {
          tokens.push_back(cur);
          cur.clear();
        }
        tokens.push_back(t);
        ++i;
        continue;
      }
    }
    if (one.find(c) != one.end()) {
      if (!cur.empty()) {
        tokens.push_back(cur);
        cur.clear();
      }
      tokens.emplace_back(1, c);
      continue;
    }
    cur.push_back(c);
  }
  if (!cur.empty())
    tokens.push_back(cur);
  return {tokens, directives};
}

// 构建 k-ary 树（叶子用 gen 包装）
pair<pair<vector<Edge>, string>, vector<string>>
build_k_ary_tree_with_wrapped_leaves(const vector<string> &leaves,
                                     NameGenerator &gen, int K) {
  if (K <= 0)
    throw std::invalid_argument("K must > 0");
  int n = (int)leaves.size();
  vector<Edge> edges;
  vector<string> leaf_names;
  leaf_names.reserve(n);
  for (int i = 0; i < n; ++i)
    leaf_names.push_back(gen.generate());
  vector<string> cur = leaf_names;
  if (cur.empty()) {
    string lone = gen.generate();
    return {{edges, lone}, leaf_names};
  }
  while (cur.size() > 1) {
    vector<string> next;
    next.reserve((cur.size() + K - 1) / K);
    for (size_t i = 0; i < cur.size(); i += K) {
      size_t end = std::min(cur.size(), i + K);
      string parent = gen.generate();
      vector<string> children;
      for (size_t j = i; j < end; ++j)
        children.push_back(cur[j]);
      edges.emplace_back(parent, children);
      next.push_back(parent);
    }
    cur.swap(next);
  }
  string root = cur.front();
  return {{edges, root}, leaf_names};
}

void print_structure_wrapped(const vector<Edge> &edges, const string &root,
                             const vector<string> &leaf_names,
                             const vector<string> &leaves,
                             std::ofstream &outfs) {
  // 叶子行顺序打乱
  std::vector<size_t> idx(leaf_names.size());
  std::iota(idx.begin(), idx.end(), 0);
  std::mt19937_64 rng((uint64_t)time(nullptr));
  std::shuffle(idx.begin(), idx.end(), rng);
  for (size_t i = 0; i < leaf_names.size(); ++i) {
    outfs << "#define " << leaf_names[idx[i]] << ' ' << leaves[idx[i]] << '\n';
  }
  for (auto &e : edges) {
    outfs << "#define " << e.first;
    for (auto &c : e.second)
      outfs << ' ' << c;
    outfs << '\n';
  }
  // 最后一行输出 root 及其 children，并打印 root 行
  const vector<string> *root_children = nullptr;
  for (auto &e : edges)
    if (e.first == root) {
      root_children = &e.second;
      break;
    }
  if (!root_children)
    outfs << "#define " << root << '\n';
  else {
    outfs << "#define " << root;
    for (auto &c : *root_children)
      outfs << ' ' << c;
    outfs << '\n' << root << '\n';
  }
}

void obf(const string &src, std::ofstream &outfs, NameGenerator &gen, int K) {
  auto [tokens, directives] = splitCppTokensWithDirectives(src);
  for (auto &d : directives)
    outfs << d << '\n';
  auto built = build_k_ary_tree_with_wrapped_leaves(tokens, gen, K);
  const auto &edges = built.first.first;
  const auto &root = built.first.second;
  const auto &leaf_names = built.second;
  print_structure_wrapped(edges, root, leaf_names, tokens, outfs);
}

} // namespace Define_Obfuscation

// -------------------- main --------------------
static cl::OptionCategory ToolCategory("cppobf options");
static cl::opt<std::string> PersistMap("persist",
                                       cl::desc("mapping file (load/save)"),
                                       cl::value_desc("file"), cl::init(""),
                                       cl::cat(ToolCategory));
static cl::opt<bool> Inplace("inplace", cl::desc("write changes in-place"),
                             cl::init(false), cl::cat(ToolCategory));
static cl::opt<bool> ProcessMacros("process-macros", cl::desc("process macros"),
                                   cl::init(false), cl::cat(ToolCategory));
static cl::opt<bool>
    AllowExternal("allow-external",
                  cl::desc("allow renaming external linkage symbols"),
                  cl::init(false), cl::cat(ToolCategory));
static cl::opt<unsigned> NameLen("namelen",
                                 cl::desc("generated name length(fixed)"),
                                 cl::init(32), cl::cat(ToolCategory));
static cl::opt<std::string> Prefix("prefix", cl::desc("generated name prefix"),
                                   cl::init(""), cl::cat(ToolCategory));
static cl::opt<unsigned>
    MaxChildrenCount("max-children-count",
                     cl::desc("max children count in a define expend"),
                     cl::init(20), cl::cat(ToolCategory));

int main(int argc, const char **argv) {
  std::string ErrorMessage;
  auto Compilations =
      FixedCompilationDatabase::loadFromCommandLine(argc, argv, ErrorMessage);
  if (!Compilations) {
    llvm::errs() << "Build dir not found, using default . -std=c++14\n";
    Compilations.reset(new FixedCompilationDatabase(".", {"-std=c++14"}));
  }
  auto ExpectedParser = CommonOptionsParser::create(
      argc, argv, ToolCategory, cl::NumOccurrencesFlag::ZeroOrMore,
      "cppobf [options] -- <source-files>");
  if (!ExpectedParser) {
    llvm::errs() << toString(ExpectedParser.takeError()) << '\n';
    return 1;
  }
  CommonOptionsParser &OptionsParser = *ExpectedParser;
  ClangTool Tool(Compilations ? *Compilations : OptionsParser.getCompilations(),
                 OptionsParser.getSourcePathList());

  NameGenerator gen(NameLen, Prefix);
  std::unordered_map<std::string, std::string> usrToObf;
  std::unordered_map<std::string, std::string> macroMap;

  if (!PersistMap.empty()) {
    if (!loadMapping(PersistMap, usrToObf))
      llvm::errs() << "Warning: failed to load mapping file: " << PersistMap
                   << "\n";
  }

  auto Factory = std::make_unique<ObfuscateActionFactory>(
      gen, usrToObf, macroMap, ProcessMacros, AllowExternal);
  int result = Tool.run(Factory.get());
  if (result != 0)
    llvm::errs() << "Tool.run failed: " << result << '\n';

  if (!PersistMap.empty()) {
    if (!saveMapping(PersistMap, usrToObf))
      llvm::errs() << "Warning: failed to save mapping file: " << PersistMap
                   << "\n";
    else
      llvm::outs() << "Saved mapping to " << PersistMap << "\n";
  }

  // 输出并利用 Define_Obfuscation 生成最终 OBF.cpp / OBF.map
  {
    std::ofstream obfOutRaw("OBF_raw.cpp");
    obfOutRaw << ObfuscateAction::finalResult;
    std::ofstream obfOut("OBF.cpp");
    Define_Obfuscation::obf(ObfuscateAction::finalResult, obfOut, gen,
                            MaxChildrenCount);
    std::ofstream mapOut("OBF.map");
    if (!mapOut) {
      llvm::errs() << "Failed to open OBF.map for writing\n";
    } else {
      mapOut << "==== USR -> obf mapping ====\n";
      for (auto &p : usrToObf)
        mapOut << p.first << " -> " << p.second << "\n";
      if (!macroMap.empty()) {
        mapOut << "==== macro -> obf mapping ====\n";
        for (auto &p : macroMap)
          mapOut << p.first << " -> " << p.second << "\n";
      }
      mapOut.close();
      llvm::outs()
          << "Saved define OBFed src to OBF.cpp\nSaved mapping to OBF.map\n";
    }
  }
  return 0;
}
