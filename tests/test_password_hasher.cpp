// NDEBUG-independent checks (see test_filehandler.cpp for rationale).
#include "PasswordHasher.hpp"
#include <iostream>

using namespace social;

namespace {
int failures = 0;
void check(bool condition, const char* description) {
    if (!condition) { std::cerr << "FAILED: " << description << "\n"; ++failures; }
}
}

int main() {
    // Verified against the official NIST SHA-256 test vector for "abc".
    check(Sha256::hash("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
          "SHA-256('abc') matches the official NIST test vector");

    std::string salt = PasswordHasher::generateSalt();
    std::string hash = PasswordHasher::hashPassword("pass123", salt);
    check(PasswordHasher::verify("pass123", salt, hash), "correct password verifies");
    check(!PasswordHasher::verify("wrongpass", salt, hash), "wrong password is rejected");

    // Different salts must produce different hashes for the same password —
    // the entire point of salting is defeating precomputed rainbow tables
    // across users who reuse the same password.
    std::string salt2 = PasswordHasher::generateSalt();
    std::string hash2 = PasswordHasher::hashPassword("pass123", salt2);
    check(salt != salt2, "generateSalt produces distinct salts across calls");
    check(hash != hash2, "same password with different salts produces different hashes");

    if (failures == 0) {
        std::cout << "All PasswordHasher tests passed.\n";
        return 0;
    }
    std::cerr << failures << " PasswordHasher test(s) failed.\n";
    return 1;
}
