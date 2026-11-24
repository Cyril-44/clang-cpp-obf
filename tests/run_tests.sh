#!/usr/bin/env bash
# tests/run_tests.sh — cppobf 回归测试
#
# 对 tests/ 下每个 .cpp / .c 文件（.py / .js 跳过）：
#   .cpp：C++ 模式（clang++ -std=c++20），全部模式；
#   .c  ：C 模式（clang -std=gnu11），C 语言可用的模式——-rt 的运行期解密用
#         lambda 实现，仅支持 C++，含它的 rt / full 两种模式对 .c 不运行。
#   * 基线可编译  ⇒ 所有模式的混淆产物必须可编译；
#     定义了 main 的还必须与原程序运行行为一致（stdout + 退出码）
#   * 基线不可编译（VSCode 片段模板、本身有错的文件）⇒ 工具必须同样
#     干净地失败（非零退出、不产出输出）——行为与基线一致才算通过
#
# 文件之后是专项检查：
#   reserve 保留名 / 注释清除 / 版式压缩与宏混淆的保真 / 压缩短名与头文件冲突 /
#   checker.c 特判程序场景对比 / checker.c 内嵌 QuickJS 引擎的差分测试。
#
# 资源护栏：每个编译限制 4GB 虚拟内存 + 180s；每次运行限制 15s。病理性用例只判
# 失败，不会拖垮机器。（模板+concepts 的文件在 g++10 下语法检查可耗 6.6GB，
# 故验证编译器优先用 clang。）
#
# 用法：tests/run_tests.sh [工具路径]   （默认 build/cppobf）
set -u
shopt -s nullglob   # 没有 .cpp / .c 文件时不迭代字面通配符

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TOOL="${1:-$ROOT/build/cppobf}"
OUT="$ROOT/tests/.obf_out"
STD=-std=c++20
CSTD=-std=gnu11
RUNTIME_TIMEOUT=15
COMPILE_TIMEOUT=180
MEMCAP_KB=4194304   # 4GB 虚拟内存上限

# 验证编译器：优先 clang（内存占用小一个量级），退回 gcc
if command -v clang++ >/dev/null 2>&1; then
    CXX=clang++; CC=clang
elif [ -x /usr/lib/llvm-18/bin/clang++ ]; then
    CXX=/usr/lib/llvm-18/bin/clang++; CC=/usr/lib/llvm-18/bin/clang
else
    CXX=g++; CC=gcc
fi

# 受限编译：guarded_compile <日志文件> <编译器参数...>
# 编译器与语言标准取自 CCUR / STDUR（按当前文件的类型设置）；
# 日志固定为第一个参数，其余原样作为编译器命令行，避免参数错位
CCUR="$CXX"; STDUR="$STD"
guarded_compile() {
    local log="$1"; shift
    ( ulimit -v $MEMCAP_KB 2>/dev/null
      exec timeout -s KILL $COMPILE_TIMEOUT $CCUR $STDUR -w -I "$ROOT/tests" "$@" ) >"$log" 2>&1
}

# 预处理后的内容指纹：去掉全部空白与数字后取哈希。
# 版式压缩、宏混淆都不该改变预处理结果；去掉空白是因为它们会重排版式，
# 去掉数字是因为行号（__LINE__、断言里的行号）随版式变化。
# 两个文件须同名（__FILE__ 一致），分别放在 $OUT/ppA $OUT/ppB 下
pp_hash() { # <目录>
    ( cd "$1" && $CCUR $STDUR -E -P x.c 2>/dev/null | tr -d '[:space:][:digit:]' | sha256sum | cut -c1-32 )
}

[ -x "$TOOL" ] || { echo "工具不存在: $TOOL（先 cmake --build build）"; exit 2; }
mkdir -p "$OUT"

# 模式: 名称:工具flags
MODES=(
    "random:-seed=42"
    "compress:-compress -seed=42"
    "compress+macro:-compress -macro-obf -seed=42"
    "macro:-no-rename -macro-obf -seed=42"
    "flatten:-flatten -seed=42"
    "opaque:-opaque -seed=42"
    "rt:-rt -seed=42"
    "full:-compress -flatten -opaque -rt -macro-obf -seed=42"
    "compress+flatten:-compress -flatten -seed=42"
)
# C 语言可用的模式（-rt / full 含 lambda，仅支持 C++）
C_MODES=(
    "random:-seed=42"
    "compress:-compress -seed=42"
    "compress+macro:-compress -macro-obf -seed=42"
    "macro:-no-rename -macro-obf -seed=42"
    "flatten:-flatten -seed=42"
    "opaque:-opaque -seed=42"
    "compress+flatten:-compress -flatten -seed=42"
    "compress+flatten+opaque:-compress -flatten -opaque -seed=42"
)

