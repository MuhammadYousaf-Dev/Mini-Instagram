#pragma once
//
// Entities.hpp
// ------------
// Plain-data value types. Deliberately "dumb": no ownership of other
// entities, no dynamic arrays living inside them. That responsibility
// belongs to containers (see DoublyLinkedList.hpp, and std::vector usage
// in Feed / PostRepository).
//
// Why this matters (previous review, item #1): the old `post` class stored
// a `post* posts` array of itself, so a single post and "all posts" were
// the same type. That's what caused the shallow-copy corruption bug on
// resize. Splitting entity vs. container removes the bug class entirely
// rather than patching around it.
//
// Why std::string instead of char[N] (previous review, item #3, #8):
// std::string owns its own buffer, grows safely, and makes strcpy-based
// buffer overflows structurally impossible — there is no fixed size to
// overflow.
//
#include <cstdint>
#include <string>

namespace social {

class Post {
public:
    Post() = default;
    Post(int id, int userId, std::string caption, std::int64_t timestamp)
        : id_(id), userId_(userId), caption_(std::move(caption)), timestamp_(timestamp) {}

    [[nodiscard]] int id() const noexcept { return id_; }
    [[nodiscard]] int userId() const noexcept { return userId_; }
    [[nodiscard]] const std::string& caption() const noexcept { return caption_; }
    [[nodiscard]] std::int64_t timestamp() const noexcept { return timestamp_; }

private:
    int id_ = 0;
    int userId_ = 0;
    std::string caption_;
    std::int64_t timestamp_ = 0;
};

class Comment {
public:
    Comment() = default;
    Comment(int id, int postId, int userId, std::string text, std::int64_t timestamp)
        : id_(id), postId_(postId), userId_(userId), text_(std::move(text)), timestamp_(timestamp) {}

    [[nodiscard]] int id() const noexcept { return id_; }
    [[nodiscard]] int postId() const noexcept { return postId_; }
    [[nodiscard]] int userId() const noexcept { return userId_; }
    [[nodiscard]] const std::string& text() const noexcept { return text_; }
    [[nodiscard]] std::int64_t timestamp() const noexcept { return timestamp_; }

private:
    int id_ = 0;
    int postId_ = 0;
    int userId_ = 0;
    std::string text_;
    std::int64_t timestamp_ = 0;
};

class Story {
public:
    Story() = default;
    Story(int id, int userId, std::int64_t timestamp, float expiresHours)
        : id_(id), userId_(userId), timestamp_(timestamp), expiresHours_(expiresHours) {}

    [[nodiscard]] int id() const noexcept { return id_; }
    [[nodiscard]] int userId() const noexcept { return userId_; }
    [[nodiscard]] std::int64_t timestamp() const noexcept { return timestamp_; }
    [[nodiscard]] float expiresHours() const noexcept { return expiresHours_; }
    [[nodiscard]] bool isSeen() const noexcept { return seen_; }
    void markSeen() noexcept { seen_ = true; }

    // A story is expired once `expiresHours_` hours have elapsed since posting.
    [[nodiscard]] bool isExpired(std::int64_t nowEpochSeconds) const noexcept {
        const double elapsedHours = static_cast<double>(nowEpochSeconds - timestamp_) / 3600.0;
        return elapsedHours >= expiresHours_;
    }

private:
    int id_ = 0;
    int userId_ = 0;
    std::int64_t timestamp_ = 0;
    float expiresHours_ = 24.0f;
    bool seen_ = false;
};

class User {
public:
    User() = default;
    User(int id, std::string username, std::string passwordHash, std::string salt)
        : id_(id), username_(std::move(username)),
          passwordHash_(std::move(passwordHash)), salt_(std::move(salt)) {}

    [[nodiscard]] int id() const noexcept { return id_; }
    [[nodiscard]] const std::string& username() const noexcept { return username_; }
    [[nodiscard]] const std::string& passwordHash() const noexcept { return passwordHash_; }
    [[nodiscard]] const std::string& salt() const noexcept { return salt_; }

private:
    int id_ = 0;
    std::string username_;
    std::string passwordHash_; // never store the raw password (see PasswordHasher.hpp)
    std::string salt_;
};

} // namespace social
