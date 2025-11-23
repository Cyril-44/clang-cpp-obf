// main.cpp
// cppobf - Clang-based obfuscator (AST + macro renaming), adjusted for
// Clang 10. Build: link clangTooling clangBasic clangASTMatchers clangRewrite
// clangIndex

#include <algorithm>
#include <bits/stdc++.h>

#include "clang/AST/AST.h"
#include "clang/AST/RecursiveASTVisitor.h"
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
// --- 新增：文件顶部放在 NameGenerator 之后或合适位置 ---
#include "clang/AST/ASTContext.h"
#include "clang/AST/ASTContext.h"    // ASTContext
#include "clang/AST/ASTTypeTraits.h" // clang::DynTypedNode (DynTypedNode 等)
#include "clang/AST/Expr.h"
#include "clang/AST/Expr.h" // Expr, IntegerLiteral, StringLiteral...
#include "clang/AST/Stmt.h"
#include "clang/Lex/LiteralSupport.h"
#include "llvm/ADT/APInt.h"
#include "llvm/ADT/SmallVector.h" // llvm::SmallVector

using namespace clang;
using namespace clang::tooling;
using namespace llvm;

class NameGenerator {
public:
  explicit NameGenerator(size_t len = 32, const std::string &prefix = "")
      : length(len), prefix(prefix) {
    auto now = std::chrono::high_resolution_clock::now().time_since_epoch();
    uint64_t seed = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
    rng.seed(seed);

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

    distAll = std::uniform_int_distribution<size_t>(0, alphaNum.size() - 1);
    distFirst = std::uniform_int_distribution<size_t>(0, firstChar.size() - 1);
  }

  std::string generate() {
    std::string s;
    s.reserve(prefix.size() + length);
    s += firstChar[distFirst(rng)];
    for (size_t i = 1; i < length; ++i)
      s += alphaNum[distAll(rng)];
    if (!prefix.empty())
      s = prefix + s;

    size_t tries = 0;
    while (!used.insert(s).second) {
      if (++tries > 16) {
        s += '_' + std::to_string(tries);
        if (used.insert(s).second)
          break;
      }
      std::string body;
      body += firstChar[distFirst(rng)];
      for (size_t i = 1; i < length; ++i)
        body += alphaNum[distAll(rng)];
      s = prefix.empty() ? body : (prefix + body);
    }
    return s;
  }

  void reset() { used.clear(); }

private:
  size_t length;
  std::string prefix;
  std::mt19937_64 rng;
  std::vector<char> alphaNum;
  std::vector<char> firstChar;
  std::unordered_set<std::string> used;
  std::uniform_int_distribution<size_t> distAll;
  std::uniform_int_distribution<size_t> distFirst;
};

std::string GLOBAL_GENERATED_CPPOBF_DECODE_FUNC_NAME;

struct ObfDataManager {
  Rewriter &R;
  NameGenerator &gen;
  const SourceManager &SM;
  std::unordered_map<std::string, std::string> strMap; // original -> varname
  std::unordered_map<long long, std::string>
      intMap; // encoded value -> varname (optional)
  std::unordered_map<int, std::string> charMap;
  bool injected = false;
  std::string headerInsert; // cached definitions to inject

  ObfDataManager(Rewriter &R, NameGenerator &g, const SourceManager &SM)
      : R(R), gen(g), SM(SM) {}

  // simple single-byte xor encoder
  static uint8_t chooseKey() {
    static std::mt19937_64 rng(
        (uint64_t)std::chrono::high_resolution_clock::now()
            .time_since_epoch()
            .count());
    return static_cast<uint8_t>(rng() & 0xFF);
  }

