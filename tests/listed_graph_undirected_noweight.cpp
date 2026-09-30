#include <bits/stdc++.h>
constexpr int N = 500005, M = N << 1;
struct EdgeNode {
    int to, nxt;
} edgs[M];
int edghead[N]{1};
inline void initGraph(int n) {
    memset(edghead + 1, 0, sizeof(int) * n);
    edghead[0] = 1;
}
inline void add2edg(int fr, int to) {
    edgs[++edghead[0]] = EdgeNode {to, edghead[fr]};
    edghead[fr] = edghead[0];
    edgs[++edghead[0]] = EdgeNode {fr, edghead[to]};
    edghead[to] = edghead[0];
}
struct Edge {
    struct EdgeIter {
        int curr;
        inline EdgeIter& operator++() { curr = edgs[curr].nxt; return *this; }
        inline bool operator==(EdgeIter rhs) const { return curr == rhs.curr; }
        inline int operator*() const { return edgs[curr].to; }
    };
    int start;
    inline EdgeIter begin() { return EdgeIter{start}; }
    inline EdgeIter end() { return EdgeIter{0}; }
    inline Edge(int u) : start(edghead[u]) {}
};
int main() {
    return 0;
}