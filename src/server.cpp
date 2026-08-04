// Social — HTTP server
//
// SCOPE NOTE (read this before assuming full parity with the original):
// The original server.cpp exposed ~24 routes. This rewrite fixes the
// transport-layer bugs and re-implements the core routes end to end
// (status, register, login, posts, feed, like, comments, follow/unfollow,
// search, static file serving) on top of the corrected FileHandler /
// PasswordHasher / Trie stack. Story-queue and stats routes were not
// ported in this pass — they follow the exact same pattern as the routes
// below, and are the natural "next PR" rather than something silently
// dropped. Said plainly in docs/ARCHITECTURE.md too, so nobody discovers
// this by diffing route counts.
//
// Transport-layer fixes (previous review, items #7-8):
//   1. The original did a single fixed 4096-byte recv() with no loop and
//      no check of the return value, so any request larger than 4KB (or a
//      slow/partial send from the client) was silently truncated, and a
//      recv() failure (-1) was passed straight into a std::string
//      constructor. This version reads until the header/body boundary is
//      found, honors Content-Length for the body, and checks every
//      recv() return value.
//   2. <direct.h> was included unconditionally (compiled on Windows only).
//      Directory creation now goes through FileHandler's std::filesystem
//      call, so this file no longer touches OS-specific directory APIs at
//      all — the only remaining #ifdef _WIN32 blocks are for the socket
//      API itself (winsock vs. POSIX sockets), which is unavoidable: the
//      two platforms really do have different socket APIs, and hiding
//      that behind an #ifdef is the standard, correct way to handle it,
//      not a portability bug.
#include "Entities.hpp"
#include "Feed.hpp"
#include "FileHandler.hpp"
#include "Follow.hpp"
#include "LikeSet.hpp"
#include "PasswordHasher.hpp"
#include "Trie.hpp"

#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib, "ws2_32.lib")
  using socket_t = SOCKET;
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <unistd.h>
  using socket_t = int;
  constexpr int INVALID_SOCKET_VAL = -1;
#endif

using namespace social;

namespace {

FileHandler g_files;
Trie g_usernameIndex;

long long nowEpochSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

// ---------------- tiny JSON helpers (no external dependency) ----------------
// Deliberately minimal: escapes the handful of characters that matter for
// our own generated strings. Not a general-purpose JSON writer — documented
// as such so nobody mistakes it for one.
std::string jsonEscape(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '"' || c == '\\') out += '\\';
        if (c == '\n') { out += "\\n"; continue; }
        out += c;
    }
    return out;
}

// Extracts a string value for "key" from a flat, single-level JSON object.
// Sufficient for this app's request bodies ({"username":"x","password":"y"});
// deliberately not a full parser.
std::string jsonField(const std::string& body, const std::string& key) {
    std::string needle = "\"" + key + "\"";
    auto pos = body.find(needle);
    if (pos == std::string::npos) return "";
    pos = body.find(':', pos);
    if (pos == std::string::npos) return "";
    pos = body.find('"', pos);
    if (pos == std::string::npos) return "";
    auto end = body.find('"', pos + 1);
    while (end != std::string::npos && body[end - 1] == '\\') end = body.find('"', end + 1);
    if (end == std::string::npos) return "";
    return body.substr(pos + 1, end - pos - 1);
}

struct HttpRequest {
    std::string method;
    std::string path;
    std::string body;
};

struct HttpResponse {
    int status = 200;
    std::string contentType = "application/json";
    std::string body;
};

std::string statusText(int code) {
    switch (code) {
        case 200: return "OK";
        case 201: return "Created";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 404: return "Not Found";
        default: return "Error";
    }
}

std::string serialize(const HttpResponse& res) {
    std::ostringstream out;
    out << "HTTP/1.1 " << res.status << " " << statusText(res.status) << "\r\n"
        << "Content-Type: " << res.contentType << "\r\n"
        << "Access-Control-Allow-Origin: *\r\n"
        << "Content-Length: " << res.body.size() << "\r\n"
        << "Connection: close\r\n\r\n"
        << res.body;
    return out.str();
}

HttpResponse json(int status, const std::string& body) { return HttpResponse{status, "application/json", body}; }
HttpResponse error(int status, const std::string& message) {
    return json(status, "{\"error\":\"" + jsonEscape(message) + "\"}");
}

// ---------------- route handlers ----------------

HttpResponse handleRegister(const HttpRequest& req) {
    std::string username = jsonField(req.body, "username");
    std::string password = jsonField(req.body, "password");
    if (username.empty() || password.size() < 4) return error(400, "username required, password min 4 chars");
    if (g_files.loadUserByUsername(username)) return error(400, "username already taken");

    int id = g_files.nextId("user_id");
    std::string salt = PasswordHasher::generateSalt();
    std::string hash = PasswordHasher::hashPassword(password, salt);
    g_files.saveUser(UserRecord{id, username, hash, salt});
    g_files.addToSearchIndex(id, username);
    g_usernameIndex.insert(username, id);

    return json(201, "{\"id\":" + std::to_string(id) + "}");
}

