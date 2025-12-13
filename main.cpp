// clang14_obfuscator.cpp
// C++20
// A source-code obfuscator using Clang 14 LibTooling. 
// Usage (example):
//   ./obfuscator input.cpp [protect.map] -- -std=c++20
// The tool will rewrite identifiers (non-keywords) in user code to obfuscated names
// while trying to avoid system headers, identifiers in the `std` namespace, macro expansions,
// and names listed in protect.map (one identifier per line).

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <random>
#include <memory>
#include <algorithm>
#include <stdexcept>
#include <cctype>
#include <utility>
#include <ctime>

#include "clang/AST/AST.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Rewrite/Core/Rewriter.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Lex/Lexer.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FormatVariadic.h"

using namespace clang;
using namespace clang::tooling;
using namespace llvm;

static cl::OptionCategory ObfCategory("obfuscator options");
static cl::opt<std::string> ProtectMapPath(cl::Positional,
    cl::desc("protect.map (optional)"), cl::init(""), cl::Optional,
    cl::cat(ObfCategory));
static llvm::cl::opt<unsigned> NameLenMin(
    "name-len-min",
    llvm::cl::desc("minimum length for obfuscated names"),
    llvm::cl::init(6),
    llvm::cl::cat(ObfCategory));
static llvm::cl::opt<unsigned> NameLenMax(
    "name-len-max",
    llvm::cl::desc("maximum length for obfuscated names"),
    llvm::cl::init(12),
    llvm::cl::cat(ObfCategory));
static llvm::cl::opt<unsigned> maxK(
    "child-cnt-max",
    llvm::cl::desc("maximum count for tree-style define childrens"),
    llvm::cl::init(16),
    llvm::cl::cat(ObfCategory));

// 静态集合与互斥量用于去重与线程安全
static std::unordered_set<std::string> s_generatedNames;
static std::mutex s_generatedNamesMutex;
static std::atomic<uint64_t> s_globalUniqCounter{0};
// -------------------- makeObfName --------------------
static std::string makeObfName() {
    static constexpr char firstChars[] =
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "_";

    static constexpr char otherChars[] =
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "0123456789"
        "_";

    unsigned minLen = std::max(1u, NameLenMin.getValue());
    unsigned maxLen = std::max(minLen, NameLenMax.getValue());

    // ===== 1. 确定性主路径 =====
    static thread_local std::mt19937_64 rng{std::random_device{}()};

    std::uniform_int_distribution<unsigned> lenDist(minLen, maxLen);
    std::uniform_int_distribution<size_t> firstDist(0, sizeof(firstChars) - 2);
    std::uniform_int_distribution<size_t> otherDist(0, sizeof(otherChars) - 2);

    unsigned len = lenDist(rng);
    std::string name;
    name.reserve(len);

    name += firstChars[firstDist(rng)];
    for (unsigned i = 1; i < len; ++i)
        name += otherChars[otherDist(rng)];

    {
        std::lock_guard<std::mutex> lock(s_generatedNamesMutex);
        if (s_generatedNames.insert(name).second)
            return name;
    }

    // ===== 2. 冲突退化路径 =====
    // 真随机 + 固定 maxLen + 全局计数
    static thread_local std::mt19937_64 trng{std::random_device{}()};
    uint64_t uniq = s_globalUniqCounter.fetch_add(1, std::memory_order_relaxed);

    std::string fallback;
    fallback.reserve(maxLen + 24);

    fallback += firstChars[firstDist(trng)];
    for (unsigned i = 1; i < maxLen; ++i)
        fallback += otherChars[otherDist(trng)];

    fallback += "_";
    fallback += std::to_string(uniq);

    {
        std::lock_guard<std::mutex> lock(s_generatedNamesMutex);
        s_generatedNames.insert(fallback);
    }
    return fallback;
}

