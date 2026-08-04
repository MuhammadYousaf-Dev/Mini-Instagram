#include "FileHandler.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <unordered_set>

namespace social {

namespace fs = std::filesystem;

FileHandler::FileHandler(fs::path dataFolder) : dataFolder_(std::move(dataFolder)) {
    for (const char* sub : {"users", "posts", "comments", "likes", "follows", "stories", "feeds", "search"}) {
        fs::create_directories(dataFolder_ / sub);
    }
    if (!fs::exists(countersPath())) {
        std::ofstream f(countersPath());
        f << "user_id:1\npost_id:1\ncomment_id:1\nstory_id:1\n";
    }
}

fs::path FileHandler::userPath(int id) const { return dataFolder_ / "users" / (std::to_string(id) + ".txt"); }
fs::path FileHandler::postPath(int id) const { return dataFolder_ / "posts" / (std::to_string(id) + ".txt"); }
fs::path FileHandler::commentPath(int postId) const { return dataFolder_ / "comments" / (std::to_string(postId) + ".txt"); }
fs::path FileHandler::likePath(int postId) const { return dataFolder_ / "likes" / (std::to_string(postId) + ".txt"); }
fs::path FileHandler::followersPath(int userId) const { return dataFolder_ / "follows" / (std::to_string(userId) + "_followers.txt"); }
fs::path FileHandler::followingPath(int userId) const { return dataFolder_ / "follows" / (std::to_string(userId) + "_following.txt"); }
fs::path FileHandler::storyPath(int userId) const { return dataFolder_ / "stories" / (std::to_string(userId) + ".txt"); }
fs::path FileHandler::feedPath(int userId) const { return dataFolder_ / "feeds" / (std::to_string(userId) + ".txt"); }
fs::path FileHandler::searchIndexPath() const { return dataFolder_ / "search" / "index.txt"; }
fs::path FileHandler::countersPath() const { return dataFolder_ / "counters.txt"; }

std::string FileHandler::escape(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (char c : text) {
        if (c == '\n') out += "\\n";
        else if (c == '\\') out += "\\\\";
        else out += c;
    }
    return out;
}

std::string FileHandler::unescape(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\\' && i + 1 < text.size()) {
            if (text[i+1] == 'n') { out += '\n'; ++i; continue; }
            if (text[i+1] == '\\') { out += '\\'; ++i; continue; }
        }
        out += text[i];
    }
    return out;
}

// ---------------- users ----------------

bool FileHandler::saveUser(const UserRecord& user) {
    std::ofstream f(userPath(user.id));
    if (!f.is_open()) return false;
    f << "id:" << user.id << '\n'
      << "username:" << escape(user.username) << '\n'
      << "password_hash:" << user.passwordHash << '\n'
      << "salt:" << user.salt << '\n';
    return true;
}

std::optional<UserRecord> FileHandler::loadUser(int userId) {
    std::ifstream f(userPath(userId));
    if (!f.is_open()) return std::nullopt;

    UserRecord rec{};
    rec.id = userId;
    std::string line;
    while (std::getline(f, line)) {
        auto pos = line.find(':');
        if (pos == std::string::npos) continue;
        std::string key = line.substr(0, pos), value = line.substr(pos + 1);
        if (key == "username") rec.username = unescape(value);
        else if (key == "password_hash") rec.passwordHash = value;
        else if (key == "salt") rec.salt = value;
    }
    return rec;
}

std::optional<UserRecord> FileHandler::loadUserByUsername(const std::string& username) {
    for (int id : getAllUserIds()) {
        auto rec = loadUser(id);
        if (rec && rec->username == username) return rec;
    }
    return std::nullopt;
}

bool FileHandler::userExists(int userId) const { return fs::exists(userPath(userId)); }