  std::string makeStrVar(const std::string &s) {
    auto it = strMap.find(s);
    if (it != strMap.end())
      return it->second;
    std::string var = gen.generate();
    uint8_t key = chooseKey();
    // encode bytes by xor key
    std::string encoded;
    encoded.reserve(s.size() + 1);
    for (unsigned char c : s)
      encoded.push_back(static_cast<char>(c ^ key));
    // terminate
    encoded.push_back(static_cast<char>(0 ^ key));
    // build C literal for array of unsigned char
    std::string arr = "static constexpr unsigned char " + var + "[] = {";
    for (size_t i = 0; i < encoded.size(); ++i) {
      unsigned char b = static_cast<unsigned char>(encoded[i]);
      arr += "0x";
      char buf[3];
      snprintf(buf, sizeof(buf), "%02x", b);
      arr += buf;
      if (i + 1 < encoded.size())
        arr += ",";
    }
    arr += "};\n";
    // also generate a tiny inline decoder function for this TU (only once)
    if (!injected) {
      GLOBAL_GENERATED_CPPOBF_DECODE_FUNC_NAME = gen.generate();
      // ensure <string> is included somewhere before this injected code
      headerInsert += "static inline const char* " +
                      GLOBAL_GENERATED_CPPOBF_DECODE_FUNC_NAME +
                      "(const unsigned char* p, unsigned char b) {"
                      "  static char a[1<<10];"
                      "  for (unsigned long i = 0; ; ++i) {"
                      "    unsigned char c = p[i];"
                      "    unsigned char d = static_cast<unsigned char>(c ^ b);"
                      "    a[i] = static_cast<char>(d);"
                      "    if (c == 0x00) {a[i+1] = '\\0'; break;}"
                      "  }"
                      "  return a;"
                      "}";

      injected = true;
    }
    // append declaration that decodes at runtime by calling decode on a copy
    // (or in-place) Note: decodes in-place on first use; thread-safety not
    // ensured
    headerInsert += arr;
    // store mapping with key appended so decode call can use key; embed key
    // into name to remember we store name as var + ":" + hex(key) so consumer
    // can extract key
    char ks[4];
    snprintf(ks, sizeof(ks), "%02x", key);
    std::string stored = var + ":" + std::string(ks);
    strMap[s] = stored;
    return stored;
  }

  // integer obfuscation: store masked const and replace literal with ((TYPE)
  // (var ^ KEY))
  std::string makeIntVar(long long v, unsigned bits = 64) {
    auto it = intMap.find(v);
    if (it != intMap.end())
      return it->second;
    std::string var = gen.generate();
    uint64_t key = ((uint64_t)chooseKey() << 56) ^
                   ((uint64_t)chooseKey() << 48) ^
                   ((uint64_t)chooseKey() << 40);
    uint64_t encoded = (uint64_t)v ^ key;
    // create unsigned long long init
    char buf[128];
    snprintf(buf, sizeof(buf),
             "static constexpr unsigned long long %s = 0x%llxULL;", var.c_str(),
             (unsigned long long)encoded);
    headerInsert += buf;
    // store var:key in mapping as "var:hexkey"
    char kbuf[128];
    snprintf(kbuf, sizeof(kbuf), "%llx", (unsigned long long)key);
    std::string stored = var + ":" + std::string(kbuf);
    intMap[v] = stored;
    return stored;
  }

  std::string makeCharVar(char c) {
    int ci = static_cast<unsigned char>(c);
    auto it = charMap.find(ci);
    if (it != charMap.end())
      return it->second;
    std::string var = gen.generate();
    uint8_t key = chooseKey();
    unsigned char enc = static_cast<unsigned char>(c ^ key);
    char buf[128];
    snprintf(buf, sizeof(buf), "static constexpr unsigned char %s = 0x%02x;",
             var.c_str(), (unsigned)enc);
    headerInsert += buf;
    char kbuf[8];
    snprintf(kbuf, sizeof(kbuf), "%02x", key);
    std::string stored = var + ":" + std::string(kbuf);
    charMap[ci] = stored;
    return stored;
  }