namespace Define_Obfuscation {
using std::string;
using std::vector;
using std::pair;
using Edge = std::pair<string, vector<string>>;

pair<vector<string>, vector<string>> splitCppTokensWithDirectives(const string &s) {
  static const std::unordered_set<string> two = {
      "==","!=","<=","=>","&&","||","++","--","+=","-=","*=","/=","%=",
      "&=","|=","^=","<<",">>","->","::",".*","->*"
  };
  static const std::unordered_set<char> one = {
      '+','-','*','/','%','&','|','^','~','!','=','<','>',
      '(',')','[',']','{','}',',',';','.',':','?'
  };

  vector<string> tokens;
  vector<string> directives;
  tokens.reserve(1024);

  enum State { Normal, InStr, InChar, LineCmt, BlockCmt, InDirective } st = Normal;
  string cur;
  auto push_cur = [&]() { if (!cur.empty()) { tokens.push_back(std::move(cur)); cur.clear(); } };

  for (size_t i = 0; i < s.size(); ++i) {
    char c = s[i];

    if (st == InStr) {
      cur.push_back(c);
      if (c == '"' && i > 0 && s[i - 1] != '\\') { push_cur(); st = Normal; }
      continue;
    }
    if (st == InChar) {
      cur.push_back(c);
      if (c == '\'' && i > 0 && s[i - 1] != '\\') { push_cur(); st = Normal; }
      continue;
    }
    if (st == LineCmt) {
      if (c == '\n') st = Normal;
      continue;
    }
    if (st == BlockCmt) {
      if (c == '*' && i + 1 < s.size() && s[i + 1] == '/') { ++i; st = Normal; }
      continue;
    }
    if (st == InDirective) {
      size_t start = i;
      for (; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size() && s[i + 1] == '\n') { ++i; continue; }
        if (s[i] == '\n') break;
      }
      size_t end = (i < s.size() ? i : s.size() - 1);
      string dir = s.substr(start, end - start + 1);
      if (!dir.empty() && dir.back() == '\n') dir.pop_back();
      directives.push_back(std::move(dir));
      st = Normal;
      continue;
    }

    // Normal
    if (c == '"') { cur.push_back(c); st = InStr; continue; }
    if (c == '\'') { cur.push_back(c); st = InChar; continue; }
    if (c == '/' && i + 1 < s.size() && s[i + 1] == '/') { st = LineCmt; ++i; continue; }
    if (c == '/' && i + 1 < s.size() && s[i + 1] == '*') { st = BlockCmt; ++i; continue; }

    if (c == '#') {
      bool onlySpaceBefore = true;
      if (i > 0) {
        size_t j = i;
        while (j > 0) {
          --j;
          if (s[j] == '\n') break;
          if (!std::isspace(static_cast<unsigned char>(s[j]))) { onlySpaceBefore = false; break; }
        }
      }
      if (onlySpaceBefore) { push_cur(); st = InDirective; --i; continue; }
    }

    if (std::isspace(static_cast<unsigned char>(c))) { push_cur(); continue; }

    if (i + 1 < s.size()) {
      string t; t.push_back(c); t.push_back(s[i + 1]);
      if (two.find(t) != two.end()) { push_cur(); tokens.push_back(std::move(t)); ++i; continue; }
    }

    if (one.find(c) != one.end()) { push_cur(); tokens.emplace_back(1, c); continue; }

    cur.push_back(c);
  }

  if (!cur.empty()) tokens.push_back(std::move(cur));
  return {tokens, directives};
}

// 构建 K 叉树（使用全局 makeObfName() 生成名字）
pair<pair<vector<Edge>, string>, vector<string>>
build_k_ary_tree_with_wrapped_leaves(const vector<string> &leaves, int K) {
  if (K <= 0) throw std::invalid_argument("K must > 0");
  int n = static_cast<int>(leaves.size());
  vector<Edge> edges;
  vector<string> leaf_names;
  leaf_names.reserve(n);
  for (int i = 0; i < n; ++i) leaf_names.push_back(makeObfName());

  if (leaf_names.empty()) {
    string lone = makeObfName();
    return {{edges, lone}, leaf_names};
  }

  vector<string> cur = leaf_names;
  while (cur.size() > 1) {
    vector<string> next;
    next.reserve((cur.size() + K - 1) / K);
    for (size_t i = 0; i < cur.size(); i += K) {
      size_t end = std::min(cur.size(), i + K);
      string parent = makeObfName();
      vector<string> children(cur.begin() + i, cur.begin() + end);
      edges.emplace_back(parent, children);
      next.push_back(parent);
    }
    cur.swap(next);
  }
  return {{edges, cur.front()}, leaf_names};
}

