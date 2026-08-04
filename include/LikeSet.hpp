#pragma once
// LikeSet.hpp — same rationale as Follow.hpp: logic was already sound
// (O(1) average via unordered_set), just removing the header-level
// `using namespace std;` and renaming from `like` (a lowercase class name
// that collides with common variable-naming conventions and reads like a
// keyword) to `LikeSet`.
#include <unordered_set>

namespace social {

class LikeSet {
public:
    void like(int userId) { likedBy_.insert(userId); }
    void unlike(int userId) { likedBy_.erase(userId); }
    [[nodiscard]] bool isLikedBy(int userId) const { return likedBy_.count(userId) > 0; }
    [[nodiscard]] std::size_t totalLikes() const noexcept { return likedBy_.size(); }

private:
    std::unordered_set<int> likedBy_;
};

} // namespace social
