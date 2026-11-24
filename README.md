# cppobf — C++ 源码混淆器（Clang LibTooling / LLVM 18）

以初始稿（纯重命名器）为基础演进的项目。支持到 C++20 的全部用户定义名字重命名、
随机/压缩两种命名模式，并内置宏定义混淆附加功能；架构上为控制流扁平化与
混淆逻辑插入预留了 pass 槽位。

同样支持 **C 源文件**（`.c`，语言模式由扩展名决定）：除依赖 lambda 的 `-rt` 外，
重命名 / 压缩 / 扁平化 / 混淆逻辑插入 / 宏混淆都可用。`tests/checker.c`
（单文件 QuickJS-NG，14 万行、宏密集、CRLF 行尾）即以 C 模式回归。

## 构建

```bash
cmake -B build -DLLVM_DIR=/usr/lib/llvm-18/lib/cmake/llvm \
               -DClang_DIR=/usr/lib/cmake/clang-18 -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

依赖：LLVM/Clang 18 开发包（`llvm-18-dev`、`clang-18` 的 libclang-cpp），以及系统
自带的 C / C++ 编译器。`project()` 同时启用 C 与 C++ 是必需的：LLVM 的 CMake
配置脚本会试编译一小段 C 代码来探测可选依赖，只启用 C++ 时，缓存里带有
`CMAKE_C_COMPILER` 的构建目录（例如 VSCode 的 CMake 插件按 kit 配置出来的）
会报 `check_source_compiles: C: needs to be enabled before use` 并配置失败。

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

# C 源文件：压缩重命名 + 控制流扁平化
./build/cppobf checker.c --compress --flatten -o out.c -- clang -std=gnu11
```

`--` 之后是传给内置 clang 的编译参数（第一个是占位的编译器名）。语言模式跟随
**源文件扩展名**，`-std` 要与之匹配：`.c` 用 `-std=gnu11` 一类，`.cpp` 用
`-std=c++20`。

### 选项

| 选项 | 说明 |
|------|------|
| `-compress` | 压缩命名（53/63 进制最短编码；同一原始名处处复用同一短名） |
| `-min-len` / `-max-len` | 随机名长度范围（默认 12/20） |
| `-seed=N` | 随机种子（0 = 每次随机） |
| `-macro-obf` | 附加功能：宏定义混淆 |
| `-no-rename` | 关闭重命名 |
| `-o FILE` | 输出文件（默认 stdout） |
| `-flatten` | 控制流扁平化（函数体 → 状态机分派循环） |
| `-opaque` | 插入混淆逻辑：常量 XOR 分解 + 不透明谓词垃圾块 |
| `-rt` | 常量/字符串**运行时**混淆：密文静态存储、运行期解密（防常量折叠，二进制无明文）。仅 C++：解密用 lambda 实现 |
| `-reserve=N` | 保留名（可重复/逗号分隔）：`-reserve=Mint` 保留该名字；`-reserve=MaxFlow` 保留类名并延申保留其全部 public 方法名；`-reserve=MaxFlow::*` 仅延申保留 public 方法名 |
| `-keep-comments` | 保留注释（**默认自动去除注释**：所有注释替换为一个空格，防 token 粘连） |

## 架构

```
main.cpp               驱动：CLI、ClangTool、pass 流水线组装
obf/Passes.hpp         ASTObfPass / TextObfPass 接口 + 各 pass 共用的禁用名集合
obf/RenamePass.hpp     C++20 全量重命名 pass（两遍遍历：收集→分配→改写）
obf/FlattenPass.hpp    控制流扁平化（纯插入式改写）
obf/OpaquePass.hpp     插入混淆逻辑：常量 XOR 分解 + 不透明谓词垃圾块
obf/MacroObfuscator.hpp 宏定义混淆（def_obf.py 的移植修正版，文本级）
obf/Compress.hpp       代码版式压缩（随 -compress 启用）
obf/Support.hpp        C/C++ 分词器（pp-number/raw string/UDL/多字符运算符/续行）+ 名字生成器
```

流水线：`[RenamePass] → [FlattenPass] → [OpaqueFlowPass]` 在 AST+Rewriter 上
依次执行，随后 `MacroObfuscator` 对改写后的文本做变换。各 pass 的编辑互不
重叠：Rename 按 token 范围替换；Flatten 纯插入；Opaque 避开 Flatten 的插入点。