HttpResponse handleLogin(const HttpRequest& req) {
    std::string username = jsonField(req.body, "username");
    std::string password = jsonField(req.body, "password");
    auto user = g_files.loadUserByUsername(username);
    if (!user || !PasswordHasher::verify(password, user->salt, user->passwordHash)) {
        return error(401, "invalid credentials");
    }
    return json(200, "{\"id\":" + std::to_string(user->id) + ",\"username\":\"" + jsonEscape(user->username) + "\"}");
}

HttpResponse handleCreatePost(const HttpRequest& req) {
    int userId = std::atoi(jsonField(req.body, "user_id").c_str());
    std::string caption = jsonField(req.body, "caption");
    if (userId <= 0 || caption.empty()) return error(400, "user_id and caption required");
    int id = g_files.nextId("post_id");
    g_files.savePost(id, userId, caption, nowEpochSeconds());
    return json(201, "{\"id\":" + std::to_string(id) + "}");
}

HttpResponse handleFeed(int userId) {
    std::ostringstream out;
    out << "[";
    bool first = true;
    for (int postId : g_files.getPostsByUser(userId)) {
        int uid; std::string caption;
        if (!g_files.loadPost(postId, uid, caption)) continue;
        if (!first) out << ",";
        first = false;
        out << "{\"id\":" << postId << ",\"user_id\":" << uid
            << ",\"caption\":\"" << jsonEscape(caption) << "\""
            << ",\"likes\":" << g_files.getLikeCount(postId) << "}";
    }
    out << "]";
    return json(200, out.str());
}

HttpResponse handleLike(const HttpRequest& req) {
    int postId = std::atoi(jsonField(req.body, "post_id").c_str());
    int userId = std::atoi(jsonField(req.body, "user_id").c_str());
    if (postId <= 0 || userId <= 0) return error(400, "post_id and user_id required");
    if (g_files.hasLiked(postId, userId)) g_files.removeLike(postId, userId);
    else g_files.saveLike(postId, userId);
    return json(200, "{\"likes\":" + std::to_string(g_files.getLikeCount(postId)) + "}");
}

HttpResponse handleCreateComment(const HttpRequest& req) {
    int postId = std::atoi(jsonField(req.body, "post_id").c_str());
    int userId = std::atoi(jsonField(req.body, "user_id").c_str());
    std::string text = jsonField(req.body, "text");
    if (postId <= 0 || userId <= 0 || text.empty()) return error(400, "post_id, user_id, text required");
    int id = g_files.nextId("comment_id");
    g_files.saveComment(postId, CommentRecord{id, userId, text, nowEpochSeconds()});
    return json(201, "{\"id\":" + std::to_string(id) + "}");
}

HttpResponse handleGetComments(int postId) {
    std::ostringstream out;
    out << "[";
    bool first = true;
    for (auto& c : g_files.loadComments(postId)) {
        if (!first) out << ",";
        first = false;
        out << "{\"id\":" << c.id << ",\"user_id\":" << c.userId
            << ",\"text\":\"" << jsonEscape(c.text) << "\"}";
    }
    out << "]";
    return json(200, out.str());
}

HttpResponse handleSearch(const std::string& prefix) {
    std::ostringstream out;
    out << "[";
    bool first = true;
    for (int id : g_usernameIndex.searchByPrefix(prefix)) {
        auto user = g_files.loadUser(id);
        if (!user) continue;
        if (!first) out << ",";
        first = false;
        out << "{\"id\":" << id << ",\"username\":\"" << jsonEscape(user->username) << "\"}";
    }
    out << "]";
    return json(200, out.str());
}

HttpResponse handleFollow(const HttpRequest& req, bool follow) {
    int followerId = std::atoi(jsonField(req.body, "follower_id").c_str());
    int followingId = std::atoi(jsonField(req.body, "following_id").c_str());
    if (followerId <= 0 || followingId <= 0) return error(400, "follower_id and following_id required");
    if (follow) g_files.saveFollow(followerId, followingId);
    else g_files.removeFollow(followerId, followingId);
    return json(200, "{\"ok\":true}");
}

std::string contentTypeFor(const std::string& path) {
    if (path.size() > 5 && path.substr(path.size() - 5) == ".html") return "text/html";
    if (path.size() > 4 && path.substr(path.size() - 4) == ".css") return "text/css";
    if (path.size() > 3 && path.substr(path.size() - 3) == ".js") return "application/javascript";
    return "application/octet-stream";
}

HttpResponse serveStatic(std::string path) {
    if (path == "/") path = "/index.html";
    std::ifstream file("frontend" + path, std::ios::binary);
    if (!file.is_open()) return error(404, "not found");
    std::ostringstream contents;
    contents << file.rdbuf();
    return HttpResponse{200, contentTypeFor(path), contents.str()};
}

// Extracts "/api/x/<id>/rest" style path segments.
std::string pathSegmentAfter(const std::string& path, const std::string& prefix) {
    if (path.rfind(prefix, 0) != 0) return "";
    std::string rest = path.substr(prefix.size());
    auto slash = rest.find('/');
    return slash == std::string::npos ? rest : rest.substr(0, slash);
}