  void injectHeaderIfNeeded(const FileID &FID) {
    if (headerInsert.empty())
      return;
    // inject at beginning of main file once per TU
    SourceLocation start = SM.getLocForStartOfFile(FID);
    R.InsertText(start, headerInsert, true, true);
    headerInsert.clear();
  }
};

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

class FullRenamer : public RecursiveASTVisitor<FullRenamer> {
public:
  FullRenamer(Rewriter &R, NameGenerator &G,
              std::unordered_map<std::string, std::string> &usrMap,
              const SourceManager &SM, bool allowExternal)
      : R(R), gen(G), usrToObf(usrMap), SM(SM), allowExternal(allowExternal) {}

  bool VisitFunctionDecl(FunctionDecl *FD) {
    if (!FD)
      return true;

    // skip implicit / invalid
    if (FD->isImplicit() || FD->isInvalidDecl())
      return true;

    // must have an identifier
    if (!FD->getIdentifier())
      return true;

    // Only consider the definition (or getDefinition()).
    FunctionDecl *Def = nullptr;
    if (FD->isThisDeclarationADefinition())
      Def = FD;
    else
      Def = FD->getDefinition();
    // if there's no definition in this TU, skip (e.g. printf)
    if (!Def)
      return true;

    // Use Rewriter's SourceManager to inspect the spelling location of the
    // definition
    SourceLocation defLoc = Def->getLocation();
    SourceLocation spellLoc = SM.getSpellingLoc(defLoc);
    if (!spellLoc.isValid())
      return true;

    // If definition not in main file and we don't allow external, skip
    if (!SM.isWrittenInMainFile(spellLoc) && !allowExternal)
      return true;

    // If external symbols are not allowed and this has external linkage, skip
    if (!allowExternal && Def->hasExternalFormalLinkage())
      return true;

    // If allowExternal is true we still want to protect main()
    if (allowExternal) {
      if (Def->isMain()) {
        llvm::errs() << "Skipping obfuscation of main()\n";
        return true;
      }
      if (Def->getName() == "main") {
        llvm::errs() << "Skipping obfuscation of function named 'main'\n";
        return true;
      }
    }

    // skip template instantiations / compiler-generated / builtin ids
    if (Def->isTemplateInstantiation())
      return true;
    //   if (Def->isBuiltinID()) return true;

    // Now apply rename using the definition's spelling location and the precise
    // name token range
    applyRenameIfNeeded(Def, spellLoc);
    return true;
  }

  bool VisitCXXRecordDecl(CXXRecordDecl *RD) {
    if (!RD->getIdentifier())
      return true;
    if (!locInMainFile(RD->getLocation()))
      return true;
    if (!allowExternal && RD->hasExternalFormalLinkage())
      return true;
    applyRenameIfNeeded(RD, RD->getLocation());
    return true;
  }

  bool VisitVarDecl(VarDecl *VD) {
    if (!VD->getIdentifier())
      return true;
    if (!locInMainFile(VD->getLocation()))
      return true;
    if (!allowExternal && VD->hasExternalStorage())
      return true;
    applyRenameIfNeeded(VD, VD->getLocation());
    return true;
  }

  bool VisitFieldDecl(FieldDecl *FD) {
    if (!FD->getIdentifier())
      return true;
    if (!locInMainFile(FD->getLocation()))
      return true;
    applyRenameIfNeeded(FD, FD->getLocation());
    return true;
  }

  bool VisitEnumDecl(EnumDecl *ED) {
    if (!ED->getIdentifier())
      return true;
    if (!locInMainFile(ED->getLocation()))
      return true;
    if (!allowExternal && ED->hasExternalFormalLinkage())
      return true;
    applyRenameIfNeeded(ED, ED->getLocation());
    return true;
  }

  bool VisitTypedefNameDecl(TypedefNameDecl *TD) {
    if (!TD->getIdentifier())
      return true;
    if (!locInMainFile(TD->getLocation()))
      return true;
    applyRenameIfNeeded(TD, TD->getLocation());
    return true;
  }