void print_structure_wrapped(const vector<Edge> &edges, const string &root,
                             const vector<string> &leaf_names, const vector<string> &leaves,
                             auto &outfs) {
  std::vector<size_t> idx(leaf_names.size());
  std::iota(idx.begin(), idx.end(), 0);
  std::mt19937_64 rng(static_cast<unsigned long>(std::time(nullptr)));
  std::shuffle(idx.begin(), idx.end(), rng);

  for (size_t i = 0; i < leaf_names.size(); ++i) {
    outfs << "#define " << leaf_names[idx[i]] << ' ' << leaves[idx[i]] << '\n';
  }

  for (const auto &e : edges) {
    outfs << "#define " << e.first;
    for (const auto &c : e.second) outfs << ' ' << c;
    outfs << '\n';
  }

  const vector<string> *root_children = nullptr;
  for (const auto &e : edges) if (e.first == root) { root_children = &e.second; break; }

  if (!root_children) outfs << "#define " << root << '\n';
  else {
    outfs << "#define " << root;
    for (const auto &c : *root_children) outfs << ' ' << c;
    outfs << '\n' << root << '\n';
  }
}

void obf(const string &src, auto &outfs) {
  auto pr = splitCppTokensWithDirectives(src);
  for (const auto &d : pr.second) outfs << d << '\n';
  auto built = build_k_ary_tree_with_wrapped_leaves(pr.first, maxK);
  const auto &edges = built.first.first;
  const auto &root = built.first.second;
  const auto &leaf_names = built.second;
  print_structure_wrapped(edges, root, leaf_names, pr.first, outfs);
}

} // namespace Define_Obfuscation

// Reads protect.map into a set of strings (one identifier per line)
static std::unordered_set<std::string> readProtectMap(const std::string &path) {
    std::unordered_set<std::string> out;
    if (path.empty()) return out;
    std::ifstream in(path);
    if (!in) {
        llvm::errs() << "Warning: cannot open protect map '" << path << "'\n";
        return out;
    }
    std::string line;
    while (std::getline(in, line)) {
        // trim whitespace
        size_t a = 0, b = line.size();
        while (a < b && isspace((unsigned char)line[a])) ++a;
        while (b > a && isspace((unsigned char)line[b-1])) --b;
        if (b > a) out.insert(line.substr(a, b-a));
    }
    return out;
}

// Helper: whether a SourceLocation is in a system header or macro expansion
static bool isLocationInSystemHeaderOrMacro(const SourceManager &SM, SourceLocation Loc) {
    if (Loc.isInvalid()) return true;
    if (Loc.isMacroID()) return true;
    return SM.isInSystemHeader(SM.getSpellingLoc(Loc));
}

class ObfuscationVisitor : public RecursiveASTVisitor<ObfuscationVisitor> {
public:
    explicit ObfuscationVisitor(Rewriter &R, ASTContext &C, const std::unordered_set<std::string> &protect)
        : TheRewriter(R), Context(C), Protect(protect) {}

    bool VisitNamedDecl(NamedDecl *ND) {
        // We will capture declarations to assign obf names
        if (!ND->getIdentifier()) return true; // anonymous

        SourceLocation L = ND->getLocation();
        if (L.isInvalid()) return true;

        const SourceManager &SM = Context.getSourceManager();
        // skip system headers and macro locations
        if (isLocationInSystemHeaderOrMacro(SM, L)) return true;

        std::string qname = ND->getQualifiedNameAsString();
        // skip std::
        if (qname.rfind("std::", 0) == 0) return true;

        std::string name = ND->getNameAsString();
        if (name.empty()) return true;
        if (isKeyword(name)) return true;
        if (Protect.count(name)) return true;

        // avoid obfuscating compiler-internal or builtin names
        if (ND->isImplicit()) return true;

        // If already mapped, skip
        if (DeclToName.count(ND)) return true;

        // create obf name
        std::string ob = makeObfName();
        DeclToName[ND] = ob;
        NameToDecls[name].push_back(ND);
        return true;
    }

