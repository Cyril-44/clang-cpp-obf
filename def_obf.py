#!/usr/bin/env python3
# obfuscate.py — token->nested-macro obfuscator for C/C++
# Usage: python obfuscate.py input.c out_obf.c out_map.json
import sys, json, io

# Config
SEG_SIZE = 2000           # 叶 token 数 / 段（可调整）
PREFIX = "OBFZ"           # 宏名前缀
MAX_MACRO_LINE = 1800     # 每个宏定义的最大字符数，超过则拆分
KEEP_COMMENTS = False     # 是否把注释当作 token 并保留

# Token types
TT_OTHER = 0
TT_IDENT  = 1
TT_NUM    = 2
TT_STR    = 3
TT_CHAR   = 4
TT_COMMENT = 5
TT_PREPROCESS = 6
TT_SPACE = 7
TT_NL = 8

# Simple helper to decide when space is required between two tokens
def need_space(left, right):
    # left/right are token dicts with 'text' and 'tt'
    # Conservative rules:
    if left is None or right is None:
        return False
    # if either is newline, no extra space token
    if left['tt'] == TT_NL or right['tt'] == TT_NL:
        return False
    # ident/ident, ident/num, num/ident need space
    if left['tt'] in (TT_IDENT,TT_NUM) and right['tt'] in (TT_IDENT,TT_NUM):
        return True
    if left['tt'] == TT_IDENT and right['tt'] == TT_STR:
        return True
    # keyword/ident handled as ident
    # else conservative: no
    return False

# Stateful tokenizer to preserve whitespace/newlines and handle strings/comments
def tokenize_body(s):
    i = 0
    n = len(s)
    tokens = []
    def push(tt, text):
        tokens.append({'tt':tt, 'text':text})
    while i < n:
        c = s[i]
        # preprocessor line (only if at line start and starts with '#')
        if c == '#' and (i==0 or s[i-1]=='\n'):
            j = i
            while j < n and s[j] != '\n':
                j += 1
            push(TT_PREPROCESS, s[i:j])
            i = j
            continue
        # whitespace and newlines
        if c.isspace():
            if c == '\n':
                push(TT_NL, '\n')
                i += 1
            else:
                j = i
                while j < n and s[j].isspace() and s[j] != '\n':
                    j += 1
                push(TT_SPACE, s[i:j])
                i = j
            continue
        # comments
        if c == '/' and i+1 < n and s[i+1] == '/':
            j = i
            while j < n and s[j] != '\n':
                j += 1
            push(TT_COMMENT, s[i:j])
            i = j
            continue
        if c == '/' and i+1 < n and s[i+1] == '*':
            j = i+2
            while j+1 < n and not (s[j] == '*' and s[j+1] == '/'):
                j += 1
            j += 2 if j+1 < n else 0
            push(TT_COMMENT, s[i:j])
            i = j
            continue
        # string literal
        if c == '"':
            j = i+1
            while j < n:
                if s[j] == '\\':
                    j += 2
                elif s[j] == '"':
                    j += 1
                    break
                else:
                    j += 1
            push(TT_STR, s[i:j])
            i = j
            continue
        # char literal
        if c == '\'':
            j = i+1
            while j < n:
                if s[j] == '\\':
                    j += 2
                elif s[j] == '\'':
                    j += 1
                    break
                else:
                    j += 1
            push(TT_CHAR, s[i:j])
            i = j
            continue
        # identifiers or numbers or operators
        if c.isalpha() or c == '_':
            j = i+1
            while j < n and (s[j].isalnum() or s[j]=='_'):
                j += 1
            push(TT_IDENT, s[i:j])
            i = j
            continue
        if c.isdigit():
            j = i+1
            while j < n and (s[j].isdigit() or s[j] in '.eE+-'):
                j += 1
            push(TT_NUM, s[i:j])
            i = j
            continue
        # everything else, single-char operator/punct or multi-char common ops
        # check multi-char ops
        two = s[i:i+2]
        three = s[i:i+3]
        if three in ('<<=', '>>='):
            push(TT_OTHER, three); i += 3; continue
        if two in ('==','!=','<=','>=','->','++','--','&&','||','<<','>>','+=','-=','*=','/=','%=','&=','|=','^='):
            push(TT_OTHER, two); i += 2; continue
        # single char
        push(TT_OTHER, c); i += 1
    return tokens

# Build leaf tokens list by removing pure SPACE tokens but preserving NL
def build_leaf_tokens(tokens):
    out = []
    for t in tokens:
        if t['tt'] == TT_SPACE:
            continue
        if t['tt'] == TT_COMMENT and not KEEP_COMMENTS:
            continue
        out.append(t)
    # insert SP tokens when needed
    res = []
    prev = None
    for t in out:
        if prev is not None and need_space(prev, t):
            res.append({'tt':TT_OTHER, 'text':' ', 'is_space_token':True})
        res.append(t)
        prev = t
    return res