  bool VisitDeclRefExpr(DeclRefExpr *DRE) {
    ValueDecl *VD = DRE->getDecl();
    if (!VD || !VD->getIdentifier())
      return true;
    if (!locInMainFile(DRE->getLocation()))
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
    if (!locInMainFile(ME->getMemberLoc()))
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

  const std::unordered_map<std::string, std::string> &getMapping() const {
    return usrToObf;
  }

  // 新增：字符串字面量
  bool VisitStringLiteral(clang::StringLiteral *SLit) {
    if (!SLit)
      return true;
    SourceLocation L = SLit->getBeginLoc();
    if (!L.isValid())
      return true;
    if (!locInMainFile(L))
      return true;
    // 安全性检查：避免在 unevaluated context / template non-type param / array
    // size etc.
    if (SLit->isWide())
      return true; // skip wide/unicode for simplicity
    std::string s = SLit->getString().str();
    // get variable (format "var:kk")
    std::string stored = obfMgr->makeStrVar(s);
    // parse stored into var and key
    auto pos = stored.find(':');
    std::string var = stored.substr(0, pos);
    std::string keyhex = stored.substr(pos + 1);
    // we replaced by call to decode function: __cppobf_decode_ptr(var, 0xKK)
    char repl[256];
    snprintf(repl, sizeof(repl),
             "(reinterpret_cast<const char*>(%s(%s, 0x%s)))",
             GLOBAL_GENERATED_CPPOBF_DECODE_FUNC_NAME.c_str(), var.c_str(),
             keyhex.c_str());
    R.ReplaceText(CharSourceRange::getTokenRange(L), repl);
    return true;
  }

  bool VisitCharacterLiteral(CharacterLiteral *CL) {
    if (!CL)
      return true;
    SourceLocation L = CL->getLocation();
    if (!L.isValid())
      return true;
    if (!locInMainFile(L))
      return true;
    // get char value (note: may be multi-byte in some contexts; handle basic)
    unsigned val = CL->getValue();
    char ch = static_cast<char>(val & 0xFF);
    std::string stored = obfMgr->makeCharVar(ch);
    auto pos = stored.find(':');
    std::string var = stored.substr(0, pos);
    std::string keyhex = stored.substr(pos + 1);
    char repl[128];
    // decode inline: (char)(( (unsigned char)var ^ 0xKK) ) but var holds
    // encoded byte we generated unsigned char var = 0xXX (encoded), and we have
    // key so write: (char)((unsigned char)(var) ^ 0xKK)
    snprintf(repl, sizeof(repl), "((char)((unsigned char)(%s) ^ 0x%s))",
             var.c_str(), keyhex.c_str());
    R.ReplaceText(CharSourceRange::getTokenRange(L), repl);
    return true;
  }
  // 辅助：判断表达式是否在“危险的编译期常量上下文”
  static bool isInConstantContext(const clang::Expr *E,
                                  clang::ASTContext &Ctx) {
    auto parents =
        Ctx.getParents(clang::ast_type_traits::DynTypedNode::create(*E));
    if (parents.empty())
      return false;
    for (const auto &P : parents) {
      if (P.get<clang::TemplateArgument>())
        return true;
      if (const clang::Expr *PE = P.get<clang::Expr>()) {
        if (llvm::isa<clang::IntegerLiteral>(PE) ||
            llvm::isa<clang::FloatingLiteral>(PE) ||
            llvm::isa<clang::CXXBoolLiteralExpr>(PE))
          return true;
        if (isInConstantContext(PE, Ctx))
          return true;
      }
      if (const clang::Decl *PD = P.get<clang::Decl>()) {
        if (llvm::isa<clang::EnumConstantDecl>(PD))
          return true;
        if (llvm::isa<clang::NonTypeTemplateParmDecl>(PD))
          return true;
      }
      if (const clang::Stmt *PS = P.get<clang::Stmt>()) {
        if (llvm::isa<clang::CaseStmt>(PS) || llvm::isa<clang::SwitchStmt>(PS))
          return true;
      }
    }
    return false;
  }

  bool VisitIntegerLiteral(clang::IntegerLiteral *IL) {
    if (!IL)
      return true;

    clang::SourceLocation L = IL->getLocation();
    if (!L.isValid() || !locInMainFile(L))
      return true;

    if (!Ctx || !obfMgr)
      return true; // 防御性检查

    // 父节点检查：跳过编译期常量上下文
    auto parents =
        Ctx->getParents(clang::ast_type_traits::DynTypedNode::create(*IL));
    for (const auto &P : parents) {
      if (P.get<clang::EnumConstantDecl>() ||
          P.get<clang::NonTypeTemplateParmDecl>() || P.get<clang::CaseStmt>()) {
        return true; // 保守跳过
      }
    }

    llvm::APInt api = IL->getValue();
    if (api.getBitWidth() > 63)
      return true; // 太大，跳过

    long long v = api.getSExtValue();

    // 转成十六进制字符串，保证 0x 后面有内容
    std::string hexVal = api.toString(16, false);

    // 生成混淆变量
    std::string stored = obfMgr->makeIntVar(v);
    auto pos = stored.find(':');
    std::string var = stored.substr(0, pos);
    std::string keyhex = stored.substr(pos + 1);

    // 用原始类型字符串替换更安全
    std::string typeStr = IL->getType().getAsString();

    char repl[256];
    snprintf(repl, sizeof(repl), "((%s)(((unsigned long long)%s) ^ 0x%sULL))",
             typeStr.c_str(), var.c_str(), keyhex.c_str());

    R.ReplaceText(clang::CharSourceRange::getTokenRange(L), repl);

    // 在头部插入完整的静态变量定义（如果需要）
    // obfMgr->injectStaticVar(var, hexVal);

    return true;
  }

private:
  bool locInMainFile(SourceLocation L) const {
    if (!L.isValid())
      return false;
    return SM.isWrittenInMainFile(SM.getSpellingLoc(L));
  }

  std::string getDeclUSR(const Decl *D) const {
    llvm::SmallString<128> buf;
    if (index::generateUSRForDecl(D, buf))
      return std::string();
    return std::string(buf.begin(), buf.end());
  }

  void applyRenameIfNeeded(const Decl *D, SourceLocation declLoc) {
    if (!declLoc.isValid())
      return;
    std::string usr = getDeclUSR(D);
    if (usr.empty())
      return;
    if (usrToObf.count(usr))
      return;
    std::string obf = gen.generate();
    usrToObf[usr] = obf;
    SourceLocation SL = SM.getSpellingLoc(declLoc);
    if (!SL.isValid())
      return;
    R.ReplaceText(CharSourceRange::getTokenRange(SL), obf);
  }

  Rewriter &R;
  NameGenerator &gen;
  std::unordered_map<std::string, std::string> &usrToObf;
  const SourceManager &SM;
  bool allowExternal;

public:
  ObfDataManager *obfMgr = nullptr; // 指向外部创建的 manager
  clang::ASTContext *Ctx = nullptr;
};

class RenamerConsumer : public ASTConsumer {
public:
  RenamerConsumer(Rewriter &R, NameGenerator &G,
                  std::unordered_map<std::string, std::string> &usrMap,
                  const SourceManager &SM, bool allowExternal)
      : obfManager(R, G, SM), renamer(R, G, usrMap, SM, allowExternal) {
    renamer.obfMgr = &obfManager; // 关键：把指针传给 renamer
  }

