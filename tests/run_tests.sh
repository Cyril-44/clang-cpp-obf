#!/usr/bin/env bash
# tests/run_tests.sh — cppobf 回归测试
#
# 对 tests/ 下每个 .cpp 文件（.py 跳过）：
#   * 基线可编译  ⇒ 4 种模式的混淆产物必须可编译；
#     定义了 main 的还必须与原程序运行行为一致（stdout + 退出码）
#   * 基线不可编译（VSCode 片段模板、本身有错的文件）⇒ 工具必须同样
#     干净地失败（非零退出、不产出输出）——行为与基线一致才算通过
#
# 用法：tests/run_tests.sh [工具路径]   （默认 build/cppobf）
set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TOOL="${1:-$ROOT/build/cppobf}"
OUT="$ROOT/tests/.obf_out"
CXX=g++
STD=-std=c++20
RUNTIME_TIMEOUT=10

[ -x "$TOOL" ] || { echo "工具不存在: $TOOL（先 cmake --build build）"; exit 2; }
mkdir -p "$OUT"

# 模式: 名称:工具flags
MODES=(
    "random:-seed=42"
    "compress:-compress -seed=42"
    "compress+macro:-compress -macro-obf -seed=42"
    "macro:-no-rename -macro-obf -seed=42"
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
    if (cd "$ROOT/tests" && $CXX $STD -w -fsyntax-only "$name" 2>/dev/null); then
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
        if ! "$TOOL" "$f" -o "$out" $flags -- clang++ -std=c++20 \
            2>"$OUT/$name.$mode.log"; then
            record fail "$name [$mode]" "工具运行失败（见 $OUT/$name.$mode.log）"
            continue
        fi
        # 混淆产物必须可编译（-I tests 保证 quoted include 仍能找到）
        if ! (cd "$ROOT/tests" && $CXX $STD -w -fsyntax-only -I "$ROOT/tests" "$out" 2>"$OUT/$name.$mode.cc.log"); then
            record fail "$name [$mode]" "混淆产物编译失败（见 $OUT/$name.$mode.cc.log）"
            continue
        fi
        # 运行等价性
        if [ "$has_main" = 1 ]; then
            obin="$OUT/$name.orig.bin"; fbin="$OUT/$name.$mode.bin"
            if ! (cd "$ROOT/tests" && $CXX $STD -w -o "$obin" "$name" 2>/dev/null); then
                record ok "$name [$mode]" "（原文件可语法检查但无法链接，跳过运行对比）"
                continue
            fi
            if ! (cd "$ROOT/tests" && $CXX $STD -w -o "$fbin" -I "$ROOT/tests" "$out" 2>>"$OUT/$name.$mode.cc.log"); then
                record fail "$name [$mode]" "混淆产物链接失败"
                continue
            fi
            ( timeout $RUNTIME_TIMEOUT "$obin" </dev/null >"$OUT/$name.orig.out" 2>/dev/null; echo $? >"$OUT/$name.orig.rc" )
            ( timeout $RUNTIME_TIMEOUT "$fbin" </dev/null >"$OUT/$name.$mode.out" 2>/dev/null; echo $? >"$OUT/$name.$mode.rc" )
            orc="$(cat "$OUT/$name.orig.rc")"; frc="$(cat "$OUT/$name.$mode.rc")"
            if [ "$orc" = 124 ] || [ "$frc" = 124 ]; then
                if [ "$orc" = "$frc" ]; then
                    record ok "$name [$mode]" "（双方均超时，一致）"
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

echo
echo "===== 结果: PASS=$pass FAIL=$fail ====="
if [ "$fail" -gt 0 ]; then
    printf '  - %s\n' "${FAILED[@]}"
    exit 1
fi
exit 0
