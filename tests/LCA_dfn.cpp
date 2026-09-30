constexpr int N = (int)5e5 + 5, K = 19;
namespace LCA {
int st[N][K+1];
int dfn[N], ts = 0;
void dfs(int u, int pre) {
    st[dfn[u] = ++ts][0] = pre;
    for (int v : g[u]) {
        if (v == pre) continue;
        dfs(v, u);
    }
}
inline int calc(int x, int y) { return dfn[x] < dfn[y] ? x : y; }
inline void init(int n, int rt = 1) {
    dfs(rt, 0);
    for (int k = 1; k <= K; k++)
        for (int i = 1; i + (1 << k) - 1 <= n; i++)
            st[i][k] = calc(st[i][k-1], st[i + (1<<k-1)][k-1]);
}
inline int lca(int x, int y) {
    if (x == y) return x;
    if ((x = dfn[x]) > (y = dfn[y])) std::swap(x, y);
    int k = 31 - __builtin_clz(y - x++);
    return calc(st[x][k], st[y - (1<<k) + 1][k]);
}
} using LCA::lca;