  void HandleTranslationUnit(ASTContext &Context) override {
    renamer.Ctx = &Context; // 同时把 ASTContext 传进去
    renamer.TraverseDecl(Context.getTranslationUnitDecl());
    FileID fid = Context.getSourceManager().getMainFileID();
    obfManager.injectHeaderIfNeeded(fid);
  }

  FullRenamer renamer;
  ObfDataManager obfManager;
};

std::string FINAL_OBF_RESULT;
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
    if (processMacros) {
      //   CI.getPreprocessor().addPPCallbacks(std::make_unique<MacroRenamer>(
      //       rewriter_, gen, macroMap, CI.getSourceManager(),
      //       CI.getLangOpts(), processMacros));
    }
    return std::make_unique<RenamerConsumer>(
        rewriter_, gen, usrMap, CI.getSourceManager(), allowExternal);
  }

  void EndSourceFileAction() override {
    FileID ID = rewriter_.getSourceMgr().getMainFileID();
    std::string out;
    llvm::raw_string_ostream os(out);
    rewriter_.getEditBuffer(ID).write(os);

    FINAL_OBF_RESULT = os.str();
    llvm::outs() << "Wrote obfuscated source to OBF_raw.cpp\n";
  }

private:
  Rewriter rewriter_;
  NameGenerator &gen;
  std::unordered_map<std::string, std::string> &usrMap;
  std::unordered_map<std::string, std::string> &macroMap;
  bool processMacros;
  bool allowExternal;
};

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

