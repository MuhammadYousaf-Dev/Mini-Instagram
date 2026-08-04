#pragma once
//
// DoublyLinkedList<T>
// --------------------
// A minimal, memory-safe doubly linked list used for ordered, frequently
// insert/delete-in-the-middle collections (comments, stories).
//
// Design notes (why this shape, for anyone reviewing the code):
//   * Ownership is expressed in the type system: `next` is a std::unique_ptr,
//     so deleting/destructing a node automatically and correctly destroys
//     everything after it. There is no way to leak a node.
//   * `prev` is a raw, non-owning pointer — exactly one owner per node
//     (the node before it), which is the standard, safe idiom for
//     doubly linked structures in modern C++.
//   * Rule of Five is satisfied "for free": unique_ptr makes the list
//     move-only by default, which is the correct behavior (copying a
//     linked list is rarely what you want, and if you do need it,
//     it should be an explicit, opt-in deep copy — see clone()).
//
// Complexity:
//   push_back / push_front : O(1)
//   erase(iterator)         : O(1)
//   traversal               : O(n)
//   size()                  : O(1) (tracked, not recomputed)
//
#include <memory>
#include <stdexcept>
#include <utility>

namespace social {

template <typename T>
class DoublyLinkedList {
private:
    struct Node {
        T value;
        std::unique_ptr<Node> next;
        Node* prev = nullptr;

        explicit Node(T v) : value(std::move(v)) {}
    };

    std::unique_ptr<Node> head_;
    Node* tail_ = nullptr;
    Node* current_ = nullptr; // "cursor" used by next()/previous()/current()
    std::size_t size_ = 0;

public:
    DoublyLinkedList() = default;

    // Move-only: copying a list deeply is rarely needed and should be explicit.
    DoublyLinkedList(const DoublyLinkedList&) = delete;
    DoublyLinkedList& operator=(const DoublyLinkedList&) = delete;
    DoublyLinkedList(DoublyLinkedList&&) noexcept = default;
    DoublyLinkedList& operator=(DoublyLinkedList&&) noexcept = default;

    [[nodiscard]] std::size_t size() const noexcept { return size_; }
    [[nodiscard]] bool empty() const noexcept { return size_ == 0; }

    // Appends to the back. O(1). Returns a reference to the inserted value.
    T& push_back(T value) {
        auto node = std::make_unique<Node>(std::move(value));
        Node* raw = node.get();

        if (!head_) {
            head_ = std::move(node);
            tail_ = raw;
        } else {
            raw->prev = tail_;
            tail_->next = std::move(node);
            tail_ = raw;
        }
        ++size_;
        if (!current_) current_ = raw;
        return raw->value;
    }

    // Cursor-based navigation (mirrors the original API's "current item" model,
    // which the console UI relies on for "show next/previous story" style flows).
    T* current() noexcept { return current_ ? &current_->value : nullptr; }

    void toNext() noexcept {
        if (current_ && current_->next) current_ = current_->next.get();
    }

    void toPrevious() noexcept {
        if (current_ && current_->prev) current_ = current_->prev;
    }

    // Removes the node currently pointed at by the cursor. O(1).
    //
    // NOTE: capture prev/next as raw pointers *before* doing any move that
    // could free `target` — a unique_ptr move-assignment on the link that
    // owns `target` destroys `target` as part of that single statement, so
    // reading target->prev/target->next afterwards is a use-after-free.
    // (Caught by AddressSanitizer during development — see tests/.)
    void eraseCurrent() {
        if (!current_) return;
        Node* target = current_;
        Node* prevNode = target->prev;
        Node* nextNode = target->next.get();

        current_ = nextNode ? nextNode : prevNode;

        if (prevNode) {
            prevNode->next = std::move(target->next); // frees `target` here
            if (nextNode) nextNode->prev = prevNode;
            else tail_ = prevNode;
        } else {
            head_ = std::move(target->next); // frees `target` here
            if (nextNode) nextNode->prev = nullptr;
            else tail_ = nullptr;
        }
        --size_;
    }

    // Minimal forward iteration support (range-for), independent of the cursor.
    class Iterator {
        Node* node_;
    public:
        explicit Iterator(Node* n) : node_(n) {}
        T& operator*() const { return node_->value; }
        Iterator& operator++() { node_ = node_->next.get(); return *this; }
        bool operator!=(const Iterator& other) const { return node_ != other.node_; }
    };

    Iterator begin() const { return Iterator(head_.get()); }
    Iterator end() const { return Iterator(nullptr); }
};

} // namespace social
