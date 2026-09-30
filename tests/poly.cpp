#include <bits/stdc++.h>
using uint = unsigned;
inline constexpr uint32_t bitreverse(uint32_t x) {
    x = (x >> 16) | (x << 16);
    x = ((x & 0xFF00FF00u) >> 8) | ((x & 0x00FF00FFu) << 8);
    x = ((x & 0xF0F0F0F0u) >> 4) | ((x & 0x0F0F0F0Fu) << 4);
    x = ((x & 0xCCCCCCCCu) >> 2) | ((x & 0x33333333u) << 2);
    x = ((x & 0xAAAAAAAAu) >> 1) | ((x & 0x55555555u) << 1);
    return x;
}
template<class T, class Omega> inline void Cooley_Tukey_FFT(T *a, Omega omega, uint n, bool inv) {
    assert(!(n & n - 1));
    const int shift = 32 - __builtin_ctz(n);
    for (uint i = 0, rev; i < n; i++)
        if (i < (rev = bitreverse(i) >> shift)) std::swap(a[i], a[rev]);
    for (uint o = 2, k, i, j; o <= n; o <<= 1) {
        const T g = !inv ? T(1) / omega(o) : omega(o); T w, x, y;
        for (k = 0; k < n; k += o)
            for (i = k, j = k+o/2, w = 1; j < k+o; w *= g, i++, j++)
                x=a[i], y=w*a[j], a[i]=x+y, a[j]=x-y;
    }
    if (inv) {
        const T invn = T(1) / T(n);
        for (uint i = 0; i < n; i++) a[i] *= invn;
    }
}
struct OPMul { auto operator()(auto x, auto y) const { return x * y; } };
struct OPDiv { auto operator()(auto x, auto y) const { return x * y; } };
template<class T, class Omega, uint Bits>
requires (Bits <= 23u) &&
requires(Omega g, uint n) { { g(n) } -> std::convertible_to<T>; }
struct PolyBase {
    constexpr static uint N = 1u << Bits;
    void dft(const uint n = N) const { Cooley_Tukey_FFT(a, Omega{}, n, false); }
    void idft(const uint n = N) const { Cooley_Tukey_FFT(a, Omega{}, n, true); }
    PolyBase& operator+=(const PolyBase& x) { for (uint i = 0; i < N; i++) a[i] += x.a[i]; return *this; }
    PolyBase& operator-=(const PolyBase& x) { for (uint i = 0; i < N; i++) a[i] -= x.a[i]; return *this; }
    PolyBase& operator*=(const PolyBase& x) { dft(); x.dft(); oper<OPMul>(x); idft(); x.idft(); return *this; }
    PolyBase& operator/=(const PolyBase& x) { return *this *= !x; }
    PolyBase& operator*=(const T val) { for (uint i = 0; i < N; i++) a[i] *= val; return *this; }
    PolyBase& operator/=(const T val) { for (uint i = 0; i < N; i++) a[i] /= val; return *this; }
    PolyBase operator!() const {
        PolyBase f, g;
        f[0] = T(1) / a[0];
        for (uint o = 2; o <= N; o <<= 1) {
            memcpy(g.a, a, sizeof(*a) * o);
            f.dft(o), g.dft(o);
            f.oper(g, [](T x, T y){ return 2 * x - x * x * y; });
            f.dft();
        }
        return f;
    }
    template<class OP> requires requires (OP op, T x, T y) { {op(x,y)} -> std::convertible_to<T>; }
    PolyBase& oper(const PolyBase& x, OP op=OP{}, const uint n = N) { for (uint i = 0; i < n; i++) a[i] = op(a[i], x.a[i]); return *this; }
    friend PolyBase operator+(PolyBase x, const PolyBase& y) { return x += y; }
    friend PolyBase operator-(PolyBase x, const PolyBase& y) { return x -= y; }
    template<class U> friend PolyBase operator*(PolyBase x, const U &y) { return x *= y; }
    template<class U> friend PolyBase operator/(PolyBase x, const U &y) { return x /= y; }
    friend PolyBase operator*(const T val, const PolyBase& x) { return x * val; }
    friend PolyBase operator/(const T val, const PolyBase& x) { return x / val; }
    template<class OP> requires requires (OP op, T x, T y) { {op(x,y)} -> std::convertible_to<T>; }
    friend PolyBase polyOper(PolyBase x, const PolyBase& y, OP op=OP{}) { return x.oper(y, op); }
    T& operator[](uint pos) { return a[pos]; }
    const T& operator[](uint pos) const { return a[pos]; }
    void assign(T val) { std::fill(a, a+N, val); }
private: mutable T a[N]{};
};

