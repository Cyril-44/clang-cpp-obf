#include <bits/stdc++.h>
using namespace std;
class FastIS
{
    static constexpr size_t BUF_SIZ = 1 << 20;
    char buffer[BUF_SIZ];
    char *p1, *p2;
    FILE *src;

public:
    FastIS(FILE *f = stdin) : p1(nullptr), p2(nullptr), src(f) {}
    __attribute__((always_inline)) inline int get() { return (p1 == p2) && (p2 = (p1 = buffer) + fread(buffer, 1, BUF_SIZ, src), p1 == p2) ? EOF : *p1++; }
    template <typename T>
    typename std::enable_if<std::is_unsigned<T>::value && std::is_integral<T>::value, FastIS &>::type inline operator>>(T &rhs)
    {
        int ch = get();
        while (~ch && (ch < '0' || ch > '9'))
            ch = get();
        for (rhs = 0; ch >= '0' && ch <= '9'; ch = get())
            rhs = (rhs << 3) + (rhs << 1) + (ch ^ '0');
        return *this;
    }
    template <typename T>
    typename std::enable_if<std::is_signed<T>::value && std::is_integral<T>::value && !std::is_same<T, bool>::value && !std::is_same<T, char>::value, FastIS &>::type inline operator>>(T &rhs)
    {
        int ch = get();
        bool flg = false;
        while (~ch && (ch < '0' || ch > '9') && (ch ^ '-'))
            ch = get();
        if (ch == '-')
            ch = get(), flg = true;
        for (rhs = 0; ch >= '0' && ch <= '9'; ch = get())
            rhs = (rhs << 3) + (rhs << 1) + (ch ^ '0');
        rhs = flg ? -rhs : rhs;
        return *this;
    }
    template <typename T>
    typename std::enable_if<std::is_floating_point<T>::value, FastIS &>::type inline operator>>(T &rhs)
    {
        int ch = get();
        bool flg = false;
        while (~ch && (ch < '0' || ch > '9') && (ch ^ '-'))
            ch = get();
        if (ch == '-')
            ch = get(), flg = true;
        using Int = typename std::conditional<std::is_same<T, float>::value, std::uint32_t, std::uint64_t>::type;
        Int integer;
        for (integer = 0; ch >= '0' && ch <= '9'; ch = get())
            integer = (integer << 3) + (integer << 1) + (ch ^ '0');
        rhs = integer;
        if (ch == '.')
        {
            Int base = 1;
            for (ch = get(); ch >= '0' && ch <= '9'; ch = get())
                rhs += (ch ^ '0') * (1. / (base *= 10));
        }
        if (flg)
            rhs = -rhs;
        return *this;
    }
    inline FastIS &operator>>(bool &x)
    {
        int ch = get();
        while (~ch && ch != '0' && ch != '1')
            ch = get();
        x = static_cast<bool>(ch & 1);
        return *this;
    }
    inline FastIS &operator>>(char &s)
    {
        int ch = get();
        while (ch == ' ' || ch == '\r' || ch == '\n')
            ch = get();
        s = ch;
        return *this;
    }
    inline FastIS &operator>>(char *s)
    {
        int ch = get();
        while (~ch && (ch == ' ' || ch == '\r' || ch == '\n'))
            ch = get();
        while (~ch && (ch != ' ' && ch != '\r' && ch != '\n'))
            *s++ = ch, ch = get();
        return *this;
    }
} fin;
class FastOS
{
    static constexpr size_t BUF_SIZ = 1 << 20;
    static constexpr double EPS = 1e-9;
    char buffer[BUF_SIZ], *p;
    const char *pt;
    FILE *dest;
    int prec;

public:
    FastOS(FILE *f = stdout) : p(buffer), pt(buffer + BUF_SIZ), dest(f), prec(-1) { setvbuf(dest, nullptr, _IONBF, 0); }
    ~FastOS()
    {
        flush();
        fclose(dest);
    }
    FastOS &flush()
    {
        fwrite(buffer, p - buffer, 1, dest);
        return *this;
    }
    FastOS &put(int c)
    {
        *p++ = c;
        if (p == pt)
            fwrite(buffer, BUF_SIZ, 1, dest), p = buffer;
        return *this;
    }
    FastOS &setprecision(const int &new_prec)
    {
        prec = new_prec;
        return *this;
    }
    template <typename T>
    typename std::enable_if<std::is_unsigned<T>::value && std::is_integral<T>::value, FastOS &>::type inline operator<<(T rhs)
    {
        static int s[32];
        s[ *s = 1] = rhs % 10;
        while (rhs /= 10)
            s[++(*s)] = rhs % 10;
        while (*s)
            put(s[(*s)--] | '0');
        return *this;
    }
    template <typename T>
    typename std::enable_if<std::is_signed<T>::value && std::is_integral<T>::value && !std::is_same<T, char>::value, FastOS &>::type inline operator<<(const T &rhs)
    {
        if (rhs < 0)
            put('-') << (typename std::make_unsigned<T>::type)(-rhs);
        else
            *this << (typename std::make_unsigned<T>::type)(rhs);
    }
    template <typename T>
    typename std::enable_if<std::is_floating_point<T>::value, FastOS &>::type inline operator<<(T rhs)
    {
        if (rhs < 0)
            put('-'), rhs = -rhs;
        using Int = typename std::conditional<std::is_same<T, float>::value, std::uint32_t, std::uint64_t>::type;
        Int inte = static_cast<Int>(rhs);
        T frac = rhs - inte;
        *this << inte;
        if (prec > 0 || prec < 0 && frac > EPS)
        {
            put('.');
            for (int i = 0; prec > 0 && i < prec || prec < 0 && frac > EPS; i++)
            {
                int digit = static_cast<int>(frac *= 10);
                put(digit | '0');
                frac -= digit;
            }
        }
        return *this;
    }
    FastOS &operator<<(const char &c)
    {
        put(c);
        return *this;
    }
} fout;
class Demical
{
    constexpr static size_t DIGITLEN = 1 << 17;
    constexpr static unsigned BASE = 1e9;
    constexpr static int BASE_I = 1e9;
    constexpr static int BASELEN = 9;
    bool sign;
    std::vector<unsigned> a;
    template <typename T>
    inline void normalize(std::vector<T> &x)
    {
        for (size_t i = 0; i < x.size(); i++)
        {
            if (i + 1 == x.size() && (x[i] >= BASE_I || x[i] < 0))
                x.emplace_back();
            if (x[i] >= BASE_I)
                x[i + 1] += x[i] / BASE_I;
            if (x[i] < 0)
                x[i + 1] -= ((-x[i] + BASE_I - 1) / BASE_I);
            x[i] = (x[i] % BASE_I + BASE_I) % BASE_I;
        }
        a.resize(x.size());
        for (size_t i = 0; i < x.size(); i++)
            a[i] = static_cast<unsigned>(x[i]);
        while (!a.empty())
            if (*a.rbegin() == 0)
                a.pop_back();
            else
                break;
        if (a.empty())
            sign = false;
    }
    template <typename FuncType>
    inline char *read(FuncType method)
    {
        static char buf[DIGITLEN];
        int top(0);
        char ch = method();
        while ((ch < '0' || ch > '9') && (ch ^ '-'))
            ch = method();
        if (ch == '-')
            buf[top++] = '-', ch = method();
        while ((top ^ DIGITLEN) && ch >= '0' && ch <= '9')
            buf[top++] = ch, ch = method();
        buf[top] = '\0';
        return buf;
    }
    inline Demical(const bool &x, const std::vector<unsigned> &y) : sign(x), a(y) {}
    Demical(const char *_s, const bool &flg) : sign(false)
    {
        char *s = const_cast<char *>((*_s ^ '-') ? (_s) : (_s + 1));
        if (*s == '0')
            return;
        if (*_s == '-')
            sign = true;
        static char _buf[DIGITLEN];
        char *buf;
        int top;
        if (flg)
        {
            for (top = 0; s[top]; ++top)
                ;
            buf = s;
        }
        else
        {
            buf = _buf;
            for (top = 0; s[top]; ++top)
            {
                if (s[top] < '0' || s[top] > '9')
                    throw;
                buf[top] = s[top];
            }
        }
        for (int i = 0, j = top - 1; i < j; i++, --j)
            buf[i] ^= buf[j] ^= buf[i] ^= buf[j];
        a.resize((top + BASELEN - 1) / BASELEN);
        for (size_t i = 0, j = 0; i < a.size(); i++)
        {
            unsigned bs = 1;
            for (char k = BASELEN; (j ^ top) && k; j++, --k)
                a[i] += bs * (buf[j] ^ '0'), bs *= 10;
        }
    }

public:
    Demical() : sign(false) {}
    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value, bool>::type>
    Demical(T x) : sign(false)
    {
        if (std::is_signed<T>::value && x < 0)
            sign = true, x = -x;
        while (x)
        {
            a.emplace_back(x % BASE);
            x /= BASE;
        }
    }
    template <size_t N>
    Demical(std::bitset<N> bit)
    {
        unsigned long tp;
        Demical base(1);
        static const Demical long_up = (sizeof(unsigned long) == 8UL) ? Demical("18446744073709551616") : Demical("4294967296");
        while (bit.any())
        {
            tp = bit & static_cast<unsigned long>(-1);
            bit >>= (sizeof(unsigned long) == 8UL) ? 64 : 32;
            *this += base * tp;
            base *= long_up;
        }
    }
    Demical(const char *s) { *this = Demical(s, false); }
    Demical(const std::string &s) { *this = Demical(s.c_str(), false); }
    explicit operator bool() const { return !a.empty(); }
    inline void in() { *this = Demical(read(getchar), true); }
    inline char *c_str() const
    {
        static char buffer[DIGITLEN];
        if (a.empty())
        {
            buffer[0] = '0';
            buffer[1] = '\0';
            return buffer;
        }
        int top(0);
        if (sign)
            buffer[top++] = '-';
        char buf[BASELEN];
        auto it = a.crbegin();
        unsigned y = *it;
        for (int i = BASELEN - 1; i >= 0; --i)
            buf[i] = (y % 10) ^ '0', y /= 10;
        for (int i = 0; i < BASELEN; i++)
            if (buf[i] ^ '0')
            {
                for (; i < BASELEN; i++)
                    buffer[top++] = buf[i];
                break;
            }
        for (++it; it != a.crend(); ++it)
        {
            y = *it;
            for (int i = BASELEN - 1; i >= 0; --i)
                buf[i] = (y % 10) ^ '0', y /= 10;
            for (int i = 0; i < BASELEN; i++)
                buffer[top++] = buf[i];
        }
        buffer[top] = '\0';
        return buffer;
    }
    friend std::string to_string(const Demical &x) { return std::string(x.c_str()); }
    template <typename IStream>
    friend IStream &operator>>(IStream &is, Demical &x)
    {
        x = Demical(x.read([&is]() -> int
                           { return is.get(); }),
                    true);
        return is;
    }
    template <typename OStream>
    friend OStream &operator<<(OStream &os, const Demical &x) { return os << x.c_str(); }
    friend inline Demical abs(Demical x)
    {
        x.sign = false;
        return x;
    }
    inline Demical operator-() const { return Demical(!sign, a); }
    inline Demical operator+(const Demical &x) const { return Demical(*this) += x; }
    inline Demical &operator-=(const Demical &x) { return *this += -x; }
    inline Demical operator-(const Demical &x) const { return Demical(*this) + -x; }
    inline Demical operator*(const Demical &x) const { return Demical(*this) *= x; }
    inline Demical operator/(const Demical &x) const { return Demical(*this) /= x; }
    inline Demical &operator%=(const Demical &x) { return *this -= *this / x * x; }
    inline Demical operator%(const Demical &x) const { return *this - *this / x * x; }
    inline Demical operator^(const Demical &x) const { return Demical(*this) ^= x; }
    inline bool operator==(const Demical &x) const { return sign == x.sign && a == x.a; }
    inline bool operator!=(const Demical &x) const { return sign != x.sign || a != x.a; }
    inline bool operator<=(const Demical &x) const { return *this == x || *this < x; }
    inline bool operator>(const Demical &x) const { return x < *this; }
    inline bool operator>=(const Demical &x) const { return x == *this || x < *this; }
    template <typename T>
    friend inline bool operator==(const T &x, const Demical &y) { return Demical(x) == y; }
    template <typename T>
    friend inline bool operator==(const Demical &x, const T &y) { return x == Demical(y); }
    template <typename T>
    friend inline bool operator!=(const T &x, const Demical &y) { return Demical(x) != y; }
    template <typename T>
    friend inline bool operator!=(const Demical &x, const T &y) { return x != Demical(y); }
    template <typename T>
    friend inline bool operator<(const T &x, const Demical &y) { return Demical(x) < y; }
    template <typename T>
    friend inline bool operator<(const Demical &x, const T &y) { return x < Demical(y); }
    template <typename T>
    friend inline bool operator<=(const T &x, const Demical &y) { return Demical(x) <= y; }
    template <typename T>
    friend inline bool operator<=(const Demical &x, const T &y) { return x <= Demical(y); }
    template <typename T>
    friend inline bool operator>(const T &x, const Demical &y) { return Demical(x) > y; }
    template <typename T>
    friend inline bool operator>(const Demical &x, const T &y) { return x > Demical(y); }
    template <typename T>
    friend inline bool operator>=(const T &x, const Demical &y) { return Demical(x) >= y; }
    template <typename T>
    friend inline bool operator>=(const Demical &x, const T &y) { return x >= Demical(y); }
    template <typename T>
    friend inline Demical operator+(const T &x, const Demical &y) { return Demical(x) + y; }
    template <typename T>
    friend inline Demical operator+(const Demical &x, const T &y) { return x + Demical(y); }
    template <typename T>
    friend inline Demical &operator+=(Demical &x, const T &y) { return x += Demical(y); }
    template <typename T>
    friend inline Demical operator-(const T &x, const Demical &y) { return Demical(x) - y; }
    template <typename T>
    friend inline Demical operator-(const Demical &x, const T &y) { return x - Demical(y); }
    template <typename T>
    friend inline Demical &operator-=(Demical &x, const T &y) { return x -= Demical(y); }
    template <typename T>
    friend inline Demical operator*(const T &x, const Demical &y) { return Demical(x) * y; }
    template <typename T>
    friend inline Demical operator*(const Demical &x, const T &y) { return x * Demical(y); }
    template <typename T>
    friend inline Demical &operator*=(Demical &x, const T &y) { return x *= Demical(y); }
    template <typename T>
    friend inline Demical operator/(const T &x, const Demical &y) { return Demical(x) / y; }
    template <typename T>
    friend inline Demical operator/(const Demical &x, const T &y) { return x / Demical(y); }
    template <typename T>
    friend inline Demical &operator/=(Demical &x, const T &y) { return x /= Demical(y); }
    template <typename T>
    friend inline Demical operator%(const T &x, const Demical &y) { return Demical(x) % y; }
    template <typename T>
    friend inline Demical operator%(const Demical &x, const T &y) { return x % Demical(y); }
    template <typename T>
    friend inline Demical &operator%=(Demical &x, const T &y) { return x %= Demical(y); }
    template <typename T>
    friend inline Demical operator^(const T &x, const Demical &y) { return Demical(x) ^ y; }
    template <typename T>
    friend inline Demical operator^(const Demical &x, const T &y) { return x ^ Demical(y); }
    template <typename T>
    friend inline Demical &operator^=(Demical &x, const T &y) { return x ^= Demical(y); }
    inline bool operator<(const Demical &x) const
    {
        if (sign && !x.sign)
            return true;
        if (!sign && x.sign)
            return false;
        if (sign)
            return -x < -*this;
        if (a.size() < x.a.size())
            return true;
        if (x.a.size() < a.size())
            return false;
        for (size_t i = a.size() - 1; i ^ static_cast<size_t>(-1); --i)
        {
            if (a[i] < x.a[i])
                return true;
            if (a[i] > x.a[i])
                return false;
        }
        return false;
    }
    inline Demical &operator+=(const Demical &x)
    {
        static std::vector<int> help_array;
        const static auto func = [](std::vector<int> &a, const std::vector<unsigned> &x, const std::vector<unsigned> &y, const int &fx, const int &fy) -> void
        {size_t digits(std::max(x.size(),y.size()));a.resize(digits+1);std::fill(a.begin(),a.end(),0);for(size_t i=0;i<digits;++i){if(i<x.size())a[i]+=x[i]*fx;if(i<y.size())a[i]+=y[i]*fy;} };
        if (sign && !x.sign)
            if (-*this > x)
                sign = false, *this += -x, sign = true;
            else
                sign = false, func(help_array, a, x.a, -1, 1);
        else if (!sign && x.sign)
            if (*this < -x)
                sign = true, *this += -x, sign = true;
            else
                func(help_array, a, x.a, 1, -1);
        else
            func(help_array, a, x.a, 1, 1);
        normalize(help_array);
        return *this;
    }
    inline Demical &operator*=(const Demical &x)
    {
        sign = static_cast<bool>(sign ^ x.sign);
        std::vector<unsigned long long> help_array(a.size() + x.a.size() + 1);
        for (size_t i = 0; i < a.size(); i++)
            for (size_t j = 0; j < x.a.size(); j++)
            {
                help_array[i + j] += 1ULL * a[i] * x.a[j];
                if (help_array[i + j] >= BASE)
                {
                    help_array[i + j + 1] += help_array[i + j] / BASE;
                    help_array[i + j] %= BASE;
                }
            }
        normalize(help_array);
        return *this;
    }
    inline Demical &operator/=(const Demical &x)
    {
        if (x.a.empty())
            throw std::domain_error("Division by zero.");
        if (a.size() < x.a.size())
            return *this = std::move(Demical());
        size_t digits(a.size() - x.a.size() + 1);
        Demical tp;
        std::vector<unsigned long long> help_array(a.size());
        bool sn = static_cast<bool>(sign ^ x.sign);
        sign = false;
        std::vector<int> div(digits);
        for (size_t i = digits - 1; i ^ static_cast<size_t>(-1); --i)
        {
            int l(0), r(BASE - 1), mid;
            while (l <= r)
            {
                mid = (l + r) >> 1;
                std::fill(help_array.begin(), help_array.end(), 0);
                for (size_t j = 0; j < x.a.size(); j++)
                    help_array[i + j] += 1ULL * mid * x.a[j];
                tp.normalize(help_array);
                if (tp > *this)
                    r = mid - 1;
                else
                    l = mid + 1;
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
    inline Demical &operator^=(Demical x)
    {
        if (x < 0)
            throw std::range_error("Negative Exponent.");
        Demical tp = abs(*this);
        *this = (sign ? -1 : 1);
        while (!x.a.empty())
        {
            if (x % 2 == 1)
                *this *= tp;
            tp *= tp;
            x /= Demical(2);
        }
        return *this;
    }
};
template <typename oeBrbVzUjn>
oeBrbVzUjn OkKSTGGSyVz(const oeBrbVzUjn &OBrLIVvbqHkWkCGy, const oeBrbVzUjn &oJyzVkZHcBhpPPE)
{ // OBrLIVvbqHkWkCGy * oHPQArhaanQOaVdDl ≡ 1 (oRxPPhnhYhTc oJyzVkZHcBhpPPE)
    assert(OBrLIVvbqHkWkCGy != 0);
    oeBrbVzUjn oHPQArhaanQOaVdDl = 0, oxciWpZoGMQnylv = 1, OrBbypkLsZbckxewo = OBrLIVvbqHkWkCGy, olvLvBXeX = oJyzVkZHcBhpPPE, OImQYwjtJSvWGWtrP;
    while (OrBbypkLsZbckxewo != 0)
    {
        OImQYwjtJSvWGWtrP = olvLvBXeX / OrBbypkLsZbckxewo;
        std::swap(OrBbypkLsZbckxewo, olvLvBXeX -= OImQYwjtJSvWGWtrP * OrBbypkLsZbckxewo);
        std::swap(oHPQArhaanQOaVdDl -= OImQYwjtJSvWGWtrP * oxciWpZoGMQnylv, oxciWpZoGMQnylv);
    }
    assert(olvLvBXeX == 1);
    return oHPQArhaanQOaVdDl;
}

template <typename Mod>
class oTGlMKgAgDGeqX
{
    using OAtfPGjNXBBtbkCgG = typename std::decay<decltype(Mod::value)>::type;
    OAtfPGjNXBBtbkCgG value;
    constexpr static OAtfPGjNXBBtbkCgG oRxPPhnhYhTc() { return Mod::value; }
    template <typename oeBrbVzUjn>
    oeBrbVzUjn oHLbOZpnthNJ(oeBrbVzUjn OBrLIVvbqHkWkCGy)
    {
        OAtfPGjNXBBtbkCgG OKhEcAnJMrKdaG = static_cast<OAtfPGjNXBBtbkCgG>(-oRxPPhnhYhTc() < OBrLIVvbqHkWkCGy && OBrLIVvbqHkWkCGy < oRxPPhnhYhTc() ? OBrLIVvbqHkWkCGy : OBrLIVvbqHkWkCGy % oRxPPhnhYhTc());
        return (OKhEcAnJMrKdaG < 0 ? OKhEcAnJMrKdaG += oRxPPhnhYhTc() : OKhEcAnJMrKdaG);
    }

public:
    constexpr oTGlMKgAgDGeqX() : value() {}
    template <typename oeBrbVzUjn>
    oTGlMKgAgDGeqX(const oeBrbVzUjn &OfxmFfKLbkLYojpOI) { value = oHLbOZpnthNJ(OfxmFfKLbkLYojpOI); }
    template <typename oeBrbVzUjn>
    explicit operator oeBrbVzUjn() const { return static_cast<oeBrbVzUjn>(value); }
    const OAtfPGjNXBBtbkCgG &operator()() const { return value; }
    oTGlMKgAgDGeqX &operator+=(const oTGlMKgAgDGeqX &OfxmFfKLbkLYojpOI)
    {
        if ((value += OfxmFfKLbkLYojpOI.value) >= oRxPPhnhYhTc())
            value -= oRxPPhnhYhTc();
        return *this;
    }
    oTGlMKgAgDGeqX &operator-=(const oTGlMKgAgDGeqX &OfxmFfKLbkLYojpOI)
    {
        if ((value -= OfxmFfKLbkLYojpOI.value) < 0)
            value += oRxPPhnhYhTc();
        return *this;
    }
    oTGlMKgAgDGeqX &operator*=(const oTGlMKgAgDGeqX &OfxmFfKLbkLYojpOI)
    {
        if constexpr (std::is_same<OAtfPGjNXBBtbkCgG, int>::value)
            value = oHLbOZpnthNJ(static_cast<int64_t>(value) * OfxmFfKLbkLYojpOI.value);
        else
            value = oHLbOZpnthNJ(value * OfxmFfKLbkLYojpOI.value);
        return *this;
    }
    oTGlMKgAgDGeqX &operator/=(const oTGlMKgAgDGeqX &OfxmFfKLbkLYojpOI) { return *this *= oTGlMKgAgDGeqX(OkKSTGGSyVz(OfxmFfKLbkLYojpOI.value, oRxPPhnhYhTc())); }
    template <typename oeBrbVzUjn>
    typename std::enable_if<std::is_integral<oeBrbVzUjn>::value, oTGlMKgAgDGeqX>::type &operator^=(oeBrbVzUjn OfxmFfKLbkLYojpOI)
    {
        if (OfxmFfKLbkLYojpOI < 0)
            return *this = oTGlMKgAgDGeqX(OkKSTGGSyVz(static_cast<OAtfPGjNXBBtbkCgG>(1), (*this ^ (-OfxmFfKLbkLYojpOI))()));
        oTGlMKgAgDGeqX OBuZukgsvDn = *this;
        for (*this = static_cast<OAtfPGjNXBBtbkCgG>(1); OfxmFfKLbkLYojpOI; OfxmFfKLbkLYojpOI >>= 1)
        {
            if (OfxmFfKLbkLYojpOI & 1)
                *this *= OBuZukgsvDn;
            OBuZukgsvDn *= OBuZukgsvDn;
        }
        return *this;
    }
    oTGlMKgAgDGeqX operator-() const { return oTGlMKgAgDGeqX(-value); }
    oTGlMKgAgDGeqX &operator++() { return *this += 1; }
    oTGlMKgAgDGeqX &operator--() { return *this -= 1; }
    oTGlMKgAgDGeqX operator++(int)
    {
        oTGlMKgAgDGeqX OBuZukgsvDn = *this;
        ++*this;
        return OBuZukgsvDn;
    }
    oTGlMKgAgDGeqX operator--(int)
    {
        oTGlMKgAgDGeqX OBuZukgsvDn = *this;
        --*this;
        return OBuZukgsvDn;
    }
    oTGlMKgAgDGeqX operator+(const oTGlMKgAgDGeqX &OfxmFfKLbkLYojpOI) const { return oTGlMKgAgDGeqX(*this) += OfxmFfKLbkLYojpOI; }
    oTGlMKgAgDGeqX operator-(const oTGlMKgAgDGeqX &OfxmFfKLbkLYojpOI) const { return oTGlMKgAgDGeqX(*this) -= OfxmFfKLbkLYojpOI; }
    oTGlMKgAgDGeqX operator*(const oTGlMKgAgDGeqX &OfxmFfKLbkLYojpOI) const { return oTGlMKgAgDGeqX(*this) *= OfxmFfKLbkLYojpOI; }
    oTGlMKgAgDGeqX operator/(const oTGlMKgAgDGeqX &OfxmFfKLbkLYojpOI) const { return oTGlMKgAgDGeqX(*this) /= OfxmFfKLbkLYojpOI; }
    bool operator==(const oTGlMKgAgDGeqX &OfxmFfKLbkLYojpOI) const { return value == OfxmFfKLbkLYojpOI.value; }
    bool operator!=(const oTGlMKgAgDGeqX &OfxmFfKLbkLYojpOI) const { return value != OfxmFfKLbkLYojpOI.value; }
    bool operator<=(const oTGlMKgAgDGeqX &OfxmFfKLbkLYojpOI) const { return value <= OfxmFfKLbkLYojpOI.value; }
    bool operator>=(const oTGlMKgAgDGeqX &OfxmFfKLbkLYojpOI) const { return value >= OfxmFfKLbkLYojpOI.value; }
    bool operator<(const oTGlMKgAgDGeqX &OfxmFfKLbkLYojpOI) const { return value < OfxmFfKLbkLYojpOI.value; }
    bool operator>(const oTGlMKgAgDGeqX &OfxmFfKLbkLYojpOI) const { return value > OfxmFfKLbkLYojpOI.value; }
    bool operator!() const { return value; }
    template <typename OgIvtEJXJMoxxU, typename oeBrbVzUjn>
    friend OgIvtEJXJMoxxU &operator>>(OgIvtEJXJMoxxU &, oTGlMKgAgDGeqX<oeBrbVzUjn> &);
    template <typename oVsAVhiPtcHA, typename oeBrbVzUjn>
    friend oVsAVhiPtcHA &operator<<(oVsAVhiPtcHA &, const oTGlMKgAgDGeqX<oeBrbVzUjn> &);
    template <typename oeBrbVzUjn, typename oOIRSkkgZI>
    friend oTGlMKgAgDGeqX operator+(const oTGlMKgAgDGeqX<oeBrbVzUjn> &oLsZUMFWY, const oOIRSkkgZI &OfxmFfKLbkLYojpOI) { return oLsZUMFWY + oTGlMKgAgDGeqX<oeBrbVzUjn>(OfxmFfKLbkLYojpOI); }
    template <typename oeBrbVzUjn, typename oOIRSkkgZI>
    friend oTGlMKgAgDGeqX operator+(const oOIRSkkgZI &oLsZUMFWY, const oTGlMKgAgDGeqX<oeBrbVzUjn> &OfxmFfKLbkLYojpOI) { return oTGlMKgAgDGeqX<oeBrbVzUjn>(oLsZUMFWY) + OfxmFfKLbkLYojpOI; }
    template <typename oeBrbVzUjn, typename oOIRSkkgZI>
    friend oTGlMKgAgDGeqX operator-(const oTGlMKgAgDGeqX<oeBrbVzUjn> &oLsZUMFWY, const oOIRSkkgZI &OfxmFfKLbkLYojpOI) { return oLsZUMFWY - oTGlMKgAgDGeqX<oeBrbVzUjn>(OfxmFfKLbkLYojpOI); }
    template <typename oeBrbVzUjn, typename oOIRSkkgZI>
    friend oTGlMKgAgDGeqX operator-(const oOIRSkkgZI &oLsZUMFWY, const oTGlMKgAgDGeqX<oeBrbVzUjn> &OfxmFfKLbkLYojpOI) { return oTGlMKgAgDGeqX<oeBrbVzUjn>(oLsZUMFWY) - OfxmFfKLbkLYojpOI; }
    template <typename oeBrbVzUjn, typename oOIRSkkgZI>
    friend oTGlMKgAgDGeqX operator*(const oTGlMKgAgDGeqX<oeBrbVzUjn> &oLsZUMFWY, const oOIRSkkgZI &OfxmFfKLbkLYojpOI) { return oLsZUMFWY * oTGlMKgAgDGeqX<oeBrbVzUjn>(OfxmFfKLbkLYojpOI); }
    template <typename oeBrbVzUjn, typename oOIRSkkgZI>
    friend oTGlMKgAgDGeqX operator*(const oOIRSkkgZI &oLsZUMFWY, const oTGlMKgAgDGeqX &OfxmFfKLbkLYojpOI) { return oTGlMKgAgDGeqX<oeBrbVzUjn>(oLsZUMFWY) * OfxmFfKLbkLYojpOI; }
    template <typename oeBrbVzUjn, typename oOIRSkkgZI>
    friend oTGlMKgAgDGeqX operator/(const oTGlMKgAgDGeqX<oeBrbVzUjn> &oLsZUMFWY, const oOIRSkkgZI &OfxmFfKLbkLYojpOI) { return oLsZUMFWY / oTGlMKgAgDGeqX<oeBrbVzUjn>(OfxmFfKLbkLYojpOI); }
    template <typename oeBrbVzUjn, typename oOIRSkkgZI>
    friend oTGlMKgAgDGeqX operator/(const oOIRSkkgZI &oLsZUMFWY, const oTGlMKgAgDGeqX &OfxmFfKLbkLYojpOI) { return oTGlMKgAgDGeqX<oeBrbVzUjn>(oLsZUMFWY) / OfxmFfKLbkLYojpOI; }
    template <typename oeBrbVzUjn, typename oOIRSkkgZI>
    friend typename std::enable_if<std::is_integral<oOIRSkkgZI>::value, oTGlMKgAgDGeqX>::type operator^(oTGlMKgAgDGeqX<oeBrbVzUjn> oLsZUMFWY, const oOIRSkkgZI &OfxmFfKLbkLYojpOI) { return oLsZUMFWY ^= OfxmFfKLbkLYojpOI; }
    template <typename oeBrbVzUjn, typename oOIRSkkgZI>
    friend oTGlMKgAgDGeqX operator+=(oTGlMKgAgDGeqX<oeBrbVzUjn> &oLsZUMFWY, const oOIRSkkgZI &OfxmFfKLbkLYojpOI) { return oLsZUMFWY += oTGlMKgAgDGeqX<oeBrbVzUjn>(OfxmFfKLbkLYojpOI); }
    template <typename oeBrbVzUjn, typename oOIRSkkgZI>
    friend oTGlMKgAgDGeqX operator-=(oTGlMKgAgDGeqX<oeBrbVzUjn> &oLsZUMFWY, const oOIRSkkgZI &OfxmFfKLbkLYojpOI) { return oLsZUMFWY -= oTGlMKgAgDGeqX<oeBrbVzUjn>(OfxmFfKLbkLYojpOI); }
    template <typename oeBrbVzUjn, typename oOIRSkkgZI>
    friend oTGlMKgAgDGeqX operator*=(oTGlMKgAgDGeqX<oeBrbVzUjn> &oLsZUMFWY, const oOIRSkkgZI &OfxmFfKLbkLYojpOI) { return oLsZUMFWY *= oTGlMKgAgDGeqX<oeBrbVzUjn>(OfxmFfKLbkLYojpOI); }
    template <typename oeBrbVzUjn, typename oOIRSkkgZI>
    friend oTGlMKgAgDGeqX operator/=(oTGlMKgAgDGeqX<oeBrbVzUjn> &oLsZUMFWY, const oOIRSkkgZI &OfxmFfKLbkLYojpOI) { return oLsZUMFWY /= oTGlMKgAgDGeqX<oeBrbVzUjn>(OfxmFfKLbkLYojpOI); }
    template <typename oeBrbVzUjn, typename oOIRSkkgZI>
    friend oTGlMKgAgDGeqX operator==(const oTGlMKgAgDGeqX<oeBrbVzUjn> &oLsZUMFWY, const oOIRSkkgZI &OfxmFfKLbkLYojpOI) { return oLsZUMFWY == oTGlMKgAgDGeqX<oeBrbVzUjn>(OfxmFfKLbkLYojpOI); }
    template <typename oeBrbVzUjn, typename oOIRSkkgZI>
    friend oTGlMKgAgDGeqX operator==(const oOIRSkkgZI &oLsZUMFWY, const oTGlMKgAgDGeqX<oeBrbVzUjn> &OfxmFfKLbkLYojpOI) { return oTGlMKgAgDGeqX<oeBrbVzUjn>(oLsZUMFWY) == OfxmFfKLbkLYojpOI; }
    template <typename oeBrbVzUjn, typename oOIRSkkgZI>
    friend oTGlMKgAgDGeqX operator!=(const oTGlMKgAgDGeqX<oeBrbVzUjn> &oLsZUMFWY, const oOIRSkkgZI &OfxmFfKLbkLYojpOI) { return oLsZUMFWY != oTGlMKgAgDGeqX<oeBrbVzUjn>(OfxmFfKLbkLYojpOI); }
    template <typename oeBrbVzUjn, typename oOIRSkkgZI>
    friend oTGlMKgAgDGeqX operator!=(const oOIRSkkgZI &oLsZUMFWY, const oTGlMKgAgDGeqX<oeBrbVzUjn> &OfxmFfKLbkLYojpOI) { return oTGlMKgAgDGeqX<oeBrbVzUjn>(oLsZUMFWY) != OfxmFfKLbkLYojpOI; }
    template <typename oeBrbVzUjn, typename oOIRSkkgZI>
    friend oTGlMKgAgDGeqX operator<=(const oTGlMKgAgDGeqX<oeBrbVzUjn> &oLsZUMFWY, const oOIRSkkgZI &OfxmFfKLbkLYojpOI) { return oLsZUMFWY <= oTGlMKgAgDGeqX<oeBrbVzUjn>(OfxmFfKLbkLYojpOI); }
    template <typename oeBrbVzUjn, typename oOIRSkkgZI>
    friend oTGlMKgAgDGeqX operator<=(const oOIRSkkgZI &oLsZUMFWY, const oTGlMKgAgDGeqX &OfxmFfKLbkLYojpOI) { return oTGlMKgAgDGeqX<oeBrbVzUjn>(oLsZUMFWY) <= OfxmFfKLbkLYojpOI; }
    template <typename oeBrbVzUjn, typename oOIRSkkgZI>
    friend oTGlMKgAgDGeqX operator>=(const oTGlMKgAgDGeqX<oeBrbVzUjn> &oLsZUMFWY, const oOIRSkkgZI &OfxmFfKLbkLYojpOI) { return oLsZUMFWY >= oTGlMKgAgDGeqX<oeBrbVzUjn>(OfxmFfKLbkLYojpOI); }
    template <typename oeBrbVzUjn, typename oOIRSkkgZI>
    friend oTGlMKgAgDGeqX operator>=(const oOIRSkkgZI &oLsZUMFWY, const oTGlMKgAgDGeqX &OfxmFfKLbkLYojpOI) { return oTGlMKgAgDGeqX<oeBrbVzUjn>(oLsZUMFWY) >= OfxmFfKLbkLYojpOI; }
    template <typename oeBrbVzUjn, typename oOIRSkkgZI>
    friend oTGlMKgAgDGeqX operator<(const oTGlMKgAgDGeqX<oeBrbVzUjn> &oLsZUMFWY, const oOIRSkkgZI &OfxmFfKLbkLYojpOI) { return oLsZUMFWY < oTGlMKgAgDGeqX<oeBrbVzUjn>(OfxmFfKLbkLYojpOI); }
    template <typename oeBrbVzUjn, typename oOIRSkkgZI>
    friend oTGlMKgAgDGeqX operator<(const oOIRSkkgZI &oLsZUMFWY, const oTGlMKgAgDGeqX &OfxmFfKLbkLYojpOI) { return oTGlMKgAgDGeqX<oeBrbVzUjn>(oLsZUMFWY) < OfxmFfKLbkLYojpOI; }
    template <typename oeBrbVzUjn, typename oOIRSkkgZI>
    friend oTGlMKgAgDGeqX operator>(const oTGlMKgAgDGeqX<oeBrbVzUjn> &oLsZUMFWY, const oOIRSkkgZI &OfxmFfKLbkLYojpOI) { return oLsZUMFWY > oTGlMKgAgDGeqX<oeBrbVzUjn>(OfxmFfKLbkLYojpOI); }
    template <typename oeBrbVzUjn, typename oOIRSkkgZI>
    friend oTGlMKgAgDGeqX operator>(const oOIRSkkgZI &oLsZUMFWY, const oTGlMKgAgDGeqX &OfxmFfKLbkLYojpOI) { return oTGlMKgAgDGeqX<oeBrbVzUjn>(oLsZUMFWY) > OfxmFfKLbkLYojpOI; }
};
template <typename OgIvtEJXJMoxxU, typename oeBrbVzUjn>
OgIvtEJXJMoxxU &operator>>(OgIvtEJXJMoxxU &owUNioikbSveL, oTGlMKgAgDGeqX<oeBrbVzUjn> &oLsZUMFWY)
{
    typename oTGlMKgAgDGeqX<oeBrbVzUjn>::OAtfPGjNXBBtbkCgG OPQctstLBKzX;
    owUNioikbSveL >> OPQctstLBKzX;
    oLsZUMFWY.value = oLsZUMFWY.oHLbOZpnthNJ(OPQctstLBKzX);
    return owUNioikbSveL;
}
template <typename oVsAVhiPtcHA, typename oeBrbVzUjn>
oVsAVhiPtcHA &operator<<(oVsAVhiPtcHA &OgFMOocVPOLyynCd, const oTGlMKgAgDGeqX<oeBrbVzUjn> &OfxmFfKLbkLYojpOI) { return OgFMOocVPOLyynCd << OfxmFfKLbkLYojpOI.value; }
using ModType = int;
constexpr ModType MOD = 1e9 + 7;
using Modular = oTGlMKgAgDGeqX<std::integral_constant<ModType, MOD>>;
// struct OXIZoxhXpmn { static ModType value; };
// ModType &Mod = OXIZoxhXpmn::value;
// using Modular = oTGlMKgAgDGeqX<OXIZoxhXpmn>;
struct Fact
{
    std::vector<Modular> ooIQXAzCHSXpzQ, oVbPPWvFWXvhS;
    const int ooJrZNWqn;
    Fact(const int &oYdwmFDhUnyf) : ooIQXAzCHSXpzQ(oYdwmFDhUnyf + 1, Modular(1)), oVbPPWvFWXvhS(oYdwmFDhUnyf + 1), ooJrZNWqn(oYdwmFDhUnyf)
    {
        ooIQXAzCHSXpzQ[0] = 1;
        for (int ouDvSloywaOn = 1; ouDvSloywaOn <= oYdwmFDhUnyf; ouDvSloywaOn++)
            ooIQXAzCHSXpzQ[ouDvSloywaOn] = ooIQXAzCHSXpzQ[ouDvSloywaOn - 1] * ouDvSloywaOn;
        oVbPPWvFWXvhS[oYdwmFDhUnyf] = Modular(1) / ooIQXAzCHSXpzQ[oYdwmFDhUnyf];
        for (int ouDvSloywaOn = oYdwmFDhUnyf; ouDvSloywaOn >= 1; ouDvSloywaOn--)
            oVbPPWvFWXvhS[ouDvSloywaOn - 1] = oVbPPWvFWXvhS[ouDvSloywaOn] * ouDvSloywaOn;
    }
    Modular C(const int &oYdwmFDhUnyf, const int &olvLvBXeX) const
    {
        if (oYdwmFDhUnyf < 0 || olvLvBXeX < 0 || oYdwmFDhUnyf < olvLvBXeX)
            return 0;
        if (oYdwmFDhUnyf > ooJrZNWqn)
            throw std::out_of_range("\u0045\u0078\u0070\u0065\u0063\u0074\u0065\u0064\u0020\u006E\u0020\u003C\u0020" + std::to_string(ooJrZNWqn) + "\u002C\u0020\u0062\u0075\u0074\u0020\u0066\u006F\u0075\u006E\u0064\u0020\u006E\u0020\u003D\u0020" + std::to_string(oYdwmFDhUnyf) + "\u002E");
        return ooIQXAzCHSXpzQ[oYdwmFDhUnyf] * oVbPPWvFWXvhS[olvLvBXeX] * oVbPPWvFWXvhS[oYdwmFDhUnyf - olvLvBXeX];
    }
    Modular A(const int &oYdwmFDhUnyf, const int &olvLvBXeX) const
    {
        if (oYdwmFDhUnyf < 0 || olvLvBXeX < 0 || oYdwmFDhUnyf < olvLvBXeX)
            return 0;
        if (oYdwmFDhUnyf > ooJrZNWqn)
            throw std::out_of_range("\u0045\u0078\u0070\u0065\u0063\u0074\u0065\u0064\u0020\u006E\u0020\u003C\u0020" + std::to_string(ooJrZNWqn) + "\u002C\u0020\u0062\u0075\u0074\u0020\u0066\u006F\u0075\u006E\u0064\u0020\u006E\u0020\u003D\u0020" + std::to_string(oYdwmFDhUnyf) + "\u002E");
        return ooIQXAzCHSXpzQ[oYdwmFDhUnyf] * oVbPPWvFWXvhS[oYdwmFDhUnyf - olvLvBXeX];
    }
};
int main()
{
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    return 0;
}