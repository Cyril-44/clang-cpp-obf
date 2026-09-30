#include <bits/stdc++.h>
constexpr int N = 205;
template<typename Tp>
struct MaxFlow {
    MaxFlow(int n_, int s, int t) : g(new EdgeList[n_+1]), dis(new int[n_+1]), head(new int[n_+1]), que(new int[n_+1]), n(n_), S(s), T(t), modified(false), maxflow() {}
    ~MaxFlow() { delete[] g; delete[] dis; delete[] head; delete[] que; }
    inline void addedg(int fr, int to, Tp c) {
        modified = true;
        g[fr].emplace_back(to, c, (int)g[to].size());
        g[to].emplace_back(fr, Tp(0), (int)g[fr].size() - 1);
    }
    inline bool bfs() {
        modified = true;
        memset(dis, 0, sizeof(int) * (n+1));
        memset(head, 0, sizeof(int) * (n+1));
        int hd = 0, tl = 0;
        que[tl++] = S; dis[S] = 1;
        while (hd ^ tl) {
            int u = que[hd++];
            for (const auto &[v, cap, bak] : g[u])
                if (cap && !dis[v]) {
                    dis[v] = dis[u] + 1;
                    if (v == T) return true;
                    que[tl++] = v;
                }
        }
        return false;
    }
    Tp dfs(int u, Tp infl) {
        if (u == T) return infl;
        Tp outfl{};
        for (int &i = head[u]; i ^ (int)(g[u].size()); i++) {
            auto &[v, cap, bak] = g[u][i];
            if (cap && dis[v] == dis[u] + 1) {
                Tp fl = dfs(v, std::min(cap, infl));
                if (fl) {
                    infl -= fl, outfl += fl;
                    cap -= fl, std::get<1>(g[v][bak]) += fl;
                    if (!infl) return outfl;
                } else dis[v] = 0; // 当前点优化
            }
        }
        return outfl;
    }
    inline Tp operator()() {
        if (!modified) return maxflow;
        while (bfs()) maxflow += dfs(S, std::numeric_limits<Tp>::max());
        return maxflow;
    }
    using EdgeList = std::vector<std::tuple<int, Tp, int>>;
    EdgeList *g;
    int *dis, *head, *que, n, S, T;
    bool modified;
    Tp maxflow;
};

template<std::signed_integral TCap, std::signed_integral TCost = TCap, class Comp = std::less<TCost>>
requires std::predicate<Comp, TCost, TCost>
struct MCostFlow {
    constexpr static TCost InitVal = Comp{}(TCost(0), TCost(1)) ? std::numeric_limits<TCost>::max() : std::numeric_limits<TCost>::min();
    MCostFlow(int n, int s, int t) : g(n+1), dis(n+1), head(n+1), vis(n+1), maxflow(), mcost(), n(n), S(s), T(t) {}
    inline bool spfa() {
        dis.assign(n+1, InitVal);
        head.assign(n+1, 0);
        vis.assign(n+1, false);
        dis[S] = 0;
        for (std::queue<int> q({S}); !q.empty(); q.pop()) {
            int u = q.front();
            vis[u] = false;
            for (const auto &[v, bak, cap, cost] : g[u])
                if (cap && Comp{}(dis[u] + cost, dis[v])) {
                    dis[v] = dis[u] + cost;
                    if (!vis[v]) q.push(v), vis[v] = true;
                }
        }
        return dis[T] != InitVal;
    }
    inline void add(int fr, int to, TCap cap, TCost cost) {
        g[fr].emplace_back(to, g[to].size(), cap, cost);
        g[to].emplace_back(fr, g[fr].size() - 1, 0, -cost);
    }
    TCap dfs(int u, TCap infl) {
        vis[u] = true;
        if (u == T) return infl;
        TCap outfl = 0;
        for (int &i = head[u]; i < (int)g[u].size(); i++) {
            auto &[v, bak, cap, cost] = g[u][i];
            if (cap && !vis[v] && dis[v] == dis[u] + cost) {
                TCap fl = dfs(v, std::min(infl, cap));
                if (fl) {
                    mcost += fl * cost;
                    infl -= fl, outfl += fl;
                    cap -= fl, std::get<2>(g[v][bak]) += fl;
                    if (!infl) break;
                } else dis[v] = InitVal;
            }
        }
        return outfl;
    }
    inline std::pair<TCap, TCost> operator()() {
        while (spfa()) { // ssp 写法
            do vis.assign(n+1, false), maxflow += dfs(S, std::numeric_limits<TCap>::max());
            while (vis[T]);
        }
        return {maxflow, mcost};
    }
    std::vector<std::vector<std::tuple<int,int,TCap, TCost>>> g;
    std::vector<TCost> dis;
    std::vector<int> head;
    std::vector<bool> vis;
    TCap maxflow;
    TCost mcost;
    int n, S, T;
};



