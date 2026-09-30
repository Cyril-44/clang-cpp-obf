#include <string>
#include <iostream>
#include <stdio.h>
#include <cstring>
#include <bitset>
#include <vector>
#include "FastIO.cpp"
class Decimal {
    constexpr static size_t DIGITLEN = 1 << 17;
    constexpr static unsigned BASE = 1e9;
    constexpr static int BASE_I = 1e9;
    constexpr static int BASELEN = 9;
    bool sign;  // 0->positive 1->negative
    std::vector<unsigned> a;
    /// @brief Helper Function : fix array due to BASE
    /// @tparam T an integral type (aka. unsigned, unsigned long long)
    /// @param x the array to fix
   
template <typename T>  void normalize(std::vector<T> &x) {
        for (size_t i = 0; i < x.size(); i++) {
            if (i+1 == x.size() && (x[i] >= BASE_I || x[i] < 0))
                x.emplace_back(); // Which is impossible
            if (x[i] >= BASE_I) x[i+1] += x[i] / BASE_I;
            if (x[i] < 0) x[i+1] -= ((-x[i] + BASE_I-1) / BASE_I);
            x[i] = (x[i] % BASE_I + BASE_I) % BASE_I;
        }
        a.resize(x.size());
        for (size_t i = 0; i < x.size(); i++)
            a[i] = static_cast<unsigned>(x[i]);
        while (!a.empty())
            if (*a.rbegin() == 0) a.pop_back();
            else break;
        if (a.empty()) sign = false;
    }
template <typename FuncType>
    char* read(FuncType method) {
        static char buf[DIGITLEN];
        int top(0);
        char ch = method();
        while ((ch < '0' || ch > '9') && (ch ^ '-')) ch = method();
        if (ch == '-') buf[top++] = '-', ch = method();
        while ((top ^ DIGITLEN) && ch >= '0' && ch <= '9')
            buf[top++] = ch, ch = method();
        buf[top] = '\0';
        return buf;
    }
    Decimal(const bool &x, const std::vector<unsigned> &y) : sign(x), a(y) {}
    Decimal(const char *_s, const bool &flg) : sign(false) {
        char *s = const_cast<char*>((*_s ^ '-') ? (_s) : (_s + 1));
        if (*s == '0') return;
        if (*_s == '-') sign = true;
        static char _buf[DIGITLEN];
        char* buf;
        int top;
        if (flg) {for (top = 0; s[top]; ++top); buf = s;}
        else {
            buf = _buf;
            for (top = 0; s[top]; ++top) {
                if (s[top] < '0' || s[top] > '9') throw;
                buf[top] = s[top];
            }
        }
        for (int i = 0, j = top-1; i < j; i++, --j)
            buf[i] ^= buf[j] ^= buf[i] ^= buf[j];
        a.resize((top + BASELEN - 1) / BASELEN);
        for (size_t i = 0, j = 0; i < a.size(); i++) {
            unsigned bs = 1;
            for (char k = BASELEN; (j ^ top) && k; j++, --k)
                a[i] += bs * (buf[j] ^ '0'), bs *= 10;
        }
    }
public:
    Decimal() : sign(false) {}
template <typename T, typename = typename std::enable_if<std::is_integral<T>::value, bool>::type> Decimal(T x) : sign(false) {
        if (std::is_signed<T>::value && x < 0)
            sign = true, x = -x;
        while (x) {
            a.emplace_back(x % BASE);
            x /= BASE;
        }
    }

template<size_t N> Decimal(std::bitset<N> bit) {
        unsigned long tp;
        Decimal base(1);
        static const Decimal long_up = (sizeof(unsigned long) == 8UL) ? Decimal("18446744073709551616") : Decimal("4294967296");
        while (bit.any()) {
            tp = bit & static_cast<unsigned long>(-1);
            bit >>= (sizeof(unsigned long) == 8UL) ? 64 : 32;
            *this += base * tp;
            base *= long_up;
        }
    }
    Decimal(const char *s) { *this = Decimal(s, false); }
    Decimal(const std::string &s) { *this = Decimal(s.c_str(), false); }
    explicit operator bool () const { return !a.empty(); }

// #ifdef _FASTIO
//     void in() { *this = Decimal(read(FastIO::nc), true); }
// #else
    void in() { *this = Decimal(read(getchar), true); }
// #endif
    char* c_str() const { // Temporary, Coverable and Unstable
        static char buffer[DIGITLEN];
        if (a.empty()) {
            buffer[0] = '0';
            buffer[1] = '\0';
            return buffer;
        }
        int top(0);
        if (sign) buffer[top++] = '-';
        char buf[BASELEN];
        auto it = a.crbegin(); unsigned y = *it;
        for (int i = BASELEN - 1; i >= 0; --i) buf[i] = (y % 10) ^ '0', y /= 10;
        for (int i = 0; i < BASELEN; i++) if (buf[i] ^ '0') { for (; i < BASELEN; i++) buffer[top++] = buf[i]; break; }
        for (++it; it != a.crend(); ++it) { y = *it;
            for (int i = BASELEN - 1; i >= 0; --i) buf[i] = (y % 10) ^ '0', y /= 10;
            for (int i = 0; i < BASELEN; i++) buffer[top++] = buf[i];
        }
        buffer[top] = '\0';
        return buffer;
    }