所有生成新名字的 pass 共用同一份禁用名集合（`forbiddenNames`）：当前文本里的
全部标识符，加上**预处理阶段见过的全部标识符**。后者覆盖系统头文件里的宏名与
声明——压缩短名一旦撞上它们，主文件的新声明会与头文件声明冲突（如 glibc 的
`j0/j1/jn/y0/y1/yn`），或被同名宏替换掉。

### 控制流扁平化的形态（obf/FlattenPass.hpp）

函数体的顶层语句序列被改写为状态机分派循环：

```cpp
<前导声明原样保留>
<垃圾块>
unsigned st = c0^K; const unsigned K = ...; bool done = false;
while (!done) switch (st ^= K) {
case c0: { <原语句0 原地保留> ; } st = c1^K; continue;
case c1: { <原语句1 原地保留> ; } st = c2^K; continue;
...
default: done = true; break;
}
for(;;){}   // 仅当函数返回类型非 void 且所有路径都以 return 收尾
```

返回类型非 void、且所有路径都以 `return` 收尾的函数（识别 return / throw /
noreturn 调用 / if-else 双分支 / 带默认分支且各分支都 return 的 switch / 无
`break` 的无限循环），`default` 出口实际不可达，但编译器看不出来，会报
`-Wreturn-type`；循环之后补一个死循环把“落空”路径显式封死。判断是保守的，
拿不准的形态不加。

C 模式下（C23 之前）`bool/false/true` 只是 `<stdbool.h>` 里的宏，生成的
`bool done = false` 改为 `int done = 0`。

工程决策：**纯插入式改写**——不替换任何既有文本，只在语句边界（`;`/`}`/`{`
之后，语句永不以标识符结尾）插入调度器文本，因此与 Rename 的按 token 替换
零重叠，且被包裹语句仍在原 AST 节点上被正常重命名。声明语句（含 VLA）全部
留在前导序贯区（case 块间跳转会跨越初始化）。跳过：含 goto/label 的函数、
协程体、constexpr/consteval、构造/析构、语句含宏展开、可用语句少于 2 条。

### 混淆逻辑插入（obf/OpaquePass.hpp）

- **常量 XOR 分解**：值 ≥ 10 的整数字面量替换为
  `((T)(A ^ B))`（A、B 为随机 64 位常数，异或还原原值）——分解结果仍是
  常量表达式，case 标签 / 数组界 / 模板实参 / 枚举初值等位置全部可用；
- **不透明谓词垃圾块**：`n*n+n` 恒为偶数这一事实驱动的自包含计算块，
  插入未扁平化函数的顶层语句前（已扁平化函数的垃圾块由 FlattenPass 在
  每个 case 中插入），无副作用；
- 生成的新名字取自"当前已改写缓冲区"的禁用集合，逐 pass 收紧，
  绝不与前序 pass 的产物撞名。

### 常量/字符串运行时混淆（`-rt`）

静态 XOR 分解只混淆源码——编译期常量折叠会把值还原进二进制。运行时
混淆让解密发生在运行期，二进制中不再出现明文：

- **整数字面量** → `([]{ static T v = <密文>; return (T)(v ^ K); }())`：
  密文 `密文 = 原值 ^ K` 预折叠，`v ^ K` 经内存加载后异或，阻断折叠；
- **窄字符串** → `([]{ static char b[] = {密文字节}; static bool o = []
  { for(...) b[i] ^= (char)(K1 + i*K2); return true; }(); return b; }())`：
  位置相关密钥逐字节加密（含结尾 NUL，源码中无 0x00 泄漏长度），
  magic static 保证解密一次、线程安全；`strings` 扫描二进制无明文。

上下文判定是正确性的关键：运行时形式是普通（非常量）表达式，只能用于
普通表达式位置。pass 沿 ParentMap 祖先链识别并跳过 case 标签、模板实参、
枚举初值、`static_assert`、`sizeof`、`asm`、位域、数组界（按"是否位于
初始化器内"精确区分）、`char a[] = "x"` 数组推导、UDL 后缀（`1_km`）、
constexpr/const 声明与 constexpr 函数体等位置——这些位置自动回退到
静态 XOR 分解（仍混淆源码）。

### 重命名的设计要点（obf/RenamePass.hpp）

- **按「原始名 → 新名」的 TU 级映射**而非按 Decl 粒度：同名声明共享新名，
  重载集 / 模板特化 / 依赖名引用（`T::member`）天然一致，同名遮蔽拓扑不变；
- 引用点只在**目标声明写在主文件**时才替换，绝不触碰 std/头文件名字；
  依赖上下文（`CXXDependentScopeMemberExpr`、`DependentNameTypeLoc` 等）带
  「限定符/基类型全由本文件构成」的守卫，避免误伤 `std::enable_if<>::type`；