HttpResponse route(const HttpRequest& req) {
    if (req.path == "/api/status") return json(200, "{\"status\":\"ok\"}");
    if (req.path == "/api/register" && req.method == "POST") return handleRegister(req);
    if (req.path == "/api/login" && req.method == "POST") return handleLogin(req);
    if (req.path == "/api/posts" && req.method == "POST") return handleCreatePost(req);
    if (req.path.rfind("/api/feed/", 0) == 0 && req.method == "GET")
        return handleFeed(std::atoi(pathSegmentAfter(req.path, "/api/feed/").c_str()));
    if (req.path == "/api/like" && req.method == "POST") return handleLike(req);
    if (req.path == "/api/comments" && req.method == "POST") return handleCreateComment(req);
    if (req.path.rfind("/api/comments/", 0) == 0 && req.method == "GET")
        return handleGetComments(std::atoi(pathSegmentAfter(req.path, "/api/comments/").c_str()));
    if (req.path.rfind("/api/search/", 0) == 0 && req.method == "GET")
        return handleSearch(pathSegmentAfter(req.path, "/api/search/"));
    if (req.path == "/api/follow" && req.method == "POST") return handleFollow(req, true);
    if (req.path == "/api/unfollow" && req.method == "POST") return handleFollow(req, false);
    if (req.method == "GET") return serveStatic(req.path);
    return error(404, "route not found");
}

HttpRequest parseRequest(const std::string& raw) {
    HttpRequest req;
    std::istringstream stream(raw);
    std::string requestLine;
    std::getline(stream, requestLine);
    std::istringstream lineStream(requestLine);
    std::string httpVersion;
    lineStream >> req.method >> req.path >> httpVersion;

    auto headerEnd = raw.find("\r\n\r\n");
    if (headerEnd != std::string::npos) req.body = raw.substr(headerEnd + 4);
    return req;
}

// Reads a full HTTP request off the socket: headers first, then exactly
// Content-Length bytes of body if present. This is the fix for the
// original's single fixed-size recv() call, which silently truncated
// anything over 4KB.
std::string readFullRequest(socket_t clientSocket) {
    std::string data;
    char chunk[4096];

    // Read until we have the full header block.
    std::size_t headerEnd;
    while ((headerEnd = data.find("\r\n\r\n")) == std::string::npos) {
        int received = recv(clientSocket, chunk, sizeof(chunk), 0);
        if (received <= 0) return data; // connection closed or error; return what we have
        data.append(chunk, static_cast<std::size_t>(received));
        if (data.size() > 1'000'000) break; // guard against unbounded header spam
    }

    // If there's a Content-Length, keep reading until the body is complete.
    auto clPos = data.find("Content-Length:");
    if (clPos != std::string::npos) {
        auto valueStart = clPos + std::string("Content-Length:").size();
        long long contentLength = std::atoll(data.c_str() + valueStart);
        std::size_t bodyStart = data.find("\r\n\r\n") + 4;
        while (static_cast<long long>(data.size() - bodyStart) < contentLength) {
            int received = recv(clientSocket, chunk, sizeof(chunk), 0);
            if (received <= 0) break;
            data.append(chunk, static_cast<std::size_t>(received));
        }
    }
    return data;
}

} // namespace

int main() {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed\n";
        return 1;
    }
#endif

    for (auto& [id, username] : g_files.allIndexedUsers()) g_usernameIndex.insert(username, id);

    socket_t serverSocket = socket(AF_INET, SOCK_STREAM, 0);
#ifdef _WIN32
    if (serverSocket == INVALID_SOCKET) { std::cerr << "socket() failed\n"; return 1; }
#else
    if (serverSocket == INVALID_SOCKET_VAL) { std::cerr << "socket() failed\n"; return 1; }
#endif

    int opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(8080);

    if (bind(serverSocket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "bind() failed - port 8080 may already be in use\n";
        return 1;
    }
    if (listen(serverSocket, 16) < 0) {
        std::cerr << "listen() failed\n";
        return 1;
    }

    std::cout << "Server listening on http://localhost:8080\n";

    while (true) {
        sockaddr_in client{};
#ifdef _WIN32
        int clientLen = sizeof(client);
#else
        socklen_t clientLen = sizeof(client);
#endif
        socket_t clientSocket = accept(serverSocket, reinterpret_cast<sockaddr*>(&client), &clientLen);
        if (clientSocket < 0) continue;

        std::string raw = readFullRequest(clientSocket);
        HttpResponse res = raw.empty() ? error(400, "empty request") : route(parseRequest(raw));
        std::string serialized = serialize(res);
        send(clientSocket, serialized.c_str(), static_cast<int>(serialized.size()), 0);

#ifdef _WIN32
        closesocket(clientSocket);
#else
        close(clientSocket);
#endif
    }

#ifdef _WIN32
    closesocket(serverSocket);
    WSACleanup();
#else
    close(serverSocket);
#endif
    return 0;
}
