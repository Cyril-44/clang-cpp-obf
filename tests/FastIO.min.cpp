#include <bits/stdc++.h>
#include <type_traits>
class FastIS {
    static constexpr size_t P = 1 << 20;
    char r[P];
    char *u, *G;
    FILE *I;
public:
    FastIS(FILE* Y = stdin) : u(nullptr), G(nullptr), I(Y) {}
    __attribute__((always_inline)) inline int get() {
        return (u == G) && (G = (u = r) + fread(r, 1, P, I), u == G) ? EOF : *u++;
    }
    template<typename A, typename = typename std::enable_if<std::is_arithmetic<A>::value>::type>
    FastIS& operator>>(A& j) {
        if _GLIBCXX17_CONSTEXPR (std::is_same<A, bool>::value) {
            int a = get();
            while (~a && a != (char)(48) && a != (char)(49)) a = get();
            j = static_cast<bool>(a & 1);
        } else if _GLIBCXX17_CONSTEXPR (std::is_same<A, char>::value) {
            j = get();
            while (j == (char)(32) || j == (char)(13) || j == (char)(10)) j = get();
        } else if _GLIBCXX17_CONSTEXPR (std::is_integral<A>::value) {
            if _GLIBCXX17_CONSTEXPR (std::is_unsigned<A>::value) {
                int a = get();
                while (~a && (a < (char)(48) || a > (char)(57))) a = get();
                for (j = 0; a >= (char)(48) && a <= (char)(57); a = get())
                    j = (j << 3) + (j << 1) + (a ^ (char)(48));
            } else {
                int a = get();
                bool m = false;
                while (~a && (a < (char)(48) || a > (char)(57)) && (a ^ (char)(45))) a = get();
                if (a == (char)(45)) a = get(), m = true;
                for (j = 0; a >= (char)(48) && a <= (char)(57); a = get())
                    j = (j << 3) + (j << 1) + (a ^ (char)(48));
                j = m ? -j : j;
            }
        } else if _GLIBCXX17_CONSTEXPR (std::is_floating_point<A>::value) {
            int a = get();
            bool m = false;
            while (~a && (a < (char)(48) || a > (char)(57)) && (a ^ (char)(45))) a = get();
            if (a == (char)(45)) a = get(), m = true;
            using E = typename std::conditional<std::is_same<A, float>::value, std::uint32_t, std::uint64_t>::type;
            E Q;
            for (Q = 0; a >= (char)(48) && a <= (char)(57); a = get())
                Q = (Q << 3) + (Q << 1) + (a ^ (char)(48));
            j = Q;
            if (a == (char)(46)) {
                E a = 1;
                for (a = get(); a >= (char)(48) && a <= (char)(57); a = get())
                    j += (a ^ (char)(48)) * (1. / (a *= 10));
            }
            if (m) j = -j;
        } else {
            throw;
        }
        return *this;
    }
    FastIS& operator>>(char *i) {
        int a = get();
        while (~a && (a == (char)(32) || a == (char)(13) || a == (char)(10))) a = get();
        while (~a && (a != (char)(32) && a != (char)(13) && a != (char)(10)))
            *i++ = a, a = get();
        return *this;
    }
} fin;
class FastOS {
    static constexpr size_t P = 1 << 20;
    static constexpr double x = 1e-9;
    char r[P], *b;
    const char *l;
    FILE *w;
    int o;
public:
    FastOS(FILE* Y = stdout) : b(r), l(r + P), w(Y), o(-1) { setvbuf(w, nullptr, _IONBF, 0); }
    ~FastOS() { flush(); fclose(w); }
    FastOS& flush() {
        fwrite(r, b - r, 1, w);
        return *this;
    }
    FastOS& put(int g) {
        *b++ = g;
        if (b == l) fwrite(r, P, 1, w), b = r;
        return *this;
    }
    FastOS& setprecision(const int& t) {
        o = t;
        return *this;
    }
    template<typename a, typename = typename std::enable_if<std::is_arithmetic<a>::value>::type>
    FastOS& operator<<(a j) {
        if _GLIBCXX17_CONSTEXPR (std::is_same<a, char>::value)
            put(j);
        else if _GLIBCXX17_CONSTEXPR (std::is_integral<a>::value) {
            if _GLIBCXX17_CONSTEXPR (std::is_signed<a>::value)
                if (j < 0) put((char)(45)), j = -j;
            static uint8_t i[40];
            i[*i = 1] = j % 10;
            while (j /= 10) i[++(*i)] = j % 10;
            while (*i) put(i[(*i)--] | (char)(48));
        }
        else if _GLIBCXX17_CONSTEXPR (std::is_floating_point<a>::value) {
            if (j < 0) put((char)(45)), j = -j;
            using E = typename std::conditional<std::is_same<a, float>::value, std::uint32_t, std::uint64_t>::type;
            E v = static_cast<E>(j);
            a B = j - v;
            *this << v;
            if (o > 0 || (o < 0 && B > x)) {
                put((char)(46));
                for (int R = 0; (o > 0 && R < o) || o < 0 && B > x; R++) {
                    int X = static_cast<int>(B *= 10);
                    put(X | (char)(48));
                    B -= X;
                }
            }
        } else {
            throw;
        }
        return *this;
    }
    FastOS& operator<<(char* i) {
        while (*i) put(*i++);
        return *this;
    }
} fout;