# 链接某些文件需要的额外库
declare -A LINKFLAGS=(
    ["checker.c"]="-ldl -lpthread -lm"
)

pass=0; fail=0
declare -a FAILED=()

record() { # ok name detail
    if [ "$1" = ok ]; then pass=$((pass+1));
    else fail=$((fail+1)); FAILED+=("$2 — $3"); echo "  FAIL: $3"; fi
}

for f in "$ROOT"/tests/*.cpp "$ROOT"/tests/*.c; do
    name="$(basename "$f")"
    echo "== $name"

    if [ "${f##*.}" = c ]; then
        CCUR="$CC"; STDUR="$CSTD"; MODELIST=("${C_MODES[@]}")
    else
        CCUR="$CXX"; STDUR="$STD"; MODELIST=("${MODES[@]}")
    fi

    base_ok=0
    if guarded_compile "$OUT/$name.base.log" -fsyntax-only "$f"; then
        base_ok=1
    fi

    if [ "$base_ok" = 0 ]; then
        # 基线即不可编译：工具必须同样失败
        if "$TOOL" "$f" -o "$OUT/should-not-exist.cpp" -- $CCUR $STDUR \
            >/dev/null 2>&1; then
            record fail "$name" "基线不可编译，但工具成功产出了输出"
        else
            record ok "$name" "基线不可编译，工具正确拒绝"
        fi
        continue
    fi

    # 原程序只链接一次：各模式产物的运行结果都与它对比
    has_main=0
    grep -qE '\bmain[[:space:]]*\(' "$f" && has_main=1
    orig_ok=0; obin="$OUT/$name.orig.bin"
    if [ "$has_main" = 1 ]; then
        if guarded_compile "$OUT/$name.link.log" -o "$obin" "$f" ${LINKFLAGS[$name]:-}; then
            orig_ok=1
            ( timeout $RUNTIME_TIMEOUT "$obin" </dev/null >"$OUT/$name.orig.out" 2>/dev/null; echo $? >"$OUT/$name.orig.rc" )
        fi
    fi

    for m in "${MODELIST[@]}"; do
        mode="${m%%:*}"; flags="${m#*:}"
        out="$OUT/$name.$mode.${f##*.}"   # 扩展名跟随源文件：C 产物要按 C 编译
        if ! timeout -s KILL 120 "$TOOL" "$f" -o "$out" $flags -- $CCUR $STDUR \
            2>"$OUT/$name.$mode.log"; then
            record fail "$name [$mode]" "工具运行失败/超时（见 $OUT/$name.$mode.log）"
            continue
        fi
        # 混淆产物必须可编译
        if ! guarded_compile "$OUT/$name.$mode.cc.log" -fsyntax-only "$out"; then
            record fail "$name [$mode]" "混淆产物编译失败/资源超限（见 $OUT/$name.$mode.cc.log）"
            continue
        fi
        # 运行等价性
        if [ "$has_main" = 1 ]; then
            fbin="$OUT/$name.$mode.bin"
            if [ "$orig_ok" = 0 ]; then
                record ok "$name [$mode]" "（原文件可语法检查但无法链接，跳过运行对比）"
                continue
            fi
            if ! guarded_compile "$OUT/$name.$mode.link.log" -o "$fbin" "$out" ${LINKFLAGS[$name]:-}; then
                record fail "$name [$mode]" "混淆产物链接失败"
                continue
            fi
            ( timeout $RUNTIME_TIMEOUT "$fbin" </dev/null >"$OUT/$name.$mode.out" 2>/dev/null; echo $? >"$OUT/$name.$mode.rc" )
            orc="$(cat "$OUT/$name.orig.rc")"; frc="$(cat "$OUT/$name.$mode.rc")"
            if [ "$orc" -ge 128 ] 2>/dev/null; then
                # 基线程序死于信号（多为空输入下未初始化变量等 UB），
                # 崩溃是否复现取决于内存布局，不构成语义对比依据
                record ok "$name [$mode]" "（基线程序在测试输入下崩溃 rc=$orc，跳过运行对比）"
            elif [ "$orc" = 137 ] || [ "$frc" = 137 ]; then
                if [ "$orc" = "$frc" ]; then
                    record ok "$name [$mode]" "（双方均超时被杀，一致）"
                else
                    record fail "$name [$mode]" "运行超时不一致（orig=$orc obf=$frc）"
                fi
            elif [ "$orc" != "$frc" ]; then
                record fail "$name [$mode]" "退出码不一致（orig=$orc obf=$frc）"
            elif ! cmp -s "$OUT/$name.orig.out" "$OUT/$name.$mode.out"; then
                record fail "$name [$mode]" "stdout 不一致"
            else
                record ok "$name [$mode]" "编译+运行一致"
            fi
        else
            record ok "$name [$mode]" "编译通过"
        fi
    done

    # ---- 版式压缩 / 宏混淆的保真：预处理结果（去空白与数字）必须与原文一致 ----
    # 这三个变体都不改名，可与原文件直接比对。压缩器曾经因为引号转义判断错误，
    # 把字符串里的 "/*" 当成注释开头而整段删除代码——这类“静默丢代码”在这里
    # 表现为指纹不等
    mkdir -p "$OUT/ppA" "$OUT/ppB"
    cp "$f" "$OUT/ppA/x.c"
    h_orig="$(pp_hash "$OUT/ppA")"
    for m in "layout:-no-rename -compress" "macro:-no-rename -macro-obf" "layout+macro:-no-rename -compress -macro-obf"; do
        mode="${m%%:*}"; flags="${m#*:}"
        if ! timeout -s KILL 120 "$TOOL" "$f" -o "$OUT/ppB/x.c" $flags -seed=42 -- $CCUR $STDUR \
            2>"$OUT/$name.fidelity.log"; then
            record fail "$name [$mode 保真]" "工具运行失败（见 $OUT/$name.fidelity.log）"
        elif [ "$(pp_hash "$OUT/ppB")" != "$h_orig" ]; then
            record fail "$name [$mode 保真]" "预处理结果与原文件不一致（文本级 pass 改变了程序内容）"
        else
            record ok "$name [$mode 保真]" "预处理结果与原文件一致"
        fi
    done
done

# reserve 保留名回归配置：文件名 -> 工具 flags；CHECK -> 必须存活在产物中的名字
declare -A RESERVE=(
    ["ModInt.cpp"]="-reserve=Mint"
    ["MaxFlow-HLPP.cpp"]="-reserve=MaxFlow"
    ["maxflow & BG match.cpp"]="-reserve=MaxFlow"
    ["listed_graph_directed_noweight.cpp"]="-reserve=edgs -reserve=edghead"
    ["listed_graph_directed_weight.cpp"]="-reserve=edgs -reserve=edghead"
    ["listed_graph_undirected_noweight.cpp"]="-reserve=edgs -reserve=edghead"
    ["listed_graph_undirected_weight.cpp"]="-reserve=edgs -reserve=edghead"
)
declare -A RESERVE_CHECK=(
    ["ModInt.cpp"]="Mint"
    ["MaxFlow-HLPP.cpp"]="MaxFlow push"
    ["maxflow & BG match.cpp"]="MaxFlow bfs"
    ["listed_graph_directed_noweight.cpp"]="edgs edghead"
    ["listed_graph_directed_weight.cpp"]="edgs edghead"
    ["listed_graph_undirected_noweight.cpp"]="edgs edghead"
    ["listed_graph_undirected_weight.cpp"]="edgs edghead"
)

# ---- reserve 保留名测试（名字存活 + 其余照常混淆 + 编译/运行一致）----
echo "== reserve 保留名测试"
CCUR="$CXX"; STDUR="$STD"
for name in "${!RESERVE[@]}"; do
    f="$ROOT/tests/$name"
    flags="${RESERVE[$name]}"
    checks="${RESERVE_CHECK[$name]:-}"
    out="$OUT/$name.reserve.cpp"
    if ! timeout -s KILL 120 "$TOOL" "$f" -o "$out" -compress -flatten -rt \
        $flags -seed=42 -- $CCUR $STDUR 2>"$OUT/$name.reserve.log"; then
        record fail "$name [reserve]" "工具运行失败（见 $OUT/$name.reserve.log）"
        continue
    fi
    bad=""
    for nm in $checks; do
        grep -qE "(^|[^A-Za-z0-9_])$nm([^A-Za-z0-9_]|$)" "$out" || bad="$bad $nm"
    done
    [ -n "$bad" ] && { record fail "$name [reserve]" "保留名未存活:$bad"; continue; }
    if ! guarded_compile "$OUT/$name.reserve.cc.log" -fsyntax-only "$out"; then
        record fail "$name [reserve]" "混淆产物编译失败（见 $OUT/$name.reserve.cc.log）"
        continue
    fi
    record ok "$name [reserve]" "保留名存活 + 编译通过"
done

# ---- 注释清除测试（默认自动去除；-keep-comments 可保留）----
echo "== 注释清除测试"
name="BigInt.cpp"; f="$ROOT/tests/$name"; out="$OUT/$name.nocomment.cpp"
if ! grep -q 'brief' "$f"; then
    record fail "$name [nocomment]" "测试前提不成立：源文件应包含注释"
elif ! timeout -s KILL 120 "$TOOL" "$f" -o "$out" -compress -seed=42 \
        -- $CCUR $STDUR 2>"$OUT/$name.nocomment.log"; then
    record fail "$name [nocomment]" "工具运行失败（见 $OUT/$name.nocomment.log）"
elif grep -q 'brief' "$out"; then
    record fail "$name [nocomment]" "注释残留：产物中仍能找到注释专属文本"
elif ! guarded_compile "$OUT/$name.nocomment.cc.log" -fsyntax-only "$out"; then
    record fail "$name [nocomment]" "产物编译失败（见 $OUT/$name.nocomment.cc.log）"
else
    record ok "$name [nocomment]" "注释已清除 + 编译通过"
fi

# ---- 压缩短名与头文件冲突 ----
# 压缩命名的短名可能撞上系统头文件里的声明：glibc 的 j0/j1/jn/y0/y1/yn 都是
# <math.h> 里的函数，主文件里出现一个 static 的同名函数就是“声明冲突”。
# 造一个名字足够多的 C 文件，让短名推进到两位字母区间，产物必须可编译、
# 可运行且结果正确
echo "== 压缩短名与头文件冲突"
CCUR="$CC"; STDUR="$CSTD"
g="$OUT/c_many_names.c"
{
    echo '#include <math.h>'
    echo '#include <stdio.h>'
    for i in $(seq 0 799); do
        printf 'static long name_%04d(long x) { return x + %d; }\n' "$i" "$i"
    done
    printf 'int main(void) {\n    long s = 0;\n'
    for i in $(seq 0 799); do printf '    s += name_%04d(s);\n' "$i"; done
    printf '    printf("%%ld\\n", s);\n    return 0;\n}\n'
} >"$g"
if ! guarded_compile "$OUT/c_many_names.base.log" -o "$OUT/c_many_names.orig.bin" "$g"; then
    record fail "c_many_names [compress]" "测试前提不成立：生成的大文件自身无法编译"
else
    "$OUT/c_many_names.orig.bin" >"$OUT/c_many_names.orig.out"
    for m in "compress:-compress" "compress+flatten:-compress -flatten"; do
        mode="${m%%:*}"; flags="${m#*:}"
        if ! timeout -s KILL 120 "$TOOL" "$g" -o "$OUT/c_many_names.$mode.c" $flags -seed=42 \
            -- $CCUR $STDUR 2>"$OUT/c_many_names.log"; then
            record fail "c_many_names [$mode]" "工具运行失败（见 $OUT/c_many_names.log）"
        elif ! guarded_compile "$OUT/c_many_names.cc.log" -o "$OUT/c_many_names.$mode.bin" "$OUT/c_many_names.$mode.c"; then
            record fail "c_many_names [$mode]" "产物编译失败（见 $OUT/c_many_names.cc.log）"
        elif ! timeout $RUNTIME_TIMEOUT "$OUT/c_many_names.$mode.bin" >"$OUT/c_many_names.$mode.out"; then
            record fail "c_many_names [$mode]" "产物运行失败/超时"
        elif ! cmp -s "$OUT/c_many_names.orig.out" "$OUT/c_many_names.$mode.out"; then
            record fail "c_many_names [$mode]" "运行结果与原程序不一致"
        else
            record ok "c_many_names [$mode]" "短名未与头文件声明冲突，运行一致"
        fi
    done
fi

# ---- checker.c 特判程序：场景对比 ----
# checker 是 `checker <infile> <outfile>` 形式的特判程序，上面的通用运行对比只跑到
# 用法提示。这里用一组固定场景（用法错误、输入不可读、输入/输出不合法、接受路径…）
# 驱动各模式的产物，stdout / stderr / 退出码必须与原程序一致。
# 每个场景输出一行 "<rc> | <stdout> | <stderr>"
echo "== checker.c 特判程序场景对比"
name="checker.c"; obin="$OUT/$name.orig.bin"
if [ ! -x "$obin" ]; then
    record fail "$name [场景]" "测试前提不成立：原程序未成功链接"
else
    cw="$OUT/checker_cases"; mkdir -p "$cw"
    run_cases() { # <binary>  → 逐场景输出一行
        local bin="$1"
        c() { # <in 内容|MISSING> <out 内容> [额外参数...]
            local inf="$cw/in.txt"
            if [ "$1" = MISSING ]; then inf="$cw/does-not-exist.txt"; else printf '%b' "$1" >"$inf"; fi
            printf '%b' "$2" >"$cw/out.txt"; shift 2
            ( cd "$cw" && timeout $RUNTIME_TIMEOUT "$bin" "$inf" "$cw/out.txt" "$@" </dev/null >o.txt 2>e.txt; echo "$? | $(tr '\n' ' ' <o.txt) | $(tr '\n' ' ' <e.txt)" )
        }
        c '' ''
        c '1 1\n' '1 1\n1\n' ignored
        c MISSING ''
        c '1 2\n3\n' '6\n'
        c '10 3\n' '5\n'
        c '3 2\n' '1\n'
        c '1 1\n' '1 1\n'
        c '1 1\n' '1 1\n1\n'
        c '1 1\n' '1 1\n2\n'
        c '1 2\n' '2 1\n1\n2\n'
        c '1 2\n' '1 2\n1\n2\n'
        c '' ''
        c '6 2\n' '0 0\n'
        c '6 3\n' '0 0\n'
        c 'abc def\n' '1\n'
        c '  1   1  \n' '  1 1 \n1\n'
        c '1 1\n' '1 1\r\n1\r\n'
    }
    run_cases "$obin" >"$cw/expect.txt"
    for m in "${C_MODES[@]}"; do
        mode="${m%%:*}"; fbin="$OUT/$name.$mode.bin"
        if [ ! -x "$fbin" ]; then
            record fail "$name [$mode 场景]" "产物未成功链接，无法做场景对比"
        elif run_cases "$fbin" >"$cw/got.txt" && cmp -s "$cw/expect.txt" "$cw/got.txt"; then
            record ok "$name [$mode 场景]" "$(wc -l <"$cw/expect.txt") 个场景的 stdout/stderr/退出码一致"
        else
            record fail "$name [$mode 场景]" "场景结果与原程序不一致（见 $cw/expect.txt 与 got.txt）"
        fi
    done
fi

# ---- checker.c 内嵌 QuickJS 引擎的差分测试 ----
# checker.c 是单文件 QuickJS-NG 应用，内嵌完整的 JS 引擎，但它的 main 只执行内嵌
# 字节码，上面的场景覆盖不到引擎本体。把 main 换成“执行命令行指定的 JS 文件”，
# 得到一个可任意驱动的解释器；原版与扁平化产物各跑一遍 checker_engine_tests.js
# （解析器、正则、数值格式化、GC、异步等 190 余个用例），输出必须逐字节一致。
# 替换方式：截到原 main 之前，接上下面的驱动——文件前半是 CRLF、驱动是 LF，
# 顺带覆盖混合行尾
echo "== checker.c 引擎差分测试"
name="checker.c"
# 文件中段 #ifdef TEST 里还有一个测试用的 main，真正的 main 是最后一个
main_ln="$(grep -n '^int main(' "$ROOT/tests/$name" | tail -1 | cut -d: -f1)"
if [ -z "$main_ln" ] || [ ! -f "$ROOT/tests/checker_engine_tests.js" ]; then
    record fail "$name [引擎]" "测试前提不成立：找不到 main 或 checker_engine_tests.js"
else
    runner="$OUT/checker_runner.c"
    {
        head -n $((main_ln - 1)) "$ROOT/tests/$name"
        cat <<'DRIVER'
int main(int argc, char **argv)
{
  int r;
  JSRuntime *rt;
  JSContext *ctx;
  r = 0;
  rt = JS_NewRuntime();
  js_std_set_worker_new_context_func(JS_NewCustomContext);
  js_std_init_handlers(rt);
  JS_SetModuleLoaderFunc2(rt, NULL, js_module_loader, js_module_check_attributes, NULL);
  ctx = JS_NewCustomContext(rt);
  js_std_add_helpers(ctx, argc, argv);
  /* test driver: evaluate the script given on the command line */
  {
    size_t buf_len;
    uint8_t *buf;
    JSValue res;
    if (argc < 2) {
      fprintf(stderr, "usage: %s <script.js>\n", argv[0]);
      js_std_free_handlers(rt);
      JS_FreeContext(ctx);
      JS_FreeRuntime(rt);
      return 2;
    }
    buf = js_load_file(ctx, &buf_len, argv[1]);
    if (!buf) {
      fprintf(stderr, "cannot read %s\n", argv[1]);
      js_std_free_handlers(rt);
      JS_FreeContext(ctx);
      JS_FreeRuntime(rt);
      return 1;
    }
    res = JS_Eval(ctx, (const char *)buf, buf_len, argv[1], JS_EVAL_TYPE_GLOBAL);
    js_free(ctx, buf);
    if (JS_IsException(res))
      js_std_dump_error(ctx);
    JS_FreeValue(ctx, res);
  }
  r = js_std_loop(ctx);
  if (r) {
    js_std_dump_error(ctx);
  }
  js_std_free_handlers(rt);
  JS_FreeContext(ctx);
  JS_FreeRuntime(rt);
  return r ? 1 : 0;
}
DRIVER
    } >"$runner"
    runner_ok=0
    # 驱动替换处的行号随版式变化，比较时把 (行:列) 抹掉
    norm() { sed -E 's/:[0-9]+:[0-9]+\)/:L:C)/g'; }
    if ! guarded_compile "$OUT/checker_runner.base.log" -o "$OUT/checker_runner.orig.bin" "$runner" ${LINKFLAGS[$name]:-}; then
        record fail "$name [引擎]" "测试前提不成立：原版运行器无法编译（见 $OUT/checker_runner.base.log）"
    elif ! timeout 120 "$OUT/checker_runner.orig.bin" "$ROOT/tests/checker_engine_tests.js" \
            >"$OUT/checker_runner.orig.out" 2>"$OUT/checker_runner.orig.err"; then
        record fail "$name [引擎]" "测试前提不成立：原版运行器执行测试脚本失败（见 $OUT/checker_runner.orig.err）"
    else
        runner_ok=1
    fi
    if [ "$runner_ok" = 1 ]; then
        for m in "flatten:-flatten" "compress+flatten:-compress -flatten" "compress+flatten+opaque:-compress -flatten -opaque"; do
            mode="${m%%:*}"; flags="${m#*:}"
            if ! timeout -s KILL 300 "$TOOL" "$runner" -o "$OUT/checker_runner.$mode.c" $flags \
                -- $CCUR $STDUR 2>"$OUT/checker_runner.$mode.log"; then
                record fail "$name [引擎 $mode]" "工具运行失败（见 $OUT/checker_runner.$mode.log）"
            elif ! guarded_compile "$OUT/checker_runner.cc.log" -o "$OUT/checker_runner.$mode.bin" "$OUT/checker_runner.$mode.c" ${LINKFLAGS[$name]:-}; then
                record fail "$name [引擎 $mode]" "产物编译失败（见 $OUT/checker_runner.cc.log）"
            elif ! timeout 120 "$OUT/checker_runner.$mode.bin" "$ROOT/tests/checker_engine_tests.js" \
                    >"$OUT/checker_runner.$mode.out" 2>"$OUT/checker_runner.$mode.err"; then
                record fail "$name [引擎 $mode]" "产物执行测试脚本时崩溃/退出码非零（见 $OUT/checker_runner.$mode.err）"
            elif ! cmp -s <(norm <"$OUT/checker_runner.orig.out") <(norm <"$OUT/checker_runner.$mode.out") \
                 || ! cmp -s <(norm <"$OUT/checker_runner.orig.err") <(norm <"$OUT/checker_runner.$mode.err"); then
                record fail "$name [引擎 $mode]" "引擎行为与原版不一致（diff $OUT/checker_runner.orig.out $OUT/checker_runner.$mode.out）"
            else
                record ok "$name [引擎 $mode]" "$(wc -l <"$OUT/checker_runner.$mode.out") 行输出与原版逐字节一致"
            fi
        done
    fi
fi

echo
echo "===== 结果: PASS=$pass FAIL=$fail ====="
if [ "$fail" -gt 0 ]; then
    printf '  - %s\n' "${FAILED[@]}"
    exit 1
fi
exit 0
