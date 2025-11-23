#include <bits/stdc++.h>
using namespace std;
constexpr int N = 1000005;
#define ll long long
inline int read() {
    int a = 0, f = 1;
    char c = getchar();
    while (c > '9' || c < '0') {
        if (c == '-')
            f = -1;
        c = getchar();
    }
    while (c >= '0' && c <= '9') {
        a = a * 10 + c - '0';
        c = getchar();
    }
    return a * f;
}
int s1[N][11], s2[N][11], a[N];
ll ny[N], jc[N];
int mod = 998244353;
ll C(int a, int b) {
    if (a < b)
        return 0;
    return jc[a] * ny[b] % mod * ny[a - b] % mod;
}
ll ksm(ll a, int b) {
    ll s = 1;
    while (b) {
        if (b % 2)
            s = (s * 1ll * a) % mod;
        a = (a * 1ll * a) % mod, b /= 2;
    }
    return s;
}
int main() {
    // freopen("1.in","r",stdin);
    // freopen("1.out","w",stdout);
    string s;
    cin >> s;
    int n = s.size();
    jc[0] = jc[1] = 1, ny[0] = ny[1] = 1;
    for (int i = 2; i <= n; i++)
        jc[i] = jc[i - 1] * i % mod;
    ny[n] = ksm(jc[n], mod - 2);
    for (int i = n - 1; i >= 2; i--)
        ny[i] = ny[i + 1] * (i + 1) % mod;
    for (int i = 0; i < n; i++)
        a[i + 1] = s[i] - '0';
    for (int i = 1; i <= n; i++) {
        for (int j = 0; j <= 9; j++)
            s1[i][j] = s1[i - 1][j];
        s1[i][a[i]]++;
    }
    for (int i = n; i >= 1; i--) {
        for (int j = 0; j <= 9; j++)
            s2[i][j] = s2[i + 1][j];
        s2[i][a[i]]++;
    }
    ll ans = 0;
    for (int i = 1; i <= n; i++) {
        int x = s1[i - 1][a[i] - 1], y = s2[i][a[i]];
        if (x <= 0)
            continue;
        x--;
        ans += C(x + y, x), ans %= mod;
    }
    printf("%lld\n", ans);
    return 0;
}
