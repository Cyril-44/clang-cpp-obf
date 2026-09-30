#include <cstdio>
#include <string>
#include <concepts>
#include <type_traits>

// 定义一个概念来匹配所有整数类型，包括 __int128
template <typename T>
concept IntegerWithI128 = std::integral<T> || std::same_as<T, __int128_t> || std::same_as<T, __uint128_t>;
class FastIS {
    static constexpr size_t BUF_SIZ = 1 << 20;
    char buffer[BUF_SIZ];
    char *p1 = nullptr, *p2 = nullptr;
    FILE *src;
public:
    FastIS(FILE *f = stdin) : src(f) {}
    [[gnu::always_inline]] char get() {
        if (p1 == p2) {
            p1 = buffer;
            p2 = buffer + fread(buffer, 1, BUF_SIZ, src);
            if (p1 == p2) return EOF;
        }
        return *p1++;
    }
    FastIS& operator>>(bool& rhs) {
        int ch = get();
        while (~ch && ch != '0' && ch != '1') ch = get();
        rhs = (ch == '1');
        return *this;
    }
    FastIS& operator>>(char& rhs) {
        rhs = get();
        while (rhs == ' ' || rhs == '\r' || rhs == '\n' || rhs == '\t') rhs = get();
        return *this;
    }
    template <IntegerWithI128 T>
    FastIS& operator>>(T& rhs) {
        int ch = get();
        bool neg = false;
        while (~ch && (ch < '0' || ch > '9') && ch != '-') ch = get();
        if constexpr (std::is_signed_v<T> || std::same_as<T, __int128_t>) {
            if (ch == '-') { neg = true; ch = get(); }
        }
        rhs = 0;
        while (ch >= '0' && ch <= '9') {
            rhs = (rhs << 3) + (rhs << 1) + (ch ^ '0');
            ch = get();
        }
        if (neg) rhs = -rhs;
        return *this;
    }
    template <std::floating_point T>
    FastIS& operator>>(T& rhs) {
        int ch = get();
        bool neg = false;
        while (~ch && (ch < '0' || ch > '9') && ch != '-') ch = get();
        if (ch == '-') { neg = true; ch = get(); }
        
        __uint128_t integer = 0;
        while (ch >= '0' && ch <= '9') {
            integer = (integer << 3) + (integer << 1) + (ch ^ '0');
            ch = get();
        }
        rhs = static_cast<T>(integer);
        if (ch == '.') {
            T base = 1.0;
            for (ch = get(); ch >= '0' && ch <= '9'; ch = get()) {
                rhs += (ch ^ '0') * (base /= 10.0);
            }
        }
        if (neg) rhs = -rhs;
        return *this;
    }
    FastIS& operator>>(char *s) {
        int ch = get();
        while (~ch && (ch <= ' ')) ch = get();
        while (~ch && (ch > ' ')) *s++ = ch, ch = get();
        *s = '\0';
        return *this;
    }
} fin;

class FastOutputStream {
    static constexpr size_t BUF_SIZ = 1 << 20;
    char buffer[BUF_SIZ], *p = buffer;
    FILE *dest; long double base = 5e-6;
    unsigned char prec = 6;
public:
    FastOutputStream(FILE *f = stdout) : dest(f) { setvbuf(dest, nullptr, _IONBF, 0); }
    ~FastOutputStream() { flush(); }
    void flush() {
        fwrite(buffer, 1, p - buffer, dest);
        p = buffer;
    }
    [[gnu::always_inline]] void put(char c) {
        if (p == buffer + BUF_SIZ) flush();
        *p++ = c;
    }
    FastOutputStream& precision(int n) { 
        prec = n;
        base = 0.5;
        while (n--) base *= 0.1;
        return *this;
    }
    unsigned char precision() const { return prec; }
    FastOutputStream& operator<<(char c) { put(c); return *this; }
    FastOutputStream& operator<<(const char *s) {
        while (*s) put(*s++);
        return *this;
    }
    FastOutputStream& operator<<(const std::string &s) {
        for (char c : s) put(c);
        return *this;
    }
    template <IntegerWithI128 T>
    FastOutputStream& operator<<(T rhs) {
        if (rhs == 0) { put('0'); return *this; }
        T temp = rhs;
        if constexpr (std::is_signed_v<T> || std::same_as<T, __int128_t>) {
            if (temp < 0) { put('-'); temp = -temp; }
        }
        static char stack[64];
        int top = 0;
        while (temp) {
            stack[top++] = static_cast<char>(temp % 10) ^ '0';
            temp /= 10;
        }
        while (top) put(stack[--top]);
        return *this;
    }
    template <std::floating_point T>
    FastOutputStream& operator<<(T rhs) {
        rhs += base;
        if (rhs < 0) { put('-'); rhs = -rhs; }
        __uint128_t inte = static_cast<__uint128_t>(rhs);
        *this << inte;
        T frac = rhs - static_cast<T>(inte);
        if (prec > 0) {
            put('.');
            for (unsigned count = prec; count; --count) {
                frac *= 10;
                int digit = static_cast<int>(frac);
                put(digit ^ '0');
                frac -= digit;
            }
        }
        return *this;
    }
} fout;
#ifdef MULTI_TEST_CASES
auto __read_extra_test_cases = [](int x){fin >> x; return x;}();
#endif
int main() {
    double x;
    fin >> x;
    fout.precision(6) << x;
    return 0;
}