    friend std::string to_string(const Decimal &x) { return std::string(x.c_str()); }

    template<typename IStream>
    friend IStream& operator>>(IStream &is, Decimal &x) {
        x = Decimal(x.read([&is]()->int {return is.get();}), true);
        return is;
    }
    template<typename OStream>
    friend OStream& operator<<(OStream &os, const Decimal &x) { return os << x.c_str(); }
    friend Decimal abs(Decimal x) { x.sign = false; return x; }
    Decimal operator-() const { return Decimal(!sign, a); }
    Decimal operator+(const Decimal &x) const { return Decimal(*this) += x; }
    Decimal& operator-=(const Decimal &x) { return *this += -x; }
    Decimal operator-(const Decimal &x) const { return Decimal(*this) + -x; }
    Decimal operator*(const Decimal &x) const { return Decimal(*this) *= x; }
    Decimal operator/(const Decimal &x) const { return Decimal(*this) /= x; }
    Decimal& operator%=(const Decimal &x) { return *this -= *this / x * x; }
    Decimal operator%(const Decimal &x) const { return *this - *this / x * x; }
    Decimal operator^(const Decimal &x) const { return Decimal(*this) ^= x; }
    bool operator==(const Decimal &x) const { return sign == x.sign && a == x.a; }
    bool operator!=(const Decimal &x) const { return sign != x.sign || a != x.a; }
    bool operator<=(const Decimal &x) const { return *this == x || *this < x; }
    bool operator>(const Decimal &x) const { return x < *this; }
    bool operator>=(const Decimal &x) const { return x == *this || x < *this; }
    template <typename T> friend bool operator==(const T &x, const Decimal &y) { return Decimal(x) == y; }
    template <typename T> friend bool operator==(const Decimal &x, const T &y) { return x == Decimal(y); }
    template <typename T> friend bool operator!=(const T &x, const Decimal &y) { return Decimal(x) != y; }
    template <typename T> friend bool operator!=(const Decimal &x, const T &y) { return x != Decimal(y); }
    template <typename T> friend bool operator<(const T &x, const Decimal &y) { return Decimal(x) < y; }
    template <typename T> friend bool operator<(const Decimal &x, const T &y) { return x < Decimal(y); }
    template <typename T> friend bool operator<=(const T &x, const Decimal &y) { return Decimal(x) <= y; }
    template <typename T> friend bool operator<=(const Decimal &x, const T &y) { return x <= Decimal(y); }
    template <typename T> friend bool operator>(const T &x, const Decimal &y) { return Decimal(x) > y; }
    template <typename T> friend bool operator>(const Decimal &x, const T &y) { return x > Decimal(y); }
    template <typename T> friend bool operator>=(const T &x, const Decimal &y) { return Decimal(x) >= y; }
    template <typename T> friend bool operator>=(const Decimal &x, const T &y) { return x >= Decimal(y); }
    template <typename T> friend Decimal operator+(const T &x, const Decimal &y) { return Decimal(x) + y; }
    template <typename T> friend Decimal operator+(const Decimal &x, const T &y) { return x + Decimal(y); }
    template <typename T> friend Decimal& operator+=(Decimal &x, const T &y) { return x += Decimal(y); }
    template <typename T> friend Decimal operator-(const T &x, const Decimal &y) { return Decimal(x) - y; }
    template <typename T> friend Decimal operator-(const Decimal &x, const T &y) { return x - Decimal(y); }
    template <typename T> friend Decimal& operator-=(Decimal &x, const T &y) { return x -= Decimal(y); }
    template <typename T> friend Decimal operator*(const T &x, const Decimal &y) { return Decimal(x) * y; }
    template <typename T> friend Decimal operator*(const Decimal &x, const T &y) { return x * Decimal(y); }
    template <typename T> friend Decimal& operator*=(Decimal &x, const T &y) { return x *= Decimal(y); }
    template <typename T> friend Decimal operator/(const T &x, const Decimal &y) { return Decimal(x) / y; }
    template <typename T> friend Decimal operator/(const Decimal &x, const T &y) { return x / Decimal(y); }
    template <typename T> friend Decimal& operator/=(Decimal &x, const T &y) { return x /= Decimal(y); }
    template <typename T> friend Decimal operator%(const T &x, const Decimal &y) { return Decimal(x) % y; }
    template <typename T> friend Decimal operator%(const Decimal &x, const T &y) { return x % Decimal(y); }
    template <typename T> friend Decimal& operator%=(Decimal &x, const T &y) { return x %= Decimal(y); }
    template <typename T> friend Decimal operator^(const T &x, const Decimal &y) { return Decimal(x) ^ y; }
    template <typename T> friend Decimal operator^(const Decimal &x, const T &y) { return x ^ Decimal(y); }
    template <typename T> friend Decimal& operator^=(Decimal &x, const T &y) { return x ^= Decimal(y); }
    bool operator<(const Decimal &x) const {
        if (sign && !x.sign) return true; // - < +
        if (!sign && x.sign) return false; // + > -
        if (sign) return -x < -*this; // -a < -b => b < a
        if (a.size() < x.a.size()) return true;
        if (x.a.size() < a.size()) return false;
        for (size_t i = a.size()-1; i ^ static_cast<size_t>(-1); --i) {
            if (a[i] < x.a[i]) return true;
            if (a[i] > x.a[i]) return false;
        }
        return false;
    }
    Decimal& operator+=(const Decimal &x) {
        static std::vector<int> help_array;
        const static auto func = [](std::vector<int>& a, const std::vector<unsigned>& x, const std::vector<unsigned>& y, const int &fx, const int &fy)->void {
            size_t digits(std::max(x.size(), y.size()));
            a.resize(digits + 1);
            std::fill(a.begin(), a.end(), 0);
            for (size_t i = 0; i < digits; ++i) {
                if (i < x.size()) a[i] += x[i] * fx;
                if (i < y.size()) a[i] += y[i] * fy;
            }
        };
        if (sign && !x.sign)
            if (-*this > x) // -a + b (a>b) = -(a + -b)
                sign = false, *this += -x, sign = true;
            else // -a + b (a<b)
                sign = false, func(help_array, a, x.a, -1, 1);
        else if (!sign && x.sign)
            if (*this < -x) // a + -b (a<b) = -(-a + b)
                sign = true, *this += -x, sign = true;
            else // a + -b (a>b)
                func(help_array, a, x.a, 1, -1);
        else func(help_array, a, x.a, 1, 1);
        normalize(help_array);
        return *this;
    }
    Decimal& operator*=(const Decimal &x) {
        sign = static_cast<bool>(sign ^ x.sign);
        std::vector<unsigned long long> help_array(a.size() + x.a.size() + 1);
        for (size_t i = 0; i < a.size(); i++)
            for (size_t j = 0; j < x.a.size(); j++) {
                help_array[i + j] += 1ULL * a[i] * x.a[j];
                if (help_array[i+j] >= BASE) { // to prevent overflow
                    help_array[i+j+1] += help_array[i+j] / BASE;
                    help_array[i+j] %= BASE;
                }
            }
        normalize(help_array);
        return *this;
    }
    Decimal& operator/=(const Decimal &x) {
        if (x.a.empty()) throw std::domain_error("Division by zero.");   // Throw an exception when x=0
        if (a.size() < x.a.size()) return *this = std::move(Decimal());
        size_t digits(a.size() - x.a.size() + 1);
        Decimal tp;
        std::vector<unsigned long long> help_array(a.size());
        bool sn = static_cast<bool>(sign ^ x.sign);
        sign = false;
        std::vector<int> div(digits);
        for (size_t i = digits - 1; i ^ static_cast<size_t>(-1); --i) {
            int l(0), r(BASE - 1), mid;
            while (l <= r) {
                mid = (l + r) >> 1;
                std::fill(help_array.begin(), help_array.end(), 0);
                for (size_t j = 0; j < x.a.size(); j++)
                    help_array[i + j] += 1ULL * mid * x.a[j];
                tp.normalize(help_array);
                if (tp > *this) r = mid - 1;
                else l = mid + 1;
            }
            std::fill(help_array.begin(), help_array.end(), 0);
            for (size_t j = 0; j < x.a.size(); j++)
                help_array[i + j] += 1ULL * r * x.a[j];
            tp.normalize(help_array);
            *this -= tp;
            div[i] = r;
        }
        sign = sn;
        normalize(div);
        return *this;
    }
    Decimal& operator^=(Decimal x) {
        if (x < 0) throw std::range_error("Negative Exponent.");
        Decimal tp = abs(*this);
        *this = (sign ? -1 : 1);
        while (!x.a.empty()) {
            if (x % 2 == 1) *this *= tp;
            tp *= tp;
            x /= Decimal(2);
        }
        return *this;
    }
};