# escape macro body safely: backslashes must be doubled to survive macro parsing when used in definitions
def escape_for_macro_body(text):
    # We will put token text directly after macro name; to be safe, escape backslashes only.
    # Do not wrap in quotes; keep original token text.
    return text.replace('\\', '\\\\')

def write_macro_def(f, name, body):
    # ensure macro lines not exceed MAX_MACRO_LINE; split by inserting intermediate macros if needed
    if len(body) <= MAX_MACRO_LINE:
        f.write(f"#define {name} {body}\n")
        return [name]
    # split body into chunks by tokens (space-separated), create intermediate macros
    parts = body.split(' ')
    chunks = []
    cur = []
    cur_len = 0
    for p in parts:
        if cur_len + len(p) + 1 > MAX_MACRO_LINE and cur:
            chunks.append(' '.join(cur))
            cur = [p]; cur_len = len(p)+1
        else:
            cur.append(p); cur_len += len(p)+1
    if cur:
        chunks.append(' '.join(cur))
    # create intermediate names
    intermediate = []
    for i,ch in enumerate(chunks):
        inter = f"{name}_P{i}"
        f.write(f"#define {inter} {ch}\n")
        intermediate.append(inter)
    # define final macro as concatenation of parts
    final_body = ' '.join(intermediate)
    f.write(f"#define {name} {final_body}\n")
    return intermediate + [name]

def generate_obfuscated(in_path, out_c_path, out_map_path):
    src = open(in_path, 'r', encoding='utf-8').read()
    # split header lines (leading preprocessor lines) and body
    lines = src.splitlines(keepends=True)
    header_lines = []
    body = []
    header_phase = True
    for ln in lines:
        if header_phase and ln.lstrip().startswith('#'):
            header_lines.append(ln)
        else:
            header_phase = False
            body.append(ln)
    body_text = ''.join(body)
    tokens = tokenize_body(body_text)
    leafs = build_leaf_tokens(tokens)
    # produce segments
    segments = []
    for i in range(0, len(leafs), SEG_SIZE):
        segments.append(leafs[i:i+SEG_SIZE])
    mapping = {'segments':[], 'prefix':PREFIX}
    out = io.StringIO()
    # write header
    for ln in header_lines:
        out.write(ln)
    # for each segment, produce leaf macros and combine
    for si,seg in enumerate(segments):
        seg_map = {'segment':si, 'tokens':[]}
        # leaf macros
        leaf_names = []
        for ti,t in enumerate(seg):
            leaf_name = f"{PREFIX}_S{si}_L{ti}"
            body_text = escape_for_macro_body(t['text'])
            out.write(f"#define {leaf_name} {body_text}\n")
            seg_map['tokens'].append({'idx':ti, 'text':t['text'], 'macro':leaf_name})
            leaf_names.append(leaf_name)
        # combine leaves into chunk macros (branch factor b)
        # simple binary combining until one top per segment
        cur_names = leaf_names
        level = 0
        while len(cur_names) > 1:
            next_names = []
            chunk_size = 8  # tune: combine 8 names per macro
            for i in range(0, len(cur_names), chunk_size):
                chunk = cur_names[i:i+chunk_size]
                cname = f"{PREFIX}_S{si}_C{level}_{i//chunk_size}"
                body = ' '.join(chunk)
                write_macro_def(out, cname, body)
                next_names.append(cname)
            cur_names = next_names
            level += 1
        top_name = cur_names[0] if cur_names else (f"{PREFIX}_S{si}_EMPTY")
        # segment top macro
        seg_top_name = f"{PREFIX}_SEG_{si}"
        out.write(f"#define {seg_top_name} {top_name}\n")
        mapping['segments'].append({'segment':si, 'top_macro':seg_top_name, 'count':len(seg)})
    # write final TOP expansion
    out.write("\n/* Expand segments */\n")
    for si in range(len(segments)):
        out.write(f"{PREFIX}_SEG_{si}\n")
    # write to files
    with open(out_c_path, 'w', encoding='utf-8') as f:
        f.write(out.getvalue())
    with open(out_map_path, 'w', encoding='utf-8') as f:
        json.dump(mapping, f, indent=2, ensure_ascii=False)
    print("Wrote:", out_c_path, out_map_path)

if __name__ == "__main__":
    if len(sys.argv) != 4:
        print("usage: python obfuscate.py in.c out_obf.c out_map.json")
        sys.exit(1)
    generate_obfuscated(sys.argv[1], sys.argv[2], sys.argv[3])