namespace Define_Obfuscation {
/*
Modular obfuscator with single entry:
string obf(const string &input)

Behavior:
- Collects and moves all leading preprocessor lines (line-start '#') to the top.
- Tokenizes the body using a state machine: preserves strings, chars, comments,
NL.
- Removes ordinary spaces but inserts explicit space tokens where necessary
(ident/num adjacency).
- Splits tokens into segments and generates nested #define macros per segment.
- Returns the complete obfuscated source text as a single string.

Notes:
- Configuration constants (SEG_SIZE, CHUNK_SIZE, PREFIX) are local and can be
adjusted.
- This function returns only the obfuscated .c/.cpp content; if you want JSON
mapping or per-segment files, the code can be extended.
*/
// obfuscate_mod_define_top.cpp
// Requires C++17
using std::pair;
using std::string;
using std::unordered_set;
using std::vector;

pair<vector<string>, vector<string>>
splitCppTokensWithDirectives(const string &s) {
  static const unordered_set<string> two = {
      "==", "!=", "<=", ">=", "&&", "||", "++", "--", "+=", "-=", "*=",
      "/=", "%=", "&=", "|=", "^=", "<<", ">>", "->", "::", ".*", "->*"};
  static const unordered_set<char> one = {
      '+', '-', '*', '/', '%', '&', '|', '^', '~', '!', '=', '<', '>',
      '(', ')', '[', ']', '{', '}', ',', ';', '.', ';', ':', '?'};

  vector<string> tokens;
  vector<string> directives;
  string cur;
  auto flush = [&]() {
    if (!cur.empty()) {
      tokens.push_back(cur);
      cur.clear();
    }
  };

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

    // 状态：字符串字面量
    if (st == InStr) {
      cur.push_back(c);
      if (c == '"' && s[i - 1] != '\\') {
        st = Normal;
        flush();
      }
      continue;
    }
    // 状态：字符字面量
    if (st == InChar) {
      cur.push_back(c);
      if (c == '\'' && s[i - 1] != '\\') {
        st = Normal;
        flush();
      }
      continue;
    }
    // 状态：行注释
    if (st == LineCmt) {
      if (c == '\n')
        st = Normal;
      continue;
    }
    // 状态：块注释
    if (st == BlockCmt) {
      if (c == '*' && i + 1 < s.size() && s[i + 1] == '/') {
        st = Normal;
        ++i;
      }
      continue;
    }
    // 状态：预处理指令，收集整行（含换行之前所有字符）
    if (st == InDirective) {
      // 收集直到未被反斜线续行或者行尾
      size_t start = i;
      bool cont = false;
      for (; i < s.size(); ++i) {
        if (s[i] == '\\') {
          // 如果是行续行，跳过下一字符并继续
          if (i + 1 < s.size() && s[i + 1] == '\n') {
            cont = true;
            ++i;
            continue;
          }
          if (i + 1 == s.size())
            break;
        }
        if (s[i] == '\n')
          break;
      }
      size_t end = (i < s.size() ? i : s.size() - 1);
      // 包含起始 '#' 前的任何前导空白
      string dir = s.substr(start, end - start + 1);
      // 去除末尾的换行（保留行内内容）
      if (!dir.empty() && dir.back() == '\n')
        dir.pop_back();
      directives.push_back(dir);
      st = Normal;
      continue;
    }

    // Normal 状态处理
    // 识别进入字符串/字符/注释
    if (c == '"') {
      flush();
      cur.push_back(c);
      st = InStr;
      continue;
    }
    if (c == '\'') {
      flush();
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

    // 识别预处理指令：行首（允许前导空白）遇到 '#'
    if (c == '#') {
      // 确保这是行首（前面只有空白或在字符串开始）
      bool onlySpaceBefore = true;
      if (i > 0) {
        size_t j = i;
        while (j > 0) {
          --j;
          char p = s[j];
          if (p == '\n')
            break;
          if (!std::isspace(static_cast<unsigned char>(p))) {
            onlySpaceBefore = false;
            break;
          }
        }
      }
      if (onlySpaceBefore) {
        flush();
        // 把当前 '#' 的位置传给 InDirective 处理（它期望 i 为起点）
        st = InDirective;
        // 将循环索引回退一个位置，因为 InDirective 分支从 i 开始读取
        // 实际上当前循环会执行 ++i, 所以需要 --i 保持位置不变，下一轮进入
        // InDirective
        --i;
        continue;
      }
    }

    if (std::isspace(static_cast<unsigned char>(c))) {
      flush();
      continue;
    }

    // 尝试两字符运算符（最长优先）
    if (i + 1 < s.size()) {
      string t;
      t.reserve(2);
      t.push_back(c);
      t.push_back(s[i + 1]);
      if (two.find(t) != two.end()) {
        flush();
        tokens.push_back(t);
        ++i;
        continue;
      }
    }
    // 单字符运算符
    if (one.find(c) != one.end()) {
      flush();
      tokens.emplace_back(1, c);
      continue;
    }

    // 一般标识符/数字/其他字符
    cur.push_back(c);
  }
  flush();
  return {tokens, directives};
}

// 返回：edges（parent->children 列表，按创建顺序），以及 root 名称，
// 同时也返回叶子名列表以便打印 leaf_val 行（叶子名与 leaves 索引对应）
pair<pair<vector<pair<string, vector<string>>>, string>, vector<string>>
build_k_ary_tree_with_wrapped_leaves(const vector<string> &leaves,
                                     NameGenerator &gen, int K) {
  if (K <= 0)
    throw std::invalid_argument("K must be >= 1");

  int n = static_cast<int>(leaves.size());
  vector<pair<string, vector<string>>> edges;
  vector<string> leaf_names;
  leaf_names.reserve(n);

  // 1) 为每个叶子生成一个 gen 名称（包装层）
  for (int i = 0; i < n; ++i) {
    leaf_names.push_back(gen.generate());
  }

  // 2) 当前层名列表初始化为叶子名（按输入顺序）
  vector<string> cur = leaf_names;

  // 如果没有叶子，生成单个 root 名称并返回
  if (cur.empty()) {
    string lone = gen.generate();
    return {{edges, lone}, leaf_names};
  }

  // 3) 非递归向上分组构造，每组最多 K 个孩子
  while (cur.size() > 1) {
    vector<string> next;
    next.reserve((cur.size() + K - 1) / K);
    for (size_t i = 0; i < cur.size(); i += K) {
      size_t end = std::min(cur.size(), i + K);
      string parent = gen.generate();
      vector<string> children;
      children.reserve(end - i);
      for (size_t j = i; j < end; ++j)
        children.push_back(cur[j]);
      edges.emplace_back(parent, std::move(children));
      next.push_back(parent);
    }
    cur.swap(next);
  }

  // cur[0] 即为根名
  string root = cur.front();
  return {{edges, root}, leaf_names};
}

// ----------------- print_structure_wrapped (重写，新增 ofstream& outfs)
// -----------------
void print_structure_wrapped(const vector<pair<string, vector<string>>> &edges,
                             const string &root,
                             const vector<string> &leaf_names,
                             const vector<string> &leaves,
                             std::ofstream &outfs) {
  // 叶子行：GEN_LEAF_NAME leaf_val
  vector<size_t> arr(leaf_names.size());
  std::iota(arr.begin(), arr.end(), 0);
  std::mt19937_64 rng(time(nullptr));
  std::shuffle(arr.begin(), arr.end(), rng);
  for (size_t i = 0; i < leaf_names.size(); ++i) {
    outfs << "#define " << leaf_names[arr[i]] << ' ' << leaves[arr[i]] << '\n';
  }
  // parent -> children 行（按创建顺序）
  for (const auto &e : edges) {
    outfs << "#define " << e.first;
    for (const auto &c : e.second)
      outfs << ' ' << c;
    outfs << '\n';
  }
  // 最后一行：root followed by its immediate children if available
  const vector<string> *root_children = nullptr;
  for (const auto &e : edges) {
    if (e.first == root) {
      root_children = &e.second;
      break;
    }
  }
  if (!root_children) {
    outfs << "#define " << root << '\n';
  } else {
    outfs << "#define " << root;
    for (const auto &c : *root_children)
      outfs << ' ' << c;
    outfs << '\n';
    outfs << root << '\n';
  }
}

// ----------------- process 总函数 -----------------
void obf(const string &src, std::ofstream &outfs, NameGenerator &gen, int K) {
  // 1) 拆分（这里把拆分结果作为叶子值的简化示例）
  auto [tokens, def] = splitCppTokensWithDirectives(src);
  for (const auto &s : def)
    outfs << s << '\n';
  // 如果你希望把整行/语句作为叶子，请先把 src 分割成行或逻辑单元；此处示例用
  // tokens 作为叶子值 2) 构造树（叶子先用 gen 包装）
  auto built = build_k_ary_tree_with_wrapped_leaves(tokens, gen, K);
  const auto &edges = built.first.first;
  const auto &root = built.first.second;
  const auto &leaf_names = built.second;
  // 3) 打印到 outfs
  print_structure_wrapped(edges, root, leaf_names, tokens, outfs);
}

} // namespace Define_Obfuscation

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
    llvm::errs() << toString(ExpectedParser.takeError()) << "\n";
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
    llvm::errs() << "Tool.run failed: " << result << "\n";

  if (!PersistMap.empty()) {
    if (!saveMapping(PersistMap, usrToObf))
      llvm::errs() << "Warning: failed to save mapping file: " << PersistMap
                   << "\n";
    else
      llvm::outs() << "Saved mapping to " << PersistMap << "\n";
  }

  {
    std::ofstream obfOutRaw("OBF_raw.cpp");
    obfOutRaw << FINAL_OBF_RESULT;
    std::ofstream obfOut("OBF.cpp");
    Define_Obfuscation::obf(FINAL_OBF_RESULT, obfOut, gen, MaxChildrenCount);
    std::ofstream mapOut("OBF.map");
    if (!mapOut) {
      llvm::errs() << "Failed to open OBF.map for writing\n";
    } else {
      mapOut << "==== USR -> obf mapping ====\n";
      for (auto &p : usrToObf) {
        mapOut << p.first << " -> " << p.second << "\n";
      }
      if (!macroMap.empty()) {
        mapOut << "==== macro -> obf mapping ====\n";
        for (auto &p : macroMap) {
          mapOut << p.first << " -> " << p.second << "\n";
        }
      }
      mapOut.close();
      llvm::outs() << "Saved define OBFed src to OBF.cpp\nSaved mapping to OBF.map\n";
    }
  }
  return 0;
}