std::vector<int> FileHandler::getAllUserIds() const {
    std::vector<int> ids;
    if (!fs::exists(dataFolder_ / "users")) return ids;
    for (const auto& entry : fs::directory_iterator(dataFolder_ / "users")) {
        if (!entry.is_regular_file()) continue;
        try {
            ids.push_back(std::stoi(entry.path().stem().string()));
        } catch (const std::exception&) {
            // Skip files that don't match the expected "<id>.txt" naming scheme.
        }
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

// ---------------- posts ----------------

bool FileHandler::savePost(int postId, int userId, const std::string& caption, long long timestamp) {
    std::ofstream f(postPath(postId));
    if (!f.is_open()) return false;
    f << "id:" << postId << '\n' << "user_id:" << userId << '\n'
      << "caption:" << escape(caption) << '\n' << "timestamp:" << timestamp << '\n';
    return true;
}

bool FileHandler::loadPost(int postId, int& userId, std::string& caption) const {
    std::ifstream f(postPath(postId));
    if (!f.is_open()) return false;
    std::string line;
    while (std::getline(f, line)) {
        auto pos = line.find(':');
        if (pos == std::string::npos) continue;
        std::string key = line.substr(0, pos), value = line.substr(pos + 1);
        if (key == "user_id") userId = std::stoi(value);
        else if (key == "caption") caption = unescape(value);
    }
    return true;
}

std::vector<int> FileHandler::getPostsByUser(int userId) const {
    std::vector<int> ids;
    if (!fs::exists(dataFolder_ / "posts")) return ids;
    for (const auto& entry : fs::directory_iterator(dataFolder_ / "posts")) {
        if (!entry.is_regular_file()) continue;
        int uid = -1;
        std::string caption;
        int postId = -1;
        try { postId = std::stoi(entry.path().stem().string()); } catch (...) { continue; }
        if (loadPost(postId, uid, caption) && uid == userId) ids.push_back(postId);
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

// ---------------- comments ----------------

bool FileHandler::saveComment(int postId, const CommentRecord& comment) {
    std::ofstream f(commentPath(postId), std::ios::app);
    if (!f.is_open()) return false;
    f << comment.id << '|' << comment.userId << '|' << escape(comment.text) << '|' << comment.timestamp << '\n';
    return true;
}

std::vector<CommentRecord> FileHandler::loadComments(int postId) const {
    std::vector<CommentRecord> out;
    std::ifstream f(commentPath(postId));
    std::string line;
    while (std::getline(f, line)) {
        std::stringstream ss(line);
        std::string idStr, uidStr, text, tsStr;
        std::getline(ss, idStr, '|');
        std::getline(ss, uidStr, '|');
        std::getline(ss, text, '|');
        std::getline(ss, tsStr, '|');
        if (idStr.empty()) continue;
        out.push_back(CommentRecord{std::stoi(idStr), std::stoi(uidStr), unescape(text), std::stoll(tsStr)});
    }
    return out;
}

// ---------------- likes ----------------

bool FileHandler::saveLike(int postId, int userId) {
    if (hasLiked(postId, userId)) return true;
    std::ofstream f(likePath(postId), std::ios::app);
    if (!f.is_open()) return false;
    f << userId << '\n';
    return true;
}

bool FileHandler::removeLike(int postId, int userId) {
    auto path = likePath(postId);
    std::ifstream in(path);
    std::vector<int> remaining;
    int uid;
    while (in >> uid) if (uid != userId) remaining.push_back(uid);
    in.close();
    std::ofstream out(path, std::ios::trunc);
    for (int id : remaining) out << id << '\n';
    return true;
}

bool FileHandler::hasLiked(int postId, int userId) const {
    std::ifstream f(likePath(postId));
    int uid;
    while (f >> uid) if (uid == userId) return true;
    return false;
}

int FileHandler::getLikeCount(int postId) const {
    std::ifstream f(likePath(postId));
    int uid, count = 0;
    while (f >> uid) ++count;
    return count;
}

// ---------------- follows ----------------

bool FileHandler::saveFollow(int followerId, int followingId) {
    if (isFollowing(followerId, followingId)) return true;
    { std::ofstream f(followingPath(followerId), std::ios::app); f << followingId << '\n'; }
    { std::ofstream f(followersPath(followingId), std::ios::app); f << followerId << '\n'; }
    return true;
}

static void removeIdFromFile(const fs::path& path, int idToRemove) {
    std::ifstream in(path);
    std::vector<int> remaining;
    int id;
    while (in >> id) if (id != idToRemove) remaining.push_back(id);
    in.close();
    std::ofstream out(path, std::ios::trunc);
    for (int id2 : remaining) out << id2 << '\n';
}

bool FileHandler::removeFollow(int followerId, int followingId) {
    removeIdFromFile(followingPath(followerId), followingId);
    removeIdFromFile(followersPath(followingId), followerId);
    return true;
}

bool FileHandler::isFollowing(int followerId, int followingId) const {
    std::ifstream f(followingPath(followerId));
    int id;
    while (f >> id) if (id == followingId) return true;
    return false;
}

std::vector<int> FileHandler::getFollowers(int userId) const {
    std::vector<int> out;
    std::ifstream f(followersPath(userId));
    int id;
    while (f >> id) out.push_back(id);
    return out;
}

std::vector<int> FileHandler::getFollowing(int userId) const {
    std::vector<int> out;
    std::ifstream f(followingPath(userId));
    int id;
    while (f >> id) out.push_back(id);
    return out;
}

// ---------------- stories ----------------

bool FileHandler::saveStory(int userId, const StoryRecord& story) {
    std::ofstream f(storyPath(userId), std::ios::app);
    if (!f.is_open()) return false;
    f << story.id << '|' << story.timestamp << '|' << (story.seen ? 1 : 0) << '|' << story.expiresHours << '\n';
    return true;
}

std::vector<StoryRecord> FileHandler::loadStories(int userId) const {
    std::vector<StoryRecord> out;
    std::ifstream f(storyPath(userId));
    std::string line;
    while (std::getline(f, line)) {
        std::stringstream ss(line);
        std::string idStr, tsStr, seenStr, expStr;
        std::getline(ss, idStr, '|');
        std::getline(ss, tsStr, '|');
        std::getline(ss, seenStr, '|');
        std::getline(ss, expStr, '|');
        if (idStr.empty()) continue;
        out.push_back(StoryRecord{std::stoi(idStr), userId, std::stoll(tsStr), seenStr == "1", std::stof(expStr)});
    }
    return out;
}

// ---------------- feed ----------------

bool FileHandler::addToFeed(int userId, int postId) {
    std::ofstream f(feedPath(userId), std::ios::app);
    if (!f.is_open()) return false;
    f << postId << '\n';
    return true;
}

std::vector<int> FileHandler::getFeedPosts(int userId) const {
    std::vector<int> out;
    std::ifstream f(feedPath(userId));
    int id;
    while (f >> id) out.push_back(id);
    return out;
}

// ---------------- search index ----------------

bool FileHandler::addToSearchIndex(int userId, const std::string& username) {
    std::ofstream f(searchIndexPath(), std::ios::app);
    if (!f.is_open()) return false;
    f << userId << '|' << escape(username) << '\n';
    return true;
}

std::vector<std::pair<int, std::string>> FileHandler::allIndexedUsers() const {
    std::vector<std::pair<int, std::string>> out;
    std::ifstream f(searchIndexPath());
    std::string line;
    while (std::getline(f, line)) {
        auto pos = line.find('|');
        if (pos == std::string::npos) continue;
        try {
            out.emplace_back(std::stoi(line.substr(0, pos)), unescape(line.substr(pos + 1)));
        } catch (...) {}
    }
    return out;
}

// ---------------- counters ----------------

int FileHandler::nextId(const std::string& counterKey) {
    std::ifstream in(countersPath());
    std::vector<std::pair<std::string, int>> counters;
    std::string line;
    int result = 1;
    while (std::getline(in, line)) {
        auto pos = line.find(':');
        if (pos == std::string::npos) continue;
        std::string key = line.substr(0, pos);
        int value = std::stoi(line.substr(pos + 1));
        if (key == counterKey) { result = value; value += 1; }
        counters.emplace_back(key, value);
    }
    in.close();

    bool found = false;
    for (auto& [key, value] : counters) if (key == counterKey) found = true;
    if (!found) counters.emplace_back(counterKey, result + 1);

    std::ofstream out(countersPath(), std::ios::trunc);
    for (auto& [key, value] : counters) out << key << ':' << value << '\n';
    return result;
}

} // namespace social
