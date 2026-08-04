// Uses a NDEBUG-independent check() rather than assert() — tests must fail
// loudly in Release builds too, not compile away to nothing.
#include "FileHandler.hpp"
#include "PasswordHasher.hpp"
#include <filesystem>
#include <iostream>

using namespace social;

namespace {
int failures = 0;
void check(bool condition, const char* description) {
    if (!condition) {
        std::cerr << "FAILED: " << description << "\n";
        ++failures;
    }
}
}

int main() {
    std::filesystem::remove_all("/tmp/social_test_data_ctest");
    FileHandler fh("/tmp/social_test_data_ctest");

    // user round trip with hashed password
    std::string salt = PasswordHasher::generateSalt();
    std::string hash = PasswordHasher::hashPassword("pass123", salt);
    UserRecord u{1, "john", hash, salt};
    check(fh.saveUser(u), "saveUser succeeds");
    auto loaded = fh.loadUser(1);
    check(loaded.has_value(), "loadUser finds saved user");
    check(loaded->username == "john", "username round-trips");
    check(PasswordHasher::verify("pass123", loaded->salt, loaded->passwordHash), "correct password verifies");
    check(!PasswordHasher::verify("wrongpass", loaded->salt, loaded->passwordHash), "wrong password is rejected");

    // caption with embedded newline survives round trip (regression test for
    // the original format, which silently corrupted multi-line captions)
    check(fh.savePost(101, 1, "line1\nline2", 12345), "savePost succeeds");
    int uid = -1;
    std::string cap;
    check(fh.loadPost(101, uid, cap), "loadPost succeeds");
    check(uid == 1, "post user_id round-trips");
    check(cap == "line1\nline2", "caption with embedded newline round-trips exactly");

    // counters increment
    int id1 = fh.nextId("post_id");
    int id2 = fh.nextId("post_id");
    check(id2 == id1 + 1, "nextId increments monotonically");

    // follows
    check(fh.saveFollow(1, 2), "saveFollow succeeds");
    check(fh.isFollowing(1, 2), "isFollowing true after follow");
    check(!fh.isFollowing(2, 1), "isFollowing false for reverse direction");
    check(fh.removeFollow(1, 2), "removeFollow succeeds");
    check(!fh.isFollowing(1, 2), "isFollowing false after unfollow");

    // likes
    check(fh.saveLike(101, 5), "saveLike succeeds");
    check(fh.hasLiked(101, 5), "hasLiked true after like");
    check(fh.getLikeCount(101) == 1, "like count is 1");
    check(fh.removeLike(101, 5), "removeLike succeeds");
    check(fh.getLikeCount(101) == 0, "like count is 0 after unlike");

    if (failures == 0) {
        std::cout << "All FileHandler tests passed.\n";
        return 0;
    }
    std::cerr << failures << " FileHandler test(s) failed.\n";
    return 1;
}