    bool VisitFunctionDecl(FunctionDecl *FD) {
        // also ensure parameters are captured via VisitNamedDecl but we may want to skip overriding
        return true;
    }

    bool VisitDeclRefExpr(DeclRefExpr *DRE) {
        ValueDecl *VD = DRE->getDecl();
        if (!VD) return true;
        NamedDecl *ND = dyn_cast<NamedDecl>(VD);
        if (!ND) return true;
        applyReplacementForUse(ND, DRE->getLocation());
        return true;
    }

    bool VisitMemberExpr(MemberExpr *ME) {
        // Member access: replace member name if mapped
        if (FieldDecl *FD = dyn_cast<FieldDecl>(ME->getMemberDecl())) {
            applyReplacementForUse(FD, ME->getMemberLoc());
        } else if (NamedDecl *ND = dyn_cast<NamedDecl>(ME->getMemberDecl())) {
            applyReplacementForUse(ND, ME->getMemberLoc());
        }
        return true;
    }

    bool VisitTypeLoc(TypeLoc TL) {
        // Handle TagTypeLoc (struct/class/union/enum) and TypedefTypeLoc
        if (TagTypeLoc TTL = TL.getAs<TagTypeLoc>()) {
            TagDecl *TD = TTL.getDecl();
            if (TD && TD->getIdentifier()) {
                applyReplacementForUse(TD, TTL.getNameLoc());
            }
        }
        if (TypedefTypeLoc TDL = TL.getAs<TypedefTypeLoc>()) {
            TypedefNameDecl *TDN = dyn_cast_or_null<TypedefNameDecl>(TDL.getTypedefNameDecl());
            if (TDN && TDN->getIdentifier()) {
                applyReplacementForUse(TDN, TDL.getNameLoc());
            }
        }
        if (ElaboratedTypeLoc ETL = TL.getAs<ElaboratedTypeLoc>()) {
            // e.g. "class Foo"
            if (NestedNameSpecifierLoc NNS = ETL.getQualifierLoc()) {
                // nothing to do for qualifier here, handled elsewhere
            }
        }
        return true;
    }

    bool VisitCXXRecordDecl(CXXRecordDecl *RD) {
        // Declaration rename handled by VisitNamedDecl
        return true;
    }

    bool VisitVarDecl(VarDecl *VD) {
        // handled by VisitNamedDecl
        return true;
    }

    bool VisitFieldDecl(FieldDecl *FD) {
        // handled by VisitNamedDecl
        return true;
    }

    bool VisitFunctionTemplateDecl(FunctionTemplateDecl *FTD) {
        // handled by VisitNamedDecl
        return true;
    }

    // After traversal we call applyDeclReplacements() to rename declarations (their spelling in source)
    void applyDeclReplacements() {
        const SourceManager &SM = Context.getSourceManager();
        for (auto &p : DeclToName) {
            const NamedDecl *ND = p.first;
            std::string ob = p.second;
            SourceLocation L = ND->getLocation();
            if (L.isInvalid()) continue;
            if (isLocationInSystemHeaderOrMacro(SM, L)) continue;
            // Obtain the token at location (the identifier) and replace it
            SourceLocation spellingLoc = SM.getSpellingLoc(L);
            Token tok;
            if (!Lexer::getRawToken(spellingLoc, tok, SM, Context.getLangOpts(), true)) {
                if (tok.isAnyIdentifier()) {
                    SourceLocation st = tok.getLocation();
                    CharSourceRange nameRange = CharSourceRange::getTokenRange(st);
                    TheRewriter.ReplaceText(nameRange, ob);
                }
            } else {
                // fallback: try simple replace of the single char range at L
                TheRewriter.ReplaceText(spellingLoc, ND->getNameAsString().size(), ob);
            }
        }
    }

