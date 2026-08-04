// Social — console client
//
// Rebuilt from the original main.cpp. Behavioral scope is intentionally
// preserved (login/register, feed, posts, likes, comments, follows,
// stories, search) but every subsystem underneath is the refactored one:
// hashed auth, std::vector-backed feed, smart-pointer-owned linked lists,
// a fixed Trie, and validated input.
#include "Entities.hpp"
#include "Feed.hpp"
#include "FileHandler.hpp"
#include "Follow.hpp"
#include "DoublyLinkedList.hpp"
#include "LikeSet.hpp"
#include "PasswordHasher.hpp"
#include "Trie.hpp"

#include <chrono>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

using namespace social;

namespace {

// ---------- portable terminal + input helpers ----------
// Replaces system("cls"): that call only worked on Windows and is a
// well-known code smell (shelling out for something the terminal itself
// supports). ANSI escape codes work on Linux, macOS, and any modern
// Windows terminal (Windows Terminal, VS Code, ConEmu); a bare legacy
// cmd.exe window is the one environment it won't clear — acceptable, since
// it degrades to "doesn't clear the screen" rather than failing to compile.
void clearScreen() { std::cout << "\033[2J\033[1;1H"; }

void pause() {
    std::cout << "\nPress Enter to continue...";
    std::cin.get();
}

// Every raw `cin >> int` in the original had no failure handling, so bad
// input (e.g. typing "abc" at a numeric prompt) left the stream in a fail
// state and could cascade into an infinite loop. This is the single choke
// point all numeric input goes through instead.
int readInt(const std::string& prompt) {
    while (true) {
        std::cout << prompt;
        int value;
        if (std::cin >> value) {
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            return value;
        }
        std::cout << "Please enter a valid number.\n";
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
}

std::string readLine(const std::string& prompt) {
    std::cout << prompt;
    std::string line;
    std::getline(std::cin, line);
    return line;
}

long long nowEpochSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

// ---------- session state ----------
struct Session {
    FileHandler files;
    Trie usernameIndex;
    int currentUserId = -1;
    std::string currentUsername;

    Session() {
        for (auto& [id, username] : files.allIndexedUsers()) usernameIndex.insert(username, id);
    }
};

bool registerUser(Session& s) {
    std::string username = readLine("Choose a username: ");
    if (username.empty()) { std::cout << "Username cannot be empty.\n"; return false; }
    if (s.files.loadUserByUsername(username)) { std::cout << "Username already taken.\n"; return false; }

    std::string password = readLine("Choose a password: ");
    if (password.size() < 4) { std::cout << "Password must be at least 4 characters.\n"; return false; }

    int id = s.files.nextId("user_id");
    std::string salt = PasswordHasher::generateSalt();
    std::string hash = PasswordHasher::hashPassword(password, salt);

    if (!s.files.saveUser(UserRecord{id, username, hash, salt})) {
        std::cout << "Failed to save user.\n";
        return false;
    }
    s.files.addToSearchIndex(id, username);
    s.usernameIndex.insert(username, id);
    std::cout << "Registered! Your user ID is " << id << ". Please log in.\n";
    return true;
}

bool login(Session& s) {
    int id = readInt("User ID: ");
    std::string password = readLine("Password: ");

    auto user = s.files.loadUser(id);
    if (!user || !PasswordHasher::verify(password, user->salt, user->passwordHash)) {
        std::cout << "Invalid credentials.\n";
        return false;
    }
    s.currentUserId = user->id;
    s.currentUsername = user->username;
    std::cout << "Welcome back, " << s.currentUsername << "!\n";
    return true;
}

void createPost(Session& s) {
    std::string caption = readLine("Caption: ");
    int id = s.files.nextId("post_id");
    if (s.files.savePost(id, s.currentUserId, caption, nowEpochSeconds())) {
        std::cout << "Posted (id " << id << ").\n";
    } else {
        std::cout << "Failed to save post.\n";
    }
}

void viewFeed(Session& s) {
    Feed feed;
    for (int postId : s.files.getPostsByUser(s.currentUserId)) {
        int uid; std::string caption;
        if (s.files.loadPost(postId, uid, caption)) feed.addPost(Post(postId, uid, caption, 0));
    }
    if (feed.empty()) { std::cout << "No posts yet.\n"; return; }

    for (const auto& post : feed.all()) {
        std::cout << "[" << post.id() << "] " << post.caption()
                  << "  (likes: " << s.files.getLikeCount(post.id()) << ")\n";
    }
}

void likeOrUnlike(Session& s) {
    int postId = readInt("Post ID: ");
    if (s.files.hasLiked(postId, s.currentUserId)) {
        s.files.removeLike(postId, s.currentUserId);
        std::cout << "Unliked.\n";
    } else {
        s.files.saveLike(postId, s.currentUserId);
        std::cout << "Liked.\n";
    }
}

void addComment(Session& s) {
    int postId = readInt("Post ID: ");
    std::string text = readLine("Comment: ");
    int id = s.files.nextId("comment_id");
    s.files.saveComment(postId, CommentRecord{id, s.currentUserId, text, nowEpochSeconds()});
    std::cout << "Comment added.\n";
}

void viewComments(Session& s) {
    int postId = readInt("Post ID: ");
    // Loaded into the custom DoublyLinkedList here specifically to exercise
    // and demonstrate it — this is the "ordered, cursor-navigable
    // collection" use case it was designed for.
    DoublyLinkedList<CommentRecord> comments;
    for (auto& c : s.files.loadComments(postId)) comments.push_back(c);

    if (comments.empty()) { std::cout << "No comments.\n"; return; }
    for (const auto& c : comments) {
        std::cout << "  user " << c.userId << ": " << c.text << "\n";
    }
}

void followOrUnfollow(Session& s) {
    int targetId = readInt("User ID to follow/unfollow: ");
    if (targetId == s.currentUserId) { std::cout << "You can't follow yourself.\n"; return; }
    if (s.files.isFollowing(s.currentUserId, targetId)) {
        s.files.removeFollow(s.currentUserId, targetId);
        std::cout << "Unfollowed.\n";
    } else {
        s.files.saveFollow(s.currentUserId, targetId);
        std::cout << "Followed.\n";
    }
}

void addStory(Session& s) {
    int id = s.files.nextId("story_id");
    s.files.saveStory(s.currentUserId, StoryRecord{id, s.currentUserId, nowEpochSeconds(), false, 24.0f});
    std::cout << "Story added (expires in 24h).\n";
}

void viewStories(Session& s) {
    int userId = readInt("View stories for user ID: ");
    DoublyLinkedList<StoryRecord> stories;
    for (auto& rec : s.files.loadStories(userId)) stories.push_back(rec);

    if (stories.empty()) { std::cout << "No stories.\n"; return; }
    long long now = nowEpochSeconds();
    for (const auto& rec : stories) {
        Story story(rec.id, rec.userId, rec.timestamp, rec.expiresHours);
        std::cout << "  story " << rec.id
                  << (story.isExpired(now) ? " [expired]" : " [active]")
                  << (rec.seen ? " [seen]" : " [unseen]") << "\n";
    }
}

void searchUsers(Session& s) {
    std::string prefix = readLine("Search prefix: ");
    auto ids = s.usernameIndex.searchByPrefix(prefix);
    if (ids.empty()) { std::cout << "No matches.\n"; return; }
    for (int id : ids) {
        auto user = s.files.loadUser(id);
        if (user) std::cout << "  " << user->id << ": " << user->username << "\n";
    }
}

void mainMenu(Session& s) {
    while (true) {
        std::cout << "\n===== Social (" << s.currentUsername << ") =====\n"
                     "1. Create post   2. View my feed   3. Like/Unlike post\n"
                     "4. Comment       5. View comments   6. Follow/Unfollow\n"
                     "7. Add story     8. View stories    9. Search users\n"
                     "0. Logout\n";
        switch (readInt("Choice: ")) {
            case 1: createPost(s); break;
            case 2: viewFeed(s); break;
            case 3: likeOrUnlike(s); break;
            case 4: addComment(s); break;
            case 5: viewComments(s); break;
            case 6: followOrUnfollow(s); break;
            case 7: addStory(s); break;
            case 8: viewStories(s); break;
            case 9: searchUsers(s); break;
            case 0: s.currentUserId = -1; return;
            default: std::cout << "Invalid choice.\n";
        }
    }
}

} // namespace

int main() {
    Session session;
    while (true) {
        clearScreen();
        std::cout << "==== SOCIAL ====\n1. Login\n2. Register\n3. Exit\n";
        int choice = readInt("Choice: ");
        if (choice == 1) { if (login(session)) mainMenu(session); else pause(); }
        else if (choice == 2) { registerUser(session); pause(); }
        else if (choice == 3) break;
        else { std::cout << "Invalid choice.\n"; pause(); }
    }
    std::cout << "Goodbye.\n";
    return 0;
}
