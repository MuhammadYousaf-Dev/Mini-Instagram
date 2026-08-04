#pragma once
//
// FileHandler.hpp
// ----------------
// Simple flat-file persistence layer.
//
// Changes from the original (previous review items #1, #7):
//   * std::filesystem::create_directories replaces the Windows-only
//     <direct.h> _mkdir call, so directory creation now compiles and
//     works identically on Windows, Linux, and macOS with zero #ifdefs.
//   * Passwords are never written in plaintext: callers must pass an
//     already-hashed password + salt (see PasswordHasher.hpp). This class
//     has no knowledge of raw passwords at all, so it's structurally
//     impossible to accidentally persist one.
//   * Free-text fields (captions, comment text) are newline-escaped
//     before writing, since the original line-based format silently
//     corrupted data if a caption contained a newline.
//
// This remains an intentionally simple flat-file store (readable, greppable,
// good for a learning project) rather than a database. docs/ARCHITECTURE.md
// explains the tradeoff and what a production version would use instead.
//
#include <filesystem>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

namespace social {

struct UserRecord {
    int id;
    std::string username;
    std::string passwordHash;
    std::string salt;
};

struct CommentRecord {
    int id;
    int userId;
    std::string text;
    long long timestamp;
};

struct StoryRecord {
    int id;
    int userId;
    long long timestamp;
    bool seen;
    float expiresHours;
};

class FileHandler {
public:
    explicit FileHandler(std::filesystem::path dataFolder = "data");

    // ---- users ----
    bool saveUser(const UserRecord& user);
    std::optional<UserRecord> loadUser(int userId);
    std::optional<UserRecord> loadUserByUsername(const std::string& username);
    bool userExists(int userId) const;
    std::vector<int> getAllUserIds() const;

    // ---- posts ----
    bool savePost(int postId, int userId, const std::string& caption, long long timestamp);
    bool loadPost(int postId, int& userId, std::string& caption) const;
    std::vector<int> getPostsByUser(int userId) const;

    // ---- comments ----
    bool saveComment(int postId, const CommentRecord& comment);
    std::vector<CommentRecord> loadComments(int postId) const;

    // ---- likes ----
    bool saveLike(int postId, int userId);
    bool removeLike(int postId, int userId);
    bool hasLiked(int postId, int userId) const;
    int getLikeCount(int postId) const;

    // ---- follows ----
    bool saveFollow(int followerId, int followingId);
    bool removeFollow(int followerId, int followingId);
    bool isFollowing(int followerId, int followingId) const;
    std::vector<int> getFollowers(int userId) const;
    std::vector<int> getFollowing(int userId) const;

    // ---- stories ----
    bool saveStory(int userId, const StoryRecord& story);
    std::vector<StoryRecord> loadStories(int userId) const;

    // ---- feed ----
    bool addToFeed(int userId, int postId);
    std::vector<int> getFeedPosts(int userId) const;

    // ---- search index ----
    bool addToSearchIndex(int userId, const std::string& username);
    std::vector<std::pair<int, std::string>> allIndexedUsers() const;

    // ---- id generation ----
    int nextId(const std::string& counterKey);

private:
    std::filesystem::path dataFolder_;

    std::filesystem::path userPath(int id) const;
    std::filesystem::path postPath(int id) const;
    std::filesystem::path commentPath(int postId) const;
    std::filesystem::path likePath(int postId) const;
    std::filesystem::path followersPath(int userId) const;
    std::filesystem::path followingPath(int userId) const;
    std::filesystem::path storyPath(int userId) const;
    std::filesystem::path feedPath(int userId) const;
    std::filesystem::path searchIndexPath() const;
    std::filesystem::path countersPath() const;

    static std::string escape(const std::string& text);
    static std::string unescape(const std::string& text);
};

} // namespace social
