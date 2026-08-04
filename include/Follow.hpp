#pragma once
// Follow.hpp
// The original version of this file was already reasonably clean
// (unordered_set-based, O(1) average follow/unfollow/check). The only
// change here is removing `using namespace std;` from the header, which
// was leaking into every translation unit that included it (previous
// review, item #3).
#include <unordered_set>

namespace social {

class FollowSet {
public:
    void follow(int userId) { following_.insert(userId); }
    void unfollow(int userId) { following_.erase(userId); }
    [[nodiscard]] bool isFollowing(int userId) const { return following_.count(userId) > 0; }
    [[nodiscard]] std::size_t totalFollowing() const noexcept { return following_.size(); }

private:
    std::unordered_set<int> following_;
};

} // namespace social