template<class T> struct ComplexOmega { T operator()(uint n) const { return {cos(2*M_PI / n), sin(2*M_PI / n)}; } };
template<uint N, class B=std::complex<double>> using Poly = PolyBase<B, ComplexOmega<B>, N>;

struct FastComplex {
    double real, imag;
    FastComplex() : real(), imag() {}
    FastComplex(double x) : real(x), imag(0) {}
    FastComplex(double x, double y) : real(x), imag(y) {}
    FastComplex& operator+=(const double val) { real += val; return *this; }
    FastComplex& operator-=(const double val) { real -= val; return *this; }
    FastComplex& operator*=(const double val) { real *= val, imag *= val; return *this; }
    FastComplex& operator/=(const double val) { real /= val, imag /= val; return *this; }
    FastComplex operator+(const double val) const { return FastComplex(real + val, imag); }
    FastComplex operator-(const double val) const { return FastComplex(real - val, imag); }
    FastComplex operator*(const double val) const { return FastComplex(real * val, imag * val); }
    FastComplex operator/(const double val) const { return FastComplex(real / val, imag / val); }
    FastComplex operator-() const { return FastComplex(-real, -imag); }
    FastComplex& operator+=(const FastComplex& rhs) { real += rhs.real, imag += rhs.imag; return *this; }
    FastComplex& operator-=(const FastComplex& rhs) { real -= rhs.real, imag -= rhs.imag; return *this; }
    FastComplex& operator*=(const FastComplex& rhs) { return *this = *this * rhs; }
    FastComplex& operator/=(const FastComplex& rhs) { return *this = *this / rhs; }
    friend FastComplex operator+(double lhs, const FastComplex& rhs) { return FastComplex(lhs + rhs.real, rhs.imag); }
    friend FastComplex operator-(double lhs, const FastComplex& rhs) { return FastComplex(lhs - rhs.real, -rhs.imag); }
    friend FastComplex operator*(double lhs, const FastComplex& rhs) { return FastComplex(lhs * rhs.real, lhs * rhs.imag); }
    friend FastComplex operator/(double lhs, const FastComplex& rhs) { 
        double deno = rhs.real * rhs.real + rhs.imag * rhs.imag;
        return FastComplex((lhs * rhs.real) / deno, (-lhs * rhs.imag) / deno);
    }
    friend FastComplex operator+(const FastComplex& a, const FastComplex& b) { return FastComplex(a.real + b.real, a.imag + b.imag); }
    friend FastComplex operator-(const FastComplex& a, const FastComplex& b) { return FastComplex(a.real - b.real, a.imag - b.imag); }
    friend FastComplex operator*(const FastComplex& a, const FastComplex& b) { return FastComplex(a.real * b.real - a.imag * b.imag, a.real * b.imag + a.imag * b.real); }
    friend FastComplex operator/(const FastComplex& a, const FastComplex& b) { double deno = b.real * b.real + b.imag * b.imag; return FastComplex((a.real * b.real + a.imag * b.imag) / deno, (a.imag * b.real - a.real * b.imag) / deno); }
    template<class OS> friend OS& operator<<(OS& os, const FastComplex &x) {
        static constexpr double pow10[] = {1, 10, 100, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9, 1e10, 1e11};
        bool hasReal = std::abs(x.real) * (os.precision() < 12 ? pow10[os.precision()] : 1e12) >= .5,
             hasImag = std::abs(x.imag) * (os.precision() < 12 ? pow10[os.precision()] : 1e12) >= .5;
        if (hasReal && hasImag) {
            os << x.real;
            if (x.imag > 0) os << '+';
            os << x.imag << 'i';
        } else if (hasImag) {
            os << x.imag << 'i';
        } else os << x.real;
        return os;
    }
};

