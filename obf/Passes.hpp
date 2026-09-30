// obf/Passes.hpp — 混淆 pass 流水线骨架
//
// 两类 pass：
//   ASTObfPass：在 Clang AST + Rewriter 上工作（重命名、控制流扁平化、
//               插入混淆逻辑等），按注册顺序执行；
//   TextObfPass：对 AST 阶段产出的最终文本做变换（宏定义混淆）。
//
// 预留槽位（本轮仅搭好接口与调用路径）：
//   * ControlFlowFlatteningPass — 控制流扁平化
//   * OpaqueFlowPass            — 不透明谓词 / 插入混淆逻辑
// 两者在 main.cpp 中可通过 -flatten / -opaque 启用；当前会明确报告
// “未实现”并退出，保证接口先行、行为不撒谎。
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

// ===== 控制流扁平化（预留） =====
// 计划实现：
//   对每个函数体 CompoundStmt 用 Rewriter 整体重写为
//     <unsigned st = <随机常数>;>
//     while (true) switch (st ^= <随机常数>) {
//       case <c1>: <原基本块1>; st = <下一状态>; continue;
//       ...
//       default: return ...;
//     }
//   需要处理：break/continue 改写为状态跳转、return 值经局部变量汇聚、
//   局部声明提升到 switch 之前、VLA 例外跳过。
class ControlFlowFlatteningPass final : public ASTObfPass {
public:
    const char *name() const override { return "control-flow-flattening"; }
    void run(clang::Rewriter &, clang::ASTContext &) override {
        llvm::errs() << "obf: error: control flow flattening (-flatten) "
                        "尚未实现，仅预留接口\n";
        NotYetImplemented = true;
    }
    static bool NotYetImplemented;
};

// ===== 插入混淆逻辑 / 不透明谓词（预留） =====
// 计划实现：
//   在语句间插入恒真/恒假的谓词表达式与垃圾计算（结果参与无害赋值），
//   以及对整数常数做 XOR 分解（沿用旧项目 ((int)(((ull)K1)^K2)) 风格）。
class OpaqueFlowPass final : public ASTObfPass {
public:
    const char *name() const override { return "opaque-flow"; }
    void run(clang::Rewriter &, clang::ASTContext &) override {
        llvm::errs() << "obf: error: opaque logic insertion (-opaque) "
                        "尚未实现，仅预留接口\n";
        NotYetImplemented = true;
    }
    static bool NotYetImplemented;
};

} // namespace obf