- 出现在**宏展开**中的名字自动冻结（宏体文本不可重写）。宏实参里被宏体当作
  **裸 token**使用的名字——`offsetof(T, member)` / `container_of(p, T, member)`
  的成员名、`#x` 字符串化、`a##b` 粘接——在 AST 中没有任何节点，遍历看不到：
  由预处理回调记录主文件里每次宏调用的源码区间，落在区间内的名字一律冻结；
- 新名避开源文件全部标识符、预处理阶段见过的全部标识符（系统头文件的宏名
  与声明）、C++20 全部关键字与保留标识符（`__x`、`_X`）；
- 覆盖：变量/函数/成员/枚举值/类型与别名/命名空间及别名/各类模板/概念
  （含 type-constraint 位置）/结构化绑定/标签/构造与析构（含 `~Name`、基类与
  委托初始化、成员初始化列表）/指定初始化器/lambda 捕获/`sizeof...(pack)`/
  命名空间限定符等；跳过 `main`、运算符函数、extern "C"。C 模式下没有
  linkage specification（`isExternC()` 对所有外部链接的函数恒为真），除 `main`
  外一律重命名；需要保持符号名的用 `-reserve`。

### 代码版式压缩（随 -compress 自动启用）

`-compress` 在压缩命名的同时自动压缩代码版式（移植自用户提供的 JS 参考实现，
算法保持一致）：

- 预处理：反斜杠续行拼接（含 CRLF）、块注释 → 单空格、行注释删除、
  `%:` → `#`；三字符组归一（`??=` → `#` 等）**只在编译器启用三字符组时**
  做（严格 ISO 模式；gnu 模式与 C++17 起默认关闭，此时 `??!` 是三个普通字符）；
- 逐行划分 token（空白分隔，字符串/字符字面量含转义不切分），token 间按需
  保留最小间隔：标识符字符相邻、同号 `+`/`-` 相邻、`/` 与 `*` 相邻时必须留
  一个空格（防粘连改变语义），其余空白全部去除；
- `#` 指令行独立成行并保持 `#define NAME` 的名称分隔；原始字符串原样旁路
  （`R"(...)"`、带自定义分隔符的 `R"x(...)x"`、`u8R`/`LR` 等前缀，由分词器
  定位——按 `R"(` … `)"` 的字面模式查找会漏掉自定义分隔符，内容被当成代码
  压缩而静默改写）；整体执行两遍以稳定版式。

字面量统一经 `lexLiteralEndInLine` 整体跳过（参考实现自带的引号状态跟踪对
`'\\'` 这类转义的收尾判断有误，会把字符串里的 `"/*"` 当成注释开头整段删除）。
分词器对续行的识别同样覆盖 CRLF：`\` + CR + LF。

效果示例：`int main( void ) { ... }` → `int main(void){...}`（单行）。

### 保留名（-reserve）

需要与外部交互的接口（库类、全局配置等）可以豁免重命名：

```bash
cppobf ModInt.cpp -reserve=Mint ...                # 保留 Mint（类/别名/函数/变量名）
cppobf MaxFlow.cpp -reserve=MaxFlow ...            # 保留类名 + 其全部 public 方法名
cppobf graph.cpp -reserve=edgs -reserve=edghead ...# 保留全局变量
cppobf lib.cpp -reserve=Lib::* ...                 # 仅保留 public 方法名（类名仍混淆）
```

保留在收集阶段从待重命名集合中整体剔除（同名声明与引用一致保持原名）。
注意映射按名字进行：保留某 public 方法名后，其他类中的同名方法也会
一并保持原名。

### 宏混淆的设计要点（相对旧版 def_obf.py 的修正）

- 预处理指令**在原位置**原样保留（指令永远不能来自宏展开）；
- 宏定义与指令流交错输出：`#include` 先于任何 define 执行，头文件在干净
  环境下解析（压缩短名不会撞上头文件内部标识符）；
- 分词修正：`::`、`...`、`->*`、`.*`、`<=>` 等不再被空格拆散；pp-number、
  raw string、编码前缀、UDL 后缀均为单一 token；`<::` 特判；
- **「标识符 + 平衡括号实参」合并为单个叶宏**，且孤立 `(`、`)` 字面输出：
  函数式宏（如 `assert`、源码自定义 `For(i,1,n)`）的名字、实参与闭括号必须
  位于同一替换列表 / 参数收集流中，否则调用无法成立、嵌套逗号会被错误切分；