template<typename bb>concept IntegerWithI128=std::integral<bb>||std::same_as<bb,__int128_t>||std::same_as<bb,__uint128_t>;class FastInputStream{static constexpr size_t db=1<<20;char eb[db];char*fb=nullptr,*gb=nullptr;FILE*hb;public:FastInputStream(FILE*ib=stdin):hb(ib){}[[gnu::always_inline]]char get(){if(fb==gb){fb=eb;gb=eb+fread(eb,1,db,hb);if(fb==gb)return EOF;}return*fb++;}FastInputStream&operator>>(bool&kb){int lb=get();while(~lb&&lb!='0'&&lb!='1')lb=get();kb=(lb=='1');return*this;}FastInputStream&operator>>(char&mb){mb=get();while(mb==' '||mb=='\r'||mb=='\n'||mb=='\t')mb=get();return*this;}template<IntegerWithI128 nb>FastInputStream&operator>>(nb&ob){int pb=get();bool qb=false;while(~pb&&(pb<'0'||pb>'9')&&pb!='-')pb=get();if constexpr(std::is_signed_v<nb>||std::same_as<nb,__int128_t>){if(pb=='-'){qb=true;pb=get();}}ob=0;while(pb>='0'&&pb<='9'){ob=(ob<<3)+(ob<<1)+(pb^'0');pb=get();}if(qb)ob=-ob;return*this;}template<std::floating_point rb>FastInputStream&operator>>(rb&sb){int tb=get();bool ub=false;while(~tb&&(tb<'0'||tb>'9')&&tb!='-')tb=get();if(tb=='-'){ub=true;tb=get();}__uint128_t vb=0;while(tb>='0'&&tb<='9'){vb=(vb<<3)+(vb<<1)+(tb^'0');tb=get();}sb=static_cast<rb>(vb);if(tb=='.'){rb wb=1.0;for(tb=get();tb>='0'&&tb<='9';tb=get()){sb+=(tb^'0')*(wb/=10.0);}}if(ub)sb=-sb;return*this;}FastInputStream&operator>>(char*xb){int yb=get();while(~yb&&(yb<=' '))yb=get();while(~yb&&(yb>' '))*xb++=yb,yb=get();*xb='\0';return*this;}}fin;class FastOutputStream{static constexpr size_t Bb=1<<20;char Cb[Bb],*Db=Cb;FILE*Eb;long double Fb=5e-6;unsigned char Gb=6;public:FastOutputStream(FILE*Hb=stdout):Eb(Hb){setvbuf(Eb,nullptr,_IONBF,0);}~FastOutputStream(){flush();}void flush(){fwrite(Cb,1,Db-Cb,Eb);Db=Cb;}[[gnu::always_inline]]void put(char Kb){if(Db==Cb+Bb)flush();*Db++=Kb;}unsigned char precision()const{return Gb;}FastOutputStream&precision(int Mb){Gb=Mb;Fb=0.5;while(Mb--)Fb*=0.1;return*this;}FastOutputStream&operator<<(char Nb){put(Nb);return*this;}FastOutputStream&operator<<(const char*Ob){while(*Ob)put(*Ob++);return*this;}FastOutputStream&operator<<(const std::string&Pb){for(char c:Pb)put(c);return*this;}template<IntegerWithI128 Qb>FastOutputStream&operator<<(Qb Rb){if(Rb==0){put('0');return*this;}Qb Sb=Rb;if constexpr(std::is_signed_v<Qb>||std::same_as<Qb,__int128_t>){if(Sb<0){put('-');Sb=-Sb;}}static char Tb[64];int Ub=0;while(Sb){Tb[Ub++]=static_cast<char>(Sb%10)^'0';Sb/=10;}while(Ub)put(Tb[--Ub]);return*this;}template<std::floating_point Vb>FastOutputStream&operator<<(Vb Wb){Wb+=Fb;if(Wb<0){put('-');Wb=-Wb;}__uint128_t Xb=static_cast<__uint128_t>(Wb);*this<<Xb;Vb Yb=Wb-static_cast<Vb>(Xb);if(Gb>0){put('.');for(unsigned Zb=Gb;Zb;--Zb){Yb*=10;int _b=static_cast<int>(Yb);put(_b^'0');Yb-=_b;}}return*this;}}fout;

