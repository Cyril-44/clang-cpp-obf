# cppobf — C++ 源码混淆器（Clang LibTooling / LLVM 18）

以初始稿（纯重命名器）为基础演进的项目。支持到 C++20 的全部用户定义名字重命名、
随机/压缩两种命名模式，并内置宏定义混淆附加功能；架构上为控制流扁平化与
混淆逻辑插入预留了 pass 槽位。

## 构建

```bash
cmake -B build -DLLVM_DIR=/usr/lib/llvm-18/lib/cmake/llvm \
               -DClang_DIR=/usr/lib/cmake/clang-18 -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

依赖：LLVM/Clang 18 开发包（`llvm-18-dev`、`clang-18` 的 libclang-cpp）。

## 用法

```bash
# 随机长名重命名（默认），结果写 stdout
./build/cppobf tests/FastMod.cpp -- clang++ -std=c++20

# 压缩名（a, b, ..., _, aa, ab...；同名复用同一短名）+ 指定种子
./build/cppobf src.cpp -compress -seed=42 -o out.cpp -- clang++ -std=c++20

# 压缩重命名 + 宏定义混淆（token 全部宏化）
./build/cppobf src.cpp -compress -macro-obf -seed=42 -o out.cpp -- clang++ -std=c++20

# 仅宏混淆（不重命名）
./build/cppobf src.cpp -no-rename -macro-obf -o out.cpp -- clang++ -std=c++20
```

`--` 之后是传给内置 clang 的编译参数（第一个是占位的编译器名）。

### 选项

| 选项 | 说明 |
|------|------|
| `-compress` | 压缩命名（53/63 进制最短编码；同一原始名处处复用同一短名） |
| `-min-len` / `-max-len` | 随机名长度范围（默认 12/20） |
| `-seed=N` | 随机种子（0 = 每次随机） |
| `-macro-obf` | 附加功能：宏定义混淆 |
| `-no-rename` | 关闭重命名 |
| `-o FILE` | 输出文件（默认 stdout） |
| `-flatten` / `-opaque` | 控制流扁平化 / 混淆逻辑插入（预留，当前报错退出） |

## 架构

```
main.cpp               驱动：CLI、ClangTool、pass 流水线组装
obf/Passes.hpp         ASTObfPass / TextObfPass 接口 + 扁平化/混淆逻辑插入预留槽位
obf/RenamePass.hpp     C++20 全量重命名 pass（两遍遍历：收集→分配→改写）
obf/MacroObfuscator.hpp 宏定义混淆（def_obf.py 的移植修正版，文本级）
obf/Support.hpp        C++ 分词器（pp-number/raw string/UDL/多字符运算符）+ 名字生成器
```

流水线：`[RenamePass] → (未来: ControlFlowFlatteningPass / OpaqueFlowPass)` 在
AST+Rewriter 上执行，随后 `MacroObfuscator` 对改写后的文本做变换。

### 重命名的设计要点（obf/RenamePass.hpp）

- **按「原始名 → 新名」的 TU 级映射**而非按 Decl 粒度：同名声明共享新名，
  重载集 / 模板特化 / 依赖名引用（`T::member`）天然一致，同名遮蔽拓扑不变；
- 引用点只在**目标声明写在主文件**时才替换，绝不触碰 std/头文件名字；
  依赖上下文（`CXXDependentScopeMemberExpr`、`DependentNameTypeLoc` 等）带
  「限定符/基类型全由本文件构成」的守卫，避免误伤 `std::enable_if<>::type`；
- 出现在**宏展开**中的名字自动冻结（宏体文本不可重写）；
- 新名避开源文件全部标识符、C++20 全部关键字与保留标识符（`__x`、`_X`）；
- 覆盖：变量/函数/成员/枚举值/类型与别名/命名空间及别名/各类模板/概念
  （含 type-constraint 位置）/结构化绑定/标签/构造与析构（含 `~Name`、基类与
  委托初始化、成员初始化列表）/指定初始化器/lambda 捕获/`sizeof...(pack)`/
  命名空间限定符等；跳过 `main`、运算符函数、extern "C"。

### 宏混淆的设计要点（相对旧版 def_obf.py 的修正）

- 预处理指令**在原位置**原样保留（指令永远不能来自宏展开）；
- 宏定义与指令流交错输出：`#include` 先于任何 define 执行，头文件在干净
  环境下解析（压缩短名不会撞上头文件内部标识符）；
- 分词修正：`::`、`...`、`->*`、`.*`、`<=>` 等不再被空格拆散；pp-number、
  raw string、编码前缀、UDL 后缀均为单一 token；`<::` 特判；
- **「标识符 + 平衡括号实参」合并为单个叶宏**，且孤立 `(`、`)` 字面输出：
  函数式宏（如 `assert`、源码自定义 `For(i,1,n)`）的名字、实参与闭括号必须
  位于同一替换列表 / 参数收集流中，否则调用无法成立、嵌套逗号会被错误切分；
- 压缩模式按 token 文本去重复用（同一个 `)` 只定义一次）；
- 保留叶宏 → 8 叉树 → 段顶宏 的层级结构与单行宏长度上限。

已知限制：跨行 raw string 以字面透传（不宏化）；文件中部夹在代码之间的
`#include` 之后若已输出大量短名宏定义，仍可能与其内部标识符冲突。

## 测试

```bash
tests/run_tests.sh          # 或指定工具路径: tests/run_tests.sh build/cppobf
```

对 `tests/` 下每个 `.cpp`（`.py` 跳过）：

- 基线可编译 ⇒ 四种模式（random / compress / compress+macro / macro）的产物
  必须编译通过；定义了 `main` 的还必须与原程序**运行行为一致**（stdout +
  退出码）；产物在 `tests/.obf_out/` 下的同名 `.*.cpp`。
- 基线不可编译（VSCode 片段模板 `$0`/`${1:...}`、自身有错的文件）⇒ 工具必须
  同样干净地失败（非零退出、不产出输出）。

当前结果：**79/79 全部通过**（18 个可编译文件 × 4 模式 + 7 个基线不可编译
文件的拒绝行为，`.py` 不适用）。
