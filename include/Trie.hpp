#pragma once
//
// Trie.hpp
// --------
// Prefix tree for O(prefix length) username lookup / autocomplete.
//
// Bug fixed from the original (previous review, item #2, verified with a
// reproducing test in tests/test_trie.cpp): insertUser() lowercased every
// character before indexing, but searchPrefix() did not, so searching an
// uppercase prefix ("John") computed a negative array index
// ('J' - 'a' == -32) and read out of bounds — undefined behavior that can
// crash or corrupt memory. Both paths now go through the same
// charToIndex() normalization, and any character outside a-z is rejected
// up front instead of silently producing a bad index.
//
// Complexity: insert O(L), search O(L + k) where L = prefix length,
// k = number of matches returned. Space: O(total characters across all
// inserted usernames) in the worst case (no shared prefixes).
//
#include <array>
#include <cctype>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace social {

class Trie {
public:
    Trie() : root_(std::make_unique<Node>()) {}

    void insert(const std::string& username, int userId) {
        Node* cur = root_.get();
        for (char ch : username) {
            auto idx = charToIndex(ch);
            if (!idx) continue; // skip non a-z characters rather than corrupting state
            if (!cur->children[*idx]) cur->children[*idx] = std::make_unique<Node>();
            cur = cur->children[*idx].get();
        }
        cur->isEnd = true;
        cur->userId = userId;
    }

    [[nodiscard]] std::vector<int> searchByPrefix(const std::string& prefix) const {
        Node* cur = root_.get();
        for (char ch : prefix) {
            auto idx = charToIndex(ch);
            if (!idx || !cur->children[*idx]) return {};
            cur = cur->children[*idx].get();
        }
        std::vector<int> results;
        collect(cur, results);
        return results;
    }

private:
    struct Node {
        std::array<std::unique_ptr<Node>, 26> children;
        bool isEnd = false;
        int userId = -1;
    };

    std::unique_ptr<Node> root_;

    // Single source of truth for char->index normalization, used by both
    // insert and search so they can never disagree again.
    static std::optional<int> charToIndex(char ch) {
        char lower = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        if (lower < 'a' || lower > 'z') return std::nullopt;
        return lower - 'a';
    }

    static void collect(const Node* node, std::vector<int>& out) {
        if (!node) return;
        if (node->isEnd) out.push_back(node->userId);
        for (const auto& child : node->children) collect(child.get(), out);
    }
};

} // namespace social
