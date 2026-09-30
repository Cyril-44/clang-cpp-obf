#include <bits/stdc++.h>
constexpr int N = 500005, M = 4000005;
struct EdgeNode{int to,nxt;}edgs[M];int edghead[N]{1};inline void initGraph(int n){memset(edghead+1,0,sizeof(int)*n);edghead[0]=1;}inline void add2edg(int fr,int to){edgs[++edghead[0]]=EdgeNode{to,edghead[fr]};edghead[fr]=edghead[0];edgs[++edghead[0]]=EdgeNode{fr,edghead[to]};edghead[to]=edghead[0];}struct Edge{struct EdgeIter{int curr;inline EdgeIter&operator++(){curr=edgs[curr].nxt;return*this;}inline bool operator!=(EdgeIter rhs)const{return curr!=rhs.curr;}inline int operator*()const{return edgs[curr].to;}};int start;inline EdgeIter begin(){return EdgeIter{start};}inline EdgeIter end(){return EdgeIter{-1};}inline Edge(int u):start(edghead[u]){}};
class FastIS{static constexpr size_t P=1<<20;char r[P];char*u,*G;FILE*I;public:FastIS(FILE*Y=stdin):u(nullptr),G(nullptr),I(Y){}__attribute__((always_inline))inline int get(){return(u==G)&&(G=(u=r)+fread(r,1,P,I),u==G)?EOF:*u++;}template<typename A,typename=typename std::enable_if<std::is_arithmetic<A>::value>::type>FastIS&operator>>(A&j){if _GLIBCXX17_CONSTEXPR(std::is_same<A,bool>::value){int a=get();while(~a&&a!=(char)(48)&&a!=(char)(49))a=get();j=static_cast<bool>(a&1);}else if _GLIBCXX17_CONSTEXPR(std::is_same<A,char>::value){j=get();while(j==(char)(32)||j==(char)(13)||j==(char)(10))j=get();}else if _GLIBCXX17_CONSTEXPR(std::is_integral<A>::value){if _GLIBCXX17_CONSTEXPR(std::is_unsigned<A>::value){int a=get();while(~a&&(a<(char)(48)||a>(char)(57)))a=get();for(j=0;a>=(char)(48)&&a<=(char)(57);a=get())j=(j<<3)+(j<<1)+(a^(char)(48));}else{int a=get();bool m=false;while(~a&&(a<(char)(48)||a>(char)(57))&&(a^(char)(45)))a=get();if(a==(char)(45))a=get(),m=true;for(j=0;a>=(char)(48)&&a<=(char)(57);a=get())j=(j<<3)+(j<<1)+(a^(char)(48));j=m?-j:j;}}else if _GLIBCXX17_CONSTEXPR(std::is_floating_point<A>::value){int a=get();bool m=false;while(~a&&(a<(char)(48)||a>(char)(57))&&(a^(char)(45)))a=get();if(a==(char)(45))a=get(),m=true;using E=typename std::conditional<std::is_same<A,float>::value,std::uint32_t,std::uint64_t>::type;E Q;for(Q=0;a>=(char)(48)&&a<=(char)(57);a=get())Q=(Q<<3)+(Q<<1)+(a^(char)(48));j=Q;if(a==(char)(46)){E a=1;for(a=get();a>=(char)(48)&&a<=(char)(57);a=get())j+=(a^(char)(48))*(1./(a*=10));}if(m)j=-j;}else throw;return*this;}FastIS&operator>>(char*i){int a=get();while(~a&&(a==(char)(32)||a==(char)(13)||a==(char)(10)))a=get();while(~a&&(a!=(char)(32)&&a!=(char)(13)&&a!=(char)(10)))*i++=a,a=get();return*this;}}fin;class FastOS{static constexpr size_t P=1<<20;static constexpr double x=1e-9;char r[P],*b;const char*l;FILE*w;int o;public:FastOS(FILE*Y=stdout):b(r),l(r+P),w(Y),o(-1){setvbuf(w,nullptr,_IONBF,0);}~FastOS(){flush();fclose(w);}FastOS&flush(){fwrite(r,b-r,1,w);return*this;}FastOS&put(int g){*b++=g;if(b==l)fwrite(r,P,1,w),b=r;return*this;}FastOS&setprecision(const int&t){o=t;return*this;}template<typename a,typename=typename std::enable_if<std::is_arithmetic<a>::value>::type>FastOS&operator<<(a j){if _GLIBCXX17_CONSTEXPR(std::is_same<a,char>::value)put(j);else if _GLIBCXX17_CONSTEXPR(std::is_integral<a>::value){if _GLIBCXX17_CONSTEXPR(std::is_signed<a>::value)if(j<0)put((char)(45)),j=-j;static uint8_t i[40];i[*i=1]=j%10;while(j/=10)i[++(*i)]=j%10;while(*i)put(i[(*i)--]|(char)(48));}else if _GLIBCXX17_CONSTEXPR(std::is_floating_point<a>::value){if(j<0)put((char)(45)),j=-j;using E=typename std::conditional<std::is_same<a,float>::value,std::uint32_t,std::uint64_t>::type;E v=static_cast<E>(j);a B=j-v;*this<<v;if(o>0||(o<0&&B>x)){put((char)(46));for(int R=0;(o>0&&R<o)||(o<0&&B>x);R++){int X=static_cast<int>(B*=10);put(X|(char)(48));B-=X;}}}else throw;return*this;}FastOS&operator<<(char*i){while(*i)put(*i++);return*this;}}fout;
#ifdef MULTI_TEST_CASES
auto __read_extra_test_cases = [](int x=0){fin >> x; return x;}();
#endif

