// NDEBUG-independent checks (see test_filehandler.cpp for rationale).
#include "Trie.hpp"
#include <iostream>

using namespace social;

namespace {
int failures = 0;
void check(bool condition, const char* description) {
    if (!condition) { std::cerr << "FAILED: " << description << "\n"; ++failures; }
}
}

int main() {
    Trie trie;
    trie.insert("john", 1);
    trie.insert("jane", 2);
    trie.insert("jack", 3);
    trie.insert("alice", 4);

    // Regression test for the original bug: uppercase input used to compute
    // a negative array index and read out of bounds. Must not crash, and
    // must return the same matches as the lowercase query.
    auto upper = trie.searchByPrefix("Jo");
    auto lower = trie.searchByPrefix("jo");
    check(upper == lower, "uppercase and lowercase prefixes return identical results");
    check(upper.size() == 1 && upper[0] == 1, "prefix 'Jo' matches only 'john'");

    auto ja = trie.searchByPrefix("ja");
    check(ja.size() == 2, "prefix 'ja' matches 'jane' and 'jack'");

    check(trie.searchByPrefix("zzz").empty(), "nonexistent prefix returns no matches");

    // Non-alphabetic characters must not crash or corrupt state.
    auto weird = trie.searchByPrefix("j0!@");
    check(weird.empty(), "prefix with non-alphabetic characters does not crash and returns no matches");

    auto alice = trie.searchByPrefix("alice");
    check(alice.size() == 1 && alice[0] == 4, "full username findable via prefix search");

    if (failures == 0) {
        std::cout << "All Trie tests passed.\n";
        return 0;
    }
    std::cerr << failures << " Trie test(s) failed.\n";
    return 1;
}