template<typename Tp>
struct MCapFlow {
    MCapFlow(int n_) : mf(n_+2, n_+1, n_+2), deg(new int[n_+1]()), n(n_), S(n_+1), T(n_+2), aval() {}
    inline int addedg(int fr, int to, Tp lwr, Tp upr) {
        mf.addedg(fr, to, upr - lwr);
        deg[fr] += lwr, deg[to] -= lwr;
        return (int)mf.g[to].size() - 1;
    }
    Tp getaval() {
        if (aval) return aval;
        for (int i = 0; i <= n; i++) // 如果 in-out > 0 连 T 否则连 S
            if (deg[i] > 0) mf.addedg(i, T, deg[i]);
            else if (deg[i] < 0) mf.addedg(S, i, -deg[i]);
        aval = mf();
        for (const auto &[v, cap, bak] : mf.g[S])
            if (cap) return -1; // S 到 x 必须满流
        return aval;
    }
    Tp getaval(int s, int t) {
        if (aval) return aval;
        int id = addedg(t, s, static_cast<Tp>(0), std::numeric_limits<Tp>::max()); // 加入 t-->s [0, +oo] 的边
        if (getaval() == -1) return -1;
        return std::get<1>(mf.g[s][id]); // 此时的可行流是 t-->s 的流量
    }
    Tp getmx(int s, int t) {
        if (aval) throw;
        addedg(t, s, static_cast<Tp>(0), std::numeric_limits<Tp>::max()); // 加入 t-->s [0, +oo] 的边
        int idt = (int)mf.g[t].size() - 1, ids = (int)mf.g[s].size() - 1;
        if (getaval() == -1) return -1;
        int res = std::get<1>(mf.g[s][ids]); // 此时的可行流是 t-->s 的流量
        std::get<1>(mf.g[t][idt]) = std::get<1>(mf.g[s][ids]) = static_cast<Tp>(0); // 删除 t-->s的边
        mf.S = s, mf.T = t, mf.maxflow = 0, mf.modified = true;
        return res + mf();
    }
    Tp getmn(int s, int t) {
        if (aval) throw;
        addedg(t, s, static_cast<Tp>(0), std::numeric_limits<Tp>::max()); // 加入 t-->s [0, +oo] 的边
        int idt = (int)mf.g[t].size() - 1, ids = (int)mf.g[s].size() - 1;
        if (getaval() == -1) return -1;
        int res = std::get<1>(mf.g[s][ids]); // 此时的可行流是 t-->s 的流量
        std::get<1>(mf.g[t][idt]) = std::get<1>(mf.g[s][ids]) = static_cast<Tp>(0); // 删除 t-->s的边
        mf.S = t, mf.T = s, mf.maxflow = 0, mf.modified = true;
        return res - mf();
    }
    MaxFlow<Tp> mf;
    std::vector<std::tuple<int,int,Tp,Tp>> edgs;
    Tp *deg; // deg 记录的是 入lwr - 出lwr
    int n, S, T;
    Tp aval;
    MaxFlow<Tp>* operator->() { return &mf; }
};



namespace Binary {
std::vector<int> edgs[N << 1];
bool vis[N << 1];
void dfs(int u) {
    if (vis[u]) return;
    for (int v : edgs[u]) dfs(v);
}
inline std::vector<int> mincover(MaxFlow<int> &mf, int n, int m) {
    int S = n+1, T = n+2;
    for (int i = 1; i <= n; i++)
        if (i <= m) mf.addedg(S, i, 1);
        else mf.addedg(i, T, 1);
    std::vector<int> ans;
    ans.reserve(n - mf());
    std::set<int> st;
    for (int i = m+1; i <= n; i++) st.insert(i);
    for (int u = 1; u <= m; u++)
        for (const auto &[v, cap, bak] : mf.g[u])
            if (cap == 0) {
                edgs[u].push_back(v);
            } else {
                edgs[v].push_back(u);
                st.erase(v);
            }
    for (int i : st) dfs(i);
    for (int i = 1; i <= m; i++)
        if (vis[i]) ans.push_back(i);
    for (int i = m+1; i <= n; i++)
        if (!vis[i]) ans.push_back(i);
    return ans;
}
inline std::vector<int> maxstandalone(MaxFlow<int> &mf, int n, int m) {
    auto anstmp = std::move(mincover(mf, n, m));
    std::vector<int> ans;
    auto ptr = anstmp.begin();
    for (int i = 1; i <= n; i++)
        if (ptr != anstmp.end() && *ptr == i) ++ptr;
        else ans.push_back(i);
    return ans;
}
inline std::vector<std::pair<int,int>> match(MaxFlow<int> &mf, int n, int m) {
    int S = n+1, T = n+2;
    for (int i = 1; i <= n; i++)
        if (i <= m) mf.addedg(S, i, 1);
        else mf.addedg(i, T, 1);
    std::vector<std::pair<int,int>> ans;
    ans.reserve(n - mf());
    for (int u = 1; u <= m; u++)
        for (const auto &[v, cap, bak] : mf.g[u])
            if (cap == 0) ans.emplace_back(u, v);
    return ans;
}
}