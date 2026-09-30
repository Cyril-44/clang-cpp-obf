#include <bits/stdc++.h>
constexpr int N = 500005, M = N;
struct EdgeNode {
    int to, nxt;
} edgs[M];
int edghead[N];
inline void initGraph(int n) { memset(edghead, 0, sizeof(int) * (n + 1)); }
inline void addedg(int fr, int to) {
    edgs[++edghead[0]] = EdgeNode{to, edghead[fr]};
    edghead[fr] = edghead[0];
}
struct Edge {
    struct EdgeIter {
        int curr;
        inline EdgeIter &operator++() {
            curr = edgs[curr].nxt;
            return *this;
        }
        inline bool operator!=(EdgeIter rhs) const { return curr != rhs.curr; }
        inline int operator*() const { return edgs[curr].to; }
    };
    int start;
    inline EdgeIter begin() { return EdgeIter{start}; }
    inline EdgeIter end() { return EdgeIter{0}; }
    inline Edge(int u) : start(edghead[u]) {}
};
std::vector<std::vector<int>> sccs;
int bl2[N], scctot;
namespace Tarjan {
unsigned dfn[N], low[N], ts;
int sta[N];
bool insta[N];
void dfs(int u) {
    dfn[u] = low[u] = ++ts;
    sta[++sta[0]] = u, insta[u] = true;
    for (int v : Edge(u)) {
        if (!dfn[v])
            dfs(v), low[u] = std::min(low[u], low[v]);
        else if (insta[v])
            low[u] = std::min(low[u], dfn[v]);
    }
    if (dfn[u] == low[u]) {
        ++scctot;
        do {
            bl2[sta[sta[0]]] = scctot;
            insta[sta[sta[0]]] = false;
        } while (sta[sta[0]--] != u);
    }
}
inline void tarj(int n) {
    scctot = 0;
    memset(dfn, 0, sizeof(int) * (n + 1));
    for (int i = 1; i <= n; i++)
        if (!dfn[i])
            dfs(i);
    sccs.assign(scctot, {});
    for (int i = 1; i <= n; i++)
        sccs[bl2[i] - 1].push_back(i);
}
inline void fetchGraph(int n, std::vector<int> g[]) {
    for (int i = 1; i <= scctot; i++)
        g[i].clear();
    std::set<std::pair<int, int>> st;
    for (int i = 1; i <= n; i++)
        for (int j : Edge(i))
            if (bl2[i] ^ bl2[j])
                if (st.emplace(bl2[i], bl2[j]).second)
                    g[bl2[i]].emplace_back(bl2[j]);
}
} // namespace Tarjan
std::vector<int> ng[N];
int ind[N];
int main() {
    int n, m;
    scanf("%d%d", &n, &m);
    for (int u, v; m--;) {
        scanf("%d%d", &u, &v);
        ++u, ++v;
        addedg(u, v);
    }
    Tarjan::tarj(n);
    Tarjan::fetchGraph(n, ng);
    for (int i = 1; i <= scctot; i++)
        for (int j : ng[i])
            ++ind[j];
    std::queue<int> q;
    for (int i = 1; i <= scctot; i++)
        if (!ind[i])
            q.push(i);
    printf("%zu\n", sccs.size());
    while (!q.empty()) {
        int u = q.front(); q.pop();
        printf("%zu", sccs[u-1].size());
        for (int i : sccs[u-1]) printf(" %d", i-1);
        putchar('\n');
        for (int v : ng[u])
            if (--ind[v] == 0) q.push(v);
    }
    return 0;
}