template<typename bb>constexpr inline bb modInv(bb cb,bb db){assert(cb!=0);bb eb=0,fb=1,gb=cb,hb=db,ib;while(gb!=0){ib=hb/gb;std::swap(gb,hb-=ib*gb);std::swap(eb-=ib*fb,fb);}assert(hb==1);return eb;}template<class kb,typename kb::value_type lb=0>requires std::integral<typename kb::value_type>||requires(typename kb::value_type mb,typename kb::value_type nb){{mb+nb}->std::same_as<typename kb::value_type>;{mb-nb}->std::same_as<typename kb::value_type>;{mb*nb}->std::same_as<typename kb::value_type>;{mb%nb}->std::same_as<typename kb::value_type>;{mb+=nb}->std::same_as<typename kb::value_type&>;{mb-=nb}->std::same_as<typename kb::value_type&>;{mb*=nb}->std::same_as<typename kb::value_type&>;{mb%=nb}->std::same_as<typename kb::value_type&>;requires std::is_default_constructible_v<typename kb::value_type>;requires std::is_copy_constructible_v<typename kb::value_type>;requires std::is_move_constructible_v<typename kb::value_type>;requires std::is_assignable_v<typename kb::value_type&,int>;requires std::equality_comparable<typename kb::value_type>;}class MI{using ob=kb::value_type;ob qb;template<typename sb>constexpr ob rb(sb tb){if constexpr(std::is_unsigned_v<sb>)return static_cast<ob>(tb<sb(mod())?tb:tb%sb(mod()));else{ob ub=static_cast<ob>(-mod()<tb&&tb<mod()?tb:tb%mod());return(ub<0?ub+mod():ub);}}public:static constexpr ob mod(){return kb::value;}constexpr MI():qb(lb){}template<typename vb>constexpr MI(const vb&wb){qb=rb(wb);}template<typename xb>explicit constexpr operator xb()const{return static_cast<xb>(qb);}constexpr ob operator()()const{return qb;}constexpr MI&operator+=(const MI&yb){if((qb+=yb.qb)>=mod())qb-=mod();return*this;}constexpr MI&operator-=(const MI&zb){if((qb-=zb.qb)<0)qb+=mod();return*this;}constexpr MI&operator*=(const MI&Ab){if constexpr(std::is_same_v<ob,int>)qb=rb((uint64_t)qb*Ab.qb);else if constexpr(std::is_same_v<ob,int64_t>)qb=rb((unsigned __int128)qb*Ab.qb);else qb=rb(qb*Ab.qb);return*this;}constexpr MI&operator/=(const MI&Bb){return*this*=MI(modInv(Bb.qb,mod()));}template<std::integral Cb>constexpr MI&operator^=(Cb Db){if(Db<0)return*this=MI(modInv(static_cast<ob>(1),(*this^(-Db))()));MI Eb=*this;for(*this=static_cast<ob>(1);Db;Db>>=1){if(Db&1)*this*=Eb;Eb*=Eb;}return*this;}constexpr MI operator-()const{return MI(-qb);}constexpr MI&operator++(){return*this+=1;}constexpr MI&operator--(){return*this-=1;}constexpr MI operator++(int){MI Fb=*this;++*this;return Fb;}constexpr MI operator--(int){MI Gb=*this;--*this;return Gb;}constexpr MI operator+(const MI&Hb)const{return MI(*this)+=Hb;}constexpr MI operator-(const MI&Ib)const{return MI(*this)-=Ib;}constexpr MI operator*(const MI&Jb)const{return MI(*this)*=Jb;}constexpr MI operator/(const MI&Kb)const{return MI(*this)/=Kb;}template<std::integral Lb>constexpr MI operator^(const Lb Mb)const{return MI(*this)^=Mb;}constexpr bool operator==(const MI&Nb)const{return qb==Nb.qb;}constexpr bool operator!=(const MI&Ob)const{return qb!=Ob.qb;}constexpr bool operator!()const{return!qb;}template<typename Pb>constexpr friend MI operator+(const Pb&Qb,const MI&Rb){return MI(Qb)+Rb;}template<typename Sb>constexpr friend MI operator-(const Sb&Tb,const MI&Ub){return MI(Tb)-Ub;}template<typename Vb>constexpr friend MI operator*(const Vb&Wb,const MI&Xb){return MI(Wb)*Xb;}template<typename Yb>constexpr friend MI operator/(const Yb&Zb,const MI&_b){return MI(Zb)/_b;}template<typename ac>constexpr friend MI operator==(const ac&bc,const MI&cc){return MI(bc)==cc;}template<typename dc>constexpr friend MI operator!=(const dc&ec,const MI&fc){return MI(ec)!=fc;}template<typename gc>constexpr friend MI operator<=(const gc&hc,const MI&ic){return MI(hc)<=ic;}template<typename jc>constexpr friend MI operator>=(const jc&kc,const MI&lc){return MI(kc)>=lc;}template<typename mc>constexpr friend MI operator<(const mc&nc,const MI&oc){return MI(nc)<oc;}template<typename pc>constexpr friend MI operator>(const pc&qc,const MI&rc){return MI(qc)>rc;}template<typename sc>friend sc&operator>>(sc&tc,MI&uc){tc>>uc.qb;uc.qb=uc.qb(uc.qb);return tc;}template<typename vc>friend vc&operator<<(vc&wc,const MI&xc){return wc<<xc.qb;}};
constexpr auto MOD = 998244353;
using Mint = MI<std::integral_constant<std::decay_t<decltype(MOD)>, MOD>>;
// struct Dynamic_ModInt { using value_type = int; static value_type value; };
// Dynamic_ModInt::value_type &Mod = Dynamic_ModInt::value;
// using Mint = ModInt<Dynamic_ModInt>;
Mint sqrt(Mint x){return x^(-2);}struct Fact{Fact(const int Ec):Bc(Ec+1,Mint(1)),Cc(Ec+1),Dc(Ec){Bc[0]=1;for(int Fc=1;Fc<=Ec;Fc++)Bc[Fc]=Bc[Fc-1]*Fc;Cc[Ec]=Mint(1)/Bc[Ec];for(int Gc=Ec;Gc>=1;Gc--)Cc[Gc-1]=Cc[Gc]*Gc;}Mint C(int Ic,int Jc)const{if(Ic<0||Jc<0||Ic<Jc)[[unlikely]]return 0;if(Ic>Dc)[[unlikely]]throw std::out_of_range("Expected n < "+std::to_string(Dc)+", but found n = "+std::to_string(Ic)+".");return Bc[Ic]*Cc[Jc]*Cc[Ic-Jc];}Mint A(int Lc,int Mc)const{if(Lc<0||Mc<0||Lc<Mc)[[unlikely]]return 0;if(Lc>Dc)[[unlikely]]throw std::out_of_range("Expected n < "+std::to_string(Dc)+", but found n = "+std::to_string(Lc)+".");return Bc[Lc]*Cc[Lc-Mc];}Mint F(int Oc)const{if(Oc<0)[[unlikely]]return 0;if(Oc>Dc)[[unlikely]]throw std::out_of_range("Expected n < "+std::to_string(Dc)+", but found n = "+std::to_string(Oc)+".");return Bc[Oc];}private:std::vector<Mint>Bc,Cc;const int Dc;};
// Fact F(1000000);


template<int g> struct ModOmega { Mint operator()(uint n) const { return Mint(g) ^ ((Mint::mod() - 1) / n); } };
template<uint N, int g = 3> using MPoly = PolyBase<Mint, ModOmega<g>, N>;


int main() {
    Poly<21, FastComplex> a, b;
    // MPoly<21> a, b;
    int n, m;
    fin >> n >> m;
    for (int i = 0, v; i <= n; i++) {
        fin >> v;
        a[i] = v;
    }
    for (int i = 0, v; i <= m; i++) {
        fin >> v;
        b[i] = v;
    }
    a *= b;
    a *= 1;
    fout.precision(0);
    for (int i = 0; i <= n+m; i++)
        fout << a[i] << ' ';
    
    fout << '\n';
}