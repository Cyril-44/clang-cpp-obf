#include <bits/stdc++.h>
template<typename Tp>
struct MaxFlow {
    MaxFlow(int n_, int s, int t) : n(n_), S(s), T(t), level(), g(n_+1), height(n_+1), gap(n_+1), extraFlow(n_+1), h(n_+1) {} 
    inline void operator()(int fr, int to, Tp c) {
        g[fr].emplace_back(to, c, (int)g[to].size());
        g[to].emplace_back(fr, 0, (int)g[fr].size() - 1);
    }
    inline bool push(int u) { // 通过 height[u]=height[v]+1 的边推送超额流 (返回true表示推送完了，false没有推送完)
        for (auto &[v, cap, bak] : g[u])
            if (cap && (u == S && height[v] != 0x3f3f3f3f || height[u] == height[v] + 1)) {
                Tp fl = (u == S) ? cap : std::min(cap, extraFlow[u]);
                if (v != S && v != T && !extraFlow[v]) h[height[v]].push(v), level = std::max(level, height[v]); // GAP 优化
                extraFlow[u] -= fl, extraFlow[v] += fl, cap -= fl, std::get<1>(g[v][bak]) += fl;
                if (!extraFlow[u]) return false;
            }
        return true;
    }
    inline void relabel(int u) { // 通过残量网络中的临边重贴高度 height
        height[u] = 0x3f3f3f3f;
        for (const auto &[v, cap, bak] : g[u])
            if (cap) height[u] = std::min(height[u], height[v]);
        if (++height[u] < n) { // GAP优化：只处理 height < n 的节点
            h[height[u]].push(u);
            level = std::max(level, height[u]);
            ++gap[height[u]];
        }
    }
    inline bool bfs() {
        std::fill(height.begin(), height.end(), 0);
        std::queue<int> q;
        q.push(T), height[T] = 0; // 反向BFS，生成初始 height
        while (!q.empty()) {
            int u = q.front(); q.pop();
            for (const auto &[v, cap, bak] : g[u])
                if (std::get<1>(g[v][bak]) && height[v] > height[u] + 1)
                    height[v] = height[u] + 1, q.push(v);
        }
        return height[S] != 0x3f3f3f3f;
    }
    inline int selectHighest() { // 选出 height 最大的节点
        while (level >= 0 && h[level].empty()) --level;
        if (level < 0) return 0;
        int ret = h[level].top();
        h[level].pop();
        return ret;
    }
    inline Tp operator()() {
        if (extraFlow[T]) return extraFlow[T];
        if (!bfs()) return 0;
        for (int i = 0; i <= n; i++)
            if (height[i] != 0x3f3f3f3f) gap[height[i]]++;
        height[S] = n;
        push(S);
        for (int u; u = selectHighest(); )
            if (push(u)) { // 处理仍然溢出
                if (!--gap[height[u]])
                    for (int i = 0; i <= n; i++)
                        if (i != S && height[i] > height[u] && height[i] <= n)
                            height[i] = n + 1;
                relabel(u);
            }
        return extraFlow[T];
    }
    int n, S, T;
    int level;
    std::vector<std::vector<std::tuple<int,Tp,int>>> g;
    std::vector<int> height, gap;
    std::vector<Tp> extraFlow;
    std::vector<std::stack<int>> h;
};