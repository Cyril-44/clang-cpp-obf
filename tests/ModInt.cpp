#include <bits/stdc++.h>
template <typename T>
constexpr inline T modInv(T x, T y) { // x * u ≡ 1 (mod y)
    assert(x != 0);
    T u = 0, v = 1, a = x, m = y, t;
    while (a != 0) {
        t = m / a;
        std::swap(a, m -= t * a);
        std::swap(u -= t * v, v);
    }
    assert(m == 1);
    return u;
}

template<class Mod, typename Mod::value_type Default = 0>
requires std::integral<typename Mod::value_type>
class ModBase {
    using Int = typename Mod::value_type;
    Int value;
    template<typename T> constexpr Int normalize(T x) {
        if constexpr (std::is_unsigned_v<T>)
            return x < T(mod()) ? x : x % T(mod());
        else {
            Int res = -mod() < x && x < mod() ? x : x % mod();
            return (res < 0 ? res + mod() : res);
        }
    }
public:
    static constexpr Int mod() { return Mod::value; }
    constexpr ModBase() : value(Default) {}
    template<typename T> constexpr ModBase(const T& rv) { value = normalize(rv); }
    template<typename T> explicit constexpr operator T() const { return static_cast<T>(value); }
    constexpr Int operator()() const { return value; }
    constexpr ModBase& operator+=(const ModBase& rv) {
        if ((value += rv.value) >= mod()) value -= mod();
        return *this;
    }
    constexpr ModBase& operator-=(const ModBase& rv) {
        if ((value -= rv.value) < 0) value += mod();
        return *this;
    }
    constexpr ModBase& operator*=(const ModBase& rv) {
        if constexpr (std::is_same_v<Int, int>)
            value = normalize((uint64_t)value * rv.value);
        else if constexpr (std::is_same_v<Int, int64_t>)
            value = normalize((unsigned __int128)value * rv.value);
        else 
            value = normalize(value * rv.value);
        return *this;
    }
    constexpr ModBase& operator/=(const ModBase& rv) { return *this *= ModBase(modInv(rv.value, mod())); }
    template<std::integral T> constexpr ModBase& operator^=(T n) {
        if (n < 0) assert(value != 0), value = 1 / value, n = -n;
        ModBase tmp = *this;
        for (*this = 1; n; n >>= 1) {
            if (n & 1) *this *= tmp;
            tmp *= tmp;
        }
        return *this;
    }
    constexpr ModBase operator-() const { return ModBase(-value); }
    constexpr ModBase& operator++() { return *this += 1; }
    constexpr ModBase& operator--() { return *this -= 1; }
    constexpr ModBase operator++(int) { ModBase tmp = *this; ++*this; return tmp; }
    constexpr ModBase operator--(int) { ModBase tmp = *this; --*this; return tmp; }
    constexpr bool operator!() const { return !value; }
    constexpr friend ModBase operator+(ModBase lv, const ModBase& rv) { return lv += rv; }
    constexpr friend ModBase operator-(ModBase lv, const ModBase& rv) { return lv -= rv; }
    constexpr friend ModBase operator*(ModBase lv, const ModBase& rv) { return lv *= rv; }
    constexpr friend ModBase operator/(ModBase lv, const ModBase& rv) { return lv /= rv; }
    constexpr friend bool operator==(ModBase lv, const ModBase& rv) { return lv.value == rv.value; }
    constexpr friend bool operator!=(ModBase lv, const ModBase& rv) { return lv.value != rv.value; }
    template<std::integral T> constexpr friend ModBase operator^(ModBase lv, const T rv) { return lv ^= rv; }
    template<typename IStream> friend IStream& operator>>(IStream& is, ModBase& lv) {
        is >> lv.value;
        lv.value = lv.normalize(lv.value);
        return is;
    }
    template<typename OStream> friend OStream& operator<<(OStream& os, const ModBase& rv) { return os << rv.value; }
};
constexpr auto MOD = (int)1e9 + 7;
using Mint = ModBase<std::integral_constant<std::decay_t<decltype(MOD)>, MOD>>;
// struct Dynamic_ModInt { using value_type = int; static value_type value; };
// Dynamic_ModInt::value_type Dynamic_ModInt::value = 0;
// Dynamic_ModInt::value_type &Mod = Dynamic_ModInt::value;
// using Mint = ModInt<Dynamic_ModInt>;
Mint sqrt(Mint x) { return x ^ (-2); }
struct Fact {
    Fact(const int n) : aa(n+1, Mint(1)), ia(n+1), sz(n) {
        aa[0] = 1;
        for (int i = 1; i <= n; i++) aa[i] = aa[i-1] * i;
        ia[n] = Mint(1) / aa[n];
        for (int i = n; i >= 1; i--) ia[i-1] = ia[i] * i;
    }
    Mint C(int n, int m) const {
        if (n < 0 || m < 0 || n < m) [[unlikely]] return 0;
        if (n > sz) [[unlikely]] throw std::out_of_range("Expected n < " + std::to_string(sz) + ", but found n = " + std::to_string(n) + ".");
        return aa[n] * ia[m] * ia[n - m];
    }
    Mint A(int n, int m) const {
        if (n < 0 || m < 0 || n < m) [[unlikely]] return 0;
        if (n > sz) [[unlikely]] throw std::out_of_range("Expected n < " + std::to_string(sz) + ", but found n = " + std::to_string(n) + ".");
        return aa[n] * ia[n - m];
    }
    Mint F(int n) const {
        if (n < 0) [[unlikely]] return 0;
        if (n > sz) [[unlikely]] throw std::out_of_range("Expected n < " + std::to_string(sz) + ", but found n = " + std::to_string(n) + ".");
        return aa[n];
    }
private:
    std::vector<Mint> aa, ia;
    const int sz;
};
Fact F(10000);
constexpr Mint M1 = 1, M2 = 3, M3 = M1 + M2, M4 = M3 / M2, M5 = M4 ^ 4, M6 = M5 * M2, M7 = 4/M3, M8 = 9*M6, M9 = M8/1145154*M6 ^ 4;
constexpr auto F1 = !M1;
int main() {
    1 + Mint(1);
    Mint abc = 1;
    abc += 2;
    sqrt(Mint(5));
    M9();
    return 0;
}