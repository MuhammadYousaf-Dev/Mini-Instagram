# Social — A DSA-Driven Social Media Platform in C++

![CI](https://github.com/yourusername/social-dsa-cpp/actions/workflows/ci.yml/badge.svg)

A small social media backend (console client + HTTP server + web frontend)
built to demonstrate hand-written data structures — a prefix-search **Trie**,
a **doubly linked list**, and hash-set-based relationship tracking — wired
into a working application with persistence, authentication, and a REST API.

This started as a course DSA project and was rebuilt into a portfolio-ready
state: memory-safety issues fixed and verified under AddressSanitizer, a
real bug (case-sensitive prefix search crash) found and fixed with a
regression test, plaintext password storage replaced with salted SHA-256,
and the whole thing made to build cross-platform via CMake. See
[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) for the full design
rationale, complexity table, and an honest list of what's still scoped out.

## Data structures used

| Structure | Where | Why |
|---|---|---|
| Trie | `include/Trie.hpp` | O(prefix length) username autocomplete/search |
| Doubly linked list (custom, smart-pointer-owned) | `include/DoublyLinkedList.hpp` | Cursor-navigable comments and stories |
| Dynamic array (`std::vector`) | `include/Feed.hpp` | Post feed storage |
| Hash set (`std::unordered_set`) | `include/Follow.hpp`, `include/LikeSet.hpp` | O(1) average follow/like lookups |

## Features

- User registration & login (salted SHA-256 password hashing)
- Create posts, view feed
- Like / unlike posts
- Comment on posts
- Follow / unfollow users
- Stories (24h expiry)
- Username prefix search
- Both a console client **and** a REST API + web frontend, sharing the same
  core library

## Build & run

Requires CMake ≥ 3.16 and a C++17 compiler. No external dependencies.

```bash
cmake -S . -B build
cmake --build build

# Console app
./build/social_console

# HTTP server (serves the API + frontend/ on port 8080)
./build/social_server
```

### Running the tests

```bash
cd build
ctest --output-on-failure
```

To build with AddressSanitizer + UndefinedBehaviorSanitizer (recommended
before making changes to `include/DoublyLinkedList.hpp` or `include/Trie.hpp`,
since both were debugged with these on):

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DSOCIAL_ENABLE_SANITIZERS=ON
cmake --build build
```

## Project structure

```
social/
├── include/            Public headers (the core library's API)
├── src/                Library + entry point (.cpp) implementations
├── tests/              Assert-based unit tests (no external test framework)
├── frontend/           Static HTML/CSS/JS client for social_server
├── docs/
│   └── ARCHITECTURE.md Design rationale, complexity table, known scope limits
├── .github/workflows/  CI: build + test on Linux, macOS, Windows
├── CMakeLists.txt
├── LICENSE
└── .gitignore
```

## Known limitations

Documented in full in [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md#known-deliberate-scope-limits-not-hidden):
flat-file persistence instead of a database, SHA-256 instead of a
memory-hard KDF like Argon2/bcrypt, and a handful of server routes
(story-queue, per-user stats) not yet ported to the rebuilt server. None of
these are hidden — they're intentional, explained tradeoffs for a learning
project, with the reasoning written down so a reviewer doesn't have to
guess.

## License

MIT — see [LICENSE](LICENSE).