- 压缩模式按 token 文本去重复用（同一个 `)` 只定义一次）。去重作用域随条件
  编译分层：`#if` 族压入新作用域、`#else`/`#elif` 清空当前层、`#endif` 弹出——
  宏定义写在它首次出现的分支里，分支外的代码不能引用（否则 C 模式下
  `#ifdef __cplusplus` 分支里的定义全部失效）；
- 保留叶宏 → 8 叉树 → 段顶宏 的层级结构与单行宏长度上限。

已知限制：含换行的 token（跨行 raw string、续行拆开的字符串字面量）不宏化，
原样透传（其后另起一行，不打乱后续的 `#define`）；文件中部夹在代码之间的
`#include` 之后若已输出大量短名宏定义，仍可能与其内部标识符冲突；被 `#`
字符串化的宏实参，token 之间的空白会被规整为单个空格（断言消息的文本可能
与原文件不同，语义不变）。

## 测试

```bash
tests/run_tests.sh          # 或指定工具路径: tests/run_tests.sh build/cppobf
```

对 `tests/` 下每个 `.cpp` / `.c` 文件（`.py` / `.js` 跳过），按扩展名选语言：
`.cpp` 用 `clang++ -std=c++20`，`.c` 用 `clang -std=gnu11`。

- 基线可编译 ⇒ 各模式的产物必须编译通过；定义了 `main` 的还必须与原程序
  **运行行为一致**（stdout + 退出码；基线程序自身因 UB 崩溃时跳过对比）。产物在
  `tests/.obf_out/` 下的 `<文件名>.<模式>.<扩展名>`。
  - `.cpp`：九种模式（random / compress / compress+macro / macro / flatten /
    opaque / rt / full 全家桶 / compress+flatten）；
  - `.c`：C 语言可用的八种（random / compress / compress+macro / macro /
    flatten / opaque / compress+flatten / compress+flatten+opaque）；`rt` 与
    full 依赖 lambda，不适用。
- 基线不可编译（VSCode 片段模板 `$0`/`${1:...}`、自身有错的文件）⇒ 工具必须
  同样干净地失败（非零退出、不产出输出）。

专项检查：

- **reserve 保留名**（按文件配置，如 ModInt→Mint、MaxFlow 文件→MaxFlow、
  listed_graph 系列→edgs/edghead）：保留名必须存活在产物中，其余名字照常混淆，
  产物必须编译通过。
- **注释清除**：默认自动去除，`-keep-comments` 可保留。
- **版式压缩 / 宏混淆的保真**：对每个可编译文件，`-no-rename -compress`、
  `-no-rename -macro-obf`、`-no-rename -compress -macro-obf` 三种产物预处理后的
  内容（去空白与数字）必须与原文件一致——文本级 pass 不得改变程序内容。
- **压缩短名与头文件冲突**：现场生成一个含 800 个函数的 C 文件，让短名推进到
  两位字母区间（撞上 glibc 的 `j0/j1/jn/y0/y1/yn` 一类名字），产物必须可编译
  且运行结果正确。
- **checker.c 特判程序场景对比**：`checker <infile> <outfile>` 是特殊裁判，无参
  时只走到用法提示。17 个固定场景（用法错误、输入不可读、输入/输出不合法、
  接受路径……）下，各模式产物的 stdout / stderr / 退出码必须与原程序一致。
- **checker.c 引擎差分测试**：checker.c 内嵌完整的 QuickJS-NG。把它的 `main`
  换成“执行命令行指定的 JS 文件”，原版与扁平化产物各跑一遍
  `tests/checker_engine_tests.js`（190 余个用例：解析器、正则、数值格式化、
  TypedArray、GC、生成器、异步……），输出必须逐字节一致。

C 语言用例（各对应一类曾导致 checker.c 产物编译失败的问题）：`c_macros.c`
（宏实参里的裸 token、C 链接名、变参宏）、`c_flatten.c`（不含 stdbool 的扁平化、
各控制流形态的出口保护）、`c_literals.c`（文本级 pass 的字面量边界）、
`c_crlf.c`（CRLF 行尾的续行与注释）。`RawString.cpp` 覆盖原始字符串在文本级
pass 中的边界。

当前结果：**311/311 全部通过**（19 个可编译的 `.cpp` × 9 模式 + 5 个 `.c` ×
8 模式、每个可编译文件 3 项保真检查、7 个基线不可编译文件的拒绝行为，以及上列
专项检查）。
