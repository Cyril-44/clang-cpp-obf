// cppobf — C++ 源码混淆器（基于 Clang LibTooling / LLVM 18）
//
// 在初始稿（纯重命名器）基础上的演进：
//   * pass 流水线架构：AST 级 pass（重命名 → 未来的扁平化/混淆逻辑插入）
//     + 文本级 pass（宏定义混淆）；
//   * 重命名覆盖 C++20 全部用户定义名字（详见 obf/RenamePass.hpp）；
//   * 随机 / 压缩两种命名模式；
//   * 宏定义混淆作为附加功能（-macro-obf）。
//
// 用法：
//   cppobf src.cpp [flags] -- clang++ -std=c++20
//   cppobf src.cpp -compress -o out.cpp -- clang++ -std=c++20
//   cppobf src.cpp -compress -macro-obf -seed=42 -o out.cpp -- clang++ -std=c++20
#include <clang/AST/ASTContext.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendActions.h>
#include <clang/Rewrite/Core/Rewriter.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>

#include <llvm/Support/raw_ostream.h>

#include "obf/MacroObfuscator.hpp"
#include "obf/Passes.hpp"
#include "obf/RenamePass.hpp"
#include "obf/Support.hpp"

#include <fstream>
#include <memory>
#include <vector>

using namespace clang;
using namespace clang::tooling;

llvm::cl::OptionCategory OBFCategory("cppobf Options");

static llvm::cl::opt<bool> Compress(
    "compress",
    llvm::cl::desc("使用压缩名字（a, b, ..., _, aa, ab, ...；同原始名复用同一短名）"),
    llvm::cl::init(false), llvm::cl::cat(OBFCategory));

static llvm::cl::opt<size_t> MinLen(
    "min-len", llvm::cl::desc("随机名字最小长度"),
    llvm::cl::init(12), llvm::cl::cat(OBFCategory));

static llvm::cl::opt<size_t> MaxLen(
    "max-len", llvm::cl::desc("随机名字最大长度"),
    llvm::cl::init(20), llvm::cl::cat(OBFCategory));

static llvm::cl::opt<unsigned long long> Seed(
    "seed", llvm::cl::desc("随机种子（0 = 不固定）"),
    llvm::cl::init(0), llvm::cl::cat(OBFCategory));

static llvm::cl::opt<bool> MacroObf(
    "macro-obf",
    llvm::cl::desc("附加功能：宏定义混淆（token → #define 树）"),
    llvm::cl::init(false), llvm::cl::cat(OBFCategory));

static llvm::cl::opt<bool> NoRename(
    "no-rename", llvm::cl::desc("关闭标识符重命名"),
    llvm::cl::init(false), llvm::cl::cat(OBFCategory));

static llvm::cl::opt<std::string> Output(
    "o", llvm::cl::desc("输出文件路径（默认写 stdout）"),
    llvm::cl::cat(OBFCategory));

static llvm::cl::opt<bool> Flatten(
    "flatten", llvm::cl::desc("控制流扁平化（预留，尚未实现）"),
    llvm::cl::init(false), llvm::cl::cat(OBFCategory));

static llvm::cl::opt<bool> Opaque(
    "opaque", llvm::cl::desc("插入混淆逻辑/不透明谓词（预留，尚未实现）"),
    llvm::cl::init(false), llvm::cl::cat(OBFCategory));

bool obf::ControlFlowFlatteningPass::NotYetImplemented = false;
bool obf::OpaqueFlowPass::NotYetImplemented = false;