bool vcut[N], ecut[M];
std::vector<std::vector<int>> bccs;
std::vector<std::vector<int>> dccs;
namespace Tarjan {
    unsigned dfn[N], low[N], ts;
    int sta[N]; bool vis[N];
    int root;
    void dfs(int u, int fr) {
        dfn[u] = low[u] = ++ts;
        sta[++sta[0]] = u;
        int f = 0;
        std::vector<int> bcc;
        for (int i = edghead[u], v; i; i = edgs[i].nxt) {
            if ((i ^ fr) == 1) continue;
            v = edgs[i].to;
            if (!dfn[v]) {
                dfs(v, i), low[u] = std::min(low[u], low[v]);
                if (dfn[u] < low[v])
                    ecut[i] = ecut[i ^ 1] = true;
                if (dfn[u] <= low[v]) {
                    if (u != root || ++f >= 2) vcut[u] = true;
                    dccs.emplace_back();
                    do dccs.back().push_back(sta[sta[0]]);
                    while (sta[sta[0]--] != v);
                    dccs.back().push_back(u);
                }
            }
            else low[u] = std::min(low[u], dfn[v]);
        }
        if (u == root && !edghead[u])
            dccs.push_back({u}), --sta[0];
    }
    void dfs2(int u) {
        bccs.back().push_back(u);
        vis[u] = true;
        for (int i = edghead[u], v; i; i = edgs[i].nxt) {
            if (!ecut[i] && !vis[v = edgs[i].to])
                dfs2(v);
        }
    }
    inline void tarj(int n) {
        bccs.clear(), dccs.clear();
        memset(dfn, 0, sizeof(int) * (n+1));
        memset(vis, 0, n + 1);
        memset(vcut, 0, n + 1);
        memset(ecut, 0, edghead[0] + 1);
        for (int i = 1; i <= n; i++)
            if (!dfn[i]) root = i, dfs(i, 0);
        for (int i = 1; i <= n; i++)
            if (!vis[i]) bccs.emplace_back(), dfs2(i);
    }
}
int main() {
    int n, m;
    fin >> n >> m;
    for (int u, v; m--; ) {
        fin >> u >> v;
        if (u == v) continue;
        add2edg(u, v);
    }
    Tarjan::tarj(n);
    /* fout << bccs.size() << '\n';
    for (const auto &bcc : bccs) {
        fout << bcc.size();
        for (int i : bcc) fout << ' ' << i;
        fout << '\n';
    } */
    fout << dccs.size() << '\n';
    for (const auto &dcc : dccs) {
        fout << dcc.size();
        for (int i : dcc) fout << ' ' << i;
        fout << '\n';
    }
    return 0;
}
