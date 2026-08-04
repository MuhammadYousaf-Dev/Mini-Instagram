# Architecture

## Layers

```
frontend/        Static HTML/CSS/JS client (talks to social_server over HTTP)
        │
src/server.cpp    Raw-socket HTTP server — routes requests to social_core
src/main.cpp       Console client — same social_core, no HTTP involved
        │
social_core (library)
├── include/Entities.hpp          Post, Comment, Story, User — plain data
├── include/DoublyLinkedList.hpp  Generic, smart-pointer-owned linked list
├── include/Trie.hpp              Username prefix search
├── include/Feed.hpp              std::vector-backed post collection
├── include/Follow.hpp            Follow relationships (unordered_set)
├── include/LikeSet.hpp           Like relationships (unordered_set)
├── include/PasswordHasher.hpp    Salted SHA-256
└── include/FileHandler.hpp / src/FileHandler.cpp
                                   Flat-file persistence (std::filesystem)
```

`social_core` has no knowledge of the console UI or the HTTP layer — both
entry points depend on it, not the other way around, so it can be unit
tested (see `tests/`) without a terminal or a socket.

## Why entities and containers are separate types

The earlier version of this project had classes like `Post` that stored a
`Post* posts` array of themselves — a single record and "all records" were
the same type. That meant copying one post (e.g. during array resize) could
shallow-copy the collection state embedded in it. Splitting **entity**
(`Post`, `Comment`, `Story` — pure data) from **container**
(`std::vector<Post>`, `DoublyLinkedList<T>`) removes that bug class
structurally rather than patching around it.

## Why a custom `DoublyLinkedList<T>` exists next to `std::vector`

For post/feed storage, `std::vector` replaced a hand-rolled dynamic array —
reimplementing what the standard library already does correctly added risk
for no benefit. For comments and stories, a custom `DoublyLinkedList<T>` was
kept, but rebuilt on `std::unique_ptr` ownership rather than raw `new`/manual
`delete`, because:

- it's genuinely useful here: comments/stories are cursor-navigated
  ("next story", "previous comment") in the original UI's UX model, which a
  doubly linked list expresses more naturally than a vector + index.
- unlike the dynamic array, this is the kind of structure worth writing by
  hand in a DSA-focused project — it's the whole point of the exercise.
- `unique_ptr`-owned `next` + raw non-owning `prev` is the standard, safe C++
  idiom for this shape; see the comment at the top of the header and the
  regression test in `tests/test_doubly_linked_list.cpp` (developed against
  AddressSanitizer, which caught a real use-after-free during
  implementation — left in the file history/commit log intentionally, since
  "I ran ASan and it caught a bug" is a more credible engineering signal
  than a claim that the code was correct on the first try).

## Complexity summary

| Structure | Operation | Complexity |
|---|---|---|
| `Trie` | insert | O(L), L = username length |
| `Trie` | prefix search | O(L + k), k = result count |
| `DoublyLinkedList<T>` | push_back | O(1) |
| `DoublyLinkedList<T>` | eraseCurrent | O(1) |
| `DoublyLinkedList<T>` | traversal | O(n) |
| `Feed` (`std::vector<Post>`) | addPost | amortized O(1) |
| `FollowSet` / `LikeSet` (`unordered_set`) | insert/erase/lookup | O(1) average |
| `FileHandler` | most operations | O(records in that file) — flat files, not indexed |

## Known, deliberate scope limits (not hidden)

- **Persistence is flat files, not a database.** Chosen for the project's
  learning goals (every record is human-readable, greppable, and requires
  zero setup). A production version would use SQLite/Postgres — the
  `FileHandler` interface is written so that swap is a single-file change,
  not a rewrite of every caller.
- **Password hashing is salted SHA-256, not bcrypt/Argon2.** SHA-256 is fast
  by design, which is a liability for password storage (see
  `include/PasswordHasher.hpp` for the full rationale). This is a large
  improvement over the original's plaintext storage, but a real deployment
  should move to a slow, memory-hard KDF.
- **`server.cpp` implements the core route set** (status, register, login,
  posts, feed, like, comments, follow/unfollow, search, static files) on the
  corrected transport layer. Story-queue and per-user stats routes from the
  original were not ported in this pass — the pattern is identical to the
  routes that were, and porting the rest is a natural, scoped follow-up
  rather than something silently dropped.
- **The bundled frontend** was written against the *original* server's API
  shape (ID-based login). The rebuilt server uses username+password JSON
  bodies for `/api/login` and `/api/register` instead. The frontend's fetch
  calls will need small updates to match — flagged here explicitly so it's
  not discovered by surprise.