namespace {

struct RunOutcome {
    std::string Text;
    bool Failed = false;
    bool GotResult = false;
};

// 依据全局选项组装 AST pass 流水线（每次动作各建一份）
static std::vector<std::unique_ptr<obf::ASTObfPass>> buildPipeline() {
    std::vector<std::unique_ptr<obf::ASTObfPass>> passes;
    if (!NoRename) {
        auto rename = std::make_unique<obf::RenamePass>();
        rename->Compress = Compress;
        rename->MinLen = MinLen;
        rename->MaxLen = MaxLen;
        rename->Seed = Seed;
        passes.push_back(std::move(rename));
    }
    if (Flatten)
        passes.push_back(std::make_unique<obf::ControlFlowFlatteningPass>());
    if (Opaque)
        passes.push_back(std::make_unique<obf::OpaqueFlowPass>());
    return passes;
}

struct ObfASTConsumer : public ASTConsumer {
    Rewriter &R;
    std::vector<std::unique_ptr<obf::ASTObfPass>> Passes;
    RunOutcome *Out;

    ObfASTConsumer(Rewriter &R,
                   std::vector<std::unique_ptr<obf::ASTObfPass>> passes,
                   RunOutcome *out)
        : R(R), Passes(std::move(passes)), Out(out) {}

    void HandleTranslationUnit(ASTContext &Ctx) override {
        for (auto &P : Passes) {
            P->run(R, Ctx);
            if (obf::ControlFlowFlatteningPass::NotYetImplemented ||
                obf::OpaqueFlowPass::NotYetImplemented) {
                Out->Failed = true;
                return;
            }
        }
        FileID FID = Ctx.getSourceManager().getMainFileID();
        std::string text;
        llvm::raw_string_ostream os(text);
        R.getEditBuffer(FID).write(os);
        os.flush();
        Out->Text = std::move(text);
        Out->GotResult = true;
    }
};

struct ObfFrontendAction : public ASTFrontendAction {
    RunOutcome *Out;
    explicit ObfFrontendAction(RunOutcome *out) : Out(out) {}

    std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI,
                                                   StringRef) override {
        R.setSourceMgr(CI.getSourceManager(), CI.getLangOpts());
        return std::make_unique<ObfASTConsumer>(R, buildPipeline(), Out);
    }
    Rewriter R;
};

class ObfActionFactory : public FrontendActionFactory {
public:
    explicit ObfActionFactory(RunOutcome *out) : Out(out) {}
    std::unique_ptr<FrontendAction> create() override {
        return std::make_unique<ObfFrontendAction>(Out);
    }
    RunOutcome *Out;
};

} // namespace

int main(int argc, const char **argv) {
    llvm::cl::HideUnrelatedOptions(OBFCategory);
    auto ExpectedParser = CommonOptionsParser::create(argc, argv, OBFCategory);
    if (!ExpectedParser) {
        llvm::errs() << llvm::toString(ExpectedParser.takeError()) << "\n";
        return 1;
    }
    CommonOptionsParser &Parser = *ExpectedParser;
    if (Parser.getSourcePathList().size() != 1) {
        llvm::errs() << "obf: error: 请一次只处理一个源文件\n";
        return 1;
    }

    ClangTool Tool(Parser.getCompilations(), Parser.getSourcePathList());

    RunOutcome outcome;
    ObfActionFactory factory(&outcome);

    // 解析失败（含片段模板等非法输入）时工具必须失败、不产出输出
    int ret = Tool.run(&factory);
    if (ret != 0) {
        llvm::errs() << "obf: error: 解析/改写失败，未生成输出\n";
        return ret;
    }
    if (!outcome.GotResult) {
        llvm::errs() << "obf: error: 未产生结果\n";
        return 1;
    }
    if (outcome.Failed) return 2;

    std::string text = std::move(outcome.Text);

    // 文本级 pass：宏定义混淆
    if (MacroObf) {
        obf::MacroObfuscator macro;
        macro.Compress = Compress;
        macro.MinLen = MinLen;
        macro.MaxLen = MaxLen;
        macro.Seed = Seed;
        text = macro.run(text);
    }

    if (Output.empty()) {
        llvm::outs() << text;
    } else {
        std::ofstream of(Output.getValue(), std::ios::binary);
        if (!of) {
            llvm::errs() << "obf: error: 无法写入 " << Output.getValue()
                         << "\n";
            return 1;
        }
        of << text;
    }
    return 0;
}
