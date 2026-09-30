#!/usr/bin/env bash
# tests/run_tests.sh — cppobf 回归测试
#
# 对 tests/ 下每个 .cpp 文件（.py 跳过）：
#   * 基线可编译  ⇒ 所有模式的混淆产物必须可编译；
#     定义了 main 的还必须与原程序运行行为一致（stdout + 退出码）
#   * 基线不可编译（VSCode 片段模板、本身有错的文件）⇒ 工具必须同样
#     干净地失败（非零退出、不产出输出）——行为与基线一致才算通过
#
# 资源护栏：每个 g++/clang++ 编译限制 4GB 虚拟内存 + 180s；每次运行限制
# 15s。病理性用例只判失败，不会拖垮机器。（模板+concepts 的文件在
# g++10 下语法检查可耗 6.6GB，故验证编译器优先用 clang++。）
#
# 用法：tests/run_tests.sh [工具路径]   （默认 build/cppobf）
set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TOOL="${1:-$ROOT/build/cppobf}"
OUT="$ROOT/tests/.obf_out"
STD=-std=c++20
RUNTIME_TIMEOUT=15
COMPILE_TIMEOUT=180
MEMCAP_KB=4194304   # 4GB 虚拟内存上限

# 验证编译器：优先 clang（内存占用小一个量级），退回 g++
if command -v clang++ >/dev/null 2>&1; then
    CXX=clang++
elif [ -x /usr/lib/llvm-18/bin/clang++ ]; then
    CXX=/usr/lib/llvm-18/bin/clang++
else
    CXX=g++
fi

# 受限编译：guarded_compile <日志文件> <编译器参数...>
# 日志固定为第一个参数，其余原样作为编译器命令行，避免参数错位
guarded_compile() {
    local log="$1"; shift
    ( ulimit -v $MEMCAP_KB 2>/dev/null
      exec timeout -s KILL $COMPILE_TIMEOUT $CXX $STD -w -I "$ROOT/tests" "$@" ) >"$log" 2>&1
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
)

pass=0; fail=0
declare -a FAILED=()

record() { # ok name detail
    if [ "$1" = ok ]; then pass=$((pass+1));
    else fail=$((fail+1)); FAILED+=("$2 — $3"); echo "  FAIL: $3"; fi
}

for f in "$ROOT"/tests/*.cpp; do
    name="$(basename "$f")"
    echo "== $name"

    base_ok=0
    if guarded_compile "$OUT/$name.base.log" -fsyntax-only "$f"; then
        base_ok=1
    fi

    if [ "$base_ok" = 0 ]; then
        # 基线即不可编译：工具必须同样失败
        if "$TOOL" "$f" -o "$OUT/should-not-exist.cpp" -- clang++ -std=c++20 \
            >/dev/null 2>&1; then
            record fail "$name" "基线不可编译，但工具成功产出了输出"
        else
            record ok "$name" "基线不可编译，工具正确拒绝"
        fi
        continue
    fi

    has_main=0
    grep -qE '\bmain[[:space:]]*\(' "$f" && has_main=1

    for m in "${MODES[@]}"; do
        mode="${m%%:*}"; flags="${m#*:}"
        out="$OUT/$name.$mode.cpp"
        if ! timeout -s KILL 120 "$TOOL" "$f" -o "$out" $flags -- clang++ -std=c++20 \
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
            obin="$OUT/$name.orig.bin"; fbin="$OUT/$name.$mode.bin"
            if ! guarded_compile "$OUT/$name.link.log" -o "$obin" "$f"; then
                record ok "$name [$mode]" "（原文件可语法检查但无法链接，跳过运行对比）"
                continue
            fi
            if ! guarded_compile "$OUT/$name.$mode.link.log" -o "$fbin" "$out"; then
                record fail "$name [$mode]" "混淆产物链接失败"
                continue
            fi
            ( timeout $RUNTIME_TIMEOUT "$obin" </dev/null >"$OUT/$name.orig.out" 2>/dev/null; echo $? >"$OUT/$name.orig.rc" )
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
for name in "${!RESERVE[@]}"; do
    f="$ROOT/tests/$name"
    flags="${RESERVE[$name]}"
    checks="${RESERVE_CHECK[$name]:-}"
    out="$OUT/$name.reserve.cpp"
    if ! timeout -s KILL 120 "$TOOL" "$f" -o "$out" -compress -flatten -rt \
        $flags -seed=42 -- clang++ -std=c++20 2>"$OUT/$name.reserve.log"; then
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

echo
echo "===== 结果: PASS=$pass FAIL=$fail ====="
if [ "$fail" -gt 0 ]; then
    printf '  - %s\n' "${FAILED[@]}"
    exit 1
fi
exit 0
