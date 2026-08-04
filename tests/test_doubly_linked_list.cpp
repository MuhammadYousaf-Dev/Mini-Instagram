// NDEBUG-independent checks (see test_filehandler.cpp for rationale).
#include "DoublyLinkedList.hpp"
#include <iostream>
#include <string>

using namespace social;

namespace {
int failures = 0;
void check(bool condition, const char* description) {
    if (!condition) { std::cerr << "FAILED: " << description << "\n"; ++failures; }
}
}

int main() {
    DoublyLinkedList<std::string> list;
    list.push_back("first");
    list.push_back("second");
    list.push_back("third");
    check(list.size() == 3, "size is 3 after three push_backs");

    check(*list.current() == "first", "cursor starts at first element");
    list.toNext();
    check(*list.current() == "second", "toNext advances cursor");
    list.toNext();
    check(*list.current() == "third", "toNext advances cursor to last element");
    list.toNext(); // already at tail, should be a no-op
    check(*list.current() == "third", "toNext at tail is a no-op");

    list.toPrevious();
    check(*list.current() == "second", "toPrevious moves cursor back");

    // erase the middle element ("second") and confirm relinking is correct
    list.eraseCurrent();
    check(list.size() == 2, "size is 2 after erasing middle element");

    std::string joined;
    for (const auto& v : list) joined += v + ",";
    check(joined == "first,third,", "remaining elements are correctly relinked after erase");

    // erase down to empty, cursor must never dangle
    list.toNext();
    list.eraseCurrent();
    list.toPrevious();
    list.eraseCurrent();
    check(list.empty(), "list is empty after erasing all elements");
    check(list.current() == nullptr, "cursor is null on empty list");

    if (failures == 0) {
        std::cout << "All DoublyLinkedList tests passed.\n";
        return 0;
    }
    std::cerr << failures << " DoublyLinkedList test(s) failed.\n";
    return 1;
}
