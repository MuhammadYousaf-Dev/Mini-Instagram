#pragma once
//
// Feed.hpp
// --------
// Previously: a hand-rolled dynamic array of `post` objects living inside
// `Feed`, doubling capacity manually and copying elements with `newArr[i]
// = posts[i]` — the exact pattern that caused the shallow-copy corruption
// bug (previous review, items #1 and #5).
//
// Now: std::vector<Post>. This is a deliberate, honest engineering choice
// worth explaining rather than hiding: reimplementing a dynamic array by
// hand adds real risk (manual capacity doubling, manual copying, manual
// cleanup) for zero benefit once std::vector already does it correctly,
// exception-safely, and with move semantics. A senior engineer would flag
// "I reinvented std::vector, badly" as the single highest-priority fix in
// this codebase — so that's what changed. The custom, hand-written data
// structures that *do* remain (Trie, DoublyLinkedList) are the ones that
// std::vector doesn't already provide, which is where writing your own
// implementation actually demonstrates DSA understanding.
//
// Complexity: push_back amortized O(1); scrolling current index O(1).
//
#include "Entities.hpp"
#include <vector>

namespace social {

class Feed {
public:
    void addPost(Post post) { posts_.push_back(std::move(post)); if (current_ < 0) current_ = 0; }

    [[nodiscard]] std::size_t size() const noexcept { return posts_.size(); }
    [[nodiscard]] bool empty() const noexcept { return posts_.empty(); }

    void next() { if (current_ >= 0 && current_ + 1 < static_cast<int>(posts_.size())) ++current_; }
    void previous() { if (current_ > 0) --current_; }

    [[nodiscard]] const Post* current() const {
        if (current_ < 0 || current_ >= static_cast<int>(posts_.size())) return nullptr;
        return &posts_[current_];
    }

    [[nodiscard]] const std::vector<Post>& all() const noexcept { return posts_; }

private:
    std::vector<Post> posts_;
    int current_ = -1;
};

} // namespace social
