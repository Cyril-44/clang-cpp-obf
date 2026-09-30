#include <iostream>
#include <sstream>
#include <string>
#include "ModInt.cpp"

int main() {
    int fail = 0;

    auto check = [&](const Modular &got, const Modular &exp, const std::string &name) {
        if (!(got == exp)) {
            std::cout << "FAIL: " << name << " got=" << got << " expected=" << exp << "\n";
            ++fail;
        } else {
            std::cout << "OK: " << name << "\n";
        }
    };

    // Basic arithmetic
    Modular a(2), b(3);
    check(a + b, Modular(5), "2+3");
    check(b - a, Modular(1), "3-2");
    check(a * b, Modular(6), "2*3");

    // Division
    Modular three(3), two(2);
    // expected 3 * inv(2) mod MOD = 500000005
    check(three / two, Modular(500000005), "3/2");

    // Power: 2^10 = 1024
    Modular p(2);
    p ^= 10;
    check(p, Modular(1024), "2^10");

    // modInv basic check: inv(3) * 3 == 1
    // Modular inv3(Modular::Modular_Base::Modular_Base); // dummy to silence unused warning (not used)
    Modular inv3_ok = Modular(modInv(3, MOD));
    check(inv3_ok * Modular(3), Modular(1), "modInv(3) * 3 == 1");

    // IO
    {
        std::stringstream ss;
        ss << Modular(123456789);
        Modular x;
        ss >> x;
        check(x, Modular(123456789 % MOD), "io << and >>");
    }

    // Factorials and combinations/permutations
    Fact F(10);
    check(F.C(5,2), Modular(10), "C(5,2) == 10");
    check(F.A(5,2), Modular(20), "A(5,2) == 20");

    if (fail) {
        std::cout << fail << " test(s) failed.\n";
        return 1;
    }
    std::cout << "All tests passed.\n";
    return 0;
}