    void flushRewrites() {
        // Nothing here; Rewriter writes in FrontendAction
    }

private:
    Rewriter &TheRewriter;
    ASTContext &Context;
    const std::unordered_set<std::string> &Protect;

    // mapping from declaration pointer to obfuscated name
    llvm::DenseMap<const NamedDecl*, std::string> DeclToName;
    std::unordered_map<std::string, std::vector<const NamedDecl*>> NameToDecls;

    static bool isKeyword(const std::string &s) {
        // Simple static list for C++ keywords - not exhaustive but covers common ones.
        static const std::unordered_set<std::string> kw = {
            "alignas","alignof","and","and_eq","asm","atomic_cancel","atomic_commit",
            "atomic_noexcept","auto","bitand","bitor","bool","break","case","catch","char",
            "char8_t","char16_t","char32_t","class","compl","concept","const","consteval",
            "constexpr","constinit","const_cast","continue","co_await","co_return","co_yield",
            "decltype","default","delete","do","double","dynamic_cast","else","enum","explicit",
            "export","extern","false","float","for","friend","goto","if","inline","int","long",
            "mutable","namespace","new","noexcept","not","not_eq","nullptr","operator","or",
            "or_eq","private","protected","public","register","reinterpret_cast","requires","return",
            "short","signed","sizeof","static","static_assert","static_cast","struct","switch",
            "synchronized","template","this","thread_local","throw","true","try","typedef","typeid",
            "typename","union","unsigned","using","virtual","void","volatile","wchar_t","while","xor","xor_eq"
        };
        return kw.count(s) > 0;
    }

    void applyReplacementForUse(NamedDecl *ND, SourceLocation Loc) {
        if (!ND) return;
        auto it = DeclToName.find(ND);
        if (it == DeclToName.end()) return;
        if (Loc.isInvalid()) return;
        const SourceManager &SM = Context.getSourceManager();
        if (isLocationInSystemHeaderOrMacro(SM, Loc)) return;
        // For a use, we get the token at Loc and replace it
        SourceLocation SpLoc = SM.getSpellingLoc(Loc);
        Token tok;
        if (!Lexer::getRawToken(SpLoc, tok, SM, Context.getLangOpts(), true)) {
            if (tok.isAnyIdentifier()) {
                SourceLocation tloc = tok.getLocation();
                CharSourceRange range = CharSourceRange::getTokenRange(tloc);
                TheRewriter.ReplaceText(range, it->second);
            }
        } else {
            // fallback
            TheRewriter.ReplaceText(SpLoc, ND->getNameAsString().size(), it->second);
        }
    }
};

std::string obfuscated_code;
class ObfuscationConsumer : public ASTConsumer {
public:
    explicit ObfuscationConsumer(Rewriter &R, ASTContext &C, const std::unordered_set<std::string> &protect)
        : Visitor(R, C, protect) {}

    void HandleTranslationUnit(ASTContext &Context) override {
        Visitor.TraverseDecl(Context.getTranslationUnitDecl());
        Visitor.applyDeclReplacements();
    }

private:
    ObfuscationVisitor Visitor;
};

class ObfuscationAction : public ASTFrontendAction {
public:
    ObfuscationAction(const std::unordered_set<std::string> &protect) : Protect(protect) {}

