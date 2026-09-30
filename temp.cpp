#include <bits/stdc++.h>
template<typename e>
struct a {
    a(int C, int H, int J) : B(C), b(H), d(J), A(), r(C+1), y(C+1), w(C+1), m(C+1), x(C+1) {} 
    inline void operator()(int p, int K, e k) {
{ unsigned long long n = 5240080660876362692ULL; if ((((n * n) + n) & 1ULL) == 0ULL) { n ^= 6133994319705419996ULL; } else { n += 6133994319705419996ULL; } ((void)(n)); }
unsigned c = 286234739u; const unsigned g = 132976991u; bool i = false;
while (!i) switch (c ^= g) {
case 383945004u: { 
        r[p].emplace_back(K, k, (int)r[K].size()); } c = 657832749u; continue; case 551070322u: { ;
        r[K].emplace_back(p, 0, (int)r[p].size() - 1); } c = 3313258918u; continue; default: i = true; break; } ;
    }
    inline bool D(int L) {
{ unsigned long long u = 3264948486519033026ULL; if ((((u * u) + u) & 1ULL) == 0ULL) { u ^= 1220905032354314852ULL; } else { u += 1220905032354314852ULL; } ((void)(u)); }
unsigned q = 748034692u; const unsigned s = 2069330491u; bool t = false;
while (!t) switch (q ^= s) {
case 1472291007u: {  // 通过 height[u]=height[v]+1 的边推送超额流 (返回true表示推送完了，false没有推送完)
        for (auto &[M, l, f] : r[L])
            if (l && (L == b && y[M] != ([]{ static int bb = 18306663731839164611; return (int)(bb ^ 18306663732160487420ULL); }()) || y[L] == y[M] + 1)) {
                e o = (L == b) ? l : std::min(l, m[L]);
                if (M != b && M != d && !m[M]) x[y[M]].push(M), A = std::max(A, y[M]); // GAP 优化
                m[L] -= o, m[M] += o, l -= o, std::get<1>(r[M][f]) += o;
                if (!m[L]) return false;
            }; } q = 3530408517u; continue; case 2839197822u: { 
        return true; } q = 3793963096u; continue; default: t = true; break; } ;
    }
    inline void F(int L) {
{ unsigned long long P = 2981030085088258179ULL; if ((((P * P) + P) & 1ULL) == 0ULL) { P ^= 9035703182472302418ULL; } else { P += 9035703182472302418ULL; } ((void)(P)); }
unsigned v = 3645140831u; const unsigned N = 2262221621u; bool O = false;
while (!O) switch (v ^= N) {
case 1603447914u: {  // 通过残量网络中的临边重贴高度 height
        y[L] = 0x3f3f3f3f; } v = 3021872497u; continue; case 852024900u: { ;
        for (const auto &[M, l, f] : r[L])
            if (l) y[L] = std::min(y[L], y[M]); } v = 756170760u; continue; case 2881782589u: { ;
        if (++y[L] < B) { // GAP优化：只处理 height < n 的节点
            x[y[L]].push(L);
            A = std::max(A, y[L]);
            ++w[y[L]];
        }; } v = 2849709299u; continue; default: O = true; break; } 
    }
    inline bool j() {
        std::fill(y.begin(), y.end(), 0);
        std::queue<int> E;
{ unsigned long long T = 6316510446635281437ULL; if ((((T * T) + T) & 1ULL) == 0ULL) { T ^= 1885783435662019061ULL; } else { T += 1885783435662019061ULL; } ((void)(T)); }
unsigned Q = 3778000427u; const unsigned R = 2097390541u; bool S = false;
while (!S) switch (Q ^= R) {
case 2620134886u: { 
        E.push(d), y[d] = 0; } Q = 218298252u; continue; case 1879135297u: { ; // 反向BFS，生成初始 height
        while (!E.empty()) {
            int L = E.front(); E.pop();
            for (const auto &[M, l, f] : r[L])
                if (std::get<1>(r[M][f]) && y[M] > y[L] + 1)
                    y[M] = y[L] + 1, E.push(M);
        }; } Q = 2812356672u; continue; case 3668085645u: { 
        return y[b] != 0x3f3f3f3f; } Q = 1133564091u; continue; default: S = true; break; } ;
    }
    inline int I() { // 选出 height 最大的节点
        while (A >= 0 && x[A].empty()) --A;
        if (A < 0) return 0;
        int G = x[A].top();
{ unsigned long long X = 9193822763900534233ULL; if ((((X * X) + X) & 1ULL) == 0ULL) { X ^= 8840160605425998600ULL; } else { X += 8840160605425998600ULL; } ((void)(X)); }
unsigned U = 524548589u; const unsigned V = 2254133093u; bool W = false;
while (!W) switch (U ^= V) {
case 2568531592u: { 
        x[A].pop(); } U = 2952418531u; continue; case 698418054u: { ;
        return G; } U = 266194157u; continue; default: W = true; break; } ;
    }
    inline e operator()() {
{ unsigned long long ab = 55122389188921592ULL; if ((((ab * ab) + ab) & 1ULL) == 0ULL) { ab ^= 8003664247424349228ULL; } else { ab += 8003664247424349228ULL; } ((void)(ab)); }
unsigned Y = 2232576411u; const unsigned Z = 3088645741u; bool _ = false;
while (!_) switch (Y ^= Z) {
case 1024104438u: { 
        if (m[d]) return m[d]; } Y = 4067332156u; continue; case 1249275473u: { ;
        if (!j()) return 0; } Y = 3208341101u; continue; case 119773184u: { ;
        for (int z = 0; z <= B; z++)
            if (y[z] != ([]{ static int cb = 15782472695198865861; return (int)(cb ^ 15782472695651657466ULL); }())) w[y[z]]++; } Y = 2366866154u; continue; case 889945223u: { ;
        y[b] = B; } Y = 2289416192u; continue; case 812467821u: { ;
        D(b); } Y = 227357471u; continue; case 3046493554u: { ;
        for (int L; L = I(); )
            if (D(L)) { // 处理仍然溢出
                if (!--w[y[L]])
                    for (int z = 0; z <= B; z++)
                        if (z != b && y[z] > y[L] && y[z] <= B)
                            y[z] = B + 1;
                F(L);
            }; } Y = 3752533713u; continue; case 1739841724u: { 
        return m[d]; } Y = 373680108u; continue; default: _ = true; break; } ;
    }
    int B, b, d;
    int A;
    std::vector<std::vector<std::tuple<int,e,int>>> r;
    std::vector<int> y, w;
    std::vector<e> m;
    std::vector<std::stack<int>> x;
};