typedef __int128_t lll;
typedef unsigned long long ull;
struct FastMod{ull b, m;
	inline void init(ull x){b = x, m = ((lll)1 << 64) / b;}
	inline ull Mod(ull a){
		ull q = ull((lll)m * a >> 64);
		ull r = a - q * b;
		return r >= b ? r - b : r;
	}
}md;