    std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI, StringRef InFile) override {
        TheRewriter.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
        return std::make_unique<ObfuscationConsumer>(TheRewriter, CI.getASTContext(), Protect);
    }

    void EndSourceFileAction() override {
        SourceManager &SM = TheRewriter.getSourceMgr();
        std::string fileprefix;
        std::string g_raw_obfuscated_code;

        for (auto it = SM.fileinfo_begin(); it != SM.fileinfo_end(); ++it) {
            const FileEntry *FE = it->first;
            if (!FE) continue;

            FileID FID = SM.translateFile(FE);
            if (FID.isInvalid()) continue;

            const RewriteBuffer *RB = TheRewriter.getRewriteBufferFor(FID);
            if (!RB) continue;

            // 🔒 只处理主源文件（避免头文件重复）
            SourceLocation Loc = SM.getLocForStartOfFile(FID);
            if (!SM.isWrittenInMainFile(Loc)) continue;

            // ===== 写 raw 文件 =====
            std::string rawPath = std::string(FE->getName()) + ".obf.raw.cpp";
            std::error_code EC;
            llvm::raw_fd_ostream rawOut(rawPath, EC, llvm::sys::fs::OF_None);
            if (EC) {
                llvm::errs() << "Cannot open " << rawPath << ": "
                            << EC.message() << "\n";
                return;
            }
            RB->write(rawOut);
            rawOut.close();

            // ===== 同时写入全局字符串 =====
            llvm::raw_string_ostream ss(g_raw_obfuscated_code);
            RB->write(ss);
            ss.flush();

            llvm::outs() << "Wrote intermediate file: " << rawPath << "\n";
            fileprefix = FE->getName();
            break; // 主 TU 只会有一个
        }

        // ===== 最终输出：Define_Obfuscation =====
        {
            std::string finalPath = fileprefix + ".obf.cpp";
            std::error_code EC;
            llvm::raw_fd_ostream finalOut(finalPath, EC, llvm::sys::fs::OF_None);
            if (EC) {
                llvm::errs() << "Cannot open " << finalPath << ": " << EC.message() << "\n";
                return;
            }

            // ⭐ 关键调用
            Define_Obfuscation::obf(g_raw_obfuscated_code, finalOut);
            finalOut.close();

            llvm::outs() << "Wrote final obfuscated file: " << finalPath << '\n';
        }
    }

private:
    Rewriter TheRewriter;
    const std::unordered_set<std::string> &Protect;
};

int main(int argc, const char **argv) {
    // Expect: obfuscator input.cpp [protect.map] -- [compiler options ...]
    if (argc < 2) {
        llvm::errs() << "Usage: obfuscator input.cpp [protect.map] -- [clang args]\n";
        return 1;
    }

    // We'll extract positional args ourselves to allow optional protect.map
    std::vector<std::string> args(argv + 1, argv + argc);
    std::string inputFile = args[0];
    std::string protectfile = "";
    // If second positional arg doesn't start with '-' and exists and there is a '--' later, treat as protect.map
    if (args.size() >= 2 && args[1] != "--") {
        // If there is a file-like second arg and not starting with '-', use as protect.map
        if (!args[1].empty() && args[1][0] != '-') {
            protectfile = args[1];
            // remove it from arg vector
            args.erase(args.begin() + 1);
        }
    }

    // find the '--' separator to split compiler args
    std::vector<std::string> compArgs;
    bool seenDashDash = false;
    for (size_t i = 1; i < (size_t)argc; ++i) {
        std::string s = argv[i];
        if (s == "--") { seenDashDash = true; continue; }
        if (seenDashDash) compArgs.push_back(s);
    }

    if (compArgs.empty()) {
        // Provide a minimal default compilation database: treat as C++20
        compArgs.push_back("-std=c++20");
    }

    std::vector<std::string> sources;
    sources.push_back(inputFile);

    FixedCompilationDatabase Compilations(".", compArgs);
    ClangTool Tool(Compilations, sources);

    auto protectSet = readProtectMap(protectfile);

    // We need to create a FrontendActionFactory that can carry the protectSet
    class Factory : public FrontendActionFactory {
    public:
        Factory(const std::unordered_set<std::string> &P) : Pset(P) {}
        std::unique_ptr<FrontendAction> create() override {
            return std::make_unique<ObfuscationAction>(Pset);
        }
    private:
        const std::unordered_set<std::string> &Pset;
    };

    // run the tool
    int res = Tool.run(new Factory(protectSet));
    if (res != 0) llvm::errs() << "ClangTool run returned " << res << "\n";
    return res;
}
